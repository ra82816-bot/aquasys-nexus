/*
 * =====================================================
 * AquaSys Nexus - Firmware Atuador v4.1.0-FINAL
 * =====================================================
 * 
 * CHANGELOG v4.1.0:
 * - CORREÇÃO CRÍTICA: Pinos corretos {23,5,4,13,22,21,14,12}
 * - CORREÇÃO CRÍTICA: TLS seguro (setCACert) para autenticação
 * - CORREÇÃO CRÍTICA: Includes corretos (WiFiClientSecure, esp_mac.h)
 * - CORREÇÃO: Backoff exponencial MQTT desde primeira falha
 * - CORREÇÃO: Tipos corretos em JSON (null vs nullptr)
 * - MUDANÇA: BLE Client (não Server) para buscar sensor
 * - MELHORIA: Ativação automática de BLE após 3min sem MQTT
 * - MELHORIA: Loop otimizado com prioridades
 * 
 * COMPATIBILIDADE:
 * - Hardware: Pinos {23,5,4,13,22,21,14,12}
 * - BLE: Atuador Client → Sensor Server
 * - MQTT: TLS 1.3 com HiveMQ
 * - Auth: Supabase Edge Function
 */

#define FIRMWARE_VERSION "4.1.0-FINAL"

// =====================================================
// INCLUDES
// =====================================================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <time.h>
#include <esp_mac.h>
#include <esp_task_wdt.h>

// BLE Client Includes
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

// =====================================================
// CONFIGURAÇÃO DE REDE
// =====================================================
const char* DEFAULT_SSID = "SELECIONAR_REDE";
const char* DEFAULT_PASSWORD = "CONFIGURAR_SENHA";
const char* AP_SSID = "HydroActuator_Config";
const char* AP_PASSWORD = "hydro1234";

// =====================================================
// CONFIGURAÇÃO MQTT (HiveMQ Cloud)
// =====================================================
const char* MQTT_BROKER = "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "esp32-user";
const char* MQTT_PASSWORD = "HydroSmart123";

// Tópicos MQTT
const char* MQTT_BASE_TOPIC = "aquasys";
const char* MQTT_RELAY_STATUS = "aquasys/relay/status";
const char* MQTT_RELAY_COMMAND = "aquasys/relay/command";
const char* MQTT_SENSOR_DATA = "aquasys/sensors/all";
const char* MQTT_HEARTBEAT = "aquasys/heartbeat";
const char* MQTT_LOG_TOPIC = "aquasys/logs";
const char* MQTT_WIFI_STATUS = "aquasys/wifi/status";
const char* MQTT_WIFI_CONFIG = "aquasys/wifi/config";

// Certificado HiveMQ (Let's Encrypt - ISRG Root X1)
const char* HIVEMQ_ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
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
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
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
-----END CERTIFICATE-----
)EOF";

// =====================================================
// CONFIGURAÇÃO DE AUTENTICAÇÃO
// =====================================================
const char* AUTH_SERVER = "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth";

// =====================================================
// CONFIGURAÇÃO BLE
// =====================================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// =====================================================
// HARDWARE - PINOS DOS RELÉS (CORRETO)
// =====================================================
const int RELAY_PINS[8] = {23, 5, 4, 13, 22, 21, 14, 12};
const int SETUP_BUTTON_PIN = 0;

// =====================================================
// OBJETOS GLOBAIS
// =====================================================
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;
WebServer server(80);

// BLE Client
BLEScan* pBLEScan = nullptr;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteChar = nullptr;

// =====================================================
// ENUMS
// =====================================================
enum RelayMode {
  MODE_MANUAL = 0,
  MODE_AUTO_LED = 1,
  MODE_AUTO_CYCLE = 2,
  MODE_AUTO_PH = 3,
  MODE_AUTO_TEMP = 4,
  MODE_AUTO_HUMIDITY = 5,
  MODE_AUTO_EC = 6
};

enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO = 1,
  LOG_WARN = 2,
  LOG_ERROR = 3
};

// =====================================================
// ESTRUTURAS DE DADOS
// =====================================================
struct RelayConfig {
  bool state;
  RelayMode mode;
  float threshold_min;
  float threshold_max;
  unsigned long cycle_on_time;
  unsigned long cycle_off_time;
  String led_schedule;
};

