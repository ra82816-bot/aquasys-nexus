/*****************************************************************************************
 * AquaSys / HydroSmart - Módulo Atuador ESP32
 * Versão: v4.1 COM ACCESS POINT (20/10/2025)
 *
 * 🔧 Esta versão mantém compatibilidade E adiciona configuração WiFi via AP:
 * - ✅ Protocolo MQTT do firmware v3.3 (que funcionava)
 * - ✅ Estrutura RelayConfig original
 * - ✅ Pinos originais dos relés
 * - ✅ Porta 8883 (MQTT over TLS)
 * - ✅ Access Point mode para configurar WiFi
 * - ✅ WebServer para interface de configuração
 * - ✅ Botão de setup força entrada em modo AP
 *
 *****************************************************************************************/

#include <WiFi.h>
#include <WebServer.h>
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
#define MQTT_CLIENT_ID "ESP32_Actuator_v4.1"

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
WebServer server(80);

String ssid_sta = "";
String password_sta = "";
bool wifiConfigured = false;
bool inAPMode = false;

// ----------------------------- RELAY CONFIGURATION -----------------------------------
enum RelayMode {
  MODE_UNUSED, MODE_LED, MODE_CYCLE,
  MODE_PH_UP,
  MODE_TEMPERATURE, MODE_HUMIDITY, MODE_EC, MODE_CO2,
  MODE_PH_DOWN
};

const char* modeNames[] = {
  "Nao Usado", "LED", "Ciclo", "pH (Subir)", "Temperatura",
  "Umidade", "EC", "CO2", "pH (Baixar)"
};

struct RelayConfig {
  RelayMode mode = MODE_UNUSED;
  int led_on_hour = 6, led_off_hour = 0;
  int cycle_on_min = 15, cycle_off_min = 15;
  int ph_pulse_sec = 5;
  float temp_threshold_on = 28.0, temp_threshold_off = 26.0;
  float humidity_threshold_on = 75.0, humidity_threshold_off = 65.0;
  float ec_threshold = 1200.0;
  int ec_pulse_sec = 5;
  float ph_threshold_low = 5.8;
  float ph_threshold_high = 6.5;
};
RelayConfig configs[8];

// ----------------------------- SENSOR DATA ------------------------------------------
struct SensorData {
  float ph = 7.0;
  float airTemp = 25.0;
  float humidity = 60.0;
  float ec = 800.0;
  float waterTemp = 22.0;
  int co2 = 400;
} currentSensorData;

// ----------------------------- STATE ------------------------------------------------
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
void startAPMode();
void handleRoot();
void handleSave();
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
  delay(1000);
  
  Serial.println("\n[INFO] ═══════════════════════════════════════");
  Serial.println("[INFO] AquaSys Atuador v4.1 COM AP");
  Serial.println("[INFO] ═══════════════════════════════════════");
  
  // Botão de setup
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  delay(50);
  
  // Verificar se botão está pressionado (forçar AP mode)
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) {
    Serial.println("[INFO] Botão de Setup pressionado! Forçando modo AP...");
    startAPMode();
    return;
  }
  
  // Configurar pinos dos relés
  Serial.println("[INFO] Configurando pinos dos relés...");
  for (int i = 0; i < 8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Relés desligados (lógica invertida)
    relayStates[i] = false;
    manual_override[i] = false;
  }
  
  // Carregar configurações
  preferences.begin("actuator-cfg", false);
  loadConfig();
  
  // WiFi
  setupWiFi();
  
  if (WiFi.status() == WL_CONNECTED) {
    // NTP
    Serial.print("[INFO] Sincronizando NTP...");
    timeClient.begin();
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
  
  Serial.println("[INFO] Sistema inicializado - v4.1 COM AP");
}

// ----------------------------- LOOP --------------------------------------------------
void loop() {
  // Se está em modo AP, só processar servidor web
  if (inAPMode) {
    server.handleClient();
    return;
  }
  
  unsigned long now = millis();
  
  // WiFi
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConfigured) {
      Serial.println("[WARN] WiFi desconectado, reconectando...");
      setupWiFi();
    }
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
    
    // Heartbeat
    if (now - lastHeartbeatMs > HEARTBEAT_INTERVAL) {
      publishHeartbeat();
      lastHeartbeatMs = now;
    }
  }
  
  // Atualizar NTP periodicamente
  if (now - lastNtpUpdate > NTP_UPDATE_INTERVAL) {
    timeClient.update();
    lastNtpUpdate = now;
  }
}

// ----------------------------- WIFI & AP MODE ----------------------------------------
void setupWiFi() {
  ssid_sta = preferences.getString("ssid", "");
  password_sta = preferences.getString("password", "");
  
  if (ssid_sta.length() > 0) {
    wifiConfigured = true;
    Serial.println("[INFO] Credenciais encontradas. Conectando ao WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[INFO] WiFi conectado!");
      Serial.print("[INFO] IP: ");
      Serial.println(WiFi.localIP());
      inAPMode = false;
      server.stop();
    } else {
      Serial.println("\n[WARN] Falha ao conectar. Iniciando modo AP.");
      startAPMode();
    }
  } else {
    Serial.println("[INFO] Nenhuma credencial WiFi encontrada. Iniciando modo AP.");
    startAPMode();
  }
}

