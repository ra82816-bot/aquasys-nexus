/*
 * AquaSys Nexus - Actuator Module v4.2.6-BLE-TYPE-FIX
 * ==============================================
 * ✅ CORREÇÕES v4.2.6:
 * - Correção definitiva de tipo BLE readValue()
 * - readValue() retorna Arduino String (não std::string)
 * - Simplificação do código BLE sem conversões desnecessárias
 * - Fix compilação: conversion from 'String' to non-scalar type 'std::string'
 * 
 * ✅ CORREÇÕES v4.2.5:
 * - Correção de tipos BLE em startBLEScan()
 * - Compatibilidade com Arduino String nas funções BLE
 * - Fix compilação: 'class String' has no member named 'find'
 * 
 * ✅ CORREÇÕES v4.2.4:
 * - Clientes SSL separados para HTTP (Supabase) e MQTT (HiveMQ)
 * - Gerenciamento independente de certificados SSL por serviço
 * - Suporte a modo insecure isolado por conexão
 * - Correção do erro PEM -4396 (conflito de estado SSL entre serviços)
 * - Uso de memória: +8KB RAM (2 clientes SSL) para maior estabilidade
 * 
 * IMPORTANTE - Tipos da Biblioteca BLE:
 * - BLEAdvertisedDevice.getName() → retorna std::string
 * - BLERemoteCharacteristic.readValue() → retorna Arduino String
 * - Sempre verificar o tipo de retorno antes de converter
 * 
 * Correções anteriores mantidas:
 * - NTP sincroniza ANTES da autenticação SSL (v4.2.0)
 * - Logging detalhado de SSL (v4.2.3)
 * - WDT reset em AP mode (v4.1.4)
 */

// ==================== INCLUDES ====================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <time.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <esp_task_wdt.h>
#include <esp_mac.h>

// ==================== CONFIGURAÇÕES ====================
// WiFi
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// Servidor de Autenticação
const char* AUTH_SERVER = "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth";
const char* AUTH_HEADER_KEY = "apikey";
const char* AUTH_HEADER_VALUE = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw";

// MQTT Broker
const char* MQTT_BROKER = "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;

// Tópicos MQTT
const char* TOPIC_SENSORS = "aquasys/sensors/all";
const char* TOPIC_HEARTBEAT = "aquasys/heartbeat";
const char* TOPIC_RELAY_STATUS = "aquasys/relay/status";
const char* TOPIC_RELAY_COMMAND = "aquasys/relay/command";
const char* TOPIC_RELAY_CONFIG = "aquasys/relay/config";
const char* TOPIC_CALIBRATION = "aquasys/calibration";
const char* TOPIC_OTA = "aquasys/ota";

// ✅ Certificado Root CA para Supabase (Let's Encrypt ISRG Root X1)
const char* SUPABASE_ROOT_CA = R"EOF(
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
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X+1mvoiBOv/2X/qkSsisRcOj/KK
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

// Certificado HiveMQ (mesmo que o Supabase)
const char* HIVEMQ_ROOT_CA = SUPABASE_ROOT_CA;

// ✅ MODO DEBUG SSL: Use 'true' para desabilitar validação SSL (apenas para debug!)
// ⚠️ ATENÇÃO: Desabilitar validação SSL é INSEGURO e deve ser usado apenas para diagnóstico
// ⚠️ Em produção, sempre use SSL_INSECURE_MODE = false
const bool SSL_INSECURE_MODE = true;  // ✅ Habilitado temporariamente para debug

// UUIDs BLE
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ==================== HARDWARE ====================
const int RELAY_PINS[8] = {23, 5, 4, 13, 22, 21, 14, 12};
const int SETUP_BUTTON_PIN = 0;

// ==================== OBJETOS GLOBAIS ====================
// ✅ v4.2.4: Clientes SSL separados para evitar conflitos
WiFiClientSecure httpClient;      // Dedicado para autenticação HTTP (Supabase)
WiFiClientSecure mqttWifiClient;  // Dedicado para conexão MQTT (HiveMQ)
PubSubClient mqttClient(mqttWifiClient);
Preferences preferences;
WebServer server(80);
BLEScan* pBLEScan = nullptr;
BLEClient* pBLEClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

// ==================== ENUMS ====================
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

// ==================== ESTRUTURAS ====================
struct RelayConfig {
  RelayMode mode;
  bool state;
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
  float ec;
  float temperature;
  float humidity;
  float water_temp;
  bool valid;
  unsigned long timestamp;
};

struct Pulse {
  int relayIndex;
  bool state;
  unsigned long endTime;
  bool active;
};

struct DiagnosticData {
  unsigned long uptime;
  int rssi;
  unsigned long freeHeap;
  unsigned long heapSize;
  bool mqttConnected;
  bool wifiConnected;
  String ipAddress;
  int mqttReconnects;
  float avg_ph_24h;
};

struct MQTTMessage {
  String topic;
  String payload;
};

// ==================== VARIÁVEIS GLOBAIS ====================
String deviceUUID = "";
String mqttUsername = "";
String mqttPassword = "";

bool authCompleted = false;
unsigned long lastAuthAttempt = 0;
const unsigned long AUTH_RETRY_INTERVAL = 30000;
int authFailureCount = 0;
const int MAX_AUTH_FAILURES = 5;

bool wifiConnected = false;
bool mqttConnected = false;
unsigned long lastWifiCheck = 0;
unsigned long lastMqttCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
const unsigned long MQTT_CHECK_INTERVAL = 5000;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

bool apMode = false;
String apSSID = "";
String apPassword = "";

RelayConfig relayConfigs[8];
Pulse activePulses[8];

