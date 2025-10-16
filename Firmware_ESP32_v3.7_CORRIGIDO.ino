/*****************************************************************************************
 * AquaSys / HydroSmart - Módulo Atuador ESP32
 * Versão: v3.7 FINAL (15/10/2025)
 *
 * 🔧 Melhorias desta versão:
 * - ✅ Inclusão de Comunicação Bluetooth Serial:
 * - O dispositivo é detectável como "HydroSmart-ESP32".
 * - Todos os logs do sistema são espelhados para o serial Bluetooth.
 * - Publicação periódica dos dados dos sensores via Bluetooth.
 * - Aceita comandos simples ("status", "wifi") para debug rápido.
 * - ✅ Mantém todas as funcionalidades das versões v3.5 e v3.6.
 * - Modo de Emergência Offline.
 * - Configuração de Wi-Fi via MQTT.
 *
 *****************************************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WebServer.h>
#include "BluetoothSerial.h"

// ----------------------------- PINOUT / CONSTANTS -------------------------------------
const int relayPins[8] = {23, 5, 4, 13, 22, 21, 14, 12};
#define SETUP_BUTTON_PIN 0

// ----------------------------- MQTT CONFIG --------------------------------------------
#define MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USERNAME "esp32-user"
#define MQTT_PASSWORD "HydroSmart123"
#define MQTT_CLIENT_ID "ESP32_Actuator_v3_7_FINAL"

#define TOPIC_SENSORS_SUB "aquasys/sensors/all"
#define TOPIC_RELAY_STATUS "aquasys/relay/status"
#define TOPIC_RELAY_COMMANDS "aquasys/relay/command"
#define TOPIC_LOGS "aquasys/logs"
#define TOPIC_WIFI_CONFIG "aquasys/relay/config/wifi"
#define TOPIC_WIFI_STATUS "aquasys/relay/status/wifi"
#define TOPIC_WIFI_GET_STATUS "aquasys/relay/wifi/get_status"

// ----------------------------- TIMING -----------------------------------------------
const unsigned long HEARTBEAT_INTERVAL = 30000UL;
const unsigned long STATE_DEBOUNCE_MS = 300UL;
const unsigned long MQTT_RECONNECT_MIN = 2000UL;
const unsigned long MQTT_RECONNECT_MAX = 60000UL;
const unsigned long NTP_UPDATE_INTERVAL = 3600000UL;
const unsigned long AUTO_UPDATE_INTERVAL = 2000UL;
#define UTC_OFFSET_SECONDS -10800

const unsigned long EMERGENCY_MODE_TIMEOUT = 60000UL;
const unsigned long EMERGENCY_CYCLE_DURATION = 900000UL;
const unsigned long WIFI_STATUS_INTERVAL = 60000UL;
const unsigned long BT_DATA_INTERVAL = 10000UL;

// ----------------------------- GLOBALS -----------------------------------------------
Preferences preferences;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS, NTP_UPDATE_INTERVAL);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);
BluetoothSerial SerialBT;

bool relayStates[8] = {false};
bool manual_override[8] = {false};
unsigned long lastStateChangeMs[8] = {0};
unsigned long cycle_last_toggle_ms[8] = {0};

struct Pulse {
  bool active;
  unsigned long startMs;
  unsigned long durationMs;
  int relayIndex;
};
Pulse pulses[8];

struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  bool valid;
};
SensorData currentSensorData = {7.0, 800.0, 25.0, 60.0, 23.0, false};

enum RelayMode {
  MODE_UNUSED=0, MODE_LED=1, MODE_CYCLE=2,
  MODE_PH_UP=3, MODE_TEMPERATURE=4, MODE_HUMIDITY=5,
  MODE_EC=6, MODE_CO2=7, MODE_PH_DOWN=8
};

struct RelayConfig {
  RelayMode mode;
  int led_on_hour, led_off_hour;
  int cycle_on_min, cycle_off_min;
  int ph_pulse_sec;
  float temp_threshold_on, temp_threshold_off;
  float humidity_threshold_on, humidity_threshold_off;
  float ec_threshold; 
  int ec_pulse_sec;
  float ph_threshold_low, ph_threshold_high;
};
RelayConfig configs[8];

String ssid_sta = "";
String password_sta = "";
bool wifiConfigured = false;
bool initialSetupMode = false;

#define LOG_CAPACITY 128
String logBuffer[LOG_CAPACITY];
int logIndex = 0;

#define OUTBOX_CAPACITY 16
String outboxTopic[OUTBOX_CAPACITY];
String outboxPayload[OUTBOX_CAPACITY];
bool outboxUsed[OUTBOX_CAPACITY];
int outboxHead = 0, outboxTail = 0, outboxCount = 0;

unsigned long lastMqttReconnectAttempt = 0;
unsigned long mqttReconnectDelay = MQTT_RECONNECT_MIN;
unsigned long lastHeartbeatMs = 0;
unsigned long lastNtpUpdateMs = 0;
unsigned long lastAutoUpdateMs = 0;
unsigned long lastLogClearMs = 0;
bool ntpInitialized = false;

bool emergencyMode = false;
unsigned long lastOnlineTimestamp = 0;
bool firstConnectionEstablished = false;
unsigned long emergency_cycle_last_toggle_ms[3] = {0, 0, 0};

unsigned long lastWifiStatusMs = 0;
unsigned long lastBtDataMs = 0;

// ----------------------------- UTILITIES ---------------------------------------------
static inline unsigned long nowMs() { return millis(); }

void logMessage(const char* level, const char* msg) {
  char buf[256];
  snprintf(buf, sizeof(buf), "[%s] %lu %s", level, millis(), msg);

  Serial.println(buf);
  if (SerialBT.hasClient()) {
    SerialBT.println(buf);
  }

  logBuffer[logIndex++ % LOG_CAPACITY] = String(buf);
  
  if (mqttClient.connected()) {
    StaticJsonDocument<256> d;
    d["level"] = level;
    d["msg"] = msg;
    d["uptime_ms"] = millis();
    String out; 
    serializeJson(d, out);
    mqttClient.publish(TOPIC_LOGS, out.c_str(), false);
  }
}

bool validateSensorData(float ph, float ec, float airTemp, float humidity, float waterTemp) {
  return (ph >= 0 && ph <= 14) &&
         (ec >= 0 && ec <= 5000) &&
         (airTemp >= -20 && airTemp <= 60) &&
         (humidity >= 0 && humidity <= 100) &&
         (waterTemp >= 0 && waterTemp <= 50);
}

// ----------------------------- PERSISTENCE -------------------------------------------
void loadConfig() {
  preferences.begin("actuator-cfg", false);
  
  for (int i=0; i<8; i++) {
    String p = "r"+String(i)+"_";
    configs[i].mode = (RelayMode)preferences.getInt((p+"mode").c_str(), MODE_UNUSED);
    configs[i].led_on_hour = preferences.getInt((p+"led_on_h").c_str(), 6);
    configs[i].led_off_hour = preferences.getInt((p+"led_off_h").c_str(), 0);
    configs[i].cycle_on_min = preferences.getInt((p+"cycle_on_m").c_str(), 15);
    configs[i].cycle_off_min = preferences.getInt((p+"cycle_off_m").c_str(), 15);
    configs[i].ph_pulse_sec = preferences.getInt((p+"ph_pulse_s").c_str(), 5);
    configs[i].ph_threshold_low = preferences.getFloat((p+"ph_low").c_str(), 5.8);
    configs[i].ph_threshold_high = preferences.getFloat((p+"ph_high").c_str(), 6.5);
    configs[i].temp_threshold_on = preferences.getFloat((p+"t_on").c_str(), 28.0);
    configs[i].temp_threshold_off = preferences.getFloat((p+"t_off").c_str(), 26.0);
    configs[i].humidity_threshold_on = preferences.getFloat((p+"h_on").c_str(), 75.0);
    configs[i].humidity_threshold_off = preferences.getFloat((p+"h_off").c_str(), 65.0);
    configs[i].ec_threshold = preferences.getFloat((p+"ec_t").c_str(), 1200.0);
    configs[i].ec_pulse_sec = preferences.getInt((p+"ec_pulse_s").c_str(), 5);
    
    manual_override[i] = preferences.getBool((p+"manual").c_str(), false);
    relayStates[i] = preferences.getBool(("relay_state_"+String(i)).c_str(), false);
  }
  
  ssid_sta = preferences.getString("ssid", "");
  password_sta = preferences.getString("password", "");
  preferences.end();
}

void saveConfig() {
  preferences.begin("actuator-cfg", false);
  for (int i=0; i<8; i++) {
    String p = "r"+String(i)+"_";
    preferences.putInt((p+"mode").c_str(), (int)configs[i].mode);
    preferences.putBool(("relay_state_"+String(i)).c_str(), relayStates[i]);
    preferences.putBool((p+"manual").c_str(), manual_override[i]);
  }
  preferences.putString("ssid", ssid_sta);
  preferences.putString("password", password_sta);
  preferences.end();
}

void saveRelayConfig(int index) {
  if (index < 0 || index >= 8) return;
  
  preferences.begin("actuator-cfg", false);
  String p = "r"+String(index)+"_";
  
  preferences.putInt((p+"mode").c_str(), (int)configs[index].mode);
  preferences.putInt((p+"led_on_h").c_str(), configs[index].led_on_hour);
  preferences.putInt((p+"led_off_h").c_str(), configs[index].led_off_hour);
  preferences.putInt((p+"cycle_on_m").c_str(), configs[index].cycle_on_min);
  preferences.putInt((p+"cycle_off_m").c_str(), configs[index].cycle_off_min);
  preferences.putInt((p+"ph_pulse_s").c_str(), configs[index].ph_pulse_sec);
  preferences.putFloat((p+"ph_low").c_str(), configs[index].ph_threshold_low);
  preferences.putFloat((p+"ph_high").c_str(), configs[index].ph_threshold_high);
  preferences.putFloat((p+"t_on").c_str(), configs[index].temp_threshold_on);
  preferences.putFloat((p+"t_off").c_str(), configs[index].temp_threshold_off);
  preferences.putFloat((p+"h_on").c_str(), configs[index].humidity_threshold_on);
  preferences.putFloat((p+"h_off").c_str(), configs[index].humidity_threshold_off);
  preferences.putFloat((p+"ec_t").c_str(), configs[index].ec_threshold);
  preferences.putInt((p+"ec_pulse_s").c_str(), configs[index].ec_pulse_sec);
  
  preferences.end();
  logMessage("INFO", ("Config saved for relay " + String(index)).c_str());
}

// ----------------------------- WIFI & AP ---------------------------------------------
void handleRoot() {
  String html = "<h2>Configurar WiFi</h2><form action='/save' method='POST'>SSID:<input name='ssid'><br>Senha:<input name='password' type='password'><br><input type='submit'></form>";
  server.send(200, "text/html", html);
}

void handleSave() {
  ssid_sta = server.arg("ssid");
  password_sta = server.arg("password");
  preferences.begin("actuator-cfg", false);
  preferences.putString("ssid", ssid_sta);
  preferences.putString("password", password_sta);
  preferences.end();
  server.send(200, "text/html", "<h2>Salvo! Reinicie o ESP.</h2>");
  ESP.restart();
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Relay_Config");
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  initialSetupMode = true;
  logMessage("INFO", "AP Mode iniciado");
}

void setupWifi() {
  if (ssid_sta.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
      delay(500);
      tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiConfigured = true;
      server.stop();
      logMessage("INFO", "WiFi conectado");
      
      if (!ntpInitialized) {
        timeClient.begin();
        int ntpRetries = 0;
        while (!timeClient.update() && ntpRetries < 5) {
          timeClient.forceUpdate();
          delay(1000);
          ntpRetries++;
        }
        if (timeClient.isTimeSet()) {
          ntpInitialized = true;
          logMessage("INFO", "NTP sincronizado");
        }
      }
    } else {
      startAPMode();
    }
  } else {
    startAPMode();
  }
}

// ----------------------------- MQTT ---------------------------------------------------
void publishWifiStatus() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  StaticJsonDocument<128> doc;
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  
  String out;
  serializeJson(doc, out);
  mqttClient.publish(TOPIC_WIFI_STATUS, out.c_str(), false);
}

void enqueueOutgoing(const char* topic, const String& payload) {
  if (outboxCount >= OUTBOX_CAPACITY) {
    outboxHead = (outboxHead + 1) % OUTBOX_CAPACITY;
    outboxCount--;
  }
  outboxTopic[outboxTail] = topic;
  outboxPayload[outboxTail] = payload;
  outboxUsed[outboxTail] = true;
  outboxTail = (outboxTail + 1) % OUTBOX_CAPACITY;
  outboxCount++;
}

void flushOutbox() {
  while (outboxCount > 0 && mqttClient.connected()) {
    if (outboxUsed[outboxHead]) {
      mqttClient.publish(outboxTopic[outboxHead].c_str(), outboxPayload[outboxHead].c_str(), true);
      outboxUsed[outboxHead] = false;
    }
    outboxHead = (outboxHead + 1) % OUTBOX_CAPACITY;
    outboxCount--;
  }
}

void publishRelayStatus(bool retained = true) {
  StaticJsonDocument<256> doc;
  for (int i=0; i<8; i++) {
    doc["relay"+String(i+1)] = relayStates[i];
  }
  String out;
  serializeJson(doc, out);
  
  if (!mqttClient.publish(TOPIC_RELAY_STATUS, out.c_str(), retained)) {
    enqueueOutgoing(TOPIC_RELAY_STATUS, out);
  }
}

void updateRelayNonBlocking(int relayIndex, bool state) {
  if (relayIndex < 0 || relayIndex >= 8) return;
  
  unsigned long now = millis();
  if (now - lastStateChangeMs[relayIndex] < STATE_DEBOUNCE_MS) return;
  
  lastStateChangeMs[relayIndex] = now;
  relayStates[relayIndex] = state;
  digitalWrite(relayPins[relayIndex], state ? LOW : HIGH);
  
  preferences.begin("actuator-cfg", false);
  preferences.putBool(("relay_state_"+String(relayIndex)).c_str(), relayStates[relayIndex]);
  preferences.end();
  
  publishRelayStatus(true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i=0; i<length; i++) msg += (char)payload[i];
  
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, msg)) {
    logMessage("ERROR", "JSON parse failed");
    return;
  }

  if (strcmp(topic, TOPIC_SENSORS_SUB) == 0) {
    float ph = doc["ph"] | currentSensorData.ph;
    float ec = doc["ec"] | currentSensorData.ec;
    float airTemp = doc["airTemp"] | currentSensorData.airTemp;
    float humidity = doc["humidity"] | currentSensorData.humidity;
    float waterTemp = doc["waterTemp"] | currentSensorData.waterTemp;
    
    if (validateSensorData(ph, ec, airTemp, humidity, waterTemp)) {
      currentSensorData.ph = ph;
      currentSensorData.ec = ec;
      currentSensorData.airTemp = airTemp;
      currentSensorData.humidity = humidity;
      currentSensorData.waterTemp = waterTemp;
      currentSensorData.valid = true;
    } else {
      logMessage("WARN", "Invalid sensor data received");
    }
  }
  else if (strcmp(topic, TOPIC_RELAY_COMMANDS) == 0) {
    if (doc.containsKey("relay") && doc.containsKey("command")) {
      int r = doc["relay"] | -1;
      bool cmd = doc["command"] | false;
      
      if (r >= 1 && r <= 8) {
        int idx = r - 1;
        manual_override[idx] = true;
        updateRelayNonBlocking(idx, cmd);
        saveConfig();
        logMessage("INFO", ("Manual command: relay " + String(r) + " = " + String(cmd)).c_str());
      }
    }
    else if (doc.containsKey("relay") && doc.containsKey("auto")) {
      int r = doc["relay"] | -1;
      bool auto_mode = doc["auto"] | false;
      
      if (r >= 1 && r <= 8 && auto_mode) {
        int idx = r - 1;
        manual_override[idx] = false;
        saveConfig();
        logMessage("INFO", ("Auto mode enabled: relay " + String(r)).c_str());
      }
    }
    else if (doc.containsKey("relay") && doc.containsKey("config")) {
      int r = doc["relay"] | -1;
      
      if (r >= 1 && r <= 8) {
        int idx = r - 1;
        JsonObject cfg = doc["config"].as<JsonObject>();
        
        configs[idx].mode = (RelayMode)(cfg["mode"] | (int)configs[idx].mode);
        configs[idx].led_on_hour = cfg["led_on_hour"] | configs[idx].led_on_hour;
        configs[idx].led_off_hour = cfg["led_off_hour"] | configs[idx].led_off_hour;
        configs[idx].cycle_on_min = cfg["cycle_on_min"] | configs[idx].cycle_on_min;
        configs[idx].cycle_off_min = cfg["cycle_off_min"] | configs[idx].cycle_off_min;
        configs[idx].ph_pulse_sec = cfg["ph_pulse_sec"] | configs[idx].ph_pulse_sec;
        configs[idx].ph_threshold_low = cfg["ph_threshold_low"] | configs[idx].ph_threshold_low;
        configs[idx].ph_threshold_high = cfg["ph_threshold_high"] | configs[idx].ph_threshold_high;
        configs[idx].temp_threshold_on = cfg["temp_threshold_on"] | configs[idx].temp_threshold_on;
        configs[idx].temp_threshold_off = cfg["temp_threshold_off"] | configs[idx].temp_threshold_off;
        configs[idx].humidity_threshold_on = cfg["humidity_threshold_on"] | configs[idx].humidity_threshold_on;
        configs[idx].humidity_threshold_off = cfg["humidity_threshold_off"] | configs[idx].humidity_threshold_off;
        configs[idx].ec_threshold = cfg["ec_threshold"] | configs[idx].ec_threshold;
        configs[idx].ec_pulse_sec = cfg["ec_pulse_sec"] | configs[idx].ec_pulse_sec;
        
        saveRelayConfig(idx);
        logMessage("INFO", ("Config updated: relay " + String(r)).c_str());
      }
    }
  }
  else if (strcmp(topic, TOPIC_WIFI_CONFIG) == 0) {
    logMessage("INFO", "Recebido comando de configuração Wi-Fi.");
    if (doc.containsKey("ssid") && doc.containsKey("password")) {
      String new_ssid = doc["ssid"];
      String new_pass = doc["password"];

      if (new_ssid.length() > 0 && new_pass.length() >= 8) {
        logMessage("INFO", ("Salvando novas credenciais: SSID=" + new_ssid).c_str());
        preferences.begin("actuator-cfg", false);
        preferences.putString("ssid", new_ssid);
        preferences.putString("password", new_pass);
        preferences.end();

        logMessage("INFO", "Credenciais salvas. Reiniciando em 2 segundos...");
        delay(2000);
        ESP.restart();
      } else {
        logMessage("ERROR", "SSID inválido ou senha muito curta (mínimo 8 caracteres).");
      }
    } else {
      logMessage("ERROR", "JSON de config Wi-Fi mal formatado.");
    }
  }
  else if (strcmp(topic, TOPIC_WIFI_GET_STATUS) == 0) {
    logMessage("INFO", "Solicitação de status WiFi recebida");
    publishWifiStatus();
  }
}

void mqttReconnect() {
  unsigned long now = millis();
  if (mqttClient.connected()) return;
  if (now - lastMqttReconnectAttempt < mqttReconnectDelay) return;
  
  lastMqttReconnectAttempt = now;
  
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    mqttReconnectDelay = MQTT_RECONNECT_MIN;
    mqttClient.subscribe(TOPIC_SENSORS_SUB);
    mqttClient.subscribe(TOPIC_RELAY_COMMANDS);
    mqttClient.subscribe(TOPIC_WIFI_CONFIG);
    mqttClient.subscribe(TOPIC_WIFI_GET_STATUS);
    flushOutbox();
    publishRelayStatus(true);
    publishWifiStatus(); // Publicar status WiFi ao conectar
    logMessage("INFO", "MQTT connected");
  } else {
    mqttReconnectDelay = min(mqttReconnectDelay * 2, MQTT_RECONNECT_MAX);
  }
}

// ----------------------------- AUTOMATIC LOGIC ----------------------------------------
void updateAutomaticRelays() {
  if (emergencyMode) return; 
  if (!currentSensorData.valid) return;
  
  unsigned long now = millis();
  
  for (int i=0; i<8; i++) {
    if (manual_override[i]) continue;
    
    switch (configs[i].mode) {
      case MODE_LED: {
        if (ntpInitialized) {
          int h = timeClient.getHours();
          bool should_on = (h >= configs[i].led_on_hour && h < configs[i].led_off_hour);
          if (relayStates[i] != should_on) {
            updateRelayNonBlocking(i, should_on);
          }
        }
        break;
      }
      
      case MODE_CYCLE: {
        unsigned long elapsed = now - cycle_last_toggle_ms[i];
        unsigned long threshold = relayStates[i] ? 
          (configs[i].cycle_on_min * 60000UL) : 
          (configs[i].cycle_off_min * 60000UL);
        
        if (elapsed >= threshold) {
          updateRelayNonBlocking(i, !relayStates[i]);
          cycle_last_toggle_ms[i] = now;
        }
        break;
      }
      
      case MODE_PH_UP: {
        if (currentSensorData.ph < configs[i].ph_threshold_low && !pulses[i].active) {
          pulses[i] = {true, now, configs[i].ph_pulse_sec * 1000UL, i};
          updateRelayNonBlocking(i, true);
        }
        break;
      }
      
      case MODE_PH_DOWN: {
        if (currentSensorData.ph > configs[i].ph_threshold_high && !pulses[i].active) {
          pulses[i] = {true, now, configs[i].ph_pulse_sec * 1000UL, i};
          updateRelayNonBlocking(i, true);
        }
        break;
      }
      
      case MODE_TEMPERATURE: {
        if (currentSensorData.airTemp > configs[i].temp_threshold_on) {
          if (!relayStates[i]) updateRelayNonBlocking(i, true);
        } else if (currentSensorData.airTemp < configs[i].temp_threshold_off) {
          if (relayStates[i]) updateRelayNonBlocking(i, false);
        }
        break;
      }
      
      case MODE_HUMIDITY: {
        if (currentSensorData.humidity > configs[i].humidity_threshold_on) {
          if (!relayStates[i]) updateRelayNonBlocking(i, true);
        } else if (currentSensorData.humidity < configs[i].humidity_threshold_off) {
          if (relayStates[i]) updateRelayNonBlocking(i, false);
        }
        break;
      }
      
      case MODE_EC: {
        if (currentSensorData.ec < configs[i].ec_threshold && !pulses[i].active) {
          pulses[i] = {true, now, configs[i].ec_pulse_sec * 1000UL, i};
          updateRelayNonBlocking(i, true);
        }
        break;
      }
      
      case MODE_CO2: {
        break;
      }
      
      default:
        break;
    }
  }
  
  for (int i=0; i<8; i++) {
    if (pulses[i].active && (now - pulses[i].startMs >= pulses[i].durationMs)) {
      updateRelayNonBlocking(i, false);
      pulses[i].active = false;
    }
  }
}

// ----------------------------- EMERGENCY MODE LOGIC ------------------------------------
void handleEmergencyMode() {
  if (!ntpInitialized) {
      for (int i=0; i<8; i++) {
          if(relayStates[i]) updateRelayNonBlocking(i, false);
      }
      return;
  }

  unsigned long now = nowMs();

  int currentHour = timeClient.getHours();
  bool lightShouldBeOn = (currentHour >= 5 && currentHour < 24);
  if (relayStates[0] != lightShouldBeOn) {
    updateRelayNonBlocking(0, lightShouldBeOn);
  }

  for (int i = 0; i < 3; i++) {
      unsigned long elapsed = now - emergency_cycle_last_toggle_ms[i];
      if (elapsed >= EMERGENCY_CYCLE_DURATION) {
          int relayIndex = i + 1;
          updateRelayNonBlocking(relayIndex, !relayStates[relayIndex]);
          emergency_cycle_last_toggle_ms[i] = now;
      }
  }

  for (int i = 4; i < 8; i++) {
    if (relayStates[i]) {
      updateRelayNonBlocking(i, false);
    }
  }
}

// ----------------------------- BLUETOOTH HANDLERS ------------------------------------
void publishDataToBluetooth() {
  if (!SerialBT.hasClient()) return;
  if (!currentSensorData.valid) return;

  StaticJsonDocument<256> doc;
  doc["type"] = "sensor_data";
  doc["ph"] = currentSensorData.ph;
  doc["ec"] = currentSensorData.ec;
  doc["airTemp"] = currentSensorData.airTemp;
  doc["humidity"] = currentSensorData.humidity;
  doc["waterTemp"] = currentSensorData.waterTemp;
  
  String out;
  serializeJson(doc, out);
  SerialBT.println(out);
}

void handleBluetoothCommands() {
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "status") {
      SerialBT.println("--- Relay Status ---");
      for (int i=0; i<8; i++) {
        String line = "Relay " + String(i+1) + ": " + (relayStates[i] ? "ON" : "OFF");
        line += (manual_override[i] ? " (Manual)" : " (Auto)");
        SerialBT.println(line);
      }
      SerialBT.println("--------------------");
    } 
    else if (cmd == "wifi") {
        SerialBT.println("--- WiFi Status ---");
        if (WiFi.status() == WL_CONNECTED) {
            SerialBT.println("Status: Connected");
            SerialBT.println("SSID: " + WiFi.SSID());
            SerialBT.println("IP: " + WiFi.localIP().toString());
            SerialBT.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
        } else {
            SerialBT.println("Status: Disconnected");
        }
        SerialBT.println("-------------------");
    }
    else {
      SerialBT.println("Unknown command. Available: 'status', 'wifi'");
    }
  }
}

// ----------------------------- WATCHDOG ----------------------------------------------
void registerWatchdogs() {
  // CORREÇÃO FINAL COM BASE NA SUGESTÃO DO COMPILADOR
  esp_task_wdt_config_t twdt_config;
  
  // Nome do membro corrigido para 'timeout_ms' e valor ajustado para 60 segundos
  twdt_config.timeout_ms = 60000;      // 60 segundos (60 * 1000)
  twdt_config.trigger_panic = true;   // Mantém a reinicialização em caso de falha
  twdt_config.idle_core_mask = 0;     // Monitora ambos os cores

  esp_task_wdt_init(&twdt_config);    // Inicializa com a configuração

  esp_task_wdt_add(NULL);             // Adiciona a task atual (loop) ao watchdog
  logMessage("INFO", "Task WDT inicializado (60s)");
}

// ----------------------------- SETUP / LOOP ------------------------------------------
void setup() {
  Serial.begin(115200);
  SerialBT.begin("HydroSmart-ESP32");

  logMessage("INFO", "Iniciando sistema...");
  
  for (int i=0; i<8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }
  
  loadConfig();
  
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
    startAPMode();
  } else {
    setupWifi();
  }
  
  espClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  
  registerWatchdogs();
  
  for (int i=0; i<8; i++) {
    digitalWrite(relayPins[i], relayStates[i] ? LOW : HIGH);
  }
  
  publishRelayStatus(true);
  logMessage("INFO", "Sistema inicializado - v3.7 FINAL");
}

void loop() {
  esp_task_wdt_reset();
  handleBluetoothCommands();

  if (initialSetupMode) {
    server.handleClient();
    return;
  }

  unsigned long now = nowMs();

  if (WiFi.status() == WL_CONNECTED) {
    if (!firstConnectionEstablished) {
      firstConnectionEstablished = true;
      logMessage("INFO", "Primeira conexão estabelecida.");
    }
    lastOnlineTimestamp = now;
    if (emergencyMode) {
      emergencyMode = false;
      logMessage("INFO", "Conexão restaurada. Retornando ao modo online.");
      updateAutomaticRelays(); 
    }
  } else {
    // Se já teve conexão uma vez E passou do timeout
    if (firstConnectionEstablished && (now - lastOnlineTimestamp > EMERGENCY_MODE_TIMEOUT)) {
      if (!emergencyMode) {
        emergencyMode = true;
        logMessage("WARN", "Conexão perdida. Ativando modo de emergência.");
        for(int i=0; i<3; i++) emergency_cycle_last_toggle_ms[i] = now;
      }
    }
  }
  
  if (emergencyMode) {
    handleEmergencyMode();
    if(now - lastMqttReconnectAttempt > 10000UL) {
        WiFi.reconnect();
        lastMqttReconnectAttempt = now;
    }

  } else {
    // Online
    if (WiFi.status() != WL_CONNECTED) {
      setupWifi();
      delay(200);
      return;
    }

    if (ntpInitialized && (now - lastNtpUpdateMs > NTP_UPDATE_INTERVAL)) {
      timeClient.update();
      lastNtpUpdateMs = now;
    }
    
    if (!mqttClient.connected()) {
      mqttReconnect();
    } else {
      mqttClient.loop();
    }
    
    if (now - lastAutoUpdateMs > AUTO_UPDATE_INTERVAL) {
      updateAutomaticRelays();
      lastAutoUpdateMs = now;
    }
    
    if (now - lastHeartbeatMs > HEARTBEAT_INTERVAL) {
      publishRelayStatus(true);
      lastHeartbeatMs = now;
    }

    if (now - lastWifiStatusMs > WIFI_STATUS_INTERVAL) {
        publishWifiStatus();
        lastWifiStatusMs = now;
    }

    if (now - lastBtDataMs > BT_DATA_INTERVAL) {
        publishDataToBluetooth();
        lastBtDataMs = now;
    }
    
    if (now - lastLogClearMs > 3600000UL) {
      logIndex = 0;
      lastLogClearMs = now;
    }
    
    flushOutbox();
  }
  
  delay(10);
}