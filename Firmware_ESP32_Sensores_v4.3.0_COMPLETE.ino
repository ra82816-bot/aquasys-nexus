/*
 * ============================================================================
 * AquaSys Nexus - Sensor Module v4.3.0-COMPLETE
 * ============================================================================
 * SOLUÇÃO DEFINITIVA - Funciona COM ou SEM internet
 * 
 * RECURSOS:
 * ✅ WiFi com modo AP automático (sem internet → AP em 15s)
 * ✅ Portal captivo moderno para configuração WiFi
 * ✅ Suporte a 3 redes WiFi com prioridades
 * ✅ BLE Server sempre ativo (para atuador)
 * ✅ Leitura de sensores: pH, EC, Temp Água, Temp Ar, Umidade
 * ✅ Publicação via MQTT (online) e BLE (sempre)
 * ✅ Watchdog robusto (60s)
 * ✅ Logging estruturado com níveis
 * ✅ UUID único por MAC
 * ✅ Calibração persistente (NVS)
 * 
 * AUTOR: HydroSmart Team
 * DATA: 2025-01-05
 * ============================================================================
 */

// ==================== SEÇÃO 1: INCLUDES & DEFINES ====================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ==================== SEÇÃO 2: CONFIGURAÇÕES ====================
// Versão do Firmware
#define FIRMWARE_VERSION "4.3.0-COMPLETE"
#define DEVICE_TYPE "SENSOR"

// Pinos dos Sensores
#define PIN_PH 34
#define PIN_EC 35
#define PIN_DHT 32
#define PIN_DS18B20 33

// Sensores
#define DHT_TYPE DHT22
DHT dht(PIN_DHT, DHT_TYPE);
OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);

// WiFi AP Mode
#define AP_SSID_PREFIX "AquaSys-SEN-"
#define AP_PASSWORD "aquasys2024"
#define AP_TIMEOUT 300000  // 5min

// Timeouts
#define WIFI_TIMEOUT 15000
#define MQTT_TIMEOUT 30000
#define SENSOR_READ_INTERVAL 30000  // 30s
#define WATCHDOG_TIMEOUT 60         // 60s

// MQTT Configuration
#define MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "esp32-user"
#define MQTT_PASS "HydroSmart123"

// MQTT Topics
#define TOPIC_SENSORS "aquasys/sensors/all"
#define TOPIC_HEARTBEAT "aquasys/heartbeat/sensor"
#define TOPIC_CALIBRATION "aquasys/calibration/sensor"

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_PH        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_EC        "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_UUID_AIR_TEMP  "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_UUID_HUMIDITY  "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define CHAR_UUID_WATER_TEMP "beb5483e-36e1-4688-b7f5-ea07361b26ac"

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

// ==================== SEÇÃO 3: ESTRUTURAS DE DADOS ====================
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
  float ph_slope;
  float ph_intercept;
  float ec_k;
  float ph_cal_4;
  float ph_cal_7;
  float ec_cal_1413;
};

// ==================== SEÇÃO 4: VARIÁVEIS GLOBAIS ====================
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

// Sensor Data
SensorData currentData = {0, 0, 0, 0, 0, false, 0};
CalibrationData calibration = {1.0, 0.0, 1.0, 4.0, 7.0, 1413.0};

// BLE
BLEServer* pBLEServer = nullptr;
BLECharacteristic* pCharPH = nullptr;
BLECharacteristic* pCharEC = nullptr;
BLECharacteristic* pCharAirTemp = nullptr;
BLECharacteristic* pCharHumidity = nullptr;
BLECharacteristic* pCharWaterTemp = nullptr;
bool bleActive = false;
bool deviceConnectedBLE = false;

// Timers
unsigned long lastSensorRead = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastWdtReset = 0;

// Preferences (NVS)
Preferences prefs;

// ==================== SEÇÃO 5: PROTÓTIPOS ====================
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

// MQTT
void setupMQTT();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData();
void publishHeartbeat();

// BLE
void setupBLE();
void publishDataToBLE();

// Sensores
void loadCalibration();
void saveCalibration();
float readPH();
float readEC(float waterTemp);
void readSensors();

// Watchdog
void initWatchdog();
void resetWatchdog();

// Setup & Loop
void setup();
void loop();

// ==================== SEÇÃO 6: IMPLEMENTAÇÃO - LOGGING ====================
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

// ==================== SEÇÃO 7: UUID ====================
String generateDeviceUUID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char uuid[20];
  sprintf(uuid, "SEN-%02X%02X%02X%02X%02X%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(uuid);
}

// ==================== SEÇÃO 8: WIFI ====================
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
}

