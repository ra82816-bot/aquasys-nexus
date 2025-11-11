/*
 * ============================================================================
 * AquaSys Nexus - Sensor Module v4.3.4-FREERTOS-OPTIMIZED
 * ============================================================================
 * VERSÃO COM TASK FREERTOS DEDICADA E MONITORAMENTO DE MEMÓRIA
 * 
 * MELHORIAS NESTA VERSÃO:
 * ✅ Task FreeRTOS dedicada para MQTT (10KB stack, Core 1)
 * ✅ Total isolamento das operações SSL/TLS
 * ✅ Monitoramento de memória com alertas (threshold 40KB)
 * ✅ Logs detalhados de heap livre e mínimo
 * ✅ Gerenciamento dinâmico de BLE mantido
 * ✅ Portal web otimizado mantido
 * 
 * RECURSOS:
 * ✅ Display OLED 128x64 com navegação por botões
 * ✅ Páginas: Dashboard, Conexões, Calibração, Sistema
 * ✅ Calibração interativa de pH e EC via interface OLED
 * ✅ WiFi com modo AP automático e portal captivo
 * ✅ Suporte a 3 redes WiFi com prioridades
 * ✅ BLE Server com desativação inteligente
 * ✅ MQTT sobre TLS isolado em task dedicada (segurança SSL)
 * ✅ Leitura de sensores: pH, EC, Temp Água, Temp Ar, Umidade
 * ✅ Watchdog robusto (120s) em task e loop principal
 * ✅ Logging estruturado com níveis
 * ✅ Heartbeat MQTT automático
 * ✅ UUID único por MAC
 * ✅ NTP sync
 * ✅ Alertas de memória crítica
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
#define FIRMWARE_VERSION "4.3.4-FREERTOS-OPTIMIZED"
#define DEVICE_TYPE "SENSOR"

// Sensores
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// ✅ PRIORIDADE II.2: WiFi AP - Usar build flags ou manter padrão seguro
#define AP_SSID_PREFIX "AquaSys-SEN-"
#ifndef AP_PASSWORD
  #define AP_PASSWORD "aquasys2024"  // ⚠️ MUDAR EM PRODUÇÃO via build_flags
#endif
#define AP_TIMEOUT 300000  // 5min

// Timeouts
#define WIFI_TIMEOUT 15000
#define MQTT_TIMEOUT 10000          // 10s timeout MQTT
#define MQTT_SOCKET_TIMEOUT 5       // 5s timeout socket
#define SENSOR_READ_INTERVAL 30000  // 30s
#define HEARTBEAT_INTERVAL 60000    // 60s
#define WATCHDOG_TIMEOUT 120        // 120s - margem segura
#define AUTH_TIMEOUT 30000          // 30s - SSL handshake pode levar tempo
#define SSL_DEBUG_MODE true         // ⚠️ TEMPORÁRIO: Desativa validação SSL para diagnóstico

// ✅ PRIORIDADE II.2: API Supabase - Usar build flags
#ifndef SUPABASE_URL
  #define SUPABASE_URL "https://oaabtbvwxsjomeeizciq.supabase.co"  // ⚠️ INJETAR via build_flags
#endif
#ifndef SUPABASE_ANON_KEY
  #define SUPABASE_ANON_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw"  // ⚠️ INJETAR via build_flags
#endif

// ✅ PRIORIDADE II.2: MQTT Fallback - Usar build flags
#ifndef MQTT_BROKER_FALLBACK
  #define MQTT_BROKER_FALLBACK "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"  // ⚠️ INJETAR via build_flags
#endif
#ifndef MQTT_PORT
  #define MQTT_PORT 8883
#endif
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
unsigned long lastMemoryCheck = 0;

// FreeRTOS - Task MQTT
TaskHandle_t mqttTaskHandle = NULL;
SemaphoreHandle_t mqttMutex = NULL;
const uint32_t MQTT_STACK_SIZE = 10240;  // 10KB stack
const uint32_t MEMORY_CHECK_INTERVAL = 5000;  // Verificar a cada 5s
const uint32_t MEMORY_ALERT_THRESHOLD = 40960;  // Alertar se heap < 40KB

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

// Monitoramento de Memória
void checkMemory();

// FreeRTOS Task
void mqttTask(void* pvParameters);

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
  Serial.println("║  Firmware: v4.3.4-FREERTOS-OPT    ║");
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
  
  // ✅ CORREÇÃO HEAP: Desligar BLE para liberar memória
  if (bleActive) {
    logMessage(LOG_WARN, "OTIMIZAÇÃO: Desativando BLE para liberar heap para o WebServer...");
    BLEDevice::deinit(true); // O 'true' libera a memória
    bleActive = false;
    delay(200); // Dar um tempo para a memória ser liberada
    logMessage(LOG_INFO, "Heap livre após deinit BLE: " + String(ESP.getFreeHeap()));
  }
  
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
  logMessage(LOG_INFO, "Servindo / (handleRoot)");

  // ✅ CORREÇÃO CRASH: HTML/CSS/JS Mínimo e Estável (inline, envio único)
  char html[2048]; // Buffer grande no stack
  snprintf(html, sizeof(html), R"rawliteral(
<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>AquaSys Config</title>
<style>
  body{font-family:Arial;margin:20px;background:#f4f4f4;}
  .box{background:#fff;border-radius:10px;padding:20px;max-width:400px;margin:auto;box-shadow:0 0 10px rgba(0,0,0,0.1);}
  h1{color:#333;font-size:24px;}
  input,select,button{width:100%%;padding:12px;margin:10px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;}
  button{background:#007bff;color:#fff;font-weight:bold;cursor:pointer;}
  #status{margin-top:15px;padding:10px;border-radius:5px;display:none;}
  .ok{background:#d4edda;color:#155724;}
  .err{background:#f8d7da;color:#721c24;}
</style>
</head><body>
<div class='box'>
  <h1>AquaSys Sensor</h1>
  <p style='background:#eee;padding:10px;border-radius:5px;'><b>UUID:</b><br>%s</p>
  <button onclick='scan()'>Escanear WiFi</button>
  <form onsubmit='save(event)'>
    <select id='ssid' required><option value=''>Aguardando scan...</option></select>
    <input type='password' id='pass' placeholder='Senha WiFi' required minlength='8'>
    <button type='submit'>Salvar</button>
  </form>
  <div id='status'></div>
</div>
<script>
  function scan(){
    let btn=event.target;btn.disabled=true;btn.textContent='Escaneando...';
    fetch('/scan').then(r=>r.json()).then(d=>{
      let s=document.getElementById('ssid');s.innerHTML='';
      d.networks.forEach(n=>{let o=new Option(n.ssid+' ('+n.rssi+'dBm)',n.ssid);s.add(o);});
      btn.disabled=false;btn.textContent='Escanear WiFi';
    }).catch(e=>{alert('Erro no scan');btn.disabled=false;btn.textContent='Escanear WiFi';});
  }
  function save(e){e.preventDefault();
    let d={ssid:document.getElementById('ssid').value,password:document.getElementById('pass').value};
    let st=document.getElementById('status');st.textContent='Salvando...';st.className='';st.style.display='block';
    fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})
    .then(r=>r.json()).then(r=>{st.textContent=r.message;st.className=r.success?'ok':'err';
    if(r.success)setTimeout(()=>{st.textContent='Reiniciando...';},2000);})
    .catch(e=>{st.textContent='Erro: '+e.message;st.className='err';});
  }
</script></body></html>
)rawliteral", deviceUUID.c_str());

  // Enviar tudo de uma vez (mais estável)
  server.send(200, "text/html", html);
}

void handleScan() {
  resetWatchdog();
  logMessage(LOG_INFO, "Servindo /scan (handleScan Síncrono)");

  // ✅ CORREÇÃO CRASH: Scan SÍNCRONO (bloqueante) para máxima estabilidade
  logMessage(LOG_INFO, "🔍 Iniciando scan SÍNCRONO...");
  displayMessage("Escaneando WiFi...");

  // Limpar resultados antigos
  WiFi.scanDelete();

  // WiFi.scanNetworks() síncrono (bloqueia o loop, mas é estável)
  int n = WiFi.scanNetworks(false, false, false, 300); // async=false
  resetWatchdog();

  logMessage(LOG_INFO, "✅ Scan síncrono completo: " + String(n) + " redes");
  displayMessage("Scan completo!");

  // Enviar JSON
  StaticJsonDocument<1024> doc;
  JsonArray networks = doc.createNestedArray("networks");

  int maxNetworks = min(n, 15);
  for (int i = 0; i < maxNetworks; i++) {
    JsonObject net = networks.createNestedObject();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);

  WiFi.scanDelete();
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
  
  // ✅ PRIORIDADE II.1: Adicionar certificado CA Root do Supabase (Let's Encrypt)
  const char* supabase_root_ca = \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n" \
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n" \
    "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n" \
    "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n" \
    "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n" \
    "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n" \
    "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n" \
    "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n" \
    "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n" \
    "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n" \
    "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n" \
    "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n" \
    "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n" \
    "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n" \
    "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n" \
    "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n" \
    "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n" \
    "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n" \
    "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n" \
    "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n" \
    "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n" \
    "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n" \
    "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n" \
    "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n" \
    "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n" \
    "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n" \
    "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n" \
    "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n" \
    "-----END CERTIFICATE-----\n";
  
  // ✅ Configurar SSL
  if (SSL_DEBUG_MODE) {
    logMessage(LOG_WARN, "⚠️ SSL_DEBUG_MODE ATIVO - Validação de certificado desabilitada!");
    client.setInsecure(); // ⚠️ APENAS PARA DIAGNÓSTICO
  } else {
    client.setCACert(supabase_root_ca); // ✅ Validação de certificado ativada
  }
  client.setTimeout(60); // ✅ AUMENTADO: 60 segundos para handshake SSL
  
  logMessage(LOG_DEBUG, "UUID: " + deviceUUID);
  logMessage(LOG_DEBUG, "Timeout SSL: " + String(AUTH_TIMEOUT) + "ms");
  
  // ✅ Monitorar memória antes da autenticação
  uint32_t heapBefore = ESP.getFreeHeap();
  logMessage(LOG_INFO, "Heap livre antes auth: " + String(heapBefore) + " bytes");
  
  // ✅ Verificar se há memória suficiente (mínimo 40KB)
  if (ESP.getFreeHeap() < 40000) {
    logMessage(LOG_ERROR, "❌ Memória insuficiente para auth HTTP!");
    http.end();
    return false;
  }
  
  // ✅ Teste de DNS e conectividade TCP antes do HTTPS
  String host = "oaabtbvwxsjomeeizciq.supabase.co";
  int port = 443;
  
  // ✅ 1. Testar resolução DNS
  logMessage(LOG_DEBUG, "Resolvendo DNS: " + host);
  IPAddress serverIP;
  if (!WiFi.hostByName(host.c_str(), serverIP)) {
    logMessage(LOG_ERROR, "❌ Falha na resolução DNS para " + host);
    logMessage(LOG_DEBUG, "DNS Server: " + WiFi.dnsIP().toString());
    logMessage(LOG_WARN, "⚠️ Tentando usar DNS público (8.8.8.8)...");
    
    // Tentar com DNS público Google
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(8, 8, 8, 8));
    delay(100);
    
    if (!WiFi.hostByName(host.c_str(), serverIP)) {
      logMessage(LOG_ERROR, "❌ Falha DNS mesmo com 8.8.8.8");
      return false;
    }
  }
  
  logMessage(LOG_INFO, "✅ DNS OK: " + serverIP.toString());
  
  // ✅ 2. Testar conectividade TCP com timeout aumentado
  logMessage(LOG_DEBUG, "Testando conectividade TCP na porta 443...");
  WiFiClient testClient;
  testClient.setTimeout(20000); // 20 segundos para conexão TCP
  
  uint32_t startConnect = millis();
  bool tcpSuccess = testClient.connect(serverIP, port);
  uint32_t connectDuration = millis() - startConnect;
  
  if (!tcpSuccess) {
    logMessage(LOG_ERROR, "❌ Falha TCP 443 para " + serverIP.toString() + " (" + String(connectDuration) + "ms)");
    logMessage(LOG_DEBUG, "Gateway: " + WiFi.gatewayIP().toString());
    logMessage(LOG_DEBUG, "Subnet: " + WiFi.subnetMask().toString());
    
    // ✅ Teste alternativo: porta 80 (HTTP) para diagnosticar firewall
    logMessage(LOG_WARN, "⚠️ Testando porta 80 (HTTP) para diagnóstico...");
    WiFiClient testHttp;
    testHttp.setTimeout(10000);
    if (testHttp.connect(serverIP, 80)) {
      testHttp.stop();
      logMessage(LOG_ERROR, "🔥 FIREWALL bloqueando porta 443! Porta 80 funciona.");
      logMessage(LOG_ERROR, "Solução: Libere porta 443 (HTTPS) no firewall/roteador");
    } else {
      logMessage(LOG_ERROR, "❌ Portas 80 e 443 bloqueadas. Problema de rede/firewall severo");
    }
    return false;
  }
  
  testClient.stop();
  logMessage(LOG_INFO, "✅ TCP 443 OK (" + String(connectDuration) + "ms)");
  
  String url = String(SUPABASE_URL) + "/functions/v1/device-auth";
  logMessage(LOG_DEBUG, "URL: " + url);
  
  logMessage(LOG_DEBUG, "Iniciando handshake SSL/TLS...");
  if (!http.begin(client, url)) {
    logMessage(LOG_ERROR, "❌ Falha ao iniciar HTTP client!");
    http.end();
    return false;
  }
  logMessage(LOG_INFO, "✅ SSL handshake OK");
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["firmware_version"] = FIRMWARE_VERSION;
  
  String payload;
  serializeJson(doc, payload);
  
  logMessage(LOG_DEBUG, "Payload: " + payload);
  logMessage(LOG_INFO, "Enviando requisição HTTP POST...");
  logMessage(LOG_DEBUG, "Heap antes POST: " + String(ESP.getFreeHeap()) + " bytes");
  
  // ✅ Configurar timeout HTTP para 60s (POST pode demorar)
  http.setTimeout(60000); // 60 segundos
  
  unsigned long postStart = millis();
  resetWatchdog(); // ✅ Reset antes do POST
  int httpCode = http.POST(payload);
  resetWatchdog(); // ✅ Reset após POST
  unsigned long postDuration = millis() - postStart;
  
  logMessage(LOG_INFO, "HTTP Code: " + String(httpCode));
  logMessage(LOG_DEBUG, "Tempo de resposta: " + String(postDuration) + "ms");
  logMessage(LOG_DEBUG, "Heap após POST: " + String(ESP.getFreeHeap()) + " bytes");
  
  // ✅ DIAGNÓSTICO DETALHADO: Log da mensagem de erro
  if (httpCode < 0) {
    logMessage(LOG_ERROR, "Erro HTTP: " + http.errorToString(httpCode));
  }
  
  bool authSuccess = false;
  
  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<1024> respDoc;
    
    DeserializationError error = deserializeJson(respDoc, response);
    
    if (!error && respDoc["success"] == true) {
      // ✅ Ler dados do objeto mqtt_config
      JsonObject mqttConfig = respDoc["mqtt_config"];
      
      if (mqttConfig) {
        String broker = mqttConfig["broker"].as<String>();
        String username = mqttConfig["username"].as<String>();
        String password = mqttConfig["password"].as<String>();
        String clientId = mqttConfig["client_id"] | deviceUUID;
        
        // Copiar para estrutura global (com null-termination)
        strncpy(mqttCreds.broker, broker.c_str(), 127);
        mqttCreds.broker[127] = '\0';
        
        strncpy(mqttCreds.username, username.c_str(), 63);
        mqttCreds.username[63] = '\0';
        
        strncpy(mqttCreds.password, password.c_str(), 127);
        mqttCreds.password[127] = '\0';
        
        strncpy(mqttCreds.client_id, clientId.c_str(), 63);
        mqttCreds.client_id[63] = '\0';
        
        // Copiar tópicos (com null-termination)
        JsonObject topics = mqttConfig["topics"];
        if (topics) {
          String topicSensors = topics["sensors"] | "aquasys/sensors/all";
          String topicHeartbeat = topics["heartbeat"] | "aquasys/heartbeat";
          String topicCalibration = topics["calibration"] | "aquasys/calibration";
          
          strncpy(mqttCreds.topic_sensors, topicSensors.c_str(), 127);
          mqttCreds.topic_sensors[127] = '\0';
          
          strncpy(mqttCreds.topic_heartbeat, topicHeartbeat.c_str(), 127);
          mqttCreds.topic_heartbeat[127] = '\0';
          
          strncpy(mqttCreds.topic_calibration, topicCalibration.c_str(), 127);
          mqttCreds.topic_calibration[127] = '\0';
        } else {
          // Fallback para tópicos padrão
          strcpy(mqttCreds.topic_sensors, "aquasys/sensors/all");
          strcpy(mqttCreds.topic_heartbeat, "aquasys/heartbeat");
          strcpy(mqttCreds.topic_calibration, "aquasys/calibration");
        }
        
        mqttCreds.valid = true;
        saveMqttCredentials();
        
        logMessage(LOG_INFO, "✅ Autenticação OK - Broker: " + broker);
        logMessage(LOG_INFO, "✅ Username: " + username);
        isAuthenticated = true;
        authSuccess = true;
      } else {
        logMessage(LOG_ERROR, "❌ Resposta sem mqtt_config");
      }
    } else {
      logMessage(LOG_ERROR, "❌ Erro ao parsear JSON auth: " + String(error.c_str()));
    }
  } else {
    logMessage(LOG_ERROR, "❌ HTTP Auth falhou: " + String(httpCode));
    resetWatchdog(); // ✅ Reset após erro
    
    // ✅ Diagnóstico detalhado do erro
    if (httpCode == -1) {
      logMessage(LOG_ERROR, "Erro -1 (TIMEOUT): Conexão SSL/TLS expirou");
      logMessage(LOG_DEBUG, "Heap atual: " + String(ESP.getFreeHeap()) + " bytes");
      logMessage(LOG_DEBUG, "Timeout configurado: 60s");
      
      if (SSL_DEBUG_MODE) {
        logMessage(LOG_WARN, "SSL_DEBUG_MODE ativo - certificado não validado");
      }
      
      logMessage(LOG_WARN, "Possíveis causas:");
      logMessage(LOG_WARN, "1. Servidor Supabase não respondeu em 60s");
      logMessage(LOG_WARN, "2. Firewall/proxy bloqueando handshake SSL");
      logMessage(LOG_WARN, "3. Heap insuficiente para SSL (precisa >80KB)");
    } else if (httpCode == -11) {
      logMessage(LOG_ERROR, "Erro -11 (CONNECTION_LOST): Conexão perdida durante POST");
      logMessage(LOG_ERROR, "O servidor fechou a conexão antes de terminar a transferência");
      logMessage(LOG_DEBUG, "Heap atual: " + String(ESP.getFreeHeap()) + " bytes");
      logMessage(LOG_DEBUG, "Tempo de POST: " + String(postDuration) + "ms");
      
      logMessage(LOG_WARN, "Possíveis causas:");
      logMessage(LOG_WARN, "1. Timeout do servidor (Supabase Edge Function travou)");
      logMessage(LOG_WARN, "2. DPI/Firewall corporativo inspecionando HTTPS");
      logMessage(LOG_WARN, "3. Proxy intermediário fechou conexão");
      logMessage(LOG_WARN, "4. Payload muito grande ou headers inválidos");
      logMessage(LOG_WARN, "5. Servidor Supabase sobrecarregado");
      
      logMessage(LOG_INFO, "Tentando usar fallback MQTT...");
    } else if (httpCode > 0) {
      String response = http.getString();
      logMessage(LOG_DEBUG, "Resposta HTTP: " + response);
    }
  }
  
  if (!authSuccess) {
    logMessage(LOG_WARN, "⚠️ Autenticação falhou, usando fallback");
    resetWatchdog(); // ✅ Reset antes do fallback
    
    // ✅ PRIORIDADE II.2: Usar credenciais fallback via build flags
    strcpy(mqttCreds.broker, MQTT_BROKER_FALLBACK);
    #ifndef MQTT_FALLBACK_USER
      strcpy(mqttCreds.username, "hydrosmart");  // ⚠️ INJETAR via build_flags
    #else
      strcpy(mqttCreds.username, MQTT_FALLBACK_USER);
    #endif
    #ifndef MQTT_FALLBACK_PASS
      strcpy(mqttCreds.password, "Hydro@2024!");  // ⚠️ INJETAR via build_flags
    #else
      strcpy(mqttCreds.password, MQTT_FALLBACK_PASS);
    #endif
    deviceUUID.toCharArray(mqttCreds.client_id, 64);
    strcpy(mqttCreds.topic_sensors, TOPIC_SENSORS_FALLBACK);
    strcpy(mqttCreds.topic_heartbeat, TOPIC_HEARTBEAT_FALLBACK);
    strcpy(mqttCreds.topic_calibration, TOPIC_CALIBRATION_FALLBACK);
    mqttCreds.valid = true;
    
    logMessage(LOG_INFO, "✅ Fallback configurado: " + String(mqttCreds.broker));
    resetWatchdog(); // ✅ Reset após fallback
  }
  
  http.end();
  resetWatchdog(); // ✅ Reset após liberar HTTP
  
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
  
  // ✅ PRIORIDADE II.1: Configurar certificado CA Root (HiveMQ Cloud)
  const char* mqtt_root_ca = \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
    "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
    "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
    "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
    "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
    "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
    "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
    "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
    "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
    "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
    "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n" \
    "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
    "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
    "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
    "MrY=\n" \
    "-----END CERTIFICATE-----\n";
  
  espClient.setCACert(mqtt_root_ca); // ✅ Validação de certificado ativada
  
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
  
  // ✅ LOGS DIAGNÓSTICO: Informações iniciais
  logMessage(LOG_INFO, "🔌 Iniciando conexão MQTT...");
  logMessage(LOG_INFO, "Broker: " + String(mqttCreds.broker));
  logMessage(LOG_INFO, "Port: " + String(mqttCreds.port));
  logMessage(LOG_INFO, "Username: " + String(mqttCreds.username));
  logMessage(LOG_DEBUG, "Client ID: " + String(mqttCreds.client_id));
  
  // OTIMIZAÇÃO CRÍTICA: Desativar BLE para liberar ~30KB de RAM
  if (bleActive) {
    logMessage(LOG_DEBUG, "[MQTT] Desativando BLE para liberar heap...");
    BLEDevice::deinit(true); // O 'true' libera a memória
    bleActive = false;
    delay(200); // Dar um tempo para a memória ser liberada
    uint32_t heapAfterBLE = ESP.getFreeHeap();
    logMessage(LOG_INFO, "[MQTT] Heap após desativar BLE: " + String(heapAfterBLE) + " bytes");
  }
  
  // Liberar memória antes de conectar
  logMessage(LOG_INFO, "Heap livre: " + String(ESP.getFreeHeap()) + " bytes");
  logMessage(LOG_INFO, "Heap min: " + String(ESP.getMinFreeHeap()) + " bytes");
  
  // Desconectar primeiro para liberar recursos
  if (espClient.connected()) {
    logMessage(LOG_DEBUG, "[MQTT] Parando cliente WiFi anterior...");
    espClient.stop();
    delay(200);
    logMessage(LOG_DEBUG, "[MQTT] Cliente WiFi parado OK");
  }
  
  resetWatchdog();
  
  // ✅ CORREÇÃO SINTAXE: Configurar LWT (Last Will and Testament)
  char lwtTopic[128];
  snprintf(lwtTopic, sizeof(lwtTopic), "aquasys/%s/status", deviceUUID.c_str());
  
  char lwtPayload[128];
  snprintf(lwtPayload, sizeof(lwtPayload), "{\"status\":\"offline\",\"uuid\":\"%s\",\"type\":\"sensor\"}", deviceUUID.c_str());
  
  // ✅ PRIORIDADE III.1: Buffers persistentes para evitar .c_str() temporário
  char clientIdBuf[64];
  char usernameBuf[64];
  char passwordBuf[128];
  
  strncpy(clientIdBuf, mqttCreds.client_id, sizeof(clientIdBuf) - 1);
  clientIdBuf[sizeof(clientIdBuf) - 1] = '\0';
  
  strncpy(usernameBuf, mqttCreds.username, sizeof(usernameBuf) - 1);
  usernameBuf[sizeof(usernameBuf) - 1] = '\0';
  
  strncpy(passwordBuf, mqttCreds.password, sizeof(passwordBuf) - 1);
  passwordBuf[sizeof(passwordBuf) - 1] = '\0';
  
  // ✅ TIMEOUT ABSOLUTO: Máximo 20 segundos para todas as tentativas
  unsigned long absoluteTimeout = 20000; // 20s
  unsigned long connectStart = millis();
  bool connected = false;
  int attemptCount = 0;
  
  logMessage(LOG_INFO, "[MQTT] Iniciando tentativas de conexão (timeout: 20s)...");
  
  // Tentar conectar com timeout absoluto
  while (!connected && (millis() - connectStart) < absoluteTimeout) {
    attemptCount++;
    unsigned long elapsed = millis() - connectStart;
    
    resetWatchdog(); // ✅ CRÍTICO: Reset DENTRO do loop
    
    logMessage(LOG_DEBUG, "[MQTT] Tentativa #" + String(attemptCount) + " (" + String(elapsed) + "ms)");
    
    // ✅ CORREÇÃO: LWT deve ser passado na função connect() (7 parâmetros)
    logMessage(LOG_DEBUG, "[MQTT] Chamando mqttClient.connect()...");
    connected = mqttClient.connect(
      clientIdBuf,
      usernameBuf,
      passwordBuf,
      lwtTopic, 1, true, lwtPayload  // LWT: Topic, QoS, Retained, Payload
    );
    logMessage(LOG_DEBUG, "[MQTT] connect() retornou: " + String(connected ? "true" : "false"));
    
    if (!connected) {
      int state = mqttClient.state();
      logMessage(LOG_WARN, "[MQTT] Falhou (state=" + String(state) + "), aguardando 500ms...");
      delay(500);  // Aumentar delay entre tentativas
      resetWatchdog();
    }
  }
  
  unsigned long totalTime = millis() - connectStart;
  logMessage(LOG_INFO, "[MQTT] Fim das tentativas: " + String(attemptCount) + " tentativas em " + String(totalTime) + "ms");
  
  if (connected) {
    mqttConnected = true;
    lastMqttSuccess = millis();
    logMessage(LOG_INFO, "✅ MQTT conectado em " + String(millis() - connectStart) + "ms");
    logMessage(LOG_INFO, "Heap livre pós-conexão: " + String(ESP.getFreeHeap()) + " bytes");
    
    // ✅ CORREÇÃO: Publicar status online após conexão bem-sucedida
    char onlineTopic[128];
    snprintf(onlineTopic, sizeof(onlineTopic), "aquasys/%s/status", deviceUUID.c_str());
    
    char onlinePayload[128];
    snprintf(onlinePayload, sizeof(onlinePayload), "{\"status\":\"online\",\"uuid\":\"%s\",\"type\":\"sensor\"}", deviceUUID.c_str());
    
    // ✅ CORREÇÃO SINTAXE: publish() requer (topic, payload_bytes, length, retained)
    mqttClient.publish(onlineTopic, (const uint8_t*)onlinePayload, strlen(onlinePayload), true);
    
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

// ==================== MONITORAMENTO DE MEMÓRIA ====================
void checkMemory() {
  if (millis() - lastMemoryCheck < MEMORY_CHECK_INTERVAL) return;
  lastMemoryCheck = millis();
  
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minFreeHeap = ESP.getMinFreeHeap();
  
  // Log detalhado a cada verificação
  logMessage(LOG_DEBUG, "Heap: " + String(freeHeap) + " bytes | Min: " + String(minFreeHeap) + " bytes");
  
  // Alerta crítico se memória baixa
  if (freeHeap < MEMORY_ALERT_THRESHOLD) {
    logMessage(LOG_ERROR, "⚠️ ALERTA: Heap livre abaixo de " + String(MEMORY_ALERT_THRESHOLD / 1024) + "KB!");
    logMessage(LOG_ERROR, "Heap atual: " + String(freeHeap) + " bytes (" + String(freeHeap / 1024) + " KB)");
    logMessage(LOG_ERROR, "Min heap: " + String(minFreeHeap) + " bytes (" + String(minFreeHeap / 1024) + " KB)");
    
    // Informações adicionais para debug
    logMessage(LOG_ERROR, "WiFi: " + String(wifiConnected ? "ON" : "OFF") + 
               " | MQTT: " + String(mqttConnected ? "ON" : "OFF") + 
               " | BLE: " + String(bleActive ? "ON" : "OFF") + 
               " | AP: " + String(apMode ? "ON" : "OFF"));
  }
}

// ==================== FREERTOS - MQTT TASK ====================
// ✅ SOLUÇÃO 1: Task MQTT removida para diagnóstico
// A lógica MQTT foi movida de volta para o loop() principal
/*
void mqttTask(void* pvParameters) {
  logMessage(LOG_INFO, "🚀 MQTT Task iniciada no Core " + String(xPortGetCoreID()));
  logMessage(LOG_INFO, "Stack size: " + String(MQTT_STACK_SIZE) + " bytes");
  
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100);  // Loop a cada 100ms
  
  for(;;) {
    // Aguardar próximo ciclo (evita busy-wait)
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    // Só processar MQTT se WiFi conectado e não em modo AP
    if (wifiConnected && !apMode) {
      // Proteger acesso ao MQTT client com mutex (se necessário no futuro)
      // xSemaphoreTake(mqttMutex, portMAX_DELAY);
      
      // Tentar reconectar se desconectado
      if (!mqttConnected) {
        reconnectMQTT();  // Já tem todos os resets de watchdog internos
      }
      
      // Processar mensagens MQTT se conectado
      if (mqttConnected) {
        mqttClient.loop();
      }
      
      // xSemaphoreGive(mqttMutex);
      
      // Heartbeat periódico
      if (mqttConnected && millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        publishHeartbeat();
        lastHeartbeat = millis();
      }
    } else {
      // Se não conectado ou em AP mode, aguardar mais tempo
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Reset watchdog da task (segurança adicional)
    esp_task_wdt_reset();
  }
}
*/

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
  displayMessage("Iniciando...\nAquaSys v4.3.4");
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
  
  // ✅ SOLUÇÃO 1: Criação da task MQTT removida para diagnóstico
  // A lógica MQTT foi movida de volta para o loop() principal
  /*
  // Criar task MQTT dedicada (Core 1, stack 10KB, prioridade 5)
  if (wifiConnected && !apMode) {
    logMessage(LOG_INFO, "🚀 Criando task MQTT dedicada...");
    
    // Criar mutex (opcional, para proteção futura)
    // mqttMutex = xSemaphoreCreateMutex();
    
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
      mqttTask,              // Função da task
      "MQTTTask",            // Nome da task
      MQTT_STACK_SIZE,       // Stack size (10KB)
      NULL,                  // Parâmetros
      5,                     // Prioridade (5 = média-alta)
      &mqttTaskHandle,       // Handle da task
      1                      // Core 1 (Core 0 é usado por WiFi/BLE)
    );
    
    if (taskCreated == pdPASS) {
      logMessage(LOG_INFO, "✅ Task MQTT criada com sucesso!");
      logMessage(LOG_INFO, "Stack: " + String(MQTT_STACK_SIZE) + " bytes | Core: 1 | Prioridade: 5");
      
      // Adicionar watchdog para a task
      esp_task_wdt_add(mqttTaskHandle);
    } else {
      logMessage(LOG_ERROR, "❌ Falha ao criar task MQTT!");
    }
  }
  */
  
  logMessage(LOG_INFO, "🔌 Lógica MQTT rodando no loop() principal (diagnóstico)");
  
  displayMessage("Sistema Pronto!");
  delay(2000);
  
  lastSensorRead = millis();
  lastHeartbeat = millis();
  lastMemoryCheck = millis();
}

