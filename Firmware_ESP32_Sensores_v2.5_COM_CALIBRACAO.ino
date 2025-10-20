/*
  AquaSys Sensor Module v2.5 COM CALIBRAÇÃO
  - Display OLED SSD1306 128x64
  - 4 Botões de navegação (UP, DOWN, SELECT, BACK)
  - MENU DE CALIBRAÇÃO completo
  - Access Point automático para configuração WiFi
  - Portal web para configurar credenciais
  - MQTT estável HiveMQ Cloud (porta 8883)
  - Comandos MQTT para calibração remota
  - Watchdog seguro
  - Alinhado com app e módulo atuador
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

// ----------------------------- VERSÃO / DEVICE -----------------------------------------
#define FIRMWARE_VERSION "2.5-CALIBRACAO"
#define DEVICE_ID "SENSOR-MODULE-01"

// ----------------------------- PINOUT -----------------------------------------------
#define PH_SENSOR_PIN 34
#define EC_SENSOR_PIN 35
#define DHT_PIN 4
#define ONE_WIRE_BUS 2
#define DHT_TYPE DHT22

// Display OLED
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// Botões
#define BUTTON_UP 32
#define BUTTON_DOWN 33
#define BUTTON_SELECT 25
#define BUTTON_BACK 26

// ----------------------------- MQTT CONFIG ---------------------------------------
#define DEFAULT_MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define DEFAULT_MQTT_PORT 8883
#define DEFAULT_MQTT_USER "esp32-user"
#define DEFAULT_MQTT_PASS "HydroSmart123"

// ----------------------------- TIMEOUTS ----------------------------------
#define WDT_TIMEOUT 30
#define SENSOR_READ_INTERVAL 5000
#define MQTT_PUBLISH_INTERVAL 15000
#define HEARTBEAT_INTERVAL 30000
#define WIFI_RECONNECT_INTERVAL 30000
#define MQTT_RECONNECT_INTERVAL 15000
#define DISPLAY_UPDATE_INTERVAL 1000

// ----------------------------- LIMITES ----------------------------------------------
#define PH_MIN 0.0
#define PH_MAX 14.0
#define EC_MIN 0.0
#define EC_MAX 5000.0
#define TEMP_MIN -10.0
#define TEMP_MAX 60.0
#define HUMIDITY_MIN 0.0
#define HUMIDITY_MAX 100.0

// ----------------------------- ESTRUTURAS -------------------------------------------
struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  bool valid;
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

// ----------------------------- OBJETOS GLOBAIS -------------------------------------
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Preferences prefs;
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

// ----------------------------- VARIÁVEIS -------------------------------------------
SensorData currentData;
String ssid_sta = "";
String password_sta = "";
String mqtt_broker = DEFAULT_MQTT_BROKER;
int mqtt_port = DEFAULT_MQTT_PORT;
String mqtt_user = DEFAULT_MQTT_USER;
String mqtt_pass = DEFAULT_MQTT_PASS;
String mqtt_client_id;

bool wifiConnected = false;
bool mqttConnected = false;
bool wifiConfigured = false;
bool apMode = false;
static bool wdtEnabled = false;

unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastDebounce[4] = {0, 0, 0, 0};

Page currentPage = SHOW_PH;
CalibrationMode calibrationMode = CAL_NONE;
int calibrationMenuIndex = 0;
const unsigned long debounceDelay = 200;

// Calibração pH
float cal_ph7_voltage = 2.52f;
float cal_ph4_voltage = 3.29f;
float ph_slope = 0.0f;
float ph_intercept = 7.0f;

// Calibração EC
float cal_ec_low_raw = 645.0f;
float cal_ec_high_raw = 2850.0f;
float cal_ec_low = 360.0f;     // Valor EC da solução de referência baixa
float cal_ec_high = 4588.0f;   // Valor EC da solução de referência alta
float temp_ec_low_value = 360.0f;
float temp_ec_high_value = 4588.0f;

// ----------------------------- PROTÓTIPOS -------------------------------------------
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
void setupMQTT();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
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
String generateClientID();

// ----------------------------- UTIL -------------------------------------------------
void logInfo(const char* msg) {
  Serial.print("[INFO] ");
  Serial.println(msg);
}

void logError(const char* msg) {
  Serial.print("[ERROR] ");
  Serial.println(msg);
}

bool isValidValue(float value, float min, float max) {
  return !isnan(value) && !isinf(value) && value >= min && value <= max;
}

String generateClientID() {
  uint64_t mac = ESP.getEfuseMac();
  char id[32];
  snprintf(id, sizeof(id), "aquasys-sensor-%04X%08X", 
           (uint16_t)(mac >> 32), (uint32_t)mac);
  return String(id);
}

// ----------------------------- WATCHDOG --------------------------------------------
void initWatchdog() {
  esp_task_wdt_config_t config;
  memset(&config, 0, sizeof(config));
  config.timeout_ms = WDT_TIMEOUT * 1000;
  config.idle_core_mask = 0;
  config.trigger_panic = true;
  
  esp_err_t err = esp_task_wdt_init(&config);
  if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
    if (esp_task_wdt_add(NULL) == ESP_OK) {
      wdtEnabled = true;
      logInfo("Watchdog inicializado");
    }
  }
}

void resetWatchdog() {
  if (wdtEnabled) esp_task_wdt_reset();
}

// ----------------------------- CONFIG & CALIBRAÇÃO ----------------------------------
void loadConfig() {
  prefs.begin("config", true);
  ssid_sta = prefs.getString("ssid", "");
  password_sta = prefs.getString("pass", "");
  mqtt_broker = prefs.getString("mqtt_broker", DEFAULT_MQTT_BROKER);
  mqtt_port = prefs.getInt("mqtt_port", DEFAULT_MQTT_PORT);
  mqtt_user = prefs.getString("mqtt_user", DEFAULT_MQTT_USER);
  mqtt_pass = prefs.getString("mqtt_pass", DEFAULT_MQTT_PASS);
  
  mqtt_client_id = prefs.getString("mqtt_client_id", "");
  if (mqtt_client_id.length() == 0) {
    mqtt_client_id = generateClientID();
    prefs.end();
    prefs.begin("config", false);
    prefs.putString("mqtt_client_id", mqtt_client_id);
    prefs.end();
    prefs.begin("config", true);
  }
  prefs.end();
  
  // Carregar calibração
  prefs.begin("calib", true);
  cal_ph7_voltage = prefs.getFloat("ph7v", 2.52f);
  cal_ph4_voltage = prefs.getFloat("ph4v", 3.29f);
  cal_ec_low_raw = prefs.getFloat("ec_low_raw", 645.0f);
  cal_ec_high_raw = prefs.getFloat("ec_high_raw", 2850.0f);
  cal_ec_low = prefs.getFloat("ec_low", 360.0f);
  cal_ec_high = prefs.getFloat("ec_high", 4588.0f);
  prefs.end();
  
  calculatePHCoefficients();
  
  Serial.printf("[INFO] Client ID: %s\n", mqtt_client_id.c_str());
  Serial.printf("[INFO] Calibração pH: 7=%.3fV, 4=%.3fV\n", cal_ph7_voltage, cal_ph4_voltage);
  Serial.printf("[INFO] Calibração EC: Low=%.0f@%.0f, High=%.0f@%.0f\n", 
                cal_ec_low, cal_ec_low_raw, cal_ec_high, cal_ec_high_raw);
}

void saveWiFiConfig(const String& ssid, const String& pass) {
  prefs.begin("config", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  logInfo("WiFi configurado e salvo!");
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
  logInfo("Calibração salva!");
  
  // Publicar dados de calibração via MQTT
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

// ----------------------------- WIFI --------------------------------------------------
void connectWiFi() {
  if (ssid_sta.length() == 0) {
    logError("Sem credenciais WiFi!");
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
    
    // Sincronizar NTP
    configTime(0, 0, "pool.ntp.org");
  } else {
    wifiConnected = false;
    wifiConfigured = false;
    logError("Falha ao conectar WiFi");
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
    logError("WiFi desconectado. Reconectando...");
    WiFi.disconnect();
    delay(100);
    WiFi.reconnect();
  } else if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
  }
}

void startAPMode() {
  logInfo("Iniciando Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("AquaSys-AP");
  apMode = true;
  
  IPAddress IP = WiFi.softAPIP();
  Serial.printf("[INFO] AP IP: %s\n", IP.toString().c_str());
  
  String msg = "Modo AP\nRede: AquaSys-AP\nIP: " + IP.toString();
  displayMessage(msg.c_str());
  
  setupWebServer();
  server.begin();
  logInfo("Servidor web iniciado no modo AP");
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>AquaSys Config</title>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;}";
    html += "h1{color:#2c3e50;}form{background:white;padding:20px;border-radius:8px;}";
    html += "input{width:100%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:4px;}";
    html += "button{background:#3498db;color:white;padding:12px 30px;border:none;border-radius:4px;cursor:pointer;}";
    html += "button:hover{background:#2980b9;}</style></head><body>";
    html += "<h1>🌊 AquaSys - Configuração WiFi</h1>";
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

// ----------------------------- MQTT -------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[512];
  unsigned int len = min(length, (unsigned int)511);
  memcpy(msg, payload, len);
  msg[len] = '\0';
  
  Serial.printf("[MQTT] %s: %s\n", topic, msg);
  
  String topicStr = String(topic);
  if (topicStr == "aquasys/sensors/command") {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (!error) {
      if (doc.containsKey("calibrate")) {
        String calType = doc["calibrate"].as<String>();
        
        // Calibração pH
        if (calType == "ph7") {
          float voltage = readAverageADC(PH_SENSOR_PIN, 10) * (3.3f / 4095.0f);
          cal_ph7_voltage = voltage;
          saveCalibration();
          Serial.printf("[CAL] pH 7.0 calibrado: %.3fV\n", voltage);
        }
        else if (calType == "ph4") {
          float voltage = readAverageADC(PH_SENSOR_PIN, 10) * (3.3f / 4095.0f);
          cal_ph4_voltage = voltage;
          saveCalibration();
          Serial.printf("[CAL] pH 4.0 calibrado: %.3fV\n", voltage);
        }
        // Calibração EC
        else if (calType == "ec_low") {
          float raw = readAverageADC(EC_SENSOR_PIN, 10);
          cal_ec_low_raw = raw;
          if (doc.containsKey("ec_value")) {
            cal_ec_low = doc["ec_value"].as<float>();
          }
          saveCalibration();
          Serial.printf("[CAL] EC baixa calibrada: %.0f @ %.0f uS\n", raw, cal_ec_low);
        }
        else if (calType == "ec_high") {
          float raw = readAverageADC(EC_SENSOR_PIN, 10);
          cal_ec_high_raw = raw;
          if (doc.containsKey("ec_value")) {
            cal_ec_high = doc["ec_value"].as<float>();
          }
          saveCalibration();
          Serial.printf("[CAL] EC alta calibrada: %.0f @ %.0f uS\n", raw, cal_ec_high);
        }
      }
    }
  }
}

void setupMQTT() {
  espClient.setInsecure();
  mqttClient.setServer(mqtt_broker.c_str(), mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setBufferSize(1024);
  logInfo("MQTT configurado (porta 8883)");
}

bool reconnectMQTT() {
  if (mqttClient.connected()) {
    mqttConnected = true;
    return true;
  }
  
  if (!wifiConnected) {
    mqttConnected = false;
    return false;
  }
  
  unsigned long now = millis();
  if (now - lastMqttAttempt < MQTT_RECONNECT_INTERVAL) return false;
  lastMqttAttempt = now;
  
  Serial.printf("[INFO] Conectando MQTT como %s...", mqtt_client_id.c_str());
  
  bool connected = false;
  if (mqtt_user.length() > 0) {
    connected = mqttClient.connect(mqtt_client_id.c_str(), 
                                   mqtt_user.c_str(), 
                                   mqtt_pass.c_str());
  } else {
    connected = mqttClient.connect(mqtt_client_id.c_str());
  }
  
  if (connected) {
    mqttConnected = true;
    Serial.println(" Conectado!");
    
    mqttClient.subscribe("aquasys/sensors/command");
    publishHeartbeat();
    
    return true;
  } else {
    mqttConnected = false;
    int state = mqttClient.state();
    Serial.printf(" Falhou! Estado: %d\n", state);
    return false;
  }
}

// ----------------------------- SENSORES ------------------------------------------------
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
  float ph = ph_slope * voltage + ph_intercept;
  return ph;
}

float readEcSensor(float waterTemp) {
  float raw = readAverageADC(EC_SENSOR_PIN, 10);
  
  float denom = cal_ec_high_raw - cal_ec_low_raw;
  if (denom == 0) denom = 1.0f;
  float slope = (cal_ec_high - cal_ec_low) / denom;
  float ec = cal_ec_low + slope * (raw - cal_ec_low_raw);
  
  if (ec < 0) ec = 0;
  
  // Compensação de temperatura
  if (isValidValue(waterTemp, TEMP_MIN, TEMP_MAX)) {
    ec = ec / (1.0f + 0.02f * (waterTemp - 25.0f));
  }
  
  return ec;
}

void readSensors() {
  resetWatchdog();
  
  // Temperatura da água (DS18B20)
  ds18b20.requestTemperatures();
  currentData.waterTemp = ds18b20.getTempCByIndex(0);
  if (currentData.waterTemp == DEVICE_DISCONNECTED_C) {
    currentData.waterTemp = NAN;
  }
  
  // Temperatura e umidade do ar (DHT22)
  currentData.airTemp = dht.readTemperature();
  currentData.humidity = dht.readHumidity();
  
  // pH
  currentData.ph = readPhSensor();
  
  // EC
  currentData.ec = readEcSensor(currentData.waterTemp);
  
  // Validar dados
  bool phValid = isValidValue(currentData.ph, PH_MIN, PH_MAX);
  bool ecValid = isValidValue(currentData.ec, EC_MIN, EC_MAX);
  bool airTempValid = isValidValue(currentData.airTemp, TEMP_MIN, TEMP_MAX);
  bool humidityValid = isValidValue(currentData.humidity, HUMIDITY_MIN, HUMIDITY_MAX);
  bool waterTempValid = isValidValue(currentData.waterTemp, TEMP_MIN, TEMP_MAX);
  
  currentData.valid = phValid && ecValid && airTempValid && humidityValid && waterTempValid;
  
  if (currentData.valid) {
    Serial.printf("[SENSOR] pH=%.2f EC=%.0f T.Ar=%.1f°C Umid=%.1f%% T.Agua=%.1f°C\n",
                  currentData.ph, currentData.ec, currentData.airTemp, 
                  currentData.humidity, currentData.waterTemp);
  }
}

// ----------------------------- PUBLICAÇÃO ------------------------------------------
void publishSensorData() {
  if (!mqttConnected || !currentData.valid) return;
  
  StaticJsonDocument<512> doc;
  doc["device_id"] = DEVICE_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["timestamp"] = millis();
  doc["ph"] = serialized(String(currentData.ph, 2));
  doc["ec"] = serialized(String(currentData.ec, 0));
  doc["temperature"] = serialized(String(currentData.airTemp, 1));
  doc["humidity"] = serialized(String(currentData.humidity, 1));
  doc["waterTemp"] = serialized(String(currentData.waterTemp, 1));
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish("aquasys/sensors/all", payload.c_str())) {
    Serial.println("[MQTT] Dados dos sensores publicados");
  } else {
    Serial.println("[ERROR] Falha ao publicar dados dos sensores");
  }
}

void publishHeartbeat() {
  if (!mqttConnected) return;
  
  StaticJsonDocument<256> doc;
  doc["device"] = "ESP32_Sensor_v2.5";
  doc["uptime"] = millis() / 1000;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish("aquasys/heartbeat", payload.c_str())) {
    Serial.println("[MQTT] Heartbeat enviado");
  }
}

void publishCalibrationData() {
  if (!mqttConnected) return;
  
  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["cal_ph7_voltage"] = cal_ph7_voltage;
  doc["cal_ph4_voltage"] = cal_ph4_voltage;
  doc["cal_ec_low_raw"] = cal_ec_low_raw;
  doc["cal_ec_high_raw"] = cal_ec_high_raw;
  doc["cal_ec_low"] = cal_ec_low;
  doc["cal_ec_high"] = cal_ec_high;
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish("aquasys/sensors/calibration", payload.c_str())) {
    Serial.println("[MQTT] Dados de calibração publicados");
  }
}

// ----------------------------- DISPLAY & BOTÕES ------------------------------------
void displayMessage(const char* message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}

void displayCalibrationMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  display.setCursor(0, 0);
  display.println("=== CALIBRACAO ===");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  const char* menuItems[] = {
    "1. pH 7.0",
    "2. pH 4.0",
    "3. EC Baixa",
    "4. EC Alta",
    "5. Sair"
  };
  
  for (int i = 0; i < 5; i++) {
    display.setCursor(0, 15 + i * 10);
    if (i == calibrationMenuIndex) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.println(menuItems[i]);
  }
  
  display.display();
}

void updateDisplay() {
  unsigned long now = millis();
  if (now - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) return;
  lastDisplayUpdate = now;
  
  if (calibrationMode == CAL_MENU) {
    displayCalibrationMenu();
    return;
  }
  
  if (calibrationMode != CAL_NONE) {
    // Mostrar tela de calibração
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    
    display.setCursor(0, 0);
    display.println("CALIBRANDO...");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    switch (calibrationMode) {
      case CAL_PH_7:
        display.setCursor(0, 15);
        display.println("pH 7.0");
        display.setCursor(0, 30);
        display.print("V: ");
        display.print(readAverageADC(PH_SENSOR_PIN, 10) * 3.3 / 4095.0, 3);
        display.setCursor(0, 50);
        display.println("SELECT=Confirma");
        break;
        
      case CAL_PH_4:
        display.setCursor(0, 15);
        display.println("pH 4.0");
        display.setCursor(0, 30);
        display.print("V: ");
        display.print(readAverageADC(PH_SENSOR_PIN, 10) * 3.3 / 4095.0, 3);
        display.setCursor(0, 50);
        display.println("SELECT=Confirma");
        break;
        
      case CAL_EC_LOW_VALUE:
        display.setCursor(0, 15);
        display.println("Valor EC Baixa:");
        display.setTextSize(2);
        display.setCursor(0, 30);
        display.print(temp_ec_low_value, 0);
        display.print(" uS");
        display.setTextSize(1);
        display.setCursor(0, 50);
        display.println("UP/DN SEL=OK");
        break;
        
      case CAL_EC_LOW:
        display.setCursor(0, 15);
        display.println("EC Baixa");
        display.setCursor(0, 30);
        display.print("Raw: ");
        display.print(readAverageADC(EC_SENSOR_PIN, 10), 0);
        display.setCursor(0, 40);
        display.print("Ref: ");
        display.print(cal_ec_low, 0);
        display.print(" uS");
        display.setCursor(0, 50);
        display.println("SELECT=Confirma");
        break;
        
      case CAL_EC_HIGH_VALUE:
        display.setCursor(0, 15);
        display.println("Valor EC Alta:");
        display.setTextSize(2);
        display.setCursor(0, 30);
        display.print(temp_ec_high_value, 0);
        display.print(" uS");
        display.setTextSize(1);
        display.setCursor(0, 50);
        display.println("UP/DN SEL=OK");
        break;
        
      case CAL_EC_HIGH:
        display.setCursor(0, 15);
        display.println("EC Alta");
        display.setCursor(0, 30);
        display.print("Raw: ");
        display.print(readAverageADC(EC_SENSOR_PIN, 10), 0);
        display.setCursor(0, 40);
        display.print("Ref: ");
        display.print(cal_ec_high, 0);
        display.print(" uS");
        display.setCursor(0, 50);
        display.println("SELECT=Confirma");
        break;
        
      default:
        break;
    }
    
    display.display();
    return;
  }
  
  // Display normal de sensores
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  // Mostrar status de conexão no topo
  if (apMode) {
    display.print("AP: AquaSys-AP");
  } else if (wifiConnected) {
    display.print("WiFi: ");
    display.print(mqttConnected ? "MQTT OK" : "MQTT X");
  } else {
    display.print("WiFi: Desconectado");
  }
  
  display.setCursor(100, 0);
  display.print(String((int)currentPage + 1) + "/5");
  
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  switch (currentPage) {
    case SHOW_PH:
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.println("pH Value");
      display.setTextSize(2);
      display.setCursor(0, 30);
      display.print(currentData.ph, 2);
      display.setTextSize(1);
      display.setCursor(0, 55);
      display.print("V: ");
      display.print((readAverageADC(PH_SENSOR_PIN, 10) * 3.3 / 4095.0), 3);
      break;
      
    case SHOW_EC:
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.println("EC/TDS Value");
      display.setTextSize(2);
      display.setCursor(0, 30);
      display.print(currentData.ec, 0);
      display.setTextSize(1);
      display.print(" uS");
      display.setCursor(0, 55);
      display.print("Raw: ");
      display.print(readAverageADC(EC_SENSOR_PIN, 10), 0);
      break;
      
    case SHOW_AIR_TEMP:
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.println("Air Temp & Humidity");
      display.setTextSize(2);
      display.setCursor(0, 30);
      display.print(currentData.airTemp, 1);
      display.print(" C");
      display.setTextSize(1);
      display.setCursor(0, 55);
      display.print("Hum: ");
      display.print(currentData.humidity, 1);
      display.print("%");
      break;
      
    case SHOW_WATER_TEMP:
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.println("Water Temperature");
      display.setTextSize(2);
      display.setCursor(0, 30);
      if (isValidValue(currentData.waterTemp, TEMP_MIN, TEMP_MAX)) {
        display.print(currentData.waterTemp, 1);
        display.print(" C");
      } else {
        display.print("ERRO");
      }
      display.setTextSize(1);
      display.setCursor(0, 55);
      display.print("DS18B20 ");
      display.print(isValidValue(currentData.waterTemp, TEMP_MIN, TEMP_MAX) ? "OK" : "FALHA");
      break;
      
    case SHOW_HUMIDITY:
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.println("Humidity");
      display.setTextSize(2);
      display.setCursor(0, 30);
      display.print(currentData.humidity, 1);
      display.print(" %");
      display.setTextSize(1);
      display.setCursor(0, 55);
      display.print("AirT: ");
      display.print(currentData.airTemp, 1);
      display.print(" C");
      break;
  }
  
  display.display();
}

void enterCalibrationMode() {
  calibrationMode = CAL_MENU;
  calibrationMenuIndex = 0;
  Serial.println("[CAL] Entrando no menu de calibração");
}

void exitCalibrationMode() {
  calibrationMode = CAL_NONE;
  Serial.println("[CAL] Saindo do menu de calibração");
}

void performCalibrationStep() {
  switch (calibrationMode) {
    case CAL_PH_7: {
      float voltage = readAverageADC(PH_SENSOR_PIN, 10) * (3.3f / 4095.0f);
      cal_ph7_voltage = voltage;
      saveCalibration();
      displayMessage("pH 7.0\nCalibrado!");
      delay(2000);
      exitCalibrationMode();
      break;
    }
    
    case CAL_PH_4: {
      float voltage = readAverageADC(PH_SENSOR_PIN, 10) * (3.3f / 4095.0f);
      cal_ph4_voltage = voltage;
      saveCalibration();
      displayMessage("pH 4.0\nCalibrado!");
      delay(2000);
      exitCalibrationMode();
      break;
    }
    
    case CAL_EC_LOW_VALUE:
      cal_ec_low = temp_ec_low_value;
      calibrationMode = CAL_EC_LOW;
      break;
    
    case CAL_EC_LOW: {
      float raw = readAverageADC(EC_SENSOR_PIN, 10);
      cal_ec_low_raw = raw;
      saveCalibration();
      displayMessage("EC Baixa\nCalibrada!");
      delay(2000);
      exitCalibrationMode();
      break;
    }
    
    case CAL_EC_HIGH_VALUE:
      cal_ec_high = temp_ec_high_value;
      calibrationMode = CAL_EC_HIGH;
      break;
    
    case CAL_EC_HIGH: {
      float raw = readAverageADC(EC_SENSOR_PIN, 10);
      cal_ec_high_raw = raw;
      saveCalibration();
      displayMessage("EC Alta\nCalibrada!");
      delay(2000);
      exitCalibrationMode();
      break;
    }
    
    default:
      break;
  }
}

void handleCalibrationButtons() {
  unsigned long now = millis();
  
  if (calibrationMode == CAL_MENU) {
    // Navegação no menu
    if (digitalRead(BUTTON_UP) == LOW && (now - lastDebounce[0]) > debounceDelay) {
      lastDebounce[0] = now;
      calibrationMenuIndex = (calibrationMenuIndex + 4) % 5;
      Serial.printf("[BTN] UP - Menu item %d\n", calibrationMenuIndex);
    }
    
    if (digitalRead(BUTTON_DOWN) == LOW && (now - lastDebounce[1]) > debounceDelay) {
      lastDebounce[1] = now;
      calibrationMenuIndex = (calibrationMenuIndex + 1) % 5;
      Serial.printf("[BTN] DOWN - Menu item %d\n", calibrationMenuIndex);
    }
    
    if (digitalRead(BUTTON_SELECT) == LOW && (now - lastDebounce[2]) > debounceDelay) {
      lastDebounce[2] = now;
      
      switch (calibrationMenuIndex) {
        case 0: // pH 7.0
          calibrationMode = CAL_PH_7;
          Serial.println("[CAL] Calibrando pH 7.0");
          break;
        case 1: // pH 4.0
          calibrationMode = CAL_PH_4;
          Serial.println("[CAL] Calibrando pH 4.0");
          break;
        case 2: // EC Baixa
          temp_ec_low_value = cal_ec_low;
          calibrationMode = CAL_EC_LOW_VALUE;
          Serial.println("[CAL] Definindo valor EC baixa");
          break;
        case 3: // EC Alta
          temp_ec_high_value = cal_ec_high;
          calibrationMode = CAL_EC_HIGH_VALUE;
          Serial.println("[CAL] Definindo valor EC alta");
          break;
        case 4: // Sair
          exitCalibrationMode();
          break;
      }
    }
    
    if (digitalRead(BUTTON_BACK) == LOW && (now - lastDebounce[3]) > debounceDelay) {
      lastDebounce[3] = now;
      exitCalibrationMode();
    }
  }
  else if (calibrationMode == CAL_EC_LOW_VALUE || calibrationMode == CAL_EC_HIGH_VALUE) {
    // Ajuste de valor de referência EC
    bool isLow = (calibrationMode == CAL_EC_LOW_VALUE);
    float& tempValue = isLow ? temp_ec_low_value : temp_ec_high_value;
    
    if (digitalRead(BUTTON_UP) == LOW && (now - lastDebounce[0]) > debounceDelay) {
      lastDebounce[0] = now;
      tempValue += 50.0f;
      if (tempValue > EC_MAX) tempValue = EC_MAX;
      Serial.printf("[CAL] Valor EC: %.0f\n", tempValue);
    }
    
    if (digitalRead(BUTTON_DOWN) == LOW && (now - lastDebounce[1]) > debounceDelay) {
      lastDebounce[1] = now;
      tempValue -= 50.0f;
      if (tempValue < EC_MIN) tempValue = EC_MIN;
      Serial.printf("[CAL] Valor EC: %.0f\n", tempValue);
    }
    
    if (digitalRead(BUTTON_SELECT) == LOW && (now - lastDebounce[2]) > debounceDelay) {
      lastDebounce[2] = now;
      performCalibrationStep();
    }
    
    if (digitalRead(BUTTON_BACK) == LOW && (now - lastDebounce[3]) > debounceDelay) {
      lastDebounce[3] = now;
      calibrationMode = CAL_MENU;
    }
  }
  else if (calibrationMode != CAL_NONE) {
    // Em processo de calibração
    if (digitalRead(BUTTON_SELECT) == LOW && (now - lastDebounce[2]) > debounceDelay) {
      lastDebounce[2] = now;
      performCalibrationStep();
    }
    
    if (digitalRead(BUTTON_BACK) == LOW && (now - lastDebounce[3]) > debounceDelay) {
      lastDebounce[3] = now;
      calibrationMode = CAL_MENU;
    }
  }
}

void handleButtons() {
  unsigned long now = millis();
  
  // Se estiver em modo de calibração, tratar botões específicos
  if (calibrationMode != CAL_NONE) {
    handleCalibrationButtons();
    return;
  }
  
  // Botão UP - próxima página
  if (digitalRead(BUTTON_UP) == LOW && (now - lastDebounce[0]) > debounceDelay) {
    lastDebounce[0] = now;
    currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
    Serial.printf("[BTN] UP - Página %d\n", currentPage + 1);
    updateDisplay();
  }
  
  // Botão DOWN - página anterior
  if (digitalRead(BUTTON_DOWN) == LOW && (now - lastDebounce[1]) > debounceDelay) {
    lastDebounce[1] = now;
    currentPage = (Page)((currentPage + PAGE_COUNT - 1) % PAGE_COUNT);
    Serial.printf("[BTN] DOWN - Página %d\n", currentPage + 1);
    updateDisplay();
  }
  
  // Botão SELECT - entrar no menu de calibração
  if (digitalRead(BUTTON_SELECT) == LOW && (now - lastDebounce[2]) > debounceDelay) {
    lastDebounce[2] = now;
    Serial.println("[BTN] SELECT - Abrindo menu de calibração");
    enterCalibrationMode();
  }
  
  // Botão BACK - reservado
  if (digitalRead(BUTTON_BACK) == LOW && (now - lastDebounce[3]) > debounceDelay) {
    lastDebounce[3] = now;
    Serial.println("[BTN] BACK");
  }
}

// ----------------------------- SETUP / LOOP ----------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println();
  Serial.println("[INFO] ═══════════════════════════════════════");
  Serial.println("[INFO] AquaSys Sensor v2.5 COM CALIBRAÇÃO");
  Serial.println("[INFO] ═══════════════════════════════════════");
  
  // Configurar ADC
  analogReadResolution(12);
  
  // Configurar botões
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);
  
  // Inicializar display OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERROR] Falha ao inicializar display OLED!");
    while (true) delay(1000);
  }
  display.clearDisplay();
  displayMessage("AquaSys v2.5\nInicializando...");
  delay(1000);
  
  // Inicializar watchdog
  initWatchdog();
  
  // Inicializar sensores
  dht.begin();
  ds18b20.begin();
  
  currentData.valid = false;
  
  // Carregar configuração
  loadConfig();
  
  // Conectar WiFi
  if (ssid_sta.length() > 0) {
    displayMessage("Conectando WiFi...");
    connectWiFi();
  } else {
    displayMessage("Sem config WiFi\nIniciando AP...");
    delay(2000);
    startAPMode();
  }
  
  // Configurar MQTT se WiFi conectado
  if (wifiConnected) {
    setupMQTT();
  }
  
  logInfo("Sistema inicializado - v2.5 COM CALIBRAÇÃO");
  displayMessage("Sistema OK!\nSELECT = Menu CAL");
  delay(2000);
}

void loop() {
  resetWatchdog();
  
  unsigned long now = millis();
  
  // Modo AP - atender requisições web
  if (apMode) {
    server.handleClient();
  }
  
  // Verificar WiFi
  if (!apMode) {
    checkWiFi();
  }
  
  // Tentar reconectar MQTT
  if (wifiConnected && !mqttConnected && !apMode) {
    reconnectMQTT();
  }
  
  // Loop MQTT
  if (mqttConnected) {
    mqttClient.loop();
  }
  
  // Ler sensores
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }
  
  // Publicar dados dos sensores
  if (now - lastMqttPublish >= MQTT_PUBLISH_INTERVAL && wifiConnected) {
    lastMqttPublish = now;
    publishSensorData();
  }
  
  // Publicar heartbeat
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL && wifiConnected) {
    lastHeartbeat = now;
    publishHeartbeat();
  }
  
  // Atualizar display
  updateDisplay();
  
  // Verificar botões
  handleButtons();
  
  delay(100);
}
