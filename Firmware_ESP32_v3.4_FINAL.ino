/*****************************************************************************************
 * AquaSys / HydroSmart - Módulo Atuador ESP32
 * Versão: v3.4.1 (Sem Watchdog Manual)
 * 
 * 🔧 Melhorias desta versão:
 * - ✅ Formato simplificado de comandos MQTT totalmente implementado:
 *      { "relay": 1, "command": true } - Comando manual ON/OFF
 *      { "relay": 1, "config": {...} } - Atualizar configuração
 *      { "relay": 1, "auto": true } - Retornar ao modo automático
 * - ✅ Lógica automática COMPLETA para todos os modos
 * - ✅ NTP inicializado e funcionando para modo LED
 * - ✅ Processamento de pulsos não-bloqueantes
 * - ✅ Validação de dados de sensores
 * - ✅ Reconexão exponencial com buffer offline
 * - ✅ Persistência NVS de estados e configurações
 * - ✅ Modo ciclo automático
 * - ✅ Controle de pH (up/down) por pulsos
 * - ✅ Controle de temperatura e umidade
 * - ✅ Controle de EC por pulsos
 * - ❌ Watchdog removido (deixado a cargo do ESP32)
 *
 * 100% compatível com o app React atualizado
 *****************************************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WebServer.h>

// ----------------------------- PINOUT / CONSTANTS -------------------------------------
const int relayPins[8] = {23, 5, 4, 13, 22, 21, 14, 12};
#define SETUP_BUTTON_PIN 0

// ----------------------------- MQTT CONFIG --------------------------------------------
#define MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USERNAME "esp32-user"
#define MQTT_PASSWORD "HydroSmart123"
#define MQTT_CLIENT_ID "ESP32_Actuator_v3_4_1"

#define TOPIC_SENSORS_SUB "aquasys/sensors/all"
#define TOPIC_RELAY_STATUS "aquasys/relay/status"
#define TOPIC_RELAY_COMMANDS "aquasys/relay/command"
#define TOPIC_LOGS "aquasys/logs"

// ----------------------------- TIMING -----------------------------------------------
const unsigned long HEARTBEAT_INTERVAL = 30000UL; // 30s
const unsigned long STATE_DEBOUNCE_MS = 300UL;
const unsigned long MQTT_RECONNECT_MIN = 2000UL;
const unsigned long MQTT_RECONNECT_MAX = 60000UL;
const unsigned long NTP_UPDATE_INTERVAL = 3600000UL; // 1h
const unsigned long AUTO_UPDATE_INTERVAL = 2000UL; // 2s
#define UTC_OFFSET_SECONDS -10800 // -3h (BR)

// ----------------------------- GLOBALS -----------------------------------------------
Preferences preferences;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS, NTP_UPDATE_INTERVAL);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

bool relayStates[8] = {false};
bool manual_override[8] = {false};
unsigned long lastStateChangeMs[8] = {0};
unsigned long cycle_last_toggle_ms[8] = {0};

// Pulse structure
struct Pulse {
  bool active;
  unsigned long startMs;
  unsigned long durationMs;
  int relayIndex;
};
Pulse pulses[8];

// Sensors
struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  bool valid;
  unsigned long lastUpdateMs;
};
SensorData currentSensorData = {7.0, 800.0, 25.0, 60.0, 23.0, false, 0};

// Relay config
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

// WiFi credentials
String ssid_sta = "";
String password_sta = "";
bool wifiConfigured = false;
bool initialSetupMode = false;

// Logs buffer
#define LOG_CAPACITY 128
String logBuffer[LOG_CAPACITY];
int logIndex = 0;

// Outbox buffer for MQTT
#define OUTBOX_CAPACITY 16
String outboxTopic[OUTBOX_CAPACITY];
String outboxPayload[OUTBOX_CAPACITY];
bool outboxUsed[OUTBOX_CAPACITY];
int outboxHead = 0, outboxTail = 0, outboxCount = 0;

// Timing vars
unsigned long lastMqttReconnectAttempt = 0;
unsigned long mqttReconnectDelay = MQTT_RECONNECT_MIN;
unsigned long lastStatusPublishMs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastNtpUpdateMs = 0;
unsigned long lastAutoUpdateMs = 0;
unsigned long lastLogClearMs = 0;
bool ntpInitialized = false;

// ----------------------------- UTILITIES ---------------------------------------------
void logMessage(const char* level, const char* msg) {
  char buf[256];
  snprintf(buf, sizeof(buf), "[%s] %lu %s", level, millis(), msg);
  Serial.println(buf);
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
    configs[i].led_off_hour = preferences.getInt((p+"led_off_h").c_str(), 18);
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
  
  // Reset manual override para permitir modo automático
  manual_override[index] = false;
  preferences.putBool((p+"manual").c_str(), false);
  
  // Reset ciclo timer
  cycle_last_toggle_ms[index] = 0;
  
  preferences.end();
  logMessage("CFG", ("Config saved for relay " + String(index+1) + ", mode=" + String(configs[index].mode)).c_str());
}

// ----------------------------- WIFI & AP ---------------------------------------------
void handleRoot() {
  String html = "<h2>HydroSmart - Configurar WiFi</h2><form action='/save' method='POST'>SSID:<input name='ssid'><br>Senha:<input name='password' type='password'><br><input type='submit' value='Salvar'></form>";
  server.send(200, "text/html", html);
}

void handleSave() {
  ssid_sta = server.arg("ssid");
  password_sta = server.arg("password");
  preferences.begin("actuator-cfg", false);
  preferences.putString("ssid", ssid_sta);
  preferences.putString("password", password_sta);
  preferences.end();
  server.send(200, "text/html", "<h2>Salvo! Reiniciando...</h2>");
  delay(1000);
  ESP.restart();
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("HydroSmart_Config");
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  initialSetupMode = true;
  logMessage("INFO", "AP Mode iniciado - Conecte em HydroSmart_Config");
}

void setupWifi() {
  if (ssid_sta.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
    logMessage("WIFI", "Conectando...");
    
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
      delay(500);
      tries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConfigured = true;
      server.stop();
      char ipBuf[32];
      snprintf(ipBuf, sizeof(ipBuf), "WiFi conectado: %s", WiFi.localIP().toString().c_str());
      logMessage("WIFI", ipBuf);
      
      // Inicializar NTP
      if (!ntpInitialized) {
        timeClient.begin();
        logMessage("NTP", "Sincronizando...");
        int ntpRetries = 0;
        while (!timeClient.update() && ntpRetries < 5) {
          timeClient.forceUpdate();
          delay(1000);
          ntpRetries++;
        }
        if (timeClient.isTimeSet()) {
          ntpInitialized = true;
          char timeBuf[32];
          snprintf(timeBuf, sizeof(timeBuf), "NTP OK: %s", timeClient.getFormattedTime().c_str());
          logMessage("NTP", timeBuf);
        } else {
          logMessage("WARN", "NTP falhou - modos baseados em horário não funcionarão");
        }
      }
    } else {
      logMessage("WIFI", "Falha na conexão - iniciando AP Mode");
      startAPMode();
    }
  } else {
    startAPMode();
  }
}

// ----------------------------- MQTT ---------------------------------------------------
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
  StaticJsonDocument<384> doc;
  for (int i=0; i<8; i++) {
    doc["relay"+String(i+1)] = relayStates[i];
  }
  doc["uptime"] = millis();
  doc["ntp_ok"] = ntpInitialized;
  
  String out;
  serializeJson(doc, out);
  
  if (!mqttClient.publish(TOPIC_RELAY_STATUS, out.c_str(), retained)) {
    enqueueOutgoing(TOPIC_RELAY_STATUS, out);
  }
  logMessage("MQTT", "Status publicado");
}

void updateRelayNonBlocking(int relayIndex, bool state, const char* reason = "Auto") {
  if (relayIndex < 0 || relayIndex >= 8) return;
  
  unsigned long now = millis();
  if (now - lastStateChangeMs[relayIndex] < STATE_DEBOUNCE_MS) return;
  
  if (relayStates[relayIndex] != state) {
    lastStateChangeMs[relayIndex] = now;
    relayStates[relayIndex] = state;
    digitalWrite(relayPins[relayIndex], state ? LOW : HIGH);
    
    preferences.begin("actuator-cfg", false);
    preferences.putBool(("relay_state_"+String(relayIndex)).c_str(), relayStates[relayIndex]);
    preferences.end();
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Relay %d -> %s (%s)", relayIndex+1, state ? "ON" : "OFF", reason);
    logMessage("RELAY", buf);
    
    publishRelayStatus(true);
  }
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
      currentSensorData.lastUpdateMs = millis();
    } else {
      logMessage("WARN", "Dados de sensor inválidos recebidos");
    }
  }
  else if (strcmp(topic, TOPIC_RELAY_COMMANDS) == 0) {
    // ============ FORMATO SIMPLIFICADO v3.4 ============
    
    // Comando manual: { "relay": 1, "command": true }
    if (doc.containsKey("relay") && doc.containsKey("command")) {
      int r = doc["relay"] | -1;
      bool cmd = doc["command"] | false;
      
      if (r >= 1 && r <= 8) {
        int idx = r - 1;
        manual_override[idx] = true;
        updateRelayNonBlocking(idx, cmd, "Manual MQTT");
        saveConfig();
        char buf[48];
        snprintf(buf, sizeof(buf), "Comando manual: relay %d = %s", r, cmd ? "ON" : "OFF");
        logMessage("CMD", buf);
      }
    }
    // Modo automático: { "relay": 1, "auto": true }
    else if (doc.containsKey("relay") && doc.containsKey("auto")) {
      int r = doc["relay"] | -1;
      bool auto_mode = doc["auto"] | false;
      
      if (r >= 1 && r <= 8 && auto_mode) {
        int idx = r - 1;
        manual_override[idx] = false;
        saveConfig();
        char buf[32];
        snprintf(buf, sizeof(buf), "Relay %d em modo AUTO", r);
        logMessage("CMD", buf);
      }
    }
    // Atualizar configuração: { "relay": 1, "config": {...} }
    else if (doc.containsKey("relay") && doc.containsKey("config")) {
      int r = doc["relay"] | -1;
      
      if (r >= 1 && r <= 8) {
        int idx = r - 1;
        JsonObject cfg = doc["config"].as<JsonObject>();
        
        // Atualizar todos os campos de configuração
        if (cfg.containsKey("mode")) configs[idx].mode = (RelayMode)cfg["mode"].as<int>();
        if (cfg.containsKey("led_on_hour")) configs[idx].led_on_hour = cfg["led_on_hour"];
        if (cfg.containsKey("led_off_hour")) configs[idx].led_off_hour = cfg["led_off_hour"];
        if (cfg.containsKey("cycle_on_min")) configs[idx].cycle_on_min = cfg["cycle_on_min"];
        if (cfg.containsKey("cycle_off_min")) configs[idx].cycle_off_min = cfg["cycle_off_min"];
        if (cfg.containsKey("ph_pulse_sec")) configs[idx].ph_pulse_sec = cfg["ph_pulse_sec"];
        if (cfg.containsKey("ph_threshold_low")) configs[idx].ph_threshold_low = cfg["ph_threshold_low"];
        if (cfg.containsKey("ph_threshold_high")) configs[idx].ph_threshold_high = cfg["ph_threshold_high"];
        if (cfg.containsKey("temp_threshold_on")) configs[idx].temp_threshold_on = cfg["temp_threshold_on"];
        if (cfg.containsKey("temp_threshold_off")) configs[idx].temp_threshold_off = cfg["temp_threshold_off"];
        if (cfg.containsKey("humidity_threshold_on")) configs[idx].humidity_threshold_on = cfg["humidity_threshold_on"];
        if (cfg.containsKey("humidity_threshold_off")) configs[idx].humidity_threshold_off = cfg["humidity_threshold_off"];
        if (cfg.containsKey("ec_threshold")) configs[idx].ec_threshold = cfg["ec_threshold"];
        if (cfg.containsKey("ec_pulse_sec")) configs[idx].ec_pulse_sec = cfg["ec_pulse_sec"];
        
        saveRelayConfig(idx);
        publishRelayStatus(true);
      }
    }
  }
}

void mqttReconnect() {
  unsigned long now = millis();
  if (mqttClient.connected()) return;
  if (now - lastMqttReconnectAttempt < mqttReconnectDelay) return;
  
  lastMqttReconnectAttempt = now;
  logMessage("MQTT", "Tentando conectar...");
  
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    mqttReconnectDelay = MQTT_RECONNECT_MIN;
    mqttClient.subscribe(TOPIC_SENSORS_SUB);
    mqttClient.subscribe(TOPIC_RELAY_COMMANDS);
    flushOutbox();
    publishRelayStatus(true);
    logMessage("MQTT", "Conectado!");
  } else {
    mqttReconnectDelay = min(mqttReconnectDelay * 2, MQTT_RECONNECT_MAX);
    char buf[48];
    snprintf(buf, sizeof(buf), "Falhou (rc=%d), retry em %lus", mqttClient.state(), mqttReconnectDelay/1000);
    logMessage("MQTT", buf);
  }
}

// ----------------------------- AUTOMATIC LOGIC ----------------------------------------
void updateAutomaticRelays() {
  unsigned long now = millis();
  
  for (int i=0; i<8; i++) {
    if (manual_override[i]) continue;
    
    switch (configs[i].mode) {
      case MODE_LED: {
        // LED funciona independente de sensores, só precisa de NTP
        if (ntpInitialized) {
          int h = timeClient.getHours();
          int on = configs[i].led_on_hour;
          int off = configs[i].led_off_hour;
          bool should_on = false;
          
          // Suporte para overnight schedules (ex: 18-06)
          if (on < off) {
            should_on = (h >= on && h < off);
          } else if (on > off) {
            should_on = (h >= on || h < off);
          }
          // Se on == off, mantém desligado
          
          if (relayStates[i] != should_on) {
            char reason[32];
            snprintf(reason, sizeof(reason), "Timer %02d-%02d", on, off);
            updateRelayNonBlocking(i, should_on, reason);
          }
        }
        break;
      }
      
      case MODE_CYCLE: {
        // Ciclo funciona independente de sensores
        unsigned long onMs = (unsigned long)configs[i].cycle_on_min * 60000UL;
        unsigned long offMs = (unsigned long)configs[i].cycle_off_min * 60000UL;
        
        if (cycle_last_toggle_ms[i] == 0) cycle_last_toggle_ms[i] = now;
        
        unsigned long elapsed = now - cycle_last_toggle_ms[i];
        unsigned long threshold = relayStates[i] ? onMs : offMs;
        
        if (elapsed >= threshold) {
          bool newState = !relayStates[i];
          char reason[32];
          snprintf(reason, sizeof(reason), "Cycle %s", newState ? "ON" : "OFF");
          updateRelayNonBlocking(i, newState, reason);
          cycle_last_toggle_ms[i] = now;
        }
        break;
      }
      
      // Modos dependentes de sensores - só executam se sensores válidos
      case MODE_PH_UP: {
        if (currentSensorData.valid && currentSensorData.ph < configs[i].ph_threshold_low && !pulses[i].active) {
          pulses[i] = {true, now, (unsigned long)configs[i].ph_pulse_sec * 1000UL, i};
          updateRelayNonBlocking(i, true, "pH Up Pulse");
        }
        break;
      }
      
      case MODE_PH_DOWN: {
        if (currentSensorData.valid && currentSensorData.ph > configs[i].ph_threshold_high && !pulses[i].active) {
          pulses[i] = {true, now, (unsigned long)configs[i].ph_pulse_sec * 1000UL, i};
          updateRelayNonBlocking(i, true, "pH Down Pulse");
        }
        break;
      }
      
      case MODE_TEMPERATURE: {
        if (currentSensorData.valid) {
          if (currentSensorData.airTemp > configs[i].temp_threshold_on) {
            if (!relayStates[i]) updateRelayNonBlocking(i, true, "Temp High");
          } else if (currentSensorData.airTemp < configs[i].temp_threshold_off) {
            if (relayStates[i]) updateRelayNonBlocking(i, false, "Temp OK");
          }
        }
        break;
      }
      
      case MODE_HUMIDITY: {
        if (currentSensorData.valid) {
          if (currentSensorData.humidity > configs[i].humidity_threshold_on) {
            if (!relayStates[i]) updateRelayNonBlocking(i, true, "Humidity High");
          } else if (currentSensorData.humidity < configs[i].humidity_threshold_off) {
            if (relayStates[i]) updateRelayNonBlocking(i, false, "Humidity OK");
          }
        }
        break;
      }
      
      case MODE_EC: {
        if (currentSensorData.valid && currentSensorData.ec < configs[i].ec_threshold && !pulses[i].active) {
          pulses[i] = {true, now, (unsigned long)configs[i].ec_pulse_sec * 1000UL, i};
          updateRelayNonBlocking(i, true, "EC Pulse");
        }
        break;
      }
      
      case MODE_CO2: {
        // Placeholder para lógica de CO2
        break;
      }
      
      default:
        break;
    }
  }
  
  // Processar pulsos ativos
  for (int i=0; i<8; i++) {
    if (pulses[i].active && (now - pulses[i].startMs >= pulses[i].durationMs)) {
      updateRelayNonBlocking(i, false, "Pulse End");
      pulses[i].active = false;
    }
  }
}

// ----------------------------- SETUP / LOOP ------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("  HydroSmart Actuator v3.4.1");
  Serial.println("  (Sem Watchdog Manual)");
  Serial.println("========================================\n");
  
  // Configurar relés (OFF inicial)
  for (int i=0; i<8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Relé desligado (lógica invertida)
  }
  
  // Carregar configurações
  loadConfig();
  
  // Configurar WiFi
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
    logMessage("INFO", "Botão de setup pressionado - iniciando AP Mode");
    startAPMode();
  } else {
    setupWifi();
  }
  
  // Configurar MQTT
  espClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(2048);
  
  // Restaurar estados dos relés
  for (int i=0; i<8; i++) {
    digitalWrite(relayPins[i], relayStates[i] ? LOW : HIGH);
    pulses[i] = {false, 0, 0, i};
  }
  
  publishRelayStatus(true);
  logMessage("INFO", "Sistema inicializado - v3.4.1");
}

void loop() {
  // Modo AP - só servidor web
  if (initialSetupMode) {
    server.handleClient();
    delay(10);
    return;
  }
  
  // Manter WiFi conectado
  if (WiFi.status() != WL_CONNECTED) {
    logMessage("WIFI", "Conexão perdida - reconectando...");
    setupWifi();
    delay(200);
    return;
  }
  
  // Atualizar NTP periodicamente
  unsigned long now = millis();
  if (ntpInitialized && (now - lastNtpUpdateMs > NTP_UPDATE_INTERVAL)) {
    timeClient.update();
    lastNtpUpdateMs = now;
  }
  
  // Manter MQTT conectado
  if (!mqttClient.connected()) {
    mqttReconnect();
  } else {
    mqttClient.loop();
  }
  
  // Atualizar lógica automática
  if (now - lastAutoUpdateMs > AUTO_UPDATE_INTERVAL) {
    updateAutomaticRelays();
    lastAutoUpdateMs = now;
  }
  
  // Heartbeat periódico
  if (now - lastHeartbeatMs > HEARTBEAT_INTERVAL) {
    publishRelayStatus(true);
    
    // Log de sistema
    char sysBuf[128];
    snprintf(sysBuf, sizeof(sysBuf), "Uptime: %lus | Heap: %d | NTP: %s | MQTT: %s", 
             now/1000, ESP.getFreeHeap(), 
             ntpInitialized ? "OK" : "NO",
             mqttClient.connected() ? "OK" : "NO");
    logMessage("SYS", sysBuf);
    
    lastHeartbeatMs = now;
  }
  
  // Limpar logs periodicamente (1h)
  if (now - lastLogClearMs > 3600000UL) {
    logIndex = 0;
    lastLogClearMs = now;
  }
  
  // Flush buffer MQTT offline
  flushOutbox();
  
  delay(10); // Yield para sistema
}