struct SensorData {
  float ph;
  float ec;
  float air_temp;
  float humidity;
  float water_temp;
  unsigned long timestamp;
  bool valid;
  String source;
};

struct Pulse {
  bool active;
  unsigned long start_time;
  unsigned long duration;
};

struct DiagnosticData {
  unsigned long uptime;
  unsigned long mqtt_reconnects;
  unsigned long wifi_reconnects;
  int wifi_rssi;
  unsigned long free_heap;
};

// Estrutura para dados BLE (32 bytes)
struct BLEData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  uint32_t timestamp;
  uint8_t reserved[7];
  uint8_t checksum;
};

// Estrutura para mensagem MQTT em fila
struct MQTTMessage {
  String topic;
  String payload;
  unsigned long timestamp;
};

// =====================================================
// VARIÁVEIS GLOBAIS - ESTADO DO DISPOSITIVO
// =====================================================
String deviceUUID = "";
bool authCompleted = false;
String authToken = "";
unsigned long authAttempts = 0;
const unsigned long MAX_AUTH_ATTEMPTS = 10;
const unsigned long AUTH_RETRY_INTERVAL = 30000;

// =====================================================
// VARIÁVEIS GLOBAIS - WIFI E AP MODE
// =====================================================
bool apMode = false;
String wifiSSID = "";
String wifiPassword = "";

// =====================================================
// VARIÁVEIS GLOBAIS - MQTT
// =====================================================
unsigned long lastMQTTAttempt = 0;
unsigned long mqttReconnectAttempts = 0;
const unsigned long MAX_MQTT_RECONNECT_ATTEMPTS = 10;
const unsigned long MQTT_RECONNECT_INTERVAL = 2000;
const size_t MAX_OUTBOX_SIZE = 50;
std::vector<MQTTMessage> mqttOutbox;

// =====================================================
// VARIÁVEIS GLOBAIS - SENSORES
// =====================================================
SensorData currentSensorData = {0, 0, 0, 0, 0, 0, false, "NONE"};
const unsigned long SENSOR_TIMEOUT = 180000;
unsigned long lastSensorDataWarning = 0;

// =====================================================
// VARIÁVEIS GLOBAIS - RELÉS
// =====================================================
RelayConfig relayConfigs[8];
Pulse pulses[8];

// =====================================================
// VARIÁVEIS GLOBAIS - TIMERS
// =====================================================
unsigned long lastStatusPublish = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastNTPUpdate = 0;
const unsigned long STATUS_PUBLISH_INTERVAL = 10000;
const unsigned long HEARTBEAT_INTERVAL = 60000;
const unsigned long NTP_UPDATE_INTERVAL = 3600000;

// =====================================================
// VARIÁVEIS GLOBAIS - BLE
// =====================================================
bool bleActive = false;
bool bleConnected = false;
String sensorBLEAddress = "";
unsigned long lastBLEScan = 0;
unsigned long lastBLERead = 0;
const unsigned long BLE_SCAN_INTERVAL = 10000;
const unsigned long BLE_READ_INTERVAL = 2000;
const unsigned long BLE_ACTIVATION_TIMEOUT = 180000;

// =====================================================
// VARIÁVEIS GLOBAIS - DIAGNÓSTICO
// =====================================================
DiagnosticData diagnostics = {0, 0, 0, 0, 0};

// =====================================================
// CALLBACKS BLE
// =====================================================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      
      sensorBLEAddress = advertisedDevice.getAddress().toString().c_str();
      Serial.println("Sensor BLE encontrado: " + sensorBLEAddress);
      advertisedDevice.getScan()->stop();
    }
  }
};

// =====================================================
// FUNÇÕES DE LOGGING
// =====================================================
void logMessage(LogLevel level, String message) {
  String levelStr;
  switch(level) {
    case LOG_DEBUG: levelStr = "DEBUG"; break;
    case LOG_INFO: levelStr = "INFO"; break;
    case LOG_WARN: levelStr = "WARN"; break;
    case LOG_ERROR: levelStr = "ERROR"; break;
  }
  
  String logMsg = "[" + levelStr + "] " + message;
  Serial.println(logMsg);
  
  if (mqttClient.connected()) {
    StaticJsonDocument<256> doc;
    doc["device_id"] = deviceUUID;
    doc["level"] = levelStr;
    doc["message"] = message;
    doc["timestamp"] = millis();
    
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(MQTT_LOG_TOPIC, payload.c_str(), false);
  }
}

