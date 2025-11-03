/*
 * =====================================================
 *  AquaSys Nexus - Actuator Module v4.0.9-FIXED
 * =====================================================
 * 
 * Firmware para controle de atuadores (relés) com:
 * - Autenticação dinâmica via Edge Function
 * - 8 relés com 9 modos de operação
 * - AP Mode com WebServer para configuração
 * - Persistência em NVS
 * - Lógica automática (pH, EC, temperatura, etc.)
 * - Pulsos não-bloqueantes
 * - Sincronização NTP
 * - Histórico de pH (24h)
 * - Watchdog robusto
 * - Buffer de mensagens offline
 * - Reconexão MQTT com backoff exponencial
 * - Validação de comandos
 * - Logs estruturados
 * - BLE como fallback local
 * 
 * CORREÇÕES v4.0.9:
 * - ✅ API do Watchdog compatível com ESP-IDF 5.x
 * - ✅ Watchdog inicializado no início do setup()
 * - ✅ Resets do WDT antes de operações bloqueantes
 * - ✅ authenticateDevice() com timeout de 5s
 * - ✅ Auto-reboot após 5 falhas de autenticação
 * - ✅ BLE reintegrado como fallback
 * - ✅ Histórico de pH funcional
 * - ✅ Indicador de fonte de dados (MQTT/BLE)
 * 
 * Autor: AquaSys Development Team
 * Data: 2025-11-03
 * =====================================================
 */

// =====================================================
// INCLUDES
// =====================================================
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// =====================================================
// CONFIGURAÇÃO DE REDE E AUTENTICAÇÃO
// =====================================================
const char* DEFAULT_SSID = "HydroSmart_AP";
const char* DEFAULT_PASSWORD = "hydro2025";

// Servidor de autenticação (Edge Function)
const char* AUTH_SERVER = "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth";
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw";

// Configurações MQTT dinâmicas (preenchidas pela autenticação)
String MQTT_BROKER = "";
int MQTT_PORT = 8883;
String MQTT_USER = "";
String MQTT_PASSWORD = "";
String TOPIC_RELAY_STATUS = "";
String TOPIC_RELAY_COMMAND = "";
String TOPIC_SENSORS = "";
String TOPIC_HEARTBEAT = "";

// Certificado raiz HiveMQ Cloud (TLS 1.3)
const char* HIVEMQ_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

// UUIDs para o serviço BLE
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// =====================================================
// HARDWARE
// =====================================================
const int RELAY_PINS[8] = {13, 12, 14, 27, 26, 25, 33, 32};
const int SETUP_BUTTON_PIN = 0; // Boot button

// =====================================================
// OBJETOS GLOBAIS
// =====================================================
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;
WebServer server(80);
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

// =====================================================
// ENUMS
// =====================================================
enum RelayMode {
  MODE_MANUAL_OFF = 0,
  MODE_MANUAL_ON = 1,
  MODE_AUTO_PH_DOWN = 2,
  MODE_AUTO_PH_UP = 3,
  MODE_AUTO_EC_UP = 4,
  MODE_AUTO_TEMP_COOL = 5,
  MODE_AUTO_TEMP_HEAT = 6,
  MODE_AUTO_HUMIDITY = 7,
  MODE_TIMER = 8
};

enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO = 1,
  LOG_WARN = 2,
  LOG_ERROR = 3
};

// =====================================================
// ESTRUTURAS
// =====================================================
struct RelayConfig {
  RelayMode mode;
  float ph_min;
  float ph_max;
  float ec_min;
  float ec_max;
  float temp_min;
  float temp_max;
  float humidity_min;
  float humidity_max;
  unsigned long timer_on_duration;
  unsigned long timer_off_duration;
  unsigned long last_timer_change;
  bool timer_state;
};

struct SensorData {
  float ph;
  int ec;
  float air_temp;
  float humidity;
  float water_temp;
  unsigned long timestamp;
  bool valid;
  String source; // "MQTT" ou "BLE"
};

struct Pulse {
  int relayIndex;
  unsigned long startTime;
  unsigned long duration;
  bool active;
};

struct DiagnosticData {
  unsigned long uptime;
  int wifi_rssi;
  bool mqtt_connected;
  bool ble_connected;
  unsigned long last_sensor_update;
  unsigned long free_heap;
  int relay_states[8];
};

// =====================================================
// VARIÁVEIS GLOBAIS
// =====================================================
String deviceUUID = "";
bool apMode = false;
bool authCompleted = false;
int authFailureCount = 0;
const int MAX_AUTH_FAILURES = 5;
bool bleConnected = false;

// Relés
RelayConfig relayConfigs[8];
bool relayStates[8] = {false, false, false, false, false, false, false, false};
unsigned long lastRelayChange[8] = {0, 0, 0, 0, 0, 0, 0, 0};
const unsigned long RELAY_DEBOUNCE = 500;

// Dados de sensores
SensorData currentSensorData = {0, 0, 0, 0, 0, 0, false, ""};

