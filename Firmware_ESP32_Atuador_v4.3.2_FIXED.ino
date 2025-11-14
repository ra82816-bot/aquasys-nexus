/*
 * ============================================================================
 * AquaSys Nexus - Actuator Module v4.3.2-FIXED
 * ============================================================================
 * CORREÇÕES:
 * ✅ Failsafe independente de NTP (baseado em millis())
 * ✅ AP Mode com WiFi.disconnect() correto
 * ✅ Failsafe funciona SEMPRE (com ou sem WiFi/MQTT)
 * ✅ Quando conectado, responde aos comandos MQTT normalmente
 * 
 * MODO FAILSAFE (PROTEÇÃO):
 * ✅ Relé 0: Iluminação simulada 24h (19h ON / 5h OFF)
 * ✅ Relés 1,2,3: Ciclo (15min ON / 15min OFF)
 * 
 * AUTOR: HydroSmart Team
 * DATA: 2025-01-14
 * ============================================================================
 */

// ==================== INCLUDES ====================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <time.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <esp_task_wdt.h>
#include <esp_mac.h>

// ==================== CONFIGURAÇÕES ====================
#define FIRMWARE_VERSION "4.3.2-FIXED"
#define WIFI_TIMEOUT 15000
#define AUTH_RETRY_INTERVAL 60000
#define MQTT_RECONNECT_INTERVAL 5000
#define STATUS_PUBLISH_INTERVAL 5000
#define HEARTBEAT_INTERVAL 60000
#define SENSOR_DATA_TIMEOUT 180000
#define BLE_SCAN_INTERVAL 30000
#define WDT_TIMEOUT 30

// Servidor de Autenticação
#define AUTH_SERVER "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth"
#define AUTH_HEADER_KEY "apikey"
#define AUTH_HEADER_VALUE "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw"

// MQTT
#define MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define TOPIC_SENSORS "aquasys/sensors/all"
#define TOPIC_RELAY_STATUS "aquasys/relay/status"
#define TOPIC_RELAY_COMMAND "aquasys/relay/command"
#define TOPIC_RELAY_CONFIG "aquasys/relay/config"
#define TOPIC_HEARTBEAT "aquasys/heartbeat"

// BLE
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_PH "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_EC "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_UUID_AIR_TEMP "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_UUID_HUMIDITY "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define CHAR_UUID_WATER_TEMP "beb5483e-36e1-4688-b7f5-ea07361b26ac"

// NTP
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (-3 * 3600)
#define DAYLIGHT_OFFSET_SEC 0

// ==================== HARDWARE ====================
const int RELAY_PINS[8] = {23, 5, 4, 13, 22, 21, 14, 12};

// ==================== ESTRUTURAS ====================
enum RelayMode {
  MODE_MANUAL_OFF = 0,
  MODE_MANUAL_ON = 1,
  MODE_AUTO_PH = 2,
  MODE_AUTO_EC = 3,
  MODE_AUTO_TEMP = 4,
  MODE_AUTO_HUMIDITY = 5,
  MODE_CYCLE = 6,
  MODE_LIGHTING = 7
};

struct RelayConfig {
  RelayMode mode;
  bool state;
  float ph_min;
  float ph_max;
  float ec_min;
  float ec_max;
  float temp_min;
  float temp_max;
  float humidity_min;
  float humidity_max;
  unsigned long cycle_on_duration;
  unsigned long cycle_off_duration;
};

struct SensorData {
  float ph;
  float ec;
  float temperature;
  float humidity;
  float water_temp;
  bool valid;
  unsigned long timestamp;
};

// ==================== OBJETOS GLOBAIS ====================
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;
WebServer server(80);
BLEScan* pBLEScan = nullptr;
BLEClient* pBLEClient = nullptr;

// ==================== VARIÁVEIS GLOBAIS ====================
String deviceUUID = "";
String mqttUsername = "";
String mqttPassword = "";
bool authCompleted = false;
bool wifiConnected = false;
bool mqttConnected = false;
bool apMode = false;
bool bleInitialized = false;
bool bleClientConnected = false;
bool failsafeOverride = false; // ✅ Permite MQTT sobrescrever failsafe
RelayConfig relayConfigs[8];
SensorData currentSensorData = {0, 0, 0, 0, 0, false, 0};
unsigned long lastAuthAttempt = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastBleScan = 0;
unsigned long lastNtpUpdate = 0;
const unsigned long NTP_UPDATE_INTERVAL = 3600000;

