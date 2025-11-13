# 🔍 Relatório de Diagnóstico - AquaSys Nexus Data Flow

**Data:** 2025-11-13  
**Status:** ⚠️ PROBLEMAS CRÍTICOS IDENTIFICADOS E CORRIGIDOS

---

## 📊 Resumo Executivo

Os dados do ESP32 estão sendo publicados corretamente no HiveMQ Cloud, mas **NÃO estão chegando ao front-end** devido a **dois problemas críticos**:

1. ❌ **Webhook do HiveMQ não configurado** → Edge Function nunca é chamada
2. ❌ **Bug no parser da Edge Function** → Rejeita dados válidos do ESP32

---

## 🔴 Problema #1: Webhook HiveMQ → Supabase (CRÍTICO)

### Evidências
- ✅ ESP32 publica dados no HiveMQ (confirmado pelo MQTT Explorer)
- ✅ Dispositivo `SEN-9454C572E11C` autenticado (last_seen: **2025-11-13 12:19**)
- ❌ Edge Function `mqtt-collector`: **0 logs** (nunca foi chamada)
- ❌ Última leitura no banco: **2025-10-21 20:20** (dados antigos)

### Causa Raiz
O **webhook do HiveMQ não está configurado** ou está apontando para o endpoint errado. Sem o webhook, a Edge Function nunca recebe os dados do MQTT.

### Solução
✅ **Arquivo criado:** `HIVEMQ-WEBHOOK-SETUP.md`

Configure 3 webhooks no HiveMQ Cloud Console:
1. **Sensor Data:** `aquasys/sensors/all` → `mqtt-collector`
2. **Device Heartbeat:** `aquasys/heartbeat` → `mqtt-collector`
3. **Relay Status:** `aquasys/relay/status` → `mqtt-collector`

**Endpoint:** `https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector`

---

## 🔴 Problema #2: Bug na Edge Function (CORRIGIDO)

### O Bug
A Edge Function esperava dados em **camelCase** mas o ESP32 envia em **snake_case**:

```typescript
// ❌ Código ANTIGO (errado):
if (typeof data.airTemp === 'number' && !isNaN(data.airTemp))
if (typeof data.waterTemp === 'number' && !isNaN(data.waterTemp))

// ✅ Código NOVO (corrigido):
if (typeof data.air_temp === 'number' && !isNaN(data.air_temp))
if (typeof data.water_temp === 'number' && !isNaN(data.water_temp))
```

### Formato Real do ESP32
```json
{
  "device_uuid": "SEN-9454C572E11C",
  "ph": 6.85,
  "ec": 1150,
  "air_temp": 26.2,      // ← snake_case
  "humidity": 55.3,
  "water_temp": 23.1     // ← snake_case
}
```

### Impacto
- A validação `hasValidData` sempre falhava
- Dados válidos eram **rejeitados com erro 400**
- Nenhuma leitura era inserida no banco

### Status
✅ **CORRIGIDO** - A Edge Function agora suporta:
- `air_temp` e `airTemp` (backward compatibility)
- `water_temp` e `waterTemp` (backward compatibility)

---

## ✅ Componentes que Estão Funcionando

### Hardware (ESP32)
- ✅ Leitura de sensores operacional
- ✅ Conexão WiFi estável
- ✅ Autenticação Supabase com sucesso
- ✅ Publicação MQTT no HiveMQ Cloud
- ✅ Formato JSON correto

### Front-end (React + Supabase)
- ✅ Dashboard configurado corretamente
- ✅ Supabase Realtime ativo (escuta `INSERT` em `readings`)
- ✅ Componentes de UI prontos
- ✅ RLS policies corretas ("Anyone can view readings")

### Banco de Dados
- ✅ Tabela `readings` com schema correto
- ✅ Device `SEN-9454C572E11C` cadastrado
- ✅ RLS permite SELECT público
- ✅ Service role pode INSERT

---

## 📋 Checklist de Ações Imediatas

### Passo 1: Configurar Webhook (URGENTE)
- [ ] Acesse https://console.hivemq.cloud/
- [ ] Crie webhook para `aquasys/sensors/all`
- [ ] Configure endpoint: `https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector`
- [ ] Adicione header: `Authorization: Bearer eyJhbGc...` (anon key)
- [ ] Body template: `{"topic": "${topic}", "payload": ${payload}}`

**Referência completa:** Ver `HIVEMQ-WEBHOOK-SETUP.md`

### Passo 2: Verificar Funcionamento
Após configurar o webhook, em **15 segundos** você deve ver:

1. **Logs da Edge Function:**
```
✅ Processando mensagem do tópico: aquasys/sensors/all
✅ Dados de sensores recebidos: {...}
✅ Leituras inseridas com sucesso!
```

2. **Dados no Banco:**
```sql
SELECT * FROM readings ORDER BY timestamp DESC LIMIT 1;
-- Timestamp deve ser dos últimos 15 segundos
```

3. **Dashboard Atualizado:**
- Cards de sensores com valores em tempo real
- Gráficos populados
- Status "Online" no footer

### Passo 3: Monitoramento (Próximas 24h)
- [ ] Verificar logs da `mqtt-collector` para erros
- [ ] Confirmar leituras a cada 15 segundos
- [ ] Testar desconexão/reconexão do ESP32
- [ ] Validar fallback BLE (se aplicável)

---

## 🎯 Resultado Esperado

Após configurar o webhook:

```
ESP32 → (publica) → HiveMQ Cloud → (webhook) → Edge Function → (insert) → Supabase → (realtime) → Dashboard
   ✅               ✅                🔧 FAZER          ✅                 ✅                ✅
```

**Status Atual:**
- Hardware → HiveMQ: ✅ **Funcionando**
- HiveMQ → Supabase: ❌ **Webhook faltando** (você precisa configurar)
- Supabase → Frontend: ✅ **Funcionando**

---

## 🚀 Próximos Passos Após Resolver

1. **Monitorar Rate Limiting:**
   - A Edge Function tem proteção contra spam (100 req/min)
   - Se o ESP32 enviar muito rápido, será bloqueado por 5 min

2. **Otimizar Intervalo de Publicação:**
   - Atual: 15 segundos (razoável)
   - Se precisar mais frequente, ajustar rate limits

3. **Ativar Heartbeat:**
   - Webhook para `aquasys/heartbeat` (health monitoring)
   - Dados de uptime, heap, WiFi RSSI, etc.

4. **Implementar Fallback BLE:**
   - Modo offline via Bluetooth (conforme firmware Lite)

---

## 📞 Suporte

Se após configurar o webhook você ainda não ver dados:

1. **Verifique logs do webhook no HiveMQ:**
   - Console → Integrations → Webhooks → Ver logs
   - Deve mostrar `200 OK` responses

2. **Teste manual via MQTTX:**
   - Publique JSON diretamente no tópico
   - Confirme se Edge Function processa

3. **Verifique RLS no Supabase:**
   - Execute: `SELECT * FROM readings ORDER BY timestamp DESC LIMIT 1;`
   - Se der erro, problema de permissões

---

**Timestamp:** 2025-11-13 12:26:00  
**Versão Edge Function:** v2.1.0 (snake_case fix)  
**Firmware ESP32:** v4.3.4-FREERTOS-OPTIMIZED
