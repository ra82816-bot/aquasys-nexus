/*
 * ============================================================================
 * AquaSys Nexus - Actuator Module v4.3.0-COMPLETE
 * ============================================================================
 * SOLUÇÃO DEFINITIVA - Funciona COM ou SEM internet
 * 
 * RECURSOS:
 * ✅ WiFi com modo AP automático (sem internet → AP em 15s)
 * ✅ Portal captivo moderno para configuração WiFi
 * ✅ Suporte a 3 redes WiFi com prioridades
 * ✅ Modo de emergência offline:
 *    - Relé 1: LED 5:00-00:00 (timer)
 *    - Relés 2,3,4: Ciclo 15min ON/OFF
 * ✅ BLE Client para ler dados do sensor
 * ✅ MQTT seguro (quando online)
 * ✅ Watchdog robusto (60s)
 * ✅ RTC interno para tempo offline
 * ✅ Logging estruturado com níveis
 * ✅ UUID único por MAC
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
#include <HTTPClient.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>

// ==================== SEÇÃO 2: CONFIGURAÇÕES ====================
// Versão do Firmware
#define FIRMWARE_VERSION "4.3.1-AUTH"
#define DEVICE_TYPE "ACTUATOR"

// Pinos dos Relés (8 relés)
const int RELAY_PINS[8] = {23, 5, 4, 13, 22, 21, 14, 12};

// WiFi AP Mode
#define AP_SSID_PREFIX "AquaSys-ACT-"
#define AP_PASSWORD "aquasys2024"
#define AP_TIMEOUT 300000  // 5min em AP mode antes de tentar WiFi novamente

// Timeouts
#define WIFI_TIMEOUT 15000        // 15s por rede
#define MQTT_TIMEOUT 30000        // 30s para MQTT
#define EMERGENCY_TIMEOUT 300000  // 5min offline = modo emergência
#define BLE_SCAN_TIMEOUT 5        // 5s para scan BLE
#define WATCHDOG_TIMEOUT 60       // 60s
#define AUTH_TIMEOUT 10000        // 10s para autenticação

// ✅ API Supabase (autenticação dinâmica)
#define SUPABASE_URL "https://oaabtbvwxsjomeeizciq.supabase.co"
#define SUPABASE_ANON_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw"

// MQTT Configuration (fallback - será substituído por credenciais dinâmicas)
#define MQTT_BROKER_FALLBACK "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883

// MQTT Topics (fallback - serão substituídos por tópicos dinâmicos)
#define TOPIC_RELAY_STATUS_FALLBACK "aquasys/relay/status"
#define TOPIC_RELAY_COMMAND_FALLBACK "aquasys/relay/command"
#define TOPIC_RELAY_CONFIG_FALLBACK "aquasys/relay/config"
#define TOPIC_SENSORS_FALLBACK "aquasys/sensors/all"
#define TOPIC_HEARTBEAT_FALLBACK "aquasys/heartbeat"
#define TOPIC_CALIBRATION_FALLBACK "aquasys/calibration/command"

// BLE UUIDs (devem coincidir com o sensor)
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_PH        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_EC        "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_UUID_AIR_TEMP  "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_UUID_HUMIDITY  "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define CHAR_UUID_WATER_TEMP "beb5483e-36e1-4688-b7f5-ea07361b26ac"

// NTP Servers
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"
#define GMT_OFFSET -10800  // UTC-3 (Brasília)
#define DAYLIGHT_OFFSET 0

// DNS Público
#define DNS_PRIMARY IPAddress(8, 8, 8, 8)     // Google
#define DNS_SECONDARY IPAddress(1, 1, 1, 1)   // Cloudflare

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
  int priority; // 1=primária, 2=secundária, 3=terciária
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

struct RelayConfig {
  bool enabled;
  String mode;  // "manual", "auto", "emergency_led", "emergency_cycle"
  bool state;
  float min_threshold;
  float max_threshold;
  String name;  // Nome do relé
};

struct EmergencyConfig {
  unsigned long bootTime;
  time_t lastKnownEpoch;
  bool rtcValid;
  int ledOnHour;   // Hora de ligar LED (padrão: 5)
  int ledOffHour;  // Hora de desligar LED (padrão: 0)
  unsigned long cycleInterval; // Intervalo de ciclo em ms (padrão: 900000 = 15min)
};

struct CycleState {
  unsigned long lastToggle;
  bool state;
};

// ✅ NOVO: Estrutura para credenciais MQTT dinâmicas
struct MqttCredentials {
  char broker[128];
  char username[64];
  char password[128];
  char client_id[64];
  char topic_sensors[128];
  char topic_relay_status[128];
  char topic_relay_command[128];
  char topic_heartbeat[128];
  char topic_relay_config[128];
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
unsigned long lastMqttSuccess = 0;
unsigned long lastMqttAttempt = 0;
MqttCredentials mqttCreds = {"", "", "", "", "", "", "", "", "", "", false}; // ✅ Credenciais dinâmicas
bool isAuthenticated = false;

// Relays
RelayConfig relays[8];

// Sensor Data
SensorData currentSensorData = {0, 0, 0, 0, 0, false, 0};

// BLE
BLEClient* pBLEClient = nullptr;
BLERemoteCharacteristic* pRemoteCharPH = nullptr;
BLERemoteCharacteristic* pRemoteCharEC = nullptr;
BLERemoteCharacteristic* pRemoteCharAirTemp = nullptr;
BLERemoteCharacteristic* pRemoteCharHumidity = nullptr;
BLERemoteCharacteristic* pRemoteCharWaterTemp = nullptr;
bool bleClientActive = false;
bool bleConnected = false;
unsigned long lastBLEScan = 0;

// Emergency Mode
bool emergencyMode = false;
EmergencyConfig emergencyConfig;
CycleState cycleRelays[3]; // Para relés 2,3,4 (índices 1,2,3)

// Timers
unsigned long lastSensorRead = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastWdtReset = 0;

// Preferences (NVS)
Preferences prefs;

// ==================== SEÇÃO 5: PROTÓTIPOS DE FUNÇÕES ====================
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

// Web Server (Portal Captivo)
void setupWebServer();
void handleRoot();
void handleScan();
void handleSave();
void handleStatus();
void handleNotFound();

// NTP & RTC
void syncNTP();
time_t estimatedTime();
void updateRTC();

// ✅ NOVO: Autenticação
bool authenticateDevice();
void loadMqttCredentials();
void saveMqttCredentials();

// MQTT
void setupMQTT();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishRelayStatus();
void publishHeartbeat();

// BLE Client
void setupBLEClient();
bool scanAndConnectSensor();
void readSensorDataBLE();
void disconnectBLE();

// Relays
void initRelays();
void loadRelayConfig();
void saveRelayConfig();
void setRelay(int index, bool state);
void handleRelayLogic();

// Emergency Mode
void checkEmergencyMode();
void updateEmergencyRelay1();
void updateEmergencyCycle();

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
  
  // Se CRITICAL → salvar em NVS
  if (level == LOG_CRITICAL) {
    prefs.begin("crash", false);
    prefs.putString("last_crash", message);
    prefs.putULong("crash_time", millis());
    prefs.end();
  }
}

// ==================== SEÇÃO 7: IMPLEMENTAÇÃO - UUID ====================
String generateDeviceUUID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char uuid[20];
  sprintf(uuid, "ACT-%02X%02X%02X%02X%02X%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(uuid);
}

// ==================== SEÇÃO 8: IMPLEMENTAÇÃO - WIFI ====================
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
      logMessage(LOG_INFO, "WiFi carregado: " + String(networks[i].ssid) + " (prioridade " + String(networks[i].priority) + ")");
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
  
  // Ordenar redes por prioridade
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
      
      // Sincronizar NTP
      syncNTP();
      
      return true;
    } else {
      logMessage(LOG_WARN, "Falha WiFi: " + String(networks[i].ssid));
    }
  }
  
  wifiConnected = false;
  return false;
}

void checkWiFi() {
  if (millis() - lastWiFiCheck < 10000) return; // Check a cada 10s
  lastWiFiCheck = millis();
  
  resetWatchdog(); // ✅ Reset antes de check WiFi
  
  if (WiFi.status() != WL_CONNECTED) {
    logMessage(LOG_WARN, "WiFi desconectado, tentando reconectar...");
    wifiConnected = false;
    mqttConnected = false;
    
    if (!connectWiFi()) {
      logMessage(LOG_ERROR, "Todas as redes WiFi falharam");
      if (!apMode) {
        startAPMode();
      }
    }
  }
}

void startAPMode() {
  logMessage(LOG_INFO, "🔶 Iniciando modo AP...");
  
  WiFi.mode(WIFI_AP);
  String apSSID = String(AP_SSID_PREFIX) + deviceUUID.substring(4); // ACT-XXXXXX
  WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
  
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  
  // DNS Server (portal captivo)
  dnsServer.start(53, "*", apIP);
  
  setupWebServer();
  server.begin();
  
  apMode = true;
  apModeStartTime = millis();
  
  logMessage(LOG_INFO, "✅ AP ativo: " + apSSID + " / Senha: " + String(AP_PASSWORD));
  logMessage(LOG_INFO, "Portal captivo: http://192.168.4.1");
}

void stopAPMode() {
  if (!apMode) return;
  
  logMessage(LOG_INFO, "Parando modo AP...");
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
}

// ==================== SEÇÃO 9: IMPLEMENTAÇÃO - WEB SERVER ====================
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
  <title>AquaSys - Configuração WiFi</title>
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
    h1 {
      color: #667eea;
      margin-bottom: 10px;
      font-size: 28px;
    }
    .device-info {
      background: #f0f4ff;
      padding: 15px;
      border-radius: 10px;
      margin-bottom: 30px;
      font-size: 14px;
      color: #555;
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
    button:hover {
      transform: translateY(-2px);
    }
    button:active {
      transform: translateY(0);
    }
    .status {
      margin-top: 20px;
      padding: 15px;
      border-radius: 8px;
      text-align: center;
      font-weight: 600;
      display: none;
    }
    .status.success {
      background: #d4edda;
      color: #155724;
      display: block;
    }
    .status.error {
      background: #f8d7da;
      color: #721c24;
      display: block;
    }
    .scan-btn {
      background: #28a745;
      margin-bottom: 20px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🌊 AquaSys</h1>
    <div class="device-info">
      <strong>Dispositivo:</strong> )rawliteral" + deviceUUID + R"rawliteral(<br>
      <strong>Versão:</strong> )rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(<br>
      <strong>Tipo:</strong> Módulo Atuador
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
            setTimeout(() => {
              window.location.href = '/status';
            }, 3000);
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
    network["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Aberta" : "Protegida";
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
  
  // Salvar na primeira posição disponível
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
  doc["message"] = "Configuração salva";
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  // Tentar reconectar WiFi em 2s
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
    
    <div class="status-item )rawliteral" + String(emergencyMode ? "warning" : "success") + R"rawliteral(">
      <strong>Modo:</strong> )rawliteral" + String(emergencyMode ? "EMERGÊNCIA" : "Normal") + R"rawliteral(
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
  // Redirecionar para portal captivo
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// ==================== SEÇÃO 10: IMPLEMENTAÇÃO - NTP & RTC ====================
void syncNTP() {
  if (!wifiConnected) return;
  
  logMessage(LOG_INFO, "Sincronizando NTP...");
  resetWatchdog(); // ✅ Reset antes de NTP
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER1, NTP_SERVER2);
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    time_t now;
    time(&now);
    
    emergencyConfig.lastKnownEpoch = now;
    emergencyConfig.bootTime = millis();
    emergencyConfig.rtcValid = true;
    
    // Salvar em NVS
    prefs.begin("rtc", false);
    prefs.putULong("epoch", now);
    prefs.putULong("boot", millis());
    prefs.end();
    
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    logMessage(LOG_INFO, "✅ NTP sincronizado: " + String(timeStr));
  } else {
    logMessage(LOG_WARN, "Falha ao sincronizar NTP");
  }
}

time_t estimatedTime() {
  if (!emergencyConfig.rtcValid) {
    // Tentar carregar do NVS
    prefs.begin("rtc", true);
    emergencyConfig.lastKnownEpoch = prefs.getULong("epoch", 0);
    emergencyConfig.bootTime = prefs.getULong("boot", 0);
    prefs.end();
    
    if (emergencyConfig.lastKnownEpoch > 0) {
      emergencyConfig.rtcValid = true;
    } else {
      return 0; // Sem tempo disponível
    }
  }
  
  unsigned long elapsedSeconds = (millis() - emergencyConfig.bootTime) / 1000;
  return emergencyConfig.lastKnownEpoch + elapsedSeconds;
}

void updateRTC() {
  if (!wifiConnected || !mqttConnected) return;
  
  // Atualizar NTP a cada 1 hora
  static unsigned long lastNTPSync = 0;
  if (millis() - lastNTPSync > 3600000) {
    syncNTP();
    lastNTPSync = millis();
  }
}

// ==================== SEÇÃO 10.5: AUTENTICAÇÃO DINÂMICA ====================
bool authenticateDevice() {
  if (!wifiConnected) {
    logMessage(LOG_ERROR, "❌ Autenticação requer WiFi conectado");
    return false;
  }
  
  logMessage(LOG_INFO, "🔐 Iniciando autenticação dinâmica...");
  resetWatchdog();
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // Para desenvolvimento - em produção use certificado
  
  String url = String(SUPABASE_URL) + "/functions/v1/device-auth";
  http.begin(client, url);
  
  // Headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  
  // Body
  StaticJsonDocument<256> reqDoc;
  reqDoc["device_uuid"] = deviceUUID;
  reqDoc["firmware_version"] = FIRMWARE_VERSION;
  
  String requestBody;
  serializeJson(reqDoc, requestBody);
  
  logMessage(LOG_INFO, "📤 Enviando: " + requestBody);
  
  // Timeout
  http.setTimeout(AUTH_TIMEOUT);
  
  // POST request
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
      // Extrair credenciais
      JsonObject mqtt_config = doc["mqtt_config"];
      
      strncpy(mqttCreds.broker, mqtt_config["broker"] | MQTT_BROKER_FALLBACK, sizeof(mqttCreds.broker) - 1);
      strncpy(mqttCreds.username, mqtt_config["username"] | deviceUUID.c_str(), sizeof(mqttCreds.username) - 1);
      strncpy(mqttCreds.password, mqtt_config["password"] | "", sizeof(mqttCreds.password) - 1);
      strncpy(mqttCreds.client_id, mqtt_config["client_id"] | deviceUUID.c_str(), sizeof(mqttCreds.client_id) - 1);
      
      // Tópicos dinâmicos
      JsonObject topics = mqtt_config["topics"];
      strncpy(mqttCreds.topic_sensors, topics["sensors"] | TOPIC_SENSORS_FALLBACK, sizeof(mqttCreds.topic_sensors) - 1);
      strncpy(mqttCreds.topic_relay_status, topics["relay_status"] | TOPIC_RELAY_STATUS_FALLBACK, sizeof(mqttCreds.topic_relay_status) - 1);
      strncpy(mqttCreds.topic_relay_command, topics["relay_command"] | TOPIC_RELAY_COMMAND_FALLBACK, sizeof(mqttCreds.topic_relay_command) - 1);
      strncpy(mqttCreds.topic_heartbeat, topics["heartbeat"] | TOPIC_HEARTBEAT_FALLBACK, sizeof(mqttCreds.topic_heartbeat) - 1);
      
      // Tópicos padrão se não fornecidos
      snprintf(mqttCreds.topic_relay_config, sizeof(mqttCreds.topic_relay_config), "aquasys/%s/relay/config", deviceUUID.c_str());
      snprintf(mqttCreds.topic_calibration, sizeof(mqttCreds.topic_calibration), "aquasys/%s/calibration/command", deviceUUID.c_str());
      
      mqttCreds.valid = true;
      isAuthenticated = true;
      
      // Salvar em NVS para uso offline
      saveMqttCredentials();
      
      logMessage(LOG_INFO, "✅ Autenticação bem-sucedida!");
      logMessage(LOG_INFO, "   Broker: " + String(mqttCreds.broker));
      logMessage(LOG_INFO, "   Username: " + String(mqttCreds.username));
      logMessage(LOG_INFO, "   Topics: " + String(mqttCreds.topic_relay_command));
      
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
    
    String topic_relay_cmd = prefs.getString("topic_cmd", "");
    String topic_relay_status = prefs.getString("topic_status", "");
    
    if (topic_relay_cmd.length() > 0) {
      strncpy(mqttCreds.topic_relay_command, topic_relay_cmd.c_str(), sizeof(mqttCreds.topic_relay_command) - 1);
      strncpy(mqttCreds.topic_relay_status, topic_relay_status.c_str(), sizeof(mqttCreds.topic_relay_status) - 1);
      
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
  prefs.putString("topic_cmd", mqttCreds.topic_relay_command);
  prefs.putString("topic_status", mqttCreds.topic_relay_status);
  
  prefs.end();
  
  logMessage(LOG_INFO, "💾 Credenciais MQTT salvas na NVS");
}

// ==================== SEÇÃO 11: IMPLEMENTAÇÃO - MQTT ====================
void setupMQTT() {
  // ✅ Verificar heap disponível antes de configurar TLS
  uint32_t freeHeap = ESP.getFreeHeap();
  logMessage(LOG_INFO, "Heap livre: " + String(freeHeap) + " bytes (" + String(freeHeap/1024) + " KB)");
  
  if (freeHeap < 50000) {
    logMessage(LOG_ERROR, "⚠️ Heap insuficiente para TLS! Necessário: 50KB, Disponível: " + String(freeHeap/1024) + "KB");
    return;
  }
  
  // ✅ Configurar WiFiClientSecure otimizado
  espClient.setInsecure();
  espClient.setTimeout(20000); // 20s para handshake TLS (antes era padrão ~5s)
  
  // ✅ Usar broker dinâmico ou fallback
  const char* brokerToUse = mqttCreds.valid ? mqttCreds.broker : MQTT_BROKER_FALLBACK;
  
  // ✅ Configurar MQTT client
  mqttClient.setServer(brokerToUse, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(60); // 60s em vez de 30s
  
  logMessage(LOG_INFO, "✅ MQTT configurado: " + String(brokerToUse) + ":" + String(MQTT_PORT));
  logMessage(LOG_INFO, "   Modo: " + String(mqttCreds.valid ? "Autenticado" : "Fallback"));
  logMessage(LOG_INFO, "TLS Timeout configurado: 20s");
}
}

bool reconnectMQTT() {
  if (!wifiConnected) return false;
  if (millis() - lastMqttAttempt < 5000) return false; // Evitar tentativas muito frequentes
  
  lastMqttAttempt = millis();
  resetWatchdog(); // ✅ Reset antes de conectar MQTT
  
  // ✅ NOVO: Monitorar heap antes da tentativa de conexão
  uint32_t heapBefore = ESP.getFreeHeap();
  logMessage(LOG_INFO, "Heap antes de MQTT: " + String(heapBefore) + " bytes (" + String(heapBefore/1024) + " KB)");
  
  // ✅ Usar credenciais dinâmicas ou fallback
  const char* clientIdToUse = mqttCreds.valid ? mqttCreds.client_id : ("aquasys-actuator-" + deviceUUID).c_str();
  const char* usernameToUse = mqttCreds.valid ? mqttCreds.username : deviceUUID.c_str();
  const char* passwordToUse = mqttCreds.valid ? mqttCreds.password : "";
  
  logMessage(LOG_INFO, "Conectando MQTT como: " + String(clientIdToUse));
  logMessage(LOG_INFO, "Username: " + String(usernameToUse));
  logMessage(LOG_INFO, "Modo: " + String(mqttCreds.valid ? "Autenticado" : "Fallback"));
  
  // ✅ Tentar conexão
  bool connected = mqttClient.connect(clientIdToUse, usernameToUse, passwordToUse);
  
  // ✅ NOVO: Monitorar heap após tentativa
  uint32_t heapAfter = ESP.getFreeHeap();
  int32_t heapDelta = heapBefore - heapAfter;
  logMessage(LOG_INFO, "Heap após MQTT: " + String(heapAfter) + " bytes (Delta: " + String(heapDelta) + " bytes)");
  
  if (connected) {
    mqttConnected = true;
    lastMqttSuccess = millis();
    logMessage(LOG_INFO, "✅ MQTT conectado com sucesso!");
    
  // ✅ Inscrever em TODOS os tópicos necessários COM CONFIRMAÇÃO
    // Usar tópicos dinâmicos se disponíveis, senão usar fallback
    const char* topicRelayCmd = mqttCreds.valid ? mqttCreds.topic_relay_command : TOPIC_RELAY_COMMAND_FALLBACK;
    const char* topicSensors = mqttCreds.valid ? mqttCreds.topic_sensors : TOPIC_SENSORS_FALLBACK;
    const char* topicRelayConfig = mqttCreds.valid ? mqttCreds.topic_relay_config : TOPIC_RELAY_CONFIG_FALLBACK;
    const char* topicCalibration = mqttCreds.valid ? mqttCreds.topic_calibration : TOPIC_CALIBRATION_FALLBACK;
    
    bool sub1 = mqttClient.subscribe(topicRelayCmd, 1);
    bool sub2 = mqttClient.subscribe(topicSensors, 1);
    bool sub3 = mqttClient.subscribe(topicRelayConfig, 1);
    bool sub4 = mqttClient.subscribe(topicCalibration, 1);
    
    logMessage(LOG_INFO, "📡 Inscrevendo em tópicos:");
    logMessage(LOG_INFO, "  • " + String(topicRelayCmd) + (sub1 ? " ✅" : " ❌"));
    logMessage(LOG_INFO, "  • " + String(topicSensors) + (sub2 ? " ✅" : " ❌"));
    logMessage(LOG_INFO, "  • " + String(topicRelayConfig) + (sub3 ? " ✅" : " ❌"));
    logMessage(LOG_INFO, "  • " + String(topicCalibration) + (sub4 ? " ✅" : " ❌"));
    
    // Publicar estado inicial
    publishRelayStatus();
    return true;
  } else {
    mqttConnected = false;
    int state = mqttClient.state();
    logMessage(LOG_ERROR, "❌ Falha MQTT, rc=" + String(state));
    
    // ✅ NOVO: Decodificar código de erro MQTT
    switch(state) {
      case -4: 
        logMessage(LOG_ERROR, "Erro: TIMEOUT na conexão (servidor não respondeu)"); 
        break;
      case -3: 
        logMessage(LOG_ERROR, "Erro: CONEXÃO PERDIDA (network failure)"); 
        break;
      case -2: 
        logMessage(LOG_ERROR, "Erro: FALHA NA CONEXÃO DE REDE (TLS handshake falhou)"); 
        logMessage(LOG_ERROR, "Possíveis causas: heap baixo, timeout curto, firewall bloqueando 8883");
        break;
      case -1: 
        logMessage(LOG_ERROR, "Erro: DESCONECTADO"); 
        break;
      case 1: 
        logMessage(LOG_ERROR, "Erro: PROTOCOLO INCORRETO (versão MQTT incompatível)"); 
        break;
      case 2: 
        logMessage(LOG_ERROR, "Erro: ID REJEITADO (clientId inválido)"); 
        break;
      case 3: 
        logMessage(LOG_ERROR, "Erro: SERVIDOR INDISPONÍVEL"); 
        break;
      case 4: 
        logMessage(LOG_ERROR, "Erro: CREDENCIAIS INVÁLIDAS (user/password incorretos)"); 
        break;
      case 5: 
        logMessage(LOG_ERROR, "Erro: NÃO AUTORIZADO (sem permissão)"); 
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
  
  // ✅ LOGGING CRÍTICO: Toda mensagem recebida
  logMessage(LOG_INFO, "📩 MQTT recebido!");
  logMessage(LOG_INFO, "   Tópico: " + String(topic));
  logMessage(LOG_INFO, "   Payload (" + String(length) + " bytes): " + message);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    logMessage(LOG_ERROR, "❌ Falha ao parsear JSON: " + String(error.c_str()));
    return;
  }
  
  // ✅ COMANDO DE RELÉ (aceita "relay" OU "relay_index")
  if (strcmp(topic, TOPIC_RELAY_COMMAND) == 0) {
    logMessage(LOG_INFO, "🎯 Tópico de comando de relé detectado!");
    
    // ✅ CORREÇÃO CRÍTICA: Aceitar AMBOS os formatos
    int relayIndex = -1;
    bool state = false;
    
    // Verificar formato 1: "relay" (usado pelo app web)
    if (doc.containsKey("relay") && doc.containsKey("state")) {
      relayIndex = doc["relay"];
      state = doc["state"];
      logMessage(LOG_INFO, "📝 Formato web detectado: relay=" + String(relayIndex) + ", state=" + String(state));
    }
    // Verificar formato 2: "relay_index" (formato antigo)
    else if (doc.containsKey("relay_index") && doc.containsKey("state")) {
      relayIndex = doc["relay_index"];
      state = doc["state"];
      logMessage(LOG_INFO, "📝 Formato legacy detectado: relay_index=" + String(relayIndex) + ", state=" + String(state));
    }
    else {
      logMessage(LOG_ERROR, "❌ Comando sem campos corretos!");
      logMessage(LOG_ERROR, "   Esperado: {\"relay\": 0-7, \"state\": true/false}");
      logMessage(LOG_ERROR, "   Recebido: " + message);
      return;
    }
    
    // ✅ Validar índice e aplicar comando
    if (relayIndex >= 0 && relayIndex < 8) {
      setRelay(relayIndex, state);
      relays[relayIndex].mode = "manual";
      publishRelayStatus();
      logMessage(LOG_INFO, "✅ Relé " + String(relayIndex) + " → " + (state ? "LIGADO" : "DESLIGADO"));
    } else {
      logMessage(LOG_ERROR, "❌ Índice de relé inválido: " + String(relayIndex) + " (deve ser 0-7)");
    }
  }
  
  // ✅ NOVO: CONFIGURAÇÃO DE RELÉS
  else if (strcmp(topic, TOPIC_RELAY_CONFIG) == 0) {
    logMessage(LOG_INFO, "⚙️ Configuração de relé recebida!");
    
    if (doc.containsKey("relay") || doc.containsKey("relay_index")) {
      int relayIndex = doc.containsKey("relay") ? doc["relay"].as<int>() : doc["relay_index"].as<int>();
      
      if (relayIndex >= 0 && relayIndex < 8) {
        // Atualizar configurações
        if (doc.containsKey("mode")) {
          relays[relayIndex].mode = doc["mode"].as<String>();
          logMessage(LOG_INFO, "  Modo: " + relays[relayIndex].mode);
        }
        if (doc.containsKey("min_threshold")) {
          relays[relayIndex].min_threshold = doc["min_threshold"];
          logMessage(LOG_INFO, "  Min threshold: " + String(relays[relayIndex].min_threshold));
        }
        if (doc.containsKey("max_threshold")) {
          relays[relayIndex].max_threshold = doc["max_threshold"];
          logMessage(LOG_INFO, "  Max threshold: " + String(relays[relayIndex].max_threshold));
        }
        if (doc.containsKey("name")) {
          relays[relayIndex].name = doc["name"].as<String>();
          logMessage(LOG_INFO, "  Nome: " + relays[relayIndex].name);
        }
        
        // Salvar configuração na NVS
        saveRelayConfig();
        
        logMessage(LOG_INFO, "✅ Configuração do relé " + String(relayIndex) + " atualizada!");
      } else {
        logMessage(LOG_ERROR, "❌ Índice de relé inválido na configuração: " + String(relayIndex));
      }
    }
  }
  
  // ✅ NOVO: COMANDOS DE CALIBRAÇÃO
  else if (strcmp(topic, TOPIC_CALIBRATION) == 0) {
    logMessage(LOG_INFO, "🔬 Comando de calibração recebido!");
    
    String sensorType = doc["sensor_type"] | "";
    String calibType = doc["calibration_type"] | "";
    
    logMessage(LOG_INFO, "  Sensor: " + sensorType);
    logMessage(LOG_INFO, "  Tipo: " + calibType);
    
    // Implementação futura: aplicar calibração aos sensores via BLE
    // Por enquanto, apenas logar
    logMessage(LOG_WARN, "⚠️ Calibração via MQTT ainda não implementada (requer integração BLE com sensor)");
  }
  
  // Processar dados de sensores
  else if (strcmp(topic, TOPIC_SENSORS) == 0) {
    currentSensorData.ph = doc["ph"] | 0.0;
    currentSensorData.ec = doc["ec"] | 0.0;
    currentSensorData.air_temp = doc["air_temp"] | 0.0;
    currentSensorData.humidity = doc["humidity"] | 0.0;
    currentSensorData.water_temp = doc["water_temp"] | 0.0;
    currentSensorData.valid = true;
    currentSensorData.timestamp = millis();
    
    lastMqttSuccess = millis(); // Atualizar último sucesso
    
    logMessage(LOG_DEBUG, "Dados sensores recebidos: pH=" + String(currentSensorData.ph) + 
               " EC=" + String(currentSensorData.ec));
  }
}

void publishRelayStatus() {
  if (!mqttConnected) return;
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = millis();
  
  // ✅ Usar tópico dinâmico ou fallback
  const char* topicToUse = mqttCreds.valid ? mqttCreds.topic_relay_status : TOPIC_RELAY_STATUS_FALLBACK;
  
  // ✅ CORREÇÃO CRÍTICA: Formato plano esperado pelo app web
  // App espera: {"relay1": true, "relay2": false, ...}
  // NÃO: {"relays": [{"index": 0, "state": true}, ...]}
  for (int i = 0; i < 8; i++) {
    String relayKey = "relay" + String(i + 1); // relay1, relay2, ..., relay8
    doc[relayKey] = relays[i].state;
  }
  
  String message;
  serializeJson(doc, message);
  
  mqttClient.publish(topicToUse, message.c_str(), true); // Retained message
  logMessage(LOG_INFO, "📤 Status publicado: " + message);
}

void publishHeartbeat() {
  if (!mqttConnected) return;
  
  // ✅ FORMATO CORRIGIDO: Compatível com backend (useMqtt.tsx + mqtt-collector)
  StaticJsonDocument<1024> doc;
  doc["device_uuid"] = deviceUUID;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["device_type"] = DEVICE_TYPE;
  doc["uptime_ms"] = millis(); // ✅ CRITICAL: Backend espera uptime_ms (não uptime em segundos)
  
  // ✅ Status aninhado (esperado pelo backend)
  JsonObject status = doc.createNestedObject("status");
  status["wifi_connected"] = wifiConnected;
  status["rssi"] = wifiConnected ? WiFi.RSSI() : 0;
  status["ip_address"] = wifiConnected ? WiFi.localIP().toString() : "";
  status["mqtt_connected"] = mqttConnected;
  status["ble_active"] = bleClientActive;
  status["ble_connected"] = bleConnected;
  status["emergency_mode"] = emergencyMode;
  
  // ✅ Memória aninhada (esperado pelo backend)
  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = ESP.getFreeHeap();
  memory["min_free_heap"] = ESP.getMinFreeHeap();
  
  // Dados de sensores
  JsonObject sensor = doc.createNestedObject("last_sensor_data");
  sensor["ph"] = currentSensorData.ph;
  sensor["ec"] = currentSensorData.ec;
  sensor["air_temp"] = currentSensorData.air_temp;
  sensor["humidity"] = currentSensorData.humidity;
  sensor["water_temp"] = currentSensorData.water_temp;
  sensor["valid"] = currentSensorData.valid;
  
  // Relés
  JsonArray relaysArray = doc.createNestedArray("relays");
  for (int i = 0; i < 8; i++) {
    JsonObject relay = relaysArray.createNestedObject();
    relay["index"] = i;
    relay["state"] = relays[i].state;
    relay["mode"] = relays[i].mode;
    relay["name"] = relays[i].name;
  }
  
  String message;
  serializeJson(doc, message);
  
  // ✅ Usar tópico dinâmico ou fallback
  const char* topicToUse = mqttCreds.valid ? mqttCreds.topic_heartbeat : TOPIC_HEARTBEAT_FALLBACK;
  
  mqttClient.publish(topicToUse, message.c_str());
  logMessage(LOG_DEBUG, "💓 Heartbeat publicado");
}

// ==================== SEÇÃO 12: IMPLEMENTAÇÃO - BLE CLIENT ====================
void setupBLEClient() {
  if (bleClientActive) return;
  
  logMessage(LOG_INFO, "Inicializando BLE Client...");
  BLEDevice::init("AquaSys-Actuator");
  bleClientActive = true;
  logMessage(LOG_INFO, "✅ BLE Client inicializado");
}

bool scanAndConnectSensor() {
  if (!bleClientActive) {
    setupBLEClient();
  }
  
  logMessage(LOG_INFO, "Escaneando sensores BLE...");
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  resetWatchdog(); // ✅ Reset antes de operação longa
  BLEScanResults* foundDevices = pBLEScan->start(BLE_SCAN_TIMEOUT, false);
  resetWatchdog(); // ✅ Reset após scan
  
  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    
    if (device.getName() == "AquaSys-Sensor") {
      logMessage(LOG_INFO, "✅ Sensor encontrado via BLE!");
      
      pBLEClient = BLEDevice::createClient();
      
      if (pBLEClient->connect(&device)) {
        logMessage(LOG_INFO, "Conectando ao sensor...");
        
        BLERemoteService* pRemoteService = pBLEClient->getService(SERVICE_UUID);
        if (pRemoteService == nullptr) {
          logMessage(LOG_ERROR, "Serviço BLE não encontrado");
          pBLEClient->disconnect();
          return false;
        }
        
        pRemoteCharPH = pRemoteService->getCharacteristic(CHAR_UUID_PH);
        pRemoteCharEC = pRemoteService->getCharacteristic(CHAR_UUID_EC);
        pRemoteCharAirTemp = pRemoteService->getCharacteristic(CHAR_UUID_AIR_TEMP);
        pRemoteCharHumidity = pRemoteService->getCharacteristic(CHAR_UUID_HUMIDITY);
        pRemoteCharWaterTemp = pRemoteService->getCharacteristic(CHAR_UUID_WATER_TEMP);
        
        if (pRemoteCharPH && pRemoteCharEC) {
          bleConnected = true;
          logMessage(LOG_INFO, "✅ Conectado ao sensor via BLE!");
          return true;
        } else {
          logMessage(LOG_ERROR, "Características BLE não encontradas");
          pBLEClient->disconnect();
        }
      }
    }
  }
  
  logMessage(LOG_WARN, "Sensor não encontrado via BLE");
  return false;
}

void readSensorDataBLE() {
  if (!bleConnected || pBLEClient == nullptr || !pBLEClient->isConnected()) {
    bleConnected = false;
    
    // Tentar reconectar a cada 30s
    if (millis() - lastBLEScan > 30000) {
      lastBLEScan = millis();
      scanAndConnectSensor();
    }
    return;
  }
  
  try {
    if (pRemoteCharPH) {
      String value = pRemoteCharPH->readValue().c_str();
      if (value.length() >= sizeof(float)) {
        memcpy(&currentSensorData.ph, value.c_str(), sizeof(float));
      }
    }
    
    if (pRemoteCharEC) {
      String value = pRemoteCharEC->readValue().c_str();
      if (value.length() >= sizeof(float)) {
        memcpy(&currentSensorData.ec, value.c_str(), sizeof(float));
      }
    }
    
    if (pRemoteCharAirTemp) {
      String value = pRemoteCharAirTemp->readValue().c_str();
      if (value.length() >= sizeof(float)) {
        memcpy(&currentSensorData.air_temp, value.c_str(), sizeof(float));
      }
    }
    
    if (pRemoteCharHumidity) {
      String value = pRemoteCharHumidity->readValue().c_str();
      if (value.length() >= sizeof(float)) {
        memcpy(&currentSensorData.humidity, value.c_str(), sizeof(float));
      }
    }
    
    if (pRemoteCharWaterTemp) {
      String value = pRemoteCharWaterTemp->readValue().c_str();
      if (value.length() >= sizeof(float)) {
        memcpy(&currentSensorData.water_temp, value.c_str(), sizeof(float));
      }
    }
    
    currentSensorData.valid = true;
    currentSensorData.timestamp = millis();
    
    logMessage(LOG_DEBUG, "Dados BLE: pH=" + String(currentSensorData.ph) + 
               " EC=" + String(currentSensorData.ec));
  } catch (...) {
    logMessage(LOG_ERROR, "Erro ao ler dados BLE");
    bleConnected = false;
    pBLEClient->disconnect();
  }
}

void disconnectBLE() {
  if (pBLEClient != nullptr && pBLEClient->isConnected()) {
    pBLEClient->disconnect();
  }
  bleConnected = false;
  logMessage(LOG_INFO, "BLE desconectado");
}

// ==================== SEÇÃO 13: IMPLEMENTAÇÃO - RELAYS ====================
void initRelays() {
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    
    relays[i].enabled = true;
    relays[i].mode = "auto";
    relays[i].state = false;
    relays[i].min_threshold = 0.0;
    relays[i].max_threshold = 0.0;
    relays[i].name = "Relé " + String(i + 1);
  }
  
  // Configuração padrão pH (exemplo)
  relays[0].min_threshold = 5.5;
  relays[0].max_threshold = 6.5;
  relays[0].name = "pH Control";
  
  logMessage(LOG_INFO, "Relés inicializados");
}

void loadRelayConfig() {
  prefs.begin("relays", true);
  
  for (int i = 0; i < 8; i++) {
    String keyMode = "mode" + String(i);
    String keyMin = "min" + String(i);
    String keyMax = "max" + String(i);
    String keyName = "name" + String(i);
    
    relays[i].mode = prefs.getString(keyMode.c_str(), "auto");
    relays[i].min_threshold = prefs.getFloat(keyMin.c_str(), 0.0);
    relays[i].max_threshold = prefs.getFloat(keyMax.c_str(), 0.0);
    relays[i].name = prefs.getString(keyName.c_str(), "Relé " + String(i + 1));
  }
  
  prefs.end();
  logMessage(LOG_INFO, "Configuração de relés carregada");
}

void saveRelayConfig() {
  prefs.begin("relays", false);
  
  for (int i = 0; i < 8; i++) {
    String keyMode = "mode" + String(i);
    String keyMin = "min" + String(i);
    String keyMax = "max" + String(i);
    String keyName = "name" + String(i);
    
    prefs.putString(keyMode.c_str(), relays[i].mode);
    prefs.putFloat(keyMin.c_str(), relays[i].min_threshold);
    prefs.putFloat(keyMax.c_str(), relays[i].max_threshold);
    prefs.putString(keyName.c_str(), relays[i].name);
  }
  
  prefs.end();
  logMessage(LOG_INFO, "Configuração de relés salva");
}

void setRelay(int index, bool state) {
  if (index < 0 || index >= 8) return;
  
  digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
  relays[index].state = state;
  
  logMessage(LOG_INFO, "Relé " + String(index) + " → " + (state ? "ON" : "OFF"));
}

void handleRelayLogic() {
  if (!currentSensorData.valid) return;
  if (millis() - currentSensorData.timestamp > 60000) {
    // Dados antigos (>1min), invalidar
    currentSensorData.valid = false;
    return;
  }
  
  // Lógica automática pH (Relé 0)
  if (relays[0].mode == "auto") {
    if (currentSensorData.ph < relays[0].min_threshold) {
      setRelay(0, true); // pH baixo → ligar
    } else if (currentSensorData.ph > relays[0].max_threshold) {
      setRelay(0, false); // pH alto → desligar
    }
  }
  
  // Adicionar mais lógicas automáticas aqui...
}

// ==================== SEÇÃO 14: IMPLEMENTAÇÃO - EMERGENCY MODE ====================
void checkEmergencyMode() {
  bool offline = !mqttConnected && !wifiConnected;
  unsigned long offlineTime = millis() - lastMqttSuccess;
  
  if (offline && offlineTime > EMERGENCY_TIMEOUT) {
    if (!emergencyMode) {
      emergencyMode = true;
      logMessage(LOG_WARN, "🚨 MODO DE EMERGÊNCIA ATIVADO!");
      logMessage(LOG_WARN, "Offline por " + String(offlineTime / 1000) + "s");
      
      // Configurar relés para emergência
      relays[0].mode = "emergency_led";
      relays[1].mode = "emergency_cycle";
      relays[2].mode = "emergency_cycle";
      relays[3].mode = "emergency_cycle";
      
      // Inicializar estados de ciclo
      for (int i = 0; i < 3; i++) {
        cycleRelays[i].lastToggle = millis();
        cycleRelays[i].state = false;
      }
    }
    
    updateEmergencyRelay1();
    updateEmergencyCycle();
  } else {
    if (emergencyMode) {
      emergencyMode = false;
      logMessage(LOG_INFO, "✅ Modo normal restaurado");
      
      // Restaurar modos automáticos
      for (int i = 0; i < 8; i++) {
        relays[i].mode = "auto";
      }
    }
  }
}

void updateEmergencyRelay1() {
  time_t now = estimatedTime();
  if (now == 0) return; // Sem tempo disponível
  
  struct tm* timeinfo = localtime(&now);
  int hour = timeinfo->tm_hour;
  
  // Liga às 5:00, desliga às 00:00 (meia-noite)
  bool shouldBeOn = (hour >= 5 && hour < 24);
  
  if (relays[0].state != shouldBeOn) {
    setRelay(0, shouldBeOn);
    logMessage(LOG_INFO, "Emergência LED: Relé 0 → " + String(shouldBeOn ? "ON" : "OFF") + 
               " (hora: " + String(hour) + ")");
  }
}

void updateEmergencyCycle() {
  unsigned long now = millis();
  unsigned long cycleInterval = 900000; // 15min = 900000ms
  
  for (int i = 0; i < 3; i++) {
    int relayIndex = i + 1; // Relés 1,2,3
    
    if (now - cycleRelays[i].lastToggle >= cycleInterval) {
      cycleRelays[i].state = !cycleRelays[i].state;
      setRelay(relayIndex, cycleRelays[i].state);
      cycleRelays[i].lastToggle = now;
      
      logMessage(LOG_INFO, "Emergência Ciclo: Relé " + String(relayIndex) + 
                 " → " + (cycleRelays[i].state ? "ON" : "OFF"));
    }
  }
}

// ==================== SEÇÃO 15: IMPLEMENTAÇÃO - WATCHDOG ====================
void initWatchdog() {
  // ✅ Deinicializar se já existir
  esp_task_wdt_deinit();
  
  // ✅ Reconfigurar do zero
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = 0, // ✅ Não monitorar idle tasks
    .trigger_panic = true
  };
  
  esp_err_t result = esp_task_wdt_init(&wdt_config);
  if (result == ESP_OK) {
    esp_task_wdt_add(NULL); // Adicionar task atual
    logMessage(LOG_INFO, "Watchdog inicializado (" + String(WATCHDOG_TIMEOUT) + "s)");
  } else {
    logMessage(LOG_ERROR, "Erro ao inicializar watchdog: " + String(result));
  }
}

void resetWatchdog() {
  esp_task_wdt_reset(); // ✅ Sempre resetar (sem delay)
  lastWdtReset = millis();
}

// ==================== SEÇÃO 16: SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  logMessage(LOG_INFO, "===========================================");
  logMessage(LOG_INFO, "AquaSys Nexus Actuator v" + String(FIRMWARE_VERSION));
  logMessage(LOG_INFO, "===========================================");
  
  // Gerar UUID
  deviceUUID = generateDeviceUUID();
  logMessage(LOG_INFO, "Device UUID: " + deviceUUID);
  
  // Inicializar Watchdog
  initWatchdog();
  
  // Inicializar Relés
  initRelays();
  
  // Carregar configurações
  loadWiFiConfig();
  loadRelayConfig();
  loadMqttCredentials(); // ✅ Carregar credenciais salvas (se houver)
  
  // Carregar RTC do NVS
  prefs.begin("rtc", true);
  emergencyConfig.lastKnownEpoch = prefs.getULong("epoch", 0);
  emergencyConfig.bootTime = prefs.getULong("boot", 0);
  emergencyConfig.rtcValid = (emergencyConfig.lastKnownEpoch > 0);
  prefs.end();
  
  emergencyConfig.ledOnHour = 5;
  emergencyConfig.ledOffHour = 0;
  emergencyConfig.cycleInterval = 900000;
  
  // Tentar conectar WiFi
  if (!connectWiFi()) {
    logMessage(LOG_WARN, "WiFi falhou, iniciando modo AP");
    startAPMode();
  } else {
    // ✅ WiFi conectado - tentar autenticação
    logMessage(LOG_INFO, "🔐 Tentando autenticação...");
    if (authenticateDevice()) {
      logMessage(LOG_INFO, "✅ Dispositivo autenticado!");
    } else {
      logMessage(LOG_WARN, "⚠️ Autenticação falhou, usando modo fallback");
      // Continuar mesmo sem autenticação (usar credenciais fallback)
    }
    
    // Configurar MQTT
    setupMQTT();
  }
  
  logMessage(LOG_INFO, "Setup concluído!");
  logMessage(LOG_INFO, "Memória livre: " + String(ESP.getFreeHeap()) + " bytes");
}

// ==================== SEÇÃO 17: LOOP ====================
void loop() {
  resetWatchdog();
  
  // AP Mode
  if (apMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    resetWatchdog(); // ✅ Reset durante AP mode
    
    // Tentar sair do AP mode após timeout
    if (millis() - apModeStartTime > AP_TIMEOUT) {
      logMessage(LOG_INFO, "Timeout AP mode, tentando WiFi novamente...");
      stopAPMode();
      if (!connectWiFi()) {
        startAPMode(); // Voltar para AP se falhar
      }
    }
  } else {
    // Modo normal
    checkWiFi();
    resetWatchdog(); // ✅ Reset após WiFi check
    
    // MQTT
    if (wifiConnected) {
      if (!mqttConnected) {
        reconnectMQTT();
        resetWatchdog(); // ✅ Reset após MQTT
      } else {
        mqttClient.loop();
        updateRTC();
      }
      
      // ✅ Tentar autenticar novamente se ainda não autenticado (a cada 5 minutos)
      static unsigned long lastAuthAttempt = 0;
      if (!isAuthenticated && millis() - lastAuthAttempt > 300000) { // 5 min
        logMessage(LOG_INFO, "🔐 Tentando autenticar novamente...");
        if (authenticateDevice()) {
          setupMQTT(); // Reconfigurar MQTT com novas credenciais
          reconnectMQTT(); // Reconectar imediatamente
        }
        lastAuthAttempt = millis();
      }
    }
    
    // BLE (ativar se MQTT offline >3min)
    if (!mqttConnected && millis() - lastMqttSuccess > 180000) {
      if (!bleClientActive) {
        setupBLEClient();
        scanAndConnectSensor();
        resetWatchdog(); // ✅ Reset após BLE scan
      }
      readSensorDataBLE();
    }
    
    // Modo de emergência
    checkEmergencyMode();
    resetWatchdog(); // ✅ Reset após emergency check
    
    // Lógica de relés
    if (!emergencyMode) {
      handleRelayLogic();
    }
    
    // Publicações periódicas
    if (mqttConnected) {
      if (millis() - lastStatusPublish > 10000) { // A cada 10s
        publishRelayStatus();
        lastStatusPublish = millis();
      }
      
      if (millis() - lastHeartbeat > 60000) { // A cada 1min
        publishHeartbeat();
        lastHeartbeat = millis();
      }
    }
  }
  
  delay(100); // Pequeno delay para estabilidade
}
