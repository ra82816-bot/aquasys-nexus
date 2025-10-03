# Integração MQTT com HiveMQ Cloud - Webhooks

**✨ NOVA ARQUITETURA - SEM SERVIDOR EXTERNO**

Este projeto agora usa **Webhooks do HiveMQ** para enviar dados diretamente para o backend, eliminando a necessidade de rodar um script Python separado.

## 🎯 Como Funciona

### Fluxo de Dados (Sensores → Backend)
1. ESP32 publica dados nos tópicos MQTT
2. HiveMQ detecta a mensagem e dispara webhook
3. Webhook chama diretamente a Edge Function
4. Dados são salvos no banco automaticamente

### Fluxo de Comandos (Backend → ESP32)
1. Usuário aciona relé no dashboard
2. Comando é salvo na tabela `relay_commands`
3. ESP32 faz polling periódico (a cada 5-10s)
4. ESP32 lê comandos pendentes e executa

## 📋 Configuração do HiveMQ Cloud

### 1. Acesse o Console do HiveMQ
- Acesse: https://console.hivemq.cloud
- Faça login na sua conta
- Selecione seu cluster

### 2. Configure Webhooks

Vá em **Integrations → Webhooks** e crie **2 webhooks**:

#### Webhook 1: Sensores
```
Name: AquaSys Sensors
Topic Filter: aquasys/sensors/all
URL: https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector
Method: POST
Headers:
  Content-Type: application/json

Body Template (JSON):
{
  "action": "process_sensors",
  "data": {
    "ph": {{ph}},
    "ec": {{ec}},
    "airTemp": {{airTemp}},
    "humidity": {{humidity}},
    "waterTemp": {{waterTemp}}
  }
}
```

#### Webhook 2: Status dos Relés
```
Name: AquaSys Relay Status
Topic Filter: aquasys/relay/status
URL: https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector
Method: POST
Headers:
  Content-Type: application/json

Body Template (JSON):
{
  "action": "process_relay_status",
  "data": {
    "relay1_led": {{relay1_led}},
    "relay2_pump": {{relay2_pump}},
    "relay3_ph_up": {{relay3_ph_up}},
    "relay4_fan": {{relay4_fan}},
    "relay5_humidity": {{relay5_humidity}},
    "relay6_ec": {{relay6_ec}},
    "relay7_co2": {{relay7_co2}},
    "relay8_generic": {{relay8_generic}}
  }
}
```

### 3. Teste os Webhooks

Publique uma mensagem de teste no HiveMQ:

**Tópico:** `aquasys/sensors/all`
**Payload:**
```json
{
  "ph": 6.5,
  "ec": 1200,
  "airTemp": 25,
  "humidity": 65,
  "waterTemp": 23
}
```

Você deve ver os dados aparecerem no dashboard imediatamente!

## 🔧 Implementação no ESP32

### Polling de Comandos

O ESP32 deve verificar periodicamente a tabela `relay_commands` para executar comandos manuais:

```cpp
// A cada 5-10 segundos
void checkRelayCommands() {
  HTTPClient http;
  http.begin("https://oaabtbvwxsjomeeizciq.supabase.co/rest/v1/relay_commands?executed=eq.false&select=*");
  http.addHeader("apikey", "sua-anon-key");
  http.addHeader("Authorization", "Bearer sua-anon-key");
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    // Parse JSON e execute comandos
    // Marque como executed=true após executar
  }
  
  http.end();
}
```

### Publicação de Dados

Continue publicando normalmente nos tópicos MQTT - os webhooks cuidam do resto:

```cpp
// Publicar sensores
StaticJsonDocument<200> doc;
doc["ph"] = readPH();
doc["ec"] = readEC();
doc["airTemp"] = readAirTemp();
doc["humidity"] = readHumidity();
doc["waterTemp"] = readWaterTemp();

String json;
serializeJson(doc, json);
mqttClient.publish("aquasys/sensors/all", json.c_str());

// Publicar status dos relés
StaticJsonDocument<200> relayDoc;
relayDoc["relay1_led"] = digitalRead(RELAY1_PIN);
relayDoc["relay2_pump"] = digitalRead(RELAY2_PIN);
// ... outros relés

String relayJson;
serializeJson(relayDoc, relayJson);
mqttClient.publish("aquasys/relay/status", relayJson.c_str());
```

## 📊 Formato das Mensagens

### Sensores (aquasys/sensors/all):
```json
{
  "ph": 6.5,
  "ec": 1200,
  "airTemp": 25,
  "humidity": 65,
  "waterTemp": 23
}
```

### Status dos Relés (aquasys/relay/status):
```json
{
  "relay1_led": true,
  "relay2_pump": false,
  "relay3_ph_up": true,
  "relay4_fan": false,
  "relay5_humidity": false,
  "relay6_ec": false,
  "relay7_co2": false,
  "relay8_generic": false
}
```

## ✅ Vantagens desta Arquitetura

- ✨ **Sem servidor externo** - não precisa manter script Python rodando
- 🚀 **Latência baixíssima** - dados chegam instantaneamente
- 💰 **Custo zero** - HiveMQ e Supabase têm planos gratuitos
- 🔄 **Auto-scaling** - Edge Functions escalam automaticamente
- 🛡️ **Mais confiável** - sem ponto único de falha

## 🗑️ Arquivos Legados

Os arquivos `bridge.py` e `requirements.txt` foram mantidos apenas como referência da arquitetura anterior. Eles **não são mais necessários** para o funcionamento do sistema.