const unsigned long RELAY_DEBOUNCE = 500;
unsigned long lastRelayChange[8] = {0};
const unsigned long PH_COOLDOWN = 300000;
const unsigned long EC_COOLDOWN = 600000;
unsigned long lastPhAction = 0;
unsigned long lastEcAction = 0;

SensorData currentSensorData = {0, 0, 0, 0, 0, false, 0};
const unsigned long SENSOR_DATA_TIMEOUT = 180000;

float ph_history[24] = {0};
int ph_history_index = 0;
unsigned long last_ph_log = 0;
const unsigned long PH_LOG_INTERVAL = 3600000;

unsigned long lastStatusPublish = 0;
unsigned long lastHeartbeat = 0;
const unsigned long STATUS_PUBLISH_INTERVAL = 5000;
const unsigned long HEARTBEAT_INTERVAL = 60000;

const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;
unsigned long lastNtpUpdate = 0;
const unsigned long NTP_UPDATE_INTERVAL = 3600000;
bool ntpSynced = false;

int mqttReconnectAttempts = 0;
unsigned long lastMqttReconnect = 0;

const int MAX_OUTBOX_SIZE = 50;
MQTTMessage outbox[MAX_OUTBOX_SIZE];
int outboxCount = 0;

bool bleInitialized = false;
bool bleClientConnected = false;
String connectedSensorAddress = "";
unsigned long lastBleDataReceived = 0;
unsigned long lastBleScan = 0;
const unsigned long BLE_SCAN_INTERVAL = 30000;
bool bleActivated = false;

const int WDT_TIMEOUT = 30;

// ==================== PROTÓTIPOS ====================
void initWatchdog();
void generateDeviceUUID();
void setupRelays();
void loadConfig();
void setupBLE();
void setupWiFi();
bool authenticateDevice();
void setupNTP();
void updateNTP();
void setupMQTT();
void checkWiFi();
void reconnectMQTT();
void updateRelays();
void updatePulses();
void publishRelayStatus();
void publishHeartbeat();
void addToOutbox(String topic, String payload);
void flushOutbox();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void updateRelay(int index, bool state, String reason);
void startPulse(int relayIndex, bool state, unsigned long duration);
void updateAutomaticRelays();
void saveRelayConfig(int index);
bool isValidRelayIndex(int index);
bool isValidRelayMode(int mode);
bool isValidSensorData(const SensorData& data);
void logMessage(LogLevel level, String message);
String getTimestamp();
void logHourlyPH(float ph);
float calculate24hAveragePH();
void startAPMode();
void handleRoot();
void handleSave();
void startBLEScan();
void connectToSensorBLE(String address);
void readSensorDataBLE();
void disconnectBLE();

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  initWatchdog();
  logMessage(LOG_INFO, "=== AquaSys Actuator v4.2.4-DUAL-SSL ===");

  generateDeviceUUID();
  logMessage(LOG_INFO, "Device UUID: " + deviceUUID);

  setupRelays();
  loadConfig();
  
  esp_task_wdt_reset();
  setupWiFi();

  // ✅ CRÍTICO: Sincronizar NTP ANTES da autenticação SSL
  if (wifiConnected && !apMode) {
    esp_task_wdt_reset();
    logMessage(LOG_INFO, "Sincronizando NTP...");
    setupNTP();
    
    // ✅ Aguardar sincronização NTP (máx 10s) - ESSENCIAL para SSL
    int ntpAttempts = 0;
    while (ntpAttempts < 20) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        ntpSynced = true;
        logMessage(LOG_INFO, "NTP sincronizado: " + getTimestamp());
        break;
      }
      delay(500);
      ntpAttempts++;
      if (ntpAttempts % 4 == 0) {
        esp_task_wdt_reset();
      }
    }
    
    if (ntpAttempts >= 20) {
      logMessage(LOG_ERROR, "NTP NÃO SINCRONIZOU - SSL pode falhar!");
      if (!SSL_INSECURE_MODE) {
        logMessage(LOG_ERROR, "Recomendado: ativar SSL_INSECURE_MODE para debug");
      }
    }
  }

  // ✅ Autenticação só após verificar NTP
  if (wifiConnected && !apMode) {
    esp_task_wdt_reset();
    
    // ⚠️ Verificar se NTP está sincronizado antes de SSL
    if (!ntpSynced && !SSL_INSECURE_MODE) {
      logMessage(LOG_WARN, "NTP não sincronizado - SSL com certificado pode falhar");
      logMessage(LOG_WARN, "Considere ativar SSL_INSECURE_MODE para debug");
    }
    
    authCompleted = authenticateDevice();
    if (!authCompleted) {
      authFailureCount++;
      logMessage(LOG_WARN, "Auth falhou. Tentativas: " + String(authFailureCount));
    } else {
      authFailureCount = 0;
    }
  }

  if (authCompleted) {
    esp_task_wdt_reset();
    setupMQTT();
  }

  logMessage(LOG_INFO, "Setup completo!");
}