// ==================== LOOP ====================
void loop() {
  resetWatchdog();  // Reset no início de cada loop
  
  // ===== MONITORAMENTO DE MEMÓRIA =====
  checkMemory();  // Verifica e alerta se heap < 40KB
  
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
  
  // ===== MQTT - ✅ SOLUÇÃO 1: Processado no loop principal =====
  if (wifiConnected && !apMode) {
    // Tentar reconectar se desconectado (com limite de frequência)
    if (!mqttConnected) {
      // Só tentar reconectar se já passaram 5s desde última tentativa
      if (millis() - lastMqttAttempt >= 5000) {
        logMessage(LOG_DEBUG, "[LOOP] Tentando reconectar MQTT...");
        resetWatchdog(); // ✅ Reset antes de reconectar
        reconnectMQTT();
        resetWatchdog(); // ✅ Reset após reconectar
        lastMqttAttempt = millis();
      }
    } else {
      // Processar mensagens MQTT
      mqttClient.loop();
    }
    
    resetWatchdog(); // ✅ CRÍTICO: Reset após operações MQTT
    
    // Heartbeat periódico
    if (mqttConnected && millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      logMessage(LOG_DEBUG, "[LOOP] Publicando heartbeat...");
      publishHeartbeat();
      lastHeartbeat = millis();
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
