# Ponte MQTT-HTTP para AquaSys

Este script conecta o HiveMQ Cloud ao sistema Lovable, recebendo dados dos sensores e status dos relés via MQTT e enviando para o backend.

## Instalação

1. Instale o Python 3.7 ou superior
2. Instale as dependências:
```bash
pip install -r requirements.txt
```

## Configuração

Configure as variáveis de ambiente com suas credenciais do HiveMQ:

### Linux/Mac:
```bash
export MQTT_BROKER="seu-broker.hivemq.cloud"
export MQTT_PORT="8883"
export MQTT_USERNAME="seu-usuario"
export MQTT_PASSWORD="sua-senha"
```

### Windows (PowerShell):
```powershell
$env:MQTT_BROKER="seu-broker.hivemq.cloud"
$env:MQTT_PORT="8883"
$env:MQTT_USERNAME="seu-usuario"
$env:MQTT_PASSWORD="sua-senha"
```

### Windows (CMD):
```cmd
set MQTT_BROKER=seu-broker.hivemq.cloud
set MQTT_PORT=8883
set MQTT_USERNAME=seu-usuario
set MQTT_PASSWORD=sua-senha
```

## Execução

```bash
python bridge.py
```

## O que faz

1. **Conecta ao HiveMQ Cloud** usando SSL/TLS
2. **Subscreve aos tópicos**:
   - `aquasys/sensors/all` - dados dos sensores
   - `aquasys/relay/status` - status dos relés
3. **Envia dados para o backend** via HTTP POST
4. **Exibe logs** de todas as operações

## Teste

Para testar, você pode usar o botão "Inserir Dados de Teste" no dashboard, ou publicar mensagens manualmente no HiveMQ.

Formato esperado das mensagens:

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

## Próximos Passos

Após a ponte estar funcionando, você precisará:

1. **Configurar os relés** no dashboard (clique no ícone de configuração em cada relé)
2. **Definir os modos de operação** (LED, Ciclo, pH, Temperatura, etc.)
3. **Implementar no ESP32** a leitura da tabela `relay_commands` para controle manual

## Manter Rodando 24/7

Para produção, considere usar:
- **Linux**: `systemd` service ou `supervisor`
- **Windows**: Executar como serviço com `NSSM`
- **Docker**: Containerizar o script
- **Cloud**: Deploy em serviço como Heroku, Railway, ou Render
