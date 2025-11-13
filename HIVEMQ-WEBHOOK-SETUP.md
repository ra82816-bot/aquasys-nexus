# 🔧 Configuração Urgente: HiveMQ Webhook → Supabase

## ⚠️ PROBLEMA IDENTIFICADO
Os dados do ESP32 estão chegando no HiveMQ Cloud, mas **não estão sendo enviados para o Supabase**. O webhook não está configurado ou está incorreto.

## 📋 Solução: Configurar Webhook no HiveMQ Cloud

### 1️⃣ Acesse o Console do HiveMQ Cloud
1. Vá para: https://console.hivemq.cloud/
2. Login com suas credenciais
3. Selecione seu cluster

### 2️⃣ Criar Webhook para Sensor Data

**Configuração do Webhook:**

```yaml
Nome: AquaSys Sensor Data to Supabase
Endpoint URL: https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector
Método: POST
Topic Filter: aquasys/sensors/all
```

**Headers (IMPORTANTE):**
```
Content-Type: application/json
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw
```

**Body Template (JSON):**
```json
{
  "topic": "${topic}",
  "payload": ${payload}
}
```

### 3️⃣ Criar Webhook para Heartbeat (Device Health)

```yaml
Nome: AquaSys Heartbeat to Supabase
Endpoint URL: https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector
Método: POST
Topic Filter: aquasys/heartbeat
```

**Headers:** (Iguais ao anterior)
```
Content-Type: application/json
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw
```

**Body Template (JSON):**
```json
{
  "topic": "${topic}",
  "payload": ${payload}
}
```

### 4️⃣ Criar Webhook para Relay Status

```yaml
Nome: AquaSys Relay Status to Supabase
Endpoint URL: https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector
Método: POST
Topic Filter: aquasys/relay/status
```

**Headers:** (Iguais aos anteriores)

**Body Template (JSON):**
```json
{
  "topic": "${topic}",
  "payload": ${payload}
}
```

## 🧪 Como Testar

### Opção 1: Via MQTTX (Manual)
1. Conecte-se ao broker HiveMQ
2. Publique no tópico `aquasys/sensors/all`:
```json
{
  "device_uuid": "SEN-9454C572E11C",
  "ph": 7.0,
  "ec": 1200,
  "air_temp": 25.0,
  "humidity": 60.0,
  "water_temp": 22.0
}
```

### Opção 2: Via Webhook Test do HiveMQ
1. No console do HiveMQ, clique no webhook criado
2. Use a função "Test Webhook"
3. Envie o JSON de exemplo acima

### Opção 3: Deixe o ESP32 Publicar Naturalmente
O firmware já está configurado. Apenas aguarde 15 segundos.

## ✅ Como Verificar se Funcionou

### 1. Checar Logs da Edge Function
No console do Supabase (ou via Lovable):
```
Ver logs da função: mqtt-collector
```

Você deve ver:
```
✅ Recebido: aquasys/sensors/all
✅ Inserindo leitura de sensores...
✅ Leitura inserida com sucesso
```

### 2. Checar Banco de Dados
Execute no SQL Editor:
```sql
SELECT * FROM readings 
ORDER BY timestamp DESC 
LIMIT 5;
```

Deve ter leituras **recentes** (últimos 15 segundos).

### 3. Checar o Front-end
Abra o Dashboard. As leituras devem aparecer **em tempo real** graças ao Supabase Realtime.

## 🔥 Se Ainda Não Funcionar

1. **Verifique o Status Code do Webhook:**
   - No HiveMQ Console, veja os logs do webhook
   - Deve retornar `200 OK`

2. **Se retornar 401 Unauthorized:**
   - O Bearer token está incorreto
   - Copie novamente da seção "Headers" acima

3. **Se retornar 500 Internal Server Error:**
   - Há um erro na Edge Function
   - Cheque os logs no Supabase

4. **Se retornar timeout:**
   - A URL do endpoint está incorreta
   - Verifique: `https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector`

## 📊 Formato de Dados Esperado

O ESP32 publica JSON neste formato:

```json
{
  "device_uuid": "SEN-9454C572E11C",
  "ph": 6.85,
  "ec": 1150,
  "air_temp": 26.2,
  "humidity": 55.3,
  "water_temp": 23.1
}
```

A Edge Function converte automaticamente para o formato do banco:
- `air_temp` → coluna `air_temp`
- Todas as chaves em lowercase batem com o schema

## 🎯 Resultado Esperado

Após configurar, você deve ver:
1. ✅ Logs constantes no `mqtt-collector`
2. ✅ Novos registros na tabela `readings` a cada 15s
3. ✅ Dashboard atualizado em tempo real
4. ✅ Last_seen_at do device atualizado constantemente

---

**Tempo estimado:** 5 minutos de configuração
**Impacto:** Crítico - sem isso, o sistema não funciona
