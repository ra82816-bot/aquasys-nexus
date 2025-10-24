# Configuração Bluetooth BLE - Fase 2

## 🎯 Resumo da Implementação

Implementamos comunicação **Bluetooth Low Energy (BLE)** entre os módulos ESP32 como **fallback automático** quando WiFi/MQTT estiverem indisponíveis.

## 📋 Arquitetura BLE

### Módulo de Sensores (v3.1) - BLE Server
- **Função**: Publica dados críticos de sensores via BLE
- **UUID do Serviço**: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- **UUID da Característica**: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- **Dados transmitidos**:
  - pH
  - Temperatura da água
  - EC (condutividade)
  - Timestamp
  - Checksum (validação de integridade)

### Módulo de Atuadores (v4.2) - BLE Client
- **Função**: Conecta-se ao sensor e recebe dados
- **Comportamento**: Escaneia e conecta automaticamente ao sensor mais próximo
- **Usa os dados BLE** para controlar relés quando MQTT estiver offline

## ⚙️ Ativação Automática

O BLE é ativado **automaticamente** nas seguintes condições:

1. **MQTT offline por mais de 3 minutos** (180 segundos)
2. **WiFi instável** com reconexões frequentes
3. **Modo manual** via comando MQTT

### Fluxo de Operação:

```
┌─────────────────────────────────────────────┐
│ Sistema Operando Normalmente                │
│ WiFi ✓ → MQTT ✓ → Dados via MQTT          │
└─────────────────────────────────────────────┘
                    │
                    ↓ (MQTT falha > 3 min)
                    │
┌─────────────────────────────────────────────┐
│ Modo Fallback BLE Ativado                   │
│ WiFi ✓ → MQTT ✗ → Dados via BLE           │
└─────────────────────────────────────────────┘
                    │
                    ↓ (MQTT restaurado)
                    │
┌─────────────────────────────────────────────┐
│ Retorno ao Modo Normal                      │
│ WiFi ✓ → MQTT ✓ → Dados via MQTT          │
│ BLE permanece ativo (standby)               │
└─────────────────────────────────────────────┘
```

## 🔒 Segurança

### Estrutura de Dados BLE
```cpp
struct BLEData {
  float ph;           // 4 bytes
  float waterTemp;    // 4 bytes
  float ec;           // 4 bytes
  uint32_t timestamp; // 4 bytes
  uint8_t checksum;   // 1 byte - XOR de todos os bytes
} __attribute__((packed));  // Total: 17 bytes
```

### Validação de Checksum
- Todos os dados BLE incluem checksum XOR
- Receptor valida antes de usar
- Dados corrompidos são descartados

## 📡 Alcance e Performance

- **Alcance típico**: 10-20 metros em ambientes internos
- **Latência**: ~100-200ms
- **Consumo de energia**: Muito baixo (~10-50mA em modo ativo)
- **Taxa de atualização**: A cada 5 segundos durante modo fallback

## 🔧 Comandos de Diagnóstico

### Via Display OLED (Módulo de Sensores)
- Navegue até a página **"BLE STATUS"** usando os botões
- Informações exibidas:
  - BLE Ativo: SIM/NÃO
  - Cliente Conectado: SIM/NÃO
  - Número de conexões
  - "MODO FALLBACK" quando ativo

### Via MQTT (Heartbeat)
```json
{
  "device": "ESP32_Sensor_HYDRO-XXXX-XXXX-XXXX",
  "ble": {
    "active": true,
    "connected": true,
    "connections": 5
  }
}
```

## 🚀 Upload dos Firmwares

### Módulo de Sensores (v3.1-BLE)
```bash
# Arduino IDE
1. Abrir: Firmware_ESP32_Sensores_v3.1_COM_BLE.ino
2. Selecionar: Placa "ESP32 Dev Module"
3. Incluir biblioteca: BLE (built-in do ESP32)
4. Compilar e enviar
```

### Módulo de Atuadores (v4.2-BLE)
```bash
# Arduino IDE
1. Abrir: Firmware_ESP32_Atuador_v4.2_COM_BLE.ino
2. Selecionar: Placa "ESP32 Dev Module"
3. Incluir biblioteca: BLE (built-in do ESP32)
4. Compilar e enviar
```

## 🔍 Monitoramento no Dashboard

O dashboard agora exibe:
- ✅ **Badge "BLE"** em firmwares com suporte
- 📊 **Status de conexão** WiFi e MQTT
- 🔋 **Memória livre** com barra de progresso
- 📡 **Qualidade do sinal** WiFi (Excelente/Bom/Fraco)
- 🔬 **Status individual dos sensores** (pH ✓, EC ✓)

## ⚠️ Notas Importantes

1. **BLE não substitui MQTT**: É apenas um fallback temporário
2. **Dados limitados**: Apenas pH, temperatura da água e EC via BLE
3. **Pareamento automático**: Módulos se conectam automaticamente
4. **Interferência**: Evite usar microondas/WiFi 2.4GHz próximos durante operação BLE
5. **Bateria**: BLE consome menos energia, ideal para backup

## 📈 Próximos Passos

- ✅ Fase 2 concluída: BLE + TLS + Diagnósticos
- 🔜 Fase 3: OTA Updates + Calibração Remota
- 🔜 Integração com notificações push quando BLE for ativado

## 🆘 Troubleshooting

### BLE não ativa
1. Verificar se passaram 3 minutos sem MQTT
2. Conferir logs no Serial Monitor
3. Reiniciar ambos os módulos

### Cliente não conecta ao Server
1. Verificar se os UUIDs são idênticos
2. Reduzir distância entre módulos (<10m)
3. Desligar outros dispositivos BLE próximos

### Checksum sempre falha
1. Verificar se struct BLEData é idêntica em ambos firmwares
2. Verificar alinhamento de memória (packed)
3. Recompilar ambos os firmwares

---

**Documentação completa da Fase 2** ✨
