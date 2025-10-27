# Prompt Detalhado: Melhorias ESP32 - HydroSmart System

## 🎯 Objetivo
Implementar melhorias nos firmwares ESP32 (módulo sensores e módulo atuadores) para garantir comunicação MQTT robusta, autenticação segura, e funcionalidades avançadas de gerenciamento remoto.

---

## 📡 MÓDULO SENSORES - Melhorias Necessárias

### 1. Configuração MQTT (Prioridade ALTA)
**Credenciais Padrão:**
```cpp
const char* mqtt_broker = "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud";
const int mqtt_port = 8884;
const char* mqtt_user = "esp32-user";
const char* mqtt_password = "HydroSmart123";
```

**Tópicos MQTT:**
```cpp
const char* TOPIC_SENSORS = "aquasys/sensors/all";
const char* TOPIC_HEARTBEAT = "aquasys/heartbeat";
const char* TOPIC_CALIBRATION = "aquasys/calibration/request";
const char* TOPIC_OTA = "aquasys/ota/request";
```

**Certificado TLS 1.3 (ISRG Root X1):**
- Incluir certificado completo para conexão segura HiveMQ Cloud
- Usar `WiFiClientSecure` com `setCACert()`

### 2. UUID Único por Dispositivo
```cpp
void generateDeviceUUID() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  
  char uuid[20];
  sprintf(uuid, "HYDRO-%02X%02X-%02X%02X-%02X%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  deviceUUID = String(uuid);
  mqttClientId = "sensor_" + deviceUUID;
}
```

**Uso:**
- Gerar UUID baseado no MAC address
- Incluir em todas mensagens MQTT (heartbeat, sensores)
- Permitir identificação única no backend

### 3. Heartbeat Diagnóstico Aprimorado
**Publicar a cada 30 segundos:**
```json
{
  "device": "ESP32_Sensor_HYDRO-XXXX-XXXX-XXXX",
  "device_uuid": "HYDRO-XXXX-XXXX-XXXX",
  "firmware": "3.2-ENHANCED",
  "uptime": 12345,
  "wifi": {
    "ssid": "MyNetwork",
    "rssi": -65,
    "ip": "192.168.1.100",
    "reconnects": 2
  },
  "mqtt": {
    "connected": true,
    "failed_attempts": 0
  },
  "sensors": {
    "ph_valid": true,
    "ec_valid": true,
    "temp_valid": true,
    "humidity_valid": true,
    "water_temp_valid": true
  },
  "memory": {
    "free_heap": 234567,
    "min_free_heap": 220000
  }
}
```

### 4. BLE Server (Transmissão de Dados)
**UUID BLE:**
```cpp
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
```

**Estrutura de Dados BLE:**
```cpp
struct BLEData {
  float ph;
  float waterTemp;
  float ec;
  uint32_t timestamp;
  uint8_t checksum;  // XOR de todos os bytes
} __attribute__((packed));
```

**Comportamento:**
- Ativar BLE apenas se MQTT falhar por > 3 minutos
- Transmitir dados de sensores via BLE Characteristic
- Desativar BLE quando MQTT reconectar

### 5. Calibração Remota via MQTT
**Escutar tópico:** `aquasys/calibration/request`

**Comando recebido:**
```json
{
  "device_uuid": "HYDRO-XXXX-XXXX-XXXX",
  "sensor": "ph",  // ou "ec"
  "action": "calibrate_7",  // ou "calibrate_4", "calibrate_10", "calibrate_1413"
  "expected_value": 7.0
}
```

**Resposta:** Publicar confirmação e novos valores calibrados

### 6. OTA Updates via MQTT
**Escutar tópico:** `aquasys/ota/request`

**Comando recebido:**
```json
{
  "device_uuid": "HYDRO-XXXX-XXXX-XXXX",
  "firmware_url": "https://example.com/firmware.bin",
  "version": "3.3.0"
}
```

**Implementar:**
- Download via HTTPS
- Verificação de integridade (hash MD5)
- Instalação e reboot automático

### 7. Validação de Dados
**Antes de publicar via MQTT:**
```cpp
bool validateSensorData() {
  if (ph < 0 || ph > 14) return false;
  if (ec < 0 || ec > 5000) return false;
  if (airTemp < -40 || airTemp > 80) return false;
  if (humidity < 0 || humidity > 100) return false;
  if (waterTemp < -10 || waterTemp > 50) return false;
  return true;
}
```

### 8. Watchdog Timer
```cpp
esp_task_wdt_init(60, true);  // 60 segundos timeout
esp_task_wdt_add(NULL);
// No loop:
esp_task_wdt_reset();
```

---

## 🔌 MÓDULO ATUADORES - Melhorias Necessárias

### 1. Configuração MQTT (Prioridade ALTA)
**Mesmas credenciais do módulo sensores**

