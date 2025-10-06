// Configuração MQTT - Credenciais HiveMQ Cloud
export const MQTT_CONFIG = {
  broker: 'wss://8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud:8884/mqtt',
  username: 'esp32-user',
  password: 'HydroSmart123',
  clientId: `aquasys-web-${Math.random().toString(16).slice(3)}`,
  topics: {
    sensors: 'aquasys/sensors/all',
    relayStatus: 'aquasys/relay/status',
    relayCommands: 'aquasys/relay/commands',
  },
};
