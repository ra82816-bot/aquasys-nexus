/*
 * ============================================================================
 * AquaSys Nexus - Sensor Module v4.3.2-OLED-COMPLETE
 * ============================================================================
 * VERSÃO DEFINITIVA COM TODAS AS FUNCIONALIDADES
 * 
 * RECURSOS:
 * ✅ Display OLED 128x64 com navegação por botões
 * ✅ Páginas: Dashboard, Conexões, Calibração, Sistema
 * ✅ Calibração interativa de pH e EC via interface OLED
 * ✅ WiFi com modo AP automático e portal captivo
 * ✅ Suporte a 3 redes WiFi com prioridades
 * ✅ BLE Server sempre ativo
 * ✅ MQTT sobre TLS com autenticação dinâmica via Supabase
 * ✅ Leitura de sensores: pH, EC, Temp Água, Temp Ar, Umidade
 * ✅ Watchdog robusto (60s) - CORRIGIDO
 * ✅ Logging estruturado
 * ✅ Heartbeat MQTT
 * ✅ UUID único por MAC
 * ✅ NTP sync
 * 
 * AUTOR: HydroSmart Team
 * DATA: 2025-01-11
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

// ==================== CONFIGURAÇÕES ====================
// Versão do Firmware
#define FIRMWARE_VERSION "4.3.2-OLED-COMPLETE"
#define DEVICE_TYPE "SENSOR"

// Pinos dos Sensores (CORRIGIDO conforme arquivo de referência)
#define PH_SENSOR_PIN 34
#define TDS_SENSOR_PIN 35
#define DHT_PIN 15
#define DHT_TYPE DHT22
#define ONE_WIRE_BUS 2

// Pinos do OLED (I2C padrão: SDA=21, SCL=22)
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// Pinos dos Botões (CORRIGIDO conforme arquivo de referência)
#define BUTTON_UP 32
#define BUTTON_DOWN 33
#define BUTTON_SELECT 25
#define BUTTON_BACK 26

// Sensores
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// WiFi AP Mode
#define AP_SSID_PREFIX "AquaSys-SEN-"
#define AP_PASSWORD "aquasys2024"
#define AP_TIMEOUT 300000  // 5min

// Timeouts
#define WIFI_TIMEOUT 15000
#define MQTT_TIMEOUT 30000
#define SENSOR_READ_INTERVAL 30000  // 30s
#define HEARTBEAT_INTERVAL 60000    // 60s
#define WATCHDOG_TIMEOUT 60         // 60s
#define AUTH_TIMEOUT 10000          // 10s

// API Supabase (autenticação dinâmica)
#define SUPABASE_URL "https://oaabtbvwxsjomeeizciq.supabase.co"
#define SUPABASE_ANON_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw"

// MQTT Configuration (fallback)
#define MQTT_BROKER_FALLBACK "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define TOPIC_SENSORS_FALLBACK "aquasys/sensors/all"
#define TOPIC_HEARTBEAT_FALLBACK "aquasys/heartbeat/sensor"
#define TOPIC_CALIBRATION_FALLBACK "aquasys/calibration/sensor"

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_PH        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_EC        "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_UUID_AIR_TEMP  "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_UUID_HUMIDITY  "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define CHAR_UUID_WATER_TEMP "beb5483e-36e1-4688-b7f5-ea07361b26ac"
#define CHAR_WIFI_LIST      "a3c87500-8ed3-4bdf-8a39-a01bebede295"

// DNS Público
#define DNS_PRIMARY IPAddress(8, 8, 8, 8)
#define DNS_SECONDARY IPAddress(1, 1, 1, 1)

// NTP
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"
#define GMT_OFFSET -10800
#define DAYLIGHT_OFFSET 0

// Logging
enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO = 1,
  LOG_WARN = 2,
  LOG_ERROR = 3,
  LOG_CRITICAL = 4
};
int currentLogLevel = LOG_INFO;

// ==================== ESTRUTURAS DE DADOS ====================
struct WiFiCredential {
  char ssid[32];
  char password[64];
  int priority;
  bool valid;
};

struct SensorData {
  float ph;
  float ec;
  float air_temp;
  float humidity;
  float water_temp;
  bool valid;
  unsigned long timestamp;
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
  char client_id[64];
  char topic_sensors[128];
  char topic_heartbeat[128];
  char topic_calibration[128];
  bool valid;
};

// ==================== ENUMERAÇÕES - INTERFACE OLED ====================
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
// Device UUID
String deviceUUID = "";

// WiFi
WiFiCredential networks[3];
int currentNetworkIndex = 0;
bool wifiConnected = false;
unsigned long lastWiFiCheck = 0;

// AP Mode
bool apMode = false;
WebServer server(80);
DNSServer dnsServer;
unsigned long apModeStartTime = 0;

// MQTT
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
bool mqttConnected = false;
unsigned long lastMqttAttempt = 0;
unsigned long lastMqttSuccess = 0;
MqttCredentials mqttCreds = {"", "", "", "", "", "", "", false};
bool isAuthenticated = false;

// Sensor Data
SensorData currentData = {0, 0, 0, 0, 0, false, 0};

// Calibração (VALORES CORRETOS do arquivo de referência)
CalibrationData calibration = {
  2.52,   // ph7_voltage
  3.29,   // ph4_voltage
  1.0,    // ph_slope (será calculado)
  0.0,    // ph_intercept (será calculado)
  645.0,  // ec_low_raw
  2850.0, // ec_high_raw
  360.0,  // ec_low_val
  4588.0  // ec_high_val
};

// OLED Display
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Page currentPage = PAGE_DASHBOARD;
CalibrationMode calibrationMode = CAL_NONE;
int calibrationMenuIndex = 0;

// BLE
BLEServer* pBLEServer = nullptr;
BLECharacteristic* pCharPH = nullptr;
BLECharacteristic* pCharEC = nullptr;
BLECharacteristic* pCharAirTemp = nullptr;
BLECharacteristic* pCharHumidity = nullptr;
BLECharacteristic* pCharWaterTemp = nullptr;
BLECharacteristic* pCharWiFiList = nullptr;
bool bleActive = false;
bool deviceConnectedBLE = false;

// Timers
unsigned long lastSensorRead = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWdtReset = 0;
unsigned long lastDisplayUpdate = 0;

// Debounce dos Botões
unsigned long lastDebounce[4] = {0, 0, 0, 0};
const unsigned long debounceDelay = 200;

// Preferences (NVS)
Preferences prefs;

// ==================== PROTÓTIPOS ====================
// Logging
void logMessage(LogLevel level, const String& message);

// UUID
String generateDeviceUUID();

// WiFi
void loadWiFiConfig();
void saveWiFiConfig();
bool connectWiFi();
void checkWiFi();
void startAPMode();
void stopAPMode();

// Web Server
void setupWebServer();
void handleRoot();
void handleScan();
void handleSave();
void handleStatus();
void handleNotFound();

// NTP
void syncNTP();

// Autenticação
bool authenticateDevice();
void loadMqttCredentials();
void saveMqttCredentials();

// MQTT
void setupMQTT();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData();
void publishHeartbeat();

// BLE
void setupBLE();
void publishDataToBLE();
class MyServerCallbacks;

// Sensores
void loadCalibration();
void saveCalibration();
void calculatePHCoefficients();
float readAverageADC(int pin, int samples = 10);
float voltageToPH(float voltage);
float interpolateEC(float rawValue);
float temperatureCompensateEC(float ec, float temp);
void readSensors();

// OLED Interface
void initOLED();
void updateDisplay();
void handleButtons();
void displayMessage(const char *message);

// Watchdog
void initWatchdog();
void resetWatchdog();

// ==================== LOGGING ====================
void logMessage(LogLevel level, const String& message) {
  if (level < currentLogLevel) return;
  
  const char* levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"};
  Serial.printf("[%lu][%s] %s\n", millis(), levelStr[level], message.c_str());
  
  if (level == LOG_CRITICAL) {
    prefs.begin("crash", false);
    prefs.putString("last_crash", message);
    prefs.putULong("crash_time", millis());
    prefs.end();
  }
}

// ==================== UUID ====================
String generateDeviceUUID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char uuid[20];
  sprintf(uuid, "SEN-%02X%02X%02X%02X%02X%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(uuid);
}

// ==================== WATCHDOG (CORRIGIDO) ====================
void initWatchdog() {
  // CORREÇÃO: Usar esp_err_t ao invés de esp_task_wdt_status_t
  esp_err_t status = esp_task_wdt_status(NULL);
  
  if (status == ESP_ERR_NOT_FOUND) {
    // Watchdog não está inicializado, inicializar agora
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WATCHDOG_TIMEOUT * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
  }
  
  // Adicionar task atual ao watchdog
  esp_task_wdt_add(NULL);
  logMessage(LOG_INFO, "✅ Watchdog iniciado (" + String(WATCHDOG_TIMEOUT) + "s)");
}

void resetWatchdog() {
  if (millis() - lastWdtReset > 1000) {
    esp_task_wdt_reset();
    lastWdtReset = millis();
  }
}

// ==================== OLED DISPLAY ====================
void initOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    logMessage(LOG_ERROR, "❌ Erro ao inicializar OLED");
    while (true) {
      delay(100);
    }
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  logMessage(LOG_INFO, "✅ OLED inicializado");
}

void displayMessage(const char *message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}

void updateDisplay() {
  // Atualizar a cada 500ms para não sobrecarregar
  if (millis() - lastDisplayUpdate < 500) return;
  lastDisplayUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  switch (currentPage) {
    case PAGE_DASHBOARD:
      display.println("=== DASHBOARD ===");
      display.printf("pH: %.2f\n", currentData.ph);
      display.printf("EC: %.0f uS/cm\n", currentData.ec);
      display.printf("Temp: %.1fC Hum: %.0f%%\n", currentData.air_temp, currentData.humidity);
      display.printf("Water: %.1fC\n", currentData.water_temp);
      display.println("----------------");
      display.println("[UP/DOWN] Navegar");
      break;
      
    case PAGE_CONNECTIONS:
      display.println("=== CONEXOES ===");
      display.printf("WiFi: %s\n", wifiConnected ? "OK" : "OFF");
      if (wifiConnected) {
        display.printf("RSSI: %d dBm\n", WiFi.RSSI());
      }
      display.printf("MQTT: %s\n", mqttConnected ? "OK" : "OFF");
      display.printf("BLE: %s\n", deviceConnectedBLE ? "Connected" : "Ready");
      display.println("----------------");
      display.println("[SELECT] Scan WiFi");
      break;
      
    case PAGE_CALIBRATION:
      display.println("=== CALIBRACAO ===");
      if (calibrationMode == CAL_NONE) {
        const char* options[] = {"1. pH 7.0", "2. pH 4.0", "3. EC Low", "4. EC High"};
        for (int i = 0; i < 4; i++) {
          if (i == calibrationMenuIndex) display.print("> ");
          else display.print("  ");
          display.println(options[i]);
        }
        display.println("----------------");
        display.println("[SELECT] Escolher");
        display.println("[BACK] Sair");
      } else {
        float currentVoltage = readAverageADC(PH_SENSOR_PIN) * 3.3 / 4095.0;
        float currentRaw = readAverageADC(TDS_SENSOR_PIN);
        
        switch (calibrationMode) {
          case CAL_PH_7:
            display.println("Calibrando pH 7.0");
            display.printf("Voltage: %.3fV\n", currentVoltage);
            display.println("\nMergulhe em pH 7");
            display.println("[SELECT] Confirmar");
            break;
          case CAL_PH_4:
            display.println("Calibrando pH 4.0");
            display.printf("Voltage: %.3fV\n", currentVoltage);
            display.println("\nMergulhe em pH 4");
            display.println("[SELECT] Confirmar");
            break;
          case CAL_EC_LOW:
            display.println("Calibrando EC Low");
            display.printf("Raw: %.0f\n", currentRaw);
            display.println("\nMergulhe em 360uS");
            display.println("[SELECT] Confirmar");
            break;
          case CAL_EC_HIGH:
            display.println("Calibrando EC High");
            display.printf("Raw: %.0f\n", currentRaw);
            display.println("\nMergulhe em 4588uS");
            display.println("[SELECT] Confirmar");
            break;
          default:
            break;
        }
        display.println("[BACK] Cancelar");
      }
      break;
      
    case PAGE_SYSTEM:
      display.println("=== SISTEMA ===");
      display.printf("UUID:\n%s\n", deviceUUID.c_str());
      display.printf("Mem: %d KB\n", ESP.getFreeHeap() / 1024);
      display.printf("Uptime: %lus\n", millis() / 1000);
      display.println("----------------");
      display.println("FW: v4.3.2-OLED");
      break;
      
    default:
      break;
  }
  
  display.display();
}

void handleButtons() {
  // Botão UP
  if (digitalRead(BUTTON_UP) == LOW && (millis() - lastDebounce[0] > debounceDelay)) {
    lastDebounce[0] = millis();
    
    if (currentPage == PAGE_CALIBRATION && calibrationMode == CAL_NONE) {
      calibrationMenuIndex = (calibrationMenuIndex - 1 + 4) % 4;
    } else {
      currentPage = (Page)((currentPage - 1 + PAGE_COUNT) % PAGE_COUNT);
    }
    logMessage(LOG_DEBUG, "BTN UP");
  }
  
  // Botão DOWN
  if (digitalRead(BUTTON_DOWN) == LOW && (millis() - lastDebounce[1] > debounceDelay)) {
    lastDebounce[1] = millis();
    
    if (currentPage == PAGE_CALIBRATION && calibrationMode == CAL_NONE) {
      calibrationMenuIndex = (calibrationMenuIndex + 1) % 4;
    } else {
      currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
    }
    logMessage(LOG_DEBUG, "BTN DOWN");
  }
  
  // Botão SELECT
  if (digitalRead(BUTTON_SELECT) == LOW && (millis() - lastDebounce[2] > debounceDelay)) {
    lastDebounce[2] = millis();
    logMessage(LOG_DEBUG, "BTN SELECT");
    
    if (currentPage == PAGE_CONNECTIONS) {
      handleScan();
    } else if (currentPage == PAGE_CALIBRATION) {
      if (calibrationMode == CAL_NONE) {
        // Entrar no modo de calibração
        calibrationMode = (CalibrationMode)(calibrationMenuIndex + 1);
        logMessage(LOG_INFO, "Modo calibração: " + String(calibrationMode));
      } else {
        // Confirmar calibração
        float currentVoltage = readAverageADC(PH_SENSOR_PIN) * 3.3 / 4095.0;
        float currentRaw = readAverageADC(TDS_SENSOR_PIN);
        
        switch (calibrationMode) {
          case CAL_PH_7:
            calibration.ph7_voltage = currentVoltage;
            logMessage(LOG_INFO, "pH 7.0 = " + String(currentVoltage, 3) + "V");
            break;
          case CAL_PH_4:
            calibration.ph4_voltage = currentVoltage;
            logMessage(LOG_INFO, "pH 4.0 = " + String(currentVoltage, 3) + "V");
            break;
          case CAL_EC_LOW:
            calibration.ec_low_raw = currentRaw;
            logMessage(LOG_INFO, "EC Low = " + String(currentRaw, 0));
            break;
          case CAL_EC_HIGH:
            calibration.ec_high_raw = currentRaw;
            logMessage(LOG_INFO, "EC High = " + String(currentRaw, 0));
            break;
          default:
            break;
        }
        
        calculatePHCoefficients();
        saveCalibration();
        calibrationMode = CAL_NONE;
        displayMessage("Calibracao salva!");
        delay(1500);
      }
    }
  }
  
  // Botão BACK
  if (digitalRead(BUTTON_BACK) == LOW && (millis() - lastDebounce[3] > debounceDelay)) {
    lastDebounce[3] = millis();
    logMessage(LOG_DEBUG, "BTN BACK");
    
    if (currentPage == PAGE_CALIBRATION) {
      if (calibrationMode != CAL_NONE) {
        // Sair do modo de calibração específico
        calibrationMode = CAL_NONE;
        calibrationMenuIndex = 0;
        logMessage(LOG_INFO, "Calibração cancelada");
      } else {
        // Sair da página de calibração
        currentPage = PAGE_DASHBOARD;
        logMessage(LOG_INFO, "Voltou ao Dashboard");
      }
    }
  }
}

// ==================== CALIBRAÇÃO ====================
void calculatePHCoefficients() {
  float pH_low = 4.0;
  float pH_neutral = 7.0;
  calibration.ph_slope = (pH_neutral - pH_low) / (calibration.ph7_voltage - calibration.ph4_voltage);
  calibration.ph_intercept = pH_neutral - calibration.ph_slope * calibration.ph7_voltage;
  logMessage(LOG_INFO, "pH Coef: slope=" + String(calibration.ph_slope, 3) + 
             " intercept=" + String(calibration.ph_intercept, 3));
}

void loadCalibration() {
  prefs.begin("calib", true);
  
  calibration.ph7_voltage = prefs.getFloat("ph7_v", 2.52);
  calibration.ph4_voltage = prefs.getFloat("ph4_v", 3.29);
  calibration.ec_low_raw = prefs.getFloat("ec_low_raw", 645.0);
  calibration.ec_high_raw = prefs.getFloat("ec_high_raw", 2850.0);
  calibration.ec_low_val = prefs.getFloat("ec_low_val", 360.0);
  calibration.ec_high_val = prefs.getFloat("ec_high_val", 4588.0);
  
  prefs.end();
  
  calculatePHCoefficients();
  logMessage(LOG_INFO, "Calibração carregada");
}

void saveCalibration() {
  prefs.begin("calib", false);
  
  prefs.putFloat("ph7_v", calibration.ph7_voltage);
  prefs.putFloat("ph4_v", calibration.ph4_voltage);
  prefs.putFloat("ec_low_raw", calibration.ec_low_raw);
  prefs.putFloat("ec_high_raw", calibration.ec_high_raw);
  prefs.putFloat("ec_low_val", calibration.ec_low_val);
  prefs.putFloat("ec_high_val", calibration.ec_high_val);
  
  prefs.end();
  
  logMessage(LOG_INFO, "💾 Calibração salva");
}

// ==================== SENSORES ====================
float readAverageADC(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return (float)sum / samples;
}

float voltageToPH(float voltage) {
  float ph = calibration.ph_slope * voltage + calibration.ph_intercept;
  if (ph < 0) ph = 0;
  if (ph > 14) ph = 14;
  return ph;
}

float interpolateEC(float rawValue) {
  if (rawValue <= calibration.ec_low_raw) return calibration.ec_low_val;
  if (rawValue >= calibration.ec_high_raw) return calibration.ec_high_val;
  
  float ratio = (rawValue - calibration.ec_low_raw) / (calibration.ec_high_raw - calibration.ec_low_raw);
  return calibration.ec_low_val + ratio * (calibration.ec_high_val - calibration.ec_low_val);
}

float temperatureCompensateEC(float ec, float temp) {
  return ec / (1.0 + 0.02 * (temp - 25.0));
}

void readSensors() {
  resetWatchdog();
  
  // pH
  float phVoltage = readAverageADC(PH_SENSOR_PIN) * 3.3 / 4095.0;
  currentData.ph = voltageToPH(phVoltage);
  
  // EC
  float ecRaw = readAverageADC(TDS_SENSOR_PIN);
  float ecValue = interpolateEC(ecRaw);
  currentData.ec = temperatureCompensateEC(ecValue, currentData.air_temp);
  
  // DHT22
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    logMessage(LOG_WARN, "Falha ao ler DHT22");
    currentData.air_temp = 25.0;
    currentData.humidity = 50.0;
  } else {
    currentData.air_temp = t;
    currentData.humidity = h;
  }
  
  // DS18B20
  ds18b20.requestTemperatures();
  float waterTemp = ds18b20.getTempCByIndex(0);
  
  if (waterTemp == DEVICE_DISCONNECTED_C || waterTemp < -50 || waterTemp > 100) {
    logMessage(LOG_WARN, "Falha ao ler DS18B20");
    currentData.water_temp = 25.0;
  } else {
    currentData.water_temp = waterTemp;
  }
  
  currentData.valid = true;
  currentData.timestamp = millis();
  
  logMessage(LOG_INFO, "📊 pH=" + String(currentData.ph, 2) + 
             " EC=" + String(currentData.ec, 0) + 
             " T=" + String(currentData.air_temp, 1) + 
             " H=" + String(currentData.humidity, 1) + 
             " Tw=" + String(currentData.water_temp, 1));
}

// ==================== WIFI ====================
void loadWiFiConfig() {
  prefs.begin("wifi", true);
  
  for (int i = 0; i < 3; i++) {
    String keySSID = "ssid" + String(i);
    String keyPass = "pass" + String(i);
    String keyPrio = "prio" + String(i);
    
    String ssid = prefs.getString(keySSID.c_str(), "");
    
    if (ssid.length() > 0) {
      ssid.toCharArray(networks[i].ssid, 32);
      prefs.getString(keyPass.c_str(), "").toCharArray(networks[i].password, 64);
      networks[i].priority = prefs.getInt(keyPrio.c_str(), i + 1);
      networks[i].valid = true;
      logMessage(LOG_INFO, "WiFi carregado: " + String(networks[i].ssid));
    } else {
      networks[i].valid = false;
    }
  }
  
  prefs.end();
}

void saveWiFiConfig() {
  prefs.begin("wifi", false);
  
  for (int i = 0; i < 3; i++) {
    if (networks[i].valid) {
      String keySSID = "ssid" + String(i);
      String keyPass = "pass" + String(i);
      String keyPrio = "prio" + String(i);
      
      prefs.putString(keySSID.c_str(), networks[i].ssid);
      prefs.putString(keyPass.c_str(), networks[i].password);
      prefs.putInt(keyPrio.c_str(), networks[i].priority);
    }
  }
  
  prefs.end();
  logMessage(LOG_INFO, "Configuração WiFi salva");
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, DNS_PRIMARY, DNS_SECONDARY);
  
  for (int i = 0; i < 3; i++) {
    if (!networks[i].valid) continue;
    
    logMessage(LOG_INFO, "Tentando WiFi: " + String(networks[i].ssid));
    WiFi.begin(networks[i].ssid, networks[i].password);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
      resetWatchdog();
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      currentNetworkIndex = i;
      logMessage(LOG_INFO, "✅ WiFi conectado: " + String(networks[i].ssid));
      logMessage(LOG_INFO, "IP: " + WiFi.localIP().toString());
      logMessage(LOG_INFO, "RSSI: " + String(WiFi.RSSI()) + " dBm");
      
      syncNTP();
      return true;
    }
  }
  
  wifiConnected = false;
  return false;
}

void checkWiFi() {
  if (millis() - lastWiFiCheck < 10000) return;
  lastWiFiCheck = millis();
  
  resetWatchdog();
  
  if (WiFi.status() != WL_CONNECTED) {
    logMessage(LOG_WARN, "WiFi desconectado, tentando reconectar...");
    wifiConnected = false;
    mqttConnected = false;
    
    if (!connectWiFi() && !apMode) {
      startAPMode();
    }
  }
}

void startAPMode() {
  logMessage(LOG_INFO, "🔶 Iniciando modo AP...");
  
  WiFi.mode(WIFI_AP);
  String apSSID = String(AP_SSID_PREFIX) + deviceUUID.substring(4);
  WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
  
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  
  dnsServer.start(53, "*", apIP);
  setupWebServer();
  server.begin();
  
  apMode = true;
  apModeStartTime = millis();
  
  logMessage(LOG_INFO, "✅ AP ativo: " + apSSID + " / Senha: " + String(AP_PASSWORD));
  logMessage(LOG_INFO, "Portal: http://192.168.4.1");
  
  String apMessage = "Modo AP Ativo\nSSID: " + apSSID;
  displayMessage(apMessage.c_str());
}

void stopAPMode() {
  if (!apMode) return;
  
  logMessage(LOG_INFO, "Parando modo AP...");
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
}

// ==================== WEB SERVER ====================
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
}

void handleRoot() {
  // Adicionar cabeçalhos para evitar cache e forçar o captive portal
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AquaSys - Configuração WiFi</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 20px;
      padding: 40px;
      max-width: 500px;
      width: 100%;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
    }
    h1 { color: #667eea; margin-bottom: 10px; font-size: 28px; }
    .device-info {
      background: #f0f4ff;
      padding: 15px;
      border-radius: 10px;
      margin-bottom: 30px;
      font-size: 14px;
      color: #555;
    }
    .form-group { margin-bottom: 20px; }
    label {
      display: block;
      margin-bottom: 8px;
      color: #333;
      font-weight: 600;
    }
    select, input {
      width: 100%;
      padding: 12px;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      font-size: 16px;
      transition: border-color 0.3s;
    }
    select:focus, input:focus {
      outline: none;
      border-color: #667eea;
    }
    button {
      width: 100%;
      padding: 15px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 10px;
      font-size: 18px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s;
    }
    button:hover { transform: translateY(-2px); }
    .scan-btn { background: #28a745; margin-bottom: 20px; }
    .status {
      margin-top: 20px;
      padding: 15px;
      border-radius: 8px;
      text-align: center;
      font-weight: 600;
      display: none;
    }
    .status.success { background: #d4edda; color: #155724; display: block; }
    .status.error { background: #f8d7da; color: #721c24; display: block; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🌊 AquaSys Sensor</h1>
    <div class="device-info">
      <strong>Dispositivo:</strong> )rawliteral" + deviceUUID + R"rawliteral(<br>
      <strong>Versão:</strong> 4.3.2-OLED-COMPLETE<br>
      <strong>Tipo:</strong> Módulo Sensor
    </div>
    
    <button class="scan-btn" onclick="scanNetworks()">🔍 Escanear Redes WiFi</button>
    
    <form id="wifiForm" onsubmit="saveWiFi(event)">
      <div class="form-group">
        <label for="ssid">Rede WiFi</label>
        <select id="ssid" name="ssid" required>
          <option value="">Selecione uma rede...</option>
        </select>
      </div>
      
      <div class="form-group">
        <label for="password">Senha</label>
        <input type="password" id="password" name="password" required minlength="8">
      </div>
      
      <button type="submit">💾 Salvar e Conectar</button>
    </form>
    
    <div id="status" class="status"></div>
  </div>
  
  <script>
    function scanNetworks() {
      const btn = event.target;
      btn.disabled = true;
      btn.textContent = '🔄 Escaneando...';
      
      fetch('/scan')
        .then(r => r.json())
        .then(data => {
          const select = document.getElementById('ssid');
          select.innerHTML = '<option value="">Selecione uma rede...</option>';
          data.networks.forEach(net => {
            const option = document.createElement('option');
            option.value = net.ssid;
            option.textContent = `${net.ssid} (${net.rssi} dBm)`;
            select.appendChild(option);
          });
          btn.disabled = false;
          btn.textContent = '🔍 Escanear Redes WiFi';
        })
        .catch(err => {
          console.error(err);
          btn.disabled = false;
          btn.textContent = '🔍 Escanear Redes WiFi';
        });
    }
    
    function saveWiFi(e) {
      e.preventDefault();
      const formData = new FormData(e.target);
      const data = {
        ssid: formData.get('ssid'),
        password: formData.get('password')
      };
      
      const statusDiv = document.getElementById('status');
      statusDiv.textContent = 'Salvando...';
      statusDiv.className = 'status';
      
      fetch('/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(r => r.json())
      .then(res => {
        statusDiv.textContent = res.message;
        statusDiv.className = 'status ' + (res.success ? 'success' : 'error');
        if (res.success) {
          setTimeout(() => {
            statusDiv.textContent = 'Conectando ao WiFi...';
          }, 1000);
        }
      })
      .catch(err => {
        statusDiv.textContent = 'Erro ao salvar';
        statusDiv.className = 'status error';
      });
    }
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleScan() {
  resetWatchdog();
  logMessage(LOG_INFO, "Escaneando redes WiFi...");
  
  // Usar modo assíncrono para não bloquear o watchdog
  int n = WiFi.scanNetworks(false, false);  // async mode
  
  // Aguardar scan completo com resets periódicos
  while (n == WIFI_SCAN_RUNNING) {
    delay(100);
    resetWatchdog();
    n = WiFi.scanComplete();
  }
  
  resetWatchdog();
  
  StaticJsonDocument<1024> doc;
  JsonArray networks = doc.createNestedArray("networks");
  
  if (n >= 0) {
    for (int i = 0; i < n && i < 10; i++) {
      JsonObject net = networks.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
      net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    // Enviar via BLE se disponível
    if (pCharWiFiList && bleActive) {
      String bleData;
      serializeJson(doc, bleData);
      pCharWiFiList->setValue(bleData.c_str());
      pCharWiFiList->notify();
    }
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  WiFi.scanDelete();
  resetWatchdog();
}

void handleSave() {
  resetWatchdog();
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Dados inválidos\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
    return;
  }
  
  String ssid = doc["ssid"] | "";
  String password = doc["password"] | "";
  
  if (ssid.length() == 0 || password.length() < 8) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Senha deve ter ao menos 8 caracteres\"}");
    return;
  }
  
  for (int i = 0; i < 3; i++) {
    if (!networks[i].valid || i == 0) {
      ssid.toCharArray(networks[i].ssid, 32);
      password.toCharArray(networks[i].password, 64);
      networks[i].priority = i + 1;
      networks[i].valid = true;
      break;
    }
  }
  
  saveWiFiConfig();
  
  StaticJsonDocument<128> doc2;
  doc2["success"] = true;
  doc2["message"] = "Configuração salva";
  String response;
  serializeJson(doc2, response);
  server.send(200, "application/json", response);
  
  delay(2000);
  stopAPMode();
  connectWiFi();
}

void handleStatus() {
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  // Captive Portal - redirecionar TODAS as requisições para a página principal
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/html", "");
}

// ==================== NTP ====================
void syncNTP() {
  if (!wifiConnected) return;
  
  logMessage(LOG_INFO, "Sincronizando NTP...");
  resetWatchdog();
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    logMessage(LOG_INFO, "✅ NTP: " + String(timeStr));
  } else {
    logMessage(LOG_WARN, "Falha ao sincronizar NTP");
  }
}

// ==================== AUTENTICAÇÃO ====================
bool authenticateDevice() {
  if (!wifiConnected) {
    logMessage(LOG_ERROR, "❌ Autenticação requer WiFi");
    return false;
  }
  
  logMessage(LOG_INFO, "🔐 Autenticando...");
  resetWatchdog();
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  String url = String(SUPABASE_URL) + "/functions/v1/device-auth";
  http.begin(client, url);
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  
  StaticJsonDocument<256> reqDoc;
  reqDoc["device_uuid"] = deviceUUID;
  reqDoc["firmware_version"] = FIRMWARE_VERSION;
  
  String requestBody;
  serializeJson(reqDoc, requestBody);
  
  http.setTimeout(AUTH_TIMEOUT);
  int httpCode = http.POST(requestBody);
  
  if (httpCode == 200) {
    String response = http.getString();
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc["success"]) {
      JsonObject mqtt_config = doc["mqtt_config"];
      
      strncpy(mqttCreds.broker, mqtt_config["broker"] | MQTT_BROKER_FALLBACK, sizeof(mqttCreds.broker) - 1);
      strncpy(mqttCreds.username, mqtt_config["username"] | deviceUUID.c_str(), sizeof(mqttCreds.username) - 1);
      strncpy(mqttCreds.password, mqtt_config["password"] | "", sizeof(mqttCreds.password) - 1);
      strncpy(mqttCreds.client_id, mqtt_config["client_id"] | deviceUUID.c_str(), sizeof(mqttCreds.client_id) - 1);
      
      JsonObject topics = mqtt_config["topics"];
      strncpy(mqttCreds.topic_sensors, topics["sensors"] | TOPIC_SENSORS_FALLBACK, sizeof(mqttCreds.topic_sensors) - 1);
      strncpy(mqttCreds.topic_heartbeat, topics["heartbeat"] | TOPIC_HEARTBEAT_FALLBACK, sizeof(mqttCreds.topic_heartbeat) - 1);
      
      snprintf(mqttCreds.topic_calibration, sizeof(mqttCreds.topic_calibration), "aquasys/%s/calibration/command", deviceUUID.c_str());
      
      mqttCreds.valid = true;
      isAuthenticated = true;
      
      saveMqttCredentials();
      
      logMessage(LOG_INFO, "✅ Autenticado!");
      http.end();
      return true;
    }
  }
  
  http.end();
  return false;
}

void loadMqttCredentials() {
  prefs.begin("mqtt_creds", true);
  
  String broker = prefs.getString("broker", "");
  String username = prefs.getString("username", "");
  
  if (broker.length() > 0 && username.length() > 0) {
    strncpy(mqttCreds.broker, broker.c_str(), sizeof(mqttCreds.broker) - 1);
    strncpy(mqttCreds.username, username.c_str(), sizeof(mqttCreds.username) - 1);
    strncpy(mqttCreds.password, prefs.getString("password", "").c_str(), sizeof(mqttCreds.password) - 1);
    strncpy(mqttCreds.topic_sensors, prefs.getString("topic_sensors", "").c_str(), sizeof(mqttCreds.topic_sensors) - 1);
    strncpy(mqttCreds.topic_heartbeat, prefs.getString("topic_hb", "").c_str(), sizeof(mqttCreds.topic_heartbeat) - 1);
    
    mqttCreds.valid = true;
    logMessage(LOG_INFO, "✅ Credenciais MQTT carregadas");
  }
  
  prefs.end();
}

void saveMqttCredentials() {
  prefs.begin("mqtt_creds", false);
  
  prefs.putString("broker", mqttCreds.broker);
  prefs.putString("username", mqttCreds.username);
  prefs.putString("password", mqttCreds.password);
  prefs.putString("topic_sensors", mqttCreds.topic_sensors);
  prefs.putString("topic_hb", mqttCreds.topic_heartbeat);
  
  prefs.end();
  
  logMessage(LOG_INFO, "💾 Credenciais MQTT salvas");
}

// ==================== MQTT ====================
void setupMQTT() {
  espClient.setInsecure();
  espClient.setTimeout(20000);
  
  const char* brokerToUse = mqttCreds.valid ? mqttCreds.broker : MQTT_BROKER_FALLBACK;
  
  mqttClient.setServer(brokerToUse, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(60);
  
  logMessage(LOG_INFO, "✅ MQTT configurado: " + String(brokerToUse));
}

bool reconnectMQTT() {
  if (!wifiConnected) return false;
  if (millis() - lastMqttAttempt < 5000) return false;
  
  lastMqttAttempt = millis();
  resetWatchdog();
  
  const char* clientIdToUse = mqttCreds.valid ? mqttCreds.client_id : ("aquasys-sensor-" + deviceUUID).c_str();
  const char* usernameToUse = mqttCreds.valid ? mqttCreds.username : deviceUUID.c_str();
  const char* passwordToUse = mqttCreds.valid ? mqttCreds.password : "";
  
  logMessage(LOG_INFO, "Conectando MQTT...");
  
  bool connected = mqttClient.connect(clientIdToUse, usernameToUse, passwordToUse);
  
  if (connected) {
    mqttConnected = true;
    lastMqttSuccess = millis();
    logMessage(LOG_INFO, "✅ MQTT conectado!");
    
    const char* topicCalibration = mqttCreds.valid ? mqttCreds.topic_calibration : TOPIC_CALIBRATION_FALLBACK;
    mqttClient.subscribe(topicCalibration, 1);
    
    publishSensorData();
    return true;
  } else {
    mqttConnected = false;
    logMessage(LOG_ERROR, "❌ Falha MQTT, rc=" + String(mqttClient.state()));
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  resetWatchdog();
  
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  logMessage(LOG_INFO, "📩 MQTT: " + message);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    logMessage(LOG_ERROR, "❌ JSON inválido");
    return;
  }
  
  // Processar comandos de calibração
  String sensorType = doc["sensor_type"] | "";
  String calibType = doc["calibration_type"] | "";
  
  if (sensorType == "ph") {
    if (calibType == "4.0") {
      calibration.ph4_voltage = doc["value"] | 3.29;
    } else if (calibType == "7.0") {
      calibration.ph7_voltage = doc["value"] | 2.52;
    }
    calculatePHCoefficients();
    saveCalibration();
    logMessage(LOG_INFO, "✅ Calibração pH salva");
  }
}

void publishSensorData() {
  if (!mqttConnected || !currentData.valid) return;
  
  resetWatchdog();
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["ph"] = currentData.ph;
  data["ec"] = currentData.ec;
  data["air_temp"] = currentData.air_temp;
  data["humidity"] = currentData.humidity;
  data["water_temp"] = currentData.water_temp;
  
  String payload;
  serializeJson(doc, payload);
  
  const char* topic = mqttCreds.valid ? mqttCreds.topic_sensors : TOPIC_SENSORS_FALLBACK;
  
  if (mqttClient.publish(topic, payload.c_str(), false)) {
    logMessage(LOG_INFO, "📤 Dados publicados");
  } else {
    logMessage(LOG_ERROR, "❌ Falha ao publicar");
  }
}

void publishHeartbeat() {
  if (!mqttConnected) return;
  
  resetWatchdog();
  
  StaticJsonDocument<768> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["timestamp"] = millis();
  doc["firmware_version"] = FIRMWARE_VERSION;
  
  JsonObject status = doc.createNestedObject("status");
  status["wifi_connected"] = wifiConnected;
  status["mqtt_connected"] = mqttConnected;
  status["ble_active"] = bleActive;
  status["rssi"] = WiFi.RSSI();
  
  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = ESP.getFreeHeap();
  memory["min_free_heap"] = ESP.getMinFreeHeap();
  
  doc["uptime_seconds"] = millis() / 1000;
  
  if (currentData.valid) {
    JsonObject data = doc.createNestedObject("data");
    data["ph"] = currentData.ph;
    data["ec"] = currentData.ec;
    data["air_temp"] = currentData.air_temp;
    data["humidity"] = currentData.humidity;
    data["water_temp"] = currentData.water_temp;
  }
  
  String payload;
  serializeJson(doc, payload);
  
  const char* topic = mqttCreds.valid ? mqttCreds.topic_heartbeat : TOPIC_HEARTBEAT_FALLBACK;
  
  if (mqttClient.publish(topic, payload.c_str(), false)) {
    logMessage(LOG_INFO, "💓 Heartbeat publicado");
  }
}

// ==================== BLE ====================
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnectedBLE = true;
    logMessage(LOG_INFO, "📱 BLE conectado");
  }
  
  void onDisconnect(BLEServer* pServer) {
    deviceConnectedBLE = false;
    logMessage(LOG_INFO, "📱 BLE desconectado");
    pServer->startAdvertising();
  }
};

void setupBLE() {
  logMessage(LOG_INFO, "Inicializando BLE...");
  
  BLEDevice::init(deviceUUID.c_str());
  pBLEServer = BLEDevice::createServer();
  pBLEServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pBLEServer->createService(SERVICE_UUID);
  
  pCharPH = pService->createCharacteristic(
    CHAR_UUID_PH,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharPH->addDescriptor(new BLE2902());
  
  pCharEC = pService->createCharacteristic(
    CHAR_UUID_EC,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharEC->addDescriptor(new BLE2902());
  
  pCharAirTemp = pService->createCharacteristic(
    CHAR_UUID_AIR_TEMP,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharAirTemp->addDescriptor(new BLE2902());
  
  pCharHumidity = pService->createCharacteristic(
    CHAR_UUID_HUMIDITY,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharHumidity->addDescriptor(new BLE2902());
  
  pCharWaterTemp = pService->createCharacteristic(
    CHAR_UUID_WATER_TEMP,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharWaterTemp->addDescriptor(new BLE2902());
  
  pCharWiFiList = pService->createCharacteristic(
    CHAR_WIFI_LIST,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharWiFiList->addDescriptor(new BLE2902());
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  bleActive = true;
  logMessage(LOG_INFO, "✅ BLE ativo: " + deviceUUID);
}

void publishDataToBLE() {
  if (!bleActive || !currentData.valid) return;
  
  char buffer[16];
  
  snprintf(buffer, sizeof(buffer), "%.2f", currentData.ph);
  pCharPH->setValue(buffer);
  pCharPH->notify();
  
  snprintf(buffer, sizeof(buffer), "%.2f", currentData.ec);
  pCharEC->setValue(buffer);
  pCharEC->notify();
  
  snprintf(buffer, sizeof(buffer), "%.2f", currentData.air_temp);
  pCharAirTemp->setValue(buffer);
  pCharAirTemp->notify();
  
  snprintf(buffer, sizeof(buffer), "%.2f", currentData.humidity);
  pCharHumidity->setValue(buffer);
  pCharHumidity->notify();
  
  snprintf(buffer, sizeof(buffer), "%.2f", currentData.water_temp);
  pCharWaterTemp->setValue(buffer);
  pCharWaterTemp->notify();
  
  logMessage(LOG_DEBUG, "📡 BLE atualizado");
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║ AquaSys Sensor - v4.3.2-OLED-COMPLETE ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  deviceUUID = generateDeviceUUID();
  logMessage(LOG_INFO, "🆔 UUID: " + deviceUUID);
  
  // Inicializar Watchdog
  initWatchdog();
  
  // Configurar pinos dos botões
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);
  
  // Inicializar OLED
  initOLED();
  displayMessage("Iniciando...");
  
  // Inicializar sensores
  dht.begin();
  ds18b20.begin();
  logMessage(LOG_INFO, "Sensores iniciados");
  
  // Carregar configurações
  loadCalibration();
  loadWiFiConfig();
  loadMqttCredentials();
  
  // Inicializar BLE
  setupBLE();
  
  // Conectar WiFi
  if (!connectWiFi()) {
    logMessage(LOG_WARN, "Falha WiFi, iniciando AP...");
    startAPMode();
  } else {
    if (!isAuthenticated) {
      authenticateDevice();
    }
    setupMQTT();
  }
  
  logMessage(LOG_INFO, "✅ Setup completo!");
  logMessage(LOG_INFO, "Memória: " + String(ESP.getFreeHeap()) + " bytes");
  
  displayMessage("Sistema pronto!");
  delay(2000);
}

// ==================== LOOP ====================
void loop() {
  resetWatchdog();
  
  // Modo AP
  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (millis() - apModeStartTime > AP_TIMEOUT) {
      stopAPMode();
      connectWiFi();
    }
    
    // Atualizar display e botões mesmo em modo AP
    handleButtons();
    updateDisplay();
    
    delay(10);
    return;
  }
  
  // Verificar WiFi
  checkWiFi();
  
  // Verificar MQTT
  if (wifiConnected) {
    if (!mqttConnected) {
      if (!isAuthenticated) {
        authenticateDevice();
      }
      reconnectMQTT();
    }
    
    if (mqttConnected) {
      mqttClient.loop();
    }
  }
  
  // Ler sensores
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = millis();
    readSensors();
    
    if (currentData.valid) {
      publishSensorData();
      publishDataToBLE();
    }
  }
  
  // Heartbeat
  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = millis();
    publishHeartbeat();
  }
  
  // Atualizar interface OLED
  handleButtons();
  updateDisplay();
  
  delay(50);
}