void stopAPMode() {
  if (!apMode) return;
  
  logMessage(LOG_INFO, "Parando modo AP...");
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
}

// ==================== SEÇÃO 9: WEB SERVER ====================
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AquaSys - Configuração Sensor</title>
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
      <strong>Versão:</strong> )rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(<br>
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
      document.getElementById('status').textContent = 'Escaneando...';
      document.getElementById('status').className = 'status';
      document.getElementById('status').style.display = 'block';
      
      fetch('/scan')
        .then(response => response.json())
        .then(data => {
          const select = document.getElementById('ssid');
          select.innerHTML = '<option value="">Selecione uma rede...</option>';
          
          data.networks.forEach(network => {
            const option = document.createElement('option');
            option.value = network.ssid;
            option.textContent = network.ssid + ' (' + network.rssi + ' dBm)';
            select.appendChild(option);
          });
          
          document.getElementById('status').textContent = data.networks.length + ' redes encontradas';
          document.getElementById('status').className = 'status success';
        })
        .catch(error => {
          document.getElementById('status').textContent = 'Erro ao escanear';
          document.getElementById('status').className = 'status error';
        });
    }
    
    function saveWiFi(event) {
      event.preventDefault();
      
      const formData = new FormData(event.target);
      
      document.getElementById('status').textContent = 'Salvando...';
      document.getElementById('status').className = 'status';
      document.getElementById('status').style.display = 'block';
      
      fetch('/save', {
        method: 'POST',
        body: formData
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            document.getElementById('status').textContent = '✅ Configuração salva! Conectando...';
            document.getElementById('status').className = 'status success';
            setTimeout(() => { window.location.href = '/status'; }, 3000);
          } else {
            document.getElementById('status').textContent = '❌ Erro: ' + data.message;
            document.getElementById('status').className = 'status error';
          }
        })
        .catch(error => {
          document.getElementById('status').textContent = '❌ Erro de comunicação';
          document.getElementById('status').className = 'status error';
        });
    }
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleScan() {
  logMessage(LOG_INFO, "Escaneando redes WiFi...");
  int n = WiFi.scanNetworks();
  
  StaticJsonDocument<2048> doc;
  JsonArray networks = doc.createNestedArray("networks");
  
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSave() {
  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    StaticJsonDocument<128> doc;
    doc["success"] = false;
    doc["message"] = "Parâmetros inválidos";
    String response;
    serializeJson(doc, response);
    server.send(400, "application/json", response);
    return;
  }
  
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  for (int i = 0; i < 3; i++) {
    if (!networks[i].valid) {
      ssid.toCharArray(networks[i].ssid, 32);
      password.toCharArray(networks[i].password, 64);
      networks[i].priority = i + 1;
      networks[i].valid = true;
      break;
    }
  }
  
  saveWiFiConfig();
  
  StaticJsonDocument<128> doc;
  doc["success"] = true;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  delay(2000);
  stopAPMode();
  connectWiFi();
}

void handleStatus() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Status - Sensor</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 20px;
      padding: 40px;
      max-width: 600px;
      margin: 0 auto;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
    }
    h1 { color: #667eea; margin-bottom: 30px; }
    .status-item {
      padding: 15px;
      margin-bottom: 15px;
      background: #f8f9fa;
      border-radius: 10px;
      border-left: 4px solid #667eea;
    }
    .success { border-left-color: #28a745; }
    .error { border-left-color: #dc3545; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📊 Status do Sensor</h1>
    
    <div class="status-item )rawliteral" + String(wifiConnected ? "success" : "error") + R"rawliteral(">
      <strong>WiFi:</strong> )rawliteral" + String(wifiConnected ? "Conectado" : "Desconectado") + R"rawliteral(
    </div>
    
    <div class="status-item )rawliteral" + String(mqttConnected ? "success" : "error") + R"rawliteral(">
      <strong>MQTT:</strong> )rawliteral" + String(mqttConnected ? "Conectado" : "Desconectado") + R"rawliteral(
    </div>
    
    <div class="status-item success">
      <strong>BLE:</strong> )rawliteral" + String(bleActive ? "Ativo" : "Inativo") + R"rawliteral(
    </div>
    
    <div class="status-item">
      <strong>pH:</strong> )rawliteral" + String(currentData.ph, 2) + R"rawliteral(<br>
      <strong>EC:</strong> )rawliteral" + String(currentData.ec, 0) + R"rawliteral( µS/cm
    </div>
  </div>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// ==================== SEÇÃO 10: NTP ====================
void syncNTP() {
  if (!wifiConnected) return;
  
  logMessage(LOG_INFO, "Sincronizando NTP...");
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    logMessage(LOG_INFO, "✅ NTP sincronizado: " + String(timeStr));
  }
}

// ==================== SEÇÃO 11: MQTT ====================
void setupMQTT() {
  espClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  
  logMessage(LOG_INFO, "MQTT configurado");
}

bool reconnectMQTT() {
  if (!wifiConnected) return false;
  if (millis() - lastMqttAttempt < 5000) return false;
  
  lastMqttAttempt = millis();
  
  String clientId = "aquasys-sensor-" + deviceUUID;
  logMessage(LOG_INFO, "Conectando MQTT...");
  
  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
    mqttConnected = true;
    logMessage(LOG_INFO, "✅ MQTT conectado!");
    
    mqttClient.subscribe(TOPIC_CALIBRATION);
    
    return true;
  } else {
    mqttConnected = false;
    logMessage(LOG_ERROR, "Falha MQTT, rc=" + String(mqttClient.state()));
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  logMessage(LOG_DEBUG, "MQTT recebido [" + String(topic) + "]: " + message);
  
  // Processar calibração remota
  if (strcmp(topic, TOPIC_CALIBRATION) == 0) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      if (doc.containsKey("ph_slope")) {
        calibration.ph_slope = doc["ph_slope"];
        calibration.ph_intercept = doc["ph_intercept"];
        saveCalibration();
        logMessage(LOG_INFO, "Calibração atualizada via MQTT");
      }
    }
  }
}