// Pulsos ativos
Pulse activePulses[8];

// Diagnóstico
DiagnosticData diagnostics;

// Timers
unsigned long lastMQTTAttempt = 0;
unsigned long lastMQTTReconnect = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastSensorDataWarning = 0;
unsigned long lastStatusPublish = 0;
unsigned long bootTime = 0;

// Intervalos
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
const unsigned long HEARTBEAT_INTERVAL = 60000;
const unsigned long STATUS_PUBLISH_INTERVAL = 10000;
const unsigned long SENSOR_TIMEOUT = 180000; // 3 minutos

// Backoff exponencial para reconexão MQTT
int mqttReconnectAttempts = 0;
const int MAX_MQTT_RECONNECT_ATTEMPTS = 10;

// Fila de mensagens offline (outbox)
struct MQTTMessage {
  String topic;
  String payload;
};
const int MAX_OUTBOX_SIZE = 50;
MQTTMessage outbox[MAX_OUTBOX_SIZE];
int outboxCount = 0;

// Credenciais WiFi (podem ser alteradas via AP)
String wifiSSID = "";
String wifiPassword = "";

// Histórico de pH (24 horas, 1 leitura/hora)
float ph_history[24];
int ph_history_index = 0;
unsigned long last_ph_log = 0;

// Contadores de boot e crash
int bootCount = 0;
int crashCount = 0;

// Cooldowns para ações automáticas
unsigned long lastPhAction = 0;
unsigned long lastEcAction = 0;
const unsigned long PH_COOLDOWN = 300000; // 5 minutos
const unsigned long EC_COOLDOWN = 600000; // 10 minutos

// =====================================================
// PROTÓTIPOS DE FUNÇÕES
// =====================================================
// Autenticação
bool authenticateDevice();

// Rede
void setupWiFi();
void checkWiFi();
void startAPMode();
void handleRoot();
void handleSave();
void handleStatus();

// NTP
void setupNTP();
void updateNTP();
String getTimestamp();

// Persistência
void loadConfig();
void saveConfig();
void saveRelayConfig(int index);

// MQTT
void setupMQTT();
bool connectMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishRelayStatus();
void publishHeartbeat();
void addToOutbox(String topic, String payload);
void flushOutbox();

// Relés
void setupRelays();
void updateRelay(int index, bool state, String reason);
void startPulse(int relayIndex, unsigned long duration);
void updatePulses();
void updateAutomaticRelays();

// Validação
bool isValidRelayIndex(int index);
bool isValidRelayMode(int mode);
bool isValidSensorData(const SensorData& data);

// Logs
void logMessage(LogLevel level, String message);

// pH History
void logHourlyPH(float ph);
float calculate24hAveragePH();

// BLE
void setupBLE();

// Watchdog
void initWatchdog();