// ==================== LOOP ====================
void loop() {
  esp_task_wdt_reset();

  if (apMode) {
    esp_task_wdt_reset();
    server.handleClient();
    delay(10);
    return;
  }
  
  if (mqttConnected && bleActivated) {
    logMessage(LOG_INFO, "MQTT reconectado - desativando fallback BLE");
    bleActivated = false;
    disconnectBLE();
    if (pBLEScan != nullptr) {
      pBLEScan->stop();
    }
  }
  
  if (!authCompleted && authFailureCount >= MAX_AUTH_FAILURES) {
    logMessage(LOG_ERROR, "5 falhas de autenticação consecutivas - reiniciando...");
    delay(2000);
    ESP.restart();
  }

  if (!authCompleted && millis() - lastAuthAttempt > AUTH_RETRY_INTERVAL) {
    lastAuthAttempt = millis();
    esp_task_wdt_reset();
    authCompleted = authenticateDevice();
    if (authCompleted) {
      authFailureCount = 0;
      setupMQTT();
    } else {
      authFailureCount++;
      logMessage(LOG_WARN, "Re-auth falhou. Tentativas: " + String(authFailureCount));
    }
  }

  if (millis() - lastWifiCheck > WIFI_CHECK_INTERVAL) {
    lastWifiCheck = millis();
    checkWiFi();
  }

  if (authCompleted) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    } else {
      mqttClient.loop();
      mqttConnected = true;
      if (outboxCount > 0) {
        flushOutbox();
      }
    }
  }

  if (!mqttConnected && !bleActivated &&
      (millis() - currentSensorData.timestamp > SENSOR_DATA_TIMEOUT)) {
    logMessage(LOG_WARN, "MQTT offline > 3min - ativando BLE Client");
    bleActivated = true;
    if (!bleInitialized) {
      esp_task_wdt_reset();
      setupBLE();
      bleInitialized = true;
    }
  }

  if (bleActivated && !bleClientConnected &&
      (millis() - lastBleScan > BLE_SCAN_INTERVAL)) {
    lastBleScan = millis();
    startBLEScan();
  }

  if (bleClientConnected) {
    readSensorDataBLE();
  }

  updateRelays();
  updatePulses();
  updateAutomaticRelays();

  if (mqttConnected && millis() - lastStatusPublish > STATUS_PUBLISH_INTERVAL) {
    lastStatusPublish = millis();
    publishRelayStatus();
  }

  if (mqttConnected && millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    lastHeartbeat = millis();
    publishHeartbeat();
  }

  if (wifiConnected && millis() - lastNtpUpdate > NTP_UPDATE_INTERVAL) {
    lastNtpUpdate = millis();
    updateNTP();
  }

  if (millis() - currentSensorData.timestamp > SENSOR_DATA_TIMEOUT) {
    if (currentSensorData.valid) {
      currentSensorData.valid = false;
      logMessage(LOG_WARN, "Sensor data timeout - marcando como inválido");
    }
  }
}

// ==================== WATCHDOG ====================
void initWatchdog() {
  esp_err_t deinit_result = esp_task_wdt_deinit();
  if (deinit_result != ESP_OK) {
    logMessage(LOG_DEBUG, "Falha ao des-init WDT (pode ser o 1º boot): " + String(deinit_result));
  }

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_err_t init_result = esp_task_wdt_init(&wdt_config);

  if (init_result == ESP_OK) {
    esp_task_wdt_add(NULL);
    logMessage(LOG_INFO, "Watchdog inicializado (30s timeout)");
  } else {
    logMessage(LOG_ERROR, "Falha ao inicializar WDT: " + String(init_result));
  }
}

// ==================== UUID ====================
void generateDeviceUUID() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  
  char uuid[32];
  snprintf(uuid, sizeof(uuid), "ACT-%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  deviceUUID = String(uuid);
}

// ==================== RELAYS ====================
void setupRelays() {
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    
    relayConfigs[i].mode = MODE_MANUAL_OFF;
    relayConfigs[i].state = false;
    relayConfigs[i].ph_min = 5.5;
    relayConfigs[i].ph_max = 6.5;
    relayConfigs[i].ec_min = 1.0;
    relayConfigs[i].ec_max = 2.5;
    relayConfigs[i].temp_min = 18.0;
    relayConfigs[i].temp_max = 26.0;
    relayConfigs[i].humidity_min = 50.0;
    relayConfigs[i].humidity_max = 70.0;
    relayConfigs[i].timer_on_duration = 60000;
    relayConfigs[i].timer_off_duration = 300000;
    relayConfigs[i].last_timer_change = 0;
    relayConfigs[i].timer_state = false;
    
    activePulses[i].active = false;
  }
  logMessage(LOG_INFO, "Relés inicializados");
}

void loadConfig() {
  preferences.begin("actuator", false);
  
  for (int i = 0; i < 8; i++) {
    String prefix = "relay" + String(i) + "_";
    
    relayConfigs[i].mode = (RelayMode)preferences.getUInt((prefix + "mode").c_str(), MODE_MANUAL_OFF);
    relayConfigs[i].ph_min = preferences.getFloat((prefix + "ph_min").c_str(), 5.5);
    relayConfigs[i].ph_max = preferences.getFloat((prefix + "ph_max").c_str(), 6.5);
    relayConfigs[i].ec_min = preferences.getFloat((prefix + "ec_min").c_str(), 1.0);
    relayConfigs[i].ec_max = preferences.getFloat((prefix + "ec_max").c_str(), 2.5);
    relayConfigs[i].temp_min = preferences.getFloat((prefix + "temp_min").c_str(), 18.0);
    relayConfigs[i].temp_max = preferences.getFloat((prefix + "temp_max").c_str(), 26.0);
    relayConfigs[i].humidity_min = preferences.getFloat((prefix + "hum_min").c_str(), 50.0);
    relayConfigs[i].humidity_max = preferences.getFloat((prefix + "hum_max").c_str(), 70.0);
    relayConfigs[i].timer_on_duration = preferences.getULong((prefix + "on_dur").c_str(), 60000);
    relayConfigs[i].timer_off_duration = preferences.getULong((prefix + "off_dur").c_str(), 300000);
  }
  
  preferences.end();
  logMessage(LOG_INFO, "Configurações carregadas");
}

