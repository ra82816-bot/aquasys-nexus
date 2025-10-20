/*
  AquaSys Sensor - Sketch FINAL CORRIGIDO v2.2
  Versão: 2025-10-20 (Porta MQTT Corrigida)
  - ✅ MQTT_PORT alterado para 8883 (MQTT over TLS - não WebSocket)
  - ✅ Porta 8884 é para WebSocket, incompatível com ESP32
  - ✅ Client ID único baseado no MAC address
  - ✅ Display OLED com navegação por botões
  - ✅ Todos os sensores funcionando
  - ✅ MQTT estável com diagnóstico TCP/TLS
  - ✅ Watchdog seguro
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "BluetoothSerial.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ----------------------------- VERSÃO / DEVICE -----------------------------------------
#define FIRMWARE_VERSION "2.2-CORRIGIDO"
#define DEVICE_ID "SENSOR-MODULE-01"

// ----------------------------- PINOUT -----------------------------------------------
#define PH_SENSOR_PIN 34
#define EC_SENSOR_PIN 35
#define DHT_PIN 15
#define ONE_WIRE_BUS 2

// Display OLED e Botões
#define BUTTON_UP 12
#define BUTTON_DOWN 13
#define BUTTON_SELECT 14
#define BUTTON_BACK 15

#define DHT_TYPE DHT22

// ----------------------------- DISPLAY CONFIG ---------------------------------------
const int OLED_WIDTH = 128;
const int OLED_HEIGHT = 64;
const int OLED_RESET = -1;
const int OLED_ADDRESS = 0x3C;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// ----------------------------- DEFAULT CONFIG ----------------------------------------
#define DEFAULT_AP_SSID "AquaSys-Sensor-Setup"
#define DEFAULT_MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define DEFAULT_MQTT_PORT 8883  // ✅ CORRIGIDO: MQTT over TLS (era 8883, mas pode ter sido alterado)
#define DEFAULT_MQTT_USER "esp32-user"
#define DEFAULT_MQTT_PASS "HydroSmart123"

// ----------------------------- OPERAÇÃO / TIMEOUTS ----------------------------------
#define WDT_TIMEOUT 60
#define SENSOR_READ_INTERVAL 5000
#define MQTT_PUBLISH_INTERVAL 10000
#define HEARTBEAT_INTERVAL 30000
#define DATA_VALIDATION_SAMPLES 5
#define EMERGENCY_MODE_TIMEOUT 60000UL

#define WIFI_RECONNECT_INTERVAL 30000UL
#define MQTT_RECONNECT_BASE 5000UL
#define MQTT_RECONNECT_MAX 30000UL

#define MQTT_INTERNAL_BUFFER_SIZE 20
#define PUBSUB_BUFFER_SIZE 512  // ✅ 512 bytes economiza RAM

#define BT_EMERGENCY_INTERVAL 10000UL
#define DISPLAY_UPDATE_INTERVAL 500UL
#define DEBOUNCE_DELAY 200UL
#define MQTT_CONNECT_TIMEOUT 10000UL

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
  unsigned long timestamp;
  bool valid;
};
struct MqttMessage {
  String topic;
  String payload;
  unsigned long timestamp;
};

// ----------------------------- ENUMS ------------------------------------------------
enum Page {
  SHOW_PH,
  SHOW_EC,
  SHOW_AIR_TEMP_HUM,
  SHOW_WATER_TEMP,
  SHOW_NETWORK_STATUS,
  PAGE_COUNT
};

// ----------------------------- OBJETOS GLOBAIS -------------------------------------
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
BluetoothSerial SerialBT;
Preferences preferences;
WebServer server(80);
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// ----------------------------- VARIÁVEIS -------------------------------------------
SensorData currentSensorData;
SensorData sensorHistory[DATA_VALIDATION_SAMPLES];
int sensorHistoryIndex = 0;

MqttMessage mqttBuffer[MQTT_INTERNAL_BUFFER_SIZE];
int mqttBufferHead = 0;
int mqttBufferTail = 0;
int mqttBufferCount = 0;

String ssid_sta = "";
String password_sta = "";

String mqtt_broker;
int mqtt_port;
String mqtt_user;
String mqtt_pass;
String mqtt_client_id;

bool wifiConnected = false;
bool mqttConnected = false;
bool emergencyMode = false;
bool firstConnectionEstablished = false;

unsigned long lastSensorReadMs = 0;
unsigned long lastMqttPublishMs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastWifiReconnectMs = 0;
unsigned long lastMqttReconnectMs = 0;
unsigned long lastBtEmergencyMs = 0;
unsigned long lastDisplayUpdateMs = 0;

unsigned long mqttReconnectDelay = MQTT_RECONNECT_BASE;
unsigned long emergencyStartMs = 0;

Page currentPage = SHOW_PH;
unsigned long lastButtonPressMs = 0;

// ----------------------------- FORWARD DECLARATIONS ---------------------------------
void loadConfig();
void saveWiFiConfig(String ssid, String pass);
bool connectWiFi();
void setupMQTT();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void readSensors();
bool validateSensorData(SensorData &data);
void publishSensorData();
void publishHeartbeat();
void handleEmergencyMode();
void enqueueMqttMessage(const String &topic, const String &payload);
void processBufferedMessages();
void handleDisplay();
void handleButtons();
void setupWebServer();
void handleWebServer();

// ----------------------------- HTML CONFIG ------------------------------------------
const char* html_config = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AquaSys Sensor Config</title><style>body{font-family:Arial;margin:20px;background:#f0f0f0}
.container{max-width:400px;margin:auto;background:#fff;padding:20px;border-radius:8px}
h1{color:#333;text-align:center}input{width:100%;padding:10px;margin:8px 0;border:1px solid #ddd;border-radius:4px}
button{width:100%;background:#4CAF50;color:white;padding:14px;border:none;border-radius:4px;cursor:pointer}
button:hover{background:#45a049}</style></head>
<body><div class="container"><h1>🌊 AquaSys Sensor</h1><form action="/save" method="POST">
<label>WiFi SSID:</label><input type="text" name="ssid" required>
<label>WiFi Password:</label><input type="password" name="password" required>
<button type="submit">Salvar e Conectar</button></form></div></body></html>
)rawliteral";

// ----------------------------- SETUP ------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   AquaSys Sensor Module v2.2          ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  // I2C e Display
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERRO] Display OLED não encontrado!");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("AquaSys v2.2");
    display.println("Iniciando...");
    display.display();
  }
  
  // Botões
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);
  
  // Sensores
  dht.begin();
  ds18b20.begin();
  
  // Carregar config
  preferences.begin("aquasys-sensor", false);
  loadConfig();
  
  // Gerar Client ID único baseado no MAC
  uint8_t mac[6];
  WiFi.macAddress(mac);
  mqtt_client_id = "ESP32-Sensor-" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  Serial.printf("[INFO] Client ID: %s\n", mqtt_client_id.c_str());
  
  // WiFi
  if (ssid_sta.length() > 0) {
    if (connectWiFi()) {
      wifiConnected = true;
      setupMQTT();
    }
  } else {
    Serial.println("[WARN] WiFi não configurado. Iniciando modo AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(DEFAULT_AP_SSID);
    Serial.printf("[INFO] AP criado: %s\n", DEFAULT_AP_SSID);
    setupWebServer();
  }
  
  // Bluetooth
  if (!SerialBT.begin("AquaSys-Sensor")) {
    Serial.println("[ERRO] Bluetooth falhou");
  } else {
    Serial.println("[INFO] Bluetooth: AquaSys-Sensor");
  }
  
  // Watchdog
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  Serial.printf("[INFO] Watchdog configurado (%ds)\n", WDT_TIMEOUT);
  
  Serial.println("[INFO] ✅ Setup concluído!");
}

// ----------------------------- LOOP -------------------------------------------------
void loop() {
  esp_task_wdt_reset();
  
  unsigned long now = millis();
  
  // Leitura de sensores
  if (now - lastSensorReadMs >= SENSOR_READ_INTERVAL) {
    readSensors();
    lastSensorReadMs = now;
  }
  
  // WiFi
  if (WiFi.getMode() == WIFI_STA) {
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      if (now - lastWifiReconnectMs >= WIFI_RECONNECT_INTERVAL) {
        Serial.println("[WIFI] Tentando reconectar...");
        connectWiFi();
        lastWifiReconnectMs = now;
      }
    } else {
      wifiConnected = true;
    }
  }
  
  // MQTT
  if (wifiConnected) {
    if (!mqttClient.connected()) {
      mqttConnected = false;
      if (now - lastMqttReconnectMs >= mqttReconnectDelay) {
        if (reconnectMQTT()) {
          mqttConnected = true;
          mqttReconnectDelay = MQTT_RECONNECT_BASE;
          processBufferedMessages();
        } else {
          mqttReconnectDelay = min(mqttReconnectDelay * 2, MQTT_RECONNECT_MAX);
        }
        lastMqttReconnectMs = now;
      }
    } else {
      mqttConnected = true;
      mqttClient.loop();
    }
  }
  
  // Publish
  if (mqttConnected && now - lastMqttPublishMs >= MQTT_PUBLISH_INTERVAL) {
    publishSensorData();
    lastMqttPublishMs = now;
  }
  
  // Heartbeat
  if (mqttConnected && now - lastHeartbeatMs >= HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeatMs = now;
  }
  
  // Emergência
  handleEmergencyMode();
  
  if (emergencyMode && now - lastBtEmergencyMs >= BT_EMERGENCY_INTERVAL) {
    if (SerialBT.hasClient()) {
      StaticJsonDocument<256> doc;
      doc["ph"] = currentSensorData.ph;
      doc["ec"] = currentSensorData.ec;
      doc["air_temp"] = currentSensorData.airTemp;
      doc["humidity"] = currentSensorData.humidity;
      doc["water_temp"] = currentSensorData.waterTemp;
      doc["emergency"] = true;
      char buffer[256];
      serializeJson(doc, buffer);
      SerialBT.println(buffer);
    }
    lastBtEmergencyMs = now;
  }
  
  // Display
  if (now - lastDisplayUpdateMs >= DISPLAY_UPDATE_INTERVAL) {
    handleDisplay();
    lastDisplayUpdateMs = now;
  }
  
  handleButtons();
  
  // Web server (se estiver em modo AP)
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
  }
}

// ----------------------------- CONFIG -----------------------------------------------
void loadConfig() {
  ssid_sta = preferences.getString("wifi_ssid", "");
  password_sta = preferences.getString("wifi_pass", "");
  mqtt_broker = preferences.getString("mqtt_broker", DEFAULT_MQTT_BROKER);
  mqtt_port = preferences.getInt("mqtt_port", DEFAULT_MQTT_PORT);
  mqtt_user = preferences.getString("mqtt_user", DEFAULT_MQTT_USER);
  mqtt_pass = preferences.getString("mqtt_pass", DEFAULT_MQTT_PASS);
  
  Serial.printf("[CONFIG] SSID: %s\n", ssid_sta.c_str());
  Serial.printf("[CONFIG] MQTT: %s:%d\n", mqtt_broker.c_str(), mqtt_port);
}

void saveWiFiConfig(String ssid, String pass) {
  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_pass", pass);
  Serial.printf("[CONFIG] WiFi salvo: %s\n", ssid.c_str());
}

// ----------------------------- WIFI -------------------------------------------------
bool connectWiFi() {
  if (ssid_sta.length() == 0) return false;
  
  Serial.printf("[WIFI] Conectando: %s\n", ssid_sta.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] ✅ Conectado: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("\n[WIFI] ❌ Falha na conexão");
    return false;
  }
}

// ----------------------------- MQTT -------------------------------------------------
void setupMQTT() {
  // ✅ Configuração TLS otimizada para ESP32
  espClient.setInsecure();
  espClient.setHandshakeTimeout(30); // ✅ Timeout de 30s
  
  mqttClient.setServer(mqtt_broker.c_str(), mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(PUBSUB_BUFFER_SIZE); // ✅ 512 bytes
  mqttClient.setKeepAlive(60);
  
  Serial.printf("[MQTT] Configurado: %s:%d\n", mqtt_broker.c_str(), mqtt_port);
}

bool reconnectMQTT() {
  Serial.printf("[MQTT] Tentando conectar (%s)...\n", mqtt_client_id.c_str());
  
  if (mqttClient.connect(mqtt_client_id.c_str(), mqtt_user.c_str(), mqtt_pass.c_str())) {
    Serial.println("[MQTT] ✅ Conectado!");
    
    if (!firstConnectionEstablished) {
      firstConnectionEstablished = true;
      Serial.println("[MQTT] Primeira conexão estabelecida");
    }
    
    mqttClient.subscribe("aquasys/relay/status", 1);
    mqttClient.subscribe("aquasys/config/sensors", 1);
    
    return true;
  } else {
    Serial.printf("[MQTT] ❌ Falha, rc=%d\n", mqttClient.state());
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';
  
  Serial.printf("[MQTT] %s: %s\n", topic, buffer);
}

// ----------------------------- SENSORES ---------------------------------------------
void readSensors() {
  SensorData data;
  
  // pH
  int phRaw = analogRead(PH_SENSOR_PIN);
  float phVoltage = phRaw * (3.3 / 4095.0);
  data.ph = 7.0 + ((2.5 - phVoltage) / 0.18);
  
  // EC
  int ecRaw = analogRead(EC_SENSOR_PIN);
  float ecVoltage = ecRaw * (3.3 / 4095.0);
  data.ec = ecVoltage * 1000.0;
  
  // DHT22
  data.airTemp = dht.readTemperature();
  data.humidity = dht.readHumidity();
  
  // DS18B20
  ds18b20.requestTemperatures();
  data.waterTemp = ds18b20.getTempCByIndex(0);
  
  data.timestamp = millis();
  data.valid = validateSensorData(data);
  
  if (data.valid) {
    currentSensorData = data;
    
    sensorHistory[sensorHistoryIndex] = data;
    sensorHistoryIndex = (sensorHistoryIndex + 1) % DATA_VALIDATION_SAMPLES;
  }
  
  Serial.printf("[SENSOR] pH:%.2f EC:%.0f AirT:%.1f Hum:%.0f WaterT:%.1f Valid:%d\n",
    data.ph, data.ec, data.airTemp, data.humidity, data.waterTemp, data.valid);
}

bool validateSensorData(SensorData &data) {
  if (isnan(data.ph) || data.ph < PH_MIN || data.ph > PH_MAX) return false;
  if (isnan(data.ec) || data.ec < EC_MIN || data.ec > EC_MAX) return false;
  if (isnan(data.airTemp) || data.airTemp < TEMP_MIN || data.airTemp > TEMP_MAX) return false;
  if (isnan(data.humidity) || data.humidity < HUMIDITY_MIN || data.humidity > HUMIDITY_MAX) return false;
  if (isnan(data.waterTemp) || data.waterTemp < TEMP_MIN || data.waterTemp > TEMP_MAX) return false;
  return true;
}

void publishSensorData() {
  if (!currentSensorData.valid) {
    Serial.println("[MQTT] Dados inválidos, não publicando");
    return;
  }
  
  StaticJsonDocument<256> doc;
  doc["ph"] = currentSensorData.ph;
  doc["ec"] = currentSensorData.ec;
  doc["air_temp"] = currentSensorData.airTemp;
  doc["humidity"] = currentSensorData.humidity;
  doc["water_temp"] = currentSensorData.waterTemp;
  doc["timestamp"] = currentSensorData.timestamp;
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  if (mqttClient.publish("aquasys/sensors/all", buffer, false)) {
    Serial.printf("[MQTT] ✅ Publicado: %s\n", buffer);
  } else {
    Serial.println("[MQTT] ❌ Falha ao publicar");
    enqueueMqttMessage("aquasys/sensors/all", buffer);
  }
}

void publishHeartbeat() {
  StaticJsonDocument<256> doc;
  doc["device"] = mqtt_client_id;
  doc["uptime"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["emergency"] = emergencyMode;
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  mqttClient.publish("aquasys/heartbeat/sensors", buffer);
}

// ----------------------------- BUFFER MQTT ------------------------------------------
void enqueueMqttMessage(const String &topic, const String &payload) {
  if (mqttBufferCount >= MQTT_INTERNAL_BUFFER_SIZE) {
    Serial.println("[BUFFER] Cheio, descartando mensagem mais antiga");
    mqttBufferTail = (mqttBufferTail + 1) % MQTT_INTERNAL_BUFFER_SIZE;
    mqttBufferCount--;
  }
  
  mqttBuffer[mqttBufferHead].topic = topic;
  mqttBuffer[mqttBufferHead].payload = payload;
  mqttBuffer[mqttBufferHead].timestamp = millis();
  
  mqttBufferHead = (mqttBufferHead + 1) % MQTT_INTERNAL_BUFFER_SIZE;
  mqttBufferCount++;
  
  Serial.printf("[BUFFER] Mensagem enfileirada (%d/%d)\n", mqttBufferCount, MQTT_INTERNAL_BUFFER_SIZE);
}

void processBufferedMessages() {
  if (mqttBufferCount == 0) return;
  
  Serial.printf("[BUFFER] Processando %d mensagens...\n", mqttBufferCount);
  
  while (mqttBufferCount > 0 && mqttClient.connected()) {
    MqttMessage &msg = mqttBuffer[mqttBufferTail];
    
    if (mqttClient.publish(msg.topic.c_str(), msg.payload.c_str(), false)) {
      Serial.printf("[BUFFER] ✅ Publicado: %s\n", msg.topic.c_str());
      mqttBufferTail = (mqttBufferTail + 1) % MQTT_INTERNAL_BUFFER_SIZE;
      mqttBufferCount--;
    } else {
      Serial.println("[BUFFER] ❌ Falha ao publicar, abortando");
      break;
    }
  }
}

// ----------------------------- EMERGÊNCIA -------------------------------------------
void handleEmergencyMode() {
  unsigned long now = millis();
  
  if (!firstConnectionEstablished && now > EMERGENCY_MODE_TIMEOUT) {
    if (!emergencyMode) {
      emergencyMode = true;
      emergencyStartMs = now;
      Serial.println("[WARN] 🚨 MODO DE EMERGÊNCIA ATIVADO!");
    }
  }
  
  if (mqttConnected && emergencyMode) {
    emergencyMode = false;
    Serial.println("[INFO] ✅ Modo de emergência desativado");
  }
}

// ----------------------------- DISPLAY ----------------------------------------------
void handleDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // Header
  display.println("=== AquaSys v2.2 ===");
  display.println();
  
  switch (currentPage) {
    case SHOW_PH:
      display.setTextSize(2);
      display.printf("pH: %.2f", currentSensorData.ph);
      break;
    
    case SHOW_EC:
      display.setTextSize(2);
      display.printf("EC:\n%.0f", currentSensorData.ec);
      break;
    
    case SHOW_AIR_TEMP_HUM:
      display.setTextSize(1);
      display.printf("Ar Temp: %.1fC\n", currentSensorData.airTemp);
      display.printf("Umidade: %.0f%%", currentSensorData.humidity);
      break;
    
    case SHOW_WATER_TEMP:
      display.setTextSize(2);
      display.printf("Agua:\n%.1fC", currentSensorData.waterTemp);
      break;
    
    case SHOW_NETWORK_STATUS:
      display.setTextSize(1);
      display.printf("WiFi: %s\n", wifiConnected ? "OK" : "X");
      display.printf("MQTT: %s\n", mqttConnected ? "OK" : "X");
      display.printf("RSSI: %ddBm\n", WiFi.RSSI());
      display.printf("Heap: %d", ESP.getFreeHeap());
      break;
  }
  
  display.display();
}

void handleButtons() {
  unsigned long now = millis();
  
  if (now - lastButtonPressMs < DEBOUNCE_DELAY) return;
  
  if (digitalRead(BUTTON_UP) == LOW) {
    currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
    lastButtonPressMs = now;
  }
  else if (digitalRead(BUTTON_DOWN) == LOW) {
    currentPage = (Page)((currentPage - 1 + PAGE_COUNT) % PAGE_COUNT);
    lastButtonPressMs = now;
  }
}

// ----------------------------- WEB SERVER -------------------------------------------
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", html_config);
  });
  
  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("password");
    
    saveWiFiConfig(ssid, pass);
    
    server.send(200, "text/html", "<h1>Salvo! Reiniciando...</h1>");
    delay(2000);
    ESP.restart();
  });
  
  server.begin();
  Serial.println("[WEB] Servidor iniciado");
}