// =====================================================
// BLE CALLBACKS
// =====================================================
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    logMessage(LOG_INFO, "BLE client connected");
  }
  
  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    logMessage(LOG_WARN, "BLE client disconnected");
    pServer->startAdvertising();
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      String bleData = String(value.c_str());
      logMessage(LOG_DEBUG, "BLE RX: " + bleData);
      
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, bleData);
      
      if (!error) {
        if (doc.containsKey("ph")) {
          currentSensorData.ph = doc["ph"];
          currentSensorData.ec = doc["ec"] | 0;
          currentSensorData.air_temp = doc["air_temp"] | 0;
          currentSensorData.humidity = doc["humidity"] | 0;
          currentSensorData.water_temp = doc["water_temp"] | 0;
          currentSensorData.timestamp = millis();
          currentSensorData.valid = true;
          currentSensorData.source = "BLE";
          
          logHourlyPH(currentSensorData.ph);
          logMessage(LOG_INFO, "Sensor data updated via BLE");
        }
      }
    }
  }
};

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // ✅ CRÍTICO: Inicializar Watchdog IMEDIATAMENTE
  initWatchdog();
  
  logMessage(LOG_INFO, "==============================================");
  logMessage(LOG_INFO, "  AquaSys Nexus - Actuator Module v4.0.9");
  logMessage(LOG_INFO, "==============================================");
  
  // Gerar UUID único do dispositivo
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  deviceUUID = String(mac[0], HEX) + String(mac[1], HEX) + 
               String(mac[2], HEX) + String(mac[3], HEX) + 
               String(mac[4], HEX) + String(mac[5], HEX);
  deviceUUID.toUpperCase();
  logMessage(LOG_INFO, "Device UUID: " + deviceUUID);
  
  // Configurar relés
  setupRelays();
  
  // Carregar configurações do NVS
  loadConfig();
  
  // Incrementar contador de boot
  bootCount++;
  preferences.begin("hydrosmart", false);
  preferences.putInt("bootCount", bootCount);
  preferences.end();
  
  logMessage(LOG_INFO, "Boot count: " + String(bootCount));
  
  // Inicializar BLE imediatamente (não depende de WiFi)
  setupBLE();
  
  // Determinar modo de operação
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  if (digitalRead(SETUP_BUTTON_PIN) == LOW || wifiSSID.length() == 0) {
    logMessage(LOG_INFO, "Starting in AP mode (setup button pressed or no WiFi config)");
    startAPMode();
    apMode = true;
    return;
  }
  
  // Modo normal: conectar ao WiFi
  esp_task_wdt_reset(); // Reset antes do WiFi
  setupWiFi();
  
  // Autenticar dispositivo
  esp_task_wdt_reset(); // Reset antes da autenticação
  logMessage(LOG_INFO, "Authenticating device...");
  if (!authenticateDevice()) {
    logMessage(LOG_ERROR, "Authentication failed - will retry in loop");
    authCompleted = false;
    return;
  }
  authCompleted = true;
  
  // Configurar NTP
  esp_task_wdt_reset(); // Reset antes do NTP
  setupNTP();
  
  // Configurar e conectar MQTT
  esp_task_wdt_reset(); // Reset antes do MQTT
  setupMQTT();
  esp_task_wdt_reset(); // Reset antes de tentar conectar
  if (connectMQTT()) {
    logMessage(LOG_INFO, "MQTT connected successfully");
  } else {
    logMessage(LOG_WARN, "MQTT connection failed - will retry in loop");
  }
  
  // Restaurar estados dos relés
  for (int i = 0; i < 8; i++) {
    updateRelay(i, relayStates[i], "Boot restore");
  }
  
  bootTime = millis();
  logMessage(LOG_INFO, "Setup completed - entering main loop");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  esp_task_wdt_reset(); // Reset no início de cada loop
  
  // Modo AP: apenas servir páginas web
  if (apMode) {
    server.handleClient();
    delay(10);
    return;
  }
  
  // Verificar se autenticação foi completada
  if (!authCompleted) {
    if (millis() - lastMQTTReconnect > 30000) {
      lastMQTTReconnect = millis();
      
      if (authFailureCount < MAX_AUTH_FAILURES) {
        logMessage(LOG_INFO, "Retrying authentication... (attempt " + 
                   String(authFailureCount + 1) + "/" + String(MAX_AUTH_FAILURES) + ")");
        esp_task_wdt_reset(); // Reset antes de tentar autenticar
        authCompleted = authenticateDevice();
        
        if (authCompleted) {
          authFailureCount = 0;
          esp_task_wdt_reset();
          setupMQTT();
        } else {
          authFailureCount++;
        }
      } else {
        logMessage(LOG_ERROR, "Authentication failed 5 times - rebooting...");
        delay(1000);
        ESP.restart();
      }
    }
    delay(1000);
    return;
  }
  
  // Verificar e reconectar WiFi se necessário
  checkWiFi();
  
  // Verificar e reconectar MQTT se necessário
  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }
  
  // Atualizar pulsos ativos
  updatePulses();
  
  // Executar lógica automática dos relés
  updateAutomaticRelays();
  
  // Publicar status dos relés
  if (millis() - lastStatusPublish > STATUS_PUBLISH_INTERVAL) {
    lastStatusPublish = millis();
    publishRelayStatus();
  }
  
  // Publicar heartbeat
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    lastHeartbeat = millis();
    publishHeartbeat();
  }
  
  // Atualizar NTP periodicamente
  updateNTP();
  
  // Verificar dados de sensores obsoletos (>2 min)
  if (currentSensorData.valid && (millis() - currentSensorData.timestamp > 120000)) {
    if (millis() - lastSensorDataWarning > 60000) {
      lastSensorDataWarning = millis();
      logMessage(LOG_WARN, "Sensor data is outdated (>2min) - Check MQTT or BLE connection");
    }
  }
  
  // Tentar enviar mensagens da fila offline
  if (mqttClient.connected() && outboxCount > 0) {
    flushOutbox();
  }
  
  delay(10);
}