// =====================================================
// FUNÇÕES DE VALIDAÇÃO
// =====================================================
bool isValidRelayIndex(int index) {
  return index >= 0 && index < 8;
}

bool isValidRelayMode(int mode) {
  return mode >= MODE_MANUAL && mode <= MODE_AUTO_EC;
}

bool isValidSensorData(const SensorData& data) {
  if (!data.valid) return false;
  if (data.ph < 0 || data.ph > 14) return false;
  if (data.ec < 0 || data.ec > 10000) return false;
  if (data.air_temp < -40 || data.air_temp > 80) return false;
  if (data.humidity < 0 || data.humidity > 100) return false;
  if (data.water_temp < -10 || data.water_temp > 50) return false;
  return true;
}

// =====================================================
// FUNÇÕES DE PERSISTÊNCIA (NVS)
// =====================================================
void loadConfig() {
  preferences.begin("hydrosmart", false);
  
  wifiSSID = preferences.getString("wifi_ssid", DEFAULT_SSID);
  wifiPassword = preferences.getString("wifi_pass", DEFAULT_PASSWORD);
  deviceUUID = preferences.getString("device_uuid", "");
  authToken = preferences.getString("auth_token", "");
  
  for (int i = 0; i < 8; i++) {
    String key = "relay_" + String(i);
    relayConfigs[i].state = preferences.getBool((key + "_state").c_str(), false);
    relayConfigs[i].mode = (RelayMode)preferences.getInt((key + "_mode").c_str(), MODE_MANUAL);
    relayConfigs[i].threshold_min = preferences.getFloat((key + "_min").c_str(), 0.0);
    relayConfigs[i].threshold_max = preferences.getFloat((key + "_max").c_str(), 0.0);
    relayConfigs[i].cycle_on_time = preferences.getULong((key + "_on").c_str(), 0);
    relayConfigs[i].cycle_off_time = preferences.getULong((key + "_off").c_str(), 0);
    relayConfigs[i].led_schedule = preferences.getString((key + "_sched").c_str(), "");
  }
  
  preferences.end();
  logMessage(LOG_INFO, "Configuração carregada da NVS");
}

void saveConfig() {
  preferences.begin("hydrosmart", false);
  preferences.putString("wifi_ssid", wifiSSID);
  preferences.putString("wifi_pass", wifiPassword);
  preferences.putString("device_uuid", deviceUUID);
  preferences.putString("auth_token", authToken);
  preferences.end();
  logMessage(LOG_INFO, "Configuração salva na NVS");
}

void saveRelayConfig(int index) {
  if (!isValidRelayIndex(index)) return;
  
  preferences.begin("hydrosmart", false);
  String key = "relay_" + String(index);
  preferences.putBool((key + "_state").c_str(), relayConfigs[index].state);
  preferences.putInt((key + "_mode").c_str(), relayConfigs[index].mode);
  preferences.putFloat((key + "_min").c_str(), relayConfigs[index].threshold_min);
  preferences.putFloat((key + "_max").c_str(), relayConfigs[index].threshold_max);
  preferences.putULong((key + "_on").c_str(), relayConfigs[index].cycle_on_time);
  preferences.putULong((key + "_off").c_str(), relayConfigs[index].cycle_off_time);
  preferences.putString((key + "_sched").c_str(), relayConfigs[index].led_schedule);
  preferences.end();
}

// =====================================================
// FUNÇÕES DE WIFI E AP MODE
// =====================================================
void startAPMode() {
  logMessage(LOG_WARN, "Iniciando modo AP para configuração...");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  logMessage(LOG_INFO, "AP IP: " + IP.toString());
  
  server.on("/", HTTP_GET, []() {
    String html = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>HydroActuator Config</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }
        h1 { color: #0066cc; }
        input { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 4px; }
        button { width: 100%; padding: 12px; background: #0066cc; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        button:hover { background: #0052a3; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Configuração WiFi</h1>
        <form action="/save" method="POST">
          <input type="text" name="ssid" placeholder="Nome da Rede (SSID)" required>
          <input type="password" name="password" placeholder="Senha WiFi" required>
          <button type="submit">Salvar e Reiniciar</button>
        </form>
      </div>
    </body>
    </html>
    )";
    server.send(200, "text/html", html);
  });
  
  server.on("/save", HTTP_POST, []() {
    wifiSSID = server.arg("ssid");
    wifiPassword = server.arg("password");
    
    saveConfig();
    
    String html = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Configuração Salva</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <meta http-equiv="refresh" content="3;url=/">
      <style>
        body { font-family: Arial; text-align: center; margin-top: 50px; }
        .success { color: green; font-size: 24px; }
      </style>
    </head>
    <body>
      <div class="success">✓ Configuração salva!</div>
      <p>Reiniciando...</p>
    </body>
    </html>
    )";
    
    server.send(200, "text/html", html);
    delay(2000);
    ESP.restart();
  });
  
  server.begin();
  apMode = true;
}