void saveRelayConfig(int index) {
  if (!isValidRelayIndex(index)) return;
  
  preferences.begin("actuator", false);
  String prefix = "relay" + String(index) + "_";
  
  preferences.putUInt((prefix + "mode").c_str(), relayConfigs[index].mode);
  preferences.putFloat((prefix + "ph_min").c_str(), relayConfigs[index].ph_min);
  preferences.putFloat((prefix + "ph_max").c_str(), relayConfigs[index].ph_max);
  preferences.putFloat((prefix + "ec_min").c_str(), relayConfigs[index].ec_min);
  preferences.putFloat((prefix + "ec_max").c_str(), relayConfigs[index].ec_max);
  preferences.putFloat((prefix + "temp_min").c_str(), relayConfigs[index].temp_min);
  preferences.putFloat((prefix + "temp_max").c_str(), relayConfigs[index].temp_max);
  preferences.putFloat((prefix + "hum_min").c_str(), relayConfigs[index].humidity_min);
  preferences.putFloat((prefix + "hum_max").c_str(), relayConfigs[index].humidity_max);
  preferences.putULong((prefix + "on_dur").c_str(), relayConfigs[index].timer_on_duration);
  preferences.putULong((prefix + "off_dur").c_str(), relayConfigs[index].timer_off_duration);
  
  preferences.end();
  
  logMessage(LOG_DEBUG, "Config relay " + String(index) + " salva");
}

void updateRelay(int index, bool state, String reason) {
  if (!isValidRelayIndex(index)) return;
  
  if (millis() - lastRelayChange[index] < RELAY_DEBOUNCE) {
    return;
  }
  
  if (relayConfigs[index].state != state) {
    relayConfigs[index].state = state;
    digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
    lastRelayChange[index] = millis();
    
    logMessage(LOG_INFO, "Relay " + String(index) + " " + 
               (state ? "ON" : "OFF") + " (" + reason + ")");
    
    if (mqttConnected) {
      publishRelayStatus();
    }
  }
}

void startPulse(int relayIndex, bool state, unsigned long duration) {
  if (!isValidRelayIndex(relayIndex)) return;
  
  activePulses[relayIndex].active = true;
  activePulses[relayIndex].relayIndex = relayIndex;
  activePulses[relayIndex].state = state;
  activePulses[relayIndex].endTime = millis() + duration;
  
  updateRelay(relayIndex, state, "pulse_start");
  
  logMessage(LOG_DEBUG, "Pulse iniciado: relay " + String(relayIndex) + 
             ", duration " + String(duration) + "ms");
}

void updatePulses() {
  for (int i = 0; i < 8; i++) {
    if (activePulses[i].active && millis() >= activePulses[i].endTime) {
      updateRelay(activePulses[i].relayIndex, !activePulses[i].state, "pulse_end");
      activePulses[i].active = false;
      logMessage(LOG_DEBUG, "Pulse finalizado: relay " + String(i));
    }
  }
}

void updateRelays() {
  for (int i = 0; i < 8; i++) {
    if (activePulses[i].active) continue;
    
    RelayMode mode = relayConfigs[i].mode;
    
    if (mode == MODE_MANUAL_OFF) {
      if (relayConfigs[i].state) {
        updateRelay(i, false, "manual_off");
      }
    } else if (mode == MODE_MANUAL_ON) {
      if (!relayConfigs[i].state) {
        updateRelay(i, true, "manual_on");
      }
    } else if (mode == MODE_TIMER) {
      unsigned long elapsed = millis() - relayConfigs[i].last_timer_change;
      
      if (relayConfigs[i].timer_state) {
        if (elapsed >= relayConfigs[i].timer_on_duration) {
          relayConfigs[i].timer_state = false;
          relayConfigs[i].last_timer_change = millis();
          updateRelay(i, false, "timer_off");
        }
      } else {
        if (elapsed >= relayConfigs[i].timer_off_duration) {
          relayConfigs[i].timer_state = true;
          relayConfigs[i].last_timer_change = millis();
          updateRelay(i, true, "timer_on");
        }
      }
    }
  }
}

void updateAutomaticRelays() {
  if (!isValidSensorData(currentSensorData)) return;
  
  for (int i = 0; i < 8; i++) {
    if (activePulses[i].active) continue;
    
    RelayMode mode = relayConfigs[i].mode;
    
    if (mode == MODE_AUTO_PH_DOWN) {
      if (millis() - lastPhAction < PH_COOLDOWN) continue;
      
      if (currentSensorData.ph > relayConfigs[i].ph_max) {
        if (!relayConfigs[i].state) {
          updateRelay(i, true, "ph_too_high");
          lastPhAction = millis();
        }
      } else if (currentSensorData.ph < relayConfigs[i].ph_min) {
        if (relayConfigs[i].state) {
          updateRelay(i, false, "ph_in_range");
        }
      }
    } else if (mode == MODE_AUTO_PH_UP) {
      if (millis() - lastPhAction < PH_COOLDOWN) continue;
      
      if (currentSensorData.ph < relayConfigs[i].ph_min) {
        if (!relayConfigs[i].state) {
          updateRelay(i, true, "ph_too_low");
          lastPhAction = millis();
        }
      } else if (currentSensorData.ph > relayConfigs[i].ph_max) {
        if (relayConfigs[i].state) {
          updateRelay(i, false, "ph_in_range");
        }
      }
    } else if (mode == MODE_AUTO_EC_UP) {
      if (millis() - lastEcAction < EC_COOLDOWN) continue;
      
      if (currentSensorData.ec < relayConfigs[i].ec_min) {
        if (!relayConfigs[i].state) {
          updateRelay(i, true, "ec_too_low");
          lastEcAction = millis();
        }
      } else if (currentSensorData.ec > relayConfigs[i].ec_max) {
        if (relayConfigs[i].state) {
          updateRelay(i, false, "ec_in_range");
        }
      }
    } else if (mode == MODE_AUTO_TEMP_COOL) {
      if (currentSensorData.temperature > relayConfigs[i].temp_max) {
        if (!relayConfigs[i].state) {
          updateRelay(i, true, "temp_too_high");
        }
      } else if (currentSensorData.temperature < relayConfigs[i].temp_min) {
        if (relayConfigs[i].state) {
          updateRelay(i, false, "temp_in_range");
        }
      }
    } else if (mode == MODE_AUTO_TEMP_HEAT) {
      if (currentSensorData.temperature < relayConfigs[i].temp_min) {
        if (!relayConfigs[i].state) {
          updateRelay(i, true, "temp_too_low");
        }
      } else if (currentSensorData.temperature > relayConfigs[i].temp_max) {
        if (relayConfigs[i].state) {
          updateRelay(i, false, "temp_in_range");
        }
      }
    } else if (mode == MODE_AUTO_HUMIDITY) {
      if (currentSensorData.humidity < relayConfigs[i].humidity_min) {
        if (!relayConfigs[i].state) {
          updateRelay(i, true, "humidity_too_low");
        }
      } else if (currentSensorData.humidity > relayConfigs[i].humidity_max) {
        if (relayConfigs[i].state) {
          updateRelay(i, false, "humidity_in_range");
        }
      }
    }
  }
}