**Tópicos MQTT:**
```cpp
const char* TOPIC_RELAY_STATUS = "aquasys/relay/status";
const char* TOPIC_RELAY_COMMAND = "aquasys/relay/command";
const char* TOPIC_HEARTBEAT = "aquasys/heartbeat";
const char* TOPIC_SENSORS = "aquasys/sensors/all";  // subscrever para lógica automática
const char* TOPIC_CALIBRATION = "aquasys/calibration/request";
const char* TOPIC_OTA = "aquasys/ota/request";
```

### 2. UUID Único por Dispositivo
**Mesmo sistema do módulo sensores:**
```cpp
mqttClientId = "actuator_" + deviceUUID;
```

### 3. Heartbeat Diagnóstico
**Publicar a cada 30 segundos:**
```json
{
  "device": "ESP32_Actuator_HYDRO-XXXX-XXXX-XXXX",
  "device_uuid": "HYDRO-XXXX-XXXX-XXXX",
  "firmware": "4.2-BLE",
  "uptime": 12345,
  "wifi": {
    "ssid": "MyNetwork",
    "rssi": -65,
    "ip": "192.168.1.100",
    "reconnects": 2
  },
  "mqtt": {
    "connected": true,
    "failed_attempts": 0
  },
  "ble": {
    "active": true,
    "connected": true,
    "sensor_from_ble": false
  },
  "relays": {
    "relay1": false,
    "relay2": false,
    "relay3": false,
    "relay4": false,
    "relay5": false,
    "relay6": false,
    "relay7": false,
    "relay8": false
  },
  "memory": {
    "free_heap": 234567,
    "min_free_heap": 220000
  }
}
```

### 4. BLE Client (Recepção de Dados de Sensores)
**Comportamento:**
- Ativar BLE Client se MQTT offline > 3 minutos
- Escanear dispositivos BLE com SERVICE_UUID específico
- Conectar ao sensor BLE e ler dados
- Usar dados BLE para lógica automática de relés
- Desativar BLE quando MQTT reconectar

**Implementação:**
```cpp
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      sensorBLEAddress = advertisedDevice.getAddress().toString().c_str();
      advertisedDevice.getScan()->stop();
    }
  }
};
```

### 5. Controle de Relés via MQTT
**Comando recebido no tópico** `aquasys/relay/command`:
```json
{
  "relay": 0,      // índice 0-7
  "state": true    // ou false
}
```

**Resposta:** Publicar status atualizado em `aquasys/relay/status`

### 6. Lógica Automática de Relés
**Baseada em dados de sensores (MQTT ou BLE):**

```cpp
void handleRelayLogic() {
  // Dados devem ter < 2 minutos
  if (millis() - sensorData.lastUpdate < 120000) {
    
    // Exemplo: pH Control
    if (sensorData.ph > 0 && sensorData.ph < 6.0) {
      digitalWrite(RELAY_PINS[2], HIGH);  // pH UP
      relayStates[2] = true;
    } else if (sensorData.ph > 7.0) {
      digitalWrite(RELAY_PINS[2], LOW);
      relayStates[2] = false;
    }
    
    // Exemplo: Temperature Control
    if (sensorData.airTemp > 28.0) {
      digitalWrite(RELAY_PINS[3], HIGH);  // Fan ON
      relayStates[3] = true;
    } else if (sensorData.airTemp < 24.0) {
      digitalWrite(RELAY_PINS[3], LOW);
      relayStates[3] = false;
    }
    
    // Adicionar lógica para EC, umidade, etc.
  }
}
```

### 7. OTA Updates
**Mesma implementação do módulo sensores**

### 8. Watchdog Timer
**Mesma implementação do módulo sensores**

---

## 🔐 SEGURANÇA E AUTENTICAÇÃO

### 1. Autenticação do Dispositivo
**Backend valida UUID antes de aceitar dados:**
- Verificar se `device_uuid` existe na tabela `devices`
- Verificar hash de senha MQTT armazenado
- Bloquear dispositivos não autorizados

### 2. Rate Limiting
**Backend implementa:**
- Máximo 100 requisições/minuto por dispositivo
- Bloqueio de 5 minutos se exceder
- Limpeza automática de registros antigos (cron job)

### 3. TLS 1.3 Obrigatório
**Todos os dispositivos devem:**
- Usar conexão TLS 1.3 com HiveMQ Cloud
- Validar certificado do broker
- Nunca transmitir dados sem criptografia

---

## 📊 QUALIDADE DE CÓDIGO

### 1. Estrutura Modular
```cpp
// ==================== PROTÓTIPOS ====================
void generateDeviceUUID();
void connectWiFi();
void setupMQTT();
void reconnectMQTT();
void publishHeartbeat();
void publishSensorData();
void handleRelayLogic();
// etc.
```

### 2. Logs Estruturados
```cpp
void logMessage(const char* level, const char* msg) {
  unsigned long s = millis() / 1000;
  Serial.printf("[%02lu:%02lu:%02lu] %s %s\n", 
    s/3600, (s%3600)/60, s%60, level, msg);
}
```

**Níveis:** `INFO`, `WARN`, `ERROR`, `DEBUG`

### 3. Tratamento de Erros
- Try-catch em operações BLE
- Verificação de retorno em todas funções críticas
- Reconexão automática WiFi/MQTT com backoff exponencial

