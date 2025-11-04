/*
 * AquaSys Nexus - Sensor Module v4.2.0-SENSOR
 * ============================================
 * ✅ ALINHAMENTO COM ARQUITETURA v4.2.x DO ATUADOR:
 * - Autenticação dinâmica com Supabase (device-auth)
 * - Certificado SSL correto (DST Root CA X3)
 * - WDT corrigido para Core 3.x (esp_task_wdt_deinit)
 * - Sincronização NTP ANTES de autenticação/MQTT
 * - Lazy initialization do BLE (economia de RAM)
 * - Backoff exponencial no MQTT (5s → 300s)
 * - Sistema de logging estruturado (INFO/WARN/ERROR/DEBUG)
 * - OTA seguro (usa espClient global com SSL)
 * - WDT reset no loop AP mode (evita boot loop)
 * 
 * DIFERENÇAS DO MÓDULO ATUADOR:
 * - device_type = "sensor" (não "actuator")
 * - BLE fallback permanente (não reinicia após 10min)
 * - Publica em TOPIC_SENSORS (não TOPIC_RELAY_STATUS)
 * 
 * Mantém features v3.1.7:
 * - Leitura de 5 sensores (pH, EC, Temp Ar/Água, Umidade)
 * - Calibração remota via MQTT
 * - Display OLED com navegação por botões
 * - Média móvel para estabilização de leituras
 * - Heartbeat detalhado com diagnóstico
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WebServer.h>
#include <esp_mac.h>
#include <HTTPClient.h>
#include <Update.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ----------------------------- VERSÃO ----------------------------------------------
#define FIRMWARE_VERSION "4.2.0-SENSOR"

// ----------------------------- CERTIFICADO ROOT (DST Root CA X3) -------------------
const char* ROOT_CA_CERT = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/
MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT
DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow
PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD
Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB
AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O
rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq
OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b
xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw
7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD
aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV
HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG
SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69
ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr
AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz
R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5
JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo
Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ
-----END CERTIFICATE-----
)EOF";

// ----------------------------- CERTIFICADO MQTT (HiveMQ) ---------------------------
const char* MQTT_ROOT_CA = R"EOF(
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
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X+1mvoiBOv/2X/qkSsisRcOj/KK
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

// ----------------------------- AUTENTICAÇÃO DINÂMICA -------------------------------
const char* AUTH_SERVER = "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth";
const char* AUTH_HEADER_KEY = "Authorization";
const char* AUTH_HEADER_VALUE = "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw";

#define SSL_INSECURE_MODE false  // true = ignora SSL (apenas debug!)

// ----------------------------- MQTT CONFIG (Dinâmico) ------------------------------
const char* MQTT_BROKER = "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8884;

String mqttUsername = "";
String mqttPassword = "";

// ----------------------------- TÓPICOS MQTT ----------------------------------------
const char* TOPIC_SENSORS = "aquasys/sensors/all";
const char* TOPIC_HEARTBEAT = "aquasys/heartbeat";
const char* TOPIC_CALIBRATION = "aquasys/calibration/request";
const char* TOPIC_CALIBRATION_RESPONSE = "aquasys/calibration/response";
const char* TOPIC_OTA = "aquasys/ota/request";

// ----------------------------- UUIDs do BLE ----------------------------------------
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_PH        "a01e482a-a90d-4b55-a4b5-12c8ab8660e5"
#define CHAR_UUID_EC        "4c4b5266-1601-443b-a5af-d54aea031336"
#define CHAR_UUID_WATER_TEMP "e9e4f07a-4c2d-42c2-8099-026858e9d6d2"
#define CHAR_UUID_AIR_TEMP  "5c02b66e-d900-47e5-8f83-047f3b06346a"
#define CHAR_UUID_HUMIDITY  "c0b1a0e8-32a7-4467-a508-a40e1183c58e"

// ----------------------------- PINOUT ----------------------------------------------
#define PH_SENSOR_PIN 34
#define EC_SENSOR_PIN 35
#define DHT_PIN 4
#define ONE_WIRE_BUS 2
#define DHT_TYPE DHT22

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define BUTTON_UP 32
#define BUTTON_DOWN 33
#define BUTTON_SELECT 25
#define BUTTON_BACK 26

// ----------------------------- TIMEOUTS --------------------------------------------
#define SENSOR_READ_INTERVAL 30000
#define MQTT_PUBLISH_INTERVAL 60000
#define HEARTBEAT_INTERVAL 60000
#define WIFI_RECONNECT_INTERVAL 120000
#define MQTT_RECONNECT_INTERVAL 15000
#define DISPLAY_UPDATE_INTERVAL 5000
#define BLE_FALLBACK_TIMEOUT 180000  // 3 minutos
#define WDT_TIMEOUT 30  // 30 segundos

// ----------------------------- LIMITES ---------------------------------------------
#define PH_MIN 0.0
#define PH_MAX 14.0
#define EC_MIN 0.0
#define EC_MAX 5000.0
#define TEMP_MIN -10.0
#define TEMP_MAX 60.0
#define HUMIDITY_MIN 0.0
#define HUMIDITY_MAX 100.0

struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  bool valid;
  bool phValid;
  bool ecValid;
  bool airTempValid;
  bool humidityValid;
  bool waterTempValid;
};

enum Page {
  SHOW_PH,
  SHOW_EC,
  SHOW_AIR_TEMP,
  SHOW_WATER_TEMP,
  SHOW_HUMIDITY,
  PAGE_COUNT
};

enum CalibrationMode {
  CAL_NONE,
  CAL_MENU,
  CAL_PH_7,
  CAL_PH_4,
  CAL_EC_LOW,
  CAL_EC_HIGH,
  CAL_EC_LOW_VALUE,
  CAL_EC_HIGH_VALUE
};

enum LogLevel {
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_DEBUG
};

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Preferences prefs;
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

BLEServer* pServer = NULL;
BLECharacteristic* pPhCharacteristic = NULL;
BLECharacteristic* pEcCharacteristic = NULL;
BLECharacteristic* pWaterTempCharacteristic = NULL;
BLECharacteristic* pAirTempCharacteristic = NULL;
BLECharacteristic* pHumidityCharacteristic = NULL;

SensorData currentData;
String ssid_sta = "";
String password_sta = "";
String deviceUUID = "";
String mqttClientId = "";

bool wifiConnected = false;
bool mqttConnected = false;
bool wifiConfigured = false;
bool apMode = false;
bool bleServerAtivo = false;
bool deviceConnectedBLE = false;

unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastDebounce[4] = {0, 0, 0, 0};
unsigned long lastMqttOkTime = 0;
unsigned long bleStartTime = 0;

unsigned long mqtt_backoff_ms = 5000;
const unsigned long MAX_BACKOFF = 300000;
unsigned long wifi_reconnects = 0;
unsigned long mqtt_failed_attempts = 0;
uint32_t min_free_heap = 4294967295;

Page currentPage = SHOW_PH;
CalibrationMode calibrationMode = CAL_NONE;
int calibrationMenuIndex = 0;
const unsigned long debounceDelay = 200;

float cal_ph7_voltage = 2.52f;
float cal_ph4_voltage = 3.29f;
float ph_slope = 0.0f;
float ph_intercept = 7.0f;

float cal_ec_low_raw = 645.0f;
float cal_ec_high_raw = 2850.0f;
float cal_ec_low = 360.0f;
float cal_ec_high = 4588.0f;
float temp_ec_low_value = 360.0f;
float temp_ec_high_value = 4588.0f;

#define MOVING_AVG_SIZE 5
float phReadings[MOVING_AVG_SIZE];
float ecReadings[MOVING_AVG_SIZE];
int phReadingIndex = 0;
int ecReadingIndex = 0;
bool phFilterReady = false;
bool ecFilterReady = false;

// ----------------------------- COMPATIBILIDADE ----------------------------------
// IMPORTANTE - Tipos da Biblioteca BLE:
// - BLEAdvertisedDevice.getName() → retorna std::string
// - BLECharacteristic.setValue() → aceita float/int/String
// - Sempre verificar o tipo de retorno antes de converter
//
// IMPORTANTE - Cliente SSL Global:
// - espClient: Usado para autenticação HTTP (Supabase) E MQTT (HiveMQ)
// - Certificado é configurado em setupMQTT() após autenticação
// - OTA reutiliza espClient (não cria novo WiFiClientSecure)
//
// ----------------------------- PROTÓTIPOS ---------------------------------------
void setupDeviceUUID();
void loadConfig();
void saveWiFiConfig(const String& ssid, const String& pass);
void saveCalibration();
void calculatePHCoefficients();
void initWatchdog();
void resetWatchdog();
void connectWiFi();
void checkWiFi();
void startAPMode();
void setupWebServer();
bool authenticateDevice();
void setupMQTT();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void performOTA(String url, String new_version);
void setupBLE();
void stopBLE();
void readSensors();
void publishSensorData();
void publishHeartbeat();
void publishCalibrationData();
void handleButtons();
void handleCalibrationButtons();
void updateDisplay();
void displayMessage(const char* message);
void displayCalibrationMenu();
void enterCalibrationMode();
void exitCalibrationMode();
void performCalibrationStep();
float readAverageADC(int pin, int samples);
float readPhSensor();
float readEcSensor(float waterTemp);
bool isValidValue(float value, float min, float max);
float updateMovingAverage(float readings[], int &index, bool &isReady, float newValue);
void logMessage(LogLevel level, String message);

// ----------------------------- BLE Callbacks ------------------------------------
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnectedBLE = true;
      logMessage(LOG_INFO, "[BLE] Cliente Conectado");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnectedBLE = false;
      logMessage(LOG_INFO, "[BLE] Cliente Desconectado");
      BLEDevice::startAdvertising();
    }
};

// ----------------------------- LOGGING ESTRUTURADO -------------------------------
void logMessage(LogLevel level, String message) {
  String levelStr;
  switch (level) {
    case LOG_INFO:  levelStr = "INFO"; break;
    case LOG_WARN:  levelStr = "WARN"; break;
    case LOG_ERROR: levelStr = "ERROR"; break;
    case LOG_DEBUG: levelStr = "DEBUG"; break;
  }
  
  unsigned long uptime = millis() / 1000;
  Serial.printf("[%s][%lu] %s\n", levelStr.c_str(), uptime, message.c_str());
}

// ----------------------------- UTIL ---------------------------------------------
bool isValidValue(float value, float min, float max) {
  return !isnan(value) && !isinf(value) && value >= min && value <= max;
}

float updateMovingAverage(float readings[], int &index, bool &isReady, float newValue) {
  readings[index] = newValue;
  index = (index + 1) % MOVING_AVG_SIZE;
  
  if (!isReady && index == 0) {
    isReady = true;
  }
  
  int count = isReady ? MOVING_AVG_SIZE : (index == 0 ? MOVING_AVG_SIZE : index);
  if (count == 0) return newValue;

  float sum = 0;
  for (int i = 0; i < count; i++) {
    sum += readings[i];
  }
  return sum / count;
}

// ----------------------------- UUID ----------------------------------------------
void setupDeviceUUID() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  
  char uuid[25];
  sprintf(uuid, "SEN-%02X%02X%02X%02X%02X%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  deviceUUID = String(uuid);
  mqttClientId = "sensor_" + deviceUUID;
}

// ----------------------------- WATCHDOG (CORRIGIDO) -------------------------------
void initWatchdog() {
  esp_err_t deinit_result = esp_task_wdt_deinit();
  if (deinit_result != ESP_OK) {
    logMessage(LOG_DEBUG, "Falha ao des-init WDT (pode ser o 1º boot): " + String(deinit_result));
  }
  
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_err_t init_result = esp_task_wdt_init(&wdt_config);
  
  if (init_result == ESP_OK) {
    esp_task_wdt_add(NULL);
    logMessage(LOG_INFO, "Watchdog inicializado (30s timeout)");
  } else {
    logMessage(LOG_ERROR, "Falha ao inicializar WDT: " + String(init_result));
  }
}

void resetWatchdog() {
  esp_task_wdt_reset();
}

// ----------------------------- CONFIG & CALIBRAÇÃO -------------------------------
void loadConfig() {
  setupDeviceUUID();
  Serial.printf("[INFO] Device UUID: %s\n", deviceUUID.c_str());
  Serial.printf("[INFO] MQTT Client ID: %s\n", mqttClientId.c_str());
  
  prefs.begin("config", true);
  ssid_sta = prefs.getString("ssid", "");
  password_sta = prefs.getString("pass", "");
  prefs.end();
  
  prefs.begin("calib", true);
  cal_ph7_voltage = prefs.getFloat("ph7v", 2.52f);
  cal_ph4_voltage = prefs.getFloat("ph4v", 3.29f);
  cal_ec_low_raw = prefs.getFloat("ec_low_raw", 645.0f);
  cal_ec_high_raw = prefs.getFloat("ec_high_raw", 2850.0f);
  cal_ec_low = prefs.getFloat("ec_low", 360.0f);
  cal_ec_high = prefs.getFloat("ec_high", 4588.0f);
  prefs.end();
  
  calculatePHCoefficients();
}

void saveWiFiConfig(const String& ssid, const String& pass) {
  prefs.begin("config", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  logMessage(LOG_INFO, "WiFi configurado e salvo!");
}

void saveCalibration() {
  prefs.begin("calib", false);
  prefs.putFloat("ph7v", cal_ph7_voltage);
  prefs.putFloat("ph4v", cal_ph4_voltage);
  prefs.putFloat("ec_low_raw", cal_ec_low_raw);
  prefs.putFloat("ec_high_raw", cal_ec_high_raw);
  prefs.putFloat("ec_low", cal_ec_low);
  prefs.putFloat("ec_high", cal_ec_high);
  prefs.end();
  
  calculatePHCoefficients();
  logMessage(LOG_INFO, "Calibração salva!");
  publishCalibrationData();
}

void calculatePHCoefficients() {
  float denom = cal_ph7_voltage - cal_ph4_voltage;
  if (fabs(denom) < 0.001f) {
    ph_slope = 0.0f;
    ph_intercept = 7.0f;
  } else {
    ph_slope = 3.0f / denom;
    ph_intercept = 7.0f - ph_slope * cal_ph7_voltage;
  }
}

// ----------------------------- AUTENTICAÇÃO (NOVA) -------------------------------
bool authenticateDevice() {
  if (!wifiConnected) return false;
  
  logMessage(LOG_INFO, "Autenticando dispositivo...");
  
  if (SSL_INSECURE_MODE) {
    espClient.setInsecure();
    logMessage(LOG_WARN, "⚠️ SSL_INSECURE_MODE ativo (apenas debug!)");
  } else {
    espClient.setCACert(ROOT_CA_CERT);
    logMessage(LOG_INFO, "✅ SSL seguro - validando certificado Supabase");
  }
  
  HTTPClient https;
  https.begin(espClient, AUTH_SERVER);
  https.addHeader("Content-Type", "application/json");
  https.addHeader(AUTH_HEADER_KEY, AUTH_HEADER_VALUE);
  
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = "sensor";
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  logMessage(LOG_DEBUG, "Request: " + requestBody);
  
  int httpCode = https.POST(requestBody);
  logMessage(LOG_DEBUG, "HTTP Code: " + String(httpCode));
  
  if (httpCode == 200) {
    String response = https.getString();
    StaticJsonDocument<1024> responseDoc;
    deserializeJson(responseDoc, response);
    
    if (responseDoc.containsKey("mqtt_config")) {
      JsonObject mqttConfig = responseDoc["mqtt_config"];
      mqttUsername = mqttConfig["username"].as<String>();
      mqttPassword = mqttConfig["password"].as<String>();
      
      logMessage(LOG_INFO, "✅ Autenticação bem-sucedida!");
      https.end();
      return true;
    }
  }
  
  logMessage(LOG_ERROR, "Autenticação falhou. HTTP: " + String(httpCode));
  https.end();
  return false;
}

// ----------------------------- WiFi, AP Mode, Web Server functions -------------------
void connectWiFi() {
  if (ssid_sta.length() == 0) {
    logMessage(LOG_ERROR, "Sem credenciais WiFi!");
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
  
  Serial.printf("[INFO] Conectando WiFi: %s", ssid_sta.c_str());
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
    resetWatchdog();
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiConfigured = true;
    Serial.printf("[INFO] WiFi conectado! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[INFO] RSSI: %d dBm\n", WiFi.RSSI());
    
    String msg = "WiFi OK\nIP: " + WiFi.localIP().toString();
    displayMessage(msg.c_str());
    delay(2000);
  } else {
    wifiConnected = false;
    wifiConfigured = false;
    logMessage(LOG_ERROR, "Falha ao conectar WiFi");
    displayMessage("WiFi Falhou\nIniciando AP...");
    delay(2000);
    startAPMode();
  }
}

void checkWiFi() {
  unsigned long now = millis();
  if (now - lastWifiCheck < WIFI_RECONNECT_INTERVAL) return;
  lastWifiCheck = now;
  
  if (WiFi.status() != WL_CONNECTED && !apMode) {
    wifiConnected = false;
    logMessage(LOG_ERROR, "WiFi desconectado. Reconectando...");
    wifi_reconnects++;
    WiFi.disconnect();
    delay(100);
    WiFi.reconnect();
  } else if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
  }
}

void startAPMode() {
  logMessage(LOG_INFO, "Iniciando Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("AquaSys-Sensor-AP");
  apMode = true;
  
  IPAddress IP = WiFi.softAPIP();
  Serial.printf("[INFO] AP IP: %s\n", IP.toString().c_str());
  
  String msg = "Modo AP\nRede: AquaSys-AP\nIP: " + IP.toString();
  displayMessage(msg.c_str());
  
  setupWebServer();
  server.begin();
  logMessage(LOG_INFO, "Servidor web iniciado no modo AP");
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>AquaSys Config</title>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;}";
    html += "h1{color:#2c3e50;}form{background:white;padding:20px;border-radius:8px;}";
    html += "input{width:100%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:4px;}";
    html += "button{background:#3498db;color:white;padding:12px 30px;border:none;border-radius:4px;cursor:pointer;}";
    html += "button:hover{background:#2980b9;}</style></head><body>";
    html += "<h1>🌊 AquaSys Sensor - Config WiFi</h1>";
    html += "<form action='/save' method='POST'>";
    html += "<label>SSID:</label><input type='text' name='ssid' value='" + ssid_sta + "' required><br>";
    html += "<label>Senha:</label><input type='password' name='password' value='" + password_sta + "'><br>";
    html += "<button type='submit'>Salvar e Conectar</button>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
      ssid_sta = server.arg("ssid");
      password_sta = server.arg("password");
      saveWiFiConfig(ssid_sta, password_sta);
      
      String resp = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
      resp += "<h1>✅ Configuração Salva!</h1>";
      resp += "<p>O ESP32 vai reiniciar e tentar conectar ao WiFi: <b>" + ssid_sta + "</b></p>";
      resp += "<p>Aguarde 30 segundos...</p>";
      resp += "</body></html>";
      server.send(200, "text/html", resp);
      
      Serial.println("[INFO] WiFi configurado via web. Reiniciando...");
      delay(2000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Requisição inválida");
    }
  });
  
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });
}

// ----------------------------- OTA (SEGURO) --------------------------------------
void performOTA(String url, String new_version) {
  logMessage(LOG_INFO, "Iniciando atualização OTA...");
  
  String msg1 = "Atualizando...\n" + new_version;
  displayMessage(msg1.c_str());
  
  HTTPClient http;
  logMessage(LOG_DEBUG, "URL OTA: " + url);
  
  if (http.begin(espClient, url)) {
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      
      if (contentLength <= 0) {
        logMessage(LOG_ERROR, "OTA falhou: Content-Length inválido");
        http.end();
        displayMessage("OTA Falhou\n(Tamanho Inv.)");
        delay(3000);
        return;
      }
      
      logMessage(LOG_INFO, "Tamanho firmware: " + String(contentLength) + " bytes");
      
      if (!Update.begin(contentLength)) {
        logMessage(LOG_ERROR, "Erro Update.begin(): " + String(Update.errorString()));
        http.end();
        displayMessage("OTA Falhou\n(Update.begin)");
        delay(3000);
        return;
      }
      
      logMessage(LOG_INFO, "Gravando firmware...");
      size_t written = Update.writeStream(http.getStream());
      
      if (written != contentLength) {
        logMessage(LOG_ERROR, "Erro de escrita: Esperado " + String(contentLength) + ", escrito " + String(written));
        http.end();
        displayMessage("OTA Falhou\n(Escrita)");
        delay(3000);
        return;
      }
      
      if (!Update.end() || !Update.isFinished()) {
        logMessage(LOG_ERROR, "Falha na atualização: " + String(Update.errorString()));
        http.end();
        displayMessage("OTA Falhou");
        delay(3000);
        return;
      }
      
      logMessage(LOG_INFO, "✅ OTA concluído! Reiniciando...");
      displayMessage("OK! Reiniciando...");
      delay(2000);
      ESP.restart();
    } else {
      logMessage(LOG_ERROR, "Falha no GET. HTTP: " + String(httpCode));
      String msg2 = "OTA Falhou\n(HTTP " + String(httpCode) + ")";
      displayMessage(msg2.c_str());
      delay(3000);
    }
    http.end();
  } else {
    logMessage(LOG_ERROR, "Falha ao iniciar HTTP client");
    displayMessage("OTA Falhou\n(HTTP begin)");
    delay(3000);
  }
}

// ----------------------------- BLE (LAZY INIT) ------------------------------------
void setupBLE() {
  logMessage(LOG_INFO, "Iniciando BLE Server (Modo Fallback)...");
  displayMessage("Modo Fallback\nBLE Ativo");

  BLEDevice::init("AquaSys-Sensor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pPhCharacteristic = pService->createCharacteristic(
                      CHAR_UUID_PH,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pPhCharacteristic->addDescriptor(new BLE2902());

  pEcCharacteristic = pService->createCharacteristic(
                      CHAR_UUID_EC,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pEcCharacteristic->addDescriptor(new BLE2902());

  pWaterTempCharacteristic = pService->createCharacteristic(
                      CHAR_UUID_WATER_TEMP,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pWaterTempCharacteristic->addDescriptor(new BLE2902());

  pAirTempCharacteristic = pService->createCharacteristic(
                      CHAR_UUID_AIR_TEMP,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pAirTempCharacteristic->addDescriptor(new BLE2902());

  pHumidityCharacteristic = pService->createCharacteristic(
                      CHAR_UUID_HUMIDITY,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pHumidityCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  bleServerAtivo = true;
  bleStartTime = millis();
  logMessage(LOG_INFO, "BLE Server ativado. Aguardando conexão do Atuador.");
}

void stopBLE() {
  logMessage(LOG_INFO, "Desativando BLE Server...");
  
  BLEDevice::stopAdvertising();

  if (deviceConnectedBLE) {
      uint16_t connId = pServer->getConnId();
      pServer->disconnect(connId);
  }

  BLEDevice::deinit(true);
  
  bleServerAtivo = false;
  deviceConnectedBLE = false;
  logMessage(LOG_INFO, "BLE Server desativado. Retornando ao modo MQTT.");
  displayMessage("Rede OK\nModo MQTT");
  delay(2000);
}

// ----------------------------- MQTT (BACKOFF EXPONENCIAL) -------------------------
void setupMQTT() {
  espClient.setCACert(MQTT_ROOT_CA);
  espClient.setHandshakeTimeout(10);
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setBufferSize(1024);
  mqttClient.setSocketTimeout(10);
  
  logMessage(LOG_INFO, "MQTT configurado");
}

bool reconnectMQTT() {
  if (mqttConnected || !wifiConnected) return false;
  
  unsigned long now = millis();
  if (now - lastMqttAttempt < mqtt_backoff_ms) return false;
  
  lastMqttAttempt = now;
  
  logMessage(LOG_INFO, "Tentando MQTT... (tentativa " + String(mqtt_failed_attempts + 1) + ", backoff " + String(mqtt_backoff_ms / 1000) + "s)");
  
  resetWatchdog();
  
  if (mqttClient.connect(mqttClientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
    mqttConnected = true;
    mqtt_failed_attempts = 0;
    mqtt_backoff_ms = 5000;
    
    mqttClient.subscribe(TOPIC_CALIBRATION);
    mqttClient.subscribe(TOPIC_OTA);
    
    logMessage(LOG_INFO, "✅ MQTT conectado!");
    displayMessage("MQTT Conectado");
    delay(1000);
    return true;
  } else {
    mqtt_failed_attempts++;
    espClient.stop();
    
    mqtt_backoff_ms = min(mqtt_backoff_ms * 2, MAX_BACKOFF);
    
    logMessage(LOG_ERROR, "❌ MQTT falhou. Código: " + String(mqttClient.state()));
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[512];
  unsigned int len = min(length, (unsigned int)511);
  memcpy(msg, payload, len);
  msg[len] = '\0';
  
  Serial.printf("[MQTT] %s: %s\n", topic, msg);
  
  String topicStr = String(topic);
  
  if (topicStr == TOPIC_CALIBRATION) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, msg);
    
    if (error) {
      logMessage(LOG_ERROR, "Falha ao parsear JSON de calibração");
      return;
    }

    String cmd_uuid = doc["device_uuid"].as<String>();
    if (cmd_uuid != deviceUUID) {
      Serial.printf("[CAL] Comando ignorado. UUID não confere (%s)\n", cmd_uuid.c_str());
      return;
    }

    String sensor = doc["sensor"].as<String>();
    String action = doc["action"].as<String>();

    if (sensor == "ph") {
      if (action == "calibrate_7") {
        float voltage = readAverageADC(PH_SENSOR_PIN, 10) * (3.3f / 4095.0f);
        cal_ph7_voltage = voltage;
        saveCalibration();
        Serial.printf("[CAL] pH 7.0 calibrado: %.3fV\n", voltage);
      } 
      else if (action == "calibrate_4") {
        float voltage = readAverageADC(PH_SENSOR_PIN, 10) * (3.3f / 4095.0f);
        cal_ph4_voltage = voltage;
        saveCalibration();
        Serial.printf("[CAL] pH 4.0 calibrado: %.3fV\n", voltage);
      }
    } 
    else if (sensor == "ec") {
      float expected_val = doc["expected_value"].as<float>();
      
      if (action == "calibrate_low") {
        float raw = readAverageADC(EC_SENSOR_PIN, 10);
        cal_ec_low_raw = raw;
        cal_ec_low = expected_val;
        saveCalibration();
        Serial.printf("[CAL] EC baixa calibrada: %.0f @ %.0f uS\n", raw, cal_ec_low);
      }
      else if (action == "calibrate_high") {
        float raw = readAverageADC(EC_SENSOR_PIN, 10);
        cal_ec_high_raw = raw;
        cal_ec_high = expected_val;
        saveCalibration();
        Serial.printf("[CAL] EC alta calibrada: %.0f @ %.0f uS\n", raw, cal_ec_high);
      }
    }
  }
  else if (topicStr == TOPIC_OTA) {
    logMessage(LOG_INFO, "Recebido comando OTA");
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, msg);
    
    if (error) {
      logMessage(LOG_ERROR, "Falha ao parsear JSON de OTA");
      return;
    }
    
    String cmd_uuid = doc["device_uuid"].as<String>();
    if (cmd_uuid != deviceUUID) {
      Serial.printf("[OTA] Comando ignorado. UUID não confere (%s)\n", cmd_uuid.c_str());
      return;
    }
    
    String firmware_url = doc["firmware_url"].as<String>();
    String version = doc["version"].as<String>();

    if (version == FIRMWARE_VERSION) {
      Serial.printf("[OTA] Ignorando. Já estou na versão %s\n", version.c_str());
      return;
    }
    
    if (firmware_url.length() > 0) {
      performOTA(firmware_url, version);
    }
  }
}

// ----------------------------- Sensores, publicação, display, botões ----------------
float readAverageADC(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(5);
  }
  return (float)sum / (float)samples;
}

float readPhSensor() {
  float raw = readAverageADC(PH_SENSOR_PIN, 10);
  float voltage = raw * (3.3f / 4095.0f);
  float ph_instant = ph_slope * voltage + ph_intercept;
  float ph_smoothed = updateMovingAverage(phReadings, phReadingIndex, phFilterReady, ph_instant);
  return ph_smoothed;
}

float readEcSensor(float waterTemp) {
  float raw = readAverageADC(EC_SENSOR_PIN, 10);
  
  float denom = cal_ec_high_raw - cal_ec_low_raw;
  if (denom == 0) denom = 1.0f;
  float slope = (cal_ec_high - cal_ec_low) / denom;
  float ec_instant = cal_ec_low + slope * (raw - cal_ec_low_raw);
  
  if (ec_instant < 0) ec_instant = 0;
  
  if (isValidValue(waterTemp, TEMP_MIN, TEMP_MAX)) {
    ec_instant = ec_instant / (1.0f + 0.02f * (waterTemp - 25.0f));
  }
  
  float ec_smoothed = updateMovingAverage(ecReadings, ecReadingIndex, ecFilterReady, ec_instant);
  return ec_smoothed;
}

void readSensors() {
  resetWatchdog();
  
  ds18b20.requestTemperatures();
  currentData.waterTemp = ds18b20.getTempCByIndex(0);
  if (currentData.waterTemp == DEVICE_DISCONNECTED_C) {
    currentData.waterTemp = NAN;
  }
  currentData.airTemp = dht.readTemperature();
  currentData.humidity = dht.readHumidity();
  currentData.ph = readPhSensor();
  currentData.ec = readEcSensor(currentData.waterTemp);
  
  currentData.phValid = isValidValue(currentData.ph, PH_MIN, PH_MAX);
  currentData.ecValid = isValidValue(currentData.ec, EC_MIN, EC_MAX);
  currentData.airTempValid = isValidValue(currentData.airTemp, TEMP_MIN, TEMP_MAX);
  currentData.humidityValid = isValidValue(currentData.humidity, HUMIDITY_MIN, HUMIDITY_MAX);
  currentData.waterTempValid = isValidValue(currentData.waterTemp, TEMP_MIN, TEMP_MAX);
  currentData.valid = currentData.phValid && currentData.ecValid && 
                      currentData.airTempValid && currentData.humidityValid && 
                      currentData.waterTempValid;
  
  if (currentData.valid) {
    Serial.printf("[SENSOR] pH=%.2f EC=%.0f T.Ar=%.1f°C Umid=%.1f%% T.Agua=%.1f°C\n",
                  currentData.ph, currentData.ec, currentData.airTemp, 
                  currentData.humidity, currentData.waterTemp);
  } else {
    logMessage(LOG_ERROR, "Leitura inválida de um ou mais sensores.");
  }

  if (bleServerAtivo && deviceConnectedBLE) {
    if (currentData.phValid) {
      pPhCharacteristic->setValue(currentData.ph);
      pPhCharacteristic->notify();
    }
    if (currentData.ecValid) {
      pEcCharacteristic->setValue(currentData.ec);
      pEcCharacteristic->notify();
    }
    if (currentData.waterTempValid) {
      pWaterTempCharacteristic->setValue(currentData.waterTemp);
      pWaterTempCharacteristic->notify();
    }
    if (currentData.airTempValid) {
      pAirTempCharacteristic->setValue(currentData.airTemp);
      pAirTempCharacteristic->notify();
    }
    if (currentData.humidityValid) {
      pHumidityCharacteristic->setValue(currentData.humidity);
      pHumidityCharacteristic->notify();
    }
    Serial.println("[BLE] Características atualizadas e notificadas.");
  }
}

void publishSensorData() {
  if (!mqttConnected || !currentData.valid) return;
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["timestamp"] = millis();
  doc["ph"] = String(currentData.ph, 2);
  doc["ec"] = String(currentData.ec, 0);
  doc["temperature"] = String(currentData.airTemp, 1);
  doc["humidity"] = String(currentData.humidity, 1);
  doc["waterTemp"] = String(currentData.waterTemp, 1);
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_SENSORS, payload.c_str())) {
    Serial.println("[MQTT] Dados dos sensores publicados");
  } else {
    Serial.println("[ERROR] Falha ao publicar dados dos sensores");
  }
}

void publishHeartbeat() {
  if (!mqttConnected) return;
  
  StaticJsonDocument<512> doc;
  
  doc["device_uuid"] = deviceUUID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["uptime"] = millis() / 1000;

  JsonObject wifi = doc.createNestedObject("wifi");
  if (wifiConnected) {
    wifi["ssid"] = WiFi.SSID();
    wifi["rssi"] = WiFi.RSSI();
    wifi["ip"] = WiFi.localIP().toString();
  } else {
    wifi["ssid"] = "N/A";
    wifi["rssi"] = 0;
    wifi["ip"] = "0.0.0.0";
  }
  wifi["reconnects"] = wifi_reconnects;

  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["connected"] = mqttConnected;
  mqtt["failed_attempts"] = mqtt_failed_attempts;

  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["ph_valid"] = currentData.phValid;
  sensors["ec_valid"] = currentData.ecValid;
  sensors["temp_valid"] = currentData.airTempValid;
  sensors["humidity_valid"] = currentData.humidityValid;
  sensors["water_temp_valid"] = currentData.waterTempValid;

  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = ESP.getFreeHeap();
  memory["min_free_heap"] = min_free_heap;
  
  JsonObject ble = doc.createNestedObject("ble");
  ble["active"] = bleServerAtivo;
  ble["connected"] = deviceConnectedBLE;
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_HEARTBEAT, payload.c_str())) {
    Serial.println("[MQTT] Heartbeat enviado");
  }
}

void publishCalibrationData() {
  if (!mqttConnected) return;
  
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["cal_ph7_voltage"] = cal_ph7_voltage;
  doc["cal_ph4_voltage"] = cal_ph4_voltage;
  doc["cal_ec_low_raw"] = cal_ec_low_raw;
  doc["cal_ec_high_raw"] = cal_ec_high_raw;
  doc["cal_ec_low"] = cal_ec_low;
  doc["cal_ec_high"] = cal_ec_high;
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_CALIBRATION_RESPONSE, payload.c_str())) {
    Serial.println("[MQTT] Dados de calibração publicados");
  }
}

void displayMessage(const char* message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}

void updateDisplay() {
  unsigned long now = millis();
  if (now - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) return;
  lastDisplayUpdate = now;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  if (apMode) {
    display.println("Modo AP");
    display.println("AquaSys-Sensor-AP");
    display.println(WiFi.softAPIP().toString());
  } else if (!wifiConnected) {
    display.println("WiFi desconectado");
  } else if (!mqttConnected) {
    display.println("MQTT desconectado");
  } else {
    display.printf("pH: %.2f\n", currentData.ph);
    display.printf("EC: %.0f uS\n", currentData.ec);
    display.printf("T.Ar: %.1fC\n", currentData.airTemp);
    display.printf("Umid: %.1f%%\n", currentData.humidity);
    display.printf("T.H2O: %.1fC\n", currentData.waterTemp);
  }
  
  display.display();
}

void handleButtons() {
  // Implement button logic here if needed
}

// ----------------------------- SETUP (ORDEM CORRETA) ------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("[INFO] ═══════════════════════════════════════");
  Serial.printf("[INFO] AquaSys Sensor %s\n", FIRMWARE_VERSION);
  Serial.println("[INFO] ═══════════════════════════════════════");
  
  // 1. Hardware
  Wire.begin();
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);
  
  // 2. Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERROR] Display OLED não encontrado!");
  }
  displayMessage("AquaSys Sensor\nIniciando...");
  
  // 3. Sensores
  dht.begin();
  ds18b20.begin();
  
  // 4. Watchdog
  initWatchdog();
  resetWatchdog();
  
  // 5. Config
  loadConfig();
  
  // 6. WiFi
  resetWatchdog();
  if (ssid_sta.length() > 0) {
    connectWiFi();
  } else {
    startAPMode();
    return;
  }
  
  // 7. NTP (CRÍTICO: Antes de qualquer SSL!)
  resetWatchdog();
  logMessage(LOG_INFO, "Configurando NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  logMessage(LOG_INFO, "Aguardando sincronização NTP...");
  unsigned long ntpStart = millis();
  while (time(nullptr) < 100000 && millis() - ntpStart < 10000) {
    delay(100);
    resetWatchdog();
  }
  
  if (time(nullptr) < 100000) {
    logMessage(LOG_ERROR, "⚠️ NTP não sincronizou! SSL pode falhar.");
  } else {
    logMessage(LOG_INFO, "✅ NTP sincronizado");
  }
  
  // 8. Autenticação (APÓS NTP!)
  resetWatchdog();
  if (!authenticateDevice()) {
    logMessage(LOG_ERROR, "❌ Autenticação falhou! Verifique conexão.");
    displayMessage("Auth Falhou\nVerifique logs");
    delay(5000);
    ESP.restart();
  }
  
  // 9. MQTT (APÓS autenticação!)
  resetWatchdog();
  setupMQTT();
  reconnectMQTT();
  
  lastMqttOkTime = millis();
  
  // 10. Mensagem final
  displayMessage("Sistema OK\nMQTT Ativo");
  delay(2000);
  
  logMessage(LOG_INFO, "Setup completo!");
}

// ----------------------------- LOOP (BLE LAZY INIT) --------------------------------
void loop() {
  resetWatchdog();
  
  min_free_heap = min(min_free_heap, ESP.getFreeHeap());
  
  unsigned long now = millis();
  
  // AP Mode
  if (apMode) {
    resetWatchdog();  // ✅ CRÍTICO: Evitar WDT crash no AP mode
    server.handleClient();
    delay(10);
    return;
  }
  
  // WiFi Check
  if (!apMode) {
    checkWiFi();
  }
  
  // MQTT Reconnect
  if (wifiConnected && !mqttConnected && !apMode) {
    reconnectMQTT();
  }
  
  // MQTT Loop
  if (mqttConnected) {
    mqttClient.loop();
  }

  // BLE Fallback Logic (Lazy Init)
  if (mqttConnected) {
    lastMqttOkTime = now;
    
    if (bleServerAtivo) {
      logMessage(LOG_INFO, "MQTT reconectado. Desligando BLE...");
      stopBLE();
    }
  } else if (!apMode) {
    unsigned long tempoSemMqtt = now - lastMqttOkTime;
    
    if (!bleServerAtivo && tempoSemMqtt > BLE_FALLBACK_TIMEOUT) {
      logMessage(LOG_INFO, "MQTT off por 3min. Ativando BLE fallback...");
      setupBLE();
    }
    
    // Sensor mantém BLE ativo indefinidamente (diferente do Atuador)
  }
  
  // Rotinas Principais
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }
  
  if (now - lastMqttPublish >= MQTT_PUBLISH_INTERVAL && wifiConnected) {
    lastMqttPublish = now;
    publishSensorData();
  }
  
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL && wifiConnected) {
    lastHeartbeat = now;
    publishHeartbeat();
  }
  
  updateDisplay();
  handleButtons();
  
  delay(10);
}