// =====================================================
// AUTENTICAÇÃO
// =====================================================
bool authenticateDevice() {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  http.begin(client, AUTH_SERVER);
  http.setTimeout(5000); // ✅ Timeout de 5 segundos
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  
  StaticJsonDocument<256> requestDoc;
  requestDoc["device_uuid"] = deviceUUID;
  requestDoc["device_type"] = "actuator";
  
  String requestBody;
  serializeJson(requestDoc, requestBody);
  
  logMessage(LOG_DEBUG, "Auth request: " + requestBody);
  
  int httpCode = http.POST(requestBody);
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    logMessage(LOG_DEBUG, "Auth response: " + response);
    
    StaticJsonDocument<1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (error) {
      logMessage(LOG_ERROR, "Failed to parse auth response: " + String(error.c_str()));
      http.end();
      return false;
    }
    
    if (responseDoc.containsKey("mqtt")) {
      JsonObject mqtt = responseDoc["mqtt"];
      MQTT_BROKER = mqtt["broker"].as<String>();
      MQTT_PORT = mqtt["port"];
      MQTT_USER = mqtt["username"].as<String>();
      MQTT_PASSWORD = mqtt["password"].as<String>();
      
      JsonObject topics = responseDoc["topics"];
      TOPIC_RELAY_STATUS = topics["relay_status"].as<String>();
      TOPIC_RELAY_COMMAND = topics["relay_command"].as<String>();
      TOPIC_SENSORS = topics["sensors"].as<String>();
      TOPIC_HEARTBEAT = topics["heartbeat"].as<String>();
      
      logMessage(LOG_INFO, "Authentication successful");
      logMessage(LOG_INFO, "MQTT Broker: " + MQTT_BROKER + ":" + String(MQTT_PORT));
      
      http.end();
      return true;
    } else {
      logMessage(LOG_ERROR, "Invalid auth response format");
      http.end();
      return false;
    }
  } else {
    logMessage(LOG_ERROR, "Auth HTTP error: " + String(httpCode));
    http.end();
    return false;
  }
}

// =====================================================
// REDE
// =====================================================
void setupWiFi() {
  logMessage(LOG_INFO, "Connecting to WiFi: " + wifiSSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 10 == 0) {
      esp_task_wdt_reset(); // Reset a cada 5 segundos
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    logMessage(LOG_INFO, "WiFi connected");
    logMessage(LOG_INFO, "IP: " + WiFi.localIP().toString());
    logMessage(LOG_INFO, "RSSI: " + String(WiFi.RSSI()) + " dBm");
  } else {
    logMessage(LOG_ERROR, "WiFi connection failed");
  }
}

void checkWiFi() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      logMessage(LOG_WARN, "WiFi disconnected - reconnecting...");
      esp_task_wdt_reset();
      setupWiFi();
    }
  }
}

