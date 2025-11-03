/*
 * ================================================================
 * AquaSys Nexus - ESP32 Actuator Module
 * Firmware Version: 4.0.8-COMPLETO
 * ================================================================
 * 
 * Funcionalidades:
 * ✅ Autenticação dinâmica via Edge Function
 * ✅ 8 Relés com 9 modos de operação
 * ✅ Access Point Mode com WebServer
 * ✅ Persistência completa (NVS/Preferences)
 * ✅ Lógica automática baseada em sensores
 * ✅ Sistema de pulsos não-bloqueante
 * ✅ NTP/RTC para controle temporal
 * ✅ Histórico de pH 24h
 * ✅ Watchdog Timer (20s)
 * ✅ Buffer MQTT offline
 * ✅ Reconexão exponencial
 * ✅ Validação de dados
 * ✅ Logs estruturados
 * ✅ Heartbeat diagnóstico
 * ✅ TLS 1.3 com HiveMQ
 * 
 * Autor: AquaSys Team
 * Data: 2025-11-03
 * ================================================================
 */

// ==================== INCLUDES ====================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>

// ==================== CONFIGURAÇÃO DE REDE ====================
// Credenciais WiFi (salvas em Preferences, valores padrão para compilação)
String ssid_sta = "";
String password_sta = "";

// Servidor de autenticação (Supabase Edge Function)
const char* AUTH_SERVER = "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth";
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw";

// Configuração dinâmica do MQTT (obtida via autenticação)
String mqttBroker = "";
int mqttPort = 8884;
String mqttUsername = "";
String mqttPassword = "";
String mqttClientId = "";

// Tópicos MQTT dinâmicos
String topicRelaySatus = "";
String topicRelayCommand = "";
String topicHeartbeat = "";
String topicSensors = "";
String topicLogs = "";

// UUID do dispositivo (gerado a partir do MAC)
String deviceUUID = "";

// Certificado raiz HiveMQ Cloud (TLS 1.3)
const char* root_ca = R"(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hvc1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----)";

// ==================== HARDWARE ====================
// Pinos dos relés (GPIO)
const int RELAY_PINS[8] = {16, 17, 18, 19, 21, 22, 23, 25};
bool relayStates[8] = {false, false, false, false, false, false, false, false};
bool manual_override[8] = {false, false, false, false, false, false, false, false};

// Pino do botão de setup (para entrar em AP mode)
const int SETUP_BUTTON_PIN = 0; // GPIO 0 (BOOT button)

// ==================== OBJETOS GLOBAIS ====================
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 3600000); // UTC-3 (Brasília)

// ==================== ENUMERAÇÕES ====================
enum RelayMode {
  MODE_UNUSED = 0,
  MODE_LED = 1,        // LED por horário (NTP)
  MODE_CYCLE = 2,      // Ciclo ON/OFF periódico
  MODE_PH_UP = 3,      // Aumentar pH (pulso)
  MODE_TEMPERATURE = 4,// Controle de temperatura (ventilador)
  MODE_HUMIDITY = 5,   // Controle de umidade
  MODE_EC = 6,         // Aumentar EC (pulso)
  MODE_CO2 = 7,        // Controle de CO2
  MODE_PH_DOWN = 8     // Diminuir pH (pulso)
};

enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO = 1,
  LOG_WARN = 2,
  LOG_ERROR = 3
};

// ==================== ESTRUTURAS ====================
struct RelayConfig {
  RelayMode mode;
  String name;
  // LED
  int led_on_hour;
  int led_off_hour;
  // CYCLE
  int cycle_on_min;
  int cycle_off_min;
  // PH
  float ph_threshold_low;
  float ph_threshold_high;
  int ph_pulse_sec;
  // TEMPERATURE
  float temp_threshold_on;
  float temp_threshold_off;
  // HUMIDITY
  float humidity_threshold_on;
  float humidity_threshold_off;
  // EC
  float ec_threshold;
  int ec_pulse_sec;
};

struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  bool valid;
  unsigned long lastUpdate;
};

struct Pulse {
  bool active;
  unsigned long startMs;
  unsigned long durationMs;
  int relayIndex;
};

struct DiagnosticData {
  unsigned long uptime;
  String wifiSSID;
  int wifiRSSI;
  String wifiIP;
  int wifiReconnects;
  bool mqttConnected;
  int mqttFailedAttempts;
  unsigned long freeHeap;
  unsigned long minFreeHeap;
};

// ==================== VARIÁVEIS GLOBAIS ====================
RelayConfig configs[8];
SensorData currentSensorData = {0, 0, 0, 0, 0, false, 0};
Pulse activePulses[8];
DiagnosticData diagnostics;

// Estado do sistema
bool apMode = false;
bool authCompleted = false;
bool ntpInitialized = false;

// Timers
unsigned long lastHeartbeat = 0;
unsigned long lastRelayStatusPublish = 0;
unsigned long lastNTPUpdate = 0;
unsigned long lastAutoLogicUpdate = 0;
unsigned long lastMQTTReconnect = 0;
unsigned long cycle_last_toggle_ms[8] = {0};
unsigned long ph_last_action_ms[8] = {0};
unsigned long ec_last_action_ms[8] = {0};

// Intervalos
const unsigned long HEARTBEAT_INTERVAL = 30000;    // 30s
const unsigned long RELAY_STATUS_INTERVAL = 15000; // 15s
const unsigned long AUTO_LOGIC_INTERVAL = 2000;    // 2s
const unsigned long NTP_UPDATE_INTERVAL = 3600000; // 1h
const unsigned long MQTT_RECONNECT_BASE = 2000;    // 2s (backoff exponencial)

