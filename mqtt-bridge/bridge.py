import paho.mqtt.client as mqtt
import requests
import json
import time
import os

# Configurações HiveMQ Cloud
MQTT_BROKER = os.getenv("MQTT_BROKER", "seu-broker.hivemq.cloud")
MQTT_PORT = int(os.getenv("MQTT_PORT", "8883"))
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "seu-usuario")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "sua-senha")

# URL do Edge Function
EDGE_FUNCTION_URL = "https://oaabtbvwxsjomeeizciq.supabase.co/functions/v1/mqtt-collector"

# Tópicos MQTT
TOPIC_SENSORS = "aquasys/sensors/all"
TOPIC_RELAY_STATUS = "aquasys/relay/status"

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

def main():
    print("=== Ponte MQTT-HTTP AquaSys ===")
    print(f"Conectando ao broker: {MQTT_BROKER}:{MQTT_PORT}")
    
    # Criar cliente MQTT
    client = mqtt.Client()
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.tls_set()  # Habilitar SSL/TLS
    
    # Configurar callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    
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
