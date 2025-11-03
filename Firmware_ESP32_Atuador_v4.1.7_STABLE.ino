/*
 * AquaSys Nexus - Actuator Module v4.1.7-STABLE
 * ===============================================
 * ✅ CORREÇÃO v4.1.7: Corrigido erro SSL -9984 (certificado)
 * ✅ CORREÇÃO v4.1.7: Adicionado fallback para modo insecure
 * ✅ CORREÇÃO v4.1.7: Logs detalhados de diagnóstico SSL
 * ✅ CORREÇÃO v4.1.7: Melhor tratamento do fluxo de registro
 * ✅ CORREÇÃO v4.1.7: Timeout de autenticação aumentado
 * 
 * IMPORTANTE: FLUXO DE PAREAMENTO
 * ================================
 * 1. ESP32 inicia e entra em AP Mode (se não tiver WiFi configurado)
 * 2. Usuário conecta ao AP "AquaSys-XXXX" e configura WiFi
 * 3. ESP32 reinicia e conecta ao WiFi
 * 4. ESP32 mostra seu UUID no Serial Monitor
 * 5. Usuário registra o dispositivo no APP usando este UUID
 * 6. ESP32 autentica e recebe credenciais MQTT
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
// ✅ FLAG DE DEBUG SSL (true = desabilita verificação, false = verifica certificado)
const bool SSL_INSECURE_MODE = true; // Mude para false em produção

// WiFi (serão substituídas por AP Mode se vazias)
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

// Certificado Root CA HiveMQ (usado se SSL_INSECURE_MODE = false)
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

// UUIDs BLE (Client - para conectar ao Sensor)
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ==================== HARDWARE ====================
const int RELAY_PINS[8] = {23, 5, 4, 13, 22, 21, 14, 12};
const int SETUP_BUTTON_PIN = 0;

// ==================== OBJETOS GLOBAIS ====================
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
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

struct MQTTMessage {
  String topic;
  String payload;
};

// ==================== VARIÁVEIS GLOBAIS ====================
// Device UUID
String deviceUUID = "";

// Credenciais MQTT (recebidas na autenticação)
String mqttUsername = "";
String mqttPassword = "";

// Estado de Autenticação
bool authCompleted = false;
unsigned long lastAuthAttempt = 0;
const unsigned long AUTH_RETRY_INTERVAL = 60000; // ✅ Aumentado para 60s
int authFailureCount = 0;
const int MAX_AUTH_FAILURES = 10; // ✅ Aumentado para 10 tentativas

// Estado WiFi/MQTT
bool wifiConnected = false;
bool mqttConnected = false;
unsigned long lastWifiCheck = 0;
unsigned long lastMqttCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
const unsigned long MQTT_CHECK_INTERVAL = 5000;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

// Modo AP
bool apMode = false;
String apSSID = "";
String apPassword = "";

// Relés
RelayConfig relayConfigs[8];
Pulse activePulses[8];

// Debounce e Cooldowns
const unsigned long RELAY_DEBOUNCE = 500;
unsigned long lastRelayChange[8] = {0};
const unsigned long PH_COOLDOWN = 300000;
const unsigned long EC_COOLDOWN = 600000;
unsigned long lastPhAction = 0;
unsigned long lastEcAction = 0;

// Dados de Sensores
SensorData currentSensorData = {0, 0, 0, 0, 0, false, 0};
const unsigned long SENSOR_DATA_TIMEOUT = 180000;

// Histórico pH 24h
float ph_history[24] = {0};
int ph_history_index = 0;
unsigned long last_ph_log = 0;
const unsigned long PH_LOG_INTERVAL = 3600000;

// Timers de Publicação
unsigned long lastStatusPublish = 0;
unsigned long lastHeartbeat = 0;
const unsigned long STATUS_PUBLISH_INTERVAL = 5000;
const unsigned long HEARTBEAT_INTERVAL = 60000;

// NTP
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;
unsigned long lastNtpUpdate = 0;
const unsigned long NTP_UPDATE_INTERVAL = 3600000;

// MQTT Reconnect
int mqttReconnectAttempts = 0;
unsigned long lastMqttReconnect = 0;

// Outbox (mensagens offline)
const int MAX_OUTBOX_SIZE = 50;
MQTTMessage outbox[MAX_OUTBOX_SIZE];
int outboxCount = 0;

// BLE Client
bool bleInitialized = false;
bool bleClientConnected = false;
String connectedSensorAddress = "";
unsigned long lastBleDataReceived = 0;
unsigned long lastBleScan = 0;
const unsigned long BLE_SCAN_INTERVAL = 30000;
bool bleActivated = false;

// Watchdog
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

  logMessage(LOG_INFO, "=== AquaSys Actuator v4.1.7-STABLE ===");
  
  // ✅ CRÍTICO: Mostrar modo SSL
  if (SSL_INSECURE_MODE) {
    logMessage(LOG_WARN, "SSL INSECURE MODE ATIVO - Desabilite em produção!");
  }

  generateDeviceUUID();
  logMessage(LOG_INFO, "Device UUID: " + deviceUUID);
  logMessage(LOG_INFO, ">>> REGISTRE ESTE UUID NO APP ANTES DE CONTINUAR <<<");

  setupRelays();
  loadConfig();
  
  esp_task_wdt_reset();
  setupWiFi();

  if (wifiConnected && !apMode) {
    esp_task_wdt_reset();
    authCompleted = authenticateDevice();
    if (!authCompleted) {
      authFailureCount++;
      logMessage(LOG_WARN, "Auth falhou. Tentativas: " + String(authFailureCount));
      logMessage(LOG_WARN, ">>> CERTIFIQUE-SE DE TER REGISTRADO O UUID NO APP <<<");
    } else {
      authFailureCount = 0;
    }
  }

  if (wifiConnected && !apMode) {
    esp_task_wdt_reset();
    setupNTP();
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
    logMessage(LOG_INFO, "MQTT reconectado. Desativando fallback BLE.");
    bleActivated = false;
    disconnectBLE();
    if (pBLEScan != nullptr) {
       pBLEScan->stop();
    }
  }

  // ✅ CORREÇÃO: Reduzir limite para 10 falhas
  if (!authCompleted && authFailureCount >= MAX_AUTH_FAILURES) {
    logMessage(LOG_ERROR, String(MAX_AUTH_FAILURES) + " falhas de autenticação - reiniciando...");
    logMessage(LOG_ERROR, ">>> REGISTRE O UUID " + deviceUUID + " NO APP <<<");
    delay(5000);
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

// ==================== RELÉS ====================
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
    relayConfigs[i].temp_max = 28.0;
    relayConfigs[i].humidity_min = 50.0;
    relayConfigs[i].humidity_max = 70.0;
    relayConfigs[i].timer_on_duration = 60000;
    relayConfigs[i].timer_off_duration = 60000;
    relayConfigs[i].last_timer_change = 0;
    relayConfigs[i].timer_state = false;

    activePulses[i].active = false;
    lastRelayChange[i] = 0;
  }
  logMessage(LOG_INFO, "Relés inicializados");
}

void updateRelay(int index, bool state, String reason) {
  if (!isValidRelayIndex(index)) {
    logMessage(LOG_ERROR, "Índice de relé inválido: " + String(index));
    return;
  }

  if (millis() - lastRelayChange[index] < RELAY_DEBOUNCE) {
    return;
  }

  if (relayConfigs[index].state != state) {
    relayConfigs[index].state = state;
    digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
    lastRelayChange[index] = millis();

    logMessage(LOG_INFO, "Relay " + String(index) + " -> " +
               (state ? "ON" : "OFF") + " (" + reason + ")");

    saveRelayConfig(index);
  }
}

void startPulse(int relayIndex, bool state, unsigned long duration) {
  if (!isValidRelayIndex(relayIndex)) return;

  activePulses[relayIndex].relayIndex = relayIndex;
  activePulses[relayIndex].state = state;
  activePulses[relayIndex].endTime = millis() + duration;
  activePulses[relayIndex].active = true;

  updateRelay(relayIndex, state, "pulse_start");

  logMessage(LOG_INFO, "Pulse iniciado: Relay " + String(relayIndex) +
             " -> " + (state ? "ON" : "OFF") + " por " + String(duration) + "ms");
}

void updatePulses() {
  for (int i = 0; i < 8; i++) {
    if (activePulses[i].active && millis() >= activePulses[i].endTime) {
      activePulses[i].active = false;
      updateRelay(i, !activePulses[i].state, "pulse_end");
    }
  }
}

void updateRelays() {
  for (int i = 0; i < 8; i++) {
    if (relayConfigs[i].mode == MODE_MANUAL_ON) {
      if (!relayConfigs[i].state) {
        updateRelay(i, true, "manual_on");
      }
    } else if (relayConfigs[i].mode == MODE_MANUAL_OFF) {
      if (relayConfigs[i].state) {
        updateRelay(i, false, "manual_off");
      }
    }
  }
}

void updateAutomaticRelays() {
  if (!isValidSensorData(currentSensorData)) return;

  for (int i = 0; i < 8; i++) {
    RelayConfig& cfg = relayConfigs[i];

    switch (cfg.mode) {
      case MODE_AUTO_PH_DOWN:
        if (millis() - lastPhAction >= PH_COOLDOWN) {
          if (currentSensorData.ph > cfg.ph_max) {
            updateRelay(i, true, "ph_auto_down");
            lastPhAction = millis();
          } else if (currentSensorData.ph < cfg.ph_min) {
            updateRelay(i, false, "ph_auto_down_stop");
          }
        }
        break;

      case MODE_AUTO_PH_UP:
        if (millis() - lastPhAction >= PH_COOLDOWN) {
          if (currentSensorData.ph < cfg.ph_min) {
            updateRelay(i, true, "ph_auto_up");
            lastPhAction = millis();
          } else if (currentSensorData.ph > cfg.ph_max) {
            updateRelay(i, false, "ph_auto_up_stop");
          }
        }
        break;

      case MODE_AUTO_EC_UP:
        if (millis() - lastEcAction >= EC_COOLDOWN) {
          if (currentSensorData.ec < cfg.ec_min) {
            updateRelay(i, true, "ec_auto_up");
            lastEcAction = millis();
          } else if (currentSensorData.ec > cfg.ec_max) {
            updateRelay(i, false, "ec_auto_up_stop");
          }
        }
        break;

      case MODE_AUTO_TEMP_COOL:
        if (currentSensorData.temperature > cfg.temp_max + 1.0) {
          updateRelay(i, true, "temp_cool");
        } else if (currentSensorData.temperature < cfg.temp_max - 1.0) {
          updateRelay(i, false, "temp_cool_stop");
        }
        break;

      case MODE_AUTO_TEMP_HEAT:
        if (currentSensorData.temperature < cfg.temp_min - 1.0) {
          updateRelay(i, true, "temp_heat");
        } else if (currentSensorData.temperature > cfg.temp_min + 1.0) {
          updateRelay(i, false, "temp_heat_stop");
        }
        break;

      case MODE_AUTO_HUMIDITY:
        if (currentSensorData.humidity < cfg.humidity_min) {
          updateRelay(i, true, "humidity_auto");
        } else if (currentSensorData.humidity > cfg.humidity_max) {
          updateRelay(i, false, "humidity_auto_stop");
        }
        break;

      case MODE_TIMER: {
        if (cfg.last_timer_change == 0) {
          cfg.last_timer_change = millis();
          cfg.timer_state = false;
        }

        unsigned long elapsed = millis() - cfg.last_timer_change;
        unsigned long target = cfg.timer_state ? cfg.timer_on_duration : cfg.timer_off_duration;

        if (elapsed >= target) {
          cfg.timer_state = !cfg.timer_state;
          cfg.last_timer_change = millis();
          updateRelay(i, cfg.timer_state, "timer_auto");
          saveRelayConfig(i);
        }
        break;
      }
      
      case MODE_MANUAL_ON:
      case MODE_MANUAL_OFF:
        break;
    }
  }
}

// ==================== PERSISTÊNCIA ====================
void loadConfig() {
  preferences.begin("hydrosmart", false);
  
  String nvs_ssid = preferences.getString("wifi_ssid", "");
  String nvs_pass = preferences.getString("wifi_password", "");
  
  if (nvs_ssid.length() > 0) {
    WIFI_SSID = nvs_ssid.c_str();
    WIFI_PASSWORD = nvs_pass.c_str();
  }
  
  for (int i = 0; i < 8; i++) {
    String prefix = "r" + String(i) + "_";
    
    if (preferences.isKey((prefix + "mode").c_str())) {
      relayConfigs[i].mode = (RelayMode)preferences.getUInt((prefix + "mode").c_str(), MODE_MANUAL_OFF);
    }
    if (preferences.isKey((prefix + "state").c_str())) {
      relayConfigs[i].state = preferences.getBool((prefix + "state").c_str(), false);
    }
    if (preferences.isKey((prefix + "ph_min").c_str())) {
      relayConfigs[i].ph_min = preferences.getFloat((prefix + "ph_min").c_str(), 5.5);
    }
    if (preferences.isKey((prefix + "ph_max").c_str())) {
      relayConfigs[i].ph_max = preferences.getFloat((prefix + "ph_max").c_str(), 6.5);
    }
    if (preferences.isKey((prefix + "ec_min").c_str())) {
      relayConfigs[i].ec_min = preferences.getFloat((prefix + "ec_min").c_str(), 1.0);
    }
    if (preferences.isKey((prefix + "ec_max").c_str())) {
      relayConfigs[i].ec_max = preferences.getFloat((prefix + "ec_max").c_str(), 2.5);
    }
    if (preferences.isKey((prefix + "temp_min").c_str())) {
      relayConfigs[i].temp_min = preferences.getFloat((prefix + "temp_min").c_str(), 18.0);
    }
    if (preferences.isKey((prefix + "temp_max").c_str())) {
      relayConfigs[i].temp_max = preferences.getFloat((prefix + "temp_max").c_str(), 28.0);
    }
    if (preferences.isKey((prefix + "hum_min").c_str())) {
      relayConfigs[i].humidity_min = preferences.getFloat((prefix + "hum_min").c_str(), 50.0);
    }
    if (preferences.isKey((prefix + "hum_max").c_str())) {
      relayConfigs[i].humidity_max = preferences.getFloat((prefix + "hum_max").c_str(), 70.0);
    }
    if (preferences.isKey((prefix + "ton").c_str())) {
      relayConfigs[i].timer_on_duration = preferences.getULong((prefix + "ton").c_str(), 60000);
    }
    if (preferences.isKey((prefix + "toff").c_str())) {
      relayConfigs[i].timer_off_duration = preferences.getULong((prefix + "toff").c_str(), 60000);
    }
  }
  
  preferences.end();
  logMessage(LOG_INFO, "Configurações carregadas");
}

void saveRelayConfig(int index) {
  if (!isValidRelayIndex(index)) return;

  preferences.begin("hydrosmart", false);
  String prefix = "r" + String(index) + "_";

  preferences.putUInt((prefix + "mode").c_str(), relayConfigs[index].mode);
  preferences.putBool((prefix + "state").c_str(), relayConfigs[index].state);
  preferences.putFloat((prefix + "ph_min").c_str(), relayConfigs[index].ph_min);
  preferences.putFloat((prefix + "ph_max").c_str(), relayConfigs[index].ph_max);
  preferences.putFloat((prefix + "ec_min").c_str(), relayConfigs[index].ec_min);
  preferences.putFloat((prefix + "ec_max").c_str(), relayConfigs[index].ec_max);
  preferences.putFloat((prefix + "temp_min").c_str(), relayConfigs[index].temp_min);
  preferences.putFloat((prefix + "temp_max").c_str(), relayConfigs[index].temp_max);
  preferences.putFloat((prefix + "hum_min").c_str(), relayConfigs[index].humidity_min);
  preferences.putFloat((prefix + "hum_max").c_str(), relayConfigs[index].humidity_max);
  preferences.putULong((prefix + "ton").c_str(), relayConfigs[index].timer_on_duration);
  preferences.putULong((prefix + "toff").c_str(), relayConfigs[index].timer_off_duration);

  preferences.end();
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
  if (data.ph < 0 || data.ph > 14) return false;
  if (data.ec < 0 || data.ec > 10) return false;
  if (data.temperature < -10 || data.temperature > 60) return false;
  if (data.humidity < 0 || data.humidity > 100) return false;
  return true;
}

// ==================== LOGGING ====================
void logMessage(LogLevel level, String message) {
  String levelStr;
  switch (level) {
    case LOG_DEBUG: levelStr = "DEBUG"; break;
    case LOG_INFO:  levelStr = "INFO "; break;
    case LOG_WARN:  levelStr = "WARN "; break;
    case LOG_ERROR: levelStr = "ERROR"; break;
  }

  Serial.print("[");
  Serial.print(millis());
  Serial.print("] [");
  Serial.print(levelStr);
  Serial.print("] ");
  Serial.println(message);

  if (mqttConnected && level >= LOG_WARN) {
    StaticJsonDocument<256> doc;
    doc["device_uuid"] = deviceUUID;
    doc["level"] = levelStr;
    doc["message"] = message;
    doc["timestamp"] = getTimestamp();

    String payload;
    serializeJson(doc, payload);

    mqttClient.publish("aquasys/logs", payload.c_str(), false);
  }
}

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String(millis());
  }

  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ==================== HISTÓRICO pH ====================
void logHourlyPH(float ph) {
  if (millis() - last_ph_log < PH_LOG_INTERVAL) return;

  last_ph_log = millis();
  ph_history[ph_history_index] = ph;
  ph_history_index = (ph_history_index + 1) % 24;

  logMessage(LOG_DEBUG, "pH histórico registrado: " + String(ph, 2));
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

// ==================== WIFI ====================
void setupWiFi() {
  if (strlen(WIFI_SSID) == 0 || strlen(WIFI_PASSWORD) == 0) {
    preferences.begin("hydrosmart", false);
    String ssid = preferences.getString("wifi_ssid", "");
    String password = preferences.getString("wifi_password", "");
    preferences.end();
    
    if (ssid.length() == 0 || password.length() == 0) {
      logMessage(LOG_WARN, "Credenciais WiFi vazias - entrando em AP Mode");
      startAPMode();
      return;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    logMessage(LOG_INFO, "Conectando ao WiFi: " + ssid);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    logMessage(LOG_INFO, "Conectando ao WiFi: " + String(WIFI_SSID));
  }
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 5 == 0) {
      esp_task_wdt_reset();
    }
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    logMessage(LOG_INFO, "WiFi conectado! IP: " + WiFi.localIP().toString());
  } else {
    logMessage(LOG_ERROR, "Falha ao conectar WiFi - entrando em AP Mode");
    startAPMode();
  }
}

void checkWiFi() {
  if (apMode) return;

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      wifiConnected = false;
      mqttConnected = false;
      logMessage(LOG_WARN, "WiFi desconectado!");
    }
    
    WiFi.reconnect();
  } else {
    if (!wifiConnected) {
      wifiConnected = true;
      logMessage(LOG_INFO, "WiFi reconectado!");
    }
  }
}

// ==================== AP MODE ====================
void startAPMode() {
  apMode = true;
  apSSID = "AquaSys-" + deviceUUID.substring(4, 16);
  apPassword = "aquasys123";

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), apPassword.c_str());

  IPAddress IP = WiFi.softAPIP();
  logMessage(LOG_INFO, "AP Mode ativo");
  logMessage(LOG_INFO, "SSID: " + apSSID);
  logMessage(LOG_INFO, "Password: " + apPassword);
  logMessage(LOG_INFO, "IP: " + IP.toString());

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void handleRoot() {
  String html = R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AquaSys Setup</title>
  <style>
    body { font-family: Arial; max-width: 400px; margin: 50px auto; padding: 20px; }
    input { width: 100%; padding: 10px; margin: 10px 0; box-sizing: border-box; }
    button { width: 100%; padding: 10px; background: #0066cc; color: white; border: none; cursor: pointer; }
    .uuid { background: #f0f0f0; padding: 10px; margin: 20px 0; border-radius: 5px; font-family: monospace; }
  </style>
</head>
<body>
  <h2>AquaSys Actuator Setup</h2>
  <div class="uuid">UUID: )";
  
  html += deviceUUID;
  html += R"(</div>
  <p><strong>IMPORTANTE:</strong> Registre este UUID no app antes de salvar!</p>
  <form action="/save" method="POST">
    <input type="text" name="ssid" placeholder="WiFi SSID" required>
    <input type="password" name="password" placeholder="WiFi Password" required>
    <button type="submit">Salvar e Reiniciar</button>
  </form>
</body>
</html>)";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    preferences.begin("hydrosmart", false);
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_password", password);
    preferences.end();
    
    server.send(200, "text/html", 
      "<html><body><h2>Salvo! Reiniciando...</h2><p>Aguarde 10s e registre o dispositivo no app.</p></body></html>");
    
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", 
      "<html><body><h2>Erro: Dados incompletos</h2></body></html>");
  }
}

// ==================== AUTENTICAÇÃO ====================
bool authenticateDevice() {
  if (!wifiConnected) return false;

  logMessage(LOG_INFO, "Autenticando dispositivo...");
  
  // ✅ CORREÇÃO v4.1.7: Configurar SSL
  if (SSL_INSECURE_MODE) {
    wifiClient.setInsecure(); // Desabilita verificação de certificado
    logMessage(LOG_WARN, "SSL: Modo inseguro ativo");
  } else {
    wifiClient.setCACert(HIVEMQ_ROOT_CA);
    logMessage(LOG_INFO, "SSL: Verificação de certificado ativa");
  }

  HTTPClient https;
  https.begin(wifiClient, AUTH_SERVER);
  https.addHeader("Content-Type", "application/json");
  https.addHeader(AUTH_HEADER_KEY, AUTH_HEADER_VALUE);
  https.setTimeout(15000); // ✅ Timeout de 15s

  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["firmware_version"] = "4.1.7-STABLE";

  String requestBody;
  serializeJson(doc, requestBody);
  
  logMessage(LOG_DEBUG, "Enviando: " + requestBody);

  int httpCode = https.POST(requestBody);
  
  // ✅ CORREÇÃO v4.1.7: Logs detalhados
  logMessage(LOG_INFO, "HTTP Status: " + String(httpCode));

  if (httpCode == 200) {
    String response = https.getString();
    logMessage(LOG_DEBUG, "Response: " + response);
    
    StaticJsonDocument<1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      if (responseDoc["success"] == true) {
        // ✅ CORREÇÃO v4.1.7: Estrutura correta da resposta
        JsonObject mqtt_config = responseDoc["mqtt_config"];
        mqttUsername = mqtt_config["username"].as<String>();
        mqttPassword = mqtt_config["password"].as<String>();
        
        logMessage(LOG_INFO, "Autenticação bem-sucedida!");
        logMessage(LOG_DEBUG, "MQTT User: " + mqttUsername);
        https.end();
        return true;
      } else {
        String error_msg = responseDoc["error"] | "Unknown error";
        String message = responseDoc["message"] | "";
        logMessage(LOG_ERROR, "Auth recusada: " + error_msg);
        if (message.length() > 0) {
          logMessage(LOG_ERROR, "Mensagem: " + message);
        }
      }
    } else {
      logMessage(LOG_ERROR, "JSON parse error: " + String(error.c_str()));
    }
  } else if (httpCode == 404) {
    logMessage(LOG_ERROR, "Dispositivo não registrado no sistema");
    logMessage(LOG_ERROR, ">>> REGISTRE O UUID " + deviceUUID + " NO APP <<<");
  } else if (httpCode == -1) {
    logMessage(LOG_ERROR, "Erro de conexão SSL. Verifique certificado ou ative SSL_INSECURE_MODE");
  } else {
    logMessage(LOG_ERROR, "HTTP Error: " + String(httpCode));
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
    logMessage(LOG_DEBUG, "NTP atualizado: " + getTimestamp());
  }
}

// ==================== MQTT ====================
void setupMQTT() {
  // ✅ CORREÇÃO v4.1.7: Configurar SSL para MQTT
  if (SSL_INSECURE_MODE) {
    wifiClient.setInsecure();
  } else {
    wifiClient.setCACert(HIVEMQ_ROOT_CA);
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

  logMessage(LOG_INFO, "Tentando MQTT... (tentativa " + 
               String(mqttReconnectAttempts + 1) + ", backoff " + 
               String(backoff/1000) + "s)");

  lastMqttReconnect = millis();
  esp_task_wdt_reset();
  
  String clientId = "aquasys-actuator-" + deviceUUID;

  if (mqttClient.connect(clientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
    mqttConnected = true;
    mqttReconnectAttempts = 0;
    logMessage(LOG_INFO, "MQTT conectado!");
    
    mqttClient.subscribe(TOPIC_SENSORS);
    mqttClient.subscribe(TOPIC_RELAY_COMMAND);
    mqttClient.subscribe(TOPIC_RELAY_CONFIG);
    mqttClient.subscribe(TOPIC_CALIBRATION);
    mqttClient.subscribe(TOPIC_OTA);
    
    publishHeartbeat();
    flushOutbox();
  } else {
    mqttReconnectAttempts++;
    
    unsigned long nextBackoff = MQTT_RECONNECT_INTERVAL * (unsigned long)pow(2, mqttReconnectAttempts);
    if (nextBackoff > 300000UL) nextBackoff = 300000UL;
    
    logMessage(LOG_WARN, "MQTT falhou (rc=" + String(mqttClient.state()) + 
               "). Próxima tentativa em " + String(nextBackoff / 1000) + "s");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);

  char payloadStr[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';

  logMessage(LOG_DEBUG, "MQTT recebido: " + topicStr);

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payloadStr);

  if (error) {
    logMessage(LOG_ERROR, "JSON parse error: " + String(error.c_str()));
    return;
  }

  if (topicStr == TOPIC_SENSORS) {
    currentSensorData.ph = doc["ph"] | 0.0f;
    currentSensorData.ec = doc["ec"] | 0.0f;
    currentSensorData.temperature = doc["temperature"] | 0.0f;
    currentSensorData.humidity = doc["humidity"] | 0.0f;
    currentSensorData.water_temp = doc["water_temp"] | 0.0f;
    currentSensorData.timestamp = millis();
    currentSensorData.valid = isValidSensorData(currentSensorData);

    if (currentSensorData.valid) {
      logHourlyPH(currentSensorData.ph);
    }
  }

  else if (topicStr == TOPIC_RELAY_COMMAND) {
    int index = doc["relay_index"] | -1;
    String command = doc["command"] | "";
    
    if (isValidRelayIndex(index)) {
      if (command == "on") {
        relayConfigs[index].mode = MODE_MANUAL_ON;
        updateRelay(index, true, "mqtt_command");
      } else if (command == "off") {
        relayConfigs[index].mode = MODE_MANUAL_OFF;
        updateRelay(index, false, "mqtt_command");
      } else if (command == "pulse") {
        unsigned long duration = doc["duration"] | 5000UL;
        startPulse(index, true, duration);
      }
      
      saveRelayConfig(index);
    }
  }

  else if (topicStr == TOPIC_RELAY_CONFIG) {
    int index = doc["relay_index"] | -1;
    
    if (isValidRelayIndex(index)) {
      int mode = doc["mode"] | -1;
      if (isValidRelayMode(mode)) {
        relayConfigs[index].mode = (RelayMode)mode;
      }
      
      if (doc.containsKey("ph_min")) relayConfigs[index].ph_min = doc["ph_min"];
      if (doc.containsKey("ph_max")) relayConfigs[index].ph_max = doc["ph_max"];
      if (doc.containsKey("ec_min")) relayConfigs[index].ec_min = doc["ec_min"];
      if (doc.containsKey("ec_max")) relayConfigs[index].ec_max = doc["ec_max"];
      if (doc.containsKey("temp_min")) relayConfigs[index].temp_min = doc["temp_min"];
      if (doc.containsKey("temp_max")) relayConfigs[index].temp_max = doc["temp_max"];
      if (doc.containsKey("humidity_min")) relayConfigs[index].humidity_min = doc["humidity_min"];
      if (doc.containsKey("humidity_max")) relayConfigs[index].humidity_max = doc["humidity_max"];
      if (doc.containsKey("timer_on")) relayConfigs[index].timer_on_duration = doc["timer_on"];
      if (doc.containsKey("timer_off")) relayConfigs[index].timer_off_duration = doc["timer_off"];
      
      saveRelayConfig(index);
      logMessage(LOG_INFO, "Relay " + String(index) + " configurado via MQTT");
    }
  }
}

void publishRelayStatus() {
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

  if (mqttConnected) {
    mqttClient.publish(TOPIC_RELAY_STATUS, payload.c_str(), false);
  } else {
    addToOutbox(TOPIC_RELAY_STATUS, payload);
  }
}

void publishHeartbeat() {
  StaticJsonDocument<1024> doc;
  doc["device_uuid"] = deviceUUID;
  doc["device_type"] = "actuator";
  doc["firmware_version"] = "4.1.7-STABLE";
  doc["timestamp"] = getTimestamp();

  JsonObject status = doc.createNestedObject("status");
  status["wifi_connected"] = wifiConnected;
  status["mqtt_connected"] = mqttConnected;
  status["ble_client_connected"] = bleClientConnected;
  status["rssi"] = WiFi.RSSI();
  status["ip_address"] = WiFi.localIP().toString();

  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = (unsigned long)ESP.getFreeHeap();
  memory["heap_size"] = (unsigned long)ESP.getHeapSize();
  memory["min_free_heap"] = (unsigned long)ESP.getMinFreeHeap();

  doc["uptime_ms"] = (unsigned long)millis();

  JsonArray relays = doc.createNestedArray("relays");
  for (int i = 0; i < 8; i++) {
    JsonObject relay = relays.createNestedObject();
    relay["index"] = i;
    relay["state"] = relayConfigs[i].state;
    relay["mode"] = relayConfigs[i].mode;
  }

  float avg24h = calculate24hAveragePH();
  JsonObject ph_stats = doc.createNestedObject("ph_stats");
  ph_stats["average_24h"] = (avg24h > 0) ? avg24h : 0;
  ph_stats["last_reading"] = currentSensorData.valid ? currentSensorData.ph : 0;

  String payload;
  serializeJson(doc, payload);

  if (mqttConnected) {
    mqttClient.publish(TOPIC_HEARTBEAT, payload.c_str(), false);
    logMessage(LOG_DEBUG, "Heartbeat publicado");
  } else {
    addToOutbox(TOPIC_HEARTBEAT, payload);
  }
}

void addToOutbox(String topic, String payload) {
  if (outboxCount >= MAX_OUTBOX_SIZE) {
    logMessage(LOG_WARN, "Outbox cheio - descartando mensagem mais antiga");
    for (int i = 0; i < MAX_OUTBOX_SIZE - 1; i++) {
      outbox[i] = outbox[i + 1];
    }
    outboxCount--;
  }

  outbox[outboxCount].topic = topic;
  outbox[outboxCount].payload = payload;
  outboxCount++;

  logMessage(LOG_DEBUG, "Mensagem adicionada ao outbox (" + String(outboxCount) + "/" + String(MAX_OUTBOX_SIZE) + ")");
}

void flushOutbox() {
  if (!mqttConnected || outboxCount == 0) return;

  int sent = 0;
  for (int i = 0; i < outboxCount && i < 5; i++) {
    if (mqttClient.publish(outbox[i].topic.c_str(), outbox[i].payload.c_str(), false)) {
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
    
    logMessage(LOG_INFO, "Outbox: " + String(sent) + " mensagens enviadas, " + 
               String(outboxCount) + " restantes");
  }
}

// ==================== BLE CLIENT ====================
void setupBLE() {
  BLEDevice::init("AquaSys-Actuator");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  logMessage(LOG_INFO, "BLE Client preparado");
}

void startBLEScan() {
  logMessage(LOG_INFO, "Iniciando BLE scan...");

  BLEScanResults* foundDevices = pBLEScan->start(5, false);

  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    
    if (device.haveName() && String(device.getName().c_str()).startsWith("AquaSys-Sensor")) {
      logMessage(LOG_INFO, "Sensor BLE encontrado: " + String(device.getAddress().toString().c_str()));
      connectToSensorBLE(device.getAddress().toString().c_str());
      break;
    }
  }

  pBLEScan->clearResults();
}

void connectToSensorBLE(String address) {
  if (bleClientConnected) return;

  logMessage(LOG_INFO, "Conectando ao sensor BLE: " + address);

  pBLEClient = BLEDevice::createClient();

  if (pBLEClient->connect(BLEAddress(address.c_str()))) {
    logMessage(LOG_INFO, "BLE conectado!");
    
    BLERemoteService* pRemoteService = pBLEClient->getService(SERVICE_UUID);
    if (pRemoteService != nullptr) {
      pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
      
      if (pRemoteCharacteristic != nullptr) {
        bleClientConnected = true;
        connectedSensorAddress = address;
        logMessage(LOG_INFO, "BLE characteristic encontrado - pronto para ler dados");
      }
    }
  } else {
    logMessage(LOG_ERROR, "Falha ao conectar BLE");
  }
}

void readSensorDataBLE() {
  if (!bleClientConnected || pRemoteCharacteristic == nullptr) return;

  if (millis() - lastBleDataReceived < 5000) return;

  if (pBLEClient->isConnected()) {
    String value = pRemoteCharacteristic->readValue();
    
    logMessage(LOG_DEBUG, "BLE data recebido: " + value);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, value);
    
    if (!error) {
      currentSensorData.ph = doc["ph"] | 0.0f;
      currentSensorData.ec = doc["ec"] | 0.0f;
      currentSensorData.temperature = doc["temperature"] | 0.0f;
      currentSensorData.humidity = doc["humidity"] | 0.0f;
      currentSensorData.water_temp = doc["water_temp"] | 0.0f;
      currentSensorData.timestamp = millis();
      currentSensorData.valid = isValidSensorData(currentSensorData);
      
      lastBleDataReceived = millis();
      
      if (currentSensorData.valid) {
        logMessage(LOG_INFO, "Dados de sensor recebidos via BLE (pH:" + String(currentSensorData.ph, 2) + ")");
      }
    }
  } else {
    disconnectBLE();
  }
}

void disconnectBLE() {
  if (bleClientConnected) {
    pBLEClient->disconnect();
    bleClientConnected = false;
    connectedSensorAddress = "";
    logMessage(LOG_INFO, "BLE desconectado");
  }
}