// ✅ FAILSAFE: Variáveis independentes (baseadas em millis())
unsigned long failsafeLightingStart = 0;
bool failsafeLightingState = false;
const unsigned long LIGHTING_ON_DURATION = 19UL * 60UL * 60UL * 1000UL;  // 19h
const unsigned long LIGHTING_OFF_DURATION = 5UL * 60UL * 60UL * 1000UL;  // 5h

unsigned long failsafeCycleLastToggle[3] = {0, 0, 0};
bool failsafeCycleState[3] = {false, false, false};
const unsigned long CYCLE_ON_DURATION = 15UL * 60UL * 1000UL;   // 15min
const unsigned long CYCLE_OFF_DURATION = 15UL * 60UL * 1000UL;  // 15min

// ==================== PROTÓTIPOS ====================
void initWatchdog();
void generateDeviceUUID();
void setupRelays();
void loadConfig();
void setupWiFi();
void safeWiFiShutdown();
bool authenticateDevice();
void setupNTP();
void updateNTP();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void updateRelay(int index, bool state);
void applyFailsafeMode();
void publishRelayStatus();
void publishHeartbeat();
void startAPMode();
void handleRoot();
void handleSave();
void setupBLE();
void startBLEScan();
void connectToSensorBLE(String address);
void readSensorDataBLE();

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  initWatchdog();
  
  Serial.println("\n[INFO] ====================================");
  Serial.println("[INFO]  AquaSys Nexus - Actuator Module");
  Serial.printf("[INFO]  Firmware: %s\n", FIRMWARE_VERSION);
  Serial.println("[INFO] ====================================");
  
  generateDeviceUUID();
  Serial.printf("\n[INFO] UUID: %s\n", deviceUUID.c_str());
  Serial.println("[INFO] >>> REGISTRE NO APP <<<\n");
  
  setupRelays();
  loadConfig();
  
  // ✅ Inicializar failsafe timestamps
  failsafeLightingStart = millis();
  for (int i = 0; i < 3; i++) {
    failsafeCycleLastToggle[i] = millis();
  }
  
  esp_task_wdt_reset();
  setupWiFi();
  
  if (wifiConnected && !apMode) {
    esp_task_wdt_reset();
    authCompleted = authenticateDevice();
    if (authCompleted) {
      setupNTP();
      setupMQTT();
    }
  }
  
  Serial.println("[INFO] ✅ Sistema pronto!");
  Serial.println("[INFO] ✅ Failsafe ativo (independente)");
}

// ==================== LOOP ====================
void loop() {
  esp_task_wdt_reset();
  
  // ✅ FAILSAFE SEMPRE ATIVO (primeiro, antes de tudo)
  if (!failsafeOverride) {
    applyFailsafeMode();
  }
  
  // AP Mode - verificar e recuperar se necessário
  if (apMode) {
    if (WiFi.getMode() != WIFI_AP) {
      Serial.println("[WARN] AP Mode perdido, recuperando...");
      startAPMode();
    }
    server.handleClient();
    delay(50);
    return;
  }
  
  // Auth retry
  if (!authCompleted && millis() - lastAuthAttempt > AUTH_RETRY_INTERVAL) {
    lastAuthAttempt = millis();
    authCompleted = authenticateDevice();
    if (authCompleted) {
      setupMQTT();
    }
  }
  
  // WiFi check
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      wifiConnected = false;
      mqttConnected = false;
      failsafeOverride = false; // ✅ Volta ao failsafe se perder WiFi
      Serial.println("[WARN] WiFi desconectado - Failsafe ativo");
    }
    WiFi.reconnect();
  } else {
    if (!wifiConnected) {
      wifiConnected = true;
      Serial.println("[INFO] WiFi reconectado");
    }
  }
  
  // MQTT
  if (authCompleted) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
      if (!mqttConnected) {
        failsafeOverride = false; // ✅ Volta ao failsafe se perder MQTT
      }
    } else {
      mqttClient.loop();
      if (!mqttConnected) {
        mqttConnected = true;
        Serial.println("[INFO] MQTT conectado - Comandos remotos ativos");
      }
    }
  }
  
  // BLE fallback
  if (!mqttConnected && millis() - currentSensorData.timestamp > SENSOR_DATA_TIMEOUT) {
    if (!bleInitialized) {
      setupBLE();
      bleInitialized = true;
    }
    
    if (!bleClientConnected && millis() - lastBleScan > BLE_SCAN_INTERVAL) {
      lastBleScan = millis();
      startBLEScan();
    }
  }
  
  if (bleClientConnected) {
    readSensorDataBLE();
  }
  
  // Publicações MQTT
  if (mqttConnected && millis() - lastStatusPublish > STATUS_PUBLISH_INTERVAL) {
    lastStatusPublish = millis();
    publishRelayStatus();
  }
  
  if (mqttConnected && millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    lastHeartbeat = millis();
    publishHeartbeat();
  }
  
  // NTP update
  if (wifiConnected && millis() - lastNtpUpdate > NTP_UPDATE_INTERVAL) {
    lastNtpUpdate = millis();
    updateNTP();
  }
  
  delay(50);
}