void startAPMode() {
  logMessage(LOG_INFO, "Starting Access Point...");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_SSID, DEFAULT_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  logMessage(LOG_INFO, "AP IP address: " + IP.toString());
  
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", handleStatus);
  server.begin();
  
  logMessage(LOG_INFO, "Web server started");
  logMessage(LOG_INFO, "Connect to WiFi: " + String(DEFAULT_SSID));
  logMessage(LOG_INFO, "Password: " + String(DEFAULT_PASSWORD));
  logMessage(LOG_INFO, "Then open: http://" + IP.toString());
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>HydroSmart Config</title>
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
      padding: 40px;
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 400px;
      width: 100%;
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
    label {
      display: block;
      margin-bottom: 8px;
      color: #333;
      font-weight: 600;
      font-size: 14px;
    }
    input {
      width: 100%;
      padding: 12px;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      font-size: 16px;
      transition: border 0.3s;
      margin-bottom: 20px;
    }
    input:focus {
      outline: none;
      border-color: #667eea;
    }
    button {
      width: 100%;
      padding: 14px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s;
    }
    button:hover {
      transform: translateY(-2px);
    }
    .info {
      margin-top: 20px;
      padding: 15px;
      background: #f5f5f5;
      border-radius: 8px;
      font-size: 12px;
      color: #666;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🌱 HydroSmart</h1>
    <p class="subtitle">Configure sua rede WiFi</p>
    <form action="/save" method="POST">
      <label>Nome da Rede (SSID)</label>
      <input type="text" name="ssid" required placeholder="Digite o SSID">
      
      <label>Senha do WiFi</label>
      <input type="password" name="password" required placeholder="Digite a senha">
      
      <button type="submit">💾 Salvar e Conectar</button>
    </form>
    <div class="info">
      ℹ️ Após salvar, o dispositivo irá reiniciar e conectar à rede configurada.
    </div>
  </div>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  wifiSSID = server.arg("ssid");
  wifiPassword = server.arg("password");
  
  preferences.begin("hydrosmart", false);
  preferences.putString("ssid", wifiSSID);
  preferences.putString("password", wifiPassword);
  preferences.end();
  
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Configuração Salva</title>
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
      padding: 40px;
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 400px;
      width: 100%;
      text-align: center;
    }
    h1 {
      color: #4caf50;
      margin-bottom: 20px;
      font-size: 32px;
    }
    p {
      color: #666;
      font-size: 16px;
      line-height: 1.6;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>✅ Configuração Salva!</h1>
    <p>O dispositivo irá reiniciar em 3 segundos e conectar à rede <strong>)" + wifiSSID + R"(</strong>.</p>
  </div>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
  
  delay(3000);
  ESP.restart();
}

void handleStatus() {
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["uptime"] = millis() / 1000;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["mqtt_connected"] = mqttClient.connected();
  doc["ble_connected"] = bleConnected;
  doc["free_heap"] = ESP.getFreeHeap();
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}

// =====================================================
// NTP
// =====================================================
void setupNTP() {
  logMessage(LOG_INFO, "Configuring NTP...");
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  
  int attempts = 0;
  time_t now = time(nullptr);
  while (now < 100000 && attempts < 20) {
    delay(500);
    now = time(nullptr);
    attempts++;
    
    if (attempts % 4 == 0) {
      esp_task_wdt_reset();
    }
  }
  
  if (now > 100000) {
    logMessage(LOG_INFO, "NTP synchronized: " + String(ctime(&now)));
  } else {
    logMessage(LOG_WARN, "NTP synchronization timeout");
  }
}

void updateNTP() {
  static unsigned long lastNTPUpdate = 0;
  if (millis() - lastNTPUpdate > 3600000) { // Atualizar a cada 1 hora
    lastNTPUpdate = millis();
    setupNTP();
  }
}

String getTimestamp() {
  time_t now = time(nullptr);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", localtime(&now));
  return String(buffer);
}

// =====================================================
// PERSISTÊNCIA
// =====================================================
void loadConfig() {
  preferences.begin("hydrosmart", true);
  
  wifiSSID = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("password", "");
  
  for (int i = 0; i < 8; i++) {
    String key = "relay" + String(i);
    relayStates[i] = preferences.getBool(key.c_str(), false);
    
    String modeKey = "mode" + String(i);
    relayConfigs[i].mode = (RelayMode)preferences.getInt(modeKey.c_str(), MODE_MANUAL_OFF);
    
    String phMinKey = "phMin" + String(i);
    relayConfigs[i].ph_min = preferences.getFloat(phMinKey.c_str(), 6.0);
    
    String phMaxKey = "phMax" + String(i);
    relayConfigs[i].ph_max = preferences.getFloat(phMaxKey.c_str(), 7.0);
    
    // Valores padrão para outros parâmetros
    relayConfigs[i].ec_min = 800;
    relayConfigs[i].ec_max = 1500;
    relayConfigs[i].temp_min = 18.0;
    relayConfigs[i].temp_max = 28.0;
    relayConfigs[i].humidity_min = 40.0;
    relayConfigs[i].humidity_max = 70.0;
    relayConfigs[i].timer_on_duration = 60000;
    relayConfigs[i].timer_off_duration = 300000;
    relayConfigs[i].last_timer_change = 0;
    relayConfigs[i].timer_state = false;
  }
  
  // Inicializar histórico de pH
  for (int i = 0; i < 24; i++) {
    ph_history[i] = 0.0;
  }
  
  bootCount = preferences.getInt("bootCount", 0);
  crashCount = preferences.getInt("crashCount", 0);
  
  preferences.end();
  
  logMessage(LOG_INFO, "Configuration loaded from NVS");
}

void saveConfig() {
  preferences.begin("hydrosmart", false);
  
  for (int i = 0; i < 8; i++) {
    String key = "relay" + String(i);
    preferences.putBool(key.c_str(), relayStates[i]);
  }
  
  preferences.end();
  logMessage(LOG_DEBUG, "Configuration saved to NVS");
}

void saveRelayConfig(int index) {
  if (!isValidRelayIndex(index)) return;
  
  preferences.begin("hydrosmart", false);
  
  String modeKey = "mode" + String(index);
  preferences.putInt(modeKey.c_str(), relayConfigs[index].mode);
  
  String phMinKey = "phMin" + String(index);
  preferences.putFloat(phMinKey.c_str(), relayConfigs[index].ph_min);
  
  String phMaxKey = "phMax" + String(index);
  preferences.putFloat(phMaxKey.c_str(), relayConfigs[index].ph_max);
  
  preferences.end();
  
  logMessage(LOG_DEBUG, "Relay " + String(index) + " config saved");
}

// =====================================================
// MQTT
// =====================================================
void setupMQTT() {
  wifiClient.setCACert(HIVEMQ_ROOT_CA);
  mqttClient.setServer(MQTT_BROKER.c_str(), MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(15);
  
  logMessage(LOG_INFO, "MQTT configured");
}

bool connectMQTT() {
  String clientId = "hydroSmart_actuator_" + deviceUUID;
  
  logMessage(LOG_INFO, "Connecting to MQTT as: " + clientId);
  
  if (mqttClient.connect(clientId.c_str(), MQTT_USER.c_str(), MQTT_PASSWORD.c_str())) {
    logMessage(LOG_INFO, "MQTT connected");
    
    mqttClient.subscribe(TOPIC_RELAY_COMMAND.c_str());
    mqttClient.subscribe(TOPIC_SENSORS.c_str());
    
    logMessage(LOG_INFO, "Subscribed to: " + TOPIC_RELAY_COMMAND);
    logMessage(LOG_INFO, "Subscribed to: " + TOPIC_SENSORS);
    
    mqttReconnectAttempts = 0;
    return true;
  } else {
    logMessage(LOG_ERROR, "MQTT connection failed, rc=" + String(mqttClient.state()));
    return false;
  }
}

void reconnectMQTT() {
  if (millis() - lastMQTTReconnect < MQTT_RECONNECT_INTERVAL) {
    return;
  }
  
  lastMQTTReconnect = millis();
  
  if (mqttReconnectAttempts >= MAX_MQTT_RECONNECT_ATTEMPTS) {
    unsigned long backoff = min(300000UL, 5000UL * pow(2, mqttReconnectAttempts - MAX_MQTT_RECONNECT_ATTEMPTS));
    if (millis() - lastMQTTAttempt < backoff) {
      return;
    }
  }
  
  logMessage(LOG_INFO, "Attempting MQTT reconnection... (attempt " + 
             String(mqttReconnectAttempts + 1) + ")");
  
  lastMQTTAttempt = millis();
  
  if (connectMQTT()) {
    mqttReconnectAttempts = 0;
  } else {
    mqttReconnectAttempts++;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  logMessage(LOG_DEBUG, "MQTT RX [" + String(topic) + "]: " + message);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    logMessage(LOG_ERROR, "JSON parse error: " + String(error.c_str()));
    return;
  }
  
  // Processar dados de sensores
  if (String(topic) == TOPIC_SENSORS) {
    if (doc.containsKey("ph")) {
      currentSensorData.ph = doc["ph"];
      currentSensorData.ec = doc["ec"] | 0;
      currentSensorData.air_temp = doc["air_temp"] | 0;
      currentSensorData.humidity = doc["humidity"] | 0;
      currentSensorData.water_temp = doc["water_temp"] | 0;
      currentSensorData.timestamp = millis();
      currentSensorData.valid = isValidSensorData(currentSensorData);
      currentSensorData.source = "MQTT";
      
      logHourlyPH(currentSensorData.ph);
      
      logMessage(LOG_DEBUG, "Sensor data updated from MQTT");
    }
  }
  
  // Processar comandos de relé
  if (String(topic) == TOPIC_RELAY_COMMAND) {
    if (!doc.containsKey("relay") || !doc.containsKey("action")) {
      logMessage(LOG_ERROR, "Invalid relay command format");
      return;
    }
    
    int relayIndex = doc["relay"];
    
    if (!isValidRelayIndex(relayIndex)) {
      logMessage(LOG_ERROR, "Invalid relay index: " + String(relayIndex));
      return;
    }
    
    String action = doc["action"].as<String>();
    
    if (action == "on") {
      relayConfigs[relayIndex].mode = MODE_MANUAL_ON;
      updateRelay(relayIndex, true, "MQTT command");
      saveRelayConfig(relayIndex);
    } 
    else if (action == "off") {
      relayConfigs[relayIndex].mode = MODE_MANUAL_OFF;
      updateRelay(relayIndex, false, "MQTT command");
      saveRelayConfig(relayIndex);
    }
    else if (action == "pulse") {
      if (doc.containsKey("duration")) {
        unsigned long duration = doc["duration"];
        startPulse(relayIndex, duration);
      }
    }
    else if (action == "config") {
      if (doc.containsKey("mode")) {
        int mode = doc["mode"];
        if (isValidRelayMode(mode)) {
          relayConfigs[relayIndex].mode = (RelayMode)mode;
          
          if (doc.containsKey("ph_min")) {
            relayConfigs[relayIndex].ph_min = doc["ph_min"];
          }
          if (doc.containsKey("ph_max")) {
            relayConfigs[relayIndex].ph_max = doc["ph_max"];
          }
          if (doc.containsKey("ec_min")) {
            relayConfigs[relayIndex].ec_min = doc["ec_min"];
          }
          if (doc.containsKey("ec_max")) {
            relayConfigs[relayIndex].ec_max = doc["ec_max"];
          }
          
          saveRelayConfig(relayIndex);
          logMessage(LOG_INFO, "Relay " + String(relayIndex) + " configured");
        }
      }
    }
  }
}

void publishRelayStatus() {
  if (!mqttClient.connected() || TOPIC_RELAY_STATUS.length() == 0) {
    return;
  }
  
  StaticJsonDocument<1024> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = getTimestamp();
  
  JsonArray relays = doc.createNestedArray("relays");
  for (int i = 0; i < 8; i++) {
    JsonObject relay = relays.createNestedObject();
    relay["index"] = i;
    relay["state"] = relayStates[i];
    relay["mode"] = relayConfigs[i].mode;
  }
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_RELAY_STATUS.c_str(), payload.c_str())) {
    logMessage(LOG_DEBUG, "Relay status published");
  } else {
    addToOutbox(TOPIC_RELAY_STATUS, payload);
  }
}

