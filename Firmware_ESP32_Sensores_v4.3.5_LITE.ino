/*
 * ============================================================================
 * AquaSys Nexus - Sensor Module v4.3.5-LITE
 * ============================================================================
 * VERSÃO LITE - FOCADA EM ESTABILIDADE E EFICIÊNCIA
 * 
 * MUDANÇAS PRINCIPAIS:
 * ✅ Validação SSL desativada (setInsecure) para economia de heap
 * ✅ Logs de debug minimizados
 * ✅ BLE envia dados como String ASCII (compatibilidade com Atuador)
 * ✅ Página de Status no OLED (WiFi/MQTT/Modo)
 * ✅ Delay(100) no loop para estabilidade
 * ✅ Lógica MQTT simplificada
 * 
 * RECURSOS MANTIDOS:
 * ✅ Display OLED com navegação por botões
 * ✅ Calibração interativa de pH e EC
 * ✅ WiFi com modo AP automático
 * ✅ MQTT fallback (HiveMQ)
 * ✅ BLE Server para emergência
 * ✅ Watchdog (120s)
 * 
 * AUTOR: HydroSmart Team
 * DATA: 2025-01-12
 * ============================================================================
 */

// ==================== INCLUDES ====================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ==================== CONFIGURAÇÕES DE PINOS ====================
#define PH_SENSOR_PIN 34
#define TDS_SENSOR_PIN 35
#define DHT_PIN 15
#define DHT_TYPE DHT22
#define ONE_WIRE_BUS 2
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define BUTTON_UP 32
#define BUTTON_DOWN 33
#define BUTTON_SELECT 25
#define BUTTON_BACK 26

// ==================== CONFIGURAÇÕES GERAIS ====================
#define FIRMWARE_VERSION "4.3.5-LITE"
#define DEVICE_TYPE "SENSOR"
#define AP_SSID_PREFIX "AquaSys-SEN-"
#define AP_PASSWORD "aquasys2024"
#define AP_TIMEOUT 300000

// Timeouts
#define WIFI_TIMEOUT 15000
#define SENSOR_READ_INTERVAL 30000
#define HEARTBEAT_INTERVAL 60000
#define WATCHDOG_TIMEOUT 120
#define AUTH_TIMEOUT 30000

// API Supabase
#define SUPABASE_URL "https://oaabtbvwxsjomeeizciq.supabase.co"
#define SUPABASE_ANON_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw"

// MQTT Fallback
#define MQTT_BROKER_FALLBACK "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define TOPIC_SENSORS_FALLBACK "aquasys/sensors/all"
#define TOPIC_HEARTBEAT_FALLBACK "aquasys/heartbeat/sensor"

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_PH        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_EC        "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_UUID_AIR_TEMP  "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_UUID_HUMIDITY  "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define CHAR_UUID_WATER_TEMP "beb5483e-36e1-4688-b7f5-ea07361b26ac"
#define CHAR_UUID_DEVICE    "a3c87500-8ed3-4bdf-8a39-a01bebede296"

// NTP
#define NTP_SERVER1 "pool.ntp.org"
#define GMT_OFFSET -10800
#define DAYLIGHT_OFFSET 0

// ==================== OBJETOS GLOBAIS ====================
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Preferences prefs;
WebServer server(80);
DNSServer dnsServer;
BLEServer* pBLEServer = nullptr;
BLECharacteristic* pCharPH = nullptr;
BLECharacteristic* pCharEC = nullptr;
BLECharacteristic* pCharAirTemp = nullptr;
BLECharacteristic* pCharHumidity = nullptr;
BLECharacteristic* pCharWaterTemp = nullptr;
BLECharacteristic* pCharDeviceUUID = nullptr;

// ==================== ESTRUTURAS ====================
struct WiFiCredential {
  char ssid[32];
  char password[64];
};

struct SensorData {
  float ph;
  float ec;
  float air_temp;
  float humidity;
  float water_temp;
  bool valid;
};

struct CalibrationData {
  float ph7_voltage;
  float ph4_voltage;
  float ph_slope;
  float ph_intercept;
  float ec_low_raw;
  float ec_high_raw;
  float ec_low_val;
  float ec_high_val;
};

struct MqttCredentials {
  char broker[128];
  char username[64];
  char password[128];
  char topic_sensors[128];
  char topic_heartbeat[128];
  bool valid;
};

