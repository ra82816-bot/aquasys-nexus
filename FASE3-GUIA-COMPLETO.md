# Fase 3: OTA Updates, Calibração Remota e Rate Limiting

## 🎯 Resumo da Implementação

Implementamos a **Fase 3** com funcionalidades avançadas de gerenciamento remoto de dispositivos:

1. **OTA Updates** - Atualização remota de firmware
2. **Calibração Remota** - Calibrar sensores via MQTT/interface web
3. **Rate Limiting** - Proteção contra sobrecarga e ataques

---

## 🔄 1. OTA Updates (Over-The-Air)

### Funcionalidades

- ✅ Upload de novo firmware para dispositivos remotamente
- ✅ Verificação automática de versão
- ✅ Opção de forçar atualização
- ✅ Histórico de atualizações
- ✅ Rollback automático em caso de falha

### Como Funciona

```
┌─────────────────────────────────────────────┐
│ 1. Usuário envia novo firmware via UI      │
│    - Seleciona dispositivo                  │
│    - Envia arquivo .bin                     │
│    - Define versão                          │
└─────────────────────────────────────────────┘
                    │
                    ↓
┌─────────────────────────────────────────────┐
│ 2. Backend processa e valida               │
│    - Verifica ownership do dispositivo      │
│    - Compara versões                        │
│    - Armazena firmware em storage           │
└─────────────────────────────────────────────┘
                    │
                    ↓
┌─────────────────────────────────────────────┐
│ 3. Comando OTA enviado via MQTT            │
│    - Dispositivo recebe URL do firmware     │
│    - Baixa e valida checksum                │
│    - Aplica atualização                     │
└─────────────────────────────────────────────┘
                    │
                    ↓
┌─────────────────────────────────────────────┐
│ 4. Verificação pós-update                  │
│    - Se boot OK: confirma nova versão       │
│    - Se falha: rollback automático          │
└─────────────────────────────────────────────┘
```

### API Endpoint: `/device-ota`

**Request:**
```json
{
  "device_uuid": "HYDRO-XXXX-XXXX-XXXX",
  "firmware_url": "https://storage.url/firmware.bin",
  "firmware_version": "3.2",
  "force": false
}
```

**Response:**
```json
{
  "success": true,
  "message": "Atualização OTA iniciada",
  "current_version": "3.1",
  "target_version": "3.2"
}
```

### Tabela `device_commands`

Registra todos os comandos OTA enviados:

```sql
CREATE TABLE device_commands (
  id UUID PRIMARY KEY,
  device_id UUID REFERENCES devices(id),
  command_type TEXT, -- 'ota_update', 'calibration', etc
  command_data JSONB,
  status TEXT, -- 'pending', 'executed', 'failed'
  executed_at TIMESTAMP,
  created_at TIMESTAMP
);
```

### Implementação no Firmware (Exemplo)

```cpp
void handleOTAUpdate(const char* firmwareUrl, const char* version) {
  logMessage("INFO", "Iniciando OTA update...");
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // Em produção, validar certificado
  
  http.begin(client, firmwareUrl);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    int contentLength = http.getSize();
    
    if (Update.begin(contentLength)) {
      size_t written = Update.writeStream(http.getStream());
      
      if (written == contentLength) {
        if (Update.end()) {
          logMessage("INFO", "OTA sucesso! Reiniciando...");
          ESP.restart();
        }
      }
    }
  }
  
  http.end();
  logMessage("ERROR", "OTA falhou");
}
```

---

## 🔬 2. Calibração Remota de Sensores

### Funcionalidades

- ✅ Calibração de pH (pontos 4.0, 7.0, 10.0)
- ✅ Calibração de EC com fator
- ✅ Perfis de calibração salvos
- ✅ Histórico de calibrações
- ✅ Alertas quando calibração estiver antiga (>30 dias)

### Interface de Calibração

A UI fornece formulário intuitivo:

**Para pH:**
- Voltagem em pH 4.0 (obrigatório)
- Voltagem em pH 7.0 (obrigatório)
- Voltagem em pH 10.0 (opcional)

**Para EC:**
- Fator de calibração baseado em solução padrão

### API Endpoint: `/device-calibration`