// ==================== VALIDAÇÕES ====================
bool isValidRelayIndex(int index) {
  return index >= 0 && index < 8;
}

bool isValidRelayMode(int mode) {
  return mode >= MODE_MANUAL_OFF && mode <= MODE_TIMER;
}

bool isValidSensorData(const SensorData& data) {
  if (!data.valid) return false;
  if (millis() - data.timestamp > SENSOR_DATA_TIMEOUT) return false;
  return true;
}

// ==================== LOGGING ====================
void logMessage(LogLevel level, String message) {
  String levelStr;
  switch (level) {
    case LOG_DEBUG: levelStr = "[DEBUG]"; break;
    case LOG_INFO:  levelStr = "[INFO ]"; break;
    case LOG_WARN:  levelStr = "[WARN ]"; break;
    case LOG_ERROR: levelStr = "[ERROR]"; break;
  }
  
  Serial.print("[");
  Serial.print(String(millis()).c_str());
  Serial.print("] ");
  Serial.print(levelStr);
  Serial.print(" ");
  Serial.println(message);
}

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "N/A";
  }
  
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

void logHourlyPH(float ph) {
  if (millis() - last_ph_log < PH_LOG_INTERVAL) return;
  
  ph_history[ph_history_index] = ph;
  ph_history_index = (ph_history_index + 1) % 24;
  last_ph_log = millis();
  
  logMessage(LOG_DEBUG, "pH logged: " + String(ph, 2));
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
  
  return (count > 0) ? (sum / count) : 0.0;
}

// ==================== WIFI ====================
void setupWiFi() {
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
    logMessage(LOG_INFO, "Botão de setup pressionado - modo AP");
    startAPMode();
    return;
  }
  
  preferences.begin("wifi", true);
  String savedSSID = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();
  
  const char* ssid = (savedSSID.length() > 0) ? savedSSID.c_str() : WIFI_SSID;
  const char* password = (savedPassword.length() > 0) ? savedPassword.c_str() : WIFI_PASSWORD;
  
  if (strlen(ssid) == 0) {
    logMessage(LOG_WARN, "WiFi não configurado - iniciando modo AP");
    startAPMode();
    return;
  }
  
  logMessage(LOG_INFO, "Conectando ao WiFi: " + String(ssid));
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 8 == 0) {
      esp_task_wdt_reset();
    }
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    logMessage(LOG_INFO, "WiFi conectado! IP: " + WiFi.localIP().toString());
  } else {
    logMessage(LOG_ERROR, "WiFi falhou - iniciando modo AP");
    startAPMode();
  }
}

void checkWiFi() {
  if (apMode) return;
  
  bool connected = (WiFi.status() == WL_CONNECTED);
  
  if (connected != wifiConnected) {
    wifiConnected = connected;
    
    if (connected) {
      logMessage(LOG_INFO, "WiFi reconectado: " + WiFi.localIP().toString());
      
      if (!authCompleted) {
        authCompleted = authenticateDevice();
        if (authCompleted) {
          setupMQTT();
        }
      }
    } else {
      logMessage(LOG_WARN, "WiFi desconectado");
      mqttConnected = false;
    }
  }
}

void startAPMode() {
  apMode = true;
  apSSID = "AquaSys-" + deviceUUID.substring(deviceUUID.length() - 6);
  apPassword = "12345678";
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), apPassword.c_str());
  
  logMessage(LOG_INFO, "Modo AP iniciado");
  logMessage(LOG_INFO, "SSID: " + apSSID);
  logMessage(LOG_INFO, "Password: " + apPassword);
  logMessage(LOG_INFO, "IP: " + WiFi.softAPIP().toString());
  
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  
  logMessage(LOG_INFO, "Servidor web iniciado");
}

void handleRoot() {
  esp_task_wdt_reset();
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>AquaSys Setup</title>";
  html += "<style>";
  html += "body { font-family: Arial; padding: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 400px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #2196F3; text-align: center; }";
  html += ".info { background: #e3f2fd; padding: 10px; border-radius: 5px; margin-bottom: 20px; }";
  html += "label { display: block; margin: 15px 0 5px; font-weight: bold; }";
  html += "input { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }";
  html += "button { width: 100%; padding: 12px; background: #2196F3; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 20px; font-size: 16px; }";
  html += "button:hover { background: #1976D2; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🌊 AquaSys Setup</h1>";
  html += "<div class='info'>";
  html += "<strong>Device UUID:</strong><br>" + deviceUUID;
  html += "</div>";
  html += "<form action='/save' method='POST'>";
  html += "<label>WiFi SSID:</label>";
  html += "<input type='text' name='ssid' required>";
  html += "<label>WiFi Password:</label>";
  html += "<input type='password' name='password' required>";
  html += "<button type='submit'>💾 Salvar e Reiniciar</button>";
  html += "</form>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  esp_task_wdt_reset();
  
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    
    logMessage(LOG_INFO, "WiFi configurado: " + ssid);
    
    server.send(200, "text/html",
      "<html><body><h2>Salvo! Reiniciando...</h2></body></html>");
    
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html",
      "<html><body><h2>Erro: Dados incompletos</h2></body></html>");
  }
}