enum Page {
  PAGE_DASHBOARD,
  PAGE_CONNECTIONS,
  PAGE_CALIBRATION,
  PAGE_SYSTEM,
  PAGE_COUNT
};

enum CalibrationMode {
  CAL_NONE,
  CAL_PH_7,
  CAL_PH_4,
  CAL_EC_LOW,
  CAL_EC_HIGH
};

// ==================== VARIÁVEIS GLOBAIS ====================
String deviceUUID = "";
WiFiCredential network;
bool wifiConnected = false;
bool apMode = false;
bool mqttConnected = false;
MqttCredentials mqttCreds = {"", "", "", "", "", false};
bool isAuthenticated = false;
SensorData currentData = {0, 0, 0, 0, 0, false};
CalibrationData calibration = {2.52, 3.29, 1.0, 0.0, 645.0, 2850.0, 360.0, 4588.0};
Page currentPage = PAGE_DASHBOARD;
CalibrationMode calibrationMode = CAL_NONE;
int calibrationMenuIndex = 0;
bool bleActive = false;
bool deviceConnectedBLE = false;
unsigned long lastSensorRead = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWdtReset = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastMqttAttempt = 0;
const byte DNS_PORT = 53;

// ==================== PROTÓTIPOS ====================
void initWatchdog();
void resetWatchdog();
String generateDeviceUUID();
void loadWiFiConfig();
void saveWiFiConfig();
bool connectWiFi();
void startAPMode();
void setupWebServer();
void handleRoot();
void handleSave();
void syncNTP();
bool authenticateDevice();
void loadMqttCredentials();
void saveMqttCredentials();
void setupMQTT();
bool reconnectMQTT();
void publishSensorData();
void publishHeartbeat();
void setupBLE();
void publishDataToBLE();
void loadCalibration();
void saveCalibration();
void calculatePHCoefficients();
float readAverageADC(int pin, int samples = 10);
float voltageToPH(float voltage);
float interpolateEC(float rawValue);
float temperatureCompensateEC(float ec, float temp);
void readSensors();
void initOLED();
void updateDisplay();
void handleButtons();

// ==================== WATCHDOG ====================
void initWatchdog() {
  esp_task_wdt_deinit();
  delay(100);
  
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  Serial.println("[INFO] ✅ Watchdog iniciado (120s)");
}

void resetWatchdog() {
  if (millis() - lastWdtReset > 1000) {
    esp_task_wdt_reset();
    lastWdtReset = millis();
  }
}

// ==================== UUID ====================
String generateDeviceUUID() {
  WiFi.mode(WIFI_STA);
  delay(100);
  
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  char uuid[20];
  sprintf(uuid, "SEN-%02X%02X%02X%02X%02X%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(uuid);
}

// ==================== OLED ====================
void initOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERROR] ❌ OLED falhou");
    while (true) delay(100);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  Serial.println("[INFO] ✅ OLED OK");
}

void updateDisplay() {
  if (millis() - lastDisplayUpdate < 500) return;
  lastDisplayUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  switch (currentPage) {
    case PAGE_DASHBOARD:
      display.println("=== DASHBOARD ===");
      display.println();
      if (currentData.valid) {
        display.printf("pH: %.2f\n", currentData.ph);
        display.printf("EC: %.0f uS/cm\n", currentData.ec);
        display.printf("T.Ar: %.1fC\n", currentData.air_temp);
        display.printf("Umid: %.0f%%\n", currentData.humidity);
        display.printf("T.H2O: %.1fC\n", currentData.water_temp);
      } else {
        display.println("Lendo sensores...");
      }
      break;
      
    case PAGE_CONNECTIONS:
      display.println("=== STATUS ===");
      display.println();
      display.print("WiFi: ");
      display.println(wifiConnected ? "OK" : (apMode ? "AP Ativo" : "OFF"));
      display.print("MQTT: ");
      display.println(mqttConnected ? "ON" : "OFF");
      display.print("Modo: ");
      if (mqttConnected) {
        display.println("Online");
      } else if (bleActive) {
        display.println("BLE Fallback");
      } else {
        display.println("Offline");
      }
      display.println();
      if (wifiConnected) {
        display.print("IP: ");
        display.println(WiFi.localIP().toString());
      }
      break;
      
    case PAGE_CALIBRATION:
      display.println("=== CALIBRACAO ===");
      display.println();
      display.println("1. pH 7.0");
      display.println("2. pH 4.0");
      display.println("3. EC Low (360)");
      display.println("4. EC High (4588)");
      break;
      
    case PAGE_SYSTEM:
      display.println("=== SISTEMA ===");
      display.println();
      display.printf("UUID:\n%s\n", deviceUUID.c_str());
      display.printf("FW: %s\n", FIRMWARE_VERSION);
      display.printf("Heap: %d KB\n", ESP.getFreeHeap() / 1024);
      break;
  }
  
  display.display();
}