void setupWifi() {
  if (wifiSSID == DEFAULT_SSID || wifiSSID.length() == 0) {
    startAPMode();
    return;
  }
  
  logMessage(LOG_INFO, "Conectando ao WiFi: " + wifiSSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    logMessage(LOG_INFO, "WiFi conectado! IP: " + WiFi.localIP().toString());
    diagnostics.wifi_rssi = WiFi.RSSI();
  } else {
    logMessage(LOG_ERROR, "Falha ao conectar WiFi - iniciando AP");
    startAPMode();
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED && !apMode) {
    diagnostics.wifi_reconnects++;
    logMessage(LOG_WARN, "WiFi desconectado - tentando reconectar...");
    WiFi.reconnect();
  }
}

// =====================================================
// FUNÇÕES DE AUTENTICAÇÃO
// =====================================================
bool authenticateDevice() {
  if (deviceUUID.length() == 0) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    deviceUUID = "ACT_" + String(macStr);
    saveConfig();
    logMessage(LOG_INFO, "UUID gerado: " + deviceUUID);
  }
  
  logMessage(LOG_INFO, "Autenticando dispositivo: " + deviceUUID);
  
  WiFiClientSecure client;
  client.setCACert(HIVEMQ_ROOT_CA);
  
  HTTPClient http;
  http.begin(client, AUTH_SERVER);
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<256> doc;
  doc["device_id"] = deviceUUID;
  doc["device_type"] = "actuator";
  doc["firmware_version"] = FIRMWARE_VERSION;
  
  String payload;
  serializeJson(doc, payload);
  
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<512> respDoc;
    DeserializationError error = deserializeJson(respDoc, response);
    
    if (!error && respDoc["authenticated"] == true) {
      authToken = respDoc["token"].as<String>();
      authCompleted = true;
      saveConfig();
      logMessage(LOG_INFO, "Autenticação bem-sucedida!");
      http.end();
      return true;
    }
  }
  
  logMessage(LOG_ERROR, "Falha na autenticação (HTTP " + String(httpCode) + ")");
  http.end();
  return false;
}

// =====================================================
// FUNÇÕES NTP
// =====================================================
void setupNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  logMessage(LOG_INFO, "NTP configurado");
}

void updateNTP() {
  if (millis() - lastNTPUpdate > NTP_UPDATE_INTERVAL) {
    lastNTPUpdate = millis();
    configTime(0, 0, "pool.ntp.org");
  }
}

String getTimestamp() {
  time_t now;
  time(&now);
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  return String(buf);
}

// =====================================================
// FUNÇÕES MQTT
// =====================================================
void addToOutbox(String topic, String payload) {
  if (mqttOutbox.size() >= MAX_OUTBOX_SIZE) {
    mqttOutbox.erase(mqttOutbox.begin());
  }
  
  MQTTMessage msg;
  msg.topic = topic;
  msg.payload = payload;
  msg.timestamp = millis();
  mqttOutbox.push_back(msg);
  
  logMessage(LOG_DEBUG, "Mensagem adicionada à fila (" + String(mqttOutbox.size()) + " msgs)");
}

void flushOutbox() {
  if (mqttOutbox.empty() || !mqttClient.connected()) return;
  
  logMessage(LOG_INFO, "Enviando " + String(mqttOutbox.size()) + " mensagens da fila...");
  
  auto it = mqttOutbox.begin();
  while (it != mqttOutbox.end()) {
    if (mqttClient.publish(it->topic.c_str(), it->payload.c_str(), false)) {
      it = mqttOutbox.erase(it);
    } else {
      logMessage(LOG_WARN, "Falha ao enviar mensagem - mantendo na fila");
      break;
    }
    esp_task_wdt_reset();
  }
}

