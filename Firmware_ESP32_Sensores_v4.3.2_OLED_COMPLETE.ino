/*
 * ============================================================================
 * Firmware ESP32 - Módulo de Sensores com OLED v4.3.2
 * ============================================================================
 * Características:
 * - Display OLED 128x64 com interface completa
 * - Navegação por páginas (Dashboard, Conexões, Calibração, Sistema)
 * - Calibração interativa de pH e EC
 * - BLE para configuração via app móvel
 * - WiFi Manager com fallback para AP
 * - MQTT sobre TLS para comunicação com servidor
 * - Watchdog para estabilidade
 * ============================================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_task_wdt.h>

// ============================================================================
// DEFINIÇÕES DE PINOS (Conforme arquivo de referência)
// ============================================================================
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

// Pinos dos botões
#define BUTTON_UP 32
#define BUTTON_DOWN 33
#define BUTTON_SELECT 25
#define BUTTON_BACK 26

// ============================================================================
// CONFIGURAÇÕES
// ============================================================================
#define WATCHDOG_TIMEOUT 60  // 60 segundos
#define MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USERNAME "esp32-user"
#define MQTT_PASSWORD "HydroSmart123"
#define MQTT_TOPIC "aquasys/sensors/all"

// ============================================================================
// OBJETOS
// ============================================================================
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Preferences preferences;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// ============================================================================
// VARIÁVEIS GLOBAIS - CALIBRAÇÃO
// ============================================================================
float cal_ph7_voltage = 2.52;
float cal_ph4_voltage = 3.29;
float ph_slope, ph_intercept;

float calibration_low_raw = 645.0;
float calibration_high_raw = 2850.0;
float calibration_low_ec = 360.0;
float calibration_high_ec = 4588.0;

// ============================================================================
// VARIÁVEIS GLOBAIS - SENSORES
// ============================================================================
float temperature_C = 25.0;
float humidity = 0.0;
float water_temperature_C = 0.0;
float lastPhValue = 0.0;
float lastEcValue = 0.0;

// ============================================================================
// VARIÁVEIS GLOBAIS - SISTEMA
// ============================================================================
String deviceUUID = "";
String ssid_sta = "";
String password_sta = "";
bool wifiConfigured = false;
bool mqttConnected = false;
unsigned long lastMQTTPublish = 0;
const unsigned long mqttInterval = 10000;

// ============================================================================
// ENUMERAÇÕES - INTERFACE
// ============================================================================
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

Page currentPage = PAGE_DASHBOARD;
CalibrationMode calibrationMode = CAL_NONE;

// ============================================================================
// VARIÁVEIS - DEBOUNCE DOS BOTÕES
// ============================================================================
unsigned long lastDebounce[4] = {0, 0, 0, 0};
const unsigned long debounceDelay = 200;

// ============================================================================
// BLE - CONFIGURAÇÃO
// ============================================================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_WIFI_SSID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_WIFI_PASS "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
#define CHAR_WIFI_LIST "a3c87500-8ed3-4bdf-8a39-a01bebede295"
#define CHAR_STATUS "d4e5f6a7-b8c9-4d3e-a2f1-0b1c2d3e4f5a"

BLEServer *pServer = nullptr;
BLECharacteristic *pCharWiFiSSID = nullptr;
BLECharacteristic *pCharWiFiPass = nullptr;
BLECharacteristic *pCharWiFiList = nullptr;
BLECharacteristic *pCharStatus = nullptr;
bool bleConnected = false;

// ============================================================================
// PROTÓTIPOS DE FUNÇÕES
// ============================================================================
void initWatchdog();
void resetWatchdog();
void generateDeviceUUID();
void initBLE();
void initOLED();
void readSensors();
void updateDisplay();
void handleButtons();
float readAverageADC(int pin, int samples = 10);
float voltageToPH(float voltage);
float interpolateEC(float rawValue);
float temperatureCompensateEC(float ec, float temp);
void calculatePHCoefficients();
void saveCalibration();
void loadCalibration();
void displayMessage(const char *message);
void connectToWiFi();
void startAPMode();
void reconnectMQTT();
void publishData();
void setupWebServer();
void handleScan();
void handleSave();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n[INFO] ==========================================");
  Serial.println("[INFO] ESP32 Sensor Module - v4.3.2 OLED");
  Serial.println("[INFO] ==========================================");
  
  // Gerar UUID do dispositivo
  generateDeviceUUID();
  Serial.printf("[INFO] Device UUID: %s\n", deviceUUID.c_str());
  
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
  Serial.println("[INFO] Sensores DHT22 e DS18B20 iniciados");
  
  // Carregar calibrações
  preferences.begin("calib", false);
  loadCalibration();
  calculatePHCoefficients();
  preferences.end();
  
  // Carregar credenciais WiFi
  preferences.begin("wifi", false);
  ssid_sta = preferences.getString("ssid0", "");
  password_sta = preferences.getString("pass0", "");
  preferences.end();
  
  if (ssid_sta.length() == 0) {
    Serial.println("[WARN] ⚠️ Nenhuma credencial salva, usando fallback");
  }
  
  // Inicializar BLE
  Serial.println("[INFO] Inicializando BLE Server...");
  initBLE();
  
  // Tentar conectar WiFi
  if (ssid_sta.length() > 0) {
    displayMessage("Conectando WiFi...");
    connectToWiFi();
  }
  
  // Se não conectou, iniciar AP
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WARN] Falha ao conectar WiFi, iniciando AP...");
    startAPMode();
  } else {
    wifiConfigured = true;
    String msg = "WiFi OK\nIP: " + WiFi.localIP().toString();
    displayMessage(msg.c_str());
    delay(2000);
  }
  
  // Configurar servidor web
  setupWebServer();
  
  // Configurar MQTT
  espClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  
  Serial.println("[INFO] ✅ Setup completo!");
  Serial.printf("[INFO] Memória livre: %d bytes\n", ESP.getFreeHeap());
  displayMessage("Sistema pronto!");
  delay(1000);
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
  resetWatchdog();
  
  // Verificar conexão WiFi
  if (WiFi.status() != WL_CONNECTED && ssid_sta.length() > 0) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 30000) {
      Serial.println("[WARN] WiFi desconectado, tentando reconectar...");
      connectToWiFi();
      lastReconnect = millis();
    }
  }
  
  // Gerenciar MQTT
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();
    
    // Publicar dados periodicamente
    if (millis() - lastMQTTPublish >= mqttInterval) {
      readSensors();
      publishData();
      lastMQTTPublish = millis();
    }
  } else {
    // Modo AP - processar requisições web
    server.handleClient();
  }
  
  // Atualizar interface
  handleButtons();
  updateDisplay();
  
  delay(50);
}

// ============================================================================
// WATCHDOG
// ============================================================================
void initWatchdog() {
  // Verificar se o watchdog já está inicializado
  esp_err_t status = esp_task_wdt_status(NULL);
  
  if (status == ESP_ERR_NOT_FOUND) {
    // Watchdog não inicializado, configurar
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WATCHDOG_TIMEOUT * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true
    };
    
    esp_err_t err = esp_task_wdt_init(&wdt_config);
    if (err == ESP_OK) {
      esp_task_wdt_add(NULL);
      Serial.println("[INFO] ✅ Watchdog iniciado (60s)");
    } else {
      Serial.printf("[ERROR] ❌ Falha ao iniciar watchdog: %d\n", err);
    }
  } else {
    // Watchdog já inicializado, apenas adicionar task
    esp_task_wdt_add(NULL);
    Serial.println("[INFO] ✅ Watchdog já ativo, task adicionada");
  }
}

void resetWatchdog() {
  esp_task_wdt_reset();
}

// ============================================================================
// GERAÇÃO DE UUID
// ============================================================================
void generateDeviceUUID() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char uuid[32];
  snprintf(uuid, sizeof(uuid), "SEN-%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  deviceUUID = String(uuid);
}

// ============================================================================
// INICIALIZAÇÃO DO OLED
// ============================================================================
void initOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERROR] ❌ Erro ao inicializar OLED");
    while (true) {
      delay(100);
    }
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  Serial.println("[INFO] ✅ OLED inicializado");
}

// ============================================================================
// BLE - CALLBACKS
// ============================================================================
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    bleConnected = true;
    Serial.println("[BLE] Cliente conectado");
  }
  
  void onDisconnect(BLEServer *pServer) {
    bleConnected = false;
    Serial.println("[BLE] Cliente desconectado");
    pServer->startAdvertising();
  }
};

class WiFiCredentialsCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    if (pCharacteristic->getUUID().toString() == CHAR_WIFI_SSID) {
      ssid_sta = String(value.c_str());
      Serial.printf("[BLE] SSID recebido: %s\n", ssid_sta.c_str());
    } else if (pCharacteristic->getUUID().toString() == CHAR_WIFI_PASS) {
      password_sta = String(value.c_str());
      Serial.println("[BLE] Senha recebida");
      
      // Salvar e conectar
      preferences.begin("wifi", false);
      preferences.putString("ssid0", ssid_sta);
      preferences.putString("pass0", password_sta);
      preferences.end();
      
      Serial.println("[BLE] Credenciais salvas, conectando...");
      connectToWiFi();
      
      // Enviar status
      if (WiFi.status() == WL_CONNECTED) {
        pCharStatus->setValue("connected");
        wifiConfigured = true;
      } else {
        pCharStatus->setValue("failed");
      }
      pCharStatus->notify();
    }
  }
};

// ============================================================================
// INICIALIZAÇÃO BLE
// ============================================================================
void initBLE() {
  BLEDevice::init(deviceUUID.c_str());
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // Característica: SSID
  pCharWiFiSSID = pService->createCharacteristic(
    CHAR_WIFI_SSID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pCharWiFiSSID->setCallbacks(new WiFiCredentialsCallbacks());
  
  // Característica: Password
  pCharWiFiPass = pService->createCharacteristic(
    CHAR_WIFI_PASS,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharWiFiPass->setCallbacks(new WiFiCredentialsCallbacks());
  
  // Característica: WiFi List
  pCharWiFiList = pService->createCharacteristic(
    CHAR_WIFI_LIST,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharWiFiList->addDescriptor(new BLE2902());
  
  // Característica: Status
  pCharStatus = pService->createCharacteristic(
    CHAR_STATUS,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharStatus->addDescriptor(new BLE2902());
  pCharStatus->setValue("disconnected");
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();
  
  Serial.printf("[INFO] ✅ BLE Server ativo: %s\n", deviceUUID.c_str());
}

// ============================================================================
// LEITURA DE SENSORES
// ============================================================================
void readSensors() {
  // pH
  float phVoltage = readAverageADC(PH_SENSOR_PIN) * 3.3 / 4095.0;
  lastPhValue = voltageToPH(phVoltage);
  
  // TDS/EC
  float tdsRaw = readAverageADC(TDS_SENSOR_PIN);
  float ecValue = interpolateEC(tdsRaw);
  lastEcValue = temperatureCompensateEC(ecValue, temperature_C);
  
  // DHT22
  float tempRead = dht.readTemperature();
  float humRead = dht.readHumidity();
  if (!isnan(tempRead)) temperature_C = tempRead;
  if (!isnan(humRead)) humidity = humRead;
  
  // DS18B20
  ds18b20.requestTemperatures();
  water_temperature_C = ds18b20.getTempCByIndex(0);
  
  // Log no serial
  Serial.printf("[SENSOR] pH: %.2f | EC: %.0f uS/cm | Temp: %.1f°C | Hum: %.1f%% | Water: %.1f°C\n",
                lastPhValue, lastEcValue, temperature_C, humidity, water_temperature_C);
}

// ============================================================================
// FUNÇÕES DE CONVERSÃO
// ============================================================================
float readAverageADC(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return (float)sum / samples;
}

float voltageToPH(float voltage) {
  return ph_slope * voltage + ph_intercept;
}

float interpolateEC(float rawValue) {
  if (rawValue <= calibration_low_raw) return calibration_low_ec;
  if (rawValue >= calibration_high_raw) return calibration_high_ec;
  
  float ratio = (rawValue - calibration_low_raw) / (calibration_high_raw - calibration_low_raw);
  return calibration_low_ec + ratio * (calibration_high_ec - calibration_low_ec);
}

float temperatureCompensateEC(float ec, float temp) {
  return ec / (1.0 + 0.02 * (temp - 25.0));
}

// ============================================================================
// CALIBRAÇÃO
// ============================================================================
void calculatePHCoefficients() {
  float pH_low = 4.0;
  float pH_neutral = 7.0;
  ph_slope = (pH_neutral - pH_low) / (cal_ph7_voltage - cal_ph4_voltage);
  ph_intercept = pH_neutral - ph_slope * cal_ph7_voltage;
  Serial.printf("[CALIB] pH: slope=%.3f, intercept=%.3f\n", ph_slope, ph_intercept);
}

void saveCalibration() {
  preferences.begin("calib", false);
  preferences.putFloat("ph7_v", cal_ph7_voltage);
  preferences.putFloat("ph4_v", cal_ph4_voltage);
  preferences.putFloat("ec_low_raw", calibration_low_raw);
  preferences.putFloat("ec_high_raw", calibration_high_raw);
  preferences.putFloat("ec_low_val", calibration_low_ec);
  preferences.putFloat("ec_high_val", calibration_high_ec);
  preferences.end();
  Serial.println("[INFO] Calibração salva");
}

void loadCalibration() {
  cal_ph7_voltage = preferences.getFloat("ph7_v", 2.52);
  cal_ph4_voltage = preferences.getFloat("ph4_v", 3.29);
  calibration_low_raw = preferences.getFloat("ec_low_raw", 645.0);
  calibration_high_raw = preferences.getFloat("ec_high_raw", 2850.0);
  calibration_low_ec = preferences.getFloat("ec_low_val", 360.0);
  calibration_high_ec = preferences.getFloat("ec_high_val", 4588.0);
  Serial.println("[INFO] Calibração carregada");
}

// ============================================================================
// INTERFACE OLED - ATUALIZAÇÃO
// ============================================================================
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  switch (currentPage) {
    case PAGE_DASHBOARD:
      display.println("=== DASHBOARD ===");
      display.printf("pH: %.2f\n", lastPhValue);
      display.printf("EC: %.0f uS/cm\n", lastEcValue);
      display.printf("Temp: %.1fC Hum: %.0f%%\n", temperature_C, humidity);
      display.printf("Water: %.1fC\n", water_temperature_C);
      display.println("----------------");
      display.println("[UP/DOWN] Nav");
      break;
      
    case PAGE_CONNECTIONS:
      display.println("=== CONEXOES ===");
      display.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "OK" : "OFF");
      if (WiFi.status() == WL_CONNECTED) {
        display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
      }
      display.printf("MQTT: %s\n", mqttConnected ? "OK" : "OFF");
      display.printf("BLE: %s\n", bleConnected ? "Connected" : "Ready");
      display.println("----------------");
      display.println("[SELECT] Scan WiFi");
      break;
      
    case PAGE_CALIBRATION:
      display.println("=== CALIBRACAO ===");
      if (calibrationMode == CAL_NONE) {
        display.println("1. pH 7.0");
        display.println("2. pH 4.0");
        display.println("3. EC Low");
        display.println("4. EC High");
        display.println("----------------");
        display.println("[SELECT] Escolher");
      } else {
        float currentVoltage = readAverageADC(PH_SENSOR_PIN) * 3.3 / 4095.0;
        float currentRaw = readAverageADC(TDS_SENSOR_PIN);
        
        switch (calibrationMode) {
          case CAL_PH_7:
            display.println("Calibrando pH 7.0");
            display.printf("V: %.3f\n", currentVoltage);
            display.println("\nMergulhe em pH 7");
            display.println("[SELECT] Confirmar");
            break;
          case CAL_PH_4:
            display.println("Calibrando pH 4.0");
            display.printf("V: %.3f\n", currentVoltage);
            display.println("\nMergulhe em pH 4");
            display.println("[SELECT] Confirmar");
            break;
          case CAL_EC_LOW:
            display.println("Calibrando EC Low");
            display.printf("Raw: %.0f\n", currentRaw);
            display.println("\nMergulhe em EC Low");
            display.println("[SELECT] Confirmar");
            break;
          case CAL_EC_HIGH:
            display.println("Calibrando EC High");
            display.printf("Raw: %.0f\n", currentRaw);
            display.println("\nMergulhe em EC High");
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
      display.printf("UUID: %s\n", deviceUUID.substring(0, 16).c_str());
      display.printf("Mem: %d KB\n", ESP.getFreeHeap() / 1024);
      display.printf("Uptime: %lus\n", millis() / 1000);
      display.println("----------------");
      display.println("Firmware v4.3.2");
      break;
      
    default:
      break;
  }
  
  display.display();
}

// ============================================================================
// INTERFACE OLED - BOTÕES
// ============================================================================
void handleButtons() {
  static int selectionIndex = 0;
  
  // Botão UP
  if (digitalRead(BUTTON_UP) == LOW && (millis() - lastDebounce[0] > debounceDelay)) {
    lastDebounce[0] = millis();
    
    if (currentPage == PAGE_CALIBRATION && calibrationMode == CAL_NONE) {
      selectionIndex = (selectionIndex - 1 + 4) % 4;
    } else {
      currentPage = (Page)((currentPage - 1 + PAGE_COUNT) % PAGE_COUNT);
    }
    Serial.printf("[BTN] UP - Page: %d\n", currentPage);
  }
  
  // Botão DOWN
  if (digitalRead(BUTTON_DOWN) == LOW && (millis() - lastDebounce[1] > debounceDelay)) {
    lastDebounce[1] = millis();
    
    if (currentPage == PAGE_CALIBRATION && calibrationMode == CAL_NONE) {
      selectionIndex = (selectionIndex + 1) % 4;
    } else {
      currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
    }
    Serial.printf("[BTN] DOWN - Page: %d\n", currentPage);
  }
  
  // Botão SELECT
  if (digitalRead(BUTTON_SELECT) == LOW && (millis() - lastDebounce[2] > debounceDelay)) {
    lastDebounce[2] = millis();
    Serial.println("[BTN] SELECT");
    
    if (currentPage == PAGE_CONNECTIONS) {
      handleScan();
    } else if (currentPage == PAGE_CALIBRATION) {
      if (calibrationMode == CAL_NONE) {
        // Entrar no modo de calibração
        calibrationMode = (CalibrationMode)(selectionIndex + 1);
        Serial.printf("[CALIB] Modo: %d\n", calibrationMode);
      } else {
        // Confirmar calibração
        float currentVoltage = readAverageADC(PH_SENSOR_PIN) * 3.3 / 4095.0;
        float currentRaw = readAverageADC(TDS_SENSOR_PIN);
        
        switch (calibrationMode) {
          case CAL_PH_7:
            cal_ph7_voltage = currentVoltage;
            Serial.printf("[CALIB] pH 7.0 = %.3fV\n", cal_ph7_voltage);
            break;
          case CAL_PH_4:
            cal_ph4_voltage = currentVoltage;
            Serial.printf("[CALIB] pH 4.0 = %.3fV\n", cal_ph4_voltage);
            break;
          case CAL_EC_LOW:
            calibration_low_raw = currentRaw;
            Serial.printf("[CALIB] EC Low = %.0f\n", calibration_low_raw);
            break;
          case CAL_EC_HIGH:
            calibration_high_raw = currentRaw;
            Serial.printf("[CALIB] EC High = %.0f\n", calibration_high_raw);
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
    Serial.println("[BTN] BACK");
    
    if (calibrationMode != CAL_NONE) {
      calibrationMode = CAL_NONE;
      selectionIndex = 0;
    }
  }
}

// ============================================================================
// MENSAGEM NO DISPLAY
// ============================================================================
void displayMessage(const char *message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}

// ============================================================================
// WIFI - CONEXÃO
// ============================================================================
void connectToWiFi() {
  if (ssid_sta.length() == 0) return;
  
  Serial.printf("[WiFi] Conectando a: %s\n", ssid_sta.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    resetWatchdog();
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] ✅ Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] ❌ Falha na conexão");
  }
}

void startAPMode() {
  Serial.println("[INFO] 🔶 Iniciando modo AP...");
  String apSSID = "AquaSys-" + deviceUUID;
  String apPassword = "aquasys2024";
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), apPassword.c_str());
  
  IPAddress IP = WiFi.softAPIP();
  Serial.printf("[INFO] ✅ AP ativo: %s / Senha: %s\n", apSSID.c_str(), apPassword.c_str());
  Serial.printf("[INFO] Portal: http://%s\n", IP.toString().c_str());
}

// ============================================================================
// SERVIDOR WEB
// ============================================================================
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<html><body><h1>AquaSys Config</h1>";
    html += "<p>Device: " + deviceUUID + "</p>";
    html += "<form action='/save' method='POST'>";
    html += "SSID: <input name='ssid' type='text'><br>";
    html += "Password: <input name='pass' type='password'><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form>";
    html += "<br><a href='/scan'>Scan WiFi Networks</a>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);
  
  server.begin();
  Serial.println("[INFO] Servidor web iniciado");
}

void handleScan() {
  Serial.println("[INFO] Escaneando redes WiFi...");
  displayMessage("Escaneando WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Escanear de forma assíncrona
  int n = WiFi.scanNetworks(false, false);
  
  // Aguardar conclusão do scan com reset do watchdog
  while (n == WIFI_SCAN_RUNNING) {
    delay(100);
    resetWatchdog();
    n = WiFi.scanComplete();
  }
  
  Serial.printf("[INFO] %d redes encontradas\n", n);
  
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN) + "}";
  }
  json += "]";
  
  // Enviar via BLE se conectado
  if (bleConnected && pCharWiFiList) {
    pCharWiFiList->setValue(json.c_str());
    pCharWiFiList->notify();
  }
  
  // Responder ao servidor web
  server.send(200, "application/json", json);
  
  WiFi.scanDelete();
  displayMessage("Scan concluido!");
  delay(1000);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    ssid_sta = server.arg("ssid");
    password_sta = server.arg("pass");
    
    preferences.begin("wifi", false);
    preferences.putString("ssid0", ssid_sta);
    preferences.putString("pass0", password_sta);
    preferences.end();
    
    server.send(200, "text/html", "<html><body><h1>Saved!</h1><p>Rebooting...</p></body></html>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "<html><body><h1>Error</h1><p>Missing parameters</p></body></html>");
  }
}

// ============================================================================
// MQTT
// ============================================================================
void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 5000) return;
  lastAttempt = millis();
  
  Serial.print("[MQTT] Conectando...");
  
  if (mqttClient.connect(deviceUUID.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println(" ✅");
    mqttConnected = true;
    
    String statusTopic = "aquasys/device/" + deviceUUID + "/status";
    mqttClient.publish(statusTopic.c_str(), "online", true);
  } else {
    Serial.printf(" ❌ (rc=%d)\n", mqttClient.state());
    mqttConnected = false;
  }
}

void publishData() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = millis();
  doc["ph"] = round(lastPhValue * 100) / 100.0;
  doc["ec"] = round(lastEcValue);
  doc["temperature"] = round(temperature_C * 10) / 10.0;
  doc["humidity"] = round(humidity * 10) / 10.0;
  doc["water_temperature"] = round(water_temperature_C * 10) / 10.0;
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  bool success = mqttClient.publish(MQTT_TOPIC, buffer);
  Serial.printf("[MQTT] Publicado: %s (%s)\n", success ? "✅" : "❌", buffer);
}