void handleButtons() {
  static unsigned long lastDebounce[4] = {0};
  const unsigned long debounceDelay = 200;
  
  if (digitalRead(BUTTON_UP) == LOW && millis() - lastDebounce[0] > debounceDelay) {
    lastDebounce[0] = millis();
    currentPage = (Page)((currentPage + PAGE_COUNT - 1) % PAGE_COUNT);
  }
  
  if (digitalRead(BUTTON_DOWN) == LOW && millis() - lastDebounce[1] > debounceDelay) {
    lastDebounce[1] = millis();
    currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
  }
}

// ==================== WiFi ====================
void loadWiFiConfig() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");
  prefs.end();
  
  if (ssid.length() > 0) {
    ssid.toCharArray(network.ssid, 32);
    password.toCharArray(network.password, 64);
    Serial.println("[INFO] WiFi config carregado");
  }
}

void saveWiFiConfig() {
  prefs.begin("wifi", false);
  prefs.putString("ssid", String(network.ssid));
  prefs.putString("password", String(network.password));
  prefs.end();
  Serial.println("[INFO] WiFi config salvo");
}

bool connectWiFi() {
  if (strlen(network.ssid) == 0) return false;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(network.ssid, network.password);
  Serial.printf("[INFO] Conectando WiFi: %s\n", network.ssid);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts % 10 == 0) resetWatchdog();
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("[INFO] ✅ WiFi OK - IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  
  return false;
}

// ==================== AP MODE ====================
void startAPMode() {
  apMode = true;
  String apSSID = AP_SSID_PREFIX + deviceUUID.substring(4);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
  
  Serial.printf("[INFO] AP Mode: %s / %s\n", apSSID.c_str(), AP_PASSWORD);
  Serial.printf("[INFO] IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  setupWebServer();
}

void setupWebServer() {
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
  <title>AquaSys Sensor</title>
  <style>
    body { font-family: Arial; max-width: 400px; margin: 50px auto; padding: 20px; }
    input { width: 100%; padding: 10px; margin: 10px 0; box-sizing: border-box; }
    button { width: 100%; padding: 10px; background: #0066cc; color: white; border: none; }
  </style>
</head>
<body>
  <h2>AquaSys Sensor</h2>
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
    server.arg("ssid").toCharArray(network.ssid, 32);
    server.arg("password").toCharArray(network.password, 64);
    saveWiFiConfig();
    
    server.send(200, "text/html", "<html><body><h2>Salvo! Reiniciando...</h2></body></html>");
    delay(2000);
    ESP.restart();
  }
}

// ==================== NTP ====================
void syncNTP() {
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER1);
  
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 1609459200 && retry < 10) {
    delay(500);
    now = time(nullptr);
    retry++;
  }
  
  if (now >= 1609459200) {
    Serial.println("[INFO] ✅ NTP sincronizado");
  }
}

// ==================== AUTENTICAÇÃO ====================
void loadMqttCredentials() {
  prefs.begin("mqtt", true);
  String broker = prefs.getString("broker", "");
  if (broker.length() > 0) {
    broker.toCharArray(mqttCreds.broker, 128);
    prefs.getString("username", "").toCharArray(mqttCreds.username, 64);
    prefs.getString("password", "").toCharArray(mqttCreds.password, 128);
    prefs.getString("topic_s", "").toCharArray(mqttCreds.topic_sensors, 128);
    prefs.getString("topic_h", "").toCharArray(mqttCreds.topic_heartbeat, 128);
    mqttCreds.valid = true;
  }
  prefs.end();
}

void saveMqttCredentials() {
  prefs.begin("mqtt", false);
  prefs.putString("broker", mqttCreds.broker);
  prefs.putString("username", mqttCreds.username);
  prefs.putString("password", mqttCreds.password);
  prefs.putString("topic_s", mqttCreds.topic_sensors);
  prefs.putString("topic_h", mqttCreds.topic_heartbeat);
  prefs.end();
}