// Backoff exponencial
int mqttReconnectAttempts = 0;

// Buffer offline (outbox)
struct OutboxMessage {
  String topic;
  String payload;
  bool used;
};
OutboxMessage outbox[16];
int outboxHead = 0;

// Histórico de pH (24 horas)
float ph_history[24];
int ph_history_index = 0;

// Contadores
int bootCount = 0;
int crashCount = 0;

// ==================== PROTÓTIPOS DE FUNÇÕES ====================
// Autenticação
String generateDeviceUUID();
bool authenticateDevice();

// Rede
void setupWiFi();
void startAPMode();
void setupMQTT();
void reconnectMQTT();
bool connectMQTT();

// WebServer (AP Mode)
void handleRoot();
void handleSave();

// NTP
void setupNTP();
void updateNTP();

// Persistência
void loadConfig();
void saveConfig();
void saveRelayConfig(int index);

// MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishRelayStatus();
void publishHeartbeat();
void enqueueOutgoing(String topic, String payload);
void flushOutbox();

// Relés
void setupRelays();
void updateRelay(int index, bool state);
void updateAutomaticRelays();
void startPulse(int relayIndex, int durationSec);
void updatePulses();

// Validação
bool validateSensorData(SensorData data);

// Logs
void logMessage(LogLevel level, String message);

// pH
void logHourlyPH(float currentPH);
float calculate24hAveragePH();

// Watchdog
void initWatchdog();

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  logMessage(LOG_INFO, "==============================================");
  logMessage(LOG_INFO, "  AquaSys Nexus - Actuator Module v4.0.8");
  logMessage(LOG_INFO, "==============================================");
  
  // Gerar UUID do dispositivo
  deviceUUID = generateDeviceUUID();
  logMessage(LOG_INFO, "Device UUID: " + deviceUUID);
  
  // Configurar pinos dos relés
  setupRelays();
  
  // Verificar botão de setup
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  delay(100);
  
  // Carregar configurações
  loadConfig();
  
  // Decidir modo de operação
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
    logMessage(LOG_INFO, "Setup button pressed - Starting AP Mode");
    apMode = true;
    startAPMode();
    return;
  }
  
  if (ssid_sta.length() == 0) {
    logMessage(LOG_WARN, "No WiFi credentials found - Starting AP Mode");
    apMode = true;
    startAPMode();
    return;
  }
  
  // Conectar WiFi
  setupWiFi();
  
  if (WiFi.status() != WL_CONNECTED) {
    logMessage(LOG_ERROR, "WiFi connection failed - Starting AP Mode");
    apMode = true;
    startAPMode();
    return;
  }
  
  // Autenticar dispositivo
  logMessage(LOG_INFO, "Authenticating device...");
  if (!authenticateDevice()) {
    logMessage(LOG_ERROR, "Authentication failed - will retry in loop");
    return;
  }
  
  authCompleted = true;
  
  // Sincronizar NTP
  setupNTP();
  
  // Configurar MQTT
  setupMQTT();
  
  // Conectar MQTT
  if (connectMQTT()) {
    logMessage(LOG_INFO, "MQTT connected successfully");
  } else {
    logMessage(LOG_WARN, "MQTT connection failed - will retry in loop");
  }
  
  // Inicializar watchdog
  initWatchdog();
  
  // Restaurar estados dos relés
  for (int i = 0; i < 8; i++) {
    updateRelay(i, relayStates[i]);
  }
  
  // Publicar status inicial
  if (mqttClient.connected()) {
    publishRelayStatus();
    publishHeartbeat();
  }
  
  logMessage(LOG_INFO, "Setup complete - entering main loop");
}

// ==================== LOOP PRINCIPAL ====================
void loop() {
  // Reset watchdog
  esp_task_wdt_reset();
  
  // Se em AP mode, apenas processar servidor
  if (apMode) {
    server.handleClient();
    delay(10);
    return;
  }
  
  // Se autenticação não foi completada, tentar novamente
  if (!authCompleted) {
    if (millis() - lastMQTTReconnect > 30000) {
      lastMQTTReconnect = millis();
      logMessage(LOG_INFO, "Retrying authentication...");
      authCompleted = authenticateDevice();
      if (authCompleted) {
        setupMQTT();
      }
    }
    delay(1000);
    return;
  }
  
  // Verificar WiFi
  if (WiFi.status() != WL_CONNECTED) {
    logMessage(LOG_WARN, "WiFi disconnected - reconnecting...");
    setupWiFi();
    delay(5000);
    return;
  }
  
  // Verificar e reconectar MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  
  // Loop MQTT
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  // Atualizar pulsos
  updatePulses();
  
  // Atualizar lógica automática (a cada 2s)
  if (millis() - lastAutoLogicUpdate > AUTO_LOGIC_INTERVAL) {
    lastAutoLogicUpdate = millis();
    updateAutomaticRelays();
  }
  
  // Publicar status dos relés (a cada 15s)
  if (mqttClient.connected() && millis() - lastRelayStatusPublish > RELAY_STATUS_INTERVAL) {
    lastRelayStatusPublish = millis();
    publishRelayStatus();
  }
  
  // Publicar heartbeat (a cada 30s)
  if (mqttClient.connected() && millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    lastHeartbeat = millis();
    publishHeartbeat();
  }
  
  // Atualizar NTP (a cada 1h)
  if (ntpInitialized && millis() - lastNTPUpdate > NTP_UPDATE_INTERVAL) {
    lastNTPUpdate = millis();
    updateNTP();
  }
  
  // Flush outbox se conectado
  if (mqttClient.connected()) {
    flushOutbox();
  }
  
  delay(50);
}