**Request:**
```json
{
  "device_uuid": "HYDRO-XXXX-XXXX-XXXX",
  "sensor_type": "ph",
  "calibration_data": {
    "ph_cal4_voltage": 2.03,
    "ph_cal7_voltage": 1.50,
    "ph_cal10_voltage": 0.97
  },
  "profile_name": "Calibração Junho 2024"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Calibração aplicada com sucesso",
  "profile_id": "uuid",
  "profile_name": "Calibração Junho 2024"
}
```

### Tabela `device_calibration_profiles`

```sql
CREATE TABLE device_calibration_profiles (
  id UUID PRIMARY KEY,
  device_id UUID REFERENCES devices(id),
  sensor_type TEXT, -- 'ph' ou 'ec'
  calibration_data JSONB,
  profile_name TEXT,
  is_active BOOLEAN, -- Apenas um perfil ativo por sensor
  created_by UUID REFERENCES auth.users(id),
  created_at TIMESTAMP
);
```

### Processo de Calibração

1. **Preparação:**
   - Mergulhar sensor em solução padrão conhecida
   - Aguardar estabilização (1-2 minutos)

2. **Leitura:**
   - Ler voltagem via Serial Monitor
   - Anotar valor exato

3. **Aplicação:**
   - Inserir valores na UI
   - Sistema calcula slope e intercept automaticamente
   - Envia via MQTT para o dispositivo

4. **Validação:**
   - Dispositivo aplica novos coeficientes
   - Testa com solução conhecida
   - Confirma sucesso

### Cálculo de Coeficientes (pH)

```cpp
void calculatePHCoefficients() {
  // Pontos conhecidos: (V4, pH4), (V7, pH7)
  phCalibSlope = (7.0 - 4.0) / (phCal7Voltage - phCal4Voltage);
  phCalibIntercept = 7.0 - (phCalibSlope * phCal7Voltage);
  
  // Fórmula: pH = slope * voltage + intercept
  
  logMessage("INFO", ("Slope: " + String(phCalibSlope)).c_str());
  logMessage("INFO", ("Intercept: " + String(phCalibIntercept)).c_str());
}
```

---

## 🛡️ 3. Rate Limiting e Proteções

### Funcionalidades

- ✅ Limite de 100 requisições/minuto por dispositivo
- ✅ Bloqueio automático de 5 minutos ao exceder
- ✅ Janela deslizante de 1 minuto
- ✅ Proteção contra DoS via MQTT
- ✅ Limpeza automática de registros antigos

### Configuração

```typescript
const RATE_LIMIT_WINDOW = 60000;        // 1 minuto
const MAX_REQUESTS_PER_WINDOW = 100;    // 100 req/min
const BLOCK_DURATION = 300000;          // 5 minutos de bloqueio
```

### Tabela `mqtt_rate_limits`

```sql
CREATE TABLE mqtt_rate_limits (
  id UUID PRIMARY KEY,
  device_id UUID REFERENCES devices(id),
  endpoint TEXT, -- Tópico MQTT
  request_count INTEGER,
  window_start TIMESTAMP,
  blocked_until TIMESTAMP,
  created_at TIMESTAMP
);
```

### Lógica de Rate Limiting

```typescript
async function checkRateLimit(deviceUuid, endpoint) {
  const now = new Date();
  
  // 1. Verificar se está bloqueado
  if (existingLimit.blocked_until > now) {
    return { allowed: false, message: "Dispositivo bloqueado" };
  }
  
  // 2. Verificar janela de tempo
  const timeSinceWindowStart = now - existingLimit.window_start;
  
  if (timeSinceWindowStart < RATE_LIMIT_WINDOW) {
    // Dentro da janela
    const newCount = existingLimit.request_count + 1;
    
    if (newCount > MAX_REQUESTS_PER_WINDOW) {
      // Excedeu limite, bloquear
      blockedUntil = now + BLOCK_DURATION;
      return { allowed: false, message: "Taxa excedida" };
    }
    
    // Incrementar contador
    return { allowed: true };
  } else {
    // Nova janela, resetar
    return { allowed: true };
  }
}
```

### Response ao Exceder Limite

```json
{
  "error": "Rate limit excedido",
  "details": "Bloqueado até 2024-06-20T15:30:00Z",
  "status": 429
}
```

### Limpeza Automática (via Cron)