// ==================== WATCHDOG ====================
void initWatchdog() {
  esp_task_wdt_deinit();
  
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  Serial.println("[INFO] ✅ Watchdog (30s)");
}

// ==================== UUID ====================
void generateDeviceUUID() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  
  char uuid[32];
  snprintf(uuid, sizeof(uuid), "ACT-%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  deviceUUID = String(uuid);
}

// ==================== RELÉS ====================
void setupRelays() {
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    
    relayConfigs[i].mode = MODE_MANUAL_OFF;
    relayConfigs[i].state = false;
    relayConfigs[i].ph_min = 5.5;
    relayConfigs[i].ph_max = 6.5;
    relayConfigs[i].ec_min = 1.0;
    relayConfigs[i].ec_max = 2.5;
    relayConfigs[i].temp_min = 18.0;
    relayConfigs[i].temp_max = 28.0;
    relayConfigs[i].humidity_min = 40.0;
    relayConfigs[i].humidity_max = 80.0;
    relayConfigs[i].cycle_on_duration = 15 * 60 * 1000;
    relayConfigs[i].cycle_off_duration = 15 * 60 * 1000;
  }
}

void loadConfig() {
  preferences.begin("relays", true);
  for (int i = 0; i < 8; i++) {
    String key = "mode_" + String(i);
    relayConfigs[i].mode = (RelayMode)preferences.getInt(key.c_str(), MODE_MANUAL_OFF);
  }
  preferences.end();
}

void updateRelay(int index, bool state) {
  if (index < 0 || index >= 8) return;
  
  relayConfigs[index].state = state;
  digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
  
  Serial.printf("[INFO] Relé %d: %s\n", index, state ? "ON" : "OFF");
}