void publishHeartbeat() {
  if (!mqttClient.connected() || TOPIC_HEARTBEAT.length() == 0) {
    return;
  }
  
  StaticJsonDocument<1024> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = "actuator";
  doc["timestamp"] = getTimestamp();
  doc["uptime"] = millis() / 1000;
  doc["boot_count"] = bootCount;
  doc["crash_count"] = crashCount;
  
  JsonObject network = doc.createNestedObject("network");
  network["wifi_rssi"] = WiFi.RSSI();
  network["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  network["mqtt_connected"] = mqttClient.connected();
  network["ble_connected"] = bleConnected;
  
  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["last_update"] = currentSensorData.timestamp;
  sensors["source"] = currentSensorData.source;
  sensors["valid"] = currentSensorData.valid;
  
  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = ESP.getFreeHeap();
  memory["min_free_heap"] = ESP.getMinFreeHeap();
  
  JsonObject ph_stats = doc.createNestedObject("ph_stats");
  float avg24h = calculate24hAveragePH();
  ph_stats["average_24h"] = (avg24h > 0) ? avg24h : nullptr;
  ph_stats["last_reading"] = currentSensorData.valid ? currentSensorData.ph : nullptr;
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_HEARTBEAT.c_str(), payload.c_str())) {
    logMessage(LOG_DEBUG, "Heartbeat published");
  } else {
    addToOutbox(TOPIC_HEARTBEAT, payload);
  }
}

