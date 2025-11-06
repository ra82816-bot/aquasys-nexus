/*
 * ============================================================================
 * AquaSys Nexus - Sensor Module v4.3.1-COMPLETE
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
 * ✅ Autenticação dinâmica via Supabase
 * ✅ TLS otimizado com timeout adequado
 * ✅ Heartbeat estruturado
 * 
 * AUTOR: HydroSmart Team
 * DATA: 2025-01-11
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

// ==================== SEÇÃO 2: CONFIGURAÇÕES ====================
// Versão do Firmware
#define FIRMWARE_VERSION "4.3.1-COMPLETE"
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
#define HEARTBEAT_INTERVAL 60000    // 60s
#define WATCHDOG_TIMEOUT 60         // 60s
#define AUTH_TIMEOUT 10000          // 10s

// ✅ API Supabase (autenticação dinâmica)
#define SUPABASE_URL "https://oaabtbvwxsjomeeizciq.supabase.co"
#define SUPABASE_ANON_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw"

// MQTT Configuration (fallback - será substituído por credenciais dinâmicas)
#define MQTT_BROKER_FALLBACK "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883

// MQTT Topics (fallback)
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

// ✅ NOVO: Estrutura para credenciais MQTT dinâmicas
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
unsigned long lastMqttSuccess = 0;
MqttCredentials mqttCreds = {"", "", "", "", "", "", "", false};
bool isAuthenticated = false;

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
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AquaSys - Status</title>
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
    .status-item strong { color: #333; }
    .success { border-left-color: #28a745; }
    .warning { border-left-color: #ffc107; }
    .error { border-left-color: #dc3545; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📊 Status do Sistema</h1>
    
    <div class="status-item )rawliteral" + String(wifiConnected ? "success" : "error") + R"rawliteral(">
      <strong>WiFi:</strong> )rawliteral" + String(wifiConnected ? "Conectado" : "Desconectado") + R"rawliteral(<br>
      )rawliteral" + (wifiConnected ? "SSID: " + String(networks[currentNetworkIndex].ssid) : "") + R"rawliteral(
    </div>
    
    <div class="status-item )rawliteral" + String(mqttConnected ? "success" : "warning") + R"rawliteral(">
      <strong>MQTT:</strong> )rawliteral" + String(mqttConnected ? "Conectado" : "Desconectado") + R"rawliteral(
    </div>
    
    <div class="status-item )rawliteral" + String(bleActive ? "success" : "warning") + R"rawliteral(">
      <strong>BLE:</strong> )rawliteral" + String(bleActive ? "Ativo" : "Inativo") + R"rawliteral(
    </div>
    
    <div class="status-item">
      <strong>Uptime:</strong> )rawliteral" + String(millis() / 1000) + R"rawliteral( segundos
    </div>
    
    <div class="status-item">
      <strong>Memória Livre:</strong> )rawliteral" + String(ESP.getFreeHeap()) + R"rawliteral( bytes
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
  resetWatchdog();
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    logMessage(LOG_INFO, "✅ NTP sincronizado: " + String(timeStr));
  } else {
    logMessage(LOG_WARN, "Falha ao sincronizar NTP");
  }
}

// ==================== SEÇÃO 11: AUTENTICAÇÃO ====================
bool authenticateDevice() {
  if (!wifiConnected) {
    logMessage(LOG_ERROR, "❌ Autenticação requer WiFi conectado");
    return false;
  }
  
  logMessage(LOG_INFO, "🔐 Iniciando autenticação dinâmica...");
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
  
  logMessage(LOG_INFO, "📤 Enviando: " + requestBody);
  
  http.setTimeout(AUTH_TIMEOUT);
  int httpCode = http.POST(requestBody);
  
  logMessage(LOG_INFO, "📡 HTTP Code: " + String(httpCode));
  
  if (httpCode == 200) {
    String response = http.getString();
    logMessage(LOG_INFO, "📥 Resposta recebida (" + String(response.length()) + " bytes)");
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
      logMessage(LOG_ERROR, "❌ Erro ao parsear resposta: " + String(error.c_str()));
      http.end();
      return false;
    }
    
    if (doc["success"]) {
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
      
      logMessage(LOG_INFO, "✅ Autenticação bem-sucedida!");
      logMessage(LOG_INFO, "   Broker: " + String(mqttCreds.broker));
      logMessage(LOG_INFO, "   Username: " + String(mqttCreds.username));
      
      http.end();
      return true;
    } else {
      logMessage(LOG_ERROR, "❌ Autenticação falhou: " + String(doc["error"] | "unknown"));
      http.end();
      return false;
    }
  } else if (httpCode == 404) {
    logMessage(LOG_ERROR, "❌ Dispositivo não registrado. Registre via web app primeiro.");
  } else if (httpCode == 429) {
    logMessage(LOG_ERROR, "❌ Rate limit excedido. Aguarde e tente novamente.");
  } else {
    logMessage(LOG_ERROR, "❌ Erro HTTP: " + String(httpCode));
  }
  
  http.end();
  return false;
}

void loadMqttCredentials() {
  prefs.begin("mqtt_creds", true);
  
  String broker = prefs.getString("broker", "");
  String username = prefs.getString("username", "");
  String password = prefs.getString("password", "");
  
  if (broker.length() > 0 && username.length() > 0) {
    strncpy(mqttCreds.broker, broker.c_str(), sizeof(mqttCreds.broker) - 1);
    strncpy(mqttCreds.username, username.c_str(), sizeof(mqttCreds.username) - 1);
    strncpy(mqttCreds.password, password.c_str(), sizeof(mqttCreds.password) - 1);
    
    String topic_sensors = prefs.getString("topic_sensors", "");
    String topic_heartbeat = prefs.getString("topic_hb", "");
    
    if (topic_sensors.length() > 0) {
      strncpy(mqttCreds.topic_sensors, topic_sensors.c_str(), sizeof(mqttCreds.topic_sensors) - 1);
      strncpy(mqttCreds.topic_heartbeat, topic_heartbeat.c_str(), sizeof(mqttCreds.topic_heartbeat) - 1);
      
      mqttCreds.valid = true;
      logMessage(LOG_INFO, "✅ Credenciais MQTT carregadas da NVS");
    }
  } else {
    logMessage(LOG_WARN, "⚠️ Nenhuma credencial salva, usando fallback");
    strncpy(mqttCreds.broker, MQTT_BROKER_FALLBACK, sizeof(mqttCreds.broker) - 1);
    mqttCreds.valid = false;
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
  
  logMessage(LOG_INFO, "💾 Credenciais MQTT salvas na NVS");
}

// ==================== SEÇÃO 12: MQTT ====================
void setupMQTT() {
  uint32_t freeHeap = ESP.getFreeHeap();
  logMessage(LOG_INFO, "Heap livre: " + String(freeHeap) + " bytes (" + String(freeHeap/1024) + " KB)");
  
  if (freeHeap < 50000) {
    logMessage(LOG_ERROR, "⚠️ Heap insuficiente para TLS! Necessário: 50KB, Disponível: " + String(freeHeap/1024) + "KB");
    return;
  }
  
  // ✅ Configurar WiFiClientSecure otimizado
  espClient.setInsecure();
  espClient.setTimeout(20000); // 20s para handshake TLS
  
  const char* brokerToUse = mqttCreds.valid ? mqttCreds.broker : MQTT_BROKER_FALLBACK;
  
  mqttClient.setServer(brokerToUse, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(60);
  
  logMessage(LOG_INFO, "✅ MQTT configurado: " + String(brokerToUse) + ":" + String(MQTT_PORT));
  logMessage(LOG_INFO, "   Modo: " + String(mqttCreds.valid ? "Autenticado" : "Fallback"));
  logMessage(LOG_INFO, "TLS Timeout configurado: 20s");
}

bool reconnectMQTT() {
  if (!wifiConnected) return false;
  if (millis() - lastMqttAttempt < 5000) return false;
  
  lastMqttAttempt = millis();
  resetWatchdog();
  
  uint32_t heapBefore = ESP.getFreeHeap();
  logMessage(LOG_INFO, "Heap antes de MQTT: " + String(heapBefore) + " bytes (" + String(heapBefore/1024) + " KB)");
  
  const char* clientIdToUse = mqttCreds.valid ? mqttCreds.client_id : ("aquasys-sensor-" + deviceUUID).c_str();
  const char* usernameToUse = mqttCreds.valid ? mqttCreds.username : deviceUUID.c_str();
  const char* passwordToUse = mqttCreds.valid ? mqttCreds.password : "";
  
  logMessage(LOG_INFO, "Conectando MQTT como: " + String(clientIdToUse));
  logMessage(LOG_INFO, "Username: " + String(usernameToUse));
  logMessage(LOG_INFO, "Modo: " + String(mqttCreds.valid ? "Autenticado" : "Fallback"));
  
  bool connected = mqttClient.connect(clientIdToUse, usernameToUse, passwordToUse);
  
  uint32_t heapAfter = ESP.getFreeHeap();
  int32_t heapDelta = heapBefore - heapAfter;
  logMessage(LOG_INFO, "Heap após MQTT: " + String(heapAfter) + " bytes (Delta: " + String(heapDelta) + " bytes)");
  
  if (connected) {
    mqttConnected = true;
    lastMqttSuccess = millis();
    logMessage(LOG_INFO, "✅ MQTT conectado com sucesso!");
    
    const char* topicCalibration = mqttCreds.valid ? mqttCreds.topic_calibration : TOPIC_CALIBRATION_FALLBACK;
    
    bool sub1 = mqttClient.subscribe(topicCalibration, 1);
    
    logMessage(LOG_INFO, "📡 Inscrevendo em tópicos:");
    logMessage(LOG_INFO, "  • " + String(topicCalibration) + (sub1 ? " ✅" : " ❌"));
    
    publishSensorData();
    return true;
  } else {
    mqttConnected = false;
    int state = mqttClient.state();
    logMessage(LOG_ERROR, "❌ Falha MQTT, rc=" + String(state));
    
    switch(state) {
      case -4: 
        logMessage(LOG_ERROR, "Erro: TIMEOUT na conexão"); 
        break;
      case -3: 
        logMessage(LOG_ERROR, "Erro: CONEXÃO PERDIDA"); 
        break;
      case -2: 
        logMessage(LOG_ERROR, "Erro: FALHA NA CONEXÃO DE REDE (TLS handshake falhou)"); 
        logMessage(LOG_ERROR, "Possíveis causas: heap baixo, timeout curto, firewall bloqueando 8883");
        break;
      case -1: 
        logMessage(LOG_ERROR, "Erro: DESCONECTADO"); 
        break;
      case 1: 
        logMessage(LOG_ERROR, "Erro: PROTOCOLO INCORRETO"); 
        break;
      case 2: 
        logMessage(LOG_ERROR, "Erro: ID REJEITADO"); 
        break;
      case 3: 
        logMessage(LOG_ERROR, "Erro: SERVIDOR INDISPONÍVEL"); 
        break;
      case 4: 
        logMessage(LOG_ERROR, "Erro: CREDENCIAIS INVÁLIDAS"); 
        break;
      case 5: 
        logMessage(LOG_ERROR, "Erro: NÃO AUTORIZADO"); 
        break;
      default:
        logMessage(LOG_ERROR, "Erro desconhecido: " + String(state));
        break;
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
  
  logMessage(LOG_INFO, "📩 MQTT recebido!");
  logMessage(LOG_INFO, "   Tópico: " + String(topic));
  logMessage(LOG_INFO, "   Payload (" + String(length) + " bytes): " + message);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    logMessage(LOG_ERROR, "❌ Falha ao parsear JSON: " + String(error.c_str()));
    return;
  }
  
  const char* topicCalibration = mqttCreds.valid ? mqttCreds.topic_calibration : TOPIC_CALIBRATION_FALLBACK;
  
  if (strcmp(topic, topicCalibration) == 0) {
    logMessage(LOG_INFO, "🔬 Comando de calibração recebido!");
    
    String sensorType = doc["sensor_type"] | "";
    String calibType = doc["calibration_type"] | "";
    
    logMessage(LOG_INFO, "  Sensor: " + sensorType);
    logMessage(LOG_INFO, "  Tipo: " + calibType);
    
    if (sensorType == "ph") {
      if (calibType == "4.0") {
        calibration.ph_cal_4 = doc["value"] | 4.0;
      } else if (calibType == "7.0") {
        calibration.ph_cal_7 = doc["value"] | 7.0;
      }
      saveCalibration();
      logMessage(LOG_INFO, "✅ Calibração pH salva");
    } else if (sensorType == "ec") {
      if (calibType == "1413") {
        calibration.ec_cal_1413 = doc["value"] | 1413.0;
      }
      saveCalibration();
      logMessage(LOG_INFO, "✅ Calibração EC salva");
    }
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
    logMessage(LOG_INFO, "📤 Dados sensores publicados: " + payload);
  } else {
    logMessage(LOG_ERROR, "❌ Falha ao publicar dados sensores");
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
  
  // Status de conexão
  JsonObject status = doc.createNestedObject("status");
  status["wifi_connected"] = wifiConnected;
  status["mqtt_connected"] = mqttConnected;
  status["ble_active"] = bleActive;
  status["rssi"] = WiFi.RSSI();
  
  // Memória
  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = ESP.getFreeHeap();
  memory["min_free_heap"] = ESP.getMinFreeHeap();
  memory["heap_size"] = ESP.getHeapSize();
  
  // Uptime
  doc["uptime_seconds"] = millis() / 1000;
  
  // Dados atuais
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
    logMessage(LOG_INFO, "💓 Heartbeat publicado (" + String(payload.length()) + " bytes)");
  } else {
    logMessage(LOG_ERROR, "❌ Falha ao publicar heartbeat");
  }
}

// ==================== SEÇÃO 13: BLE ====================
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnectedBLE = true;
    logMessage(LOG_INFO, "📱 Cliente BLE conectado");
  }
  
  void onDisconnect(BLEServer* pServer) {
    deviceConnectedBLE = false;
    logMessage(LOG_INFO, "📱 Cliente BLE desconectado");
    pServer->startAdvertising();
  }
};

void setupBLE() {
  logMessage(LOG_INFO, "Inicializando BLE Server...");
  
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
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  bleActive = true;
  logMessage(LOG_INFO, "✅ BLE Server ativo: " + deviceUUID);
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
  
  logMessage(LOG_DEBUG, "📡 Dados publicados via BLE");
}

// ==================== SEÇÃO 14: SENSORES ====================
void loadCalibration() {
  prefs.begin("calibration", true);
  
  calibration.ph_slope = prefs.getFloat("ph_slope", 1.0);
  calibration.ph_intercept = prefs.getFloat("ph_intercept", 0.0);
  calibration.ec_k = prefs.getFloat("ec_k", 1.0);
  calibration.ph_cal_4 = prefs.getFloat("ph_cal_4", 4.0);
  calibration.ph_cal_7 = prefs.getFloat("ph_cal_7", 7.0);
  calibration.ec_cal_1413 = prefs.getFloat("ec_cal_1413", 1413.0);
  
  prefs.end();
  
  logMessage(LOG_INFO, "Calibração carregada");
}

void saveCalibration() {
  prefs.begin("calibration", false);
  
  prefs.putFloat("ph_slope", calibration.ph_slope);
  prefs.putFloat("ph_intercept", calibration.ph_intercept);
  prefs.putFloat("ec_k", calibration.ec_k);
  prefs.putFloat("ph_cal_4", calibration.ph_cal_4);
  prefs.putFloat("ph_cal_7", calibration.ph_cal_7);
  prefs.putFloat("ec_cal_1413", calibration.ec_cal_1413);
  
  prefs.end();
  
  logMessage(LOG_INFO, "Calibração salva");
}

float readPH() {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PIN_PH);
    delay(10);
  }
  float avgValue = sum / 10.0;
  float voltage = avgValue * (3.3 / 4095.0);
  
  float ph = calibration.ph_slope * voltage + calibration.ph_intercept;
  
  if (ph < 0) ph = 0;
  if (ph > 14) ph = 14;
  
  return ph;
}

float readEC(float waterTemp) {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PIN_EC);
    delay(10);
  }
  float avgValue = sum / 10.0;
  float voltage = avgValue * (3.3 / 4095.0);
  
  float ec = voltage * calibration.ec_k * 1000.0;
  
  float tempCoef = 1.0 + 0.02 * (waterTemp - 25.0);
  ec = ec / tempCoef;
  
  if (ec < 0) ec = 0;
  
  return ec;
}

void readSensors() {
  resetWatchdog();
  
  // Temperatura e umidade do ar
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    logMessage(LOG_WARN, "Falha ao ler DHT22");
    currentData.air_temp = 0;
    currentData.humidity = 0;
  } else {
    currentData.air_temp = t;
    currentData.humidity = h;
  }
  
  // Temperatura da água
  ds18b20.requestTemperatures();
  float waterTemp = ds18b20.getTempCByIndex(0);
  
  if (waterTemp == DEVICE_DISCONNECTED_C || waterTemp < -50 || waterTemp > 100) {
    logMessage(LOG_WARN, "Falha ao ler DS18B20");
    currentData.water_temp = 25.0; // Fallback
  } else {
    currentData.water_temp = waterTemp;
  }
  
  // pH
  currentData.ph = readPH();
  
  // EC
  currentData.ec = readEC(currentData.water_temp);
  
  currentData.valid = true;
  currentData.timestamp = millis();
  
  logMessage(LOG_INFO, "📊 Leitura: pH=" + String(currentData.ph, 2) + 
             " EC=" + String(currentData.ec, 0) + 
             " TempAr=" + String(currentData.air_temp, 1) + 
             " Umid=" + String(currentData.humidity, 1) + 
             " TempH2O=" + String(currentData.water_temp, 1));
}

// ==================== SEÇÃO 15: WATCHDOG ====================
void initWatchdog() {
  // Verificar se watchdog já foi inicializado (ESP32 Core 3.x)
  esp_task_wdt_status_t status = esp_task_wdt_status(NULL);
  
  if (status == ESP_ERR_NOT_FOUND) {
    // Watchdog não está inicializado, inicializar agora
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WATCHDOG_TIMEOUT * 1000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
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

// ==================== SEÇÃO 16: SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   AquaSys Nexus - Módulo Sensor       ║");
  Serial.println("║   Versão: " + String(FIRMWARE_VERSION) + "              ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  
  deviceUUID = generateDeviceUUID();
  logMessage(LOG_INFO, "🆔 UUID: " + deviceUUID);
  
  initWatchdog();
  
  dht.begin();
  ds18b20.begin();
  
  loadCalibration();
  loadWiFiConfig();
  loadMqttCredentials();
  
  setupBLE();
  
  if (!connectWiFi()) {
    logMessage(LOG_WARN, "Falha ao conectar WiFi, iniciando AP...");
    startAPMode();
  } else {
    if (!isAuthenticated) {
      authenticateDevice();
    }
    setupMQTT();
  }
  
  logMessage(LOG_INFO, "✅ Setup completo!");
  logMessage(LOG_INFO, "Memória livre: " + String(ESP.getFreeHeap()) + " bytes");
}

// ==================== SEÇÃO 17: LOOP ====================
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
  
  delay(100);
}