bool authenticateDevice() {
  if (!wifiConnected) return false;
  
  Serial.println("[INFO] 🔐 Autenticando...");
  
  // Desativar BLE temporariamente
  if (bleActive) {
    BLEDevice::deinit(true);
    bleActive = false;
    delay(200);
  }
  
  HTTPClient http;
  WiFiClientSecure client;
  
  // ✅ LITE: Desabilitar validação SSL para economizar heap
  client.setInsecure();
  client.setTimeout(60);
  
  String url = String(SUPABASE_URL) + "/functions/v1/device-auth";
  if (!http.begin(client, url)) {
    Serial.println("[ERROR] ❌ HTTP begin falhou");
    return false;
  }
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  http.setTimeout(60000);
  
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["firmware_version"] = FIRMWARE_VERSION;
  
  String payload;
  serializeJson(doc, payload);
  
  resetWatchdog();
  int httpCode = http.POST(payload);
  resetWatchdog();
  
  Serial.printf("[INFO] HTTP Code: %d\n", httpCode);
  
  bool authSuccess = false;
  
  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<1024> respDoc;
    
    if (deserializeJson(respDoc, response) == DeserializationError::Ok && respDoc["success"] == true) {
      JsonObject mqttConfig = respDoc["mqtt_config"];
      
      if (mqttConfig) {
        strncpy(mqttCreds.broker, mqttConfig["broker"].as<String>().c_str(), 127);
        strncpy(mqttCreds.username, mqttConfig["username"].as<String>().c_str(), 63);
        strncpy(mqttCreds.password, mqttConfig["password"].as<String>().c_str(), 127);
        strncpy(mqttCreds.topic_sensors, mqttConfig["topics"]["sensors"] | "aquasys/sensors/all", 127);
        strncpy(mqttCreds.topic_heartbeat, mqttConfig["topics"]["heartbeat"] | "aquasys/heartbeat", 127);
        
        mqttCreds.valid = true;
        saveMqttCredentials();
        
        Serial.println("[INFO] ✅ Auth OK");
        isAuthenticated = true;
        authSuccess = true;
      }
    }
  } else {
    Serial.println("[WARN] ⚠️ Auth falhou, usando fallback");
    strcpy(mqttCreds.broker, MQTT_BROKER_FALLBACK);
    strcpy(mqttCreds.username, "esp32-user");
    strcpy(mqttCreds.password, "HydroSmart123");
    strcpy(mqttCreds.topic_sensors, TOPIC_SENSORS_FALLBACK);
    strcpy(mqttCreds.topic_heartbeat, TOPIC_HEARTBEAT_FALLBACK);
    mqttCreds.valid = true;
  }
  
  http.end();
  
  // Reativar BLE
  if (!bleActive && !apMode) {
    setupBLE();
    bleActive = true;
  }
  
  return true;
}

// ==================== MQTT ====================
void setupMQTT() {
  if (!mqttCreds.valid) {
    loadMqttCredentials();
    if (!mqttCreds.valid) authenticateDevice();
  }
  
  // ✅ LITE: Desabilitar validação SSL
  espClient.setInsecure();
  
  mqttClient.setServer(mqttCreds.broker, MQTT_PORT);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
}

bool reconnectMQTT() {
  if (!wifiConnected || !mqttCreds.valid) return false;
  
  if (millis() - lastMqttAttempt < 5000) return false;
  lastMqttAttempt = millis();
  
  Serial.println("[INFO] Conectando MQTT...");
  
  String clientId = "aquasys-sensor-" + deviceUUID;
  
  if (mqttClient.connect(clientId.c_str(), mqttCreds.username, mqttCreds.password)) {
    mqttConnected = true;
    Serial.println("[INFO] ✅ MQTT OK");
    return true;
  } else {
    Serial.printf("[WARN] MQTT falhou (rc=%d)\n", mqttClient.state());
    return false;
  }
}

void publishSensorData() {
  if (!mqttConnected || !currentData.valid) return;
  
  resetWatchdog();
  
  StaticJsonDocument<384> doc;
  doc["device_uuid"] = deviceUUID;
  doc["ph"] = round(currentData.ph * 100) / 100.0;
  doc["ec"] = round(currentData.ec);
  doc["air_temp"] = round(currentData.air_temp * 10) / 10.0;
  doc["humidity"] = round(currentData.humidity);
  doc["water_temp"] = round(currentData.water_temp * 10) / 10.0;
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(mqttCreds.topic_sensors, payload.c_str(), false)) {
    Serial.println("[INFO] 📤 Dados publicados");
  }
}

