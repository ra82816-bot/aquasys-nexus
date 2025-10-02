import paho.mqtt.client as mqtt
import requests
import json
import time
import os
import threading

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
                    # Publicar comando no MQTT
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

def main():
    global mqtt_client
    
    print("=== Ponte MQTT-HTTP AquaSys ===")
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
    
    # Conectar ao broker
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        # Manter o cliente rodando
        print("Ponte ativa. Aguardando mensagens...\n")
        client.loop_forever()
        
    except Exception as e:
        print(f"Erro ao conectar: {e}")
        return

if __name__ == "__main__":
    main()
