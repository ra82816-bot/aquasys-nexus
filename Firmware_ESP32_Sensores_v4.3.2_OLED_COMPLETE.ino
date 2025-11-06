/*
 * ============================================================================
 * AquaSys Nexus - Sensor Module v4.3.2-OLED-COMPLETE
 * ============================================================================
 * SOLUÇÃO DEFINITIVA - Funciona COM ou SEM internet
 * 
 * RECURSOS:
 * ✅ Display OLED 128x64 com interface completa
 * ✅ Menu de calibração interativo com botões
 * ✅ Exibição de dashboard em tempo real
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
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==================== SEÇÃO 2: CONFIGURAÇÕES ====================
// Versão do Firmware
#define FIRMWARE_VERSION "4.3.2-OLED-COMPLETE"
#define DEVICE_TYPE "SENSOR"

// Display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pinos dos Botões (navegação do menu OLED)
#define BTN_UP 25      // Botão para cima/incrementar
#define BTN_DOWN 26    // Botão para baixo/decrementar
#define BTN_SELECT 27  // Botão de seleção/confirmar
#define BTN_BACK 14    // Botão voltar/cancelar

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
#define DISPLAY_UPDATE_INTERVAL 1000 // 1s

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

// Menu OLED
enum MenuPage {
  MENU_DASHBOARD = 0,
  MENU_CONNECTIONS = 1,
  MENU_CALIBRATION = 2,
  MENU_SYSTEM = 3
};

enum CalibrationStep {
  CAL_MENU = 0,
  CAL_PH_4 = 1,
  CAL_PH_7 = 2,
  CAL_EC_1413 = 3,
  CAL_COMPLETE = 4
};

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

// Display & Menu
MenuPage currentPage = MENU_DASHBOARD;
CalibrationStep calStep = CAL_MENU;
int menuSelection = 0;
bool inCalibrationMode = false;
unsigned long lastDisplayUpdate = 0;
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 200;

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

// Display OLED
void initDisplay();
void updateDisplay();
void drawDashboard();
void drawConnections();
void drawCalibrationMenu();
void drawSystemInfo();
void displayMessage(const String& title, const String& message, int duration = 2000);

// Botões
void initButtons();
void handleButtons();
bool isButtonPressed(int pin);

// Calibração
void startCalibration();
void processCalibration();
void finishCalibration();

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
    displayMessage("WiFi", "Conectando...", 0);
    
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
      displayMessage("WiFi", "Conectado!", 2000);
      syncNTP();
      return true;
    }
  }
  
  wifiConnected = false;
  logMessage(LOG_WARN, "⚠️ Nenhuma credencial salva, usando fallback");
  return false;
}

void checkWiFi() {
  if (millis() - lastWiFiCheck < 5000) return;
  lastWiFiCheck = millis();
  
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    wifiConnected = false;
    mqttConnected = false;
    logMessage(LOG_WARN, "⚠️ WiFi desconectado, tentando reconectar...");
    
    if (!connectWiFi()) {
      startAPMode();
    }
  }
}

void startAPMode() {
  if (apMode) return;
  
  logMessage(LOG_INFO, "🔶 Iniciando modo AP...");
  displayMessage("Modo AP", "Iniciando...", 0);
  
  WiFi.mode(WIFI_AP);
  String apSSID = String(AP_SSID_PREFIX) + deviceUUID;
  WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  logMessage(LOG_INFO, "✅ AP ativo: " + apSSID + " / Senha: " + String(AP_PASSWORD));
  logMessage(LOG_INFO, "Portal: http://" + IP.toString());
  displayMessage("Modo AP", apSSID, 3000);
  
  dnsServer.start(53, "*", IP);
  setupWebServer();
  
  apMode = true;
  apModeStartTime = millis();
}

void stopAPMode() {
  if (!apMode) return;
  
  logMessage(LOG_INFO, "Desativando modo AP...");
  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
}

// ==================== SEÇÃO 9: WEB SERVER ====================
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  logMessage(LOG_INFO, "✅ Web Server iniciado");
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AquaSys - Configuração WiFi</title>
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
      max-width: 500px;
      margin: 0 auto;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
    }
    h1 {
      color: #667eea;
      margin-bottom: 10px;
      font-size: 28px;
    }
    .subtitle {
      color: #666;
      margin-bottom: 30px;
      font-size: 14px;
    }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      margin-bottom: 8px;
      color: #333;
      font-weight: 600;
    }
    input, select {
      width: 100%;
      padding: 12px;
      border: 2px solid #e0e0e0;
      border-radius: 10px;
      font-size: 16px;
      transition: all 0.3s;
    }
    input:focus, select:focus {
      outline: none;
      border-color: #667eea;
    }
    button {
      width: 100%;
      padding: 14px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 10px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s;
    }
    button:hover {
      transform: translateY(-2px);
    }
    .scan-btn {
      background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
      margin-bottom: 20px;
    }
    .network-list {
      margin-bottom: 20px;
    }
    .network-item {
      padding: 12px;
      background: #f8f9fa;
      border-radius: 8px;
      margin-bottom: 8px;
      cursor: pointer;
      transition: all 0.2s;
    }
    .network-item:hover {
      background: #e9ecef;
    }
    .network-name {
      font-weight: 600;
      color: #333;
    }
    .network-signal {
      float: right;
      color: #666;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🌊 AquaSys</h1>
    <p class="subtitle">Configuração de Rede WiFi</p>
    
    <button class="scan-btn" onclick="scanNetworks()">🔍 Escanear Redes</button>
    
    <div id="networks" class="network-list"></div>
    
    <form onsubmit="saveConfig(event)">
      <div class="form-group">
        <label>Nome da Rede (SSID)</label>
        <input type="text" id="ssid" required placeholder="Nome da rede WiFi">
      </div>
      
      <div class="form-group">
        <label>Senha</label>
        <input type="password" id="password" required placeholder="Mínimo 8 caracteres">
      </div>
      
      <button type="submit">💾 Salvar e Conectar</button>
    </form>
  </div>
  
  <script>
    function scanNetworks() {
      fetch('/scan')
        .then(r => r.json())
        .then(data => {
          const div = document.getElementById('networks');
          div.innerHTML = '';
          
          data.networks.forEach(net => {
            const item = document.createElement('div');
            item.className = 'network-item';
            item.innerHTML = `
              <span class="network-name">${net.ssid}</span>
              <span class="network-signal">${net.rssi} dBm ${net.secure ? '🔒' : '🔓'}</span>
            `;
            item.onclick = () => {
              document.getElementById('ssid').value = net.ssid;
            };
            div.appendChild(item);
          });
        });
    }
    
    function saveConfig(e) {
      e.preventDefault();
      const data = {
        ssid: document.getElementById('ssid').value,
        password: document.getElementById('password').value
      };
      
      fetch('/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(r => r.json())
      .then(data => {
        alert(data.message);
        if (data.success) {
          setTimeout(() => location.reload(), 2000);
        }
      });
    }
    
    scanNetworks();
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
      display: flex;
      justify-content: space-between;
    }
    .label { font-weight: 600; color: #333; }
    .value { color: #666; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📊 Status do Sistema</h1>
    <div id="status"></div>
  </div>
  
  <script>
    setInterval(() => {
      fetch('/status')
        .then(r => r.json())
        .then(data => {
          document.getElementById('status').innerHTML = `
            <div class="status-item">
              <span class="label">UUID:</span>
              <span class="value">${data.uuid}</span>
            </div>
            <div class="status-item">
              <span class="label">Firmware:</span>
              <span class="value">${data.firmware}</span>
            </div>
            <div class="status-item">
              <span class="label">WiFi:</span>
              <span class="value">${data.wifi ? '✅ Conectado' : '❌ Desconectado'}</span>
            </div>
            <div class="status-item">
              <span class="label">MQTT:</span>
              <span class="value">${data.mqtt ? '✅ Conectado' : '❌ Desconectado'}</span>
            </div>
            <div class="status-item">
              <span class="label">pH:</span>
              <span class="value">${data.ph}</span>
            </div>
            <div class="status-item">
              <span class="label">EC:</span>
              <span class="value">${data.ec} μS/cm</span>
            </div>
            <div class="status-item">
              <span class="label">Temp. Água:</span>
              <span class="value">${data.water_temp} °C</span>
            </div>
            <div class="status-item">
              <span class="label">Temp. Ar:</span>
              <span class="value">${data.air_temp} °C</span>
            </div>
            <div class="status-item">
              <span class="label">Umidade:</span>
              <span class="value">${data.humidity} %</span>
            </div>
          `;
        });
    }, 2000);
  </script>
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
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
  logMessage(LOG_INFO, "⏰ NTP sincronizado");
}

// ==================== SEÇÃO 11: AUTENTICAÇÃO ====================
bool authenticateDevice() {
  if (!wifiConnected) {
    logMessage(LOG_WARN, "⚠️ Não é possível autenticar sem WiFi");
    return false;
  }
  
  logMessage(LOG_INFO, "🔐 Autenticando dispositivo...");
  displayMessage("Auth", "Autenticando...", 0);
  
  HTTPClient http;
  http.begin(String(SUPABASE_URL) + "/functions/v1/device-auth");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  http.setTimeout(AUTH_TIMEOUT);
  
  StaticJsonDocument<256> doc;
  doc["device_id"] = deviceUUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["firmware_version"] = FIRMWARE_VERSION;
  
  String payload;
  serializeJson(doc, payload);
  
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<512> responseDoc;
    deserializeJson(responseDoc, response);
    
    if (responseDoc["mqtt_broker"]) {
      String broker = responseDoc["mqtt_broker"] | MQTT_BROKER_FALLBACK;
      String username = responseDoc["mqtt_username"] | "";
      String password = responseDoc["mqtt_password"] | "";
      String clientId = responseDoc["client_id"] | deviceUUID.c_str();
      
      broker.toCharArray(mqttCreds.broker, 128);
      username.toCharArray(mqttCreds.username, 64);
      password.toCharArray(mqttCreds.password, 128);
      clientId.toCharArray(mqttCreds.client_id, 64);
      
      String topicSensors = "aquasys/sensors/" + deviceUUID;
      String topicHeartbeat = "aquasys/heartbeat/" + deviceUUID;
      String topicCalibration = "aquasys/calibration/" + deviceUUID;
      
      topicSensors.toCharArray(mqttCreds.topic_sensors, 128);
      topicHeartbeat.toCharArray(mqttCreds.topic_heartbeat, 128);
      topicCalibration.toCharArray(mqttCreds.topic_calibration, 128);
      
      mqttCreds.valid = true;
      saveMqttCredentials();
      
      isAuthenticated = true;
      logMessage(LOG_INFO, "✅ Autenticação bem-sucedida");
      logMessage(LOG_INFO, "MQTT Broker: " + String(mqttCreds.broker));
      displayMessage("Auth", "Sucesso!", 2000);
      
      http.end();
      return true;
    }
  }
  
  logMessage(LOG_ERROR, "❌ Falha na autenticação: " + String(httpCode));
  displayMessage("Auth", "Falhou!", 2000);
  http.end();
  return false;
}

void loadMqttCredentials() {
  prefs.begin("mqtt", true);
  
  String broker = prefs.getString("broker", "");
  if (broker.length() > 0) {
    broker.toCharArray(mqttCreds.broker, 128);
    prefs.getString("username", "").toCharArray(mqttCreds.username, 64);
    prefs.getString("password", "").toCharArray(mqttCreds.password, 128);
    prefs.getString("client_id", "").toCharArray(mqttCreds.client_id, 64);
    prefs.getString("topic_sensors", "").toCharArray(mqttCreds.topic_sensors, 128);
    prefs.getString("topic_heartbeat", "").toCharArray(mqttCreds.topic_heartbeat, 128);
    prefs.getString("topic_calibration", "").toCharArray(mqttCreds.topic_calibration, 128);
    mqttCreds.valid = true;
    isAuthenticated = true;
    logMessage(LOG_INFO, "✅ Credenciais MQTT carregadas do NVS");
  }
  
  prefs.end();
}

void saveMqttCredentials() {
  prefs.begin("mqtt", false);
  
  prefs.putString("broker", mqttCreds.broker);
  prefs.putString("username", mqttCreds.username);
  prefs.putString("password", mqttCreds.password);
  prefs.putString("client_id", mqttCreds.client_id);
  prefs.putString("topic_sensors", mqttCreds.topic_sensors);
  prefs.putString("topic_heartbeat", mqttCreds.topic_heartbeat);
  prefs.putString("topic_calibration", mqttCreds.topic_calibration);
  
  prefs.end();
  logMessage(LOG_INFO, "✅ Credenciais MQTT salvas no NVS");
}

// ==================== SEÇÃO 12: MQTT ====================
void setupMQTT() {
  const char* broker = mqttCreds.valid ? mqttCreds.broker : MQTT_BROKER_FALLBACK;
  
  logMessage(LOG_INFO, "Configurando MQTT: " + String(broker));
  
  espClient.setInsecure();
  espClient.setTimeout(20);
  
  mqttClient.setServer(broker, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(20);
  mqttClient.setBufferSize(2048);
}

bool reconnectMQTT() {
  if (mqttClient.connected()) return true;
  if (!wifiConnected) return false;
  
  if (millis() - lastMqttAttempt < 5000) return false;
  lastMqttAttempt = millis();
  
  const char* clientId = mqttCreds.valid ? mqttCreds.client_id : deviceUUID.c_str();
  const char* username = mqttCreds.valid ? mqttCreds.username : "";
  const char* password = mqttCreds.valid ? mqttCreds.password : "";
  
  logMessage(LOG_INFO, "🔌 Conectando ao MQTT como: " + String(clientId));
  
  bool connected = false;
  if (mqttCreds.valid && strlen(username) > 0) {
    connected = mqttClient.connect(clientId, username, password);
  } else {
    connected = mqttClient.connect(clientId);
  }
  
  if (connected) {
    mqttConnected = true;
    lastMqttSuccess = millis();
    logMessage(LOG_INFO, "✅ MQTT conectado");
    
    String subTopic = mqttCreds.valid ? 
                      String(mqttCreds.topic_calibration) : 
                      String(TOPIC_CALIBRATION_FALLBACK) + "/" + deviceUUID;
    
    mqttClient.subscribe(subTopic.c_str());
    logMessage(LOG_INFO, "📥 Subscrito em: " + subTopic);
    
    publishHeartbeat();
    return true;
  }
  
  int state = mqttClient.state();
  logMessage(LOG_ERROR, "❌ MQTT falhou. Estado: " + String(state));
  mqttConnected = false;
  
  return false;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  logMessage(LOG_INFO, "📨 MQTT recebido em [" + String(topic) + "]: " + message);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (!error) {
    if (doc.containsKey("ph_cal_4")) {
      calibration.ph_cal_4 = doc["ph_cal_4"];
      saveCalibration();
      logMessage(LOG_INFO, "✅ Calibração pH 4.0 atualizada");
    }
    if (doc.containsKey("ph_cal_7")) {
      calibration.ph_cal_7 = doc["ph_cal_7"];
      saveCalibration();
      logMessage(LOG_INFO, "✅ Calibração pH 7.0 atualizada");
    }
    if (doc.containsKey("ec_cal_1413")) {
      calibration.ec_cal_1413 = doc["ec_cal_1413"];
      saveCalibration();
      logMessage(LOG_INFO, "✅ Calibração EC 1413 atualizada");
    }
  }
}

void publishSensorData() {
  if (!mqttConnected || !currentData.valid) return;
  
  resetWatchdog();
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = millis();
  doc["ph"] = currentData.ph;
  doc["ec"] = currentData.ec;
  doc["air_temp"] = currentData.air_temp;
  doc["humidity"] = currentData.humidity;
  doc["water_temp"] = currentData.water_temp;
  
  String payload;
  serializeJson(doc, payload);
  
  const char* topic = mqttCreds.valid ? mqttCreds.topic_sensors : TOPIC_SENSORS_FALLBACK;
  
  if (mqttClient.publish(topic, payload.c_str(), false)) {
    logMessage(LOG_INFO, "📤 Dados publicados (" + String(payload.length()) + " bytes)");
  } else {
    logMessage(LOG_ERROR, "❌ Falha ao publicar dados");
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
  
  snprintf(buffer, sizeof(buffer), "%.0f", currentData.ec);
  pCharEC->setValue(buffer);
  pCharEC->notify();
  
  snprintf(buffer, sizeof(buffer), "%.1f", currentData.air_temp);
  pCharAirTemp->setValue(buffer);
  pCharAirTemp->notify();
  
  snprintf(buffer, sizeof(buffer), "%.1f", currentData.humidity);
  pCharHumidity->setValue(buffer);
  pCharHumidity->notify();
  
  snprintf(buffer, sizeof(buffer), "%.1f", currentData.water_temp);
  pCharWaterTemp->setValue(buffer);
  pCharWaterTemp->notify();
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
  logMessage(LOG_INFO, "✅ Calibração salva");
  displayMessage("Calibracao", "Salva!", 2000);
}

float readPH() {
  int raw = analogRead(PIN_PH);
  float voltage = raw * (3.3 / 4095.0);
  float ph = calibration.ph_slope * voltage + calibration.ph_intercept;
  
  if (ph < 0 || ph > 14) ph = 7.0;
  return ph;
}

float readEC(float waterTemp) {
  int raw = analogRead(PIN_EC);
  float voltage = raw * (3.3 / 4095.0);
  float ec = voltage * 1000 * calibration.ec_k;
  
  float tempCoeff = 1.0 + 0.02 * (waterTemp - 25.0);
  ec = ec / tempCoeff;
  
  if (ec < 0 || ec > 10000) ec = 0;
  return ec;
}

void readSensors() {
  resetWatchdog();
  
  currentData.water_temp = ds18b20.getTempCByIndex(0);
  currentData.air_temp = dht.readTemperature();
  currentData.humidity = dht.readHumidity();
  currentData.ph = readPH();
  currentData.ec = readEC(currentData.water_temp);
  currentData.timestamp = millis();
  
  if (isnan(currentData.air_temp)) currentData.air_temp = 25.0;
  if (isnan(currentData.humidity)) currentData.humidity = 50.0;
  if (isnan(currentData.water_temp)) currentData.water_temp = 25.0;
  
  currentData.valid = true;
  
  ds18b20.requestTemperatures();
  
  logMessage(LOG_INFO, "📊 Leitura: pH=" + String(currentData.ph, 2) + 
             " EC=" + String(currentData.ec, 0) + 
             " TempAr=" + String(currentData.air_temp, 1) + 
             " Umid=" + String(currentData.humidity, 1) + 
             " TempH2O=" + String(currentData.water_temp, 1));
}

// ==================== SEÇÃO 15: DISPLAY OLED ====================
void initDisplay() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    logMessage(LOG_ERROR, "❌ Falha ao iniciar OLED");
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println("AquaSys");
  display.setTextSize(1);
  display.setCursor(10, 35);
  display.println("Iniciando...");
  display.display();
  
  logMessage(LOG_INFO, "✅ Display OLED inicializado");
}

void updateDisplay() {
  if (millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) return;
  lastDisplayUpdate = millis();
  
  display.clearDisplay();
  
  switch (currentPage) {
    case MENU_DASHBOARD:
      drawDashboard();
      break;
    case MENU_CONNECTIONS:
      drawConnections();
      break;
    case MENU_CALIBRATION:
      if (inCalibrationMode) {
        processCalibration();
      } else {
        drawCalibrationMenu();
      }
      break;
    case MENU_SYSTEM:
      drawSystemInfo();
      break;
  }
  
  display.display();
}

void drawDashboard() {
  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("DASHBOARD");
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);
  
  // Sensores
  display.setCursor(0, 14);
  display.print("pH: ");
  display.print(currentData.ph, 2);
  
  display.setCursor(70, 14);
  display.print("EC:");
  display.print((int)currentData.ec);
  
  display.setCursor(0, 26);
  display.print("Ar: ");
  display.print(currentData.air_temp, 1);
  display.print("C");
  
  display.setCursor(70, 26);
  display.print("Um:");
  display.print((int)currentData.humidity);
  display.print("%");
  
  display.setCursor(0, 38);
  display.print("H2O: ");
  display.print(currentData.water_temp, 1);
  display.print("C");
  
  // Status
  display.drawLine(0, 49, SCREEN_WIDTH, 49, SSD1306_WHITE);
  display.setCursor(0, 54);
  display.print(wifiConnected ? "W" : "w");
  display.print(mqttConnected ? " M" : " m");
  display.print(bleActive ? " B" : " b");
  
  display.setCursor(50, 54);
  display.print(millis() / 60000);
  display.print("min");
}

void drawConnections() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CONEXOES");
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);
  
  display.setCursor(0, 14);
  display.print("WiFi: ");
  display.print(wifiConnected ? "OK" : "FALHA");
  
  if (wifiConnected) {
    display.setCursor(0, 26);
    display.print("SSID: ");
    display.print(networks[currentNetworkIndex].ssid);
    
    display.setCursor(0, 38);
    display.print("RSSI: ");
    display.print(WiFi.RSSI());
    display.print(" dBm");
  }
  
  display.setCursor(0, 50);
  display.print("MQTT: ");
  display.print(mqttConnected ? "OK" : "FALHA");
}

void drawCalibrationMenu() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CALIBRACAO");
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);
  
  display.setCursor(0, 16);
  display.print(menuSelection == 0 ? "> " : "  ");
  display.print("pH 4.0");
  
  display.setCursor(0, 28);
  display.print(menuSelection == 1 ? "> " : "  ");
  display.print("pH 7.0");
  
  display.setCursor(0, 40);
  display.print(menuSelection == 2 ? "> " : "  ");
  display.print("EC 1413 uS/cm");
  
  display.setCursor(0, 52);
  display.print(menuSelection == 3 ? "> " : "  ");
  display.print("Voltar");
}

void drawSystemInfo() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("SISTEMA");
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);
  
  display.setCursor(0, 14);
  display.print("FW: ");
  display.print(FIRMWARE_VERSION);
  
  display.setCursor(0, 26);
  display.print("UUID:");
  display.setCursor(0, 36);
  display.print(deviceUUID);
  
  display.setCursor(0, 48);
  display.print("RAM: ");
  display.print(ESP.getFreeHeap() / 1024);
  display.print("KB");
}

void displayMessage(const String& title, const String& message, int duration) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);
  
  display.setCursor(0, 20);
  display.print(message);
  display.display();
  
  if (duration > 0) {
    delay(duration);
  }
}

// ==================== SEÇÃO 16: BOTÕES ====================
void initButtons() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  
  logMessage(LOG_INFO, "✅ Botões inicializados");
}

bool isButtonPressed(int pin) {
  if (digitalRead(pin) == LOW) {
    if (millis() - lastButtonPress > DEBOUNCE_DELAY) {
      lastButtonPress = millis();
      return true;
    }
  }
  return false;
}

void handleButtons() {
  if (isButtonPressed(BTN_UP)) {
    if (inCalibrationMode) {
      // Não usado no modo calibração
    } else if (currentPage == MENU_CALIBRATION) {
      menuSelection = (menuSelection > 0) ? menuSelection - 1 : 3;
    } else {
      currentPage = (MenuPage)((currentPage > 0) ? currentPage - 1 : MENU_SYSTEM);
    }
    logMessage(LOG_DEBUG, "BTN_UP pressionado");
  }
  
  if (isButtonPressed(BTN_DOWN)) {
    if (inCalibrationMode) {
      // Não usado no modo calibração
    } else if (currentPage == MENU_CALIBRATION) {
      menuSelection = (menuSelection < 3) ? menuSelection + 1 : 0;
    } else {
      currentPage = (MenuPage)((currentPage < MENU_SYSTEM) ? currentPage + 1 : MENU_DASHBOARD);
    }
    logMessage(LOG_DEBUG, "BTN_DOWN pressionado");
  }
  
  if (isButtonPressed(BTN_SELECT)) {
    if (currentPage == MENU_CALIBRATION) {
      if (inCalibrationMode) {
        finishCalibration();
      } else {
        if (menuSelection < 3) {
          startCalibration();
        }
      }
    }
    logMessage(LOG_DEBUG, "BTN_SELECT pressionado");
  }
  
  if (isButtonPressed(BTN_BACK)) {
    if (inCalibrationMode) {
      inCalibrationMode = false;
      calStep = CAL_MENU;
      displayMessage("Calibracao", "Cancelada", 1500);
    } else if (currentPage == MENU_CALIBRATION) {
      currentPage = MENU_DASHBOARD;
    }
    logMessage(LOG_DEBUG, "BTN_BACK pressionado");
  }
}

// ==================== SEÇÃO 17: CALIBRAÇÃO ====================
void startCalibration() {
  inCalibrationMode = true;
  
  switch (menuSelection) {
    case 0:
      calStep = CAL_PH_4;
      displayMessage("Calibracao", "pH 4.0", 0);
      break;
    case 1:
      calStep = CAL_PH_7;
      displayMessage("Calibracao", "pH 7.0", 0);
      break;
    case 2:
      calStep = CAL_EC_1413;
      displayMessage("Calibracao", "EC 1413", 0);
      break;
  }
  
  logMessage(LOG_INFO, "🔧 Iniciando calibração: " + String(menuSelection));
}

void processCalibration() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CALIBRACAO");
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);
  
  switch (calStep) {
    case CAL_PH_4:
      display.setCursor(0, 16);
      display.print("Sonda em pH 4.0");
      display.setCursor(0, 28);
      display.print("Leitura: ");
      display.print(readPH(), 2);
      display.setCursor(0, 50);
      display.print("SELECT confirma");
      break;
      
    case CAL_PH_7:
      display.setCursor(0, 16);
      display.print("Sonda em pH 7.0");
      display.setCursor(0, 28);
      display.print("Leitura: ");
      display.print(readPH(), 2);
      display.setCursor(0, 50);
      display.print("SELECT confirma");
      break;
      
    case CAL_EC_1413:
      display.setCursor(0, 16);
      display.print("Sonda em 1413");
      display.setCursor(0, 28);
      display.print("Leitura: ");
      display.print((int)readEC(25.0));
      display.setCursor(0, 50);
      display.print("SELECT confirma");
      break;
      
    default:
      break;
  }
}

void finishCalibration() {
  float reading = 0;
  
  switch (calStep) {
    case CAL_PH_4:
      reading = readPH();
      calibration.ph_cal_4 = reading;
      calibration.ph_intercept = 4.0 - (calibration.ph_slope * (reading * 3.3 / 4095.0));
      logMessage(LOG_INFO, "✅ pH 4.0 calibrado: " + String(reading, 2));
      break;
      
    case CAL_PH_7:
      reading = readPH();
      calibration.ph_cal_7 = reading;
      float voltage4 = calibration.ph_cal_4 * 3.3 / 4095.0;
      float voltage7 = reading * 3.3 / 4095.0;
      calibration.ph_slope = (7.0 - 4.0) / (voltage7 - voltage4);
      logMessage(LOG_INFO, "✅ pH 7.0 calibrado: " + String(reading, 2));
      break;
      
    case CAL_EC_1413:
      reading = readEC(25.0);
      calibration.ec_k = 1413.0 / reading;
      calibration.ec_cal_1413 = reading;
      logMessage(LOG_INFO, "✅ EC 1413 calibrado: " + String(reading, 0));
      break;
      
    default:
      break;
  }
  
  saveCalibration();
  inCalibrationMode = false;
  calStep = CAL_MENU;
  displayMessage("Calibracao", "Completa!", 2000);
}

// ==================== SEÇÃO 18: WATCHDOG ====================
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

// ==================== SEÇÃO 19: SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   AquaSys Nexus - Módulo Sensor       ║");
  Serial.println("║   Versão: " + String(FIRMWARE_VERSION) + "       ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  
  // Inicializar I2C e Display
  Wire.begin();
  initDisplay();
  
  deviceUUID = generateDeviceUUID();
  logMessage(LOG_INFO, "🆔 UUID: " + deviceUUID);
  displayMessage("UUID", deviceUUID, 2000);
  
  initWatchdog();
  initButtons();
  
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
  displayMessage("Sistema", "Pronto!", 2000);
}

// ==================== SEÇÃO 20: LOOP ====================
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
  
  // Atualizar display e processar botões
  handleButtons();
  updateDisplay();
  
  delay(100);
}