// ==================== MODO DE PROTEÇÃO (FAILSAFE) ====================
void applyFailsafeMode() {
  // ✅ FAILSAFE 1: Relé 0 = Iluminação (19h ON / 5h OFF)
  unsigned long lightingElapsed = millis() - failsafeLightingStart;
  
  if (failsafeLightingState) {
    // Estado ON: verifica se passou 19h
    if (lightingElapsed >= LIGHTING_ON_DURATION) {
      failsafeLightingState = false;
      failsafeLightingStart = millis();
      updateRelay(0, false);
      Serial.println("[FAILSAFE] Iluminação DESLIGADA (após 19h)");
    } else {
      // Garante que está ligado
      if (!relayConfigs[0].state) {
        updateRelay(0, true);
        Serial.println("[FAILSAFE] Iluminação LIGADA (ciclo 19h)");
      }
    }
  } else {
    // Estado OFF: verifica se passou 5h
    if (lightingElapsed >= LIGHTING_OFF_DURATION) {
      failsafeLightingState = true;
      failsafeLightingStart = millis();
      updateRelay(0, true);
      Serial.println("[FAILSAFE] Iluminação LIGADA (após 5h OFF)");
    } else {
      // Garante que está desligado
      if (relayConfigs[0].state) {
        updateRelay(0, false);
        Serial.println("[FAILSAFE] Iluminação DESLIGADA (ciclo 5h)");
      }
    }
  }
  
  // ✅ FAILSAFE 2: Relés 1,2,3 = Ciclo (15min ON / 15min OFF)
  for (int i = 0; i < 3; i++) {
    int relayIndex = i + 1; // Relés 1, 2, 3
    
    unsigned long cycleElapsed = millis() - failsafeCycleLastToggle[i];
    unsigned long targetDuration = failsafeCycleState[i] ? CYCLE_ON_DURATION : CYCLE_OFF_DURATION;
    
    if (cycleElapsed >= targetDuration) {
      // Alternar estado
      failsafeCycleState[i] = !failsafeCycleState[i];
      failsafeCycleLastToggle[i] = millis();
      
      updateRelay(relayIndex, failsafeCycleState[i]);
      Serial.printf("[FAILSAFE] Ciclo Relé %d: %s (15min)\n", 
                    relayIndex, failsafeCycleState[i] ? "ON" : "OFF");
    }
  }
}

// ==================== WiFi ====================
void setupWiFi() {
  preferences.begin("hydrosmart", false);
  String ssid = preferences.getString("wifi_ssid", "");
  String password = preferences.getString("wifi_password", "");
  preferences.end();
  
  if (ssid.length() == 0) {
    Serial.println("[WARN] Sem WiFi - AP Mode");
    startAPMode();
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("[INFO] Conectando WiFi: %s\n", ssid.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts % 10 == 0) esp_task_wdt_reset();
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("[INFO] ✅ WiFi OK - IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[ERROR] WiFi falhou - Iniciando AP Mode");
    delay(2000);
    WiFi.disconnect(true);
    delay(1000);
    startAPMode();
  }
}

// ==================== CLEANUP WiFi ====================
void safeWiFiShutdown() {
  Serial.println("[INFO] Limpando configuração WiFi...");
  
  WiFi.disconnect(true);
  delay(500);
  
  WiFi.mode(WIFI_OFF);
  delay(500);
  
  Serial.println("[INFO] WiFi cleanup completo");
}

// ==================== AP MODE ====================
void startAPMode() {
  apMode = true;
  
  String apSSID = "AquaSys-" + deviceUUID.substring(4);
  String apPassword = "aquasys123";
  
  Serial.println("[INFO] Preparando AP Mode...");
  
  // Limpeza adequada do WiFi
  safeWiFiShutdown();
  
  // Múltiplas tentativas de inicialização
  int attempts = 0;
  bool apSuccess = false;
  
  while (attempts < 3 && !apSuccess) {
    Serial.printf("[INFO] Tentativa AP %d/3...\n", attempts + 1);
    
    WiFi.mode(WIFI_AP);
    delay(1000);
    
    if (WiFi.softAP(apSSID.c_str(), apPassword.c_str())) {
      apSuccess = true;
      break;
    }
    
    attempts++;
    if (attempts < 3) {
      Serial.println("[WARN] Falha, retentando...");
      delay(2000);
    }
  }
  
  if (apSuccess) {
    delay(2000);
    
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[INFO] ✅ AP Mode OK: %s\n", apSSID.c_str());
    Serial.printf("[INFO] ✅ Senha: %s\n", apPassword.c_str());
    Serial.printf("[INFO] ✅ IP: %s\n", apIP.toString().c_str());
    
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    
    Serial.println("[INFO] ✅ Web server iniciado");
  } else {
    Serial.println("[ERROR] ❌ Todas as tentativas de AP falharam");
    Serial.println("[INFO] Continuando sem AP Mode (failsafe ativo)");
    apMode = false;
  }
}

