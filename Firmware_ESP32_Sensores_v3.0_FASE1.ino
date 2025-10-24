/*
 * Firmware ESP32 Sensores v3.0 - FASE 1
 * 
 * Melhorias implementadas:
 * - Sistema de UUID único por dispositivo
 * - Intervalos otimizados (leitura 30s, publicação 60s)
 * - Sistema de diagnóstico completo (health check)
 * - Logs estruturados com níveis
 * - Monitoramento de WiFi/MQTT detalhado
 * - Contadores de reconexão e erros
 * - Recovery automático
 */

#include <WiFi.h>
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

// ==================== CONFIGURAÇÕES ====================
#define FIRMWARE_VERSION "3.0"
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

// Intervalos otimizados (Fase 1)
#define SENSOR_READ_INTERVAL 30000    // 30 segundos (antes: 5s)
#define MQTT_PUBLISH_INTERVAL 60000   // 60 segundos (antes: 15s)
#define HEARTBEAT_INTERVAL 30000      // 30 segundos
#define DISPLAY_UPDATE_INTERVAL 2000  // 2 segundos

// Timeouts
#define WIFI_TIMEOUT_MS 20000
#define MQTT_RECONNECT_INTERVAL 5000
#define WATCHDOG_TIMEOUT_SEC 60

// MQTT Broker
const char* mqtt_broker = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_user = "aquasys";
const char* mqtt_password = "aquasys2024";

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
  PAGE_TOTAL
};

// ==================== OBJETOS GLOBAIS ====================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;
DHT dht(DHT_PIN, DHT22);
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

// ==================== VARIÁVEIS GLOBAIS ====================

// UUID único do dispositivo
String deviceUUID = "";
String mqttClientId = "";

// Configuração WiFi
String wifiSSID = "";
String wifiPassword = "";
bool isAPMode = false;

// Dados
SensorData currentData;
DiagnosticData diagnostics;

// Estados de conexão
bool wifiConnected = false;
bool mqttConnected = false;

// Timestamps
unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastMqttReconnect = 0;
unsigned long bootTime = 0;
unsigned long lastSuccessfulPublish = 0;

// Display
Page currentPage = PAGE_PH_EC;

// Calibração
float phCalibSlope = 3.5;
float phCalibIntercept = 0.0;
float phCal4Voltage = 2.03;
float phCal7Voltage = 1.50;
float ecCalibFactor = 1.0;

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

// ==================== GERAÇÃO DE UUID ====================

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
  phCal4Voltage = preferences.getFloat("ph_cal4", 2.03);
  phCal7Voltage = preferences.getFloat("ph_cal7", 1.50);
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
      logMessage(LOG_WARN, "WiFi desconectado, tentando reconectar...");
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
  
  logMessage(LOG_INFO, ("Modo AP iniciado: " + apSSID).c_str());
  logMessage(LOG_INFO, ("IP do AP: " + WiFi.softAPIP().toString()).c_str());
  
  setupWebServer();
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<title>HydroSmart Setup</title></head><body>";
    html += "<h1>Configuração WiFi - " + deviceUUID + "</h1>";
    html += "<form action='/save' method='POST'>";
    html += "SSID: <input name='ssid' required><br>";
    html += "Senha: <input name='pass' type='password' required><br>";
    html += "<input type='submit' value='Salvar'></form></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    saveWiFiConfig(ssid, pass);
    
    server.send(200, "text/html", 
      "<html><body><h1>Configuração salva!</h1>"
      "<p>Reiniciando...</p></body></html>");
    
    delay(2000);
    ESP.restart();
  });
  
  server.begin();
}

// ==================== MQTT ====================

void setupMQTT() {
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(15);
  
  logMessage(LOG_INFO, "MQTT configurado");
}

