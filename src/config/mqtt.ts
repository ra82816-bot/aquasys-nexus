// Configuração MQTT - Atualize com suas credenciais do HiveMQ Cloud
export const MQTT_CONFIG = {
  broker: 'wss://your-cluster.s1.eu.hivemq.cloud:8884/mqtt', // Substitua pelo seu broker
  username: 'your-username', // Substitua pelo seu username
  password: 'your-password', // Substitua pela sua senha
  clientId: `aquasys-web-${Math.random().toString(16).slice(3)}`,
  topics: {
    sensors: 'aquasys/sensors/all',
    relayStatus: 'aquasys/relay/status',
    relayCommands: 'aquasys/relay/commands',
  },
};