// ==================== AUTENTICAÇÃO ====================
String generateDeviceUUID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  char uuid[32];
  snprintf(uuid, sizeof(uuid), "HYDRO-%02X%02X-%02X%02X-%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  return String(uuid);
}

bool authenticateDevice() {
  if (WiFi.status() != WL_CONNECTED) {
    logMessage(LOG_ERROR, "Cannot authenticate - WiFi not connected");
    return false;
  }
  
  HTTPClient http;
  http.begin(AUTH_SERVER);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  
  StaticJsonDocument<256> requestDoc;
  requestDoc["device_uuid"] = deviceUUID;
  requestDoc["firmware_version"] = "4.0.8-COMPLETO";
  
  String requestBody;
  serializeJson(requestDoc, requestBody);
  
  logMessage(LOG_DEBUG, "Auth request: " + requestBody);
  
  int httpCode = http.POST(requestBody);
  
  if (httpCode == 200) {
    String response = http.getString();
    logMessage(LOG_DEBUG, "Auth response: " + response);
    
    StaticJsonDocument<1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (error) {
      logMessage(LOG_ERROR, "Failed to parse auth response: " + String(error.c_str()));
      http.end();
      return false;
    }
    
    if (responseDoc["success"] == true) {
      mqttBroker = responseDoc["mqtt_config"]["broker"].as<String>();
      mqttUsername = responseDoc["mqtt_config"]["username"].as<String>();
      mqttPassword = responseDoc["mqtt_config"]["password"].as<String>();
      mqttClientId = responseDoc["mqtt_config"]["client_id"].as<String>();
      
      topicSensors = responseDoc["mqtt_config"]["topics"]["sensors"].as<String>();
      topicRelaySatus = responseDoc["mqtt_config"]["topics"]["relay_status"].as<String>();
      topicRelayCommand = responseDoc["mqtt_config"]["topics"]["relay_command"].as<String>();
      topicHeartbeat = responseDoc["mqtt_config"]["topics"]["heartbeat"].as<String>();
      
      // Tópico de logs (não vem da API, mas é padrão)
      topicLogs = "aquasys/" + deviceUUID + "/logs";
      
      logMessage(LOG_INFO, "Authentication successful");
      logMessage(LOG_INFO, "MQTT Broker: " + mqttBroker);
      logMessage(LOG_INFO, "MQTT Username: " + mqttUsername);
      
      http.end();
      return true;
    } else {
      logMessage(LOG_ERROR, "Authentication failed: " + responseDoc["error"].as<String>());
    }
  } else {
    logMessage(LOG_ERROR, "Auth HTTP error: " + String(httpCode));
  }
  
  http.end();
  return false;
}

// ==================== REDE - WIFI ====================
void setupWiFi() {
  logMessage(LOG_INFO, "Connecting to WiFi: " + ssid_sta);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    logMessage(LOG_INFO, "WiFi connected");
    logMessage(LOG_INFO, "IP: " + WiFi.localIP().toString());
    logMessage(LOG_INFO, "RSSI: " + String(WiFi.RSSI()) + " dBm");
    
    diagnostics.wifiSSID = ssid_sta;
    diagnostics.wifiIP = WiFi.localIP().toString();
    diagnostics.wifiRSSI = WiFi.RSSI();
  } else {
    logMessage(LOG_ERROR, "WiFi connection failed");
  }
}

void startAPMode() {
  logMessage(LOG_INFO, "Starting Access Point Mode");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_HydroSmart_Config");
  
  IPAddress IP = WiFi.softAPIP();
  logMessage(LOG_INFO, "AP IP address: " + IP.toString());
  
  // Configurar rotas do WebServer
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  
  server.begin();
  logMessage(LOG_INFO, "WebServer started on port 80");
  
  apMode = true;
}

// ==================== WEBSERVER (AP MODE) ====================
void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>HydroSmart Config</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 400px;
      width: 100%;
      padding: 40px;
    }
    h1 {
      color: #667eea;
      font-size: 28px;
      margin-bottom: 10px;
      text-align: center;
    }
    .subtitle {
      color: #666;
      font-size: 14px;
      text-align: center;
      margin-bottom: 30px;
    }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      color: #333;
      font-weight: 600;
      margin-bottom: 8px;
      font-size: 14px;
    }
    input[type='text'],
    input[type='password'] {
      width: 100%;
      padding: 12px 16px;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      font-size: 16px;
      transition: border-color 0.3s;
    }
    input[type='text']:focus,
    input[type='password']:focus {
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
      transition: transform 0.2s, box-shadow 0.2s;
    }
    button:hover {
      transform: translateY(-2px);
      box-shadow: 0 8px 20px rgba(102, 126, 234, 0.4);
    }
    button:active {
      transform: translateY(0);
    }
    .info {
      background: #f0f4ff;
      border-left: 4px solid #667eea;
      padding: 12px;
      margin-top: 20px;
      border-radius: 4px;
      font-size: 12px;
      color: #555;
    }
  </style>