// ==================== AUTENTICAÇÃO ====================
// ✅ v4.2.4: Usa httpClient dedicado para HTTP/Supabase
bool authenticateDevice() {
  if (!wifiConnected) return false;

  logMessage(LOG_INFO, "Autenticando dispositivo...");

  // ✅ Configurar SSL específico para HTTP (Supabase)
  if (SSL_INSECURE_MODE) {
    httpClient.setInsecure();
    logMessage(LOG_WARN, "⚠️  SSL_INSECURE_MODE ATIVO - Certificado SSL ignorado!");
    logMessage(LOG_WARN, "⚠️  Modo inseguro - Use apenas para debug/diagnóstico");
    logMessage(LOG_WARN, "⚠️  Em produção, sempre use SSL_INSECURE_MODE = false");
  } else {
    httpClient.setCACert(SUPABASE_ROOT_CA);
    logMessage(LOG_INFO, "✅ SSL seguro ativado - Validando certificado Supabase");
    
    if (!ntpSynced) {
      logMessage(LOG_WARN, "⚠️  NTP não sincronizado - Validação SSL pode falhar!");
      logMessage(LOG_WARN, "⚠️  Certificados SSL exigem data/hora corretos");
    } else {
      logMessage(LOG_INFO, "✅ NTP sincronizado - Certificado será validado");
    }
  }

  HTTPClient https;
  https.begin(httpClient, AUTH_SERVER);  // ✅ Usando httpClient dedicado
  https.addHeader("Content-Type", "application/json");
  https.addHeader(AUTH_HEADER_KEY, AUTH_HEADER_VALUE);

  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["firmware_version"] = "4.2.4-DUAL-SSL";

  String requestBody;
  serializeJson(doc, requestBody);

  logMessage(LOG_DEBUG, "Request: " + requestBody);

  int httpCode = https.POST(requestBody);

  logMessage(LOG_DEBUG, "HTTP Code: " + String(httpCode));

  if (httpCode == 200) {
    String response = https.getString();
    logMessage(LOG_DEBUG, "Response: " + response);
    
    StaticJsonDocument<1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (error) {
      logMessage(LOG_ERROR, "JSON parse error: " + String(error.c_str()));
      https.end();
      return false;
    }

    if (responseDoc.containsKey("mqtt_config")) {
      JsonObject mqttConfig = responseDoc["mqtt_config"];
      mqttUsername = mqttConfig["username"].as<String>();
      mqttPassword = mqttConfig["password"].as<String>();
      
      logMessage(LOG_INFO, "✅ Autenticação bem-sucedida!");
      logMessage(LOG_DEBUG, "MQTT User: " + mqttUsername);
      https.end();
      return true;
    } else {
      logMessage(LOG_ERROR, "Resposta sem mqtt_config");
      https.end();
      return false;
    }
  } else if (httpCode == 404) {
    logMessage(LOG_ERROR, "Dispositivo não registrado no app!");
    logMessage(LOG_WARN, "Registre o UUID " + deviceUUID + " no app antes de continuar.");
  } else if (httpCode == -1) {
    logMessage(LOG_ERROR, "❌ Erro de conexão SSL!");
    if (!SSL_INSECURE_MODE) {
      logMessage(LOG_ERROR, "💡 SOLUÇÃO: Ative SSL_INSECURE_MODE = true para debug");
      logMessage(LOG_ERROR, "💡 Verifique se NTP está sincronizado (data/hora corretos)");
    } else {
      logMessage(LOG_ERROR, "Falha mesmo com SSL_INSECURE_MODE - verificar rede");
    }
  } else {
    logMessage(LOG_ERROR, "Autenticação falhou. HTTP: " + String(httpCode));
  }

  https.end();
  return false;
}

// ==================== NTP ====================
void setupNTP() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  logMessage(LOG_INFO, "NTP configurado");
}

void updateNTP() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    ntpSynced = true;
    logMessage(LOG_DEBUG, "NTP atualizado: " + getTimestamp());
  }
}

// ==================== MQTT ====================
// ✅ v4.2.4: Usa mqttWifiClient dedicado para MQTT/HiveMQ
void setupMQTT() {
  // ✅ Configurar SSL específico para MQTT (HiveMQ)
  if (SSL_INSECURE_MODE) {
    mqttWifiClient.setInsecure();
    logMessage(LOG_WARN, "⚠️  MQTT em modo SSL inseguro (apenas debug!)");
  } else {
    mqttWifiClient.setCACert(HIVEMQ_ROOT_CA);
    logMessage(LOG_INFO, "✅ MQTT com SSL seguro (validando certificado)");
  }
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);

  logMessage(LOG_INFO, "MQTT configurado");
}