void handleRoot() {
  String html = R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AquaSys Actuator</title>
  <style>
    body { font-family: Arial; max-width: 400px; margin: 50px auto; padding: 20px; }
    input { width: 100%; padding: 10px; margin: 10px 0; box-sizing: border-box; }
    button { width: 100%; padding: 10px; background: #0066cc; color: white; border: none; }
  </style>
</head>
<body>
  <h2>AquaSys Actuator</h2>
  <p>UUID: <code>)";
  html += deviceUUID;
  html += R"(</code></p>
  <form action="/save" method="POST">
    <input type="text" name="ssid" placeholder="WiFi SSID" required>
    <input type="password" name="password" placeholder="WiFi Password" required>
    <button type="submit">Salvar e Reiniciar</button>
  </form>
</body>
</html>)";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    preferences.begin("hydrosmart", false);
    preferences.putString("wifi_ssid", server.arg("ssid"));
    preferences.putString("wifi_password", server.arg("password"));
    preferences.end();
    
    server.send(200, "text/html", "<html><body><h2>Salvo! Reiniciando...</h2></body></html>");
    delay(2000);
    ESP.restart();
  }
}

// ==================== AUTENTICAÇÃO ====================
bool authenticateDevice() {
  if (!wifiConnected) return false;
  
  Serial.println("[INFO] 🔐 Autenticando...");
  
  HTTPClient https;
  wifiClient.setInsecure();
  
  https.begin(wifiClient, AUTH_SERVER);
  https.addHeader("Content-Type", "application/json");
  https.addHeader(AUTH_HEADER_KEY, AUTH_HEADER_VALUE);
  
  StaticJsonDocument<128> doc;
  doc["device_id"] = deviceUUID;
  doc["device_type"] = "actuator";
  
  String payload;
  serializeJson(doc, payload);
  
  int httpCode = https.POST(payload);
  
  if (httpCode == 200) {
    String response = https.getString();
    StaticJsonDocument<512> respDoc;
    DeserializationError error = deserializeJson(respDoc, response);
    
    if (!error && respDoc.containsKey("mqtt_username") && respDoc.containsKey("mqtt_password")) {
      mqttUsername = respDoc["mqtt_username"].as<String>();
      mqttPassword = respDoc["mqtt_password"].as<String>();
      Serial.println("[INFO] ✅ Auth OK");
      https.end();
      return true;
    }
  }
  
  Serial.printf("[ERROR] Auth falhou: %d\n", httpCode);
  https.end();
  return false;
}

// ==================== NTP ====================
void setupNTP() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("[INFO] NTP configurado");
}

void updateNTP() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
}

// ==================== MQTT ====================
void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);
}

void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < MQTT_RECONNECT_INTERVAL) return;
  
  lastAttempt = millis();
  
  if (!authCompleted) return;
  
  Serial.println("[INFO] Conectando MQTT...");
  
  if (mqttClient.connect(deviceUUID.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
    Serial.println("[INFO] ✅ MQTT conectado");
    mqttConnected = true;
    
    mqttClient.subscribe(TOPIC_RELAY_COMMAND);
    mqttClient.subscribe(TOPIC_RELAY_CONFIG);
    mqttClient.subscribe(TOPIC_SENSORS);
    
    publishRelayStatus();
  } else {
    Serial.printf("[ERROR] MQTT falhou: %d\n", mqttClient.state());
    mqttConnected = false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.println("[ERROR] JSON inválido");
    return;
  }
  
  String topicStr = String(topic);
  
  // ✅ MQTT COMMAND: Sobrescreve failsafe
  if (topicStr == TOPIC_RELAY_COMMAND) {
    int relay = doc["relay"];
    bool state = doc["state"];
    
    if (relay >= 0 && relay < 8) {
      failsafeOverride = true; // ✅ Ativa override
      updateRelay(relay, state);
      publishRelayStatus();
      Serial.printf("[MQTT] Comando recebido - Relé %d: %s (override ativo)\n", 
                    relay, state ? "ON" : "OFF");
    }
  }
  
  // ✅ MQTT CONFIG: Atualiza configuração
  else if (topicStr == TOPIC_RELAY_CONFIG) {
    int relay = doc["relay"];
    
    if (relay >= 0 && relay < 8) {
      if (doc.containsKey("mode")) {
        String mode = doc["mode"].as<String>();
        
        if (mode == "manual") {
          relayConfigs[relay].mode = doc["state"] ? MODE_MANUAL_ON : MODE_MANUAL_OFF;
          failsafeOverride = true;
        } else if (mode == "auto") {
          failsafeOverride = false; // ✅ Volta ao failsafe
          Serial.println("[MQTT] Modo AUTO - Failsafe reativado");
        }
      }
      
      publishRelayStatus();
    }
  }
  
  // ✅ SENSOR DATA
  else if (topicStr == TOPIC_SENSORS) {
    currentSensorData.ph = doc["ph"] | 0.0f;
    currentSensorData.ec = doc["ec"] | 0.0f;
    currentSensorData.temperature = doc["temperature"] | 0.0f;
    currentSensorData.humidity = doc["humidity"] | 0.0f;
    currentSensorData.water_temp = doc["water_temp"] | 0.0f;
    currentSensorData.valid = true;
    currentSensorData.timestamp = millis();
  }
}