</head>
<body>
  <div class='container'>
    <h1>🌿 HydroSmart</h1>
    <p class='subtitle'>Configuração de Rede WiFi</p>
    
    <form action='/save' method='POST'>
      <div class='form-group'>
        <label for='ssid'>Nome da Rede (SSID)</label>
        <input type='text' id='ssid' name='ssid' placeholder='Minha_Rede_WiFi' required>
      </div>
      
      <div class='form-group'>
        <label for='password'>Senha WiFi</label>
        <input type='password' id='password' name='password' placeholder='••••••••' required>
      </div>
      
      <button type='submit'>💾 Salvar e Conectar</button>
    </form>
    
    <div class='info'>
      ℹ️ Após salvar, o dispositivo reiniciará e tentará conectar à rede configurada.
    </div>
  </div>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  logMessage(LOG_INFO, "Saving WiFi credentials: " + ssid);
  
  preferences.begin("aquasys", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
  
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Configuração Salva</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
      background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 400px;
      width: 100%;
      padding: 40px;
      text-align: center;
    }
    .success-icon {
      font-size: 64px;
      margin-bottom: 20px;
    }
    h1 {
      color: #11998e;
      font-size: 24px;
      margin-bottom: 10px;
    }
    p {
      color: #666;
      line-height: 1.6;
      margin-bottom: 20px;
    }
    .loader {
      border: 4px solid #f3f3f3;
      border-top: 4px solid #11998e;
      border-radius: 50%;
      width: 40px;
      height: 40px;
      animation: spin 1s linear infinite;
      margin: 20px auto;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
  </style>
</head>
<body>
  <div class='container'>
    <div class='success-icon'>✅</div>
    <h1>Configuração Salva!</h1>
    <p>O dispositivo está reiniciando e tentará conectar à rede WiFi configurada.</p>
    <div class='loader'></div>
    <p style='font-size: 12px; color: #999;'>Aguarde 10 segundos...</p>
  </div>
  <script>
    setTimeout(function() {
      window.location.href = '/';
    }, 10000);
  </script>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
  
  delay(2000);
  ESP.restart();
}

// ==================== MQTT ====================
void setupMQTT() {
  logMessage(LOG_INFO, "Configuring MQTT...");
  
  espClient.setCACert(root_ca);
  mqttClient.setServer(mqttBroker.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1536);
  
  logMessage(LOG_INFO, "MQTT configured for broker: " + mqttBroker);
}

bool connectMQTT() {
  if (mqttClient.connected()) {
    return true;
  }
  
  logMessage(LOG_INFO, "Connecting to MQTT...");
  logMessage(LOG_DEBUG, "Client ID: " + mqttClientId);
  logMessage(LOG_DEBUG, "Username: " + mqttUsername);
  
  bool connected = mqttClient.connect(
    mqttClientId.c_str(),
    mqttUsername.c_str(),
    mqttPassword.c_str()
  );
  
  if (connected) {
    logMessage(LOG_INFO, "MQTT connected");
    
    // Subscribe nos tópicos
    mqttClient.subscribe(topicSensors.c_str(), 1);
    mqttClient.subscribe(topicRelayCommand.c_str(), 1);
    
    logMessage(LOG_INFO, "Subscribed to: " + topicSensors);
    logMessage(LOG_INFO, "Subscribed to: " + topicRelayCommand);
    
    // Resetar contador de tentativas
    mqttReconnectAttempts = 0;
    diagnostics.mqttConnected = true;
    diagnostics.mqttFailedAttempts = 0;
    
    return true;
  } else {
    logMessage(LOG_ERROR, "MQTT connection failed, rc=" + String(mqttClient.state()));
    diagnostics.mqttConnected = false;
    diagnostics.mqttFailedAttempts++;
    
    return false;
  }
}

