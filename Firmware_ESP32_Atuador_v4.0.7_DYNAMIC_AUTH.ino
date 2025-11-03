/*
 * ═══════════════════════════════════════════════════════════════════════
 * FIRMWARE ESP32 - MÓDULO DE ATUADORES v4.0.7-DYNAMIC-AUTH
 * Sistema AquaSys Nexus - Hidroponia Inteligente
 * ═══════════════════════════════════════════════════════════════════════
 * 
 * NOVIDADES NESTA VERSÃO:
 * - Autenticação dinâmica via Edge Function
 * - Busca credenciais MQTT do servidor ao invés de usar fixas
 * - Tópicos MQTT personalizados por dispositivo (UUID)
 * - Suporte a firmware_version maior (VARCHAR 50)
 * - Melhor tratamento de erros HTTP
 * 
 * DEPENDÊNCIAS:
 * - WiFiClientSecure (ESP32 Core)
 * - PubSubClient v2.8+ (MQTT)
 * - ArduinoJson v6.21+ (Parsing JSON)
 * - HTTPClient (ESP32 Core)
 * 
 * ═══════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// ═══════════════════════════════════════════════════════════════════════
// CONFIGURAÇÕES DE REDE E IDENTIFICAÇÃO
// ═══════════════════════════════════════════════════════════════════════

// WiFi
const char* WIFI_SSID = "SUA_REDE_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA_WIFI";

// Identificação do Dispositivo (gerado do MAC Address)
String DEVICE_UUID = "";
const char* FIRMWARE_VERSION = "4.0.7-DYNAMIC-AUTH";

// Servidor de autenticação
const char* AUTH_SERVER = "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/device-auth";
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw";

// Configurações MQTT (serão obtidas dinamicamente)
String MQTT_BROKER = "";
String MQTT_USERNAME = "";
String MQTT_PASSWORD = "";
String MQTT_CLIENT_ID = "";

// Tópicos MQTT (serão personalizados por UUID)
String TOPIC_RELAY_STATUS = "";
String TOPIC_RELAY_COMMAND = "";
String TOPIC_HEARTBEAT = "";

// ═══════════════════════════════════════════════════════════════════════
// CONFIGURAÇÕES DE HARDWARE
// ═══════════════════════════════════════════════════════════════════════

// Pinos dos Relés
const int RELAY_PINS[8] = {25, 26, 27, 14, 12, 13, 15, 2};
bool relayStates[8] = {false, false, false, false, false, false, false, false};

// ═══════════════════════════════════════════════════════════════════════
// CLIENTES DE REDE
// ═══════════════════════════════════════════════════════════════════════

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// ═══════════════════════════════════════════════════════════════════════
// TIMERS E CONTROLE
// ═══════════════════════════════════════════════════════════════════════

unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 30000; // 30 segundos

unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000; // 5 segundos

bool authCompleted = false;

// ═══════════════════════════════════════════════════════════════════════
// PROTÓTIPOS DE FUNÇÕES
// ═══════════════════════════════════════════════════════════════════════

void generateDeviceUUID();
bool authenticateDevice();
void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishRelayStatus();
void publishHeartbeat();
void handleRelayCommand(JsonDocument& doc);
void setupRelays();
void logMessage(String level, String message);

// ═══════════════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  logMessage("INFO", "═══════════════════════════════════════════");
  logMessage("INFO", "AquaSys Nexus - Módulo de Atuadores");
  logMessage("INFO", "Firmware: " + String(FIRMWARE_VERSION));
  logMessage("INFO", "═══════════════════════════════════════════");
  
  // Gerar UUID do dispositivo
  generateDeviceUUID();
  logMessage("INFO", "Device UUID: " + DEVICE_UUID);
  
  // Configurar relés
  setupRelays();
  
  // Conectar WiFi
  connectWiFi();
  
  // Autenticar dispositivo e obter credenciais MQTT
  if (!authenticateDevice()) {
    logMessage("ERROR", "Falha na autenticação! Dispositivo não registrado?");
    logMessage("ERROR", "Registre o dispositivo em: /devices");
    logMessage("ERROR", "UUID: " + DEVICE_UUID);
    delay(10000);
    ESP.restart();
  }
  
  // Configurar MQTT Client
  wifiClient.setInsecure(); // Para HiveMQ Cloud (produção deve usar certificado)
  mqttClient.setServer(MQTT_BROKER.c_str(), 8884);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);
  
  // Conectar MQTT
  connectMQTT();
  
  logMessage("INFO", "Setup completo! Sistema operacional.");
}

// ═══════════════════════════════════════════════════════════════════════
// LOOP PRINCIPAL
// ═══════════════════════════════════════════════════════════════════════

void loop() {
  // Manter conexão MQTT
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      connectMQTT();
    }
  } else {
    mqttClient.loop();
  }
  
  // Publicar heartbeat periodicamente
  unsigned long now = millis();
  if (now - lastHeartbeat > HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    publishHeartbeat();
  }
  
  delay(100);
}

// ═══════════════════════════════════════════════════════════════════════
// FUNÇÕES DE AUTENTICAÇÃO
// ═══════════════════════════════════════════════════════════════════════

void generateDeviceUUID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  char uuid[32];
  snprintf(uuid, sizeof(uuid), "HYDRO-%02X%02X-%02X%02X-%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  DEVICE_UUID = String(uuid);
}

bool authenticateDevice() {
  logMessage("INFO", "Autenticando dispositivo...");
  
  HTTPClient http;
  http.begin(AUTH_SERVER);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  
  // Preparar payload JSON
  StaticJsonDocument<256> requestDoc;
  requestDoc["device_uuid"] = DEVICE_UUID;
  requestDoc["firmware_version"] = FIRMWARE_VERSION;
  
  String requestBody;
  serializeJson(requestDoc, requestBody);
  
  logMessage("DEBUG", "Auth request: " + requestBody);
  
  int httpCode = http.POST(requestBody);
  
  if (httpCode == 200) {
    String response = http.getString();
    logMessage("DEBUG", "Auth response: " + response);
    
    StaticJsonDocument<1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (error) {
      logMessage("ERROR", "JSON parse error: " + String(error.c_str()));
      http.end();
      return false;
    }
    
    // Extrair configurações MQTT
    if (responseDoc["success"]) {
      JsonObject mqtt_config = responseDoc["mqtt_config"];
      
      MQTT_BROKER = mqtt_config["broker"].as<String>();
      MQTT_USERNAME = mqtt_config["username"].as<String>();
      MQTT_PASSWORD = mqtt_config["password"].as<String>();
      MQTT_CLIENT_ID = mqtt_config["client_id"].as<String>();
      
      JsonObject topics = mqtt_config["topics"];
      TOPIC_RELAY_STATUS = topics["relay_status"].as<String>();
      TOPIC_RELAY_COMMAND = topics["relay_command"].as<String>();
      TOPIC_HEARTBEAT = topics["heartbeat"].as<String>();
      
      logMessage("INFO", "Autenticação bem-sucedida!");
      logMessage("INFO", "MQTT Broker: " + MQTT_BROKER);
      logMessage("INFO", "Topic Status: " + TOPIC_RELAY_STATUS);
      
      http.end();
      authCompleted = true;
      return true;
    }
  } else {
    logMessage("ERROR", "Auth failed! HTTP Code: " + String(httpCode));
    String response = http.getString();
    logMessage("ERROR", "Response: " + response);
  }
  
  http.end();
  return false;
}

// ═══════════════════════════════════════════════════════════════════════
// FUNÇÕES DE REDE
// ═══════════════════════════════════════════════════════════════════════

void connectWiFi() {
  logMessage("INFO", "Conectando WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    logMessage("INFO", "WiFi conectado!");
    logMessage("INFO", "IP: " + WiFi.localIP().toString());
    logMessage("INFO", "RSSI: " + String(WiFi.RSSI()) + " dBm");
  } else {
    logMessage("ERROR", "Falha ao conectar WiFi!");
    delay(5000);
    ESP.restart();
  }
}

void connectMQTT() {
  if (!authCompleted) {
    logMessage("WARN", "Aguardando autenticação...");
    return;
  }
  
  logMessage("INFO", "Conectando MQTT...");
  logMessage("DEBUG", "Broker: " + MQTT_BROKER);
  logMessage("DEBUG", "ClientID: " + MQTT_CLIENT_ID);
  logMessage("DEBUG", "Username: " + MQTT_USERNAME);
  
  if (mqttClient.connect(MQTT_CLIENT_ID.c_str(), MQTT_USERNAME.c_str(), MQTT_PASSWORD.c_str())) {
    logMessage("INFO", "MQTT conectado!");
    
    // Subscrever ao tópico de comandos
    if (mqttClient.subscribe(TOPIC_RELAY_COMMAND.c_str())) {
      logMessage("INFO", "Subscrito a: " + TOPIC_RELAY_COMMAND);
    } else {
      logMessage("ERROR", "Falha ao subscrever comandos!");
    }
    
    // Publicar status inicial
    publishRelayStatus();
    publishHeartbeat();
    
  } else {
    int state = mqttClient.state();
    logMessage("ERROR", "MQTT falhou! State: " + String(state));
    
    switch(state) {
      case -4: logMessage("ERROR", "MQTT_CONNECTION_TIMEOUT"); break;
      case -3: logMessage("ERROR", "MQTT_CONNECTION_LOST"); break;
      case -2: logMessage("ERROR", "MQTT_CONNECT_FAILED"); break;
      case -1: logMessage("ERROR", "MQTT_DISCONNECTED"); break;
      case 1: logMessage("ERROR", "MQTT_CONNECT_BAD_PROTOCOL"); break;
      case 2: logMessage("ERROR", "MQTT_CONNECT_BAD_CLIENT_ID"); break;
      case 3: logMessage("ERROR", "MQTT_CONNECT_UNAVAILABLE"); break;
      case 4: logMessage("ERROR", "MQTT_CONNECT_BAD_CREDENTIALS"); break;
      case 5: logMessage("ERROR", "MQTT_CONNECT_UNAUTHORIZED"); break;
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════
// CALLBACK MQTT
// ═══════════════════════════════════════════════════════════════════════

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  logMessage("INFO", "Mensagem MQTT recebida em: " + String(topic));
  
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  logMessage("DEBUG", "Payload: " + String(message));
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    logMessage("ERROR", "JSON parse error: " + String(error.c_str()));
    return;
  }
  
  // Processar comando de relé
  if (String(topic) == TOPIC_RELAY_COMMAND) {
    handleRelayCommand(doc);
  }
}

// ═══════════════════════════════════════════════════════════════════════
// FUNÇÕES DE CONTROLE DE RELÉS
// ═══════════════════════════════════════════════════════════════════════

void setupRelays() {
  logMessage("INFO", "Configurando relés...");
  
  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW); // Relés desligados (lógica invertida)
    relayStates[i] = false;
  }
  
  logMessage("INFO", "8 relés configurados e desligados");
}

void handleRelayCommand(JsonDocument& doc) {
  int relayIndex = doc["relay_index"] | -1;
  bool command = doc["command"] | false;
  
  if (relayIndex < 0 || relayIndex >= 8) {
    logMessage("ERROR", "Índice de relé inválido: " + String(relayIndex));
    return;
  }
  
  logMessage("INFO", "Comando Relé " + String(relayIndex + 1) + ": " + (command ? "ON" : "OFF"));
  
  // Lógica invertida: LOW = ON, HIGH = OFF
  digitalWrite(RELAY_PINS[relayIndex], command ? LOW : HIGH);
  relayStates[relayIndex] = command;
  
  // Publicar status atualizado
  delay(100);
  publishRelayStatus();
}

void publishRelayStatus() {
  if (!mqttClient.connected()) {
    return;
  }
  
  StaticJsonDocument<512> doc;
  
  doc["device_uuid"] = DEVICE_UUID;
  doc["timestamp"] = millis();
  doc["relay1_led"] = relayStates[0];
  doc["relay2_pump"] = relayStates[1];
  doc["relay3_ph_up"] = relayStates[2];
  doc["relay4_fan"] = relayStates[3];
  doc["relay5_humidity"] = relayStates[4];
  doc["relay6_ec"] = relayStates[5];
  doc["relay7_co2"] = relayStates[6];
  doc["relay8_generic"] = relayStates[7];
  
  String output;
  serializeJson(doc, output);
  
  if (mqttClient.publish(TOPIC_RELAY_STATUS.c_str(), output.c_str())) {
    logMessage("DEBUG", "Status publicado: " + output);
  } else {
    logMessage("ERROR", "Falha ao publicar status!");
  }
}

void publishHeartbeat() {
  if (!mqttClient.connected()) {
    return;
  }
  
  StaticJsonDocument<768> doc;
  
  doc["device_uuid"] = DEVICE_UUID;
  doc["device_type"] = "actuator";
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["timestamp"] = millis();
  doc["uptime_seconds"] = millis() / 1000;
  
  // Status de rede
  JsonObject network = doc.createNestedObject("network");
  network["wifi_ssid"] = WiFi.SSID();
  network["wifi_rssi"] = WiFi.RSSI();
  network["wifi_ip"] = WiFi.localIP().toString();
  network["mqtt_connected"] = mqttClient.connected();
  
  // Status dos relés
  JsonArray relays = doc.createNestedArray("relay_states");
  for (int i = 0; i < 8; i++) {
    relays.add(relayStates[i]);
  }
  
  // Memória
  doc["free_heap"] = ESP.getFreeHeap();
  doc["min_free_heap"] = ESP.getMinFreeHeap();
  
  String output;
  serializeJson(doc, output);
  
  if (mqttClient.publish(TOPIC_HEARTBEAT.c_str(), output.c_str())) {
    logMessage("INFO", "Heartbeat enviado");
  } else {
    logMessage("ERROR", "Falha ao enviar heartbeat!");
  }
}

// ═══════════════════════════════════════════════════════════════════════
// FUNÇÕES DE LOGGING
// ═══════════════════════════════════════════════════════════════════════

void logMessage(String level, String message) {
  String timestamp = String(millis() / 1000.0, 2);
  Serial.println("[" + timestamp + "s] [" + level + "] " + message);
}