Configurar no Supabase:

```sql
SELECT cron.schedule(
  'cleanup-rate-limits',
  '0 */1 * * *', -- A cada hora
  $$
    SELECT public.cleanup_old_rate_limits();
  $$
);
```

---

## 📊 Dashboard de Gerenciamento

### Componentes Adicionados

1. **DeviceCalibration.tsx**
   - Formulário de calibração
   - Seleção de dispositivo e sensor
   - Instruções passo-a-passo

2. **DeviceList.tsx** (Melhorado)
   - Indicadores de health em tempo real
   - Status de WiFi, MQTT, BLE
   - Métricas de memória
   - Última calibração

### Página Devices

```
┌────────────────────────────────────────────┐
│           GERENCIAR DISPOSITIVOS           │
├────────────────────────────────────────────┤
│                                            │
│  [Vincular Novo]  [Calibração]             │
│                                            │
│  ┌──────────────────────────────────────┐ │
│  │ HYDRO-XXXX-XXXX-XXXX                 │ │
│  │ ● Online | WiFi: -67dBm | Mem: 156KB │ │
│  │ pH ✓ | EC ✓ | Temp ✓                 │ │
│  │ Última calibração: 5 dias atrás       │ │
│  │ [Calibrar] [Atualizar Firmware]      │ │
│  └──────────────────────────────────────┘ │
│                                            │
└────────────────────────────────────────────┘
```

---

## 🔐 Segurança Implementada

### 1. Autenticação de Comandos
- Apenas proprietários podem enviar comandos
- Validação de ownership em todas as operações
- JWT obrigatório em edge functions

### 2. Rate Limiting
- Previne ataques DoS
- Bloqueia dispositivos maliciosos
- Logs de tentativas excessivas

### 3. Validação de Firmware
- Checksum MD5 obrigatório
- Validação de tamanho máximo
- Rollback automático em falhas

### 4. Proteção de Dados de Calibração
- RLS policies aplicadas
- Apenas usuário proprietário pode modificar
- Histórico imutável

---

## 📝 Comandos MQTT

### Estrutura de Comando

Todos os comandos seguem o padrão:

```json
{
  "device_uuid": "HYDRO-XXXX-XXXX-XXXX",
  "command": "command_type",
  "data": { /* dados específicos */ },
  "timestamp": 1234567890
}
```

### Tipos de Comandos

1. **OTA Update:**
```json
{
  "command": "ota_update",
  "firmware_url": "https://...",
  "firmware_version": "3.2",
  "checksum": "md5hash"
}
```

2. **Calibração:**
```json
{
  "command": "calibrate",
  "sensor_type": "ph",
  "calibration_data": {
    "ph_cal4_voltage": 2.03,
    "ph_cal7_voltage": 1.50
  }
}
```

3. **Controle de Relé:**
```json
{
  "command": "relay_control",
  "relay_index": 3,
  "state": true,
  "duration_ms": 5000
}
```

---

## 🚀 Próximos Passos

### Melhorias Futuras:
- [ ] OTA automático baseado em schedule
- [ ] Calibração assistida por IA
- [ ] Backup/restauração de configurações
- [ ] Telemetria avançada
- [ ] Notificações push para alertas de calibração

### Monitoramento:
- [ ] Dashboard de taxa de requisições
- [ ] Alertas de dispositivos bloqueados
- [ ] Métricas de sucesso de OTA
- [ ] Estatísticas de calibração

---

## 🆘 Troubleshooting

### OTA não funciona
1. Verificar conectividade WiFi
2. Confirmar URL do firmware acessível
3. Validar espaço disponível (>1.5MB)
4. Conferir logs no Serial Monitor

### Rate limit bloqueando indevidamente
1. Verificar intervalos de publicação do firmware
2. Ajustar `MAX_REQUESTS_PER_WINDOW` se necessário
3. Limpar registros antigos manualmente

### Calibração não aplica
1. Conferir se dispositivo está online
2. Verificar se comando foi recebido (logs MQTT)
3. Validar valores de voltagem inseridos
4. Reiniciar dispositivo após calibração

---

**Fase 3 Completa!** ✨

Sistema agora possui gerenciamento remoto completo, proteções contra abuso e calibração profissional de sensores.