void reconnectMQTT() {
  unsigned long now = millis();
  
  // Backoff exponencial: 2s, 4s, 8s, 16s, 32s, 60s (max)
  unsigned long backoff = MQTT_RECONNECT_BASE * (1 << mqttReconnectAttempts);
  if (backoff > 60000) backoff = 60000;
  
  if (now - lastMQTTReconnect < backoff) {
    return;
  }
  
  lastMQTTReconnect = now;
  mqttReconnectAttempts++;
  
  logMessage(LOG_WARN, "Attempting MQTT reconnection (attempt " + String(mqttReconnectAttempts) + ")");
  
  if (connectMQTT()) {
    logMessage(LOG_INFO, "MQTT reconnected successfully");
    flushOutbox();
  } else {
    logMessage(LOG_WARN, "MQTT reconnection failed, will retry in " + String(backoff/1000) + "s");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  
  // Converter payload para string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  String payloadStr = String(message);
  
  logMessage(LOG_DEBUG, "MQTT received on " + topicStr + ": " + payloadStr);
  
  // Parse JSON
  StaticJsonDocument<1536> doc;
  DeserializationError error = deserializeJson(doc, payloadStr);
  
  if (error) {
    logMessage(LOG_ERROR, "Failed to parse MQTT JSON: " + String(error.c_str()));
    return;
  }
  
  // ===== PROCESSAR DADOS DE SENSORES =====
  if (topicStr == topicSensors) {
    currentSensorData.ph = doc["ph"] | 0.0f;
    currentSensorData.ec = doc["ec"] | 0.0f;
    currentSensorData.airTemp = doc["air_temp"] | 0.0f;
    currentSensorData.humidity = doc["humidity"] | 0.0f;
    currentSensorData.waterTemp = doc["water_temp"] | 0.0f;
    currentSensorData.lastUpdate = millis();
    
    // Validar dados
    if (validateSensorData(currentSensorData)) {
      currentSensorData.valid = true;
      logMessage(LOG_DEBUG, "Sensor data updated - pH: " + String(currentSensorData.ph, 2));
      
      // Log de pH para histórico
      logHourlyPH(currentSensorData.ph);
    } else {
      currentSensorData.valid = false;
      logMessage(LOG_WARN, "Invalid sensor data received");
    }
  }
  
  // ===== PROCESSAR COMANDOS DE RELÉS =====
  if (topicStr == topicRelayCommand) {
    // Formato 1: Manual Override
    if (doc.containsKey("relay") && doc.containsKey("command")) {
      int relay = doc["relay"].as<int>();
      bool command = doc["command"].as<bool>();
      
      if (relay >= 1 && relay <= 8) {
        int index = relay - 1;
        manual_override[index] = true;
        updateRelay(index, command);
        
        logMessage(LOG_INFO, "Manual command: Relay " + String(relay) + " -> " + (command ? "ON" : "OFF"));
        publishRelayStatus();
      }
    }
    
    // Formato 2: Modo Automático
    if (doc.containsKey("relay") && doc.containsKey("auto")) {
      int relay = doc["relay"].as<int>();
      bool autoMode = doc["auto"].as<bool>();
      
      if (relay >= 1 && relay <= 8 && autoMode) {
        int index = relay - 1;
        manual_override[index] = false;
        
        logMessage(LOG_INFO, "Auto mode enabled for Relay " + String(relay));
        publishRelayStatus();
      }
    }
    
    // Formato 3: Atualizar Configuração
    if (doc.containsKey("relay") && doc.containsKey("config")) {
      int relay = doc["relay"].as<int>();
      
      if (relay >= 1 && relay <= 8) {
        int index = relay - 1;
        JsonObject config = doc["config"];
        
        if (config.containsKey("mode")) {
          configs[index].mode = (RelayMode)config["mode"].as<int>();
        }
        if (config.containsKey("name")) {
          configs[index].name = config["name"].as<String>();
        }
        if (config.containsKey("led_on_hour")) {
          configs[index].led_on_hour = config["led_on_hour"];
        }
        if (config.containsKey("led_off_hour")) {
          configs[index].led_off_hour = config["led_off_hour"];
        }
        if (config.containsKey("cycle_on_min")) {
          configs[index].cycle_on_min = config["cycle_on_min"];
        }
        if (config.containsKey("cycle_off_min")) {
          configs[index].cycle_off_min = config["cycle_off_min"];
        }
        if (config.containsKey("ph_threshold_low")) {
          configs[index].ph_threshold_low = config["ph_threshold_low"];
        }
        if (config.containsKey("ph_threshold_high")) {
          configs[index].ph_threshold_high = config["ph_threshold_high"];
        }
        if (config.containsKey("ph_pulse_sec")) {
          configs[index].ph_pulse_sec = config["ph_pulse_sec"];
        }
        if (config.containsKey("temp_threshold_on")) {
          configs[index].temp_threshold_on = config["temp_threshold_on"];
        }
        if (config.containsKey("temp_threshold_off")) {
          configs[index].temp_threshold_off = config["temp_threshold_off"];
        }
        if (config.containsKey("humidity_threshold_on")) {
          configs[index].humidity_threshold_on = config["humidity_threshold_on"];
        }
        if (config.containsKey("humidity_threshold_off")) {
          configs[index].humidity_threshold_off = config["humidity_threshold_off"];
        }
        if (config.containsKey("ec_threshold")) {
          configs[index].ec_threshold = config["ec_threshold"];
        }
        if (config.containsKey("ec_pulse_sec")) {
          configs[index].ec_pulse_sec = config["ec_pulse_sec"];
        }
        
        saveRelayConfig(index);
        logMessage(LOG_INFO, "Config updated for Relay " + String(relay));
      }
    }
  }
}

void publishRelayStatus() {
  if (!mqttClient.connected()) {
    return;
  }
  
  StaticJsonDocument<512> doc;
  
  for (int i = 0; i < 8; i++) {
    String key = "relay" + String(i + 1);
    doc[key] = relayStates[i];
  }
  
  String payload;
  serializeJson(doc, payload);
  
  bool success = mqttClient.publish(topicRelaySatus.c_str(), payload.c_str(), true);
  
  if (success) {
    logMessage(LOG_DEBUG, "Relay status published");
  } else {
    logMessage(LOG_WARN, "Failed to publish relay status");
    enqueueOutgoing(topicRelaySatus, payload);
  }
}

void publishHeartbeat() {
  if (!mqttClient.connected()) {
    return;
  }
  
  // Atualizar diagnósticos
  diagnostics.uptime = millis();
  diagnostics.freeHeap = ESP.getFreeHeap();
  diagnostics.minFreeHeap = ESP.getMinFreeHeap();
  
  if (WiFi.status() == WL_CONNECTED) {
    diagnostics.wifiRSSI = WiFi.RSSI();
    diagnostics.wifiIP = WiFi.localIP().toString();
  }
  
  StaticJsonDocument<1024> doc;
  
  doc["device"] = "ESP32_Actuator_" + deviceUUID;
  doc["device_uuid"] = deviceUUID;
  doc["firmware"] = "4.0.8-COMPLETO";
  doc["uptime"] = diagnostics.uptime;
  
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["ssid"] = diagnostics.wifiSSID;
  wifi["rssi"] = diagnostics.wifiRSSI;
  wifi["ip"] = diagnostics.wifiIP;
  wifi["reconnects"] = diagnostics.wifiReconnects;
  
  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["connected"] = diagnostics.mqttConnected;
  mqtt["failed_attempts"] = diagnostics.mqttFailedAttempts;
  
  JsonObject relays = doc.createNestedObject("relays");
  for (int i = 0; i < 8; i++) {
    String key = "relay" + String(i + 1);
    JsonObject relay = relays.createNestedObject(key);
    relay["state"] = relayStates[i];
    relay["manual"] = manual_override[i];
    relay["mode"] = configs[i].mode;
    relay["name"] = configs[i].name;
  }
  
  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = diagnostics.freeHeap;
  memory["min_free_heap"] = diagnostics.minFreeHeap;
  
  String payload;
  serializeJson(doc, payload);
  
  bool success = mqttClient.publish(topicHeartbeat.c_str(), payload.c_str());
  
  if (success) {
    logMessage(LOG_DEBUG, "Heartbeat published");
  } else {
    logMessage(LOG_WARN, "Failed to publish heartbeat");
    enqueueOutgoing(topicHeartbeat, payload);
  }
}

void enqueueOutgoing(String topic, String payload) {
  outbox[outboxHead].topic = topic;
  outbox[outboxHead].payload = payload;
  outbox[outboxHead].used = true;
  
  outboxHead = (outboxHead + 1) % 16;
  
  logMessage(LOG_DEBUG, "Message queued in outbox");
}

void flushOutbox() {
  if (!mqttClient.connected()) {
    return;
  }
  
  int flushed = 0;
  
  for (int i = 0; i < 16; i++) {
    if (outbox[i].used) {
      bool success = mqttClient.publish(outbox[i].topic.c_str(), outbox[i].payload.c_str());
      
      if (success) {
        outbox[i].used = false;
        flushed++;
      } else {
        break; // Parar se falhar
      }
    }
  }
  
  if (flushed > 0) {
    logMessage(LOG_INFO, "Flushed " + String(flushed) + " messages from outbox");
  }
}

// ==================== NTP ====================
void setupNTP() {
  logMessage(LOG_INFO, "Initializing NTP...");
  
  timeClient.begin();
  
  // Forçar atualização inicial (5 tentativas)
  for (int i = 0; i < 5; i++) {
    if (timeClient.update()) {
      ntpInitialized = true;
      logMessage(LOG_INFO, "NTP synchronized - Time: " + timeClient.getFormattedTime());
      lastNTPUpdate = millis();
      return;
    }
    delay(1000);
  }
  
  logMessage(LOG_WARN, "NTP initialization failed - will retry later");
}

void updateNTP() {
  if (timeClient.update()) {
    logMessage(LOG_DEBUG, "NTP updated - Time: " + timeClient.getFormattedTime());
  } else {
    logMessage(LOG_WARN, "NTP update failed");
  }
}

// ==================== PERSISTÊNCIA ====================
void loadConfig() {
  logMessage(LOG_INFO, "Loading configuration from NVS...");
  
  preferences.begin("aquasys", false);
  
  // Credenciais WiFi
  ssid_sta = preferences.getString("ssid", "");
  password_sta = preferences.getString("password", "");
  
  // Estados dos relés
  for (int i = 0; i < 8; i++) {
    String key = "relay_" + String(i);
    relayStates[i] = preferences.getBool(key.c_str(), false);
    
    String overrideKey = "override_" + String(i);
    manual_override[i] = preferences.getBool(overrideKey.c_str(), false);
  }
  
  // Configurações dos relés
  for (int i = 0; i < 8; i++) {
    String prefix = "cfg" + String(i) + "_";
    
    configs[i].mode = (RelayMode)preferences.getInt((prefix + "mode").c_str(), MODE_UNUSED);
    configs[i].name = preferences.getString((prefix + "name").c_str(), "Relay " + String(i + 1));
    configs[i].led_on_hour = preferences.getInt((prefix + "led_on").c_str(), 6);
    configs[i].led_off_hour = preferences.getInt((prefix + "led_off").c_str(), 22);
    configs[i].cycle_on_min = preferences.getInt((prefix + "cycle_on").c_str(), 15);
    configs[i].cycle_off_min = preferences.getInt((prefix + "cycle_off").c_str(), 15);
    configs[i].ph_threshold_low = preferences.getFloat((prefix + "ph_low").c_str(), 5.5);
    configs[i].ph_threshold_high = preferences.getFloat((prefix + "ph_high").c_str(), 6.5);
    configs[i].ph_pulse_sec = preferences.getInt((prefix + "ph_pulse").c_str(), 3);
    configs[i].temp_threshold_on = preferences.getFloat((prefix + "temp_on").c_str(), 28.0);
    configs[i].temp_threshold_off = preferences.getFloat((prefix + "temp_off").c_str(), 24.0);
    configs[i].humidity_threshold_on = preferences.getFloat((prefix + "hum_on").c_str(), 70.0);
    configs[i].humidity_threshold_off = preferences.getFloat((prefix + "hum_off").c_str(), 60.0);
    configs[i].ec_threshold = preferences.getFloat((prefix + "ec_th").c_str(), 1.0);
    configs[i].ec_pulse_sec = preferences.getInt((prefix + "ec_pulse").c_str(), 5);
  }
  
  // Contadores
  bootCount = preferences.getInt("bootCount", 0);
  crashCount = preferences.getInt("crashCount", 0);
  
  bootCount++;
  preferences.putInt("bootCount", bootCount);
  
  preferences.end();
  
  logMessage(LOG_INFO, "Configuration loaded - Boot count: " + String(bootCount));
}

void saveConfig() {
  preferences.begin("aquasys", false);
  
  // Estados dos relés
  for (int i = 0; i < 8; i++) {
    String key = "relay_" + String(i);
    preferences.putBool(key.c_str(), relayStates[i]);
    
    String overrideKey = "override_" + String(i);
    preferences.putBool(overrideKey.c_str(), manual_override[i]);
  }
  
  preferences.end();
}

void saveRelayConfig(int index) {
  if (index < 0 || index >= 8) return;
  
  preferences.begin("aquasys", false);
  
  String prefix = "cfg" + String(index) + "_";
  
  preferences.putInt((prefix + "mode").c_str(), configs[index].mode);
  preferences.putString((prefix + "name").c_str(), configs[index].name);
  preferences.putInt((prefix + "led_on").c_str(), configs[index].led_on_hour);
  preferences.putInt((prefix + "led_off").c_str(), configs[index].led_off_hour);
  preferences.putInt((prefix + "cycle_on").c_str(), configs[index].cycle_on_min);
  preferences.putInt((prefix + "cycle_off").c_str(), configs[index].cycle_off_min);
  preferences.putFloat((prefix + "ph_low").c_str(), configs[index].ph_threshold_low);
  preferences.putFloat((prefix + "ph_high").c_str(), configs[index].ph_threshold_high);
  preferences.putInt((prefix + "ph_pulse").c_str(), configs[index].ph_pulse_sec);
  preferences.putFloat((prefix + "temp_on").c_str(), configs[index].temp_threshold_on);
  preferences.putFloat((prefix + "temp_off").c_str(), configs[index].temp_threshold_off);
  preferences.putFloat((prefix + "hum_on").c_str(), configs[index].humidity_threshold_on);
  preferences.putFloat((prefix + "hum_off").c_str(), configs[index].humidity_threshold_off);
  preferences.putFloat((prefix + "ec_th").c_str(), configs[index].ec_threshold);
  preferences.putInt((prefix + "ec_pulse").c_str(), configs[index].ec_pulse_sec);
  
  preferences.end();
  
  logMessage(LOG_DEBUG, "Config saved for relay " + String(index + 1));
}

// ==================== RELÉS ====================
void setupRelays() {
  logMessage(LOG_INFO, "Configuring relay pins...");
  
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], HIGH); // Relés com lógica invertida (HIGH = OFF)
    relayStates[i] = false;
    manual_override[i] = false;
    
    // Inicializar pulsos
    activePulses[i].active = false;
    activePulses[i].startMs = 0;
    activePulses[i].durationMs = 0;
    activePulses[i].relayIndex = i;
  }
  
  logMessage(LOG_INFO, "All relays initialized to OFF");
}

