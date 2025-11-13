# 💓 Guia Visual: Configurar Webhook de Heartbeat no HiveMQ Cloud

## 📍 Você está aqui
Os dados de sensores já estão funcionando, mas o **status dos dispositivos** (online/offline) não atualiza porque o webhook de heartbeat não está configurado.

## 🎯 O que este webhook faz?
- Atualiza `last_seen_at` dos dispositivos
- Popula a tabela `device_health` com métricas detalhadas
- Permite que as abas **Status** e **Gerenciar** mostrem dispositivos como online

---

## 🚀 Passo a Passo (5 minutos)

### 1️⃣ Acesse o HiveMQ Cloud Console
```
🌐 URL: https://console.hivemq.cloud/
👤 Login: Suas credenciais do HiveMQ
🎛️ Cluster: Selecione seu cluster AquaSys
```

### 2️⃣ Navegue até a Seção de Webhooks
```
Menu lateral → "Extensions" → "Webhooks"
Ou procure por "Data Hub" ou "Integrations" dependendo da versão
```

### 3️⃣ Clique em "Create Webhook" ou "Add Webhook"

### 4️⃣ Preencha os Campos EXATAMENTE Como Abaixo

#### 📝 Informações Básicas
```yaml
Nome/Name: AquaSys Heartbeat
Descrição: Envia heartbeats do ESP32 para Supabase para monitoramento de status
Status: ✅ Enabled
```

#### 🎯 Configuração do Tópico
```yaml
Topic Filter: aquasys/heartbeat
QoS: 0 ou 1 (qualquer um funciona)
```

#### 🌐 Endpoint Configuration
```yaml
Method: POST
URL: https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector
Timeout: 30000ms (30 segundos)
```

#### 🔒 Headers (CRÍTICO - Copie Exatamente)
Adicione DOIS headers:

**Header 1:**
```
Key: Content-Type
Value: application/json
```

**Header 2:**
```
Key: Authorization
Value: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw
```

⚠️ **IMPORTANTE:** O valor do Authorization deve começar com "Bearer " (com espaço)

#### 📦 Body/Payload Template
No campo de template do body, cole EXATAMENTE isto:
```json
{
  "topic": "${topic}",
  "payload": ${payload}
}
```

**Explicação:**
- `${topic}` - HiveMQ substitui pelo nome do tópico (aquasys/heartbeat)
- `${payload}` - HiveMQ substitui pelo JSON do ESP32

### 5️⃣ Salve o Webhook
Clique em **"Save"** ou **"Create"**

---

## 🧪 Teste Imediato

### Opção A: Teste Direto no Console
1. Na lista de webhooks, clique no webhook que você criou
2. Procure por "Test" ou "Test Webhook"
3. Cole este JSON de teste:
```json
{
  "device_uuid": "SEN-9454C572E11C",
  "device_type": "sensor",
  "firmware_version": "4.3.5-LITE",
  "uptime": 12345,
  "free_heap": 150000,
  "min_free_heap": 120000,
  "wifi": {
    "ssid": "MyNetwork",
    "rssi": -65,
    "reconnects": 0
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
  }
}
```
4. Clique em **"Send Test"**
5. Verifique se retorna `200 OK`

### Opção B: Aguarde o ESP32 Publicar (30 segundos)
O firmware já publica heartbeats automaticamente a cada 30 segundos.

---

## ✅ Como Saber se Funcionou?

### 1. Logs da Edge Function (Supabase)
Dentro de 30 segundos, você verá nos logs:
```
💓 Heartbeat recebido: {...}
✅ Device encontrado: SEN-9454C572E11C (ID: ...)
💾 Salvando device_health: {...}
✅ Health data salvo para SEN-9454C572E11C
✅ last_seen_at atualizado para SEN-9454C572E11C
```

### 2. Banco de Dados
Execute esta query:
```sql
SELECT 
  d.device_uuid,
  d.last_seen_at,
  dh.wifi_rssi,
  dh.mqtt_connected,
  dh.free_heap
FROM devices d
LEFT JOIN device_health dh ON dh.device_id = d.id
WHERE d.device_uuid = 'SEN-9454C572E11C'
ORDER BY dh.timestamp DESC
LIMIT 1;
```

Deve mostrar:
- ✅ `last_seen_at` com timestamp recente (< 2 minutos)
- ✅ `wifi_rssi`, `mqtt_connected`, `free_heap` preenchidos

### 3. Front-end (Dashboard)
- Vá para a aba **"Status"** ou **"Gerenciar"**
- O dispositivo `SEN-9454C572E11C` deve aparecer com:
  - 🟢 Badge "Online"
  - Informações de WiFi (SSID, RSSI)
  - Memória livre
  - Status MQTT

---

## 🔧 Troubleshooting

### ❌ Erro 401 Unauthorized
**Causa:** Token do Authorization incorreto
**Solução:** Copie novamente o header Authorization da seção 4️⃣

### ❌ Erro 400 Bad Request
**Causa:** Body template incorreto
**Solução:** Verifique se o JSON no body template está exatamente como mostrado

### ❌ Erro 500 Internal Server Error
**Causa:** Problema na Edge Function
**Solução:** 
1. Verifique os logs do `mqtt-collector`
2. Confirme que o payload do heartbeat tem `device_uuid`

### ⚠️ Webhook criado mas dispositivo não aparece online
**Causa Provável:** O webhook foi criado, mas o ESP32 ainda não publicou o próximo heartbeat
**Solução:** Aguarde até 30 segundos ou reinicie o ESP32

### 🔍 Como Ver os Logs do Webhook no HiveMQ?
1. Na lista de webhooks, clique no webhook criado
2. Procure por "Logs", "History" ou "Delivery Log"
3. Você verá:
   - ✅ Status Code: 200 = Sucesso
   - ❌ Status Code: 4xx/5xx = Erro

---

## 📊 Dados que o Heartbeat Envia

O ESP32 v4.3.5-LITE envia a cada 30 segundos:
```json
{
  "device_uuid": "SEN-9454C572E11C",
  "device_type": "sensor",
  "firmware_version": "4.3.5-LITE",
  "uptime": 123456,
  "free_heap": 150000,
  "wifi": {
    "ssid": "SuaRede",
    "rssi": -65,
    "ip": "192.168.1.100"
  },
  "mqtt": {
    "connected": true,
    "failed_attempts": 0
  },
  "sensors": {
    "ph_valid": true,
    "ec_valid": true
  }
}
```

A Edge Function `mqtt-collector` já está **100% preparada** para receber e processar estes dados!

---

## 🎉 Próximos Passos (Após Configurar)

1. ✅ Confirme que o dispositivo aparece online no Dashboard
2. ✅ Verifique a aba Status para ver as métricas de health
3. ✅ Opcionalmente, configure webhooks para os outros tópicos:
   - `aquasys/relay/status` (para status dos relés)
   - `aquasys/sensor/calibration` (se usar calibração)

---

## 📚 Documentação de Referência

- [HiveMQ Webhooks Documentation](https://docs.hivemq.com/)
- [HIVEMQ-WEBHOOK-SETUP.md](./HIVEMQ-WEBHOOK-SETUP.md) - Setup completo de todos os webhooks
- Edge Function: `supabase/functions/mqtt-collector/index.ts`

---

**🔴 Lembre-se:** A configuração é feita **no console do HiveMQ Cloud**, não no código. Este é um processo manual de 5 minutos que só precisa ser feito uma vez!
