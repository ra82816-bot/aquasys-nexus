import paho.mqtt.client as mqtt
import requests
import json
import time
import os
import threading
import subprocess
from flask import Flask, send_from_directory, jsonify
from flask_cors import CORS

# Configurações HiveMQ Cloud
MQTT_BROKER = os.getenv("MQTT_BROKER", "8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud")
MQTT_PORT = int(os.getenv("MQTT_PORT", "8883"))
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "esp32-user")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "HydroSmart123")

# Supabase
SUPABASE_URL = os.getenv("SUPABASE_URL", "https://oaabtbvwxsjomeeizciq.supabase.co")
SUPABASE_ANON_KEY = os.getenv("SUPABASE_ANON_KEY", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9hYWJ0YnZ3eHNqb21lZWl6Y2lxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTkzNzI4NzEsImV4cCI6MjA3NDk0ODg3MX0.ZcCr9BFJPMNfy409gkK8VucnfXhluX82LJ8f4HI4bPw")

# Edge Functions
EDGE_FUNCTION_URL = f"{SUPABASE_URL}/functions/v1/mqtt-collector"

# Tópicos MQTT
TOPIC_SENSORS = "aquasys/sensors/all"
TOPIC_RELAY_STATUS = "aquasys/relay/status"
TOPIC_RELAY_COMMANDS = "aquasys/relay/commands"

# Cliente MQTT global
mqtt_client = None

# Configurações de streaming
CAMERA_IP = os.getenv("CAMERA_IP", "192.168.0.17")
CAMERA_USER = os.getenv("CAMERA_USER", "admin")
CAMERA_PASS = os.getenv("CAMERA_PASS", "Crepaldi")
CAMERA_RTSP_PATH = os.getenv("CAMERA_RTSP_PATH", "stream1")
RTSP_URL = f"rtsp://{CAMERA_USER}:{CAMERA_PASS}@{CAMERA_IP}:554/{CAMERA_RTSP_PATH}"

# Diretório para armazenar segmentos HLS
HLS_DIR = os.path.join(os.path.dirname(__file__), "hls_stream")
os.makedirs(HLS_DIR, exist_ok=True)

# Processo FFmpeg global
ffmpeg_process = None

# Flask app para servir HLS
app = Flask(__name__)
CORS(app, resources={r"/*": {"origins": "*"}})

@app.route('/stream/<path:filename>')
def serve_hls(filename):
    """Serve arquivos HLS (.m3u8 e .ts)"""
    return send_from_directory(HLS_DIR, filename)

@app.route('/health')
def health():
    """Endpoint de health check"""
    return jsonify({
        "status": "online",
        "ffmpeg_running": ffmpeg_process is not None and ffmpeg_process.poll() is None,
        "rtsp_url": f"rtsp://***:***@{CAMERA_IP}:554/{CAMERA_RTSP_PATH}"
    })

def on_connect(client, userdata, flags, rc):
    print(f"Conectado ao HiveMQ com código: {rc}")
    if rc == 0:
        # Subscrever aos tópicos
        client.subscribe(TOPIC_SENSORS)
        client.subscribe(TOPIC_RELAY_STATUS)
        print(f"Subscrito a {TOPIC_SENSORS} e {TOPIC_RELAY_STATUS}")
    else:
        print(f"Falha na conexão, código: {rc}")

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        print(f"\nMensagem recebida no tópico {msg.topic}:")
        print(json.dumps(payload, indent=2))
        
        # Determinar a ação baseado no tópico
        if msg.topic == TOPIC_SENSORS:
            send_to_edge_function("process_sensors", payload)
        elif msg.topic == TOPIC_RELAY_STATUS:
            send_to_edge_function("process_relay_status", payload)
            
    except Exception as e:
        print(f"Erro ao processar mensagem: {e}")

def send_to_edge_function(action, data):
    try:
        payload = {
            "action": action,
            "data": data
        }
        
        response = requests.post(
            EDGE_FUNCTION_URL,
            json=payload,
            headers={"Content-Type": "application/json"},
            timeout=10
        )
        
        if response.status_code == 200:
            print(f"✓ Dados enviados com sucesso ({action})")
        else:
            print(f"✗ Erro ao enviar dados: {response.status_code} - {response.text}")
            
    except Exception as e:
        print(f"✗ Erro na requisição HTTP: {e}")

def check_pending_commands():
    """Verifica comandos pendentes no banco e publica no MQTT"""
    global mqtt_client
    
    while True:
        try:
            # Buscar comandos não executados
            response = requests.get(
                f"{SUPABASE_URL}/rest/v1/relay_commands",
                params={
                    "executed": "eq.false",
                    "order": "timestamp.asc",
                    "limit": "10"
                },
                headers={
                    "apikey": SUPABASE_ANON_KEY,
                    "Authorization": f"Bearer {SUPABASE_ANON_KEY}"
                },
                timeout=5
            )
            
            if response.status_code == 200:
                commands = response.json()
                
                for cmd in commands:
                    # Verificar se é um comando de ping (relay_index = -1)
                    if cmd["relay_index"] == -1:
                        print("🔔 Ping recebido! Solicitando dados ao ESP32...")
                        
                        # Publicar comando de ping no MQTT
                        ping_msg = {"action": "ping"}
                        
                        if mqtt_client:
                            mqtt_client.publish(
                                TOPIC_RELAY_COMMANDS,
                                json.dumps(ping_msg),
                                qos=1
                            )
                            print("📤 Comando de ping enviado ao ESP32")
                    else:
                        # Comando normal de relé
                        command_msg = {
                            "relay_index": cmd["relay_index"],
                            "command": cmd["command"]
                        }
                        
                        if mqtt_client:
                            mqtt_client.publish(
                                TOPIC_RELAY_COMMANDS,
                                json.dumps(command_msg),
                                qos=1
                            )
                            print(f"📤 Comando publicado: Relé {cmd['relay_index']} -> {cmd['command']}")
                    
                    # Marcar como executado
                    update_response = requests.patch(
                        f"{SUPABASE_URL}/rest/v1/relay_commands",
                        params={"id": f"eq.{cmd['id']}"},
                        json={"executed": True},
                        headers={
                            "apikey": SUPABASE_ANON_KEY,
                            "Authorization": f"Bearer {SUPABASE_ANON_KEY}",
                            "Content-Type": "application/json",
                            "Prefer": "return=minimal"
                        },
                        timeout=5
                    )
                    
                    if update_response.status_code in [200, 204]:
                        print(f"✓ Comando {cmd['id']} marcado como executado")
                    
        except Exception as e:
            print(f"⚠ Erro ao verificar comandos: {e}")
        
        # Verificar a cada 2 segundos
        time.sleep(2)

def start_ffmpeg_stream():
    """Inicia o processo FFmpeg para converter RTSP → HLS"""
    global ffmpeg_process
    
    print(f"\n=== Iniciando conversão RTSP → HLS ===")
    print(f"RTSP: {RTSP_URL}")
    print(f"HLS Dir: {HLS_DIR}")
    
    # Limpar arquivos HLS antigos
    for file in os.listdir(HLS_DIR):
        if file.endswith(('.ts', '.m3u8')):
            os.remove(os.path.join(HLS_DIR, file))
    
    # Comando FFmpeg otimizado para streaming
    ffmpeg_cmd = [
        'ffmpeg',
        '-rtsp_transport', 'tcp',  # Usar TCP para RTSP (mais estável)
        '-i', RTSP_URL,
        '-c:v', 'copy',  # Copiar codec de vídeo (sem recodificação)
        '-c:a', 'aac',   # Codec de áudio AAC
        '-f', 'hls',     # Formato HLS
        '-hls_time', '2',  # Duração de cada segmento (2 segundos)
        '-hls_list_size', '5',  # Manter 5 segmentos na playlist
        '-hls_flags', 'delete_segments+append_list',  # Deletar segmentos antigos
        '-hls_segment_filename', os.path.join(HLS_DIR, 'segment_%03d.ts'),
        os.path.join(HLS_DIR, 'stream.m3u8')
    ]
    
    try:
        ffmpeg_process = subprocess.Popen(
            ffmpeg_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        print("✓ FFmpeg iniciado com sucesso")
        
        # Thread para monitorar FFmpeg
        def monitor_ffmpeg():
            while True:
                if ffmpeg_process.poll() is not None:
                    print("⚠ FFmpeg encerrado inesperadamente. Reiniciando...")
                    time.sleep(5)
                    start_ffmpeg_stream()
                    break
                time.sleep(10)
        
        threading.Thread(target=monitor_ffmpeg, daemon=True).start()
        
    except Exception as e:
        print(f"✗ Erro ao iniciar FFmpeg: {e}")
        print("Certifique-se de que o FFmpeg está instalado: sudo apt-get install ffmpeg")

def start_flask_server():
    """Inicia o servidor Flask em uma thread separada"""
    print("\n=== Iniciando servidor HLS ===")
    print("Servidor disponível em: http://localhost:5000")
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)

def main():
    global mqtt_client
    
    print("=== Ponte MQTT-HTTP + Streaming AquaSys ===")
    print(f"Conectando ao broker: {MQTT_BROKER}:{MQTT_PORT}")
    
    # Criar cliente MQTT
    client = mqtt.Client()
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.tls_set()  # Habilitar SSL/TLS
    
    # Configurar callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    
    mqtt_client = client
    
    # Iniciar thread para verificar comandos pendentes
    command_thread = threading.Thread(target=check_pending_commands, daemon=True)
    command_thread.start()
    print("✓ Thread de verificação de comandos iniciada")
    
    # Iniciar streaming de câmera
    start_ffmpeg_stream()
    
    # Iniciar servidor Flask em thread separada
    flask_thread = threading.Thread(target=start_flask_server, daemon=True)
    flask_thread.start()
    print("✓ Servidor HLS iniciado")
    
    # Conectar ao broker
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        # Manter o cliente rodando
        print("\nPonte ativa. Aguardando mensagens MQTT...\n")
        client.loop_forever()
        
    except Exception as e:
        print(f"Erro ao conectar: {e}")
        return

if __name__ == "__main__":
    main()