void publishSensorData() {
  if (!mqttConnected) return;
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = millis();
  doc["ph"] = currentData.ph;
  doc["ec"] = currentData.ec;
  doc["air_temp"] = currentData.air_temp;
  doc["humidity"] = currentData.humidity;
  doc["water_temp"] = currentData.water_temp;
  doc["valid"] = currentData.valid;
  
  String message;
  serializeJson(doc, message);
  
  mqttClient.publish(TOPIC_SENSORS, message.c_str());
  logMessage(LOG_DEBUG, "Dados publicados via MQTT");
}

void publishHeartbeat() {
  if (!mqttConnected) return;
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ble_active"] = bleActive;
  doc["ble_connected"] = deviceConnectedBLE;
  
  String message;
  serializeJson(doc, message);
  
  mqttClient.publish(TOPIC_HEARTBEAT, message.c_str());
  logMessage(LOG_DEBUG, "Heartbeat publicado");
}

// ==================== SEÇÃO 12: BLE ====================
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnectedBLE = true;
    logMessage(LOG_INFO, "✅ Cliente BLE conectado");
  }
  
  void onDisconnect(BLEServer* pServer) {
    deviceConnectedBLE = false;
    logMessage(LOG_INFO, "Cliente BLE desconectado");
    pServer->startAdvertising(); // Reiniciar anúncio
  }
};

void setupBLE() {
  if (bleActive) return;
  
  logMessage(LOG_INFO, "Inicializando BLE Server...");
  
  BLEDevice::init("AquaSys-Sensor");
  pBLEServer = BLEDevice::createServer();
  pBLEServer->setCallbacks(new ServerCallbacks());
  
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
  
  pService->start();
  
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  bleActive = true;
  logMessage(LOG_INFO, "✅ BLE Server ativo");
}

void publishDataToBLE() {
  if (!bleActive || !deviceConnectedBLE) return;
  
  pCharPH->setValue(currentData.ph);
  pCharPH->notify();
  
  pCharEC->setValue(currentData.ec);
  pCharEC->notify();
  
  pCharAirTemp->setValue(currentData.air_temp);
  pCharAirTemp->notify();
  
  pCharHumidity->setValue(currentData.humidity);
  pCharHumidity->notify();
  
  pCharWaterTemp->setValue(currentData.water_temp);
  pCharWaterTemp->notify();
  
  logMessage(LOG_DEBUG, "Dados publicados via BLE");
}

// ==================== SEÇÃO 13: SENSORES ====================
void loadCalibration() {
  prefs.begin("calibration", true);
  
  calibration.ph_slope = prefs.getFloat("ph_slope", 1.0);
  calibration.ph_intercept = prefs.getFloat("ph_int", 0.0);
  calibration.ec_k = prefs.getFloat("ec_k", 1.0);
  
  prefs.end();
  
  logMessage(LOG_INFO, "Calibração carregada: pH slope=" + String(calibration.ph_slope, 4));
}

void saveCalibration() {
  prefs.begin("calibration", false);
  
  prefs.putFloat("ph_slope", calibration.ph_slope);
  prefs.putFloat("ph_int", calibration.ph_intercept);
  prefs.putFloat("ec_k", calibration.ec_k);
  
  prefs.end();
  
  logMessage(LOG_INFO, "Calibração salva");
}

