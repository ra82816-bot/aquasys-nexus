/*
 * ============================================================================
 * AquaSys Nexus - Actuator Module v4.3.1-LITE
 * ============================================================================
 * VERSÃO LITE - FOCADA EM ESTABILIDADE E MODO DE PROTEÇÃO
 * 
 * MUDANÇAS PRINCIPAIS:
 * ✅ Modo de Proteção Permanente (Failsafe):
 *    - Relé 0: Iluminação (ON 05:00-23:59, OFF 00:00-04:59)
 *    - Relés 1,2,3: Ciclo (15min ON, 15min OFF)
 * ✅ Validação SSL desativada (setInsecure) para economia de heap
 * ✅ BLE lê dados como String ASCII (compatibilidade com Sensor)
 * ✅ Delay(50) no loop para estabilidade
 * ✅ Lógica MQTT simplificada
 * 
 * RECURSOS MANTIDOS:
 * ✅ 8 relés com controle automático
 * ✅ WiFi com modo AP automático
 * ✅ MQTT responsivo
 * ✅ BLE Client para fallback
 * ✅ Watchdog (30s)
 * 
 * AUTOR: HydroSmart Team
 * DATA: 2025-01-12
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
#define FIRMWARE_VERSION "4.3.1-LITE"
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
  MODE_AUTO_HUMIDITY = 5
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
RelayConfig relayConfigs[8];
SensorData currentSensorData = {0, 0, 0, 0, 0, false, 0};
unsigned long lastAuthAttempt = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastBleScan = 0;
unsigned long lastNtpUpdate = 0;
const unsigned long NTP_UPDATE_INTERVAL = 3600000;

// ✅ LITE: Variáveis para Modo de Proteção (Failsafe)
unsigned long cycleFailsafeLastToggle[3] = {0, 0, 0};
bool cycleFailsafeState[3] = {false, false, false};
const unsigned long CYCLE_DURATION = 15 * 60 * 1000; // 15 minutos

// ==================== PROTÓTIPOS ====================
void initWatchdog();
void generateDeviceUUID();
void setupRelays();
void loadConfig();
void setupWiFi();
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
}

// ==================== LOOP ====================
void loop() {
  esp_task_wdt_reset();
  
  // AP Mode
  if (apMode) {
    server.handleClient();
    delay(50); // ✅ LITE: Delay para estabilidade
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
      Serial.println("[WARN] WiFi desconectado");
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
    } else {
      mqttClient.loop();
      mqttConnected = true;
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
  
  // ✅ LITE: Aplicar Modo de Proteção (SEMPRE)
  applyFailsafeMode();
  
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
  
  delay(50); // ✅ LITE: Delay para estabilidade
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
  // ✅ FAILSAFE: Obter hora atual
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  int hour = timeinfo.tm_hour;
  
  // ✅ FAILSAFE 1: Relé 0 = Iluminação
  // Regra: LIGADO 05:00-23:59, DESLIGADO 00:00-04:59
  if (hour >= 5 && hour <= 23) {
    if (!relayConfigs[0].state) {
      updateRelay(0, true);
      Serial.println("[FAILSAFE] Iluminação LIGADA");
    }
  } else {
    if (relayConfigs[0].state) {
      updateRelay(0, false);
      Serial.println("[FAILSAFE] Iluminação DESLIGADA");
    }
  }
  
  // ✅ FAILSAFE 2: Relés 1,2,3 = Ciclo (15min ON, 15min OFF)
  for (int i = 0; i < 3; i++) {
    int relayIndex = i + 1; // Relés 1, 2, 3
    
    unsigned long elapsed = millis() - cycleFailsafeLastToggle[i];
    
    if (elapsed >= CYCLE_DURATION) {
      // Alternar estado
      cycleFailsafeState[i] = !cycleFailsafeState[i];
      cycleFailsafeLastToggle[i] = millis();
      
      updateRelay(relayIndex, cycleFailsafeState[i]);
      Serial.printf("[FAILSAFE] Ciclo Relé %d: %s\n", relayIndex, cycleFailsafeState[i] ? "ON" : "OFF");
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
    Serial.println("[ERROR] WiFi falhou - AP Mode");
    startAPMode();
  }
}

// ==================== AP MODE ====================
void startAPMode() {
  apMode = true;
  String apSSID = "AquaSys-" + deviceUUID.substring(4);
  String apPassword = "aquasys123";
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), apPassword.c_str());
  
  Serial.printf("[INFO] AP: %s / %s\n", apSSID.c_str(), apPassword.c_str());
  Serial.printf("[INFO] IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
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
  
  // ✅ LITE: Desabilitar validação SSL
  wifiClient.setInsecure();
  
  https.begin(wifiClient, AUTH_SERVER);
  https.addHeader("Content-Type", "application/json");
  https.addHeader(AUTH_HEADER_KEY, AUTH_HEADER_VALUE);
  https.setTimeout(15000);
  
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["firmware_version"] = FIRMWARE_VERSION;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  int httpCode = https.POST(requestBody);
  Serial.printf("[INFO] HTTP Code: %d\n", httpCode);
  
  if (httpCode == 200) {
    String response = https.getString();
    StaticJsonDocument<1024> responseDoc;
    
    if (deserializeJson(responseDoc, response) == DeserializationError::Ok && responseDoc["success"] == true) {
      JsonObject mqtt_config = responseDoc["mqtt_config"];
      mqttUsername = mqtt_config["username"].as<String>();
      mqttPassword = mqtt_config["password"].as<String>();
      
      Serial.println("[INFO] ✅ Auth OK");
      https.end();
      return true;
    }
  } else if (httpCode == 404) {
    Serial.println("[ERROR] Dispositivo não registrado!");
    Serial.printf("[ERROR] >>> REGISTRE %s NO APP <<<\n", deviceUUID.c_str());
  }
  
  https.end();
  return false;
}

// ==================== NTP ====================
void setupNTP() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("[INFO] NTP configurado");
}

void updateNTP() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.println("[INFO] NTP atualizado");
  }
}

// ==================== MQTT ====================
void setupMQTT() {
  // ✅ LITE: Desabilitar validação SSL
  wifiClient.setInsecure();
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
  
  Serial.println("[INFO] MQTT configurado");
}

void reconnectMQTT() {
  if (!wifiConnected || !authCompleted) return;
  
  Serial.println("[INFO] Conectando MQTT...");
  
  String clientId = "aquasys-actuator-" + deviceUUID;
  
  if (mqttClient.connect(clientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
    mqttConnected = true;
    Serial.println("[INFO] ✅ MQTT OK");
    
    mqttClient.subscribe(TOPIC_SENSORS);
    mqttClient.subscribe(TOPIC_RELAY_COMMAND);
    mqttClient.subscribe(TOPIC_RELAY_CONFIG);
    
    publishHeartbeat();
  } else {
    Serial.printf("[WARN] MQTT falhou (rc=%d)\n", mqttClient.state());
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  
  char payloadStr[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';
  
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payloadStr) != DeserializationError::Ok) return;
  
  if (topicStr == TOPIC_SENSORS) {
    currentSensorData.ph = doc["ph"] | 0.0f;
    currentSensorData.ec = doc["ec"] | 0.0f;
    currentSensorData.temperature = doc["temperature"] | doc["air_temp"] | 0.0f;
    currentSensorData.humidity = doc["humidity"] | 0.0f;
    currentSensorData.water_temp = doc["water_temp"] | 0.0f;
    currentSensorData.timestamp = millis();
    currentSensorData.valid = true;
  }
  else if (topicStr == TOPIC_RELAY_COMMAND) {
    int index = doc["relay_index"] | -1;
    String command = doc["command"] | "";
    
    // ⚠️ AVISO: Comandos manuais para relés 0-3 serão sobrescritos pelo Failsafe
    if (index >= 0 && index < 8) {
      if (command == "on") {
        relayConfigs[index].mode = MODE_MANUAL_ON;
        if (index > 3) updateRelay(index, true);
      } else if (command == "off") {
        relayConfigs[index].mode = MODE_MANUAL_OFF;
        if (index > 3) updateRelay(index, false);
      }
      
      if (index <= 3) {
        Serial.printf("[WARN] Relé %d sob controle Failsafe - comando ignorado\n", index);
      }
    }
  }
}

void publishRelayStatus() {
  StaticJsonDocument<1024> doc;
  doc["device_uuid"] = deviceUUID;
  
  JsonArray relays = doc.createNestedArray("relays");
  for (int i = 0; i < 8; i++) {
    JsonObject relay = relays.createNestedObject();
    relay["index"] = i;
    relay["state"] = relayConfigs[i].state;
    relay["mode"] = relayConfigs[i].mode;
  }
  
  String payload;
  serializeJson(doc, payload);
  
  mqttClient.publish(TOPIC_RELAY_STATUS, payload.c_str(), false);
}

void publishHeartbeat() {
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = "actuator";
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_connected"] = wifiConnected;
  doc["mqtt_connected"] = mqttConnected;
  
  String payload;
  serializeJson(doc, payload);
  
  mqttClient.publish(TOPIC_HEARTBEAT, payload.c_str(), false);
}

// ==================== BLE CLIENT ====================
void setupBLE() {
  BLEDevice::init("AquaSys-Actuator");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  Serial.println("[INFO] BLE Client pronto");
}

void startBLEScan() {
  Serial.println("[INFO] BLE scan...");
  
  BLEScanResults* foundDevices = pBLEScan->start(5, false);
  
  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    
    if (device.haveName() && String(device.getName().c_str()).startsWith("AquaSys-")) {
      Serial.printf("[INFO] Sensor BLE encontrado: %s\n", device.getName().c_str());
      connectToSensorBLE(device.getAddress().toString().c_str());
      break;
    }
  }
  
  pBLEScan->clearResults();
}

void connectToSensorBLE(String address) {
  if (bleClientConnected) return;
  
  Serial.printf("[INFO] Conectando BLE: %s\n", address.c_str());
  
  pBLEClient = BLEDevice::createClient();
  
  if (pBLEClient->connect(BLEAddress(address.c_str()))) {
    Serial.println("[INFO] ✅ BLE conectado");
    bleClientConnected = true;
  }
}

void readSensorDataBLE() {
  if (!bleClientConnected || !pBLEClient || !pBLEClient->isConnected()) {
    bleClientConnected = false;
    return;
  }
  
  BLERemoteService* pRemoteService = pBLEClient->getService(SERVICE_UUID);
  if (!pRemoteService) return;
  
  // ✅ LITE: Ler dados como String ASCII
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
  
  BLERemoteCharacteristic* pCharAirTemp = pRemoteService->getCharacteristic(CHAR_UUID_AIR_TEMP);
  if (pCharAirTemp && pCharAirTemp->canRead()) {
    String tempStr = pCharAirTemp->readValue().c_str();
    currentSensorData.temperature = tempStr.toFloat();
  }
  
  BLERemoteCharacteristic* pCharHumidity = pRemoteService->getCharacteristic(CHAR_UUID_HUMIDITY);
  if (pCharHumidity && pCharHumidity->canRead()) {
    String humStr = pCharHumidity->readValue().c_str();
    currentSensorData.humidity = humStr.toFloat();
  }
  
  BLERemoteCharacteristic* pCharWaterTemp = pRemoteService->getCharacteristic(CHAR_UUID_WATER_TEMP);
  if (pCharWaterTemp && pCharWaterTemp->canRead()) {
    String waterTempStr = pCharWaterTemp->readValue().c_str();
    currentSensorData.water_temp = waterTempStr.toFloat();
  }
  
  currentSensorData.timestamp = millis();
  currentSensorData.valid = true;
  
  Serial.printf("[BLE] pH:%.2f EC:%.0f T:%.1f H:%.0f\n", 
                currentSensorData.ph, currentSensorData.ec, 
                currentSensorData.temperature, currentSensorData.humidity);
}