void reconnectMQTT() {
  if (mqttClient.connected() || !wifiConnected || !authCompleted) {
    return;
  }
  
  unsigned long backoff = MQTT_RECONNECT_INTERVAL * (unsigned long)pow(2, mqttReconnectAttempts);
  if (backoff > 300000UL) {
    backoff = 300000UL;
  }
  
  if (millis() - lastMqttReconnect < backoff) {
    return;
  }
  
  lastMqttReconnect = millis();
  mqttReconnectAttempts++;
  
  logMessage(LOG_INFO, "Tentando MQTT... (tentativa " + String(mqttReconnectAttempts) + 
             ", backoff " + String(backoff/1000) + "s)");
  
  esp_task_wdt_reset();
  
  bool connected = mqttClient.connect(
    deviceUUID.c_str(),
    mqttUsername.c_str(),
    mqttPassword.c_str()
  );
  
  if (connected) {
    mqttConnected = true;
    mqttReconnectAttempts = 0;
    
    logMessage(LOG_INFO, "✅ MQTT conectado!");
    
    mqttClient.subscribe(TOPIC_RELAY_COMMAND);
    mqttClient.subscribe(TOPIC_RELAY_CONFIG);
    mqttClient.subscribe(TOPIC_CALIBRATION);
    mqttClient.subscribe(TOPIC_OTA);
    
    String sensors_topic = "aquasys/" + deviceUUID + "/sensors";
    mqttClient.subscribe(sensors_topic.c_str());
    
    publishHeartbeat();
    publishRelayStatus();
  } else {
    int rc = mqttClient.state();
    logMessage(LOG_WARN, "MQTT falhou (rc=" + String(rc) + "). Próxima tentativa em " + 
               String(backoff/1000) + "s");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  String payloadStr = String(message);
  
  logMessage(LOG_DEBUG, "MQTT <- " + topicStr + ": " + payloadStr);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payloadStr);
  
  if (error) {
    logMessage(LOG_WARN, "JSON inválido: " + String(error.c_str()));
    return;
  }
  
  if (topicStr == TOPIC_RELAY_COMMAND || topicStr.endsWith("/relay/command")) {
    int relay = doc["relay"];
    
    if (!isValidRelayIndex(relay)) {
      logMessage(LOG_WARN, "Índice de relay inválido: " + String(relay));
      return;
    }
    
    if (doc.containsKey("state")) {
      bool state = doc["state"];
      updateRelay(relay, state, "mqtt_command");
    } else if (doc.containsKey("pulse")) {
      bool state = doc["pulse"]["state"];
      unsigned long duration = doc["pulse"]["duration"];
      startPulse(relay, state, duration);
    }
  } else if (topicStr == TOPIC_RELAY_CONFIG || topicStr.endsWith("/relay/config")) {
    int relay = doc["relay"];
    
    if (!isValidRelayIndex(relay)) {
      logMessage(LOG_WARN, "Índice de relay inválido: " + String(relay));
      return;
    }
    
    if (doc.containsKey("mode")) {
      int mode = doc["mode"];
      if (isValidRelayMode(mode)) {
        relayConfigs[relay].mode = (RelayMode)mode;
      }
    }
    
    if (doc.containsKey("ph_min")) relayConfigs[relay].ph_min = doc["ph_min"];
    if (doc.containsKey("ph_max")) relayConfigs[relay].ph_max = doc["ph_max"];
    if (doc.containsKey("ec_min")) relayConfigs[relay].ec_min = doc["ec_min"];
    if (doc.containsKey("ec_max")) relayConfigs[relay].ec_max = doc["ec_max"];
    if (doc.containsKey("temp_min")) relayConfigs[relay].temp_min = doc["temp_min"];
    if (doc.containsKey("temp_max")) relayConfigs[relay].temp_max = doc["temp_max"];
    if (doc.containsKey("humidity_min")) relayConfigs[relay].humidity_min = doc["humidity_min"];
    if (doc.containsKey("humidity_max")) relayConfigs[relay].humidity_max = doc["humidity_max"];
    if (doc.containsKey("timer_on_duration")) relayConfigs[relay].timer_on_duration = doc["timer_on_duration"];
    if (doc.containsKey("timer_off_duration")) relayConfigs[relay].timer_off_duration = doc["timer_off_duration"];
    
    saveRelayConfig(relay);
    
    logMessage(LOG_INFO, "Config relay " + String(relay) + " atualizada");
    publishRelayStatus();
  } else if (topicStr.endsWith("/sensors")) {
    currentSensorData.ph = doc["ph"];
    currentSensorData.ec = doc["ec"];
    currentSensorData.temperature = doc["temperature"];
    currentSensorData.humidity = doc["humidity"];
    currentSensorData.water_temp = doc["water_temp"];
    currentSensorData.timestamp = millis();
    currentSensorData.valid = true;
    
    logMessage(LOG_DEBUG, "Dados dos sensores atualizados via MQTT");
    logHourlyPH(currentSensorData.ph);
  }
}

void publishRelayStatus() {
  if (!mqttConnected) {
    addToOutbox(TOPIC_RELAY_STATUS, "");
    return;
  }
  
  StaticJsonDocument<1024> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = getTimestamp();
  
  JsonArray relays = doc.createNestedArray("relays");
  
  for (int i = 0; i < 8; i++) {
    JsonObject relay = relays.createNestedObject();
    relay["index"] = i;
    relay["state"] = relayConfigs[i].state;
    relay["mode"] = relayConfigs[i].mode;
  }
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_RELAY_STATUS, payload.c_str())) {
    logMessage(LOG_DEBUG, "Status dos relés publicado");
  } else {
    logMessage(LOG_WARN, "Falha ao publicar status dos relés");
    addToOutbox(TOPIC_RELAY_STATUS, payload);
  }
}

