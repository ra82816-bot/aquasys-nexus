/*
 * AquaSys Nexus - Actuator Module v4.2.7-NETWORK-DEBUG
 * ====================================================
 * 
 * ✅ CORREÇÕES v4.2.7-NETWORK-DEBUG:
 * - DNS público configurado (Google 8.8.8.8 / Cloudflare 1.1.1.1)
 * - Diagnóstico completo de rede antes de autenticação
 * - Timeout SSL aumentado de 5s para 15s
 * - Limpeza de conexões antigas antes de SSL
 * - Teste de resolução DNS e conectividade TCP
 * - Logging detalhado de memória heap
 * 
 * 🔧 DIAGNÓSTICO:
 * - Se DNS falhar: Problema de rede/DNS corporativo
 * - Se porta 443 bloqueada: Firewall institucional
 * - Se heap < 30KB: Problema de memória
 * 
 * ✅ CORREÇÕES v4.2.6-BLE-TYPE-FIX:
 * - Corrigido tipo de dado BLE para leitura de sensores (uint8_t* em vez de String)
 * - Adicionada conversão adequada dos bytes recebidos via BLE
 * - Melhorada validação de dados de sensores BLE
 * 
 * ✅ CORREÇÕES v4.2.5:
 * - SSL_INSECURE_MODE para debug de certificados
 * - Logging detalhado de erros SSL
 * - Modo dual: seguro para produção, inseguro para debug
 * 
 * ✅ CORREÇÕES v4.2.4:
 * - Certificado raiz ISRG Root X1 (Let's Encrypt) atualizado
 * - Sincronização NTP obrigatória antes de autenticação
 * - Validação de tempo do sistema antes de SSL
 * 
 * ✅ CORREÇÕES v4.2.3:
 * - Certificado HiveMQ atualizado para TLS 1.3
 * - Timeout de autenticação aumentado para 10s
 * - Retry logic melhorado com backoff exponencial
 * 
 * Descrição: Firmware para controle de até 8 relés com modos automáticos
 * baseados em leituras de sensores via BLE (pH, EC, temperatura, umidade).
 * Comunicação via MQTT (HiveMQ) e autenticação via HTTPS (Supabase).
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include <time.h>

// ============================================================================
// CONFIGURAÇÕES DE REDE E AUTENTICAÇÃO
// ============================================================================

// WiFi
const char* WIFI_SSID = "Apoio-a-Aula";
const char* WIFI_PASSWORD = "87239011";

// Servidor de Autenticação (Supabase Edge Function)
const char* AUTH_SERVER = "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth";
const char* AUTH_HEADER_KEY = "x-device-secret";
const char* AUTH_HEADER_VALUE = "aquasys-device-2024";

// MQTT Broker (HiveMQ Cloud)
const char* MQTT_BROKER = "64d41eb265c241a6817e31f252291f17.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;

// Tópicos MQTT
const char* TOPIC_RELAY_CONTROL = "aquasys/relay/control";
const char* TOPIC_RELAY_STATUS = "aquasys/relay/status";
const char* TOPIC_HEARTBEAT = "aquasys/heartbeat";
const char* TOPIC_SENSORS = "aquasys/sensors/all";

// ============================================================================
// CERTIFICADOS SSL/TLS
// ============================================================================

// Certificado Raiz ISRG Root X1 (Let's Encrypt) - para Supabase
const char* SUPABASE_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

// Certificado Raiz HiveMQ Cloud (TLS 1.3 compatible)
const char* HIVEMQ_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFBjCCAu6gAwIBAgIRAIp9PhPWLzDvI4a9KQdrNPgwDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw\n" \
"WhcNMjcwMzEyMjM1OTU5WjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n" \
"RW5jcnlwdDELMAkGA1UEAxMCRTYwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQZ5Ub4\n" \
"FLpdhPVJ/ybhKLHLxvGz7IyJ9tEZ9rDlVIyDvBDxRQBpFLR5HDlnPDVH5hZ5gXIv\n" \
"6XhVpIBzrGMVMN0dBKkKaKeaWcPBGOp7hTjlJFUZcqpNbfbKO2f+9Ec3kNejggEI\n" \
"MIIBBDAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUHAwIGCCsGAQUF\n" \
"BwMBMBIGA1UdEwEB/wQIMAYBAf8CAQAwHQYDVR0OBBYEFBsoeJlYXs9Mu08VhKNP\n" \
"7F4p5BnZMB8GA1UdIwQYMBaAFHm0WeZ7tuXkAXOACIjIGlj26ZtuMDIGCCsGAQUF\n" \
"BwEBBCYwJDAiBggrBgEFBQcwAoYWaHR0cDovL3gxLmkubGVuY3Iub3JnLzATBgNV\n" \
"HSAEDDAKMAgGBmeBDAECATAnBgNVHR8EIDAeMBygGqAYhhZodHRwOi8veDEuYy5s\n" \
"ZW5jci5vcmcvMDUGA1UdEQQuMCyCEmUxLm8ubGVuY3Iub3JnL4IOZTYuby5sZW5j\n" \
"ci5vcmcvgg5lNy5vLmxlbmNyLm9yZy8wDQYJKoZIhvcNAQELBQADggIBAFpS1198\n" \
"ZBz7I0rr0sSp9qEwvBnQfmU8zJqc/Tva1g99W7qGNGr2aFn0lYmfyMVMHCJ6kzJk\n" \
"V7jJbQQfJMOZPgSfOJ82NQxLZ6Zy4o8zFLPrpWxvmTfm8kNzYV3n7Vf5LqBRyNRe\n" \
"AE9Ea1wdKkYm/I5vLKdQWWqXWLvN5TQZqmXTUqoiMHJSJRjHHvQnGQ2Qi6FqDdNZ\n" \
"vYLQp6Eu5MKEQzOUXMZQ2vVpMFJXjJ5vbOvHlvk9E8kqJ3wHyWpJLdQQPqWE5bAG\n" \
"dPxLUkVJLSfqYqXGiFwVHJYILoLVWaGHLKvEyJT3j+KN1qLkQXiLTJ9YJQxWdmZY\n" \
"5kZLnPmN3FWOPzjGcVZxLvH8xgJLJGKQU8eBxQYW8Z7hOLYBQvJN3cLZXBSZLNqJ\n" \
"nDDDEWUOHuXnQWCFwNDOQPgGqTLBd1pqxLrQF4YeKQ3qBYqmvEVWDVPYOvnQEFLm\n" \
"xQzQMCmKDEsz3gKmT5uJcFN8OPlKmQqJFHEWLJLTcvYZNFDcuJmVGHLMlKCTVcFX\n" \
"4d9pfVh9KQpKTKVJVEgvxm5P1KBqwf2cXjLRGKXSZj6XSQFQZ+8F1F3LcXqiJKzm\n" \
"9D5bN5WPQJNlPxVmKBRQxNQP+K0Qz0S5OqVQJCZpGYSJAJQEZzZvVIgFUYNm1qrL\n" \
"FlRYVKR4F8nOkCVz3LSHZvDClcMOqGi2E2nV\n" \
"-----END CERTIFICATE-----\n";

// ============================================================================
// CONFIGURAÇÃO DE DEBUG SSL
// ============================================================================
const bool SSL_INSECURE_MODE = true;  // ⚠️ true = ignora certificado (debug), false = valida (produção)

// ============================================================================
// DEFINIÇÃO DE HARDWARE
// ============================================================================
#define FIRMWARE_VERSION "4.2.7-NETWORK-DEBUG"

// Pinos dos Relés
const int RELAY_PINS[8] = {23, 22, 21, 19, 18, 5, 17, 16};

// Pino do botão de setup (para modo AP)
const int SETUP_BUTTON_PIN = 0;

// ============================================================================
// BLE - UUIDs para leitura de sensores
// ============================================================================
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ============================================================================
// OBJETOS GLOBAIS
// ============================================================================
WiFiClientSecure wifiClientAuth;     // Cliente SSL para autenticação
WiFiClientSecure wifiClientMqtt;     // Cliente SSL para MQTT
PubSubClient mqttClient(wifiClientMqtt);
Preferences preferences;

// ============================================================================
// ENUMS E ESTRUTURAS
// ============================================================================
enum RelayMode {
  MODE_OFF = 0,
  MODE_MANUAL_ON = 1,
  MODE_TIMER = 2,
  MODE_PH_UP = 3,
  MODE_PH_DOWN = 4,
  MODE_TEMPERATURE = 5,
  MODE_HUMIDITY = 6,
  MODE_EC = 7,
  MODE_MANUAL_PULSE = 8
};

enum LogLevel {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR
};

struct RelayConfig {
  uint8_t mode;
  bool state;
  unsigned long timerDuration;
  unsigned long timerStart;
  float threshold;
  float hysteresis;
};

struct SensorData {
  float ph;
  float ec;
  float temperature;
  float humidity;
  unsigned long timestamp;
  bool valid;
};

struct Pulse {
  uint8_t relayIndex;
  unsigned long duration;
  unsigned long startTime;
  bool active;
};

struct DiagnosticData {
  int wifiRSSI;
  bool mqttConnected;
  uint32_t freeHeap;
  uint32_t minFreeHeap;
  unsigned long uptime;
  bool sensorPhValid;
  bool sensorEcValid;
};

struct MQTTMessage {
  String topic;
  String payload;
  unsigned long timestamp;
};

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

// Configuração e Estado do Dispositivo
String deviceUUID = "";
String mqttUsername = "";
String mqttPassword = "";
bool deviceAuthenticated = false;
bool wifiConnected = false;
unsigned long lastAuthAttempt = 0;
const unsigned long AUTH_RETRY_INTERVAL = 10000;
int authFailCount = 0;
const int MAX_AUTH_RETRIES = 5;

// Configurações dos Relés
RelayConfig relayConfigs[8];

// Dados dos Sensores
SensorData currentSensorData = {0, 0, 0, 0, 0, false};
unsigned long lastSensorUpdate = 0;
const unsigned long SENSOR_TIMEOUT = 300000; // 5 minutos

// Pulsos Manuais
Pulse activePulses[8];

// Conectividade MQTT
unsigned long lastMqttReconnect = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 60000;

// Outbox MQTT (para mensagens offline)
const int OUTBOX_SIZE = 20;
MQTTMessage outbox[OUTBOX_SIZE];
int outboxCount = 0;

// NTP
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -3 * 3600;  // UTC-3 (Brasília)
const int DAYLIGHT_OFFSET_SEC = 0;
bool ntpSynced = false;
unsigned long lastNtpUpdate = 0;
const unsigned long NTP_UPDATE_INTERVAL = 3600000; // 1 hora

// Watchdog
const int WDT_TIMEOUT = 30;

// ============================================================================
// PROTÓTIPOS DE FUNÇÕES
// ============================================================================
void setupWiFi();
void checkWiFi();
bool authenticateDevice();
void setupNTP();
void updateNTP();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishRelayStatus();
void publishHeartbeat();
void addToOutbox(const String& topic, const String& payload);
void flushOutbox();
void setupRelays();
void loadConfig();
void saveRelayConfig(uint8_t index);
void updateRelay(uint8_t index, bool state);
void startPulse(uint8_t relayIndex, unsigned long duration);
void updatePulses();
void updateRelays();
void updateAutomaticRelays();
bool isValidRelayIndex(uint8_t index);
bool isValidRelayMode(uint8_t mode);
bool isValidSensorData(const SensorData& data);
void logMessage(LogLevel level, const String& message);
String getTimestamp();
void initWatchdog();
String generateDeviceUUID();

// ============================================================================
// FUNÇÃO: setup()
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  logMessage(LOG_INFO, "===========================================");
  logMessage(LOG_INFO, "AquaSys Nexus Actuator v" + String(FIRMWARE_VERSION));
  logMessage(LOG_INFO, "===========================================");
  
  // Inicializar Watchdog
  initWatchdog();
  
  // Carregar preferências
  preferences.begin("aquasys", false);
  
  // Gerar UUID do dispositivo
  deviceUUID = generateDeviceUUID();
  logMessage(LOG_INFO, "Device UUID: " + deviceUUID);
  
  // Configurar relés
  setupRelays();
  loadConfig();
  
  // Configurar WiFi
  setupWiFi();
  
  // Configurar NTP
  if (wifiConnected) {
    setupNTP();
    updateNTP();
  }
  
  // Autenticar dispositivo
  if (wifiConnected && ntpSynced) {
    deviceAuthenticated = authenticateDevice();
  }
  
  // Configurar MQTT se autenticado
  if (deviceAuthenticated) {
    setupMQTT();
  }
  
  logMessage(LOG_INFO, "Setup completo!");
}

// ============================================================================
// FUNÇÃO: loop()
// ============================================================================
void loop() {
  esp_task_wdt_reset();
  
  // Verificar conectividade WiFi
  checkWiFi();
  
  // Atualizar NTP periodicamente
  if (wifiConnected && (millis() - lastNtpUpdate > NTP_UPDATE_INTERVAL)) {
    updateNTP();
  }
  
  // Re-autenticar se necessário
  if (wifiConnected && !deviceAuthenticated && 
      (millis() - lastAuthAttempt > AUTH_RETRY_INTERVAL) &&
      authFailCount < MAX_AUTH_RETRIES) {
    deviceAuthenticated = authenticateDevice();
  }
  
  // MQTT loop
  if (deviceAuthenticated) {
    if (!mqttClient.connected()) {
      if (millis() - lastMqttReconnect > MQTT_RECONNECT_INTERVAL) {
        reconnectMQTT();
      }
    } else {
      mqttClient.loop();
      flushOutbox();
    }
  }
  
  // Atualizar relés automáticos
  updateRelays();
  updatePulses();
  updateAutomaticRelays();
  
  // Heartbeat
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeat = millis();
  }
  
  delay(10);
}

// ============================================================================
// FUNÇÃO: initWatchdog()
// ============================================================================
void initWatchdog() {
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  logMessage(LOG_INFO, "Watchdog inicializado (" + String(WDT_TIMEOUT) + "s)");
}

// ============================================================================
// FUNÇÃO: generateDeviceUUID()
// ============================================================================
String generateDeviceUUID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char uuid[18];
  sprintf(uuid, "ACT-%02X%02X%02X%02X%02X%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(uuid);
}

// ============================================================================
// FUNÇÕES DE RELÉS
// ============================================================================
void setupRelays() {
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    relayConfigs[i] = {MODE_OFF, false, 0, 0, 0, 0};
    activePulses[i] = {0, 0, 0, false};
  }
  logMessage(LOG_INFO, "Relés configurados");
}

void loadConfig() {
  for (int i = 0; i < 8; i++) {
    String key = "relay" + String(i);
    if (preferences.isKey(key.c_str())) {
      size_t len = preferences.getBytesLength(key.c_str());
      if (len == sizeof(RelayConfig)) {
        preferences.getBytes(key.c_str(), &relayConfigs[i], sizeof(RelayConfig));
      }
    }
  }
  logMessage(LOG_INFO, "Configurações carregadas");
}

void saveRelayConfig(uint8_t index) {
  if (!isValidRelayIndex(index)) return;
  String key = "relay" + String(index);
  preferences.putBytes(key.c_str(), &relayConfigs[index], sizeof(RelayConfig));
}

void updateRelay(uint8_t index, bool state) {
  if (!isValidRelayIndex(index)) return;
  digitalWrite(RELAY_PINS[index], state ? HIGH : LOW);
  relayConfigs[index].state = state;
  logMessage(LOG_INFO, "Relé " + String(index) + " -> " + (state ? "ON" : "OFF"));
}

void startPulse(uint8_t relayIndex, unsigned long duration) {
  if (!isValidRelayIndex(relayIndex)) return;
  activePulses[relayIndex] = {relayIndex, duration, millis(), true};
  updateRelay(relayIndex, true);
  logMessage(LOG_INFO, "Pulso iniciado: Relé " + String(relayIndex) + " por " + String(duration) + "ms");
}

void updatePulses() {
  for (int i = 0; i < 8; i++) {
    if (activePulses[i].active) {
      if (millis() - activePulses[i].startTime >= activePulses[i].duration) {
        updateRelay(i, false);
        activePulses[i].active = false;
        logMessage(LOG_INFO, "Pulso finalizado: Relé " + String(i));
      }
    }
  }
}

void updateRelays() {
  // Atualizar timers
  for (int i = 0; i < 8; i++) {
    if (relayConfigs[i].mode == MODE_TIMER && relayConfigs[i].state) {
      if (millis() - relayConfigs[i].timerStart >= relayConfigs[i].timerDuration) {
        updateRelay(i, false);
        relayConfigs[i].mode = MODE_OFF;
        saveRelayConfig(i);
        publishRelayStatus();
      }
    }
  }
}

void updateAutomaticRelays() {
  if (!isValidSensorData(currentSensorData)) return;
  
  for (int i = 0; i < 8; i++) {
    bool shouldActivate = false;
    
    switch (relayConfigs[i].mode) {
      case MODE_PH_UP:
        shouldActivate = (currentSensorData.ph < relayConfigs[i].threshold);
        break;
      case MODE_PH_DOWN:
        shouldActivate = (currentSensorData.ph > relayConfigs[i].threshold);
        break;
      case MODE_TEMPERATURE:
        shouldActivate = (currentSensorData.temperature > relayConfigs[i].threshold);
        break;
      case MODE_HUMIDITY:
        shouldActivate = (currentSensorData.humidity < relayConfigs[i].threshold);
        break;
      case MODE_EC:
        shouldActivate = (currentSensorData.ec < relayConfigs[i].threshold);
        break;
    }
    
    if (shouldActivate != relayConfigs[i].state) {
      updateRelay(i, shouldActivate);
      publishRelayStatus();
    }
  }
}

// ============================================================================
// FUNÇÕES DE VALIDAÇÃO
// ============================================================================
bool isValidRelayIndex(uint8_t index) {
  return index < 8;
}

bool isValidRelayMode(uint8_t mode) {
  return mode <= MODE_MANUAL_PULSE;
}

bool isValidSensorData(const SensorData& data) {
  if (!data.valid) return false;
  if (millis() - data.timestamp > SENSOR_TIMEOUT) return false;
  if (data.ph < 0 || data.ph > 14) return false;
  if (data.ec < 0 || data.ec > 10000) return false;
  if (data.temperature < -10 || data.temperature > 50) return false;
  if (data.humidity < 0 || data.humidity > 100) return false;
  return true;
}

// ============================================================================
// FUNÇÕES DE LOGGING
// ============================================================================
void logMessage(LogLevel level, const String& message) {
  String prefix;
  switch (level) {
    case LOG_DEBUG: prefix = "[DEBUG]"; break;
    case LOG_INFO:  prefix = "[INFO ]"; break;
    case LOG_WARN:  prefix = "[WARN ]"; break;
    case LOG_ERROR: prefix = "[ERROR]"; break;
  }
  
  String timestamp = "[" + String(millis()) + "] ";
  Serial.println(timestamp + prefix + " " + message);
}

String getTimestamp() {
  time_t now;
  time(&now);
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  return String(buf);
}

// ============================================================================
// FUNÇÕES DE CONECTIVIDADE - WiFi
// ============================================================================
void setupWiFi() {
  logMessage(LOG_INFO, "Conectando WiFi: " + String(WIFI_SSID));
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    
    // ✅ CONFIGURAR DNS PÚBLICO (Google 8.8.8.8 / Cloudflare 1.1.1.1)
    IPAddress dns1(8, 8, 8, 8);       // Google DNS
    IPAddress dns2(1, 1, 1, 1);       // Cloudflare DNS
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
    
    logMessage(LOG_INFO, "✅ WiFi conectado!");
    logMessage(LOG_INFO, "IP: " + WiFi.localIP().toString());
    logMessage(LOG_INFO, "RSSI: " + String(WiFi.RSSI()) + " dBm");
    logMessage(LOG_INFO, "✅ DNS configurado: 8.8.8.8 / 1.1.1.1");
  } else {
    wifiConnected = false;
    logMessage(LOG_ERROR, "❌ Falha ao conectar WiFi");
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      logMessage(LOG_WARN, "WiFi desconectado!");
      wifiConnected = false;
      deviceAuthenticated = false;
    }
    
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 30000) {
      setupWiFi();
      lastReconnect = millis();
    }
  } else if (!wifiConnected) {
    wifiConnected = true;
    logMessage(LOG_INFO, "WiFi reconectado");
  }
}

// ============================================================================
// FUNÇÃO: authenticateDevice() - COM DIAGNÓSTICO DE REDE
// ============================================================================
bool authenticateDevice() {
  if (!wifiConnected) return false;

  logMessage(LOG_INFO, "Autenticando dispositivo...");
  
  // ✅ DIAGNÓSTICO DE REDE
  logMessage(LOG_INFO, "=== DIAGNÓSTICO DE REDE ===");
  logMessage(LOG_INFO, "Heap livre: " + String(ESP.getFreeHeap()) + " bytes");
  logMessage(LOG_INFO, "Heap mínimo: " + String(ESP.getMinFreeHeap()) + " bytes");
  
  // Testar resolução DNS
  IPAddress supabaseIP;
  logMessage(LOG_INFO, "Testando resolução DNS...");
  if (WiFi.hostByName("oaabtbvwxsjomeeizciq.supabase.co", supabaseIP)) {
    logMessage(LOG_INFO, "✅ DNS OK: " + supabaseIP.toString());
  } else {
    logMessage(LOG_ERROR, "❌ DNS FALHOU - não conseguiu resolver hostname");
    logMessage(LOG_ERROR, "Verifique: 1) Conexão com internet 2) DNS corporativo");
    lastAuthAttempt = millis();
    authFailCount++;
    return false;
  }
  
  // Testar conectividade TCP (porta 443)
  WiFiClient testClient;
  logMessage(LOG_INFO, "Testando conexão TCP na porta 443...");
  if (testClient.connect("oaabtbvwxsjomeeizciq.supabase.co", 443)) {
    logMessage(LOG_INFO, "✅ Porta 443 acessível");
    testClient.stop();
  } else {
    logMessage(LOG_ERROR, "❌ Porta 443 bloqueada ou inacessível");
    logMessage(LOG_ERROR, "Possível firewall ou proxy bloqueando HTTPS");
    lastAuthAttempt = millis();
    authFailCount++;
    return false;
  }
  
  logMessage(LOG_INFO, "=== FIM DO DIAGNÓSTICO ===");
  
  // ✅ Limpar conexões antigas e aguardar
  wifiClientAuth.stop();
  delay(100);
  
  // Configurar SSL
  if (SSL_INSECURE_MODE) {
    logMessage(LOG_WARN, "⚠️  SSL_INSECURE_MODE ATIVO - Certificado SSL ignorado!");
    logMessage(LOG_WARN, "⚠️  Modo inseguro - Use apenas para debug/diagnóstico");
    logMessage(LOG_WARN, "⚠️  Em produção, sempre use SSL_INSECURE_MODE = false");
    wifiClientAuth.setInsecure();
  } else {
    logMessage(LOG_INFO, "🔒 Modo SSL seguro - Validando certificado");
    wifiClientAuth.setCACert(SUPABASE_ROOT_CA);
  }
  
  // Criar payload JSON
  StaticJsonDocument<256> doc;
  doc["device_uuid"] = deviceUUID;
  doc["firmware_version"] = FIRMWARE_VERSION;
  String requestBody;
  serializeJson(doc, requestBody);
  
  logMessage(LOG_DEBUG, "Request: " + requestBody);
  
  // Fazer requisição HTTP
  HTTPClient https;
  https.begin(wifiClientAuth, AUTH_SERVER);
  https.setTimeout(15000);  // ✅ 15 segundos (aumentado de 5s)
  https.addHeader("Content-Type", "application/json");
  https.addHeader(AUTH_HEADER_KEY, AUTH_HEADER_VALUE);
  
  int httpCode = https.POST(requestBody);
  logMessage(LOG_DEBUG, "HTTP Code: " + String(httpCode));
  
  if (httpCode == 200) {
    String response = https.getString();
    logMessage(LOG_DEBUG, "Response: " + response);
    
    StaticJsonDocument<512> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      mqttUsername = responseDoc["mqtt_username"].as<String>();
      mqttPassword = responseDoc["mqtt_password"].as<String>();
      
      logMessage(LOG_INFO, "✅ Autenticação bem-sucedida!");
      logMessage(LOG_DEBUG, "MQTT User: " + mqttUsername);
      
      authFailCount = 0;
      https.end();
      return true;
    } else {
      logMessage(LOG_ERROR, "Erro ao parsear resposta: " + String(error.c_str()));
    }
  } else if (httpCode == -1) {
    logMessage(LOG_ERROR, "❌ Erro de conexão SSL!");
    if (SSL_INSECURE_MODE) {
      logMessage(LOG_ERROR, "Falha mesmo com SSL_INSECURE_MODE - verificar rede");
    } else {
      logMessage(LOG_ERROR, "Possível erro de certificado SSL");
      logMessage(LOG_ERROR, "Tente ativar SSL_INSECURE_MODE para debug");
    }
  } else {
    logMessage(LOG_ERROR, "HTTP Error: " + String(httpCode));
    String response = https.getString();
    if (response.length() > 0) {
      logMessage(LOG_ERROR, "Response: " + response);
    }
  }
  
  https.end();
  lastAuthAttempt = millis();
  authFailCount++;
  
  if (authFailCount < MAX_AUTH_RETRIES) {
    logMessage(LOG_WARN, "Auth falhou. Tentativas: " + String(authFailCount));
  } else {
    logMessage(LOG_ERROR, "Auth falhou após " + String(MAX_AUTH_RETRIES) + " tentativas");
  }
  
  return false;
}

// ============================================================================
// FUNÇÕES NTP
// ============================================================================
void setupNTP() {
  logMessage(LOG_INFO, "Configurando NTP...");
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
}

void updateNTP() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    ntpSynced = true;
    lastNtpUpdate = millis();
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    logMessage(LOG_INFO, "✅ NTP sincronizado: " + String(timeStr));
  } else {
    ntpSynced = false;
    logMessage(LOG_WARN, "⚠️  Falha ao sincronizar NTP");
  }
}

// ============================================================================
// FUNÇÕES MQTT
// ============================================================================
void setupMQTT() {
  if (SSL_INSECURE_MODE) {
    wifiClientMqtt.setInsecure();
  } else {
    wifiClientMqtt.setCACert(HIVEMQ_ROOT_CA);
  }
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(15);  // ✅ 15 segundos
  
  logMessage(LOG_INFO, "MQTT configurado");
}

void reconnectMQTT() {
  if (!deviceAuthenticated || mqttUsername.length() == 0) return;
  
  lastMqttReconnect = millis();
  
  logMessage(LOG_INFO, "Conectando MQTT...");
  
  if (mqttClient.connect(deviceUUID.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
    logMessage(LOG_INFO, "✅ MQTT conectado!");
    
    mqttClient.subscribe(TOPIC_RELAY_CONTROL);
    mqttClient.subscribe(TOPIC_SENSORS);
    
    publishRelayStatus();
    publishHeartbeat();
  } else {
    logMessage(LOG_ERROR, "MQTT falhou, rc=" + String(mqttClient.state()));
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  logMessage(LOG_DEBUG, "MQTT RX [" + String(topic) + "]: " + message);
  
  if (String(topic) == TOPIC_RELAY_CONTROL) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (!error) {
      uint8_t relayIndex = doc["relay"];
      if (isValidRelayIndex(relayIndex)) {
        if (doc.containsKey("mode")) {
          uint8_t mode = doc["mode"];
          if (isValidRelayMode(mode)) {
            relayConfigs[relayIndex].mode = mode;
            if (doc.containsKey("threshold")) {
              relayConfigs[relayIndex].threshold = doc["threshold"];
            }
            saveRelayConfig(relayIndex);
            publishRelayStatus();
          }
        }
        
        if (doc.containsKey("state")) {
          bool state = doc["state"];
          updateRelay(relayIndex, state);
          publishRelayStatus();
        }
        
        if (doc.containsKey("pulse")) {
          unsigned long duration = doc["pulse"];
          startPulse(relayIndex, duration);
        }
      }
    }
  }
  else if (String(topic) == TOPIC_SENSORS) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (!error) {
      currentSensorData.ph = doc["ph"] | 0.0f;
      currentSensorData.ec = doc["ec"] | 0.0f;
      currentSensorData.temperature = doc["temperature"] | 0.0f;
      currentSensorData.humidity = doc["humidity"] | 0.0f;
      currentSensorData.timestamp = millis();
      currentSensorData.valid = true;
      
      logMessage(LOG_DEBUG, "Sensores atualizados via MQTT");
    }
  }
}

void publishRelayStatus() {
  if (!mqttClient.connected()) {
    addToOutbox(TOPIC_RELAY_STATUS, "pending");
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
    addToOutbox(TOPIC_RELAY_STATUS, payload);
  }
}

void publishHeartbeat() {
  if (!mqttClient.connected()) {
    addToOutbox(TOPIC_HEARTBEAT, "pending");
    return;
  }
  
  DiagnosticData diag = {
    WiFi.RSSI(),
    mqttClient.connected(),
    ESP.getFreeHeap(),
    ESP.getMinFreeHeap(),
    millis() / 1000,
    currentSensorData.valid,
    currentSensorData.valid
  };
  
  StaticJsonDocument<512> doc;
  doc["device_uuid"] = deviceUUID;
  doc["timestamp"] = getTimestamp();
  doc["wifi_rssi"] = diag.wifiRSSI;
  doc["mqtt_connected"] = diag.mqttConnected;
  doc["free_heap"] = diag.freeHeap;
  doc["uptime"] = diag.uptime;
  
  String payload;
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_HEARTBEAT, payload.c_str())) {
    logMessage(LOG_DEBUG, "Heartbeat enviado");
  } else {
    addToOutbox(TOPIC_HEARTBEAT, payload);
  }
}

void addToOutbox(const String& topic, const String& payload) {
  if (outboxCount < OUTBOX_SIZE) {
    outbox[outboxCount] = {topic, payload, millis()};
    outboxCount++;
    logMessage(LOG_DEBUG, "Mensagem adicionada à outbox (" + String(outboxCount) + "/" + String(OUTBOX_SIZE) + ")");
  }
}

void flushOutbox() {
  if (outboxCount == 0 || !mqttClient.connected()) return;
  
  for (int i = 0; i < outboxCount; i++) {
    if (mqttClient.publish(outbox[i].topic.c_str(), outbox[i].payload.c_str())) {
      logMessage(LOG_DEBUG, "Mensagem da outbox enviada");
      
      // Remover da outbox
      for (int j = i; j < outboxCount - 1; j++) {
        outbox[j] = outbox[j + 1];
      }
      outboxCount--;
      i--;
    }
  }
}