void updateRelay(int index, bool state) {
  if (index < 0 || index >= 8) return;
  
  // Debouncing (300ms)
  static unsigned long lastUpdate[8] = {0};
  unsigned long now = millis();
  
  if (now - lastUpdate[index] < 300) {
    return;
  }
  
  lastUpdate[index] = now;
  
  // Atualizar GPIO (lógica invertida)
  digitalWrite(RELAY_PINS[index], state ? LOW : HIGH);
  relayStates[index] = state;
  
  // Salvar estado
  saveConfig();
  
  logMessage(LOG_INFO, "Relay " + String(index + 1) + " -> " + (state ? "ON" : "OFF"));
}

void startPulse(int relayIndex, int durationSec) {
  if (relayIndex < 0 || relayIndex >= 8) return;
  
  if (activePulses[relayIndex].active) {
    logMessage(LOG_WARN, "Pulse already active for relay " + String(relayIndex + 1));
    return;
  }
  
  activePulses[relayIndex].active = true;
  activePulses[relayIndex].startMs = millis();
  activePulses[relayIndex].durationMs = durationSec * 1000UL;
  
  updateRelay(relayIndex, true);
  
  logMessage(LOG_INFO, "Pulse started on relay " + String(relayIndex + 1) + " for " + String(durationSec) + "s");
}