void reconnectMQTT() {
  if (isAPMode || !wifiConnected) return;
  
  unsigned long now = millis();
  if (now - lastMqttReconnect < MQTT_RECONNECT_INTERVAL) return;
  lastMqttReconnect = now;
  
  if (!mqttClient.connected()) {
    logMessage(LOG_INFO, "Tentando conectar ao MQTT...");
    
    if (mqttClient.connect(mqttClientId.c_str(), mqtt_user, mqtt_password)) {
      mqttConnected = true;
      mqttClient.subscribe(TOPIC_COMMANDS);
      logMessage(LOG_INFO, "MQTT conectado");
      diagnostics.mqttFailedAttempts = 0;
    } else {
      mqttConnected = false;
      diagnostics.mqttFailedAttempts++;
      logMessage(LOG_ERROR, ("Falha MQTT: " + String(mqttClient.state())).c_str());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  lastSuccessfulPublish = millis();
  diagnostics.mqttLastMessageAge = 0;
  
  logMessage(LOG_DEBUG, ("Mensagem recebida: " + String(topic)).c_str());
  
  // Processar comandos futuros aqui
}

// ==================== LEITURA DE SENSORES ====================

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
  
  // Verificar se há pelo menos um sensor válido
  currentData.hasAnyValidData = currentData.phValid || currentData.ecValid || 
                                currentData.airTempValid || currentData.humidityValid || 
                                currentData.waterTempValid;
  
  if (!currentData.hasAnyValidData) {
    diagnostics.sensorReadErrors++;
    logMessage(LOG_ERROR, "Nenhum sensor válido!");
  } else {
    logMessage(LOG_DEBUG, "Sensores lidos com sucesso");
  }
}

// ==================== PUBLICAÇÃO MQTT ====================

void publishSensorData() {
  if (!mqttConnected || !currentData.hasAnyValidData) return;
  
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
    lastSuccessfulPublish = millis();
    logMessage(LOG_INFO, "Dados publicados");
  } else {
    diagnostics.publishErrors++;
    logMessage(LOG_ERROR, "Falha ao publicar dados");
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
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  mqttClient.publish(TOPIC_HEARTBEAT, buffer, false);
  logMessage(LOG_DEBUG, "Heartbeat enviado");
}

void updateDiagnostics() {
  diagnostics.uptime = millis() / 1000;
  diagnostics.wifiRssi = WiFi.RSSI();
  diagnostics.mqttConnected = mqttClient.connected();
  diagnostics.freeHeap = ESP.getFreeHeap();
  diagnostics.minFreeHeap = ESP.getMinFreeHeap();
  
  if (lastSuccessfulPublish > 0) {
    diagnostics.mqttLastMessageAge = millis() - lastSuccessfulPublish;
  }
}

// ==================== DISPLAY ====================

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // Cabeçalho
  display.println(deviceUUID);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  switch(currentPage) {
    case PAGE_PH_EC:
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.print("pH: ");
      display.setTextSize(2);
      if (currentData.phValid) {
        display.println(currentData.ph, 2);
      } else {
        display.println("---");
      }
      
      display.setTextSize(1);
      display.print("EC: ");
      display.setTextSize(2);
      if (currentData.ecValid) {
        display.print(currentData.ec, 0);
        display.setTextSize(1);
        display.println(" uS");
      } else {
        display.println("---");
      }
      break;
      
    case PAGE_TEMP_HUMIDITY:
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.print("Temp Ar: ");
      display.setTextSize(2);
      if (currentData.airTempValid) {
        display.print(currentData.airTemp, 1);
        display.setTextSize(1);
        display.println(" C");
      } else {
        display.println("---");
      }
      
      display.setTextSize(1);
      display.print("Umidade: ");
      display.setTextSize(2);
      if (currentData.humidityValid) {
        display.print(currentData.humidity, 0);
        display.setTextSize(1);
        display.println(" %");
      } else {
        display.println("---");
      }
      break;
      
    case PAGE_WATER_TEMP:
      display.setCursor(0, 25);
      display.setTextSize(1);
      display.println("Temp Agua:");
      display.setTextSize(3);
      if (currentData.waterTempValid) {
        display.print(currentData.waterTemp, 1);
        display.setTextSize(1);
        display.println(" C");
      } else {
        display.println("---");
      }
      break;
      
    case PAGE_DIAGNOSTICS:
      display.setCursor(0, 15);
      display.setTextSize(1);
      display.print("WiFi: ");
      display.println(wifiConnected ? "OK" : "ERRO");
      display.print("MQTT: ");
      display.println(mqttConnected ? "OK" : "ERRO");
      display.print("RSSI: ");
      display.print(diagnostics.wifiRssi);
      display.println(" dBm");
      display.print("Heap: ");
      display.print(diagnostics.freeHeap / 1024);
      display.println(" KB");
      display.print("Boot: ");
      display.println(diagnostics.bootCount);
      break;
  }
  
  // Rodapé - indicadores de status
  display.drawLine(0, 55, 128, 55, SSD1306_WHITE);
  display.setCursor(0, 57);
  display.print(wifiConnected ? "W" : "w");
  display.print(" ");
  display.print(mqttConnected ? "M" : "m");
  display.print(" ");
  display.print(currentData.hasAnyValidData ? "S" : "s");
  
  display.display();
}