void startAPMode() {
  inAPMode = true;
  wifiConfigured = false;
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_HydroSmart_Config");
  
  Serial.println("[INFO] ═══════════════════════════════════════");
  Serial.println("[INFO] MODO ACCESS POINT ATIVO");
  Serial.print("[INFO] AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("[INFO] SSID: ESP32_HydroSmart_Config");
  Serial.println("[INFO] Acesse http://192.168.4.1 no navegador");
  Serial.println("[INFO] ═══════════════════════════════════════");
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>"
                "<title>HydroSmart - Configurar WiFi</title>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<style>"
                "body{font-family: Arial, sans-serif; text-align: center; padding: 50px; background: #f0f8ff;}"
                "h2{color: #0066cc;}"
                "form{display: inline-block; padding: 30px; border-radius: 10px; background: white; box-shadow: 0 2px 10px rgba(0,0,0,0.1);}"
                "input{width: 100%; padding: 12px; margin: 10px 0; box-sizing: border-box; border: 1px solid #ccc; border-radius: 5px;}"
                "input[type=submit]{background: #0066cc; color: white; border: none; cursor: pointer; font-size: 16px;}"
                "input[type=submit]:hover{background: #0052a3;}"
                "label{display: block; text-align: left; margin-top: 15px; color: #333;}"
                "</style>"
                "</head><body>"
                "<h2>🌊 HydroSmart - Configuração WiFi</h2>"
                "<p>Configure as credenciais da sua rede WiFi</p>"
                "<form action='/save' method='POST'>"
                "<label>SSID (Nome da Rede):</label>"
                "<input type='text' name='ssid' required placeholder='Nome da sua rede WiFi'><br>"
                "<label>Senha:</label>"
                "<input type='password' name='password' required placeholder='Senha da rede WiFi'><br>"
                "<input type='submit' value='Salvar e Reiniciar'>"
                "</form>"
                "</body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  ssid_sta = server.arg("ssid");
  password_sta = server.arg("password");
  
  preferences.putString("ssid", ssid_sta);
  preferences.putString("password", password_sta);
  
  String html = "<!DOCTYPE html><html><head>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<style>body{font-family: Arial, sans-serif; text-align: center; padding: 50px; background: #f0f8ff;}"
                "h2{color: #00cc00;}</style>"
                "</head><body>"
                "<h2>✅ Configurações Salvas!</h2>"
                "<p>O ESP32 irá reiniciar e conectar à rede WiFi.</p>"
                "<p>Aguarde alguns segundos...</p>"
                "</body></html>";
  
  server.send(200, "text/html", html);
  
  Serial.println("[INFO] Credenciais WiFi salvas. Reiniciando...");
  delay(2000);
  ESP.restart();
}

// ----------------------------- MQTT --------------------------------------------------
void setupMQTT() {
  espClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024); // Buffer maior para evitar problemas
  Serial.println("[INFO] MQTT configurado (porta 8883)");
}