void updatePulses() {
  unsigned long now = millis();
  
  for (int i = 0; i < 8; i++) {
    if (activePulses[i].active) {
      if (now - activePulses[i].startMs >= activePulses[i].durationMs) {
        updateRelay(i, false);
        activePulses[i].active = false;
        
        logMessage(LOG_INFO, "Pulse completed on relay " + String(i + 1));
      }
    }
  }
}

void updateAutomaticRelays() {
  // Só executar se houver dados válidos de sensores
  if (!currentSensorData.valid) {
    return;
  }
  
  // Timeout de 30 segundos para dados de sensores
  if (millis() - currentSensorData.lastUpdate > 30000) {
    currentSensorData.valid = false;
    logMessage(LOG_WARN, "Sensor data timeout - disabling automatic logic");
    return;
  }
  
  for (int i = 0; i < 8; i++) {
    // Pular se em modo manual
    if (manual_override[i]) {
      continue;
    }
    
    // Pular se pulso ativo
    if (activePulses[i].active) {
      continue;
    }
    
    RelayConfig cfg = configs[i];
    
    switch (cfg.mode) {
      case MODE_UNUSED:
        // Não fazer nada
        break;
        
      case MODE_LED:
        if (ntpInitialized) {
          int currentHour = timeClient.getHours();
          bool shouldBeOn = false;
          
          if (cfg.led_on_hour < cfg.led_off_hour) {
            // Ex: 6h às 22h
            shouldBeOn = (currentHour >= cfg.led_on_hour && currentHour < cfg.led_off_hour);
          } else {
            // Ex: 22h às 6h (atravessa meia-noite)
            shouldBeOn = (currentHour >= cfg.led_on_hour || currentHour < cfg.led_off_hour);
          }
          
          if (relayStates[i] != shouldBeOn) {
            updateRelay(i, shouldBeOn);
          }
        }
        break;
        
      case MODE_CYCLE:
        {
          unsigned long now = millis();
          unsigned long elapsed = now - cycle_last_toggle_ms[i];
          unsigned long cycleTime = relayStates[i] ? (cfg.cycle_on_min * 60000UL) : (cfg.cycle_off_min * 60000UL);
          
          if (elapsed >= cycleTime) {
            updateRelay(i, !relayStates[i]);
            cycle_last_toggle_ms[i] = now;
          }
        }
        break;
        
      case MODE_PH_UP:
        {
          unsigned long now = millis();
          
          // Aguardar 2 minutos entre ações
          if (now - ph_last_action_ms[i] < 120000) {
            break;
          }
          
          if (currentSensorData.ph < cfg.ph_threshold_low) {
            startPulse(i, cfg.ph_pulse_sec);
            ph_last_action_ms[i] = now;
            
            logMessage(LOG_INFO, "pH too low (" + String(currentSensorData.ph, 2) + ") - activating pH UP");
          }
        }
        break;
        
      case MODE_PH_DOWN:
        {
          unsigned long now = millis();
          
          // Aguardar 2 minutos entre ações
          if (now - ph_last_action_ms[i] < 120000) {
            break;
          }
          
          if (currentSensorData.ph > cfg.ph_threshold_high) {
            startPulse(i, cfg.ph_pulse_sec);
            ph_last_action_ms[i] = now;
            
            logMessage(LOG_INFO, "pH too high (" + String(currentSensorData.ph, 2) + ") - activating pH DOWN");
          }
        }
        break;
        
      case MODE_TEMPERATURE:
        {
          bool shouldBeOn = false;
          
          if (currentSensorData.airTemp > cfg.temp_threshold_on) {
            shouldBeOn = true;
          } else if (currentSensorData.airTemp < cfg.temp_threshold_off) {
            shouldBeOn = false;
          } else {
            // Histerese - manter estado atual
            shouldBeOn = relayStates[i];
          }
          
          if (relayStates[i] != shouldBeOn) {
            updateRelay(i, shouldBeOn);
          }
        }
        break;
        
      case MODE_HUMIDITY:
        {
          bool shouldBeOn = false;
          
          if (currentSensorData.humidity > cfg.humidity_threshold_on) {
            shouldBeOn = true;
          } else if (currentSensorData.humidity < cfg.humidity_threshold_off) {
            shouldBeOn = false;
          } else {
            // Histerese - manter estado atual
            shouldBeOn = relayStates[i];
          }
          
          if (relayStates[i] != shouldBeOn) {
            updateRelay(i, shouldBeOn);
          }
        }
        break;
        
      case MODE_EC:
        {
          unsigned long now = millis();
          
          // Aguardar 5 minutos entre ações
          if (now - ec_last_action_ms[i] < 300000) {
            break;
          }
          
          if (currentSensorData.ec < cfg.ec_threshold) {
            startPulse(i, cfg.ec_pulse_sec);
            ec_last_action_ms[i] = now;
            
            logMessage(LOG_INFO, "EC too low (" + String(currentSensorData.ec, 2) + ") - activating EC dosing");
          }
        }
        break;
        
      case MODE_CO2:
        // Similar a temperatura (usar sensor de CO2 se disponível)
        // Por ora, placeholder
        break;
    }
  }
}