void handleButtons() {
  static unsigned long lastButtonPress = 0;
  unsigned long now = millis();
  
  if (now - lastButtonPress < 200) return;
  
  if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
    currentPage = (Page)((currentPage + 1) % PAGE_TOTAL);
    lastButtonPress = now;
  }
  
  if (digitalRead(BUTTON_UP_PIN) == LOW) {
    currentPage = (Page)((currentPage - 1 + PAGE_TOTAL) % PAGE_TOTAL);
    lastButtonPress = now;
  }
}

// ==================== RECOVERY ====================

void checkRecovery() {
  // Verificar memória baixa
  if (diagnostics.freeHeap < 50000) {
    logMessage(LOG_WARN, "Memória baixa detectada!");
  }
  
  // Reiniciar após muitas falhas
  if (diagnostics.mqttFailedAttempts > 20) {
    logMessage(LOG_ERROR, "Muitas falhas MQTT, reiniciando...");
    preferences.begin("hydrosmart", false);
    preferences.putUInt("crash_count", diagnostics.crashCount + 1);
    preferences.end();
    delay(1000);
    ESP.restart();
  }
  
  // Safe boot se muitos crashes
  if (diagnostics.crashCount > 3 && diagnostics.uptime < 30) {
    logMessage(LOG_ERROR, "Loop de crash detectado, entrando em safe mode");
    startAPMode();
  }
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  bootTime = millis();
  
  logMessage(LOG_INFO, "=== HydroSmart Sensor v3.0 ===");
  logMessage(LOG_INFO, "Iniciando sistema...");
  
  // Gerar UUID
  generateDeviceUUID();
  
  // Inicializar watchdog
  initWatchdog();
  
  // Configurar botões
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  
  // Inicializar display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    logMessage(LOG_ERROR, "Falha ao inicializar display");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("HydroSmart v3.0");
    display.println(deviceUUID);
    display.println("Iniciando...");
    display.display();
  }
  
  // Carregar configuração
  loadConfig();
  
  // Inicializar sensores
  dht.begin();
  ds18b20.begin();
  
  // Conectar WiFi
  connectWiFi();
  
  // Configurar MQTT
  setupMQTT();
  
  logMessage(LOG_INFO, "Setup concluído!");
  
  // Primeira leitura
  readSensors();
  updateDisplay();
}

// ==================== LOOP ====================

void loop() {
  resetWatchdog();
  
  unsigned long now = millis();
  
  // Modo AP
  if (isAPMode) {
    server.handleClient();
    delay(10);
    return;
  }
  
  // Verificar conexões
  checkWiFi();
  reconnectMQTT();
  
  if (mqttConnected) {
    mqttClient.loop();
  }
  
  // Leitura de sensores (30s)
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }
  
  // Publicação MQTT (60s)
  if (now - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMqttPublish = now;
    publishSensorData();
  }
  
  // Heartbeat (30s)
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    publishHeartbeat();
  }
  
  // Atualizar display (2s)
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
  
  // Processar botões
  handleButtons();
  
  // Verificar recovery
  if (now % 60000 < 100) {  // A cada minuto
    checkRecovery();
  }
  
  delay(10);
}