void publishHeartbeat() {
  if (!mqttConnected) {
    addToOutbox(TOPIC_HEARTBEAT, "");
    return;
  }
  
  DiagnosticData diag;
  diag.uptime = millis() / 1000;
  diag.rssi = WiFi.RSSI();
  diag.freeHeap = ESP.getFreeHeap();
  diag.heapSize = ESP.getHeapSize();
  diag.mqttConnected = mqttConnected;
  diag.wifiConnected = wifiConnected;
  diag.ipAddress = WiFi.localIP().toString();
  diag.mqttReconnects = mqttReconnectAttempts;
  diag.avg_ph_24h = calculate24hAveragePH();
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = getTimestamp();
  doc["uptime"] = diag.uptime;
  doc["rssi"] = diag.rssi;
  doc["free_heap"] = diag.freeHeap;
  doc["heap_size"] = diag.heapSize;
  doc["mqtt_connected"] = diag.mqttConnected;
  doc["wifi_connected"] = diag.wifiConnected;
  doc["ip_address"] = diag.ipAddress;
  doc["mqtt_reconnects"] = diag.mqttReconnects;
  doc["avg_ph_24h"] = diag.avg_ph_24h;
  doc["firmware_version"] = "4.2.4-DUAL-SSL";
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_HEARTBEAT, payload.c_str())) {
    logMessage(LOG_DEBUG, "Heartbeat enviado");
  } else {
    logMessage(LOG_WARN, "Falha ao enviar heartbeat");
    addToOutbox(TOPIC_HEARTBEAT, payload);
  }
}

void addToOutbox(String topic, String payload) {
  if (outboxCount >= MAX_OUTBOX_SIZE) {
    logMessage(LOG_WARN, "Outbox cheio - descartando mensagem mais antiga");
    for (int i = 0; i < MAX_OUTBOX_SIZE - 1; i++) {
      outbox[i] = outbox[i + 1];
    }
    outboxCount = MAX_OUTBOX_SIZE - 1;
  }
  
  outbox[outboxCount].topic = topic;
  outbox[outboxCount].payload = payload;
  outboxCount++;
  
  logMessage(LOG_DEBUG, "Mensagem adicionada ao outbox (" + String(outboxCount) + "/" + 
             String(MAX_OUTBOX_SIZE) + ")");
}

void flushOutbox() {
  if (outboxCount == 0 || !mqttConnected) return;
  
  int sent = 0;
  for (int i = 0; i < outboxCount; i++) {
    if (mqttClient.publish(outbox[i].topic.c_str(), outbox[i].payload.c_str())) {
      sent++;
    } else {
      break;
    }
  }
  
  if (sent > 0) {
    for (int i = 0; i < outboxCount - sent; i++) {
      outbox[i] = outbox[i + sent];
    }
    outboxCount -= sent;
    logMessage(LOG_INFO, "Outbox flush: " + String(sent) + " mensagens enviadas");
  }
}

// ==================== BLE ====================
void setupBLE() {
  logMessage(LOG_INFO, "Inicializando BLE Client...");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  logMessage(LOG_INFO, "BLE Client inicializado");
}

void startBLEScan() {
  if (pBLEScan == nullptr) return;
  
  logMessage(LOG_INFO, "Iniciando scan BLE...");
  BLEScanResults* foundDevices = pBLEScan->start(5, false);
  
  if (foundDevices->getCount() > 0) {
    for (int i = 0; i < foundDevices->getCount(); i++) {
      BLEAdvertisedDevice device = foundDevices->getDevice(i);
      
      // IMPORTANTE: device.getName() retorna std::string, converter para String do Arduino
      if (device.haveName()) {
        String deviceName = String(device.getName().c_str());
        if (deviceName.indexOf("AquaSys-Sensor") >= 0) {
          logMessage(LOG_INFO, "Sensor encontrado: " + String(device.getAddress().toString().c_str()));
          connectToSensorBLE(String(device.getAddress().toString().c_str()));
          break;
        }
      }
    }
  } else {
    logMessage(LOG_DEBUG, "Nenhum sensor BLE encontrado");
  }
  
  pBLEScan->clearResults();
}

void connectToSensorBLE(String address) {
  if (pBLEClient != nullptr && pBLEClient->isConnected()) {
    pBLEClient->disconnect();
  }
  
  logMessage(LOG_INFO, "Conectando ao sensor BLE: " + address);
  
  pBLEClient = BLEDevice::createClient();
  BLEAddress bleAddress(address.c_str());
  
  if (pBLEClient->connect(bleAddress)) {
    bleClientConnected = true;
    connectedSensorAddress = address;
    
    logMessage(LOG_INFO, "Conectado ao sensor via BLE");
    
    BLERemoteService* pRemoteService = pBLEClient->getService(BLEUUID(SERVICE_UUID));
    if (pRemoteService != nullptr) {
      pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
      if (pRemoteCharacteristic != nullptr) {
        logMessage(LOG_INFO, "Característica BLE encontrada");
      }
    }
  } else {
    logMessage(LOG_WARN, "Falha ao conectar ao sensor BLE");
    bleClientConnected = false;
  }
}

void readSensorDataBLE() {
  if (pBLEClient == nullptr || !pBLEClient->isConnected() || pRemoteCharacteristic == nullptr) {
    bleClientConnected = false;
    return;
  }
  
  if (millis() - lastBleDataReceived < 5000) {
    return;
  }
  
  // IMPORTANTE: readValue() retorna Arduino String diretamente
  String data = pRemoteCharacteristic->readValue();
  if (data.length() > 0) {
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, data);
    
    if (!error) {
      currentSensorData.ph = doc["ph"];
      currentSensorData.ec = doc["ec"];
      currentSensorData.temperature = doc["temperature"];
      currentSensorData.humidity = doc["humidity"];
      currentSensorData.water_temp = doc["water_temp"];
      currentSensorData.timestamp = millis();
      currentSensorData.valid = true;
      
      lastBleDataReceived = millis();
      
      logMessage(LOG_INFO, "Dados recebidos via BLE - pH: " + String(currentSensorData.ph, 2));
      logHourlyPH(currentSensorData.ph);
    }
  }
}

void disconnectBLE() {
  if (pBLEClient != nullptr && pBLEClient->isConnected()) {
    pBLEClient->disconnect();
    bleClientConnected = false;
    logMessage(LOG_INFO, "Desconectado do sensor BLE");
  }
}