void publishRelayStatus() {
  StaticJsonDocument<1024> doc;
  doc["device_id"] = deviceUUID;
  doc["timestamp"] = getTimestamp();
  
  JsonArray relays = doc.createNestedArray("relays");
  for (int i = 0; i < 8; i++) {
    JsonObject relay = relays.createNestedObject();
    relay["index"] = i;
    relay["state"] = relayConfigs[i].state;
    relay["mode"] = relayConfigs[i].mode;
    relay["pin"] = RELAY_PINS[i];
  }
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_RELAY_STATUS, payload.c_str(), false);
  } else {
    addToOutbox(MQTT_RELAY_STATUS, payload);
  }
}

void publishHeartbeat() {
  diagnostics.uptime = millis() / 1000;
  diagnostics.free_heap = ESP.getFreeHeap();
  diagnostics.wifi_rssi = WiFi.RSSI();
  
  StaticJsonDocument<1024> doc;
  doc["device_id"] = deviceUUID;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["timestamp"] = getTimestamp();
  doc["uptime_seconds"] = diagnostics.uptime;
  
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["ssid"] = wifiSSID;
  wifi["rssi"] = diagnostics.wifi_rssi;
  wifi["ip"] = WiFi.localIP().toString();
  wifi["reconnects"] = diagnostics.wifi_reconnects;
  
  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["connected"] = mqttClient.connected();
  mqtt["reconnects"] = diagnostics.mqtt_reconnects;
  mqtt["outbox_size"] = mqttOutbox.size();
  
  JsonObject ble = doc.createNestedObject("ble");
  ble["active"] = bleActive;
  ble["connected"] = bleConnected;
  ble["sensor_address"] = sensorBLEAddress;
  
  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = diagnostics.free_heap;
  
  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["valid"] = currentSensorData.valid;
  sensors["source"] = currentSensorData.source;
  if (currentSensorData.valid) {
    sensors["age_ms"] = millis() - currentSensorData.timestamp;
  }
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_HEARTBEAT, payload.c_str(), false);
  } else {
    addToOutbox(MQTT_HEARTBEAT, payload);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String payloadStr = "";
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  
  logMessage(LOG_DEBUG, "MQTT recebido: " + topicStr);
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payloadStr);
  
  if (error) {
    logMessage(LOG_ERROR, "Erro ao parsear JSON MQTT");
    return;
  }
  
  // Receber dados de sensores
  if (topicStr == MQTT_SENSOR_DATA) {
    currentSensorData.ph = doc["ph"] | 0.0;
    currentSensorData.ec = doc["ec"] | 0.0;
    currentSensorData.air_temp = doc["air_temp"] | 0.0;
    currentSensorData.humidity = doc["humidity"] | 0.0;
    currentSensorData.water_temp = doc["water_temp"] | 0.0;
    currentSensorData.timestamp = millis();
    currentSensorData.valid = isValidSensorData(currentSensorData);
    currentSensorData.source = "MQTT";
    
    if (currentSensorData.valid) {
      logMessage(LOG_DEBUG, "Dados de sensores atualizados via MQTT");
    }
  }
  
  // Comandos de relé
  else if (topicStr == MQTT_RELAY_COMMAND) {
    int relayIndex = doc["relay_index"] | -1;
    
    if (!isValidRelayIndex(relayIndex)) {
      logMessage(LOG_ERROR, "Índice de relé inválido: " + String(relayIndex));
      return;
    }
    
    if (doc.containsKey("state")) {
      bool newState = doc["state"];
      relayConfigs[relayIndex].state = newState;
      digitalWrite(RELAY_PINS[relayIndex], newState ? HIGH : LOW);
      saveRelayConfig(relayIndex);
      logMessage(LOG_INFO, "Relé " + String(relayIndex) + " -> " + (newState ? "ON" : "OFF"));
    }
    
    if (doc.containsKey("mode")) {
      int mode = doc["mode"];
      if (isValidRelayMode(mode)) {
        relayConfigs[relayIndex].mode = (RelayMode)mode;
        saveRelayConfig(relayIndex);
        logMessage(LOG_INFO, "Modo relé " + String(relayIndex) + " -> " + String(mode));
      }
    }
    
    if (doc.containsKey("config")) {
      JsonObject config = doc["config"];
      relayConfigs[relayIndex].threshold_min = config["threshold_min"] | 0.0;
      relayConfigs[relayIndex].threshold_max = config["threshold_max"] | 0.0;
      relayConfigs[relayIndex].cycle_on_time = config["cycle_on_time"] | 0;
      relayConfigs[relayIndex].cycle_off_time = config["cycle_off_time"] | 0;
      relayConfigs[relayIndex].led_schedule = config["led_schedule"] | "";
      saveRelayConfig(relayIndex);
      logMessage(LOG_INFO, "Config relé " + String(relayIndex) + " atualizada");
    }
    
    publishRelayStatus();
  }
  
  // Configuração WiFi
  else if (topicStr == MQTT_WIFI_CONFIG) {
    String newSSID = doc["ssid"] | "";
    String newPass = doc["password"] | "";
    
    if (newSSID.length() > 0) {
      wifiSSID = newSSID;
      wifiPassword = newPass;
      saveConfig();
      logMessage(LOG_INFO, "Nova configuração WiFi recebida - reiniciando...");
      delay(1000);
      ESP.restart();
    }
  }
}