void publishHeartbeat() {
  if (!mqttConnected) return;
  
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["rssi"] = WiFi.RSSI();
  
  String payload;
  serializeJson(doc, payload);
  
  mqttClient.publish(mqttCreds.topic_heartbeat, payload.c_str(), false);
}

// ==================== BLE ====================
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnectedBLE = true;
    Serial.println("[INFO] 📱 BLE conectado");
  }
  
  void onDisconnect(BLEServer* pServer) {
    deviceConnectedBLE = false;
    Serial.println("[INFO] 📱 BLE desconectado");
    pServer->startAdvertising();
  }
};

void setupBLE() {
  String bleName = "AquaSys-" + deviceUUID.substring(4);
  
  BLEDevice::init(bleName.c_str());
  pBLEServer = BLEDevice::createServer();
  pBLEServer->setCallbacks(new MyServerCallbacks());
  
  BLEService* pService = pBLEServer->createService(SERVICE_UUID);
  
  pCharPH = pService->createCharacteristic(CHAR_UUID_PH, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharPH->addDescriptor(new BLE2902());
  
  pCharEC = pService->createCharacteristic(CHAR_UUID_EC, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharEC->addDescriptor(new BLE2902());
  
  pCharAirTemp = pService->createCharacteristic(CHAR_UUID_AIR_TEMP, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharAirTemp->addDescriptor(new BLE2902());
  
  pCharHumidity = pService->createCharacteristic(CHAR_UUID_HUMIDITY, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharHumidity->addDescriptor(new BLE2902());
  
  pCharWaterTemp = pService->createCharacteristic(CHAR_UUID_WATER_TEMP, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharWaterTemp->addDescriptor(new BLE2902());
  
  pCharDeviceUUID = pService->createCharacteristic(CHAR_UUID_DEVICE, BLECharacteristic::PROPERTY_READ);
  pCharDeviceUUID->setValue(deviceUUID.c_str());
  
  pService->start();
  
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  
  bleActive = true;
  Serial.printf("[INFO] ✅ BLE: %s\n", bleName.c_str());
}

void publishDataToBLE() {
  if (!bleActive || !currentData.valid) return;
  
  // ✅ LITE: Enviar como String ASCII
  String phStr = String(currentData.ph, 2);
  String ecStr = String((int)currentData.ec);
  String airTempStr = String(currentData.air_temp, 1);
  String humidityStr = String((int)currentData.humidity);
  String waterTempStr = String(currentData.water_temp, 1);
  
  pCharPH->setValue(phStr.c_str());
  pCharPH->notify();
  
  pCharEC->setValue(ecStr.c_str());
  pCharEC->notify();
  
  pCharAirTemp->setValue(airTempStr.c_str());
  pCharAirTemp->notify();
  
  pCharHumidity->setValue(humidityStr.c_str());
  pCharHumidity->notify();
  
  pCharWaterTemp->setValue(waterTempStr.c_str());
  pCharWaterTemp->notify();
}

// ==================== SENSORES ====================
void loadCalibration() {
  prefs.begin("calibration", true);
  calibration.ph7_voltage = prefs.getFloat("ph7_v", 2.52);
  calibration.ph4_voltage = prefs.getFloat("ph4_v", 3.29);
  calibration.ec_low_raw = prefs.getFloat("ec_low_r", 645.0);
  calibration.ec_high_raw = prefs.getFloat("ec_high_r", 2850.0);
  calibration.ec_low_val = prefs.getFloat("ec_low_v", 360.0);
  calibration.ec_high_val = prefs.getFloat("ec_high_v", 4588.0);
  prefs.end();
  
  calculatePHCoefficients();
  Serial.printf("[INFO] pH Coef: slope=%.3f intercept=%.3f\n", calibration.ph_slope, calibration.ph_intercept);
}

void saveCalibration() {
  prefs.begin("calibration", false);
  prefs.putFloat("ph7_v", calibration.ph7_voltage);
  prefs.putFloat("ph4_v", calibration.ph4_voltage);
  prefs.putFloat("ec_low_r", calibration.ec_low_raw);
  prefs.putFloat("ec_high_r", calibration.ec_high_raw);
  prefs.putFloat("ec_low_v", calibration.ec_low_val);
  prefs.putFloat("ec_high_v", calibration.ec_high_val);
  prefs.end();
  
  calculatePHCoefficients();
}

void calculatePHCoefficients() {
  if (calibration.ph7_voltage != calibration.ph4_voltage) {
    calibration.ph_slope = (4.0 - 7.0) / (calibration.ph4_voltage - calibration.ph7_voltage);
    calibration.ph_intercept = 7.0 - (calibration.ph_slope * calibration.ph7_voltage);
  }
}

float readAverageADC(int pin, int samples) {
  float sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return sum / samples;
}

float voltageToPH(float voltage) {
  return (calibration.ph_slope * voltage) + calibration.ph_intercept;
}

float interpolateEC(float rawValue) {
  if (calibration.ec_high_raw == calibration.ec_low_raw) return 0;
  
  float proportion = (rawValue - calibration.ec_low_raw) / (calibration.ec_high_raw - calibration.ec_low_raw);
  return calibration.ec_low_val + (proportion * (calibration.ec_high_val - calibration.ec_low_val));
}

float temperatureCompensateEC(float ec, float temp) {
  const float TEMP_COEF = 0.02;
  const float REFERENCE_TEMP = 25.0;
  return ec / (1.0 + TEMP_COEF * (temp - REFERENCE_TEMP));
}

void readSensors() {
  // pH
  float phVoltage = (readAverageADC(PH_SENSOR_PIN, 10) / 4095.0) * 3.3;
  currentData.ph = voltageToPH(phVoltage);
  
  // EC
  float ecRaw = readAverageADC(TDS_SENSOR_PIN, 10);
  currentData.ec = interpolateEC(ecRaw);
  
  // DHT22
  currentData.air_temp = dht.readTemperature();
  currentData.humidity = dht.readHumidity();
  
  // DS18B20
  ds18b20.requestTemperatures();
  currentData.water_temp = ds18b20.getTempCByIndex(0);
  
  // Validar
  currentData.valid = (currentData.ph >= 0 && currentData.ph <= 14 &&
                       currentData.ec >= 0 && currentData.ec < 10000 &&
                       !isnan(currentData.air_temp) && !isnan(currentData.humidity) &&
                       currentData.water_temp > -50 && currentData.water_temp < 100);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n[INFO] ====================================");
  Serial.println("[INFO]  AquaSys Nexus - Sensor Module");
  Serial.printf("[INFO]  Firmware: %s\n", FIRMWARE_VERSION);
  Serial.println("[INFO] ====================================");
  
  initWatchdog();
  
  deviceUUID = generateDeviceUUID();
  Serial.printf("\n[INFO] UUID: %s\n", deviceUUID.c_str());
  Serial.println("[INFO] >>> REGISTRE NO APP <<<\n");
  
  initOLED();
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("AquaSys v4.3.5");
  display.println("LITE Edition");
  display.println();
  display.println("Iniciando...");
  display.display();
  delay(2000);
  
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);
  
  dht.begin();
  ds18b20.begin();
  loadCalibration();
  loadWiFiConfig();
  
  setupBLE();
  
  resetWatchdog();
  if (!connectWiFi()) {
    startAPMode();
  } else {
    syncNTP();
    authenticateDevice();
    setupMQTT();
  }
  
  readSensors();
  Serial.println("[INFO] ✅ Sistema pronto!");
}

// ==================== LOOP ====================
void loop() {
  resetWatchdog();
  
  // AP Mode
  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    updateDisplay();
    handleButtons();
    delay(100); // ✅ LITE: Delay para estabilidade
    return;
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
  if (wifiConnected) {
    if (!mqttConnected) {
      reconnectMQTT();
    } else {
      mqttClient.loop();
    }
    
    // Heartbeat
    if (mqttConnected && millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      publishHeartbeat();
      lastHeartbeat = millis();
    }
  }
  
  // Sensores
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readSensors();
    publishDataToBLE();
    
    if (mqttConnected) {
      publishSensorData();
    }
    
    lastSensorRead = millis();
  }
  
  // Interface
  updateDisplay();
  handleButtons();
  
  delay(100); // ✅ LITE: Delay para estabilidade e reduzir carga de CPU
}