void reconnectMQTT() {
  unsigned long now = millis();
  if (now - lastMqttReconnectAttempt < MQTT_RECONNECT_INTERVAL) {
    return;
  }
  lastMqttReconnectAttempt = now;
  
  if (!mqttClient.connected()) {
    Serial.print("[INFO] Conectando ao MQTT...");
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println(" Conectado!");
      mqttClient.subscribe(MQTT_TOPIC_SENSORS_SUB);
      mqttClient.subscribe(MQTT_TOPIC_COMMAND_SUB);
      publishRelayStatus();
      publishHeartbeat();
    } else {
      Serial.print(" Falha (rc=");
      Serial.print(mqttClient.state());
      Serial.println(")");
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.print("[WARN] JSON parse falhou: ");
    Serial.println(error.c_str());
    return;
  }
  
  String topicStr = String(topic);
  
  // Dados dos sensores
  if (topicStr == MQTT_TOPIC_SENSORS_SUB) {
    if (doc.containsKey("ph")) currentSensorData.ph = doc["ph"];
    if (doc.containsKey("airTemp")) currentSensorData.airTemp = doc["airTemp"];
    if (doc.containsKey("humidity")) currentSensorData.humidity = doc["humidity"];
    if (doc.containsKey("ec")) currentSensorData.ec = doc["ec"];
    if (doc.containsKey("waterTemp")) currentSensorData.waterTemp = doc["waterTemp"];
    if (doc.containsKey("co2")) currentSensorData.co2 = doc["co2"];
    
    Serial.printf("[DATA] pH:%.2f Temp:%.1f Hum:%.1f EC:%.0f\n",
                  currentSensorData.ph, currentSensorData.airTemp,
                  currentSensorData.humidity, currentSensorData.ec);
  }
  
  // Comandos de relé
  else if (topicStr == MQTT_TOPIC_COMMAND_SUB) {
    // Formato: {"relay": 1, "state": true} OU {"relay1": true, "relay2": false}
    
    if (doc.containsKey("relay") && doc.containsKey("state")) {
      int relay = doc["relay"];
      bool state = doc["state"];
      
      if (relay >= 1 && relay <= 8) {
        manual_override[relay - 1] = true;
        updateRelay(relay - 1, state);
        Serial.printf("[CMD] Relé %d -> %s (MANUAL)\n", relay, state ? "ON" : "OFF");
      }
    }
    else {
      // Formato alternativo: relay1, relay2, etc.
      for (int i = 0; i < 8; i++) {
        String key = "relay" + String(i + 1);
        if (doc.containsKey(key)) {
          bool state = doc[key];
          manual_override[i] = true;
          updateRelay(i, state);
          Serial.printf("[CMD] Relé %d -> %s (MANUAL)\n", i + 1, state ? "ON" : "OFF");
        }
      }
    }
    
    // Comando de reset manual override
    if (doc.containsKey("auto")) {
      int relay = doc["auto"];
      if (relay >= 1 && relay <= 8) {
        manual_override[relay - 1] = false;
        Serial.printf("[CMD] Relé %d -> AUTOMÁTICO\n", relay);
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
  doc["timestamp"] = timeClient.getEpochTime();
  
  String output;
  serializeJson(doc, output);
  mqttClient.publish(MQTT_TOPIC_STATUS_PUB, output.c_str(), true);
}

void publishHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["device"] = MQTT_CLIENT_ID;
  doc["uptime"] = millis() / 1000;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  
  String output;
  serializeJson(doc, output);
  mqttClient.publish("aquasys/heartbeat", output.c_str());
}

// ----------------------------- RELAY LOGIC -------------------------------------------
void handleRelayLogic() {
  if (!timeClient.isTimeSet()) return;
  
  // Reset flag de pH às 00h
  if (timeClient.getHours() == 0) {
    ph_check_today = false;
  }
  
  for (int i = 0; i < 8; i++) {
    if (manual_override[i] || configs[i].mode == MODE_UNUSED) {
      continue;
    }
    
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
          float avg = calculate24hAveragePH();
          if (avg > 0 && avg < configs[i].ph_threshold_low) {
            shouldBeOn = true;
          }
        }
        break;
      }
      
      case MODE_PH_DOWN: {
        if (timeClient.getHours() == 6 && !ph_check_today) {
          float avg = calculate24hAveragePH();
          if (avg > configs[i].ph_threshold_high) {
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
  
  // Marcar que já foi feito o check de pH às 6h
  if (timeClient.getHours() == 6) {
    ph_check_today = true;
  }
}

void updateRelay(int relayIndex, bool state) {
  if (relayIndex < 0 || relayIndex >= 8) return;
  
  if (relayStates[relayIndex] != state) {
    relayStates[relayIndex] = state;
    digitalWrite(relayPins[relayIndex], state ? LOW : HIGH); // Lógica invertida
    
    Serial.printf("[RELAY] Relé %d -> %s\n", relayIndex + 1, state ? "LIGADO" : "DESLIGADO");
    publishRelayStatus();
    
    // Pulso automático para pH e EC
    RelayMode currentMode = configs[relayIndex].mode;
    if (state && (currentMode == MODE_PH_UP || currentMode == MODE_PH_DOWN || currentMode == MODE_EC)) {
      unsigned long pulse_ms = (currentMode == MODE_EC) ? 
                               configs[relayIndex].ec_pulse_sec * 1000UL : 
                               configs[relayIndex].ph_pulse_sec * 1000UL;
      
      Serial.printf("[PULSE] Aguardando %lu ms...\n", pulse_ms);
      unsigned long pulseEnd = millis() + pulse_ms;
      while (millis() < pulseEnd) {
        mqttClient.loop();
        delay(10);
      }
      updateRelay(relayIndex, false);
    }
  }
}

// ----------------------------- PERSISTENCE -------------------------------------------
void loadConfig() {
  Serial.println("[INFO] Carregando configurações dos 8 relés...");
  
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
  
  // Defaults para primeira inicialização
  if (preferences.getInt("r0_mode", -1) == -1) configs[0].mode = MODE_LED;
  if (preferences.getInt("r1_mode", -1) == -1) configs[1].mode = MODE_CYCLE;
  if (preferences.getInt("r2_mode", -1) == -1) configs[2].mode = MODE_PH_UP;
  if (preferences.getInt("r3_mode", -1) == -1) configs[3].mode = MODE_TEMPERATURE;
  
  Serial.println("[INFO] Configurações carregadas!");
}

void saveConfig() {
  Serial.println("[INFO] Salvando configurações dos 8 relés...");
  
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
  
  Serial.println("[INFO] Configurações salvas!");
}

// ----------------------------- pH HISTORY --------------------------------------------
void logHourlyPH(float currentPH) {
  ph_history[ph_history_index] = currentPH;
  ph_history_index = (ph_history_index + 1) % 24;
  Serial.printf("[PH] Valor %.2f logado no histórico\n", currentPH);
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