void setupMQTT() {
  wifiClient.setCACert(HIVEMQ_ROOT_CA);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(2048);
  logMessage(LOG_INFO, "MQTT configurado");
}

bool connectMQTT() {
  String clientId = "actuator_" + deviceUUID;
  
  logMessage(LOG_INFO, "Conectando ao MQTT...");
  
  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
    logMessage(LOG_INFO, "MQTT conectado!");
    
    mqttClient.subscribe(MQTT_SENSOR_DATA);
    mqttClient.subscribe(MQTT_RELAY_COMMAND);
    mqttClient.subscribe(MQTT_WIFI_CONFIG);
    
    publishHeartbeat();
    publishRelayStatus();
    
    return true;
  }
  
  logMessage(LOG_ERROR, "Falha MQTT, estado: " + String(mqttClient.state()));
  return false;
}

void reconnectMQTT() {
  if (mqttClient.connected() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  // Calcular backoff exponencial: 2s, 4s, 8s, 16s, 32s, 60s (max)
  unsigned long backoff = MQTT_RECONNECT_INTERVAL * (1UL << mqttReconnectAttempts);
  if (backoff > 60000) backoff = 60000;

  if (millis() - lastMQTTAttempt < backoff) {
    return;
  }

  logMessage(LOG_INFO, "Tentando reconexão MQTT... (tentativa " +
             String(mqttReconnectAttempts + 1) + ", aguardou " + String(backoff/1000) + "s)");

  lastMQTTAttempt = millis();
  esp_task_wdt_reset();

  if (connectMQTT()) {
    logMessage(LOG_INFO, "MQTT reconectado com sucesso!");
    mqttReconnectAttempts = 0;
    diagnostics.mqtt_reconnects++;
    flushOutbox();
  } else {
    mqttReconnectAttempts++;
    unsigned long nextBackoff = MQTT_RECONNECT_INTERVAL * (1UL << mqttReconnectAttempts);
    if (nextBackoff > 60000) nextBackoff = 60000;
    logMessage(LOG_WARN, "Reconexão MQTT falhou, próxima tentativa em " + String(nextBackoff/1000) + "s");
  }
}

// =====================================================
// FUNÇÕES DE CONTROLE DE RELÉS
// =====================================================
void setupRelays() {
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], relayConfigs[i].state ? HIGH : LOW);
    pulses[i].active = false;
  }
  logMessage(LOG_INFO, "Relés inicializados");
}

void updateRelay(int index, bool state) {
  if (!isValidRelayIndex(index)) return;
  
  relayConfigs[index].state = state;
  digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
}

void startPulse(int index, unsigned long duration) {
  if (!isValidRelayIndex(index)) return;
  
  pulses[index].active = true;
  pulses[index].start_time = millis();
  pulses[index].duration = duration;
  updateRelay(index, true);
}

void updatePulses() {
  for (int i = 0; i < 8; i++) {
    if (pulses[i].active && (millis() - pulses[i].start_time >= pulses[i].duration)) {
      pulses[i].active = false;
      updateRelay(i, false);
    }
  }
}

