/*****************************************************************************************
 * AquaSys / HydroSmart - Módulo Atuador ESP32
 * Versão: v4.0 COMPATÍVEL (20/10/2025)
 *
 * 🔧 Esta versão restaura a compatibilidade com o app:
 * - ✅ Protocolo MQTT do firmware v3.3 (que funcionava)
 * - ✅ Estrutura RelayConfig original
 * - ✅ Pinos originais dos relés
 * - ✅ Porta 8883 (MQTT over TLS)
 * - ✅ Comandos MQTT compatíveis com o app
 * - ✅ Simplificado (sem Bluetooth/WebServer/EmergencyMode)
 *
 *****************************************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

// ----------------------------- PINOUT (ORIGINAL) -------------------------------------
const int relayPins[8] = {23, 5, 4, 13, 22, 21, 14, 12}; // ✅ GPIO 12 restaurado
#define SETUP_BUTTON_PIN 0

// ----------------------------- MQTT CONFIG (PORTA CORRIGIDA) -------------------------
#define MQTT_BROKER "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883  // ✅ MQTT over TLS (não WebSocket)
#define MQTT_USERNAME "esp32-user"
#define MQTT_PASSWORD "HydroSmart123"
#define MQTT_CLIENT_ID "ESP32_Actuator_v4"

// Tópicos compatíveis com o app
#define MQTT_TOPIC_SENSORS_SUB "aquasys/sensors/all"
#define MQTT_TOPIC_COMMAND_SUB "aquasys/relay/command"
#define MQTT_TOPIC_STATUS_PUB "aquasys/relay/status"

// ----------------------------- TIMING -----------------------------------------------
const unsigned long HEARTBEAT_INTERVAL = 30000UL;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000UL;
const unsigned long STATUS_PUBLISH_INTERVAL = 15000UL;
const unsigned long NTP_UPDATE_INTERVAL = 3600000UL;
#define UTC_OFFSET_SECONDS -10800

// ----------------------------- GLOBALS -----------------------------------------------
Preferences preferences;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS, NTP_UPDATE_INTERVAL);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// ----------------------------- RELAY CONFIG (ESTRUTURA ORIGINAL) ---------------------
enum RelayMode {
  MODE_UNUSED = 0,
  MODE_LED = 1,
  MODE_CYCLE = 2,
  MODE_PH_UP = 3,      // Antigo MODE_PH
  MODE_TEMPERATURE = 4,
  MODE_HUMIDITY = 5,
  MODE_EC = 6,
  MODE_CO2 = 7,
  MODE_PH_DOWN = 8     // Novo modo
};

struct RelayConfig {
  RelayMode mode;
  int led_on_hour;
  int led_off_hour;
  int cycle_on_min;
  int cycle_off_min;
  int ph_pulse_sec;
  float temp_threshold_on;
  float temp_threshold_off;
  float humidity_threshold_on;
  float humidity_threshold_off;
  float ec_threshold;
  int ec_pulse_sec;
  float ph_threshold_low;
  float ph_threshold_high;
};

RelayConfig configs[8];

// ----------------------------- SENSOR DATA -------------------------------------------
struct SensorData {
  float ph;
  float ec;
  float airTemp;
  float humidity;
  float waterTemp;
  bool valid;
};
SensorData currentSensorData = {7.0, 800.0, 25.0, 60.0, 23.0, false};

// ----------------------------- STATE -------------------------------------------------
bool relayStates[8] = {false};
bool manual_override[8] = {false};
unsigned long cycle_last_toggle_times[8] = {0};
bool ph_check_today = false;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastStatusPublish = 0;
unsigned long last_ph_log_time = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastNtpUpdate = 0;

float ph_history[24] = {0};
int ph_history_index = 0;

// ----------------------------- FORWARD DECLARATIONS ----------------------------------
void setupWiFi();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishRelayStatus();
void publishHeartbeat();
void handleRelayLogic();
void updateRelay(int relayIndex, bool state);
void loadConfig();
void saveConfig();
void logHourlyPH(float currentPH);
float calculate24hAveragePH();

// ----------------------------- SETUP -------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n[INFO] ═══════════════════════════════════════");
  Serial.println("[INFO] AquaSys Atuador v4.0 COMPATÍVEL");
  Serial.println("[INFO] ═══════════════════════════════════════");
  
  // Configurar pinos dos relés
  for (int i = 0; i < 8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Relés desligados (lógica invertida)
    relayStates[i] = false;
    manual_override[i] = false;
  }
  
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  
  // Carregar configurações
  preferences.begin("actuator-cfg", false);
  loadConfig();
  
  // WiFi
  setupWiFi();
  
  if (WiFi.status() == WL_CONNECTED) {
    // NTP
    timeClient.begin();
    Serial.print("[INFO] Sincronizando NTP...");
    for (int i = 0; i < 10; i++) {
      if (timeClient.update()) {
        Serial.println(" OK!");
        Serial.printf("[INFO] Hora: %s\n", timeClient.getFormattedTime().c_str());
        break;
      }
      delay(500);
      Serial.print(".");
    }
    
    // MQTT
    setupMQTT();
  }
  
  Serial.println("[INFO] Sistema inicializado - v4.0 COMPATÍVEL");
}

// ----------------------------- LOOP --------------------------------------------------
void loop() {
  unsigned long now = millis();
  
  // WiFi
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
    return;
  }
  
  // MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }
  
  // Lógica dos relés
  if (timeClient.isTimeSet()) {
    handleRelayLogic();
    
    // Status periódico
    if (now - lastStatusPublish > STATUS_PUBLISH_INTERVAL) {
      publishRelayStatus();
      lastStatusPublish = now;
    }
    
    // Log de pH a cada hora
    if (now - last_ph_log_time > 3600000) {
      logHourlyPH(currentSensorData.ph);
      last_ph_log_time = now;
    }
  }
  
  // Heartbeat
  if (now - lastHeartbeatMs > HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeatMs = now;
  }
  
  // Atualizar NTP
  if (now - lastNtpUpdate > NTP_UPDATE_INTERVAL) {
    timeClient.update();
    lastNtpUpdate = now;
  }
}

// ----------------------------- WIFI --------------------------------------------------
void setupWiFi() {
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  
  if (ssid.length() == 0) {
    Serial.println("[WARN] WiFi não configurado!");
    return;
  }
  
  Serial.printf("[INFO] Conectando WiFi: %s\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[INFO] WiFi conectado: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WARN] WiFi não conectado");
  }
}

// ----------------------------- MQTT --------------------------------------------------
void setupMQTT() {
  espClient.setInsecure();  // Aceita certificado auto-assinado
  espClient.setHandshakeTimeout(30);
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024); // Buffer aumentado para JSONs grandes
  mqttClient.setKeepAlive(60);
  
  Serial.printf("[INFO] MQTT configurado: %s:%d\n", MQTT_BROKER, MQTT_PORT);
}

void reconnectMQTT() {
  unsigned long now = millis();
  
  if (WiFi.status() != WL_CONNECTED) return;
  if (now - lastMqttReconnectAttempt < MQTT_RECONNECT_INTERVAL) return;
  
  lastMqttReconnectAttempt = now;
  
  Serial.printf("[INFO] Tentando MQTT... ");
  
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("✅ CONECTADO!");
    
    mqttClient.subscribe(MQTT_TOPIC_SENSORS_SUB, 1);
    mqttClient.subscribe(MQTT_TOPIC_COMMAND_SUB, 1);
    
    Serial.println("[INFO] Subscrito aos tópicos");
    
    publishRelayStatus();
    publishHeartbeat();
    
  } else {
    Serial.printf("❌ FALHOU, rc=%d\n", mqttClient.state());
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  
  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';
  
  Serial.printf("[MQTT] %s: %s\n", topic, buffer);
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, buffer);
  
  if (error) {
    Serial.printf("[ERRO] JSON inválido: %s\n", error.c_str());
    return;
  }
  
  // ✅ SENSORES (compatível com formato antigo E novo)
  if (topicStr == MQTT_TOPIC_SENSORS_SUB) {
    currentSensorData.ph = doc["ph"] | 7.0;
    currentSensorData.ec = doc["ec"] | 800.0;
    currentSensorData.airTemp = doc["airTemp"] | doc["air_temp"] | 25.0;
    currentSensorData.humidity = doc["humidity"] | 60.0;
    currentSensorData.waterTemp = doc["waterTemp"] | doc["water_temp"] | 23.0;
    currentSensorData.valid = true;
    Serial.println("[INFO] Dados de sensores atualizados");
  }
  
  // ✅ COMANDOS (compatível com AMBOS os formatos!)
  else if (topicStr == MQTT_TOPIC_COMMAND_SUB) {
    const char* command = doc["command"];
    
    // FORMATO ANTIGO: {"command": "manual_override", "payload": {...}}
    if (command != nullptr) {
      JsonObject payload_obj = doc["payload"];
      
      if (strcmp(command, "update_config") == 0) {
        int relayIndex = payload_obj["relay_index"];
        if (relayIndex >= 0 && relayIndex < 8) {
          Serial.printf("[CONFIG] Atualizando Relé %d\n", relayIndex + 1);
          JsonObject cfg = payload_obj["config"];
          
          if (cfg.containsKey("mode")) configs[relayIndex].mode = (RelayMode)(cfg["mode"].as<int>());
          if (cfg.containsKey("led_on_hour")) configs[relayIndex].led_on_hour = cfg["led_on_hour"];
          if (cfg.containsKey("led_off_hour")) configs[relayIndex].led_off_hour = cfg["led_off_hour"];
          if (cfg.containsKey("cycle_on_min")) configs[relayIndex].cycle_on_min = cfg["cycle_on_min"];
          if (cfg.containsKey("cycle_off_min")) configs[relayIndex].cycle_off_min = cfg["cycle_off_min"];
          if (cfg.containsKey("ph_pulse_sec")) configs[relayIndex].ph_pulse_sec = cfg["ph_pulse_sec"];
          if (cfg.containsKey("ph_threshold_low")) configs[relayIndex].ph_threshold_low = cfg["ph_threshold_low"];
          if (cfg.containsKey("ph_threshold_high")) configs[relayIndex].ph_threshold_high = cfg["ph_threshold_high"];
          if (cfg.containsKey("temp_threshold_on")) configs[relayIndex].temp_threshold_on = cfg["temp_threshold_on"];
          if (cfg.containsKey("temp_threshold_off")) configs[relayIndex].temp_threshold_off = cfg["temp_threshold_off"];
          if (cfg.containsKey("humidity_threshold_on")) configs[relayIndex].humidity_threshold_on = cfg["humidity_threshold_on"];
          if (cfg.containsKey("humidity_threshold_off")) configs[relayIndex].humidity_threshold_off = cfg["humidity_threshold_off"];
          if (cfg.containsKey("ec_threshold")) configs[relayIndex].ec_threshold = cfg["ec_threshold"];
          if (cfg.containsKey("ec_pulse_sec")) configs[relayIndex].ec_pulse_sec = cfg["ec_pulse_sec"];
          
          saveConfig();
          Serial.println("[CONFIG] Configuração salva");
          publishRelayStatus();
        }
      }
      else if (strcmp(command, "manual_override") == 0) {
        int relayNum = payload_obj["relay"];
        if (relayNum >= 1 && relayNum <= 8) {
          manual_override[relayNum - 1] = true;
          const char* state = payload_obj["state"];
          updateRelay(relayNum - 1, strcmp(state, "on") == 0);
        }
      }
      else if (strcmp(command, "set_auto") == 0) {
        int relayNum = payload_obj["relay"];
        if (relayNum >= 1 && relayNum <= 8) {
          manual_override[relayNum - 1] = false;
          Serial.printf("[CMD] Relé %d → AUTO\n", relayNum);
          publishRelayStatus();
        }
      }
    }
    
    // FORMATO NOVO: {"relay": 1, "command": true}
    else if (doc.containsKey("relay")) {
      int relay = doc["relay"];
      int idx = relay - 1;
      
      if (idx >= 0 && idx < 8) {
        if (doc.containsKey("command")) {
          bool cmd = doc["command"];
          manual_override[idx] = true;
          updateRelay(idx, cmd);
        }
        else if (doc.containsKey("auto")) {
          manual_override[idx] = false;
          Serial.printf("[CMD] Relé %d → AUTO\n", relay);
          publishRelayStatus();
        }
      }
    }
  }
}

void publishRelayStatus() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  for (int i = 0; i < 8; i++) {
    String key = "relay" + String(i + 1);
    doc[key] = relayStates[i];
  }
  
  String output;
  serializeJson(doc, output);
  
  bool published = mqttClient.publish(MQTT_TOPIC_STATUS_PUB, output.c_str(), true);
  if (published) {
    Serial.printf("[MQTT] Status publicado\n");
  } else {
    Serial.println("[ERRO] Falha ao publicar status");
  }
}

void publishHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["device"] = MQTT_CLIENT_ID;
  doc["uptime"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  mqttClient.publish("aquasys/heartbeat", buffer);
}

// ----------------------------- RELAY LOGIC -------------------------------------------
void handleRelayLogic() {
  if (!timeClient.isTimeSet()) return;
  
  // Reset diário do check de pH
  if (timeClient.getHours() == 0) {
    ph_check_today = false;
  }
  
  for (int i = 0; i < 8; i++) {
    if (manual_override[i] || configs[i].mode == MODE_UNUSED) continue;
    
    bool shouldBeOn = relayStates[i];
    
    switch (configs[i].mode) {
      case MODE_LED: {
        int currentHour = timeClient.getHours();
        int onHour = configs[i].led_on_hour;
        int offHour = configs[i].led_off_hour;
        
        if (onHour < offHour) {
          shouldBeOn = (currentHour >= onHour && currentHour < offHour);
        } else {
          shouldBeOn = (currentHour >= onHour || currentHour < offHour);
        }
        break;
      }
      
      case MODE_CYCLE: {
        unsigned long now = millis();
        unsigned long on_ms = configs[i].cycle_on_min * 60000UL;
        unsigned long off_ms = configs[i].cycle_off_min * 60000UL;
        
        if (relayStates[i] && (now - cycle_last_toggle_times[i] >= on_ms)) {
          shouldBeOn = false;
          cycle_last_toggle_times[i] = now;
        } else if (!relayStates[i] && (now - cycle_last_toggle_times[i] >= off_ms)) {
          shouldBeOn = true;
          cycle_last_toggle_times[i] = now;
        }
        break;
      }
      
      case MODE_PH_UP: {
        if (timeClient.getHours() == 6 && !ph_check_today) {
          if (calculate24hAveragePH() < configs[i].ph_threshold_low) {
            shouldBeOn = true;
          }
        }
        break;
      }
      
      case MODE_PH_DOWN: {
        if (timeClient.getHours() == 6 && !ph_check_today) {
          if (calculate24hAveragePH() > configs[i].ph_threshold_high) {
            shouldBeOn = true;
          }
        }
        break;
      }
      
      case MODE_TEMPERATURE: {
        if (currentSensorData.airTemp >= configs[i].temp_threshold_on) {
          shouldBeOn = true;
        } else if (currentSensorData.airTemp <= configs[i].temp_threshold_off) {
          shouldBeOn = false;
        }
        break;
      }
      
      case MODE_HUMIDITY: {
        if (currentSensorData.humidity >= configs[i].humidity_threshold_on) {
          shouldBeOn = true;
        } else if (currentSensorData.humidity <= configs[i].humidity_threshold_off) {
          shouldBeOn = false;
        }
        break;
      }
      
      case MODE_EC: {
        if (currentSensorData.ec < configs[i].ec_threshold) {
          shouldBeOn = true;
        }
        break;
      }
      
      default:
        shouldBeOn = false;
        break;
    }
    
    updateRelay(i, shouldBeOn);
  }
  
  // Marcar check de pH como feito às 6h
  if (timeClient.getHours() == 6) {
    ph_check_today = true;
  }
}

void updateRelay(int relayIndex, bool state) {
  if (relayIndex < 0 || relayIndex >= 8) return;
  
  if (relayStates[relayIndex] != state) {
    relayStates[relayIndex] = state;
    digitalWrite(relayPins[relayIndex], state ? LOW : HIGH); // Lógica invertida
    
    Serial.printf("[RELAY] Relé %d → %s\n", relayIndex + 1, state ? "LIGADO" : "DESLIGADO");
    publishRelayStatus();
    
    // Pulso para pH e EC
    RelayMode currentMode = configs[relayIndex].mode;
    if (state && (currentMode == MODE_PH_UP || currentMode == MODE_PH_DOWN || currentMode == MODE_EC)) {
      unsigned long pulse_ms = (currentMode == MODE_EC) 
        ? configs[relayIndex].ec_pulse_sec * 1000UL 
        : configs[relayIndex].ph_pulse_sec * 1000UL;
      
      unsigned long pulseEnd = millis() + pulse_ms;
      while (millis() < pulseEnd) {
        mqttClient.loop();
        delay(10);
      }
      updateRelay(relayIndex, false);
    }
  }
}

// ----------------------------- CONFIG PERSISTENCE ------------------------------------
void loadConfig() {
  Serial.println("[CONFIG] Carregando configurações...");
  
  for (int i = 0; i < 8; i++) {
    String prefix = "r" + String(i) + "_";
    
    configs[i].mode = (RelayMode)preferences.getInt((prefix + "mode").c_str(), MODE_UNUSED);
    configs[i].led_on_hour = preferences.getInt((prefix + "lonh").c_str(), 6);
    configs[i].led_off_hour = preferences.getInt((prefix + "loffh").c_str(), 0);
    configs[i].cycle_on_min = preferences.getInt((prefix + "conm").c_str(), 15);
    configs[i].cycle_off_min = preferences.getInt((prefix + "coffm").c_str(), 15);
    configs[i].ph_pulse_sec = preferences.getInt((prefix + "phps").c_str(), 5);
    configs[i].ph_threshold_low = preferences.getFloat((prefix + "phtl").c_str(), 5.8);
    configs[i].ph_threshold_high = preferences.getFloat((prefix + "phth").c_str(), 6.5);
    configs[i].temp_threshold_on = preferences.getFloat((prefix + "tton").c_str(), 28.0);
    configs[i].temp_threshold_off = preferences.getFloat((prefix + "ttof").c_str(), 26.0);
    configs[i].humidity_threshold_on = preferences.getFloat((prefix + "hton").c_str(), 75.0);
    configs[i].humidity_threshold_off = preferences.getFloat((prefix + "htof").c_str(), 65.0);
    configs[i].ec_threshold = preferences.getFloat((prefix + "ect").c_str(), 1200.0);
    configs[i].ec_pulse_sec = preferences.getInt((prefix + "ecps").c_str(), 5);
  }
  
  // Configurações padrão se primeira vez
  if (preferences.getInt("r0_mode", -1) == -1) {
    configs[0].mode = MODE_LED;
    configs[1].mode = MODE_CYCLE;
    configs[2].mode = MODE_PH_UP;
    configs[3].mode = MODE_TEMPERATURE;
    saveConfig();
  }
}

void saveConfig() {
  Serial.println("[CONFIG] Salvando configurações...");
  
  for (int i = 0; i < 8; i++) {
    String prefix = "r" + String(i) + "_";
    
    preferences.putInt((prefix + "mode").c_str(), configs[i].mode);
    preferences.putInt((prefix + "lonh").c_str(), configs[i].led_on_hour);
    preferences.putInt((prefix + "loffh").c_str(), configs[i].led_off_hour);
    preferences.putInt((prefix + "conm").c_str(), configs[i].cycle_on_min);
    preferences.putInt((prefix + "coffm").c_str(), configs[i].cycle_off_min);
    preferences.putInt((prefix + "phps").c_str(), configs[i].ph_pulse_sec);
    preferences.putFloat((prefix + "phtl").c_str(), configs[i].ph_threshold_low);
    preferences.putFloat((prefix + "phth").c_str(), configs[i].ph_threshold_high);
    preferences.putFloat((prefix + "tton").c_str(), configs[i].temp_threshold_on);
    preferences.putFloat((prefix + "ttof").c_str(), configs[i].temp_threshold_off);
    preferences.putFloat((prefix + "hton").c_str(), configs[i].humidity_threshold_on);
    preferences.putFloat((prefix + "htof").c_str(), configs[i].humidity_threshold_off);
    preferences.putFloat((prefix + "ect").c_str(), configs[i].ec_threshold);
    preferences.putInt((prefix + "ecps").c_str(), configs[i].ec_pulse_sec);
  }
}

// ----------------------------- PH TRACKING -------------------------------------------
void logHourlyPH(float currentPH) {
  ph_history[ph_history_index] = currentPH;
  ph_history_index = (ph_history_index + 1) % 24;
  Serial.printf("[PH] Novo valor %.2f logado no histórico\n", currentPH);
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
