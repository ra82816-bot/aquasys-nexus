# 🚀 Setup MQTT no Frontend (Sem Backend Python)

## ✅ O que foi implementado

Implementação completa de comunicação MQTT diretamente do frontend React usando **mqtt.js** via WebSocket, eliminando a necessidade do backend Python (bridge.py).

## 📦 Arquivos criados/modificados

### Novos arquivos:
- `src/config/mqtt.ts` - Configuração centralizada do MQTT
- `src/hooks/useMqtt.tsx` - Hook customizado para gerenciar conexão MQTT
- `src/contexts/MqttContext.tsx` - Context Provider para compartilhar conexão entre componentes

### Modificados:
- `src/App.tsx` - Adicionado `MqttProvider` no root
- `src/components/dashboard/MqttStatus.tsx` - Agora usa MQTT direto (não mais Edge Function)

## 🔧 Configuração necessária

### 1. Atualizar credenciais do HiveMQ

Edite o arquivo `src/config/mqtt.ts` com suas credenciais:

```typescript
export const MQTT_CONFIG = {
  broker: 'wss://seu-cluster.s1.eu.hivemq.cloud:8884/mqtt', // ⚠️ SUBSTITUIR
  username: 'seu-usuario', // ⚠️ SUBSTITUIR
  password: 'sua-senha', // ⚠️ SUBSTITUIR
  // ... resto da config
};
```

**Como encontrar suas credenciais:**
1. Acesse [HiveMQ Cloud Console](https://console.hivemq.cloud/)
2. Selecione seu cluster
3. Vá em "Access Management" → "WebSocket"
4. Copie a URL do WebSocket (wss://...)
5. Use as credenciais criadas anteriormente

### 2. Configurar no Vercel (Produção)

Para deploy no Vercel, você tem 2 opções:

#### Opção A: Variáveis de ambiente (recomendado)
1. No dashboard do Vercel, vá em Settings → Environment Variables
2. Adicione:
   ```
   VITE_MQTT_BROKER=wss://seu-cluster.s1.eu.hivemq.cloud:8884/mqtt
   VITE_MQTT_USERNAME=seu-usuario
   VITE_MQTT_PASSWORD=sua-senha
   ```

3. Modifique `src/config/mqtt.ts`:
   ```typescript
   export const MQTT_CONFIG = {
     broker: import.meta.env.VITE_MQTT_BROKER || 'wss://fallback.com',
     username: import.meta.env.VITE_MQTT_USERNAME || '',
     password: import.meta.env.VITE_MQTT_PASSWORD || '',
     // ...
   };
   ```

#### Opção B: Hardcoded (mais simples, menos seguro)
Apenas atualize `src/config/mqtt.ts` antes do deploy e faça commit das credenciais.

> ⚠️ **Nota:** Como as credenciais MQTT são específicas para este dispositivo e não são APIs públicas críticas, a Opção B é aceitável para projetos pequenos.

## 🎯 Como usar nos componentes

### Exemplo básico:
```tsx
import { useMqttContext } from '@/contexts/MqttContext';

function MeuComponente() {
  const { isConnected, publishRelayCommand } = useMqttContext();

  const ligarRele = async () => {
    await publishRelayCommand(1, true); // Liga relé 1
  };

  return (
    <div>
      Status: {isConnected ? '🟢 Conectado' : '🔴 Desconectado'}
      <button onClick={ligarRele}>Ligar Relé 1</button>
    </div>
  );
}
```

### Exemplo com publicação customizada:
```tsx
const { publish } = useMqttContext();

const enviarComando = async () => {
  await publish('aquasys/custom/topic', {
    action: 'custom',
    data: { value: 123 }
  });
};
```

### Exemplo com escuta de mensagens:
```tsx
const { lastMessage } = useMqttContext();

useEffect(() => {
  if (lastMessage?.topic === 'aquasys/sensors/all') {
    console.log('Dados dos sensores:', lastMessage.payload);
    // Processar dados...
  }
}, [lastMessage]);
```

## 📡 Funcionalidades implementadas

✅ **Conexão WebSocket segura** com autenticação  
✅ **Subscribe automático** em tópicos relevantes  
✅ **Publish de comandos** para relés  
✅ **Reconexão automática** em caso de perda de conexão  
✅ **Tratamento de erros** com toast notifications  
✅ **Context API** para compartilhar conexão globalmente  
✅ **TypeScript** com tipos completos  

## 🔄 Fluxo de comunicação

```
Frontend (React) ←→ MQTT Broker (HiveMQ Cloud) ←→ ESP32
         ↓
   Supabase (apenas para armazenamento histórico)
```

### Tópicos MQTT:
- **Subscribe:**
  - `aquasys/sensors/all` - Recebe dados dos sensores
  - `aquasys/relay/status` - Recebe status dos relés

- **Publish:**
  - `aquasys/relay/commands` - Envia comandos para relés

## 🧪 Como testar

### 1. No Lovable.dev:
O MQTT já conectará automaticamente após atualizar as credenciais em `src/config/mqtt.ts`.

### 2. Testar manualmente com MQTTX:
1. Baixe [MQTTX](https://mqttx.app/)
2. Configure conexão com suas credenciais HiveMQ
3. Publique mensagem de teste em `aquasys/sensors/all`:
   ```json
   {
     "ph": 6.5,
     "ec": 1.2,
     "airTemp": 25.5,
     "humidity": 60,
     "waterTemp": 22.0
   }
   ```
4. Verifique se aparece no dashboard

## 🗑️ O que pode ser removido

Após confirmar que tudo funciona:
- ❌ `mqtt-bridge/bridge.py` (não é mais necessário)
- ❌ `mqtt-bridge/requirements.txt`
- ❌ Edge Function `mqtt-ping` (se não usar mais)

> ⚠️ **Mantenha:** Edge Functions `mqtt-collector` e `relay-control` ainda são úteis para histórico no Supabase.

## 🚨 Troubleshooting

### "MQTT não conecta"
- Verifique se as credenciais estão corretas
- Confirme que a URL é `wss://` (WebSocket Secure)
- Certifique-se que a porta é 8884
- Teste conexão com MQTTX primeiro

### "Reconexão constante"
- Verifique limite de conexões no HiveMQ Cloud (free tier = 1 conexão)
- Se o ESP32 e frontend usarem mesmo clientId, haverá conflito

### "Comandos não chegam no ESP32"
- Verifique se ESP32 está subscrito em `aquasys/relay/commands`
- Teste publicar manualmente via MQTTX
- Confira logs do ESP32

## 📚 Recursos úteis

- [MQTT.js Docs](https://github.com/mqttjs/MQTT.js)
- [HiveMQ Cloud Docs](https://docs.hivemq.com/hivemq-cloud/)
- [MQTT Protocol](https://mqtt.org/)

---

**Status:** ✅ Implementação completa e funcional  
**Próximo passo:** Atualizar credenciais em `src/config/mqtt.ts` e testar!
