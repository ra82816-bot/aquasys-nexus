# 🚀 Guia Completo: Configurar Webhooks HiveMQ Cloud

## Passo 1: Acessar o Console do HiveMQ

1. Acesse: **https://console.hivemq.cloud**
2. Faça login com suas credenciais
3. Você verá a lista dos seus clusters
4. Clique no cluster que você está usando para o AquaSys

## Passo 2: Navegar até Webhooks

1. No menu lateral esquerdo, procure por **"Integrations"** ou **"Extensions"**
2. Clique em **"Webhooks"** ou **"HTTP Extension"**
3. Clique no botão **"+ New Webhook"** ou **"Add Webhook"**

## Passo 3: Criar Webhook para Sensores

### Configurações Básicas:
- **Name/Description:** `AquaSys Sensors`
- **Enabled:** ✅ Marcar como habilitado

### Topic Configuration:
- **Topic Filter:** `aquasys/sensors/all`
- **QoS:** 0 ou 1 (recomendado 1)

### Endpoint Configuration:
- **URL:** `https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector`
- **HTTP Method:** `POST`

### Headers:
Adicione o seguinte header:
```
Content-Type: application/json
```

### Body/Payload Template:

**Se o HiveMQ permitir usar variáveis do payload:**
```json
{
  "action": "process_sensors",
  "data": ${payload}
}
```

**OU se precisar mapear manualmente:**
```json
{
  "action": "process_sensors",
  "data": {
    "ph": ${ph},
    "ec": ${ec},
    "airTemp": ${airTemp},
    "humidity": ${humidity},
    "waterTemp": ${waterTemp}
  }
}
```

**OU formato mais simples (passar payload direto):**
```json
{
  "action": "process_sensors",
  "data": ${payload}
}
```

### Salvar:
- Clique em **"Save"** ou **"Create"**
- Verifique se o webhook aparece como **"Active"** ou **"Enabled"**

---

## Passo 4: Criar Webhook para Status dos Relés

Repita o processo acima com as seguintes configurações:

### Configurações Básicas:
- **Name/Description:** `AquaSys Relay Status`
- **Enabled:** ✅ Marcar como habilitado

### Topic Configuration:
- **Topic Filter:** `aquasys/relay/status`
- **QoS:** 0 ou 1

### Endpoint Configuration:
- **URL:** `https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector`
- **HTTP Method:** `POST`

### Headers:
```
Content-Type: application/json
```

### Body/Payload Template:
```json
{
  "action": "process_relay_status",
  "data": ${payload}
}
```

**OU se precisar mapear:**
```json
{
  "action": "process_relay_status",
  "data": {
    "relay1_led": ${relay1_led},
    "relay2_pump": ${relay2_pump},
    "relay3_ph_up": ${relay3_ph_up},
    "relay4_fan": ${relay4_fan},
    "relay5_humidity": ${relay5_humidity},
    "relay6_ec": ${relay6_ec},
    "relay7_co2": ${relay7_co2},
    "relay8_generic": ${relay8_generic}
  }
}
```

### Salvar:
- Clique em **"Save"** ou **"Create"**

---

## Passo 5: Testar os Webhooks

### Teste Manual no Console HiveMQ:

1. No HiveMQ Cloud Console, vá para **"Web Client"** ou **"MQTT Web Client"**
2. Conecte-se ao seu cluster
3. Publique uma mensagem de teste:

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

4. Verifique:
   - Vá ao seu dashboard do AquaSys
   - Os dados devem aparecer automaticamente
   - Verifique também os logs da Edge Function para confirmar recebimento

---

## 🔍 Solução de Problemas

### Webhook não está disparando:

1. **Verifique se está habilitado:**
   - Vá em Webhooks e confirme que está "Active"

2. **Verifique o Topic Filter:**
   - Deve ser EXATAMENTE: `aquasys/sensors/all` ou `aquasys/relay/status`
   - Não pode ter espaços extras

3. **Verifique a URL:**
   - Deve ser exatamente: `https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector`
   - Sem "/" no final

4. **Verifique os Logs do HiveMQ:**
   - Na interface do webhook, procure por "Logs" ou "History"
   - Veja se há erros de conexão

### Dados não aparecem no Dashboard:

1. **Verifique os logs da Edge Function:**
   - Acesse o backend do Lovable Cloud
   - Vá em Functions → mqtt-collector → Logs
   - Veja se há erros

2. **Verifique o formato do payload:**
   - O JSON deve estar correto
   - Use um validador JSON online se necessário

3. **Teste diretamente a Edge Function:**
   - Use Postman ou curl para testar:
   ```bash
   curl -X POST https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector \
   -H "Content-Type: application/json" \
   -d '{
     "action": "process_sensors",
     "data": {
       "ph": 6.5,
       "ec": 1200,
       "airTemp": 25,
       "humidity": 65,
       "waterTemp": 23
     }
   }'
   ```

---

## 📝 Notas Importantes

### Formato do Payload Template:

Diferentes versões do HiveMQ Cloud podem ter sintaxes diferentes:

- **Versão mais recente:** `${payload}` (passa todo o payload JSON)
- **Versão antiga:** `{{payload}}` (duplo colchete)
- **Versão específica:** Precisa mapear cada campo individualmente

**Recomendação:** Tente primeiro com `${payload}`, se não funcionar, tente mapear os campos manualmente.

### Rate Limits:

- HiveMQ Cloud free tier pode ter limites de requisições
- Se tiver muitos dados, considere adicionar um delay no ESP32

### Autenticação:

- A Edge Function `mqtt-collector` está configurada com `verify_jwt = false`
- Isso permite que o HiveMQ chame diretamente sem autenticação
- É seguro porque apenas aceita dados de sensores (read-only na perspectiva do webhook)

---

## ✅ Checklist Final

- [ ] Webhook "AquaSys Sensors" criado e ativo
- [ ] Webhook "AquaSys Relay Status" criado e ativo
- [ ] Teste manual enviou dados com sucesso
- [ ] Dados aparecem no dashboard
- [ ] ESP32 continua publicando normalmente nos tópicos MQTT

**Pronto! Agora seu sistema está 100% serverless e não precisa mais do script Python rodando.**