// ==================== VALIDAÇÃO ====================
bool validateSensorData(SensorData data) {
  if (data.ph < 0 || data.ph > 14) return false;
  if (data.ec < 0 || data.ec > 5000) return false;
  if (data.airTemp < -10 || data.airTemp > 60) return false;
  if (data.humidity < 0 || data.humidity > 100) return false;
  if (data.waterTemp < 0 || data.waterTemp > 50) return false;
  
  return true;
}

// ==================== LOGS ====================
void logMessage(LogLevel level, String message) {
  String levelStr;
  
  switch (level) {
    case LOG_DEBUG:
      levelStr = "DEBUG";
      break;
    case LOG_INFO:
      levelStr = "INFO";
      break;
    case LOG_WARN:
      levelStr = "WARN";
      break;
    case LOG_ERROR:
      levelStr = "ERROR";
      break;
  }
  
  String timestamp = ntpInitialized ? timeClient.getFormattedTime() : String(millis() / 1000) + "s";
  String logLine = "[" + timestamp + "] [" + levelStr + "] " + message;
  
  Serial.println(logLine);
  
  // Enviar para MQTT (apenas INFO, WARN, ERROR)
  if (level >= LOG_INFO && mqttClient.connected() && topicLogs.length() > 0) {
    StaticJsonDocument<256> doc;
    doc["timestamp"] = timestamp;
    doc["level"] = levelStr;
    doc["message"] = message;
    
    String payload;
    serializeJson(doc, payload);
    
    mqttClient.publish(topicLogs.c_str(), payload.c_str());
  }
}

// ==================== pH HISTÓRICO ====================
void logHourlyPH(float currentPH) {
  static unsigned long lastLog = 0;
  unsigned long now = millis();
  
  // Log a cada 1 hora
  if (now - lastLog < 3600000) {
    return;
  }
  
  lastLog = now;
  
  ph_history[ph_history_index] = currentPH;
  ph_history_index = (ph_history_index + 1) % 24;
  
  logMessage(LOG_DEBUG, "pH logged to history: " + String(currentPH, 2));
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
  
  if (count == 0) return 0;
  
  return sum / count;
}

// ==================== WATCHDOG ====================
void initWatchdog() {
  esp_task_wdt_init(20, true); // 20 segundos
  esp_task_wdt_add(NULL);
  
  logMessage(LOG_INFO, "Watchdog timer initialized (20s)");
}
