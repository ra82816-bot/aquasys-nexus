/*
 * =============================================================================
 * HYDROSMART - MÓDULO DE SENSORES v1.5 SIMPLE
 * =============================================================================
 * ESP32-S3 DevKit | Firmware simplificado sem QR Code
 * 
 * Sensores:
 * - pH (ADC1, pino 1) - Calibração dois pontos (pH 4.0 e pH 7.0)
 * - EC/TDS (ADC1, pino 2) - Interpolação linear com compensação de temperatura
 * - DHT22 (pino 15) - Temperatura e umidade do ar
 * - DS18B20 (pino 4) - Temperatura da água
 * 
 * Comunicação:
 * - WiFi com modo AP para configuração
 * - MQTT via HiveMQ Cloud (TLS)
 * - Tópico publicação: aquasys/sensors/all
 * - Tópico calibração: aquasys/sensors/calibrate
 * 
 * Display:
 * - OLED SSD1306 128x64 I2C
 * - Ciclo automático de telas (sensores + identidade)
 * - UUID e Token exibidos como texto (sem QR Code)
 * 
 * Versão: 1.5 SIMPLE
 * Data: Dezembro 2024
 * =============================================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// =============================================================================
// CONFIGURAÇÃO DE HARDWARE
// =============================================================================

// Pinos dos sensores
#define PH_SENSOR_PIN     1   // ADC1 Canal 0 (GPIO 1 no ESP32-S3)
#define EC_SENSOR_PIN     2   // ADC1 Canal 1 (GPIO 2 no ESP32-S3)
#define DHT_PIN           15
#define DHT_TYPE          DHT22
#define ONE_WIRE_BUS      4

// Configuração OLED
#define OLED_WIDTH        128
#define OLED_HEIGHT       64
#define OLED_RESET        -1
#define OLED_ADDRESS      0x3C

// Botões (opcional - para navegação manual)
#define BUTTON_UP         32
#define BUTTON_DOWN       33
#define BUTTON_SELECT     25
#define BUTTON_BACK       26

// =============================================================================
// CONFIGURAÇÃO MQTT
// =============================================================================

#define MQTT_BROKER       "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud"
#define MQTT_PORT         8883
#define MQTT_USERNAME     "esp32-user"
#define MQTT_PASSWORD     "HydroSmart123"
#define MQTT_TOPIC_SENSORS    "aquasys/sensors/all"
#define MQTT_TOPIC_CALIBRATE  "aquasys/sensors/calibrate"

// =============================================================================
// OBJETOS GLOBAIS
// =============================================================================

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Preferences preferences;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// =============================================================================
// VARIÁVEIS DE CALIBRAÇÃO pH (Dois pontos - padrão industrial)
// =============================================================================

float cal_ph7_voltage = 2.52;   // Tensão medida em solução pH 7.0
float cal_ph4_voltage = 3.29;   // Tensão medida em solução pH 4.0
float ph_slope, ph_intercept;   // Coeficientes calculados

// =============================================================================
// VARIÁVEIS DE CALIBRAÇÃO EC
// =============================================================================

float calibration_low_raw = 645.0;
float calibration_high_raw = 2850.0;
float calibration_low_ec = 360.0;
float calibration_high_ec = 4588.0;

// =============================================================================
// VARIÁVEIS DE SENSORES
// =============================================================================

float temperature_C = 25.0;
float humidity = 0.0;
float water_temperature_C = 0.0;
float current_ph = 0.0;
float current_ec = 0.0;
float current_ph_voltage = 0.0;

// =============================================================================
// VARIÁVEIS DE REDE
// =============================================================================

String ssid_sta = "";
String password_sta = "";
bool wifiConfigured = false;

// =============================================================================
// IDENTIDADE DO DISPOSITIVO
// =============================================================================

String device_uuid = "";
String claim_token = "";

// =============================================================================
// CONTROLE DE TEMPO
// =============================================================================

unsigned long lastMQTTPublish = 0;
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastDisplayCycle = 0;
unsigned long lastWiFiCheck = 0;

const unsigned long MQTT_INTERVAL = 30000;      // Publicar MQTT a cada 30s
const unsigned long SENSOR_INTERVAL = 2000;     // Ler sensores a cada 2s
const unsigned long DISPLAY_INTERVAL = 1000;    // Atualizar display a cada 1s
const unsigned long DISPLAY_CYCLE = 5000;       // Ciclar telas a cada 5s
const unsigned long WIFI_CHECK_INTERVAL = 10000;// Verificar WiFi a cada 10s

// Tela atual (0 = sensores, 1 = identidade)
int currentScreen = 0;

// =============================================================================
// PROTÓTIPOS DE FUNÇÕES
// =============================================================================

void generateDeviceIdentity();
void loadCalibration();
void saveCalibration();
void calculatePHCoefficients();
float readAverageADC(int pin, int samples = 10);
float voltageToPH(float voltage);
float interpolateEC(float rawValue);
float temperatureCompensateEC(float ec, float temp);
void readSensors();
void updateDisplay();
void drawSensorScreen();
void drawIdentityScreen();
void connectToWiFi();
void startAPMode();
void setupWebServer();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData();
void displayMessage(const char* message);

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("HydroSmart Sensor Module v1.5 SIMPLE");
  Serial.println("========================================");

  // Inicializar OLED
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERRO] Falha ao inicializar OLED!");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    displayMessage("HydroSmart v1.5\nIniciando...");
  }

  // Inicializar sensores
  dht.begin();
  ds18b20.begin();
  Serial.println("[OK] Sensores inicializados");

  // Carregar calibrações
  preferences.begin("sensor_calib", false);
  loadCalibration();
  calculatePHCoefficients();
  
  // Gerar/carregar identidade do dispositivo
  generateDeviceIdentity();

  // Configurar WiFi
  preferences.begin("wifi", false);
  ssid_sta = preferences.getString("ssid", "");
  password_sta = preferences.getString("password", "");
  
  if (ssid_sta.length() > 0) {
    displayMessage("Conectando WiFi...");
    connectToWiFi();
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    startAPMode();
  } else {
    wifiConfigured = true;
    String msg = "WiFi OK\nIP: " + WiFi.localIP().toString();
    displayMessage(msg.c_str());
    delay(2000);
  }

  // Configurar servidor web
  setupWebServer();

  // Configurar MQTT
  espClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);

  Serial.println("[OK] Setup completo!");
  displayMessage("Setup completo!");
  delay(1000);
}

// =============================================================================
// LOOP PRINCIPAL
// =============================================================================

void loop() {
  unsigned long now = millis();

  // Verificar conexão WiFi
  if (now - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = now;
    if (WiFi.status() != WL_CONNECTED && ssid_sta.length() > 0) {
      Serial.println("[WIFI] Reconectando...");
      connectToWiFi();
    }
  }

  // Se WiFi conectado
  if (WiFi.status() == WL_CONNECTED) {
    wifiConfigured = true;
    
    // Manter conexão MQTT
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();

    // Publicar dados periodicamente
    if (now - lastMQTTPublish > MQTT_INTERVAL) {
      lastMQTTPublish = now;
      publishSensorData();
    }
  } else {
    // Modo AP - atender requisições web
    server.handleClient();
  }

  // Ler sensores periodicamente
  if (now - lastSensorRead > SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }

  // Atualizar display
  if (now - lastDisplayUpdate > DISPLAY_INTERVAL) {
    lastDisplayUpdate = now;
    updateDisplay();
  }

  // Ciclar entre telas
  if (now - lastDisplayCycle > DISPLAY_CYCLE) {
    lastDisplayCycle = now;
    currentScreen = (currentScreen + 1) % 2;
  }

  delay(10); // Pequeno yield
}

// =============================================================================
// IDENTIDADE DO DISPOSITIVO
// =============================================================================

void generateDeviceIdentity() {
  preferences.begin("device_id", false);
  
  device_uuid = preferences.getString("uuid", "");
  claim_token = preferences.getString("token", "");
  
  if (device_uuid.length() == 0) {
    // Gerar UUID baseado no chip ID
    uint64_t chipId = ESP.getEfuseMac();
    char uuidStr[37];
    snprintf(uuidStr, sizeof(uuidStr), 
             "%04X%04X-%04X-%04X-%04X-%04X%04X%04X",
             (uint16_t)(chipId >> 48),
             (uint16_t)(chipId >> 32),
             (uint16_t)(chipId >> 16),
             (uint16_t)(chipId),
             (uint16_t)(random(0xFFFF)),
             (uint16_t)(random(0xFFFF)),
             (uint16_t)(random(0xFFFF)),
             (uint16_t)(random(0xFFFF)));
    device_uuid = String(uuidStr);
    preferences.putString("uuid", device_uuid);
    Serial.println("[ID] UUID gerado: " + device_uuid);
  }
  
  if (claim_token.length() == 0) {
    // Gerar token de 6 dígitos
    char tokenStr[7];
    snprintf(tokenStr, sizeof(tokenStr), "%06d", random(100000, 999999));
    claim_token = String(tokenStr);
    preferences.putString("token", claim_token);
    Serial.println("[ID] Token gerado: " + claim_token);
  }
  
  Serial.println("[ID] Device UUID: " + device_uuid);
  Serial.println("[ID] Claim Token: " + claim_token);
}

// =============================================================================
// CALIBRAÇÃO pH - DOIS PONTOS (pH 4.0 e pH 7.0)
// =============================================================================

void calculatePHCoefficients() {
  float pH_low = 4.0;
  float pH_neutral = 7.0;
  
  // Calcular slope e intercept para regressão linear
  // pH = slope * voltage + intercept
  ph_slope = (pH_neutral - pH_low) / (cal_ph7_voltage - cal_ph4_voltage);
  ph_intercept = pH_neutral - ph_slope * cal_ph7_voltage;
  
  Serial.println("[CAL] Coeficientes pH calculados:");
  Serial.print("  Slope: "); Serial.println(ph_slope, 4);
  Serial.print("  Intercept: "); Serial.println(ph_intercept, 4);
}

float voltageToPH(float voltage) {
  float ph = ph_slope * voltage + ph_intercept;
  
  // Limitar a faixa válida
  if (ph < 0) ph = 0;
  if (ph > 14) ph = 14;
  
  return ph;
}

void loadCalibration() {
  cal_ph7_voltage = preferences.getFloat("cal_ph7_v", 2.52);
  cal_ph4_voltage = preferences.getFloat("cal_ph4_v", 3.29);
  calibration_low_raw = preferences.getFloat("ec_low_raw", 645.0);
  calibration_high_raw = preferences.getFloat("ec_high_raw", 2850.0);
  calibration_low_ec = preferences.getFloat("ec_low_ec", 360.0);
  calibration_high_ec = preferences.getFloat("ec_high_ec", 4588.0);
  
  Serial.println("[CAL] Calibração carregada:");
  Serial.print("  pH7 Voltage: "); Serial.println(cal_ph7_voltage, 3);
  Serial.print("  pH4 Voltage: "); Serial.println(cal_ph4_voltage, 3);
}

void saveCalibration() {
  preferences.putFloat("cal_ph7_v", cal_ph7_voltage);
  preferences.putFloat("cal_ph4_v", cal_ph4_voltage);
  preferences.putFloat("ec_low_raw", calibration_low_raw);
  preferences.putFloat("ec_high_raw", calibration_high_raw);
  preferences.putFloat("ec_low_ec", calibration_low_ec);
  preferences.putFloat("ec_high_ec", calibration_high_ec);
  
  Serial.println("[CAL] Calibração salva!");
}

// =============================================================================
// LEITURA EC/TDS
// =============================================================================

float interpolateEC(float rawValue) {
  float slope = (calibration_high_ec - calibration_low_ec) / 
                (calibration_high_raw - calibration_low_raw);
  float ecValue = calibration_low_ec + slope * (rawValue - calibration_low_raw);
  
  if (ecValue < 0) ecValue = 0;
  return ecValue;
}

float temperatureCompensateEC(float ec, float temp) {
  // Compensação padrão: 2% por grau Celsius
  return ec / (1 + 0.02 * (temp - 25.0));
}

// =============================================================================
// LEITURA DE SENSORES
// =============================================================================

float readAverageADC(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return (float)sum / (float)samples;
}

void readSensors() {
  // Ler pH
  float phRaw = readAverageADC(PH_SENSOR_PIN);
  current_ph_voltage = phRaw * 3.3 / 4095.0;
  current_ph = voltageToPH(current_ph_voltage);
  
  // Ler EC
  float ecRaw = readAverageADC(EC_SENSOR_PIN);
  float ecValue = interpolateEC(ecRaw);
  current_ec = temperatureCompensateEC(ecValue, temperature_C);
  
  // Ler DHT22
  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();
  if (!isnan(newTemp)) temperature_C = newTemp;
  if (!isnan(newHum)) humidity = newHum;
  
  // Ler DS18B20
  ds18b20.requestTemperatures();
  float waterTemp = ds18b20.getTempCByIndex(0);
  if (waterTemp != DEVICE_DISCONNECTED_C) {
    water_temperature_C = waterTemp;
  }

  // Log serial
  Serial.printf("[SENSOR] pH: %.2f (V: %.3f) | EC: %.0f uS/cm | Ar: %.1fC | Hum: %.1f%% | Agua: %.1fC\n",
                current_ph, current_ph_voltage, current_ec, 
                temperature_C, humidity, water_temperature_C);
}

// =============================================================================
// DISPLAY OLED
// =============================================================================

void updateDisplay() {
  if (currentScreen == 0) {
    drawSensorScreen();
  } else {
    drawIdentityScreen();
  }
}

void drawSensorScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Cabeçalho
  display.setCursor(0, 0);
  display.print("HydroSmart");
  
  // Indicador WiFi
  display.setCursor(100, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi");
  } else {
    display.print("X");
  }
  
  // Linha separadora
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  // Dados dos sensores
  display.setCursor(0, 14);
  display.print("pH:   ");
  display.print(current_ph, 2);
  display.print(" (");
  display.print(current_ph_voltage, 2);
  display.print("V)");
  
  display.setCursor(0, 24);
  display.print("EC:   ");
  display.print(current_ec, 0);
  display.print(" uS/cm");
  
  display.setCursor(0, 34);
  display.print("Ar:   ");
  display.print(temperature_C, 1);
  display.print("C  Hum:");
  display.print(humidity, 0);
  display.print("%");
  
  display.setCursor(0, 44);
  display.print("Agua: ");
  display.print(water_temperature_C, 1);
  display.print("C");
  
  // Indicador de tela
  display.setCursor(110, 56);
  display.print("1/2");
  
  display.display();
}

void drawIdentityScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Cabeçalho
  display.setCursor(0, 0);
  display.print("Device Identity");
  
  // Linha separadora
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  // UUID (dividido em duas linhas)
  display.setCursor(0, 14);
  display.print("UUID:");
  display.setCursor(0, 24);
  display.print(device_uuid.substring(0, 18));
  display.setCursor(0, 34);
  display.print(device_uuid.substring(18));
  
  // Token
  display.setCursor(0, 48);
  display.print("Token: ");
  display.setTextSize(2);
  display.print(claim_token);
  display.setTextSize(1);
  
  // Indicador de tela
  display.setCursor(110, 56);
  display.print("2/2");
  
  display.display();
}

void displayMessage(const char* message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}

// =============================================================================
// CONEXÃO WIFI
// =============================================================================

void connectToWiFi() {
  Serial.println("[WIFI] Conectando: " + ssid_sta);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(200);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Conectado! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WIFI] Falha na conexão");
  }
}

void startAPMode() {
  Serial.println("[AP] Iniciando Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("HydroSmart_Sensor");
  Serial.println("[AP] SSID: HydroSmart_Sensor");
  Serial.println("[AP] IP: " + WiFi.softAPIP().toString());
  displayMessage("Modo AP\nRede: HydroSmart_Sensor\nIP: 192.168.4.1");
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><title>HydroSmart Config</title></head><body>";
    html += "<h1>Configurar WiFi</h1>";
    html += "<form action='/save' method='POST'>";
    html += "SSID: <input type='text' name='ssid' value='" + ssid_sta + "'><br><br>";
    html += "Senha: <input type='password' name='password'><br><br>";
    html += "<input type='submit' value='Salvar'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
      ssid_sta = server.arg("ssid");
      password_sta = server.arg("password");
      preferences.putString("ssid", ssid_sta);
      preferences.putString("password", password_sta);
      wifiConfigured = false;
      
      String resp = "<!DOCTYPE html><html><body>";
      resp += "<h1>Salvo! Reiniciando...</h1>";
      resp += "</body></html>";
      server.send(200, "text/html", resp);
      
      Serial.println("[WEB] WiFi configurado: " + ssid_sta);
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Requisição inválida");
    }
  });
  
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });
  
  server.begin();
  Serial.println("[WEB] Servidor iniciado");
}

// =============================================================================
// MQTT
// =============================================================================

void reconnectMQTT() {
  if (mqttClient.connected()) return;
  
  Serial.print("[MQTT] Conectando...");
  String clientId = "HydroSmart_Sensor_" + device_uuid.substring(0, 8);
  
  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("Conectado!");
    
    // Inscrever no tópico de calibração
    mqttClient.subscribe(MQTT_TOPIC_CALIBRATE);
    Serial.println("[MQTT] Inscrito em: " + String(MQTT_TOPIC_CALIBRATE));
  } else {
    Serial.print("Falha, rc=");
    Serial.println(mqttClient.state());
  }
}

void publishCalibrationResponse(const char* sensor, const char* point, bool success, float value) {
  if (!mqttClient.connected()) {
    Serial.println("[CAL] ✗ MQTT desconectado - resposta não enviada!");
    return;
  }
  
  StaticJsonDocument<256> doc;
  doc["sensor"] = sensor;
  doc["point"] = point;
  doc["success"] = success;
  doc["value"] = value;
  doc["timestamp"] = millis();
  doc["uuid"] = device_uuid;
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  Serial.printf("[CAL] Publicando resposta: %s\n", buffer);
  Serial.println("[CAL] Tópico: aquasys/sensors/calibrate/response");
  
  // Publicar confirmação no tópico de resposta (usando char* simples)
  bool published = mqttClient.publish("aquasys/sensors/calibrate/response", buffer);
  
  if (published) {
    Serial.println("[CAL] ✓ Resposta enviada com sucesso!");
  } else {
    Serial.println("[CAL] ✗ FALHA ao enviar resposta!");
    Serial.printf("[CAL] Estado MQTT: %d\n", mqttClient.state());
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println("[MQTT] Recebido em " + String(topic) + ": " + message);
  
  // Processar comando de calibração
  if (String(topic) == MQTT_TOPIC_CALIBRATE) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
      Serial.println("[MQTT] Erro ao parsear JSON - calibração ignorada");
      return;
    }
    
    String sensor = doc["sensor"] | "";
    String action = doc["action"] | "";
    
    Serial.printf("[CAL] Comando recebido: sensor=%s, action=%s\n", sensor.c_str(), action.c_str());
    
    if (sensor == "ph" && action == "calibrate") {
      String point = doc["point"] | "";
      Serial.printf("[CAL] Calibrando pH no ponto: %s\n", point.c_str());
      
      // Ler tensão atual do sensor (média de 20 amostras para precisão)
      float currentVoltage = readAverageADC(PH_SENSOR_PIN, 20) * 3.3 / 4095.0;
      bool calibrated = false;
      
      if (point == "7.00" || point == "7.0" || point == "7") {
        cal_ph7_voltage = currentVoltage;
        Serial.printf("[CAL] ✓ pH 7.0 calibrado: %.4fV\n", cal_ph7_voltage);
        calibrated = true;
      } 
      else if (point == "4.01" || point == "4.0" || point == "4") {
        cal_ph4_voltage = currentVoltage;
        Serial.printf("[CAL] ✓ pH 4.0 calibrado: %.4fV\n", cal_ph4_voltage);
        calibrated = true;
      }
      else {
        Serial.printf("[CAL] ✗ Ponto pH não reconhecido: %s\n", point.c_str());
      }
      
      if (calibrated) {
        // Recalcular coeficientes e salvar
        calculatePHCoefficients();
        saveCalibration();
        publishCalibrationResponse("ph", point.c_str(), true, currentVoltage);
      } else {
        publishCalibrationResponse("ph", point.c_str(), false, 0);
      }
    }
    else if (sensor == "ec" && action == "calibrate") {
      float standardUs = doc["standard_us"] | 0.0;
      Serial.printf("[CAL] Calibrando EC com padrão: %.0f uS/cm\n", standardUs);
      
      if (standardUs > 0) {
        float currentRaw = readAverageADC(EC_SENSOR_PIN, 20);
        // Atualizar ponto de calibração alto
        calibration_high_raw = currentRaw;
        calibration_high_ec = standardUs;
        saveCalibration();
        Serial.printf("[CAL] ✓ EC calibrado: Raw=%.0f para %.0f uS/cm\n", currentRaw, standardUs);
        publishCalibrationResponse("ec", String(standardUs).c_str(), true, currentRaw);
      } else {
        Serial.println("[CAL] ✗ Valor EC inválido");
        publishCalibrationResponse("ec", "0", false, 0);
      }
    }
    else {
      Serial.printf("[CAL] Sensor ou ação desconhecidos: %s / %s\n", sensor.c_str(), action.c_str());
    }
  }
}

void publishSensorData() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  doc["ph"] = current_ph;
  doc["ec"] = current_ec;
  doc["airTemp"] = temperature_C;
  doc["humidity"] = humidity;
  doc["waterTemp"] = water_temperature_C;
  doc["uptime_ms"] = millis();
  doc["uuid"] = device_uuid;
  doc["token"] = claim_token;
  
  char buffer[512];
  size_t n = serializeJson(doc, buffer);
  
  if (mqttClient.publish(MQTT_TOPIC_SENSORS, buffer, n)) {
    Serial.println("[MQTT] Dados publicados");
  } else {
    Serial.println("[MQTT] Falha ao publicar");
  }
}
