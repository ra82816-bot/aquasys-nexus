/*
 * Firmware ESP32 Sensores v3.1 - FASE 2 (COM BLE)
 * 
 * Novidades v3.1:
 * - Comunicação BLE como fallback (Server)
 * - TLS 1.3 para MQTT com certificado válido
 * - Ativação automática de BLE após 3 min sem MQTT
 * - Protocolo BLE simplificado com dados críticos
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ==================== CONFIGURAÇÕES ====================
#define FIRMWARE_VERSION "3.1-BLE"
#define DEVICE_TYPE "sensor"

// Pinos
#define PH_SENSOR_PIN 34
#define EC_SENSOR_PIN 35
#define DHT_PIN 4
#define DS18B20_PIN 5
#define BUTTON_UP_PIN 12
#define BUTTON_DOWN_PIN 13

// Display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Intervalos otimizados
#define SENSOR_READ_INTERVAL 30000
#define MQTT_PUBLISH_INTERVAL 60000
#define HEARTBEAT_INTERVAL 30000
#define DISPLAY_UPDATE_INTERVAL 2000
#define BLE_ACTIVATION_TIMEOUT 180000  // 3 minutos sem MQTT = ativar BLE

// Timeouts
#define WIFI_TIMEOUT_MS 20000
#define MQTT_RECONNECT_INTERVAL 5000
#define WATCHDOG_TIMEOUT_SEC 60

// BLE UUIDs (usar UUIDs únicos para seu projeto)
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// MQTT Broker (HiveMQ com TLS)
const char* mqtt_broker = "broker.hivemq.com";
const int mqtt_port = 8883;  // Porta TLS
const char* mqtt_user = "aquasys";
const char* mqtt_password = "aquasys2024";

// Certificado raiz HiveMQ (Let's Encrypt)
const char* root_ca = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// Tópicos MQTT
const char* TOPIC_SENSORS = "aquasys/sensors/all";
const char* TOPIC_HEARTBEAT = "aquasys/heartbeat";
const char* TOPIC_COMMANDS = "aquasys/commands/#";

// Limites dos sensores
#define PH_MIN 0.0
#define PH_MAX 14.0
#define EC_MIN 0.0
#define EC_MAX 5000.0
#define TEMP_MIN -10.0
#define TEMP_MAX 60.0
#define HUMIDITY_MIN 0.0
#define HUMIDITY_MAX 100.0

// ==================== ESTRUTURAS DE DADOS ====================

struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  
  bool phValid;
  bool ecValid;
  bool airTempValid;
  bool humidityValid;
  bool waterTempValid;
  
  bool hasAnyValidData;
};

// Estrutura BLE simplificada (apenas dados críticos)
struct BLEData {
  float ph;
  float waterTemp;
  float ec;
  uint32_t timestamp;
  uint8_t checksum;
} __attribute__((packed));

struct DiagnosticData {
  uint32_t uptime;
  int32_t wifiRssi;
  String wifiSsid;
  String wifiIp;
  uint32_t wifiReconnects;
  
  bool mqttConnected;
  uint32_t mqttFailedAttempts;
  uint32_t mqttLastMessageAge;
  
  uint32_t freeHeap;
  uint32_t minFreeHeap;
  
  uint32_t sensorReadErrors;
  uint32_t publishErrors;
  
  uint32_t bootCount;
  uint32_t crashCount;
  
  bool bleActive;
  uint32_t bleConnections;
};

enum LogLevel {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR
};

enum Page {
  PAGE_PH_EC,
  PAGE_TEMP_HUMIDITY,
  PAGE_WATER_TEMP,
  PAGE_DIAGNOSTICS,
  PAGE_BLE_STATUS,
  PAGE_TOTAL
};

// ==================== OBJETOS GLOBAIS ====================

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;
DHT dht(DHT_PIN, DHT22);
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

// BLE
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool bleDeviceConnected = false;
bool bleActive = false;

// ==================== VARIÁVEIS GLOBAIS ====================

String deviceUUID = "";
String mqttClientId = "";
String wifiSSID = "";
String wifiPassword = "";
bool isAPMode = false;

SensorData currentData;
DiagnosticData diagnostics;

bool wifiConnected = false;
bool mqttConnected = false;

unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastMqttReconnect = 0;
unsigned long lastSuccessfulMqtt = 0;
unsigned long bootTime = 0;

Page currentPage = PAGE_PH_EC;

float phCalibSlope = 3.5;
float phCalibIntercept = 0.0;
float ecCalibFactor = 1.0;

// ==================== CALLBACKS BLE ====================

class ServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleDeviceConnected = true;
    diagnostics.bleConnections++;
    logMessage(LOG_INFO, "Cliente BLE conectado");
  }

  void onDisconnect(BLEServer* pServer) {
    bleDeviceConnected = false;
    logMessage(LOG_INFO, "Cliente BLE desconectado");
    // Reiniciar advertising
    BLEDevice::startAdvertising();
  }
};

// ==================== PROTÓTIPOS ====================

void generateDeviceUUID();
void loadConfig();
void saveWiFiConfig(const String& ssid, const String& pass);
void connectWiFi();
void checkWiFi();
void startAPMode();
void setupWebServer();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void initWatchdog();
void resetWatchdog();
void readSensors();
void publishSensorData();
void publishHeartbeat();
void updateDisplay();
void handleButtons();
void logMessage(LogLevel level, const char* message);
void checkRecovery();
void updateDiagnostics();
void setupBLE();
void publishViaBLE();
void checkBLEActivation();
uint8_t calculateChecksum(BLEData* data);

// ==================== FUNÇÕES DE LOG ====================

void logMessage(LogLevel level, const char* message) {
  String prefix;
  switch(level) {
    case LOG_DEBUG: prefix = "[DEBUG]"; break;
    case LOG_INFO:  prefix = "[INFO] "; break;
    case LOG_WARN:  prefix = "[WARN] "; break;
    case LOG_ERROR: prefix = "[ERROR]"; break;
  }
  
  char timestamp[32];
  unsigned long seconds = millis() / 1000;
  sprintf(timestamp, "[%02lu:%02lu:%02lu]", 
          seconds / 3600, (seconds % 3600) / 60, seconds % 60);
  
  Serial.printf("%s %s %s\n", timestamp, prefix.c_str(), message);
}

// ==================== UUID ====================

void generateDeviceUUID() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  
  char uuid[20];
  sprintf(uuid, "HYDRO-%02X%02X-%02X%02X-%02X%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  deviceUUID = String(uuid);
  mqttClientId = "sensor_" + deviceUUID;
  
  logMessage(LOG_INFO, ("Device UUID: " + deviceUUID).c_str());
}

// ==================== WATCHDOG ====================

void initWatchdog() {
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
  esp_task_wdt_add(NULL);
  logMessage(LOG_INFO, "Watchdog inicializado");
}

void resetWatchdog() {
  esp_task_wdt_reset();
}

// ==================== CONFIGURAÇÃO ====================

void loadConfig() {
  preferences.begin("hydrosmart", false);
  
  wifiSSID = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_pass", "");
  
  phCalibSlope = preferences.getFloat("ph_slope", 3.5);
  phCalibIntercept = preferences.getFloat("ph_intcpt", 0.0);
  ecCalibFactor = preferences.getFloat("ec_factor", 1.0);
  
  diagnostics.bootCount = preferences.getUInt("boot_count", 0) + 1;
  diagnostics.crashCount = preferences.getUInt("crash_count", 0);
  
  preferences.putUInt("boot_count", diagnostics.bootCount);
  preferences.end();
  
  logMessage(LOG_INFO, ("Boot count: " + String(diagnostics.bootCount)).c_str());
}

void saveWiFiConfig(const String& ssid, const String& pass) {
  preferences.begin("hydrosmart", false);
  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_pass", pass);
  preferences.end();
  
  logMessage(LOG_INFO, "Configuração WiFi salva");
}

// ==================== WIFI ====================

void connectWiFi() {
  if (wifiSSID.length() == 0) {
    logMessage(LOG_WARN, "Sem credenciais WiFi, iniciando modo AP");
    startAPMode();
    return;
  }
  
  logMessage(LOG_INFO, ("Conectando ao WiFi: " + wifiSSID).c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
    resetWatchdog();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    diagnostics.wifiSsid = WiFi.SSID();
    diagnostics.wifiIp = WiFi.localIP().toString();
    diagnostics.wifiRssi = WiFi.RSSI();
    
    logMessage(LOG_INFO, ("WiFi conectado! IP: " + diagnostics.wifiIp).c_str());
  } else {
    logMessage(LOG_ERROR, "Falha ao conectar WiFi, iniciando modo AP");
    diagnostics.wifiReconnects++;
    startAPMode();
  }
}

void checkWiFi() {
  if (isAPMode) return;
  
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      wifiConnected = false;
      logMessage(LOG_WARN, "WiFi desconectado");
      diagnostics.wifiReconnects++;
    }
    WiFi.reconnect();
  } else {
    if (!wifiConnected) {
      wifiConnected = true;
      diagnostics.wifiRssi = WiFi.RSSI();
      logMessage(LOG_INFO, "WiFi reconectado");
    }
  }
}

void startAPMode() {
  isAPMode = true;
  String apSSID = "HydroSmart_" + deviceUUID.substring(6);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), "12345678");
  
  logMessage(LOG_INFO, ("Modo AP: " + apSSID).c_str());
  logMessage(LOG_INFO, ("IP: " + WiFi.softAPIP().toString()).c_str());
  
  setupWebServer();
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<title>HydroSmart Setup</title></head><body>";
    html += "<h1>WiFi - " + deviceUUID + "</h1>";
    html += "<form action='/save' method='POST'>";
    html += "SSID: <input name='ssid' required><br>";
    html += "Senha: <input name='pass' type='password' required><br>";
    html += "<input type='submit' value='Salvar'></form></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/save", HTTP_POST, []() {
    saveWiFiConfig(server.arg("ssid"), server.arg("pass"));
    server.send(200, "text/html", 
      "<html><body><h1>Salvo! Reiniciando...</h1></body></html>");
    delay(2000);
    ESP.restart();
  });
  
  server.begin();
}

// ==================== MQTT ====================

void setupMQTT() {
  // Configurar TLS com certificado
  espClient.setCACert(root_ca);
  
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(15);
  
  logMessage(LOG_INFO, "MQTT TLS configurado");
}

void reconnectMQTT() {
  if (isAPMode || !wifiConnected) return;
  
  unsigned long now = millis();
  if (now - lastMqttReconnect < MQTT_RECONNECT_INTERVAL) return;
  lastMqttReconnect = now;
  
  if (!mqttClient.connected()) {
    logMessage(LOG_INFO, "Conectando MQTT TLS...");
    
    if (mqttClient.connect(mqttClientId.c_str(), mqtt_user, mqtt_password)) {
      mqttConnected = true;
      lastSuccessfulMqtt = now;
      mqttClient.subscribe(TOPIC_COMMANDS);
      logMessage(LOG_INFO, "MQTT TLS conectado");
      diagnostics.mqttFailedAttempts = 0;
    } else {
      mqttConnected = false;
      diagnostics.mqttFailedAttempts++;
      logMessage(LOG_ERROR, ("Falha MQTT: " + String(mqttClient.state())).c_str());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  lastSuccessfulMqtt = millis();
  logMessage(LOG_DEBUG, ("Msg: " + String(topic)).c_str());
}

// ==================== BLE ====================

void setupBLE() {
  logMessage(LOG_INFO, "Inicializando BLE...");
  
  BLEDevice::init(("HydroSmart_" + deviceUUID).c_str());
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | 
    BLECharacteristic::PROPERTY_NOTIFY
  );
  
  pCharacteristic->addDescriptor(new BLE2902());
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  bleActive = true;
  diagnostics.bleActive = true;
  
  logMessage(LOG_INFO, "BLE Server ativo");
}

uint8_t calculateChecksum(BLEData* data) {
  uint8_t* bytes = (uint8_t*)data;
  uint8_t checksum = 0;
  for (size_t i = 0; i < sizeof(BLEData) - 1; i++) {
    checksum ^= bytes[i];
  }
  return checksum;
}

void publishViaBLE() {
  if (!bleActive || !currentData.hasAnyValidData) return;
  
  BLEData bleData;
  bleData.ph = currentData.phValid ? currentData.ph : 0.0f;
  bleData.waterTemp = currentData.waterTempValid ? currentData.waterTemp : 0.0f;
  bleData.ec = currentData.ecValid ? currentData.ec : 0.0f;
  bleData.timestamp = millis() / 1000;
  bleData.checksum = calculateChecksum(&bleData);
  
  pCharacteristic->setValue((uint8_t*)&bleData, sizeof(BLEData));
  pCharacteristic->notify();
  
  logMessage(LOG_DEBUG, "Dados enviados via BLE");
}

void checkBLEActivation() {
  if (bleActive) return;
  
  unsigned long now = millis();
  
  // Ativar BLE se MQTT offline por > 3 min
  if (!mqttConnected && (now - lastSuccessfulMqtt > BLE_ACTIVATION_TIMEOUT)) {
    logMessage(LOG_WARN, "MQTT offline > 3min, ativando BLE fallback");
    setupBLE();
  }
}

// ==================== SENSORES ====================

float readAverageADC(int pin, int samples = 10) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return sum / (float)samples;
}

void readSensors() {
  memset(&currentData, 0, sizeof(SensorData));
  currentData.hasAnyValidData = false;
  
  // pH
  float phVoltage = (readAverageADC(PH_SENSOR_PIN) / 4095.0) * 3.3;
  currentData.ph = phCalibSlope * phVoltage + phCalibIntercept;
  currentData.phValid = (currentData.ph >= PH_MIN && currentData.ph <= PH_MAX && 
                        !isnan(currentData.ph) && !isinf(currentData.ph));
  
  // EC
  float ecVoltage = (readAverageADC(EC_SENSOR_PIN) / 4095.0) * 3.3;
  currentData.ec = (ecVoltage * 1000.0) * ecCalibFactor;
  currentData.ecValid = (currentData.ec >= EC_MIN && currentData.ec <= EC_MAX && 
                        !isnan(currentData.ec) && !isinf(currentData.ec));
  
  // DHT22
  currentData.airTemp = dht.readTemperature();
  currentData.humidity = dht.readHumidity();
  currentData.airTempValid = (currentData.airTemp >= TEMP_MIN && 
                              currentData.airTemp <= TEMP_MAX && 
                              !isnan(currentData.airTemp));
  currentData.humidityValid = (currentData.humidity >= HUMIDITY_MIN && 
                               currentData.humidity <= HUMIDITY_MAX && 
                               !isnan(currentData.humidity));
  
  // DS18B20
  ds18b20.requestTemperatures();
  currentData.waterTemp = ds18b20.getTempCByIndex(0);
  currentData.waterTempValid = (currentData.waterTemp >= TEMP_MIN && 
                                currentData.waterTemp <= TEMP_MAX && 
                                currentData.waterTemp != DEVICE_DISCONNECTED_C);
  
  currentData.hasAnyValidData = currentData.phValid || currentData.ecValid || 
                                currentData.airTempValid || currentData.humidityValid || 
                                currentData.waterTempValid;
  
  if (!currentData.hasAnyValidData) {
    diagnostics.sensorReadErrors++;
    logMessage(LOG_ERROR, "Nenhum sensor válido!");
  }
}

// ==================== PUBLICAÇÃO ====================

void publishSensorData() {
  // Tentar MQTT primeiro
  if (mqttConnected && currentData.hasAnyValidData) {
    StaticJsonDocument<256> doc;
    doc["device_uuid"] = deviceUUID;
    
    if (currentData.phValid) doc["ph"] = currentData.ph;
    if (currentData.ecValid) doc["ec"] = currentData.ec;
    if (currentData.airTempValid) doc["airTemp"] = currentData.airTemp;
    if (currentData.humidityValid) doc["humidity"] = currentData.humidity;
    if (currentData.waterTempValid) doc["waterTemp"] = currentData.waterTemp;
    
    char buffer[256];
    serializeJson(doc, buffer);
    
    if (mqttClient.publish(TOPIC_SENSORS, buffer, false)) {
      lastSuccessfulMqtt = millis();
      logMessage(LOG_INFO, "Dados via MQTT");
      return;
    } else {
      diagnostics.publishErrors++;
    }
  }
  
  // Fallback: publicar via BLE
  if (bleActive) {
    publishViaBLE();
  }
}

void publishHeartbeat() {
  if (!mqttConnected) return;
  
  updateDiagnostics();
  
  StaticJsonDocument<512> doc;
  doc["device"] = "ESP32_Sensor_" + deviceUUID;
  doc["device_uuid"] = deviceUUID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["uptime"] = diagnostics.uptime;
  
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["ssid"] = diagnostics.wifiSsid;
  wifi["rssi"] = diagnostics.wifiRssi;
  wifi["ip"] = diagnostics.wifiIp;
  wifi["reconnects"] = diagnostics.wifiReconnects;
  
  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["connected"] = diagnostics.mqttConnected;
  mqtt["failed_attempts"] = diagnostics.mqttFailedAttempts;
  mqtt["last_message_age_ms"] = diagnostics.mqttLastMessageAge;
  
  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = diagnostics.freeHeap;
  memory["min_free_heap"] = diagnostics.minFreeHeap;
  
  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["ph_valid"] = currentData.phValid;
  sensors["ec_valid"] = currentData.ecValid;
  sensors["temp_valid"] = currentData.airTempValid;
  sensors["humidity_valid"] = currentData.humidityValid;
  sensors["water_temp_valid"] = currentData.waterTempValid;
  
  JsonObject ble = doc.createNestedObject("ble");
  ble["active"] = diagnostics.bleActive;
  ble["connected"] = bleDeviceConnected;
  ble["connections"] = diagnostics.bleConnections;
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  mqttClient.publish(TOPIC_HEARTBEAT, buffer, false);
}

void updateDiagnostics() {
  diagnostics.uptime = millis() / 1000;
  diagnostics.wifiRssi = WiFi.RSSI();
  diagnostics.mqttConnected = mqttClient.connected();
  diagnostics.freeHeap = ESP.getFreeHeap();
  diagnostics.minFreeHeap = ESP.getMinFreeHeap();
  diagnostics.bleActive = bleActive;
  
  if (lastSuccessfulMqtt > 0) {
    diagnostics.mqttLastMessageAge = millis() - lastSuccessfulMqtt;
  }
}

// ==================== DISPLAY ====================

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println(deviceUUID);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  switch(currentPage) {
    case PAGE_PH_EC:
      display.setCursor(0, 15);
      display.print("pH: ");
      display.setTextSize(2);
      display.println(currentData.phValid ? String(currentData.ph, 2) : "---");
      display.setTextSize(1);
      display.print("EC: ");
      display.setTextSize(2);
      display.print(currentData.ecValid ? String(currentData.ec, 0) : "---");
      break;
      
    case PAGE_WATER_TEMP:
      display.setCursor(0, 25);
      display.println("Temp Agua:");
      display.setTextSize(3);
      display.print(currentData.waterTempValid ? String(currentData.waterTemp, 1) : "---");
      break;
      
    case PAGE_BLE_STATUS:
      display.setCursor(0, 15);
      display.println("=== BLE STATUS ===");
      display.print("Ativo: ");
      display.println(bleActive ? "SIM" : "NAO");
      display.print("Conectado: ");
      display.println(bleDeviceConnected ? "SIM" : "NAO");
      display.print("Conexoes: ");
      display.println(diagnostics.bleConnections);
      if (bleActive && !mqttConnected) {
        display.println("MODO FALLBACK");
      }
      break;
      
    case PAGE_DIAGNOSTICS:
      display.setCursor(0, 15);
      display.print("WiFi: ");
      display.println(wifiConnected ? "OK" : "ERRO");
      display.print("MQTT: ");
      display.println(mqttConnected ? "OK" : "ERRO");
      display.print("BLE: ");
      display.println(bleActive ? "ON" : "OFF");
      display.print("Heap: ");
      display.print(diagnostics.freeHeap / 1024);
      display.println(" KB");
      break;
  }
  
  // Rodapé
  display.drawLine(0, 55, 128, 55, SSD1306_WHITE);
  display.setCursor(0, 57);
  display.print(wifiConnected ? "W" : "w");
  display.print(" ");
  display.print(mqttConnected ? "M" : "m");
  display.print(" ");
  display.print(bleActive ? "B" : "b");
  display.print(" ");
  display.print(currentData.hasAnyValidData ? "S" : "s");
  
  display.display();
}

void handleButtons() {
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  
  if (now - lastPress < 200) return;
  
  if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
    currentPage = (Page)((currentPage + 1) % PAGE_TOTAL);
    lastPress = now;
  }
  
  if (digitalRead(BUTTON_UP_PIN) == LOW) {
    currentPage = (Page)((currentPage - 1 + PAGE_TOTAL) % PAGE_TOTAL);
    lastPress = now;
  }
}

// ==================== RECOVERY ====================

void checkRecovery() {
  if (diagnostics.freeHeap < 50000) {
    logMessage(LOG_WARN, "Memória baixa!");
  }
  
  if (diagnostics.mqttFailedAttempts > 20) {
    logMessage(LOG_ERROR, "Muitas falhas MQTT, reiniciando...");
    ESP.restart();
  }
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  logMessage(LOG_INFO, "=== HydroSmart Sensor v3.1-BLE ===");
  
  generateDeviceUUID();
  initWatchdog();
  
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    logMessage(LOG_ERROR, "Display falhou");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("HydroSmart v3.1");
    display.println(deviceUUID);
    display.println("BLE+TLS Ready");
    display.display();
  }
  
  loadConfig();
  
  dht.begin();
  ds18b20.begin();
  
  connectWiFi();
  setupMQTT();
  
  logMessage(LOG_INFO, "Setup completo!");
  
  readSensors();
  updateDisplay();
}

// ==================== LOOP ====================

void loop() {
  resetWatchdog();
  
  unsigned long now = millis();
  
  if (isAPMode) {
    server.handleClient();
    delay(10);
    return;
  }
  
  checkWiFi();
  reconnectMQTT();
  
  if (mqttConnected) {
    mqttClient.loop();
  }
  
  // Verificar ativação de BLE
  checkBLEActivation();
  
  // Leitura de sensores (30s)
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }
  
  // Publicação (60s)
  if (now - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMqttPublish = now;
    publishSensorData();
  }
  
  // Heartbeat (30s)
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    publishHeartbeat();
  }
  
  // Display (2s)
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
  
  handleButtons();
  
  if (now % 60000 < 100) {
    checkRecovery();
  }
  
  delay(10);
}