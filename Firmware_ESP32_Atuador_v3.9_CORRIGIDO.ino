/*****************************************************************************************
 * AquaSys / HydroSmart - Módulo Atuador ESP32
 * Versão: v3.9 CORRIGIDO (20/10/2025)
 *
 * 🔧 Correções desta versão:
 * - ✅ MQTT_PORT corrigido para 8883 (MQTT over TLS - NÃO WebSocket)
 * - ✅ Porta 8884 é para WebSocket, incompatível com WiFiClientSecure+PubSubClient
 * - ✅ ESP32 usa porta 8883, frontend usa 8884 (ambos funcionam no HiveMQ)
 * - ✅ Buffer MQTT otimizado (512 bytes é suficiente)
 * - ✅ Handshake timeout configurado
 * - ✅ Todas as funcionalidades anteriores mantidas
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
const int relayPins[8] = {23, 5, 4, 13, 22, 21, 14, 26}; 
#define SETUP_BUTTON_PIN 0

// ----------------------------- MQTT CONFIG --------------------------------------------
#define MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883  // ✅ CORRIGIDO: MQTT over TLS (não WebSocket!)
#define MQTT_USERNAME "esp32-user"
#define MQTT_PASSWORD "HydroSmart123"
#define MQTT_CLIENT_ID "ESP32_Actuator_v3_9"

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
const unsigned long INITIAL_BOOT_TIMEOUT = 120000UL;

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
  float threshold;
  float hysteresis;
  unsigned long cycleOnMs;
  unsigned long cycleOffMs;
  bool enabled;
  String name;
};
RelayConfig relayConfigs[8];

// ----------------------------- ESTADO -----------------------------------------------
unsigned long bootTime = 0;
bool firstConnectionEstablished = false;
bool emergencyMode = false;
unsigned long emergencyStartMs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long mqttReconnectDelay = MQTT_RECONNECT_MIN;
unsigned long lastNtpUpdate = 0;
unsigned long lastWifiStatusMs = 0;
unsigned long lastBtDataMs = 0;

bool setupMode = false;
unsigned long setupButtonPressTime = 0;
const unsigned long SETUP_BUTTON_HOLD_TIME = 3000;

// ----------------------------- WIFI CONFIG WEB SERVER -------------------------------
const char* html_wifi_config = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AquaSys WiFi</title><style>body{font-family:Arial;margin:20px;background:#f0f0f0}
.container{max-width:400px;margin:auto;background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}
h1{color:#333;text-align:center}input,select{width:100%;padding:10px;margin:8px 0;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}
button{width:100%;background:#4CAF50;color:white;padding:14px;border:none;border-radius:4px;cursor:pointer;font-size:16px}
button:hover{background:#45a049}.status{padding:10px;margin:10px 0;border-radius:4px;text-align:center}
.success{background:#d4edda;color:#155724}.error{background:#f8d7da;color:#721c24}</style></head>
<body><div class="container"><h1>🌊 AquaSys WiFi</h1><form action="/save" method="POST">
<label>SSID:</label><input type="text" name="ssid" required>
<label>Password:</label><input type="password" name="password" required>
<button type="submit">Salvar e Conectar</button></form></div></body></html>
)rawliteral";

// ----------------------------- FORWARD DECLARATIONS ---------------------------------
void setupWiFi();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishRelayStatus();
void publishHeartbeat();
void updateRelays();
void handleAutoModes();
void checkEmergencyMode();
void handlePulses();
void loadRelayConfigs();
void saveRelayConfig(int index);
void handleSetupButton();
void startSetupMode();
void stopSetupMode();
void handleWebServer();
void sendBluetoothData();

// ----------------------------- SETUP ------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  
  bootTime = millis();
  
  Serial.println("\n[INFO] ═══════════════════════════════════════");
  Serial.println("[INFO] AquaSys Atuador v3.9 CORRIGIDO");
  Serial.println("[INFO] ═══════════════════════════════════════");
  
  // Pinos dos relés
  for (int i = 0; i < 8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
    relayStates[i] = false;
    manual_override[i] = false;
    pulses[i].active = false;
  }
  
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  
  // Carregar configurações
  preferences.begin("aquasys", false);
  loadRelayConfigs();
  
  // WiFi
  setupWiFi();
  
  // NTP
  timeClient.begin();
  timeClient.update();
  Serial.printf("[INFO] %lu NTP sincronizado\n", millis());
  
  // Watchdog
  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);
  Serial.printf("[INFO] %lu Task WDT reconfigurado (60s)\n", millis());
  
  // MQTT
  setupMQTT();
  
  // Bluetooth
  if (!SerialBT.begin("AquaSys-Atuador")) {
    Serial.println("[WARN] Bluetooth falhou ao iniciar");
  } else {
    Serial.println("[INFO] Bluetooth iniciado: AquaSys-Atuador");
  }
  
  Serial.printf("[INFO] %lu Sistema inicializado - v3.9 CORRIGIDO\n", millis());
}

// ----------------------------- LOOP -------------------------------------------------
void loop() {
  unsigned long now = millis();
  
  esp_task_wdt_reset();
  
  handleSetupButton();
  
  if (setupMode) {
    server.handleClient();
    return;
  }
  
  // WiFi
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }
  
  // MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }
  
  // Modo emergência
  checkEmergencyMode();
  
  if (emergencyMode) {
    // Em emergência, apenas cicla LED e bomba
    static unsigned long lastEmergencyToggle = 0;
    if (now - lastEmergencyToggle > 15000) {
      relayStates[0] = !relayStates[0]; // LED
      relayStates[1] = !relayStates[1]; // Bomba
      digitalWrite(relayPins[0], relayStates[0] ? HIGH : LOW);
      digitalWrite(relayPins[1], relayStates[1] ? HIGH : LOW);
      lastEmergencyToggle = now;
    }
  } else {
    handleAutoModes();
    handlePulses();
  }
  
  updateRelays();
  
  // Heartbeat
  if (now - lastHeartbeatMs > HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeatMs = now;
  }
  
  // Status WiFi via MQTT
  if (now - lastWifiStatusMs > WIFI_STATUS_INTERVAL) {
    publishWiFiStatus();
    lastWifiStatusMs = now;
  }
  
  // Dados via Bluetooth
  if (now - lastBtDataMs > BT_DATA_INTERVAL) {
    sendBluetoothData();
    lastBtDataMs = now;
  }
  
  // NTP
  if (now - lastNtpUpdate > NTP_UPDATE_INTERVAL) {
    timeClient.update();
    lastNtpUpdate = now;
  }
}

// ----------------------------- WIFI -------------------------------------------------
void setupWiFi() {
  String ssid = preferences.getString("wifi_ssid", "");
  String password = preferences.getString("wifi_pass", "");
  
  if (ssid.length() == 0) {
    Serial.println("[WARN] WiFi não configurado. Aguardando config via BT ou botão setup.");
    return;
  }
  
  Serial.printf("[INFO] Conectando WiFi: %s\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[INFO] %lu WiFi conectado: %s\n", millis(), WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WARN] WiFi não conectado");
  }
}

// ----------------------------- MQTT -------------------------------------------------
void setupMQTT() {
  // ✅ Configuração TLS otimizada para ESP32
  espClient.setInsecure();  // Aceita certificado auto-assinado do HiveMQ
  espClient.setHandshakeTimeout(30); // ✅ Timeout de 30s para handshake TLS
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512); // ✅ 512 bytes é suficiente e economiza RAM
  mqttClient.setKeepAlive(60);
  
  Serial.printf("[INFO] MQTT configurado: %s:%d\n", MQTT_BROKER, MQTT_PORT);
}

void reconnectMQTT() {
  unsigned long now = millis();
  
  if (WiFi.status() != WL_CONNECTED) return;
  
  if (now - lastMqttReconnectAttempt < mqttReconnectDelay) return;
  
  lastMqttReconnectAttempt = now;
  
  Serial.printf("[INFO] %lu Tentando MQTT...\n", now);
  
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("[INFO] ✅ MQTT conectado!");
    
    if (!firstConnectionEstablished) {
      firstConnectionEstablished = true;
      Serial.printf("[INFO] %lu Primeira conexão estabelecida.\n", millis());
    }
    
    mqttReconnectDelay = MQTT_RECONNECT_MIN;
    
    mqttClient.subscribe(TOPIC_SENSORS_SUB, 1);
    mqttClient.subscribe(TOPIC_RELAY_COMMANDS, 1);
    mqttClient.subscribe(TOPIC_WIFI_CONFIG, 1);
    mqttClient.subscribe(TOPIC_WIFI_GET_STATUS, 1);
    
    Serial.println("[INFO] Subscrito aos tópicos");
    
    publishRelayStatus();
    publishHeartbeat();
    
  } else {
    Serial.printf("[ERRO] MQTT falhou, rc=%d\n", mqttClient.state());
    mqttReconnectDelay = min(mqttReconnectDelay * 2, MQTT_RECONNECT_MAX);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  
  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';
  
  Serial.printf("[MQTT] %s: %s\n", topic, buffer);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, buffer);
  
  if (error) {
    Serial.printf("[ERRO] JSON inválido: %s\n", error.c_str());
    return;
  }
  
  // Sensores
  if (topicStr == TOPIC_SENSORS_SUB) {
    currentSensorData.ph = doc["ph"] | 7.0;
    currentSensorData.ec = doc["ec"] | 800.0;
    currentSensorData.airTemp = doc["air_temp"] | doc["airTemp"] | 25.0;
    currentSensorData.humidity = doc["humidity"] | 60.0;
    currentSensorData.waterTemp = doc["water_temp"] | doc["waterTemp"] | 23.0;
    currentSensorData.valid = true;
    Serial.println("[INFO] Dados de sensores atualizados");
  }
  
  // Comandos de relé
  else if (topicStr == TOPIC_RELAY_COMMANDS) {
    if (doc.containsKey("relay")) {
      int relay = doc["relay"];
      int idx = relay - 1;
      
      if (idx >= 0 && idx < 8) {
        if (doc.containsKey("command")) {
          bool cmd = doc["command"];
          relayStates[idx] = cmd;
          manual_override[idx] = true;
          digitalWrite(relayPins[idx], cmd ? HIGH : LOW);
          Serial.printf("[CMD] Relé %d → %s (manual)\n", relay, cmd ? "ON" : "OFF");
          publishRelayStatus();
        }
        else if (doc.containsKey("auto")) {
          manual_override[idx] = false;
          Serial.printf("[CMD] Relé %d → AUTO\n", relay);
          publishRelayStatus();
        }
        else if (doc.containsKey("config")) {
          JsonObject cfg = doc["config"];
          relayConfigs[idx].mode = (RelayMode)(cfg["mode"] | 0);
          relayConfigs[idx].threshold = cfg["threshold"] | 0.0;
          relayConfigs[idx].hysteresis = cfg["hysteresis"] | 0.5;
          relayConfigs[idx].cycleOnMs = cfg["cycleOnMs"] | 60000UL;
          relayConfigs[idx].cycleOffMs = cfg["cycleOffMs"] | 60000UL;
          relayConfigs[idx].enabled = cfg["enabled"] | false;
          relayConfigs[idx].name = cfg["name"] | String("Relé ") + String(relay);
          saveRelayConfig(idx);
          Serial.printf("[CONFIG] Relé %d configurado\n", relay);
          publishRelayStatus();
        }
      }
    }
  }
  
  // Config WiFi
  else if (topicStr == TOPIC_WIFI_CONFIG) {
    String ssid = doc["ssid"] | "";
    String pass = doc["password"] | "";
    if (ssid.length() > 0) {
      preferences.putString("wifi_ssid", ssid);
      preferences.putString("wifi_pass", pass);
      Serial.printf("[WIFI] Novo WiFi salvo: %s\n", ssid.c_str());
      ESP.restart();
    }
  }
  
  // Get WiFi Status
  else if (topicStr == TOPIC_WIFI_GET_STATUS) {
    publishWiFiStatus();
  }
}

void publishRelayStatus() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  for (int i = 0; i < 8; i++) {
    String key = "relay" + String(i + 1);
    doc[key] = relayStates[i];
  }
  doc["timestamp"] = timeClient.getEpochTime();
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  bool published = mqttClient.publish(TOPIC_RELAY_STATUS, buffer, true);
  if (published) {
    Serial.printf("[MQTT] Status publicado: %s\n", buffer);
  } else {
    Serial.println("[ERRO] Falha ao publicar status");
  }
}

void publishHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["device"] = MQTT_CLIENT_ID;
  doc["uptime"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["emergency"] = emergencyMode;
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  mqttClient.publish("aquasys/heartbeat", buffer);
}

void publishWiFiStatus() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["connected"] = (WiFi.status() == WL_CONNECTED);
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["mac"] = WiFi.macAddress();
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  mqttClient.publish(TOPIC_WIFI_STATUS, buffer);
}

// ----------------------------- RELAY LOGIC ------------------------------------------
void updateRelays() {
  for (int i = 0; i < 8; i++) {
    if (manual_override[i]) continue;
    
    unsigned long now = millis();
    if (now - lastStateChangeMs[i] < STATE_DEBOUNCE_MS) continue;
    
    bool newState = relayStates[i];
    digitalWrite(relayPins[i], newState ? HIGH : LOW);
  }
}

void handleAutoModes() {
  if (!currentSensorData.valid) return;
  
  for (int i = 0; i < 8; i++) {
    if (manual_override[i] || !relayConfigs[i].enabled) continue;
    
    bool shouldActivate = false;
    
    switch (relayConfigs[i].mode) {
      case MODE_LED:
      case MODE_CYCLE: {
        unsigned long now = millis();
        unsigned long elapsed = now - cycle_last_toggle_ms[i];
        unsigned long target = relayStates[i] ? relayConfigs[i].cycleOnMs : relayConfigs[i].cycleOffMs;
        if (elapsed >= target) {
          relayStates[i] = !relayStates[i];
          cycle_last_toggle_ms[i] = now;
          lastStateChangeMs[i] = now;
        }
        continue;
      }
      
      case MODE_PH_UP:
        shouldActivate = (currentSensorData.ph < relayConfigs[i].threshold - relayConfigs[i].hysteresis);
        break;
      
      case MODE_PH_DOWN:
        shouldActivate = (currentSensorData.ph > relayConfigs[i].threshold + relayConfigs[i].hysteresis);
        break;
      
      case MODE_TEMPERATURE:
        shouldActivate = (currentSensorData.airTemp < relayConfigs[i].threshold - relayConfigs[i].hysteresis);
        break;
      
      case MODE_HUMIDITY:
        shouldActivate = (currentSensorData.humidity < relayConfigs[i].threshold - relayConfigs[i].hysteresis);
        break;
      
      case MODE_EC:
        shouldActivate = (currentSensorData.ec < relayConfigs[i].threshold - relayConfigs[i].hysteresis);
        break;
      
      default:
        continue;
    }
    
    if (shouldActivate != relayStates[i]) {
      relayStates[i] = shouldActivate;
      lastStateChangeMs[i] = millis();
    }
  }
}

void handlePulses() {
  unsigned long now = millis();
  for (int i = 0; i < 8; i++) {
    if (pulses[i].active) {
      if (now - pulses[i].startMs >= pulses[i].durationMs) {
        pulses[i].active = false;
        relayStates[i] = false;
        manual_override[i] = false;
        Serial.printf("[PULSE] Relé %d pulse finalizado\n", i + 1);
        publishRelayStatus();
      }
    }
  }
}

void checkEmergencyMode() {
  unsigned long now = millis();
  
  // Se nunca conectou e já passou o timeout inicial
  if (!firstConnectionEstablished && (now - bootTime > INITIAL_BOOT_TIMEOUT)) {
    if (!emergencyMode) {
      emergencyMode = true;
      emergencyStartMs = now;
      Serial.println("[WARN] 🚨 MODO DE EMERGÊNCIA ATIVADO!");
    }
  }
  
  // Se conectou, desativa emergência
  if (mqttClient.connected() && emergencyMode) {
    emergencyMode = false;
    Serial.println("[INFO] ✅ Modo de emergência desativado");
  }
}

// ----------------------------- CONFIG PERSISTENCE -----------------------------------
void loadRelayConfigs() {
  for (int i = 0; i < 8; i++) {
    String prefix = "relay" + String(i) + "_";
    relayConfigs[i].mode = (RelayMode)preferences.getInt((prefix + "mode").c_str(), 0);
    relayConfigs[i].threshold = preferences.getFloat((prefix + "thr").c_str(), 0.0);
    relayConfigs[i].hysteresis = preferences.getFloat((prefix + "hys").c_str(), 0.5);
    relayConfigs[i].cycleOnMs = preferences.getULong((prefix + "on").c_str(), 60000);
    relayConfigs[i].cycleOffMs = preferences.getULong((prefix + "off").c_str(), 60000);
    relayConfigs[i].enabled = preferences.getBool((prefix + "ena").c_str(), false);
    relayConfigs[i].name = preferences.getString((prefix + "name").c_str(), String("Relé ") + String(i + 1));
  }
}

void saveRelayConfig(int index) {
  String prefix = "relay" + String(index) + "_";
  preferences.putInt((prefix + "mode").c_str(), relayConfigs[index].mode);
  preferences.putFloat((prefix + "thr").c_str(), relayConfigs[index].threshold);
  preferences.putFloat((prefix + "hys").c_str(), relayConfigs[index].hysteresis);
  preferences.putULong((prefix + "on").c_str(), relayConfigs[index].cycleOnMs);
  preferences.putULong((prefix + "off").c_str(), relayConfigs[index].cycleOffMs);
  preferences.putBool((prefix + "ena").c_str(), relayConfigs[index].enabled);
  preferences.putString((prefix + "name").c_str(), relayConfigs[index].name);
}

// ----------------------------- SETUP MODE -------------------------------------------
void handleSetupButton() {
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
    if (setupButtonPressTime == 0) {
      setupButtonPressTime = millis();
    } else if (millis() - setupButtonPressTime > SETUP_BUTTON_HOLD_TIME) {
      if (!setupMode) {
        startSetupMode();
      }
      setupButtonPressTime = 0;
    }
  } else {
    setupButtonPressTime = 0;
  }
}

void startSetupMode() {
  setupMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("AquaSys-Setup");
  
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", html_wifi_config);
  });
  
  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", password);
    
    server.send(200, "text/html", "<h1>Salvo! Reiniciando...</h1>");
    delay(2000);
    ESP.restart();
  });
  
  server.begin();
  Serial.println("[SETUP] Modo setup ativado. AP: AquaSys-Setup");
}

// ----------------------------- BLUETOOTH --------------------------------------------
void sendBluetoothData() {
  if (!SerialBT.hasClient()) return;
  
  StaticJsonDocument<512> doc;
  
  // Relés
  for (int i = 0; i < 8; i++) {
    String key = "relay" + String(i + 1);
    doc[key] = relayStates[i];
  }
  
  // Sensores
  doc["ph"] = currentSensorData.ph;
  doc["ec"] = currentSensorData.ec;
  doc["air_temp"] = currentSensorData.airTemp;
  doc["humidity"] = currentSensorData.humidity;
  doc["water_temp"] = currentSensorData.waterTemp;
  
  // Status
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["mqtt_connected"] = mqttClient.connected();
  doc["emergency_mode"] = emergencyMode;
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  SerialBT.println(buffer);
}