void addToOutbox(String topic, String payload) {
  if (outboxCount < MAX_OUTBOX_SIZE) {
    outbox[outboxCount].topic = topic;
    outbox[outboxCount].payload = payload;
    outboxCount++;
    logMessage(LOG_DEBUG, "Message added to outbox (" + String(outboxCount) + "/" + String(MAX_OUTBOX_SIZE) + ")");
  } else {
    logMessage(LOG_WARN, "Outbox full - message discarded");
  }
}

void flushOutbox() {
  if (outboxCount == 0) return;
  
  int sentCount = 0;
  for (int i = 0; i < outboxCount; i++) {
    if (mqttClient.publish(outbox[i].topic.c_str(), outbox[i].payload.c_str())) {
      sentCount++;
    } else {
      break;
    }
  }
  
  if (sentCount > 0) {
    for (int i = sentCount; i < outboxCount; i++) {
      outbox[i - sentCount] = outbox[i];
    }
    outboxCount -= sentCount;
    logMessage(LOG_INFO, "Flushed " + String(sentCount) + " messages from outbox");
  }
}

// =====================================================
// RELÉS
// =====================================================
void setupRelays() {
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    activePulses[i].active = false;
  }
  logMessage(LOG_INFO, "Relays initialized");
}

void updateRelay(int index, bool state, String reason) {
  if (!isValidRelayIndex(index)) return;
  
  if (millis() - lastRelayChange[index] < RELAY_DEBOUNCE) {
    return;
  }
  
  if (relayStates[index] != state) {
    relayStates[index] = state;
    digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
    lastRelayChange[index] = millis();
    
    logMessage(LOG_INFO, "Relay " + String(index) + " -> " + 
               (state ? "ON" : "OFF") + " (" + reason + ")");
    
    saveConfig();
  }
}

void startPulse(int relayIndex, unsigned long duration) {
  if (!isValidRelayIndex(relayIndex)) return;
  
  activePulses[relayIndex].relayIndex = relayIndex;
  activePulses[relayIndex].startTime = millis();
  activePulses[relayIndex].duration = duration;
  activePulses[relayIndex].active = true;
  
  updateRelay(relayIndex, true, "Pulse start");
  
  logMessage(LOG_INFO, "Pulse started on relay " + String(relayIndex) + 
             " for " + String(duration) + "ms");
}

void updatePulses() {
  for (int i = 0; i < 8; i++) {
    if (activePulses[i].active) {
      if (millis() - activePulses[i].startTime >= activePulses[i].duration) {
        activePulses[i].active = false;
        updateRelay(i, false, "Pulse end");
      }
    }
  }
}

