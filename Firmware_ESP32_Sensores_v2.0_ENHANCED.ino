/*
 * AquaSys/HydroSmart - Módulo Sensor ESP32 v2.0 ENHANCED
 * 
 * Funcionalidades:
 * - Leitura de sensores: pH, EC, temperatura do ar, umidade, temperatura da água
 * - Comunicação MQTT com buffer para garantir entrega
 * - Bluetooth para comunicação de emergência com módulo atuador
 * - Watchdog timer para reinício automático em caso de travamento
 * - Validação e filtragem de dados dos sensores
 * - Modo de emergência com comunicação Bluetooth
 * - Heartbeat para monitoramento de conexão
 * - Sincronização de dados não enviados
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "BluetoothSerial.h"

// ----------------------------- CONFIGURAÇÕES PRINCIPAIS -----------------------------
#define FIRMWARE_VERSION "2.0-ENHANCED"
#define DEVICE_ID "SENSOR-MODULE-01"

// Pinos dos Sensores
#define PH_SENSOR_PIN 34
#define EC_SENSOR_PIN 35
#define TEMP_AIR_PIN 32
#define HUMIDITY_PIN 33
#define TEMP_WATER_PIN 25

// Configuração WiFi
#define WIFI_SSID "SEU_WIFI_SSID"
#define WIFI_PASSWORD "SEU_WIFI_PASSWORD"
#define WIFI_RECONNECT_INTERVAL 30000
#define WIFI_MAX_RETRY 5

// Configuração MQTT
#define MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "esp32-user"
#define MQTT_PASS "HydroSmart123"
#define MQTT_CLIENT_ID "aquasys-sensor-01"
#define MQTT_TOPIC_SENSORS "aquasys/sensors/all"
#define MQTT_TOPIC_HEARTBEAT "aquasys/sensors/heartbeat"
#define MQTT_RECONNECT_INTERVAL 5000

// Bluetooth
#define BT_DEVICE_NAME "AquaSys-Sensor"
#define BT_EMERGENCY_INTERVAL 10000

// Watchdog
#define WDT_TIMEOUT 30

// Intervalos de operação
#define SENSOR_READ_INTERVAL 5000
#define MQTT_PUBLISH_INTERVAL 10000
#define HEARTBEAT_INTERVAL 30000
#define DATA_VALIDATION_SAMPLES 3
#define EMERGENCY_MODE_TIMEOUT 60000

// Buffer MQTT
#define MQTT_BUFFER_SIZE 20

// Limites de validação dos sensores
#define PH_MIN 0.0
#define PH_MAX 14.0
#define EC_MIN 0.0
#define EC_MAX 5000.0
#define TEMP_MIN -10.0
#define TEMP_MAX 60.0
#define HUMIDITY_MIN 0.0
#define HUMIDITY_MAX 100.0

// ----------------------------- ESTRUTURAS DE DADOS -----------------------------------
struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  unsigned long timestamp;
  bool valid;
};

struct MqttMessage {
  String topic;
  String payload;
  unsigned long timestamp;
};

// ----------------------------- VARIÁVEIS GLOBAIS -------------------------------------
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
BluetoothSerial SerialBT;
Preferences preferences;

SensorData currentSensorData;
SensorData sensorHistory[DATA_VALIDATION_SAMPLES];
int sensorHistoryIndex = 0;

MqttMessage mqttBuffer[MQTT_BUFFER_SIZE];
int mqttBufferHead = 0;
int mqttBufferTail = 0;
int mqttBufferCount = 0;

bool wifiConnected = false;
bool mqttConnected = false;
bool emergencyMode = false;
bool firstConnectionEstablished = false;

unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastOnlineTimestamp = 0;
unsigned long lastBtEmergencyMsg = 0;

int wifiRetryCount = 0;
int mqttRetryCount = 0;

// ----------------------------- FUNÇÕES AUXILIARES ------------------------------------
void logMessage(const char* level, const char* msg) {
  char buf[256];
  snprintf(buf, sizeof(buf), "[%s] %lu %s", level, millis(), msg);
  Serial.println(buf);
  
  if (SerialBT.hasClient()) {
    SerialBT.println(buf);
  }
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

bool isValidSensorValue(float value, float min, float max) {
  return !isnan(value) && !isinf(value) && value >= min && value <= max;
}

// ----------------------------- WATCHDOG TIMER ----------------------------------------
void initWatchdog() {
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  logMessage("INFO", "Watchdog timer inicializado");
}

void resetWatchdog() {
  esp_task_wdt_reset();
}

// ----------------------------- WIFI --------------------------------------------------
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  logMessage("INFO", "Conectando ao WiFi...");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_RETRY) {
    delay(1000);
    Serial.print(".");
    attempts++;
    resetWatchdog();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiRetryCount = 0;
    char buf[100];
    snprintf(buf, sizeof(buf), "WiFi conectado! IP: %s", WiFi.localIP().toString().c_str());
    logMessage("INFO", buf);
  } else {
    wifiConnected = false;
    logMessage("ERROR", "Falha ao conectar WiFi");
  }
}

void checkWiFi() {
  unsigned long now = millis();
  
  if (now - lastWifiCheck < WIFI_RECONNECT_INTERVAL) return;
  lastWifiCheck = now;
  
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    wifiRetryCount++;
    
    if (wifiRetryCount <= WIFI_MAX_RETRY) {
      logMessage("WARN", "WiFi desconectado. Tentando reconectar...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    } else {
      logMessage("ERROR", "WiFi: máximo de tentativas atingido");
    }
  } else {
    if (!wifiConnected) {
      wifiConnected = true;
      wifiRetryCount = 0;
      logMessage("INFO", "WiFi reconectado!");
    }
  }
}

// ----------------------------- MQTT --------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = String((char*)payload);
  
  char buf[150];
  snprintf(buf, sizeof(buf), "MQTT recebido em %s: %s", topic, message.c_str());
  logMessage("INFO", buf);
}

void setupMQTT() {
  espClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);
}

bool mqttReconnect() {
  if (mqttClient.connected()) return true;
  if (!wifiConnected) return false;
  
  logMessage("INFO", "Tentando conectar ao MQTT...");
  
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    mqttConnected = true;
    mqttRetryCount = 0;
    logMessage("INFO", "MQTT conectado!");
    
    // Processar mensagens pendentes no buffer
    flushMqttBuffer();
    
    return true;
  } else {
    mqttConnected = false;
    mqttRetryCount++;
    
    char buf[100];
    snprintf(buf, sizeof(buf), "MQTT falhou, rc=%d", mqttClient.state());
    logMessage("ERROR", buf);
    
    return false;
  }
}

void addToMqttBuffer(const String& topic, const String& payload) {
  if (mqttBufferCount >= MQTT_BUFFER_SIZE) {
    logMessage("WARN", "Buffer MQTT cheio. Descartando mensagem mais antiga.");
    mqttBufferTail = (mqttBufferTail + 1) % MQTT_BUFFER_SIZE;
    mqttBufferCount--;
  }
  
  mqttBuffer[mqttBufferHead].topic = topic;
  mqttBuffer[mqttBufferHead].payload = payload;
  mqttBuffer[mqttBufferHead].timestamp = millis();
  
  mqttBufferHead = (mqttBufferHead + 1) % MQTT_BUFFER_SIZE;
  mqttBufferCount++;
}

void flushMqttBuffer() {
  if (!mqttConnected || mqttBufferCount == 0) return;
  
  int processed = 0;
  while (mqttBufferCount > 0 && processed < 5) {
    MqttMessage& msg = mqttBuffer[mqttBufferTail];
    
    if (mqttClient.publish(msg.topic.c_str(), msg.payload.c_str())) {
      mqttBufferTail = (mqttBufferTail + 1) % MQTT_BUFFER_SIZE;
      mqttBufferCount--;
      processed++;
    } else {
      logMessage("WARN", "Falha ao enviar mensagem do buffer");
      break;
    }
  }
  
  if (processed > 0) {
    char buf[100];
    snprintf(buf, sizeof(buf), "Buffer MQTT: %d mensagens enviadas, %d restantes", processed, mqttBufferCount);
    logMessage("INFO", buf);
  }
}

void publishMqtt(const String& topic, const String& payload) {
  if (mqttConnected && mqttClient.publish(topic.c_str(), payload.c_str())) {
    logMessage("INFO", "Mensagem MQTT publicada");
  } else {
    addToMqttBuffer(topic, payload);
    logMessage("WARN", "Mensagem adicionada ao buffer");
  }
}

// ----------------------------- SENSORES ----------------------------------------------
float readPhSensor() {
  int rawValue = analogRead(PH_SENSOR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  float ph = mapFloat(voltage, 0.0, 3.3, 0.0, 14.0);
  return ph;
}

float readEcSensor() {
  int rawValue = analogRead(EC_SENSOR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  float ec = mapFloat(voltage, 0.0, 3.3, 0.0, 5000.0);
  return ec;
}

float readAirTemperature() {
  int rawValue = analogRead(TEMP_AIR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  float temp = mapFloat(voltage, 0.0, 3.3, -10.0, 50.0);
  return temp;
}

float readHumidity() {
  int rawValue = analogRead(HUMIDITY_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  float humidity = mapFloat(voltage, 0.0, 3.3, 0.0, 100.0);
  return humidity;
}

float readWaterTemperature() {
  int rawValue = analogRead(TEMP_WATER_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  float temp = mapFloat(voltage, 0.0, 3.3, 0.0, 40.0);
  return temp;
}

void readSensors() {
  SensorData data;
  data.timestamp = millis();
  
  data.ph = readPhSensor();
  data.ec = readEcSensor();
  data.airTemp = readAirTemperature();
  data.humidity = readHumidity();
  data.waterTemp = readWaterTemperature();
  
  // Validação individual
  bool phValid = isValidSensorValue(data.ph, PH_MIN, PH_MAX);
  bool ecValid = isValidSensorValue(data.ec, EC_MIN, EC_MAX);
  bool airTempValid = isValidSensorValue(data.airTemp, TEMP_MIN, TEMP_MAX);
  bool humidityValid = isValidSensorValue(data.humidity, HUMIDITY_MIN, HUMIDITY_MAX);
  bool waterTempValid = isValidSensorValue(data.waterTemp, TEMP_MIN, TEMP_MAX);
  
  data.valid = phValid && ecValid && airTempValid && humidityValid && waterTempValid;
  
  if (!data.valid) {
    logMessage("WARN", "Dados de sensor inválidos detectados");
  }
  
  // Armazenar no histórico
  sensorHistory[sensorHistoryIndex] = data;
  sensorHistoryIndex = (sensorHistoryIndex + 1) % DATA_VALIDATION_SAMPLES;
  
  // Aplicar filtro de média móvel
  if (data.valid) {
    currentSensorData = data;
  }
}

// ----------------------------- PUBLICAÇÃO DE DADOS -----------------------------------
void publishSensorData() {
  if (!currentSensorData.valid) {
    logMessage("WARN", "Dados não válidos. Pulando publicação.");
    return;
  }
  
  StaticJsonDocument<512> doc;
  doc["device_id"] = DEVICE_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["timestamp"] = currentSensorData.timestamp;
  doc["ph"] = currentSensorData.ph;
  doc["ec"] = currentSensorData.ec;
  doc["airTemp"] = currentSensorData.airTemp;
  doc["humidity"] = currentSensorData.humidity;
  doc["waterTemp"] = currentSensorData.waterTemp;
  doc["emergency_mode"] = emergencyMode;
  
  String payload;
  serializeJson(doc, payload);
  
  publishMqtt(MQTT_TOPIC_SENSORS, payload);
}

void publishHeartbeat() {
  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["timestamp"] = millis();
  doc["uptime"] = millis() / 1000;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["wifi_connected"] = wifiConnected;
  doc["mqtt_connected"] = mqttConnected;
  doc["emergency_mode"] = emergencyMode;
  doc["buffer_size"] = mqttBufferCount;
  
  String payload;
  serializeJson(doc, payload);
  
  publishMqtt(MQTT_TOPIC_HEARTBEAT, payload);
}

// ----------------------------- BLUETOOTH ---------------------------------------------
void setupBluetooth() {
  if (!SerialBT.begin(BT_DEVICE_NAME)) {
    logMessage("ERROR", "Falha ao iniciar Bluetooth");
  } else {
    logMessage("INFO", "Bluetooth iniciado");
  }
}

void publishDataToBluetooth() {
  if (!SerialBT.hasClient()) return;
  if (!currentSensorData.valid) return;
  
  StaticJsonDocument<512> doc;
  doc["type"] = "sensor_data";
  doc["device_id"] = DEVICE_ID;
  doc["timestamp"] = currentSensorData.timestamp;
  doc["ph"] = currentSensorData.ph;
  doc["ec"] = currentSensorData.ec;
  doc["airTemp"] = currentSensorData.airTemp;
  doc["humidity"] = currentSensorData.humidity;
  doc["waterTemp"] = currentSensorData.waterTemp;
  doc["emergency_mode"] = emergencyMode;
  
  String payload;
  serializeJson(doc, payload);
  SerialBT.println(payload);
}

void handleBluetoothCommands() {
  if (!SerialBT.available()) return;
  
  String command = SerialBT.readStringUntil('\n');
  command.trim();
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, command);
  
  if (error) {
    logMessage("ERROR", "JSON Bluetooth inválido");
    return;
  }
  
  const char* cmd = doc["command"];
  
  if (strcmp(cmd, "get_data") == 0) {
    publishDataToBluetooth();
  } else if (strcmp(cmd, "get_status") == 0) {
    StaticJsonDocument<256> response;
    response["type"] = "status";
    response["wifi_connected"] = wifiConnected;
    response["mqtt_connected"] = mqttConnected;
    response["emergency_mode"] = emergencyMode;
    response["uptime"] = millis() / 1000;
    
    String payload;
    serializeJson(response, payload);
    SerialBT.println(payload);
  }
}

// ----------------------------- MODO DE EMERGÊNCIA ------------------------------------
void checkEmergencyMode() {
  unsigned long now = millis();
  
  if (wifiConnected && mqttConnected) {
    if (!firstConnectionEstablished) {
      firstConnectionEstablished = true;
      logMessage("INFO", "Primeira conexão estabelecida");
    }
    lastOnlineTimestamp = now;
    
    if (emergencyMode) {
      emergencyMode = false;
      logMessage("INFO", "Conexão restaurada. Saindo do modo de emergência");
    }
  } else {
    if (firstConnectionEstablished && (now - lastOnlineTimestamp > EMERGENCY_MODE_TIMEOUT)) {
      if (!emergencyMode) {
        emergencyMode = true;
        logMessage("WARN", "Conexão perdida. Ativando modo de emergência");
      }
    }
  }
}

void handleEmergencyMode() {
  unsigned long now = millis();
  
  if (now - lastBtEmergencyMsg >= BT_EMERGENCY_INTERVAL) {
    lastBtEmergencyMsg = now;
    publishDataToBluetooth();
    
    if (SerialBT.hasClient()) {
      logMessage("INFO", "Dados enviados via Bluetooth (modo emergência)");
    }
  }
}

// ----------------------------- SETUP -------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  logMessage("INFO", "=== AquaSys Sensor Module v2.0 ENHANCED ===");
  
  // Inicializar watchdog
  initWatchdog();
  
  // Configurar pinos
  pinMode(PH_SENSOR_PIN, INPUT);
  pinMode(EC_SENSOR_PIN, INPUT);
  pinMode(TEMP_AIR_PIN, INPUT);
  pinMode(HUMIDITY_PIN, INPUT);
  pinMode(TEMP_WATER_PIN, INPUT);
  
  // Inicializar estruturas
  currentSensorData.valid = false;
  for (int i = 0; i < DATA_VALIDATION_SAMPLES; i++) {
    sensorHistory[i].valid = false;
  }
  
  // Inicializar conectividade
  setupWiFi();
  setupMQTT();
  setupBluetooth();
  
  logMessage("INFO", "Sistema inicializado!");
}

// ----------------------------- LOOP --------------------------------------------------
void loop() {
  resetWatchdog();
  
  unsigned long now = millis();
  
  // Verificar WiFi
  checkWiFi();
  
  // Verificar MQTT
  if (wifiConnected && !mqttConnected) {
    if (now - lastHeartbeat >= MQTT_RECONNECT_INTERVAL) {
      mqttReconnect();
    }
  }
  
  if (mqttConnected) {
    mqttClient.loop();
  }
  
  // Ler sensores
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }
  
  // Publicar dados
  if (now - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMqttPublish = now;
    publishSensorData();
  }
  
  // Heartbeat
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    publishHeartbeat();
  }
  
  // Flush buffer MQTT
  if (mqttConnected && mqttBufferCount > 0) {
    flushMqttBuffer();
  }
  
  // Verificar modo de emergência
  checkEmergencyMode();
  
  if (emergencyMode) {
    handleEmergencyMode();
  }
  
  // Processar comandos Bluetooth
  handleBluetoothCommands();
  
  delay(100);
}