void updateAutomaticRelays() {
  if (!currentSensorData.valid) return;
  
  for (int i = 0; i < 8; i++) {
    if (relayConfigs[i].mode == MODE_MANUAL || pulses[i].active) continue;
    
    float value = 0.0;
    bool shouldActivate = false;
    
    switch (relayConfigs[i].mode) {
      case MODE_AUTO_PH:
        value = currentSensorData.ph;
        shouldActivate = (value < relayConfigs[i].threshold_min || value > relayConfigs[i].threshold_max);
        break;
        
      case MODE_AUTO_EC:
        value = currentSensorData.ec;
        shouldActivate = (value < relayConfigs[i].threshold_min || value > relayConfigs[i].threshold_max);
        break;
        
      case MODE_AUTO_TEMP:
        value = currentSensorData.air_temp;
        shouldActivate = (value < relayConfigs[i].threshold_min || value > relayConfigs[i].threshold_max);
        break;
        
      case MODE_AUTO_HUMIDITY:
        value = currentSensorData.humidity;
        shouldActivate = (value < relayConfigs[i].threshold_min);
        break;
        
      case MODE_AUTO_CYCLE:
        {
          static unsigned long cycleTimers[8] = {0};
          unsigned long elapsed = millis() - cycleTimers[i];
          
          if (relayConfigs[i].state && elapsed >= relayConfigs[i].cycle_on_time) {
            updateRelay(i, false);
            cycleTimers[i] = millis();
          } else if (!relayConfigs[i].state && elapsed >= relayConfigs[i].cycle_off_time) {
            updateRelay(i, true);
            cycleTimers[i] = millis();
          }
        }
        continue;
        
      default:
        continue;
    }
    
    if (shouldActivate != relayConfigs[i].state) {
      updateRelay(i, shouldActivate);
      saveRelayConfig(i);
    }
  }
}

// =====================================================
// FUNÇÕES BLE
// =====================================================
void setupBLE() {
  logMessage(LOG_INFO, "Inicializando BLE Client...");
  
  String bleName = "HydroActuator_" + deviceUUID.substring(deviceUUID.length() - 8);
  BLEDevice::init(bleName.c_str());
  
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
  bleActive = true;
  logMessage(LOG_INFO, "BLE Client ativo - procurando sensor...");
}

void scanBLE() {
  if (!bleActive || bleConnected) return;
  
  if (millis() - lastBLEScan < BLE_SCAN_INTERVAL) return;
  lastBLEScan = millis();
  
  logMessage(LOG_DEBUG, "Escaneando sensores BLE...");
  BLEScanResults* foundDevices = pBLEScan->start(3, false);
  pBLEScan->clearResults();
  
  if (sensorBLEAddress.length() > 0 && !bleConnected) {
    connectToBLEServer();
  }
}

void connectToBLEServer() {
  if (bleConnected || sensorBLEAddress.length() == 0) return;
  
  logMessage(LOG_INFO, "Conectando ao sensor BLE: " + sensorBLEAddress);
  
  pClient = BLEDevice::createClient();
  BLEAddress addr(sensorBLEAddress.c_str());
  
  if (pClient->connect(addr)) {
    logMessage(LOG_INFO, "BLE conectado!");
    
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService != nullptr) {
      pRemoteChar = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
      if (pRemoteChar != nullptr) {
        bleConnected = true;
        logMessage(LOG_INFO, "Característica BLE obtida - leitura ativa");
      } else {
        logMessage(LOG_ERROR, "Característica BLE não encontrada");
        pClient->disconnect();
      }
    } else {
      logMessage(LOG_ERROR, "Serviço BLE não encontrado");
      pClient->disconnect();
    }
  } else {
    logMessage(LOG_ERROR, "Falha ao conectar ao sensor BLE");
  }
}