float readPH() {
  int raw = analogRead(PIN_PH);
  float voltage = raw * (3.3 / 4095.0);
  float ph = calibration.ph_slope * voltage + calibration.ph_intercept;
  
  return constrain(ph, 0.0, 14.0);
}

float readEC(float waterTemp) {
  int raw = analogRead(PIN_EC);
  float voltage = raw * (3.3 / 4095.0);
  float ec = voltage * 1000.0 * calibration.ec_k; // µS/cm
  
  // Compensação temperatura (2%/°C)
  float tempCompensated = ec / (1.0 + 0.02 * (waterTemp - 25.0));
  
  return constrain(tempCompensated, 0.0, 5000.0);
}

void readSensors() {
  // Ler DHT22
  currentData.air_temp = dht.readTemperature();
  currentData.humidity = dht.readHumidity();
  
  // Ler DS18B20
  ds18b20.requestTemperatures();
  currentData.water_temp = ds18b20.getTempCByIndex(0);
  
  // Ler pH e EC
  currentData.ph = readPH();
  currentData.ec = readEC(currentData.water_temp);
  
  // Validar
  currentData.valid = !isnan(currentData.air_temp) && 
                      !isnan(currentData.humidity) &&
                      currentData.water_temp > -50.0 &&
                      currentData.ph > 0.0 && currentData.ph < 14.0;
  
  currentData.timestamp = millis();
  
  logMessage(LOG_INFO, "Leitura: pH=" + String(currentData.ph, 2) + 
             " EC=" + String(currentData.ec, 0) + 
             " Temp=" + String(currentData.water_temp, 1) + "°C");
}

// ==================== SEÇÃO 14: WATCHDOG ====================
void initWatchdog() {
  esp_err_t wdt_status = esp_task_wdt_status(NULL);
  
  if (wdt_status == ESP_ERR_NOT_FOUND) {
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WATCHDOG_TIMEOUT * 1000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    logMessage(LOG_INFO, "Watchdog inicializado (" + String(WATCHDOG_TIMEOUT) + "s)");
  } else {
    logMessage(LOG_INFO, "Watchdog já inicializado");
  }
  
  esp_task_wdt_add(NULL);
}

void resetWatchdog() {
  if (millis() - lastWdtReset > 10000) {
    esp_task_wdt_reset();
    lastWdtReset = millis();
  }
}

// ==================== SEÇÃO 15: SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  logMessage(LOG_INFO, "===========================================");
  logMessage(LOG_INFO, "AquaSys Nexus Sensor v" + String(FIRMWARE_VERSION));
  logMessage(LOG_INFO, "===========================================");
  
  deviceUUID = generateDeviceUUID();
  logMessage(LOG_INFO, "Device UUID: " + deviceUUID);
  
  initWatchdog();
  
  // Inicializar sensores
  dht.begin();
  ds18b20.begin();
  
  loadWiFiConfig();
  loadCalibration();
  
  // Inicializar BLE (sempre ativo)
  setupBLE();
  
  // Tentar WiFi
  if (!connectWiFi()) {
    logMessage(LOG_WARN, "WiFi falhou, iniciando modo AP");
    startAPMode();
  } else {
    setupMQTT();
  }
  
  logMessage(LOG_INFO, "Setup concluído!");
  logMessage(LOG_INFO, "Memória livre: " + String(ESP.getFreeHeap()) + " bytes");
}

// ==================== SEÇÃO 16: LOOP ====================
void loop() {
  resetWatchdog();
  
  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (millis() - apModeStartTime > AP_TIMEOUT) {
      stopAPMode();
      if (!connectWiFi()) {
        startAPMode();
      }
    }
  } else {
    checkWiFi();
    
    if (wifiConnected) {
      if (!mqttConnected) {
        reconnectMQTT();
      } else {
        mqttClient.loop();
      }
    }
    
    // Ler sensores periodicamente
    if (millis() - lastSensorRead > SENSOR_READ_INTERVAL) {
      readSensors();
      
      // Publicar via MQTT (se conectado)
      if (mqttConnected) {
        publishSensorData();
      }
      
      // Publicar via BLE (se cliente conectado)
      if (deviceConnectedBLE) {
        publishDataToBLE();
      }
      
      lastSensorRead = millis();
    }
    
    // Heartbeat
    if (mqttConnected && millis() - lastHeartbeat > 60000) {
      publishHeartbeat();
      lastHeartbeat = millis();
    }
  }
  
  delay(100);
}