### 4. Otimização de Memória
- Usar `StaticJsonDocument` ao invés de `DynamicJsonDocument`
- Limpar strings não utilizadas
- Monitorar `ESP.getFreeHeap()` e `ESP.getMinFreeHeap()`

---

## 🧪 TESTES E VALIDAÇÃO

### 1. Cenários de Teste
- ✅ Conexão MQTT normal
- ✅ Falha MQTT → Ativação BLE
- ✅ Reconexão MQTT → Desativação BLE
- ✅ Calibração remota pH/EC
- ✅ OTA Update bem-sucedido
- ✅ Comandos de relé via MQTT
- ✅ Lógica automática baseada em sensores
- ✅ WiFi desconectado → Modo AP
- ✅ Watchdog Timer reset

### 2. Logs de Diagnóstico
**Incluir em todas etapas críticas:**
```cpp
logMessage("INFO", "WiFi conectado");
logMessage("WARN", "MQTT offline, ativando BLE");
logMessage("ERROR", "Falha ao ler sensor pH");
logMessage("DEBUG", "Heartbeat publicado");
```

---

## 📝 DOCUMENTAÇÃO

### 1. Comentários no Código
- Explicar lógica complexa
- Documentar estruturas de dados
- Indicar intervalos de timing
- Referenciar documentos externos

### 2. Arquivo README por Firmware
- Versão atual
- Changelog
- Instruções de upload
- Configuração de pinos
- Troubleshooting

### 3. Versionamento Semântico
- `MAJOR.MINOR.PATCH`
- Exemplo: `3.2.0`, `4.2.1`

---

## 🚀 PRIORIDADES DE IMPLEMENTAÇÃO

### Fase 1 (CRÍTICA)
1. ✅ Corrigir credenciais MQTT
2. ✅ Implementar UUID único
3. ✅ Heartbeat diagnóstico
4. ✅ TLS 1.3 configurado

### Fase 2 (ALTA)
5. ✅ BLE Server (Sensores) / BLE Client (Atuadores)
6. ✅ Calibração remota
7. ✅ OTA Updates
8. ✅ Watchdog Timer

### Fase 3 (MÉDIA)
9. ✅ Lógica automática avançada de relés
10. ✅ Validação de dados robusta
11. ✅ Rate limiting (backend)

### Fase 4 (BAIXA - Futuro)
12. 🔄 Interface web para configuração
13. 🔄 Logs persistentes em SD card
14. 🔄 Suporte a múltiplos sensores

---

## 📋 CHECKLIST FINAL

### Módulo Sensores
- [ ] Credenciais MQTT corretas
- [ ] UUID único implementado
- [ ] Heartbeat publicando corretamente
- [ ] TLS 1.3 funcionando
- [ ] BLE Server ativando quando MQTT offline
- [ ] Calibração remota respondendo
- [ ] OTA Updates funcionando
- [ ] Watchdog Timer resetando
- [ ] Validação de dados implementada
- [ ] Logs estruturados em todos os pontos críticos

### Módulo Atuadores
- [ ] Credenciais MQTT corretas
- [ ] UUID único implementado
- [ ] Heartbeat publicando corretamente
- [ ] TLS 1.3 funcionando
- [ ] BLE Client conectando ao sensor
- [ ] Comandos de relé via MQTT funcionando
- [ ] Lógica automática baseada em sensores
- [ ] Status de relés publicando corretamente
- [ ] OTA Updates funcionando
- [ ] Watchdog Timer resetando

---

## 🔗 INTEGRAÇÃO COM BACKEND

### Edge Functions Necessárias
1. ✅ `mqtt-collector` - Recebe dados MQTT e salva no Supabase
2. ✅ `mqtt-ping` - Monitora heartbeats
3. ✅ `device-ota` - Gerencia OTA updates
4. ✅ `device-calibration` - Processa calibrações remotas
5. ✅ `relay-control` - Envia comandos para relés

### Tabelas Supabase
1. ✅ `devices` - Registro de dispositivos
2. ✅ `device_owners` - Vínculo dispositivo-usuário
3. ✅ `device_health` - Heartbeats e diagnósticos
4. ✅ `readings` - Leituras de sensores
5. ✅ `relay_status` - Estados dos relés
6. ✅ `device_commands` - Comandos OTA/calibração
7. ✅ `mqtt_rate_limits` - Controle de abuso

---

## 📞 SUPORTE E MANUTENÇÃO

### Monitoramento
- Dashboard com status de todos os dispositivos
- Alertas de MQTT offline > 5 minutos
- Gráficos de heap memory ao longo do tempo
- Logs de erros centralizados

### Troubleshooting
1. **MQTT não conecta:** Verificar credenciais, certificado TLS, firewall
2. **BLE não ativa:** Verificar tempo desde última mensagem MQTT
3. **OTA falha:** Verificar URL do firmware, conexão WiFi estável
4. **Watchdog reset:** Analisar logs para identificar operação bloqueante

---

**FIM DO PROMPT**
