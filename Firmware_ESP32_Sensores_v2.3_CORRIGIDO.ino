/*
  AquaSys Sensor Module v2.3 CORRIGIDO
  - Compatível com HiveMQ Cloud WSS
  - MQTT estável na porta 8883
  - Client ID único e correto
  - Watchdog seguro
  - Alinhado com app e módulo atuador v4.1
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

// ----------------------------- VERSÃO / DEVICE -----------------------------------------
#define FIRMWARE_VERSION "2.3-CORRIGIDO"
#define DEVICE_ID "SENSOR-MODULE-01"

// ----------------------------- PINOUT -----------------------------------------------
#define PH_SENSOR_PIN 34
#define EC_SENSOR_PIN 35
#define DHT_PIN 4
#define ONE_WIRE_BUS 2
#define DHT_TYPE DHT22

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

// ----------------------------- OBJETOS GLOBAIS -------------------------------------
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Preferences prefs;
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

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
static bool wdtEnabled = false;

unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastMqttAttempt = 0;

// Calibração pH
float cal_ph7_voltage = 2.52f;
float cal_ph4_voltage = 3.29f;
float ph_slope = 0.0f;
float ph_intercept = 7.0f;

// Calibração EC
float cal_ec_low_raw = 645.0f;
float cal_ec_high_raw = 2850.0f;
float cal_ec_low = 360.0f;
float cal_ec_high = 4588.0f;

// ----------------------------- PROTÓTIPOS -------------------------------------------
void loadConfig();
void saveWiFiConfig(const String& ssid, const String& pass);
void calculatePHCoefficients();
void initWatchdog();
void resetWatchdog();
void connectWiFi();
void checkWiFi();
void setupMQTT();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void readSensors();
void publishSensorData();
void publishHeartbeat();
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
  
  // Gerar Client ID único se não existir
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
  Serial.printf("[INFO] MQTT: %s:%d\n", mqtt_broker.c_str(), mqtt_port);
}

void saveWiFiConfig(const String& ssid, const String& pass) {
  prefs.begin("config", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
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
    Serial.printf("[INFO] WiFi conectado! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[INFO] RSSI: %d dBm\n", WiFi.RSSI());
    
    // Sincronizar NTP
    configTime(0, 0, "pool.ntp.org");
  } else {
    wifiConnected = false;
    logError("Falha ao conectar WiFi");
  }
}

void checkWiFi() {
  unsigned long now = millis();
  if (now - lastWifiCheck < WIFI_RECONNECT_INTERVAL) return;
  lastWifiCheck = now;
  
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    logError("WiFi desconectado. Reconectando...");
    WiFi.disconnect();
    delay(100);
    WiFi.reconnect();
  } else {
    wifiConnected = true;
  }
}

// ----------------------------- MQTT -------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[256];
  unsigned int len = min(length, (unsigned int)255);
  memcpy(msg, payload, len);
  msg[len] = '\0';
  
  Serial.printf("[MQTT] %s: %s\n", topic, msg);
  
  // Processar comandos se necessário (calibração, etc)
  String topicStr = String(topic);
  if (topicStr == "aquasys/sensors/command") {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (!error) {
      if (doc.containsKey("calibrate")) {
        // Implementar calibração via MQTT se necessário
      }
    }
  }
}

void setupMQTT() {
  espClient.setInsecure(); // HiveMQ Cloud requer TLS mas sem validação de certificado
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
    
    // Subscrever tópicos de comando
    mqttClient.subscribe("aquasys/sensors/command");
    
    // Publicar status inicial
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
  
  // Calcular EC a partir da calibração
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
  } else {
    Serial.println("[WARN] Dados dos sensores inválidos");
    if (!phValid) Serial.println("  - pH inválido");
    if (!ecValid) Serial.println("  - EC inválido");
    if (!airTempValid) Serial.println("  - Temperatura do ar inválida");
    if (!humidityValid) Serial.println("  - Umidade inválida");
    if (!waterTempValid) Serial.println("  - Temperatura da água inválida");
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
  doc["device"] = "ESP32_Sensor_v2.3";
  doc["uptime"] = millis() / 1000;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish("aquasys/heartbeat", payload.c_str())) {
    Serial.println("[MQTT] Heartbeat enviado");
  }
}

// ----------------------------- SETUP / LOOP ----------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println();
  Serial.println("[INFO] ═══════════════════════════════════════");
  Serial.println("[INFO] AquaSys Sensor v2.3 CORRIGIDO");
  Serial.println("[INFO] ═══════════════════════════════════════");
  
  // Configurar ADC
  analogReadResolution(12);
  
  // Inicializar watchdog
  initWatchdog();
  
  // Inicializar sensores
  dht.begin();
  ds18b20.begin();
  
  currentData.valid = false;
  
  // Carregar configuração
  loadConfig();
  
  // Conectar WiFi
  connectWiFi();
  
  // Configurar MQTT
  setupMQTT();
  
  logInfo("Sistema inicializado - v2.3 CORRIGIDO");
}

void loop() {
  resetWatchdog();
  
  unsigned long now = millis();
  
  // Verificar WiFi
  checkWiFi();
  
  // Tentar reconectar MQTT
  if (wifiConnected && !mqttConnected) {
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
  if (now - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMqttPublish = now;
    publishSensorData();
  }
  
  // Publicar heartbeat
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    publishHeartbeat();
  }
  
  delay(100); // Delay para estabilidade do watchdog
}