void publishRelayStatus() {
  StaticJsonDocument<512> doc;
  doc["device_id"] = deviceUUID;
  doc["timestamp"] = millis();
  doc["failsafe_mode"] = !failsafeOverride;
  
  JsonArray relays = doc.createNestedArray("relays");
  for (int i = 0; i < 8; i++) {
    JsonObject relay = relays.createNestedObject();
    relay["index"] = i;
    relay["state"] = relayConfigs[i].state;
    relay["mode"] = relayConfigs[i].mode;
  }
  
  char buffer[512];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_RELAY_STATUS, buffer);
}

void publishHeartbeat() {
  StaticJsonDocument<256> doc;
  doc["device_id"] = deviceUUID;
  doc["timestamp"] = millis();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;
  doc["failsafe_active"] = !failsafeOverride;
  
  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_HEARTBEAT, buffer);
}

// ==================== BLE ====================
void setupBLE() {
  BLEDevice::init("AquaSys_Actuator");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void startBLEScan() {
  Serial.println("[BLE] Escaneando sensores...");
  BLEScanResults* foundDevices = pBLEScan->start(5, false);
  
  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    
    String deviceName = device.getName().c_str();
    if (device.haveName() && deviceName.indexOf("AquaSys") != -1) {
      Serial.printf("[BLE] Sensor encontrado: %s\n", deviceName.c_str());
      connectToSensorBLE(device.getAddress().toString().c_str());
      break;
    }
  }
  
  pBLEScan->clearResults();
}

void connectToSensorBLE(String address) {
  if (pBLEClient == nullptr) {
    pBLEClient = BLEDevice::createClient();
  }
  
  if (pBLEClient->connect(BLEAddress(address.c_str()))) {
    Serial.println("[BLE] ✅ Conectado ao sensor");
    bleClientConnected = true;
  } else {
    Serial.println("[BLE] ❌ Falha na conexão");
  }
}

void readSensorDataBLE() {
  if (!bleClientConnected || pBLEClient == nullptr) return;
  
  BLERemoteService* pRemoteService = pBLEClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("[BLE] ❌ Serviço não encontrado");
    bleClientConnected = false;
    return;
  }
  
  // Ler características
  BLERemoteCharacteristic* pCharPH = pRemoteService->getCharacteristic(CHAR_UUID_PH);
  if (pCharPH && pCharPH->canRead()) {
    String phStr = pCharPH->readValue().c_str();
    currentSensorData.ph = phStr.toFloat();
  }
  
  BLERemoteCharacteristic* pCharEC = pRemoteService->getCharacteristic(CHAR_UUID_EC);
  if (pCharEC && pCharEC->canRead()) {
    String ecStr = pCharEC->readValue().c_str();
    currentSensorData.ec = ecStr.toFloat();
  }
  
  BLERemoteCharacteristic* pCharTemp = pRemoteService->getCharacteristic(CHAR_UUID_AIR_TEMP);
  if (pCharTemp && pCharTemp->canRead()) {
    String tempStr = pCharTemp->readValue().c_str();
    currentSensorData.temperature = tempStr.toFloat();
  }
  
  currentSensorData.valid = true;
  currentSensorData.timestamp = millis();
  
  Serial.printf("[BLE] Dados: pH=%.2f, EC=%.2f, Temp=%.2f\n",
                currentSensorData.ph, currentSensorData.ec, currentSensorData.temperature);
}
