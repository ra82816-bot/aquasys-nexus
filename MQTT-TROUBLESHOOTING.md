# 🔧 Troubleshooting MQTT - "Not Authorized"

## ❌ Erro atual
```
Connection refused: Not authorized (código 5)
```

## ✅ Checklist de verificação

### 1. Verificar credenciais no HiveMQ Cloud

1. Acesse: https://console.hivemq.cloud/
2. Selecione seu cluster: `8cda72f06f464778bc53751d7cc88ac2`
3. Vá em **"Access Management"**
4. Verifique se o usuário `esp32-user` existe
5. **IMPORTANTE:** Confirme a senha - ela pode ter sido redefinida

### 2. Criar/Recriar credenciais WebSocket

No HiveMQ Console:

1. Vá em **Access Management** → **Credentials**
2. Clique em **"Add Credentials"** (ou delete e recrie `esp32-user`)
3. Configure:
   - **Username:** `esp32-user` (ou crie `web-client`)
   - **Password:** escolha uma senha forte
   - **Permissions:** Certifique-se que tem permissão para:
     - ✅ Publish em `aquasys/#`
     - ✅ Subscribe em `aquasys/#`

### 3. Verificar configuração WebSocket

1. No HiveMQ Console, vá em **"Overview"**
2. Procure pela seção **"WebSocket"**
3. Confirme que a URL é exatamente:
   ```
   wss://8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud:8884/mqtt
   ```

### 4. Testar com MQTTX (recomendado)

Antes de testar no navegador, teste com o MQTTX Desktop:

1. Baixe: https://mqttx.app/
2. Crie nova conexão:
   - **Host:** `wss://8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud`
   - **Port:** `8884`
   - **Path:** `/mqtt`
   - **Username:** `esp32-user`
   - **Password:** sua senha
   - **SSL/TLS:** ✅ Habilitado
3. Clique em **Connect**
4. Se conectar ✅ → As credenciais estão corretas
5. Se falhar ❌ → As credenciais estão incorretas no HiveMQ

## 🔄 Solução alternativa: Criar novo usuário

Se o problema persistir, crie um usuário específico para o frontend:

### No HiveMQ Console:

1. Access Management → Credentials → **Add Credentials**
2. Configure:
   ```
   Username: web-dashboard
   Password: [escolha uma senha forte, ex: WebDash2024!@#]
   ```
3. Permissions:
   - Topic: `aquasys/#`
   - Publish: ✅
   - Subscribe: ✅

### Atualize src/config/mqtt.ts:

```typescript
export const MQTT_CONFIG = {
  broker: 'wss://8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud:8884/mqtt',
  username: 'web-dashboard', // NOVO USUÁRIO
  password: 'WebDash2024!@#', // NOVA SENHA
  // ... resto igual
};
```

## 🚨 Causas comuns

### Senha incorreta
- A senha no HiveMQ é **case-sensitive**
- Verifique espaços extras antes/depois da senha
- Tente recriar a credencial

### Permissões insuficientes
- O usuário precisa de permissão para os tópicos `aquasys/*`
- Verifique se não há restrições de IP (improvável no plano free)

### Limite de conexões (Free Tier)
- O plano gratuito do HiveMQ permite **apenas 1 conexão simultânea**
- Se o ESP32 estiver conectado, o frontend não conseguirá conectar
- Teste desconectando o ESP32 primeiro

### ClientId duplicado
- Se o ESP32 usar o mesmo `clientId`, haverá conflito
- O frontend já usa ID único: `aquasys-web-${Math.random()...}`

## 📝 Próximos passos

1. **Primeiro:** Teste com MQTTX para confirmar credenciais
2. **Segundo:** Se MQTTX funcionar, o problema está no código
3. **Terceiro:** Se MQTTX falhar, recrie as credenciais no HiveMQ
4. **Quarto:** Certifique-se que o ESP32 não está usando todas as conexões disponíveis

## 🔍 Debug adicional

Se após todas as verificações o erro persistir, pode ser:
- Firewall/proxy corporativo bloqueando WebSocket
- Restrições de CORS (improvável com HiveMQ)
- Problemas temporários no HiveMQ Cloud

**Solução:** Tente em outra rede (mobile hotspot) para descartar bloqueios de rede.
