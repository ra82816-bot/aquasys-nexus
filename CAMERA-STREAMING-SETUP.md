# Configuração de Streaming de Câmera IP

Este guia explica como configurar o streaming da câmera IP no AquaSys usando RTSP → HLS.

## Arquitetura

```
Câmera IP (RTSP) → Bridge Python (FFmpeg) → HLS → App React (hls.js)
```

## Pré-requisitos

### 1. FFmpeg
O FFmpeg deve estar instalado no sistema onde o bridge Python roda:

**Linux:**
```bash
sudo apt-get update
sudo apt-get install ffmpeg
```

**macOS:**
```bash
brew install ffmpeg
```

**Windows:**
Baixe de [ffmpeg.org](https://ffmpeg.org/download.html) e adicione ao PATH

### 2. Dependências Python
```bash
cd mqtt-bridge
pip install -r requirements.txt
```

## Configuração

### 1. Variáveis de Ambiente

Crie um arquivo `.env` no diretório `mqtt-bridge/` ou configure as variáveis:

```bash
# Configurações da Câmera
CAMERA_IP=192.168.0.17
CAMERA_USER=admin
CAMERA_PASS=Crepaldi
CAMERA_RTSP_PATH=stream1

# Configurações MQTT (já existentes)
MQTT_BROKER=8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud
MQTT_PORT=8883
MQTT_USERNAME=esp32-user
MQTT_PASSWORD=HydroSmart123

# Supabase (já existentes)
SUPABASE_URL=https://oaabtbvwxsjomeeizciq.supabase.co
SUPABASE_ANON_KEY=...
```

### 2. URLs RTSP Comuns

Dependendo da marca da câmera, o path RTSP pode variar:

**Hikvision:**
```
rtsp://admin:senha@192.168.0.17:554/Streaming/Channels/101
```

**Dahua:**
```
rtsp://admin:senha@192.168.0.17:554/cam/realmonitor?channel=1&subtype=0
```

**Genérico:**
```
rtsp://admin:senha@192.168.0.17:554/stream1
rtsp://admin:senha@192.168.0.17:554/live
```

**Testar URL RTSP:**
```bash
ffplay -rtsp_transport tcp rtsp://admin:Crepaldi@192.168.0.17:554/stream1
```

## Executando

### 1. Iniciar o Bridge
```bash
cd mqtt-bridge
python bridge.py
```

O bridge irá:
- Conectar ao broker MQTT (HiveMQ)
- Iniciar conversão RTSP → HLS com FFmpeg
- Servir os segmentos HLS em `http://localhost:5000`

### 2. Logs Esperados
```
=== Ponte MQTT-HTTP + Streaming AquaSys ===
Conectando ao broker: 8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud:8883

=== Iniciando conversão RTSP → HLS ===
RTSP: rtsp://***:***@192.168.0.17:554/stream1
HLS Dir: /path/to/mqtt-bridge/hls_stream
✓ FFmpeg iniciado com sucesso

=== Iniciando servidor HLS ===
Servidor disponível em: http://localhost:5000
✓ Servidor HLS iniciado

Conectado ao HiveMQ com código: 0
Subscrito a aquasys/sensors/all e aquasys/relay/status
✓ Thread de verificação de comandos iniciada

Ponte ativa. Aguardando mensagens MQTT...
```

### 3. Acessar no App
1. Navegue para `/camera` no app
2. O stream será carregado automaticamente
3. Se necessário, configure a URL em Configurações

## Troubleshooting

### Stream não conecta
1. Verifique se o bridge está rodando
2. Teste a URL RTSP com `ffplay`
3. Verifique os logs do FFmpeg no console do bridge
4. Confirme que a câmera está acessível na rede

### Erro "FFmpeg not found"
- Instale o FFmpeg conforme instruções acima
- Verifique se está no PATH: `ffmpeg -version`

### Latência alta
Ajuste os parâmetros FFmpeg no `bridge.py`:
```python
'-hls_time', '1',  # Reduzir para 1 segundo
'-hls_list_size', '3',  # Manter menos segmentos
```

### Stream trava ou para
- O FFmpeg tentará reconectar automaticamente
- Verifique a estabilidade da rede
- Considere usar TCP em vez de UDP: `-rtsp_transport tcp`

## Otimizações

### Performance
- Use `-c:v copy` para não recodificar vídeo (mais rápido)
- Ajuste `-hls_time` para balancear latência vs. estabilidade
- Configure `-b:v` para limitar bitrate se necessário

### Qualidade
Para forçar recodificação com qualidade específica:
```python
'-c:v', 'libx264',
'-preset', 'ultrafast',
'-tune', 'zerolatency',
'-b:v', '2M',
```

## Múltiplas Câmeras

Para adicionar mais câmeras, expanda o bridge com múltiplas instâncias FFmpeg:

```python
cameras = [
    {"id": "cam1", "rtsp": "rtsp://...", "port": 5001},
    {"id": "cam2", "rtsp": "rtsp://...", "port": 5002},
]
```

## Segurança

⚠️ **IMPORTANTE:**
- Nunca exponha o servidor HLS diretamente na internet
- Use VPN ou túnel seguro para acesso remoto
- Considere adicionar autenticação no endpoint Flask
- Mantenha as credenciais em variáveis de ambiente

## Próximos Passos

- [ ] Adicionar autenticação ao servidor HLS
- [ ] Implementar seleção de múltiplas câmeras no app
- [ ] Adicionar gravação de clips
- [ ] Implementar detecção de movimento
- [ ] Adicionar suporte a PTZ (Pan-Tilt-Zoom)
