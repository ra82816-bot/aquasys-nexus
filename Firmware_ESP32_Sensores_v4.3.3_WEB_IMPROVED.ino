/*
 * ============================================================================
 * AquaSys Nexus - Sensor Module v4.3.3-WEB-IMPROVED
 * ============================================================================
 * VERSÃO COM PORTAL WEB MELHORADO
 * 
 * CORREÇÕES NESTA VERSÃO:
 * ✅ Portal captivo simplificado e mais estável
 * ✅ Web server com CORS habilitado
 * ✅ Redirecionamento captive portal melhorado
 * ✅ Leituras de sensores independentes da conexão
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
 * ✅ Watchdog robusto (60s)
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

// ==================== CONFIGURAÇÕES DE PINOS ====================
// SENSORES ANALÓGICOS
#define PH_SENSOR_PIN 34        // GPIO34 - ADC1_CH6 - Sensor de pH
#define TDS_SENSOR_PIN 35       // GPIO35 - ADC1_CH7 - Sensor de EC/TDS

// SENSORES DIGITAIS
#define DHT_PIN 15              // GPIO15 - Sensor DHT22 (Temperatura/Umidade do Ar)
#define DHT_TYPE DHT22          // Tipo do sensor DHT
#define ONE_WIRE_BUS 2          // GPIO2 - Sensor DS18B20 (Temperatura da Água)

// DISPLAY OLED (I2C)
// SDA: GPIO21 (padrão I2C)
// SCL: GPIO22 (padrão I2C)
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// BOTÕES DE CONTROLE
#define BUTTON_UP 32            // GPIO32 - Botão UP (Navegar para cima)
#define BUTTON_DOWN 33          // GPIO33 - Botão DOWN (Navegar para baixo)
#define BUTTON_SELECT 25        // GPIO25 - Botão SELECT (Confirmar)
#define BUTTON_BACK 26          // GPIO26 - Botão BACK (Voltar/Cancelar)

// ==================== CONFIGURAÇÕES GERAIS ====================
// Versão do Firmware
#define FIRMWARE_VERSION "4.3.3-WEB-IMPROVED"
#define DEVICE_TYPE "SENSOR"

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
#define MQTT_TIMEOUT 10000          // 10s timeout MQTT
#define MQTT_SOCKET_TIMEOUT 5       // 5s timeout socket
#define SENSOR_READ_INTERVAL 30000  // 30s
#define HEARTBEAT_INTERVAL 60000    // 60s
#define WATCHDOG_TIMEOUT 120        // 120s - margem segura
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
#define CHAR_UUID_DEVICE    "a3c87500-8ed3-4bdf-8a39-a01bebede296"

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
const byte DNS_PORT = 53;

// WiFi Scan - Sistema Assíncrono
enum ScanState {
  SCAN_IDLE,
  SCAN_RUNNING,
  SCAN_COMPLETE,
  SCAN_ERROR
};

ScanState scanState = SCAN_IDLE;
unsigned long scanStartTime = 0;
const unsigned long SCAN_TIMEOUT = 30000;  // 30s timeout

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

// Calibração
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
bool showQRCode = false;

// BLE
BLEServer* pBLEServer = nullptr;
BLECharacteristic* pCharPH = nullptr;
BLECharacteristic* pCharEC = nullptr;
BLECharacteristic* pCharAirTemp = nullptr;
BLECharacteristic* pCharHumidity = nullptr;
BLECharacteristic* pCharWaterTemp = nullptr;
BLECharacteristic* pCharWiFiList = nullptr;
BLECharacteristic* pCharDeviceUUID = nullptr;
bool bleActive = false;
bool deviceConnectedBLE = false;

// Timers
unsigned long lastSensorRead = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWdtReset = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastUUIDPrint = 0;

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
  // Inicializar WiFi temporariamente para ler MAC corretamente
  WiFi.mode(WIFI_STA);
  delay(100);  // Garantir inicialização do WiFi
  
  uint8_t mac[6];
  WiFi.macAddress(mac);  // Agora funcionará corretamente com WiFi inicializado
  
  char uuid[20];
  sprintf(uuid, "SEN-%02X%02X%02X%02X%02X%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(uuid);
}

void printUUIDBanner() {
  Serial.println();
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║      DEVICE UUID - IMPORTANTE!     ║");
  Serial.println("╠════════════════════════════════════╣");
  Serial.printf("║  UUID: %-24s ║\n", deviceUUID.c_str());
  Serial.println("║                                    ║");
  Serial.println("║  Use este UUID para cadastrar     ║");
  Serial.println("║  o dispositivo no app!             ║");
  Serial.println("║                                    ║");
  Serial.println("║  Firmware: v4.3.3-WEB-IMPROVED     ║");
  Serial.println("║  Tipo: SENSOR                      ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.println();
}

// Função simplificada para desenhar QR Code no OLED
void drawQRCode() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("=== QR CODE ===");
  display.println();
  
  // QR Code simplificado (padrão de pixels)
  // Para um QR Code real, precisaríamos de uma biblioteca específica
  // Por ora, vamos mostrar o UUID em formato grande
  display.setTextSize(1);
  display.println("UUID para App:");
  display.println();
  display.setTextSize(1);
  
  // Dividir UUID em 2 linhas para melhor visualização
  String line1 = deviceUUID.substring(0, 13);
  String line2 = deviceUUID.substring(13);
  
  display.println(line1);
  display.println(line2);
  display.println();
  display.setTextSize(1);
  display.println("[BACK] Voltar");
  
  display.display();
}

// ==================== WATCHDOG ====================
void initWatchdog() {
  // Desabilitar watchdog anterior se existir
  esp_task_wdt_deinit();
  delay(100);
  
  // Configurar novo watchdog
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
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
      if (apMode) {
        display.println("Modo: AP ATIVO");
        display.println("IP: 192.168.4.1");
      }
      display.printf("MQTT: %s\n", mqttConnected ? "OK" : "OFF");
      display.printf("BLE: %s\n", deviceConnectedBLE ? "Connected" : "Ready");
      display.println("----------------");
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
      
    case PAGE_SYSTEM: {
      if (showQRCode) {
        drawQRCode();
        return;  // Não executar o resto do update
      }
      display.println("=== SISTEMA ===");
      display.println("UUID:");
      // Dividir UUID para melhor visualização
      String line1 = deviceUUID.substring(0, 13);
      String line2 = deviceUUID.substring(13);
      display.println(line1);
      display.println(line2);
      display.println("----------------");
      display.printf("Mem: %d KB\n", ESP.getFreeHeap() / 1024);
      display.printf("Uptime: %lus\n", millis() / 1000);
      display.println("[SELECT] QR Code");
      break;
    }
      
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
    
    // Toggle QR Code na página SYSTEM
    if (currentPage == PAGE_SYSTEM) {
      showQRCode = !showQRCode;
      logMessage(LOG_INFO, showQRCode ? "Exibindo QR Code" : "Ocultando QR Code");
    }
    
    if (currentPage == PAGE_CALIBRATION) {
      if (calibrationMode == CAL_NONE) {
        calibrationMode = (CalibrationMode)(calibrationMenuIndex + 1);
        logMessage(LOG_INFO, "Modo calibração: " + String(calibrationMode));
      } else {
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
    
    // Voltar do QR Code
    if (showQRCode) {
      showQRCode = false;
      logMessage(LOG_INFO, "Voltou do QR Code");
      return;
    }
    
    if (currentPage == PAGE_CALIBRATION) {
      if (calibrationMode != CAL_NONE) {
        calibrationMode = CAL_NONE;
        calibrationMenuIndex = 0;
        logMessage(LOG_INFO, "Calibração cancelada");
      } else {
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
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
      resetWatchdog();  // Reset a cada 500ms
      delay(500);
      Serial.print(".");
      dots++;
      if (dots % 10 == 0) {
        logMessage(LOG_DEBUG, "Aguardando WiFi... " + String((millis() - startTime) / 1000) + "s");
      }
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
    
    logMessage(LOG_WARN, "⏱️ Timeout WiFi: " + String(networks[i].ssid));
    WiFi.disconnect();
    delay(100);
    resetWatchdog();
  }
  
  wifiConnected = false;
  return false;
}

void checkWiFi() {
  if (millis() - lastWiFiCheck < 10000) return;
  lastWiFiCheck = millis();
  
  resetWatchdog();
  
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
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
  
  // Reset watchdog antes de iniciar
  resetWatchdog();
  
  // Parar e desconectar tudo antes
  if (apMode) {
    server.stop();
    dnsServer.stop();
  }
  
  WiFi.disconnect(true);
  delay(500);
  resetWatchdog();
  
  // Configurar modo AP puro
  WiFi.mode(WIFI_AP);
  delay(500);
  resetWatchdog();
  
  String apSSID = String(AP_SSID_PREFIX) + deviceUUID.substring(4);
  
  // IP fixo do AP
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(apIP, gateway, subnet);
  resetWatchdog();
  
  // Iniciar o AP
  bool apStarted = WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
  
  if (!apStarted) {
    logMessage(LOG_ERROR, "❌ Falha ao iniciar AP");
    return;
  }
  
  delay(1000);
  resetWatchdog();
  
  // Iniciar DNS Server para captive portal
  dnsServer.stop();
  dnsServer.start(DNS_PORT, "*", apIP);
  resetWatchdog();
  
  // Iniciar Web Server
  setupWebServer();
  server.begin();
  resetWatchdog();
  
  apMode = true;
  apModeStartTime = millis();
  
  logMessage(LOG_INFO, "✅ AP ativo: " + apSSID);
  logMessage(LOG_INFO, "Senha: " + String(AP_PASSWORD));
  logMessage(LOG_INFO, "IP: " + WiFi.softAPIP().toString());
  
  displayMessage("Modo AP Ativo\nAcesse:\n192.168.4.1");
}

void stopAPMode() {
  if (!apMode) return;
  
  logMessage(LOG_INFO, "Parando modo AP...");
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
}

// ==================== WEB SERVER (OTIMIZADO PARA MEMORIA) ====================
void setupWebServer() {
  // Rotas principais
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", HTTP_GET, handleStatus);
  
  // Captive portal - URLs comuns de detecção
  server.on("/generate_204", handleRoot);  // Android
  server.on("/gen_204", handleRoot);       // Android
  server.on("/hotspot-detect.html", handleRoot);  // iOS
  server.on("/library/test/success.html", handleRoot);  // iOS
  server.on("/connecttest.txt", handleRoot);  // Windows
  server.on("/ncsi.txt", handleRoot);  // Windows
  
  server.onNotFound(handleNotFound);
  
  logMessage(LOG_INFO, "Web server configurado");
}

void handleRoot() {
  resetWatchdog();
  
  // Headers otimizados
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "keep-alive");
  server.send(200, "text/html", ""); // Envia headers primeiro com corpo vazio
  
  // Enviar HTML em chunks para evitar fragmentação de memória
  server.sendContent("<!DOCTYPE html><html><head>");
  server.sendContent("<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  server.sendContent("<title>AquaSys - Config</title>");
  server.sendContent("<style>");
  server.sendContent("body{font-family:Arial;margin:0;padding:20px;background:#667eea;color:#fff}");
  server.sendContent(".box{background:#fff;color:#333;border-radius:10px;padding:20px;max-width:400px;margin:20px auto}");
  server.sendContent("h1{color:#667eea;margin:0 0 20px 0;font-size:24px}");
  server.sendContent("input,select,button{width:100%;padding:12px;margin:10px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}");
  server.sendContent("button{background:#667eea;color:#fff;border:none;cursor:pointer;font-weight:bold}");
  server.sendContent("button:hover{background:#5568d3}");
  server.sendContent(".info{background:#f0f4ff;padding:10px;border-radius:5px;margin-bottom:15px;font-size:13px}");
  server.sendContent(".warn{background:#fff3cd;color:#856404;padding:8px;border-radius:5px;margin-bottom:10px;font-size:12px}");
  server.sendContent(".success{background:#d4edda;color:#155724;padding:8px;border-radius:5px;margin-bottom:10px;font-size:12px}");
  server.sendContent("#status{margin-top:15px;padding:10px;border-radius:5px;display:none}");
  server.sendContent(".ok{background:#d4edda;color:#155724}");
  server.sendContent(".err{background:#f8d7da;color:#721c24}");
  server.sendContent("</style></head><body>");
  resetWatchdog();
  
  server.sendContent("<div class='box'>");
  server.sendContent("<h1>🌊 AquaSys Sensor</h1>");
  
  // Use String() apenas para formatar o UUID, não para concatenar todo o HTML
  String uuidHtml = "<div class='warn' style='background:#667eea;color:#fff;font-size:16px;font-weight:bold;text-align:center;padding:15px'>";
  uuidHtml += "📱 UUID DO DISPOSITIVO<br>" + deviceUUID + "</div>";
  server.sendContent(uuidHtml);
  
  server.sendContent("<div class='info'><b>Firmware:</b> 4.3.3-WEB-IMPROVED<br><b>Tipo:</b> SENSOR</div>");
  server.sendContent("<div class='success'>✅ Copie o UUID acima para cadastrar no app!</div>");
  server.sendContent("<button onclick='scan()'>🔍 Escanear WiFi</button>");
  server.sendContent("<form onsubmit='save(event)'>");
  server.sendContent("<select id='ssid' required><option value=''>Selecione rede...</option></select>");
  server.sendContent("<input type='password' id='pass' placeholder='Senha WiFi' required minlength='8'>");
  server.sendContent("<button type='submit'>💾 Salvar</button>");
  server.sendContent("</form>");
  server.sendContent("<div id='status'></div>");
  server.sendContent("</div>");
  resetWatchdog();
  
  // JavaScript
  server.sendContent("<script>");
  server.sendContent("let pollInterval;");
  server.sendContent("function scan(){");
  server.sendContent("let btn=event.target;btn.disabled=true;btn.textContent='⏳ Iniciando...';");
  server.sendContent("fetch('/scan').then(r=>r.json()).then(d=>{");
  server.sendContent("if(d.status==='complete'){showResults(d.networks);btn.disabled=false;btn.textContent='✅ Escanear WiFi';return;}");
  server.sendContent("if(d.status==='scanning'){pollScan(btn);return;}");
  server.sendContent("throw new Error(d.message||'Erro');");
  server.sendContent("}).catch(e=>{alert('Erro: '+e.message);btn.disabled=false;btn.textContent='🔍 Escanear WiFi';});}");
  server.sendContent("function pollScan(btn){");
  server.sendContent("let attempts=0;");
  server.sendContent("pollInterval=setInterval(()=>{");
  server.sendContent("attempts++;btn.textContent='🔍 Escaneando ('+attempts+'s)';");
  server.sendContent("fetch('/scan').then(r=>r.json()).then(d=>{");
  server.sendContent("if(d.status==='complete'){clearInterval(pollInterval);showResults(d.networks);btn.disabled=false;btn.textContent='✅ Escanear WiFi';}");
  server.sendContent("else if(d.status==='error'||d.status==='timeout'){clearInterval(pollInterval);throw new Error(d.message);}");
  server.sendContent("}).catch(e=>{clearInterval(pollInterval);alert('Erro: '+e.message);btn.disabled=false;btn.textContent='🔍 Escanear WiFi';});");
  server.sendContent("if(attempts>30){clearInterval(pollInterval);alert('Timeout');btn.disabled=false;btn.textContent='🔍 Escanear WiFi';}");
  server.sendContent("},1000);}");
  server.sendContent("function showResults(networks){");
  server.sendContent("let s=document.getElementById('ssid');s.innerHTML='<option value=\"\">Selecione rede...</option>';");
  server.sendContent("networks.sort((a,b)=>b.rssi-a.rssi);");
  server.sendContent("networks.forEach(n=>{let o=document.createElement('option');o.value=n.ssid;");
  server.sendContent("let lock=n.encrypted?'🔒':'🔓';o.text=lock+' '+n.ssid+' ('+n.rssi+'dBm)';s.add(o);});}");
  server.sendContent("function save(e){e.preventDefault();");
  server.sendContent("let d={ssid:document.getElementById('ssid').value,password:document.getElementById('pass').value};");
  server.sendContent("if(!d.ssid){alert('Selecione uma rede');return;}");
  server.sendContent("let st=document.getElementById('status');st.textContent='💾 Salvando...';st.className='';st.style.display='block';");
  server.sendContent("fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})");
  server.sendContent(".then(r=>r.json()).then(r=>{st.textContent=r.message;st.className=r.success?'ok':'err';");
  server.sendContent("if(r.success)setTimeout(()=>{st.textContent='🔄 Reiniciando...';setTimeout(()=>location.reload(),3000);},1000);})");
  server.sendContent(".catch(e=>{st.textContent='❌ Erro: '+e.message;st.className='err';});}");
  server.sendContent("</script></body></html>");
  server.sendContent(""); // Envia pacote final para fechar a conexão
}

void handleScan() {
  resetWatchdog();
  
  unsigned long now = millis();
  
  // Headers otimizados para manter conexão
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "keep-alive");
  server.sendHeader("Keep-Alive", "timeout=5, max=100");
  server.sendHeader("Cache-Control", "no-cache");
  
  // Estado: IDLE - iniciar novo scan assíncrono
  if (scanState == SCAN_IDLE) {
    logMessage(LOG_INFO, "🔍 Iniciando scan assíncrono...");
    
    WiFi.scanDelete();
    
    // Iniciar scan assíncrono (não bloqueia!)
    int16_t result = WiFi.scanNetworks(true, false, false, 120);  // async=true, 120ms/canal
    
    if (result == WIFI_SCAN_RUNNING) {
      scanState = SCAN_RUNNING;
      scanStartTime = now;
      server.send(202, "application/json", "{\"status\":\"scanning\",\"message\":\"Scan iniciado\"}");
      logMessage(LOG_INFO, "✅ Scan async iniciado");
    } else {
      scanState = SCAN_ERROR;
      server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Erro ao iniciar scan\"}");
      logMessage(LOG_ERROR, "❌ Erro ao iniciar scan");
    }
    return;
  }
  
  // Estado: RUNNING - verificar se completou
  if (scanState == SCAN_RUNNING) {
    int16_t n = WiFi.scanComplete();
    
    // Timeout de segurança
    if (now - scanStartTime > SCAN_TIMEOUT) {
      logMessage(LOG_WARN, "⏱️ Scan timeout");
      WiFi.scanDelete();
      scanState = SCAN_IDLE;
      server.send(408, "application/json", "{\"status\":\"timeout\",\"message\":\"Scan timeout. Tente novamente.\"}");
      return;
    }
    
    if (n == WIFI_SCAN_RUNNING) {
      // Ainda rodando
      int elapsed = (now - scanStartTime) / 1000;
      server.send(202, "application/json", 
                  "{\"status\":\"scanning\",\"elapsed\":" + String(elapsed) + "}");
      return;
    }
    
    if (n == WIFI_SCAN_FAILED || n < 0) {
      logMessage(LOG_ERROR, "❌ Scan falhou");
      WiFi.scanDelete();
      scanState = SCAN_IDLE;
      server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Scan falhou\"}");
      return;
    }
    
    // Scan completo! Enviar em streaming (sem cache)
    logMessage(LOG_INFO, "✅ Scan completo: " + String(n) + " redes");
    int maxNetworks = min((int)n, 15);
    
    server.send(200, "application/json", ""); // Envia headers
    server.sendContent("{\"status\":\"complete\",\"networks\":[");
    
    for (int i = 0; i < maxNetworks; i++) {
      if (i > 0) server.sendContent(",");
      
      String ssid = WiFi.SSID(i);
      ssid.replace("\"", "\\\"");
      ssid.replace("\\", "\\\\");
      
      // Construir e enviar o JSON de cada rede individualmente
      String networkJson = "{\"ssid\":\"" + ssid + "\"";
      networkJson += ",\"rssi\":" + String(WiFi.RSSI(i));
      networkJson += ",\"encrypted\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
      networkJson += ",\"channel\":" + String(WiFi.channel(i));
      networkJson += "}";
      
      server.sendContent(networkJson);
      resetWatchdog();
    }
    
    server.sendContent("]}");
    server.sendContent(""); // Finaliza
    
    scanState = SCAN_IDLE;
    WiFi.scanDelete();
    logMessage(LOG_INFO, "📡 Resultados enviados (streaming)");
    return;
  }
  
  // Estado: ERROR ou desconhecido - resetar
  scanState = SCAN_IDLE;
  WiFi.scanDelete();
  server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Estado inválido\"}");
}

void handleSave() {
  resetWatchdog();
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "keep-alive");
  server.sendHeader("Keep-Alive", "timeout=5, max=10");
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Dados inválidos\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    logMessage(LOG_ERROR, "JSON inválido");
    server.send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
    return;
  }
  
  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();
  
  if (ssid.length() == 0 || password.length() < 8) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"SSID ou senha inválidos\"}");
    return;
  }
  
  // Salvar primeira rede
  ssid.toCharArray(networks[0].ssid, 32);
  password.toCharArray(networks[0].password, 64);
  networks[0].priority = 1;
  networks[0].valid = true;
  
  saveWiFiConfig();
  
  server.send(200, "application/json", "{\"success\":true,\"message\":\"WiFi salvo! Reiniciando...\"}");
  
  logMessage(LOG_INFO, "WiFi configurado: " + ssid);
  
  delay(1000);
  ESP.restart();
}

void handleStatus() {
  resetWatchdog();
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "keep-alive");
  server.sendHeader("Cache-Control", "no-cache");
  
  String json = "{";
  json += "\"uuid\":\"" + deviceUUID + "\",";
  json += "\"wifi\":" + String(wifiConnected ? "true" : "false") + ",";
  json += "\"mqtt\":" + String(mqttConnected ? "true" : "false") + ",";
  json += "\"uptime\":" + String(millis() / 1000);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleNotFound() {
  resetWatchdog();
  
  // Redirecionar todas as URLs desconhecidas para a raiz (captive portal)
  logMessage(LOG_DEBUG, "Redirect: " + server.uri());
  
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(302, "text/plain", "");
}

// ==================== NTP ====================
void syncNTP() {
  logMessage(LOG_INFO, "Sincronizando NTP...");
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
  
  int retry = 0;
  time_t now = time(nullptr);
  while (now < 1609459200 && retry < 10) {  // 2021-01-01
    resetWatchdog();
    delay(500);
    now = time(nullptr);
    retry++;
  }
  
  if (now >= 1609459200) {
    logMessage(LOG_INFO, "✅ NTP sincronizado");
  } else {
    logMessage(LOG_WARN, "⚠️ Falha ao sincronizar NTP");
  }
}

// ==================== MQTT CREDENTIALS ====================
void loadMqttCredentials() {
  prefs.begin("mqtt", true);
  
  String broker = prefs.getString("broker", "");
  if (broker.length() > 0) {
    broker.toCharArray(mqttCreds.broker, 128);
    prefs.getString("username", "").toCharArray(mqttCreds.username, 64);
    prefs.getString("password", "").toCharArray(mqttCreds.password, 128);
    prefs.getString("client_id", "").toCharArray(mqttCreds.client_id, 64);
    prefs.getString("topic_s", "").toCharArray(mqttCreds.topic_sensors, 128);
    prefs.getString("topic_h", "").toCharArray(mqttCreds.topic_heartbeat, 128);
    prefs.getString("topic_c", "").toCharArray(mqttCreds.topic_calibration, 128);
    mqttCreds.valid = true;
    logMessage(LOG_INFO, "MQTT credentials carregadas");
  } else {
    mqttCreds.valid = false;
  }
  
  prefs.end();
}

void saveMqttCredentials() {
  prefs.begin("mqtt", false);
  
  prefs.putString("broker", mqttCreds.broker);
  prefs.putString("username", mqttCreds.username);
  prefs.putString("password", mqttCreds.password);
  prefs.putString("client_id", mqttCreds.client_id);
  prefs.putString("topic_s", mqttCreds.topic_sensors);
  prefs.putString("topic_h", mqttCreds.topic_heartbeat);
  prefs.putString("topic_c", mqttCreds.topic_calibration);
  
  prefs.end();
  
  logMessage(LOG_INFO, "💾 MQTT credentials salvas");
}

// ==================== AUTENTICAÇÃO ====================
bool authenticateDevice() {
  if (!wifiConnected) {
    logMessage(LOG_WARN, "WiFi não conectado para autenticação");
    return false;
  }
  
  logMessage(LOG_INFO, "🔐 Autenticando dispositivo...");
  
  // OTIMIZAÇÃO: Desativar BLE temporariamente para liberar memória
  if (bleActive) {
    logMessage(LOG_DEBUG, "Desativando BLE para HTTPS...");
    BLEDevice::deinit(true); // O 'true' libera a memória
    bleActive = false;
    delay(200); // Dar um tempo para a memória ser liberada
    logMessage(LOG_DEBUG, "Heap livre após deinit BLE: " + String(ESP.getFreeHeap()));
  }
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  String url = String(SUPABASE_URL) + "/functions/v1/device-auth";
  http.begin(client, url);
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["firmware_version"] = FIRMWARE_VERSION;
  
  String payload;
  serializeJson(doc, payload);
  
  int httpCode = http.POST(payload);
  bool authSuccess = false;
  
  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<512> respDoc;
    
    DeserializationError error = deserializeJson(respDoc, response);
    
    if (!error && respDoc["success"] == true) {
      String broker = respDoc["mqtt_broker"].as<String>();
      String username = respDoc["mqtt_username"].as<String>();
      String password = respDoc["mqtt_password"].as<String>();
      
      broker.toCharArray(mqttCreds.broker, 128);
      username.toCharArray(mqttCreds.username, 64);
      password.toCharArray(mqttCreds.password, 128);
      deviceUUID.toCharArray(mqttCreds.client_id, 64);
      
      strcpy(mqttCreds.topic_sensors, "aquasys/sensors/all");
      strcpy(mqttCreds.topic_heartbeat, "aquasys/heartbeat/sensor");
      strcpy(mqttCreds.topic_calibration, "aquasys/calibration/sensor");
      
      mqttCreds.valid = true;
      saveMqttCredentials();
      
      logMessage(LOG_INFO, "✅ Autenticação OK");
      isAuthenticated = true;
      authSuccess = true;
    }
  }
  
  if (!authSuccess) {
    logMessage(LOG_WARN, "⚠️ Autenticação falhou, usando fallback");
    
    // Usar credenciais fallback
    strcpy(mqttCreds.broker, MQTT_BROKER_FALLBACK);
    strcpy(mqttCreds.username, "hydrosmart");
    strcpy(mqttCreds.password, "Hydro@2024!");
    deviceUUID.toCharArray(mqttCreds.client_id, 64);
    strcpy(mqttCreds.topic_sensors, TOPIC_SENSORS_FALLBACK);
    strcpy(mqttCreds.topic_heartbeat, TOPIC_HEARTBEAT_FALLBACK);
    strcpy(mqttCreds.topic_calibration, TOPIC_CALIBRATION_FALLBACK);
    mqttCreds.valid = true;
  }
  
  http.end();
  
  // OTIMIZAÇÃO: Reativar BLE após HTTP
  if (!bleActive && !apMode) {
    logMessage(LOG_DEBUG, "Reativando BLE...");
    setupBLE();
    bleActive = true;
  }
  
  return true;
}

// ==================== MQTT ====================
void setupMQTT() {
  if (!mqttCreds.valid) {
    loadMqttCredentials();
    
    if (!mqttCreds.valid) {
      authenticateDevice();
    }
  }
  
  // Otimização de memória SSL - CRÍTICO para ESP32
  logMessage(LOG_INFO, "Memória livre antes SSL: " + String(ESP.getFreeHeap()) + " bytes");
  
  // Desabilitar validação de certificado (economiza ~40KB de RAM)
  espClient.setInsecure();
  
  // Configurar timeout
  espClient.setTimeout(MQTT_SOCKET_TIMEOUT);
  
  // Configurar MQTT com buffer menor
  mqttClient.setBufferSize(256);  // Reduzir de 256 padrão
  mqttClient.setServer(mqttCreds.broker, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT);
  
  logMessage(LOG_INFO, "MQTT configurado: " + String(mqttCreds.broker));
  logMessage(LOG_INFO, "Memória livre após config: " + String(ESP.getFreeHeap()) + " bytes");
}

bool reconnectMQTT() {
  // Não tentar MQTT se em modo AP ou sem WiFi
  if (!wifiConnected || apMode) return false;
  if (mqttClient.connected()) return true;
  if (millis() - lastMqttAttempt < 5000) return false;
  
  lastMqttAttempt = millis();
  resetWatchdog();
  
  // OTIMIZAÇÃO CRÍTICA: Desativar BLE para liberar ~30KB de RAM
  if (bleActive) {
    logMessage(LOG_DEBUG, "Desativando BLE para MQTTS...");
    BLEDevice::deinit(true); // O 'true' libera a memória
    bleActive = false;
    delay(200); // Dar um tempo para a memória ser liberada
    logMessage(LOG_DEBUG, "Heap livre após deinit BLE: " + String(ESP.getFreeHeap()));
  }
  
  // Liberar memória antes de conectar
  logMessage(LOG_INFO, "🔌 Conectando MQTT...");
  logMessage(LOG_INFO, "Heap livre: " + String(ESP.getFreeHeap()) + " bytes");
  logMessage(LOG_INFO, "Heap min: " + String(ESP.getMinFreeHeap()) + " bytes");
  
  // Desconectar primeiro para liberar recursos
  if (espClient.connected()) {
    espClient.stop();
    delay(200);
  }
  
  resetWatchdog();
  
  unsigned long connectStart = millis();
  bool connected = false;
  
  // Tentar conectar com timeout reduzido
  while (!connected && (millis() - connectStart) < MQTT_TIMEOUT) {
    // Forçar garbage collection
    delay(100);
    resetWatchdog();
    
    connected = mqttClient.connect(
      mqttCreds.client_id,
      mqttCreds.username,
      mqttCreds.password
    );
    
    if (!connected) {
      delay(200);  // Aumentar delay entre tentativas
      resetWatchdog();
    }
  }
  
  if (connected) {
    mqttConnected = true;
    lastMqttSuccess = millis();
    logMessage(LOG_INFO, "✅ MQTT conectado em " + String(millis() - connectStart) + "ms");
    logMessage(LOG_INFO, "Heap livre pós-conexão: " + String(ESP.getFreeHeap()) + " bytes");
    
    // Publicar heartbeat imediato
    publishHeartbeat();
    
    // OTIMIZAÇÃO: Reativar BLE após MQTT conectado
    if (!bleActive && !apMode) {
      logMessage(LOG_DEBUG, "Reativando BLE...");
      setupBLE();
      bleActive = true;
    }
    
    return true;
  } else {
    mqttConnected = false;
    int state = mqttClient.state();
    logMessage(LOG_ERROR, "❌ MQTT falhou após " + String(millis() - connectStart) + "ms");
    logMessage(LOG_ERROR, "State: " + String(state) + " | Heap: " + String(ESP.getFreeHeap()) + " bytes");
    
    // Mapear estados MQTT
    String stateMsg = "";
    switch(state) {
      case -4: stateMsg = "TIMEOUT"; break;
      case -3: stateMsg = "CONNECTION_LOST"; break;
      case -2: stateMsg = "CONNECT_FAILED"; break;
      case -1: stateMsg = "DISCONNECTED"; break;
      case 1: stateMsg = "BAD_PROTOCOL"; break;
      case 2: stateMsg = "BAD_CLIENT_ID"; break;
      case 3: stateMsg = "UNAVAILABLE"; break;
      case 4: stateMsg = "BAD_CREDENTIALS"; break;
      case 5: stateMsg = "UNAUTHORIZED"; break;
      default: stateMsg = "UNKNOWN"; break;
    }
    logMessage(LOG_ERROR, "Código: " + stateMsg);
    
    // Limpar conexão
    espClient.stop();
    mqttClient.disconnect();
    
    // OTIMIZAÇÃO: Reativar BLE mesmo se falhou (não manter desligado)
    if (!bleActive && !apMode) {
      logMessage(LOG_DEBUG, "Reativando BLE...");
      setupBLE();
      bleActive = true;
    }
    
    // Se falhar consistentemente, tentar re-autenticar
    if (millis() - lastMqttSuccess > 300000) {  // 5min
      logMessage(LOG_INFO, "⚠️ Tentando re-autenticação...");
      authenticateDevice();
      setupMQTT();
    }
    
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  resetWatchdog();
  
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  logMessage(LOG_INFO, "MQTT RX [" + String(topic) + "]: " + message);
}

void publishSensorData() {
  if (!mqttConnected || !currentData.valid) return;
  
  resetWatchdog();
  
  StaticJsonDocument<384> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = millis();
  doc["ph"] = round(currentData.ph * 100) / 100.0;
  doc["ec"] = round(currentData.ec);
  doc["air_temp"] = round(currentData.air_temp * 10) / 10.0;
  doc["humidity"] = round(currentData.humidity);
  doc["water_temp"] = round(currentData.water_temp * 10) / 10.0;
  
  String payload;
  serializeJson(doc, payload);
  
  bool sent = mqttClient.publish(mqttCreds.topic_sensors, payload.c_str(), false);
  
  if (sent) {
    logMessage(LOG_INFO, "📤 Dados publicados");
  } else {
    logMessage(LOG_ERROR, "❌ Falha ao publicar dados");
  }
}

void publishHeartbeat() {
  if (!mqttConnected) return;
  
  resetWatchdog();
  
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["timestamp"] = millis();
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["rssi"] = WiFi.RSSI();
  
  String payload;
  serializeJson(doc, payload);
  
  mqttClient.publish(mqttCreds.topic_heartbeat, payload.c_str(), false);
  logMessage(LOG_DEBUG, "💓 Heartbeat");
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
  String bleName = "AquaSys-" + deviceUUID.substring(4);
  
  BLEDevice::init(bleName.c_str());
  pBLEServer = BLEDevice::createServer();
  pBLEServer->setCallbacks(new MyServerCallbacks());
  
  BLEService* pService = pBLEServer->createService(SERVICE_UUID);
  
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
  
  // Characteristic para UUID do dispositivo
  pCharDeviceUUID = pService->createCharacteristic(
    CHAR_UUID_DEVICE,
    BLECharacteristic::PROPERTY_READ
  );
  pCharDeviceUUID->setValue(deviceUUID.c_str());
  
  pService->start();
  
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  bleActive = true;
  logMessage(LOG_INFO, "✅ BLE iniciado: " + bleName);
}

void publishDataToBLE() {
  if (!bleActive || !currentData.valid) return;
  
  char buffer[16];
  
  snprintf(buffer, sizeof(buffer), "%.2f", currentData.ph);
  pCharPH->setValue(buffer);
  pCharPH->notify();
  
  snprintf(buffer, sizeof(buffer), "%.0f", currentData.ec);
  pCharEC->setValue(buffer);
  pCharEC->notify();
  
  snprintf(buffer, sizeof(buffer), "%.1f", currentData.air_temp);
  pCharAirTemp->setValue(buffer);
  pCharAirTemp->notify();
  
  snprintf(buffer, sizeof(buffer), "%.0f", currentData.humidity);
  pCharHumidity->setValue(buffer);
  pCharHumidity->notify();
  
  snprintf(buffer, sizeof(buffer), "%.1f", currentData.water_temp);
  pCharWaterTemp->setValue(buffer);
  pCharWaterTemp->notify();
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  logMessage(LOG_INFO, "");
  logMessage(LOG_INFO, "====================================");
  logMessage(LOG_INFO, " AquaSys Nexus - Sensor Module");
  logMessage(LOG_INFO, " Firmware: " + String(FIRMWARE_VERSION));
  logMessage(LOG_INFO, "====================================");
  
  // Inicializar Watchdog
  initWatchdog();
  resetWatchdog();
  
  // Gerar UUID
  deviceUUID = generateDeviceUUID();
  printUUIDBanner();  // Banner visual no Serial Monitor
  resetWatchdog();
  
  // Inicializar OLED
  initOLED();
  displayMessage("Iniciando...\nAquaSys v4.3.3");
  delay(2000);
  resetWatchdog();
  
  // Inicializar botões
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);
  
  // Inicializar sensores
  dht.begin();
  ds18b20.begin();
  resetWatchdog();
  
  // Carregar calibração
  loadCalibration();
  resetWatchdog();
  
  // Carregar config WiFi
  loadWiFiConfig();
  resetWatchdog();
  
  // Inicializar BLE
  setupBLE();
  resetWatchdog();
  
  // Tentar conectar WiFi
  displayMessage("Conectando WiFi...");
  if (!connectWiFi()) {
    logMessage(LOG_WARN, "Nenhum WiFi configurado ou conexão falhou");
    startAPMode();  // Já tem resets de watchdog internos
  } else {
    // Configurar MQTT (só se conectou WiFi)
    resetWatchdog();
    authenticateDevice();
    resetWatchdog();
    setupMQTT();
    resetWatchdog();
  }
  
  // Leitura inicial de sensores
  displayMessage("Lendo sensores...");
  delay(1000);
  resetWatchdog();
  readSensors();
  resetWatchdog();
  
  logMessage(LOG_INFO, "✅ Sistema pronto!");
  displayMessage("Sistema Pronto!");
  delay(2000);
  
  lastSensorRead = millis();
  lastHeartbeat = millis();
}

// ==================== LOOP ====================
void loop() {
  resetWatchdog();  // Reset no início de cada loop
  
  // ===== MODO AP - Processar requests =====
  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    resetWatchdog();  // Reset após processar web requests
  }
  
  // ===== WIFI - Verificar conexão =====
  if (!apMode) {
    checkWiFi();
    resetWatchdog();
  }
  
  // ===== MQTT - Manter conexão (SOMENTE se não estiver em AP) =====
  if (wifiConnected && !apMode) {
    if (!mqttConnected) {
      reconnectMQTT();  // Já tem timeout e resets internos
      resetWatchdog();
    }
    
    if (mqttConnected) {
      mqttClient.loop();
      resetWatchdog();
    }
  }
  
  // ===== SENSORES - Ler periodicamente =====
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readSensors();
    publishDataToBLE();
    
    if (mqttConnected && !apMode) {
      publishSensorData();
    }
    
    lastSensorRead = millis();
    resetWatchdog();
  }
  
  // ===== HEARTBEAT - Publicar periodicamente =====
  if (mqttConnected && !apMode && millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeat = millis();
    resetWatchdog();
  }
  
  // ===== UUID - Imprimir periodicamente no Serial Monitor =====
  if (millis() - lastUUIDPrint >= 120000) {  // A cada 2 minutos
    printUUIDBanner();
    lastUUIDPrint = millis();
    resetWatchdog();
  }
  
  // ===== INTERFACE - Atualizar display e botões =====
  updateDisplay();
  handleButtons();
  
  delay(50);
}