void updateAutomaticRelays() {
  if (!currentSensorData.valid) return;
  
  for (int i = 0; i < 8; i++) {
    RelayConfig& config = relayConfigs[i];
    
    switch (config.mode) {
      case MODE_AUTO_PH_DOWN:
        if (currentSensorData.ph > config.ph_max) {
          if (millis() - lastPhAction > PH_COOLDOWN) {
            startPulse(i, 5000);
            lastPhAction = millis();
            logMessage(LOG_INFO, "Auto pH DOWN: pH=" + String(currentSensorData.ph));
          }
        }
        break;
        
      case MODE_AUTO_PH_UP:
        if (currentSensorData.ph < config.ph_min) {
          if (millis() - lastPhAction > PH_COOLDOWN) {
            startPulse(i, 5000);
            lastPhAction = millis();
            logMessage(LOG_INFO, "Auto pH UP: pH=" + String(currentSensorData.ph));
          }
        }
        break;
        
      case MODE_AUTO_EC_UP:
        if (currentSensorData.ec < config.ec_min) {
          if (millis() - lastEcAction > EC_COOLDOWN) {
            startPulse(i, 10000);
            lastEcAction = millis();
            logMessage(LOG_INFO, "Auto EC UP: EC=" + String(currentSensorData.ec));
          }
        }
        break;
        
      case MODE_AUTO_TEMP_COOL:
        if (currentSensorData.water_temp > config.temp_max) {
          updateRelay(i, true, "Auto cooling");
        } else if (currentSensorData.water_temp < config.temp_max - 1.0) {
          updateRelay(i, false, "Auto cooling stop");
        }
        break;
        
      case MODE_AUTO_TEMP_HEAT:
        if (currentSensorData.water_temp < config.temp_min) {
          updateRelay(i, true, "Auto heating");
        } else if (currentSensorData.water_temp > config.temp_min + 1.0) {
          updateRelay(i, false, "Auto heating stop");
        }
        break;
        
      case MODE_AUTO_HUMIDITY:
        if (currentSensorData.humidity < config.humidity_min) {
          updateRelay(i, true, "Auto humidify");
        } else if (currentSensorData.humidity > config.humidity_max) {
          updateRelay(i, false, "Auto humidify stop");
        }
        break;
        
      case MODE_TIMER:
        if (config.timer_state) {
          if (millis() - config.last_timer_change > config.timer_on_duration) {
            updateRelay(i, false, "Timer OFF");
            config.timer_state = false;
            config.last_timer_change = millis();
          }
        } else {
          if (millis() - config.last_timer_change > config.timer_off_duration) {
            updateRelay(i, true, "Timer ON");
            config.timer_state = true;
            config.last_timer_change = millis();
          }
        }
        break;
        
      case MODE_MANUAL_ON:
        updateRelay(i, true, "Manual ON");
        break;
        
      case MODE_MANUAL_OFF:
        updateRelay(i, false, "Manual OFF");
        break;
    }
  }
}

// =====================================================
// VALIDAÇÃO
// =====================================================
bool isValidRelayIndex(int index) {
  return index >= 0 && index < 8;
}

bool isValidRelayMode(int mode) {
  return mode >= MODE_MANUAL_OFF && mode <= MODE_TIMER;
}

bool isValidSensorData(const SensorData& data) {
  return data.ph >= 0 && data.ph <= 14 &&
         data.ec >= 0 && data.ec <= 5000 &&
         data.air_temp >= -10 && data.air_temp <= 60 &&
         data.humidity >= 0 && data.humidity <= 100 &&
         data.water_temp >= 0 && data.water_temp <= 50;
}

// =====================================================
// LOGS
// =====================================================
void logMessage(LogLevel level, String message) {
  String levelStr;
  switch (level) {
    case LOG_DEBUG: levelStr = "DEBUG"; break;
    case LOG_INFO:  levelStr = "INFO "; break;
    case LOG_WARN:  levelStr = "WARN "; break;
    case LOG_ERROR: levelStr = "ERROR"; break;
  }
  
  String timestamp = getTimestamp();
  String logLine = "[" + timestamp + "] [" + levelStr + "] " + message;
  
  Serial.println(logLine);
  
  if (level >= LOG_INFO && mqttClient.connected()) {
    StaticJsonDocument<256> doc;
    doc["device_uuid"] = deviceUUID;
    doc["level"] = levelStr;
    doc["message"] = message;
    doc["timestamp"] = timestamp;
    
    String payload;
    serializeJson(doc, payload);
    
    String logTopic = "hydrosmart/" + deviceUUID + "/logs";
    mqttClient.publish(logTopic.c_str(), payload.c_str());
  }
}

// =====================================================
// pH HISTORY
// =====================================================
void logHourlyPH(float ph) {
  if (millis() - last_ph_log < 3600000) { // 1 hora
    return;
  }
  
  last_ph_log = millis();
  ph_history[ph_history_index] = ph;
  ph_history_index = (ph_history_index + 1) % 24;
  
  logMessage(LOG_DEBUG, "pH logged to history: " + String(ph));
}

float calculate24hAveragePH() {
  float sum = 0;
  int count = 0;
  
  for (int i = 0; i < 24; i++) {
    if (ph_history[i] > 0) {
      sum += ph_history[i];
      count++;
    }
  }
  
  return (count > 0) ? (sum / count) : 0;
}

// =====================================================
// BLE
// =====================================================
void setupBLE() {
  BLEDevice::init("HydroSmart_Actuator_" + deviceUUID);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  logMessage(LOG_INFO, "BLE advertising started");
}

// =====================================================
// WATCHDOG
// =====================================================
void initWatchdog() {
  // ✅ API compatível com ESP-IDF 5.x (ESP32 Arduino Core 3.x+)
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 20000,        // 20 segundos
    .idle_core_mask = 0,        // Não monitorar idle tasks
    .trigger_panic = true       // Reiniciar em caso de timeout
  };
  
  esp_err_t result = esp_task_wdt_init(&wdt_config);
  
  if (result == ESP_OK) {
    esp_task_wdt_add(NULL); // Adicionar task atual
    logMessage(LOG_INFO, "Watchdog initialized (20s timeout)");
  } else {
    logMessage(LOG_ERROR, "Watchdog init failed: " + String(result));
  }
}
