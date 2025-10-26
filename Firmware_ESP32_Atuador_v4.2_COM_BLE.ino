/*
 * Firmware ESP32 Atuador v4.2 - FASE 2 (COM BLE)
 * 
 * Novidades v4.2:
 * - UUID único por dispositivo
 * - Cliente BLE para receber dados de sensores
 * - TLS 1.3 para MQTT
 * - Diagnósticos aprimorados
 * - Ativação automática de BLE quando MQTT falhar
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <time.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define FIRMWARE_VERSION "4.2-BLE"
#define DEVICE_TYPE "actuator"

// Pinos dos relés
const int RELAY_PINS[8] = {23, 22, 21, 19, 18, 17, 16, 15};
#define SETUP_BUTTON_PIN 0

// Intervalos
#define HEARTBEAT_INTERVAL 30000
#define RELAY_LOGIC_INTERVAL 10000
#define STATUS_PUBLISH_INTERVAL 15000
#define BLE_SCAN_INTERVAL 5000
#define BLE_ACTIVATION_TIMEOUT 180000

// MQTT - Credenciais HiveMQ Cloud
const char* mqtt_broker = "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud";
const int mqtt_port = 8884;
const char* mqtt_user = "esp32-user";
const char* mqtt_password = "HydroSmart123";

const char* root_ca = R"EOF(
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

// Tópicos MQTT
const char* TOPIC_RELAY_STATUS = "aquasys/relay/status";
const char* TOPIC_RELAY_COMMAND = "aquasys/relay/command";
const char* TOPIC_HEARTBEAT = "aquasys/heartbeat";
const char* TOPIC_SENSORS = "aquasys/sensors/all";

// BLE UUIDs (devem corresponder ao sensor)
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Estrutura BLE
struct BLEData {
  float ph;
  float waterTemp;
  float ec;
  uint32_t timestamp;
  uint8_t checksum;
} __attribute__((packed));

struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  uint32_t lastUpdate;
  bool fromBLE;
};

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;
WebServer server(80);

// BLE
BLEScan* pBLEScan = nullptr;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteChar = nullptr;
bool bleActive = false;
bool bleConnected = false;
String sensorBLEAddress = "";

String deviceUUID = "";
String mqttClientId = "";
String wifiSSID = "";
String wifiPassword = "";
bool isAPMode = false;

bool relayStates[8] = {false};
SensorData sensorData;

unsigned long lastHeartbeat = 0;
unsigned long lastRelayLogic = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastBLEScan = 0;
unsigned long lastSuccessfulMqtt = 0;

uint32_t wifiReconnects = 0;
uint32_t mqttFailedAttempts = 0;

// ==================== PROTÓTIPOS ====================

void generateDeviceUUID();
void setupBLE();
void scanBLE();
void connectToBLEServer();
void readBLEData();
void publishRelayStatus();
void publishHeartbeat();
void handleRelayLogic();
void checkBLEActivation();

// ==================== LOG ====================

void logMessage(const char* level, const char* msg) {
  unsigned long s = millis() / 1000;
  Serial.printf("[%02lu:%02lu:%02lu] %s %s\n", 
    s/3600, (s%3600)/60, s%60, level, msg);
}

// ==================== UUID ====================

void generateDeviceUUID() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  
  char uuid[20];
  sprintf(uuid, "HYDRO-%02X%02X-%02X%02X-%02X%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  deviceUUID = String(uuid);
  mqttClientId = "actuator_" + deviceUUID;
  
  logMessage("INFO", ("UUID: " + deviceUUID).c_str());
}

// ==================== WIFI ====================

void connectWiFi() {
  preferences.begin("hydrosmart", false);
  wifiSSID = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_pass", "");
  preferences.end();
  
  if (wifiSSID.length() == 0) {
    logMessage("WARN", "Sem WiFi, modo AP");
    startAPMode();
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    logMessage("INFO", ("WiFi OK: " + WiFi.localIP().toString()).c_str());
  } else {
    logMessage("ERROR", "WiFi falhou, modo AP");
    wifiReconnects++;
    startAPMode();
  }
}

void checkWiFi() {
  if (isAPMode) return;
  
  if (WiFi.status() != WL_CONNECTED) {
    wifiReconnects++;
    WiFi.reconnect();
  }
}

void startAPMode() {
  isAPMode = true;
  String apSSID = "HydroActuator_" + deviceUUID.substring(6);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), "12345678");
  logMessage("INFO", ("AP: " + apSSID).c_str());
  
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<title>HydroSmart Actuator</title></head><body>";
    html += "<h1>WiFi - " + deviceUUID + "</h1>";
    html += "<form action='/save' method='POST'>";
    html += "SSID: <input name='ssid' required><br>";
    html += "Senha: <input name='pass' type='password' required><br>";
    html += "<input type='submit' value='Salvar'></form></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/save", HTTP_POST, []() {
    preferences.begin("hydrosmart", false);
    preferences.putString("wifi_ssid", server.arg("ssid"));
    preferences.putString("wifi_pass", server.arg("pass"));
    preferences.end();
    
    server.send(200, "text/html", 
      "<html><body><h1>Salvo! Reiniciando...</h1></body></html>");
    delay(2000);
    ESP.restart();
  });
  
  server.begin();
}

// ==================== MQTT ====================

void setupMQTT() {
  espClient.setCACert(root_ca);
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  logMessage("INFO", "MQTT TLS configurado");
}

void reconnectMQTT() {
  if (isAPMode || WiFi.status() != WL_CONNECTED) return;
  
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 5000) return;
  lastAttempt = millis();
  
  if (!mqttClient.connected()) {
    logMessage("INFO", "Conectando MQTT TLS...");
    
    if (mqttClient.connect(mqttClientId.c_str(), mqtt_user, mqtt_password)) {
      lastSuccessfulMqtt = millis();
      mqttClient.subscribe(TOPIC_RELAY_COMMAND);
      mqttClient.subscribe(TOPIC_SENSORS);
      logMessage("INFO", "MQTT TLS OK");
      mqttFailedAttempts = 0;
    } else {
      mqttFailedAttempts++;
      logMessage("ERROR", ("MQTT falhou: " + String(mqttClient.state())).c_str());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  lastSuccessfulMqtt = millis();
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    logMessage("ERROR", "JSON parse falhou");
    return;
  }
  
  // Processar dados de sensores via MQTT
  if (String(topic) == TOPIC_SENSORS) {
    sensorData.ph = doc["ph"] | 0.0f;
    sensorData.ec = doc["ec"] | 0.0f;
    sensorData.airTemp = doc["airTemp"] | 0.0f;
    sensorData.humidity = doc["humidity"] | 0.0f;
    sensorData.waterTemp = doc["waterTemp"] | 0.0f;
    sensorData.lastUpdate = millis();
    sensorData.fromBLE = false;
    
    logMessage("DEBUG", "Sensores via MQTT");
  }
  
  // Processar comandos de relés
  if (String(topic) == TOPIC_RELAY_COMMAND) {
    int relayIdx = doc["relay"] | -1;
    bool state = doc["state"] | false;
    
    if (relayIdx >= 0 && relayIdx < 8) {
      digitalWrite(RELAY_PINS[relayIdx], state ? HIGH : LOW);
      relayStates[relayIdx] = state;
      logMessage("INFO", ("Relé " + String(relayIdx) + ": " + (state ? "ON" : "OFF")).c_str());
      publishRelayStatus();
    }
  }
}

void publishRelayStatus() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  for (int i = 0; i < 8; i++) {
    doc["relay" + String(i+1)] = relayStates[i];
  }
  doc["timestamp"] = millis() / 1000;
  
  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_RELAY_STATUS, buffer);
}

void publishHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  doc["device"] = "ESP32_Actuator_" + deviceUUID;
  doc["device_uuid"] = deviceUUID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["uptime"] = millis() / 1000;
  
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["ssid"] = WiFi.SSID();
  wifi["rssi"] = WiFi.RSSI();
  wifi["ip"] = WiFi.localIP().toString();
  wifi["reconnects"] = wifiReconnects;
  
  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["connected"] = mqttClient.connected();
  mqtt["failed_attempts"] = mqttFailedAttempts;
  
  JsonObject ble = doc.createNestedObject("ble");
  ble["active"] = bleActive;
  ble["connected"] = bleConnected;
  ble["sensor_from_ble"] = sensorData.fromBLE;
  
  JsonObject memory = doc.createNestedObject("memory");
  memory["free_heap"] = ESP.getFreeHeap();
  memory["min_free_heap"] = ESP.getMinFreeHeap();
  
  char buffer[512];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_HEARTBEAT, buffer);
}

// ==================== BLE ====================

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      
      sensorBLEAddress = advertisedDevice.getAddress().toString().c_str();
      logMessage("INFO", ("Sensor BLE encontrado: " + sensorBLEAddress).c_str());
      advertisedDevice.getScan()->stop();
    }
  }
};

void setupBLE() {
  logMessage("INFO", "Inicializando BLE Client...");
  
  BLEDevice::init(("HydroActuator_" + deviceUUID).c_str());
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
  bleActive = true;
  logMessage("INFO", "BLE Client ativo");
}

void scanBLE() {
  if (!bleActive || bleConnected) return;
  
  logMessage("DEBUG", "Escaneando BLE...");
  BLEScanResults* foundDevices = pBLEScan->start(3, false);
  pBLEScan->clearResults();
  
  if (sensorBLEAddress.length() > 0 && !bleConnected) {
    connectToBLEServer();
  }
}

void connectToBLEServer() {
  if (bleConnected || sensorBLEAddress.length() == 0) return;
  
  logMessage("INFO", "Conectando ao sensor BLE...");
  
  pClient = BLEDevice::createClient();
  BLEAddress addr(sensorBLEAddress.c_str());
  
  if (pClient->connect(addr)) {
    logMessage("INFO", "BLE conectado!");
    
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService != nullptr) {
      pRemoteChar = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
      if (pRemoteChar != nullptr) {
        bleConnected = true;
        logMessage("INFO", "Característica BLE OK");
      }
    }
  } else {
    logMessage("ERROR", "Falha ao conectar BLE");
  }
}

void readBLEData() {
  if (!bleConnected || pRemoteChar == nullptr) return;
  
  try {
    std::string value = pRemoteChar->readValue();
    
    if (value.length() == sizeof(BLEData)) {
      BLEData bleData;
      memcpy(&bleData, value.data(), sizeof(BLEData));
      
      // Validar checksum
      uint8_t* bytes = (uint8_t*)&bleData;
      uint8_t checksum = 0;
      for (size_t i = 0; i < sizeof(BLEData) - 1; i++) {
        checksum ^= bytes[i];
      }
      
      if (checksum == bleData.checksum) {
        sensorData.ph = bleData.ph;
        sensorData.ec = bleData.ec;
        sensorData.waterTemp = bleData.waterTemp;
        sensorData.lastUpdate = millis();
        sensorData.fromBLE = true;
        
        logMessage("DEBUG", "Dados BLE recebidos");
      }
    }
  } catch (...) {
    logMessage("ERROR", "Erro lendo BLE");
    bleConnected = false;
  }
}

void checkBLEActivation() {
  if (bleActive) return;
  
  unsigned long now = millis();
  
  // Ativar BLE se MQTT offline > 3 min
  if (!mqttClient.connected() && (now - lastSuccessfulMqtt > BLE_ACTIVATION_TIMEOUT)) {
    logMessage("WARN", "MQTT offline > 3min, ativando BLE");
    setupBLE();
  }
}

// ==================== LÓGICA DOS RELÉS ====================

void handleRelayLogic() {
  // Exemplo simples: controle de pH
  if (millis() - sensorData.lastUpdate < 120000) {  // Dados < 2 min
    if (sensorData.ph > 0 && sensorData.ph < 6.0) {
      digitalWrite(RELAY_PINS[2], HIGH);  // Relé pH UP
      relayStates[2] = true;
    } else if (sensorData.ph > 7.0) {
      digitalWrite(RELAY_PINS[2], LOW);
      relayStates[2] = false;
    }
  }
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  logMessage("INFO", "=== HydroSmart Actuator v4.2-BLE ===");
  
  generateDeviceUUID();
  
  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);
  
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  
  connectWiFi();
  setupMQTT();
  
  logMessage("INFO", "Setup completo!");
}

// ==================== LOOP ====================

void loop() {
  esp_task_wdt_reset();
  
  unsigned long now = millis();
  
  if (isAPMode) {
    server.handleClient();
    delay(10);
    return;
  }
  
  checkWiFi();
  reconnectMQTT();
  
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  checkBLEActivation();
  
  // Escanear BLE (5s)
  if (bleActive && now - lastBLEScan >= BLE_SCAN_INTERVAL) {
    lastBLEScan = now;
    if (!bleConnected) {
      scanBLE();
    } else {
      readBLEData();
    }
  }
  
  // Lógica dos relés (10s)
  if (now - lastRelayLogic >= RELAY_LOGIC_INTERVAL) {
    lastRelayLogic = now;
    handleRelayLogic();
  }
  
  // Status (15s)
  if (now - lastStatusPublish >= STATUS_PUBLISH_INTERVAL) {
    lastStatusPublish = now;
    publishRelayStatus();
  }
  
  // Heartbeat (30s)
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    publishHeartbeat();
  }
  
  delay(10);
}