void readBLEData() {
  if (!bleConnected || pRemoteChar == nullptr) return;
  
  if (millis() - lastBLERead < BLE_READ_INTERVAL) return;
  lastBLERead = millis();
  
  try {
    std::string value = pRemoteChar->readValue();
    
    if (value.length() == sizeof(BLEData)) {
      BLEData bleData;
      memcpy(&bleData, value.data(), sizeof(BLEData));
      
      uint8_t* bytes = (uint8_t*)&bleData;
      uint8_t checksum = 0;
      for (size_t i = 0; i < sizeof(BLEData) - 1; i++) {
        checksum ^= bytes[i];
      }
      
      if (checksum == bleData.checksum) {
        currentSensorData.ph = bleData.ph;
        currentSensorData.ec = bleData.ec;
        currentSensorData.air_temp = bleData.airTemp;
        currentSensorData.humidity = bleData.humidity;
        currentSensorData.water_temp = bleData.waterTemp;
        currentSensorData.timestamp = millis();
        currentSensorData.valid = true;
        currentSensorData.source = "BLE";
        
        logMessage(LOG_DEBUG, "Dados BLE: pH=" + String(bleData.ph, 2) + 
                              " EC=" + String(bleData.ec) + 
                              " T=" + String(bleData.airTemp, 1));
      } else {
        logMessage(LOG_WARN, "Checksum BLE inválido");
      }
    }
  } catch (...) {
    logMessage(LOG_ERROR, "Erro ao ler dados BLE - reconectando...");
    bleConnected = false;
    if (pClient != nullptr) {
      pClient->disconnect();
    }
  }
}

// =====================================================
// WATCHDOG
// =====================================================
void initWatchdog() {
  esp_task_wdt_deinit();
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  logMessage(LOG_INFO, "Watchdog inicializado (30s)");
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("AquaSys Nexus - Actuator Module");
  Serial.println("Firmware Version: " + String(FIRMWARE_VERSION));
  Serial.println("========================================\n");
  
  initWatchdog();
  
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
    Serial.println("Botão BOOT pressionado - entrando em modo AP");
    delay(1000);
    if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
      startAPMode();
      return;
    }
  }
  
  loadConfig();
  setupRelays();
  setupWifi();
  
  if (!apMode) {
    setupNTP();
    
    if (!authenticateDevice()) {
      logMessage(LOG_ERROR, "Falha na autenticação inicial");
    }
    
    setupMQTT();
    connectMQTT();
  }
  
  logMessage(LOG_INFO, "Setup concluído!");
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================
void loop() {
  esp_task_wdt_reset();
  
  // 1. Modo AP tem prioridade
  if (apMode) {
    server.handleClient();
    delay(10);
    return;
  }
  
  // 2. Re-autenticação se necessário
  if (!authCompleted && (millis() > 5000)) {
    if (authAttempts < MAX_AUTH_ATTEMPTS) {
      static unsigned long lastAuthAttempt = 0;
      if (millis() - lastAuthAttempt > AUTH_RETRY_INTERVAL) {
        lastAuthAttempt = millis();
        authAttempts++;
        authenticateDevice();
      }
    }
  }
  
  // 3. Verificar WiFi
  if (WiFi.status() != WL_CONNECTED) {
    checkWiFi();
    delay(1000);
    return;
  }
  
  // 4. Atualizar NTP periodicamente
  updateNTP();
  
  // 5. Tentar reconectar MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }
  
  // 6. Ativar BLE se MQTT falhar por muito tempo
  if (!bleActive && !mqttClient.connected()) {
    if (millis() - lastMQTTAttempt > BLE_ACTIVATION_TIMEOUT) {
      logMessage(LOG_WARN, "MQTT offline >3min - ativando BLE");
      setupBLE();
    }
  }
  
  // 7. Operações BLE
  if (bleActive) {
    if (!bleConnected) {
      scanBLE();
    } else {
      readBLEData();
    }
  }
  
  // 8. Atualizar pulsos
  updatePulses();
  
  // 9. Lógica automática de relés (a cada 5s)
  static unsigned long lastAutoUpdate = 0;
  if (millis() - lastAutoUpdate >= 5000) {
    lastAutoUpdate = millis();
    updateAutomaticRelays();
  }
  
  // 10. Publicar status de relés (a cada 10s)
  if (millis() - lastStatusPublish >= STATUS_PUBLISH_INTERVAL) {
    lastStatusPublish = millis();
    publishRelayStatus();
  }
  
  // 11. Publicar heartbeat (a cada 60s)
  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = millis();
    publishHeartbeat();
  }
  
  // 12. Verificar dados obsoletos de sensores
  if (currentSensorData.valid && (millis() - currentSensorData.timestamp > SENSOR_TIMEOUT)) {
    if (millis() - lastSensorDataWarning > 60000) {
      lastSensorDataWarning = millis();
      logMessage(LOG_WARN, "Dados de sensores obsoletos (>3min) - verificar MQTT/BLE");
    }
  }
  
  delay(10);
}
