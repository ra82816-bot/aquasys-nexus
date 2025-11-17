import { useEffect, useRef, useState, useCallback } from 'react';
import mqtt, { MqttClient } from 'mqtt';
import { MQTT_CONFIG } from '@/config/mqtt';
import { useToast } from '@/hooks/use-toast';
import { supabase } from '@/integrations/supabase/client';

export interface MqttMessage {
  topic: string;
  payload: any;
  timestamp: Date;
}

export const useMqtt = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [lastMessage, setLastMessage] = useState<MqttMessage | null>(null);
  const [lastSensorUpdate, setLastSensorUpdate] = useState<number>(0);
  const [sensorTimeout, setSensorTimeout] = useState(false);
  const clientRef = useRef<MqttClient | null>(null);
  const { toast } = useToast();

  const connect = useCallback(() => {
    if (clientRef.current?.connected) {
      console.log('MQTT já conectado');
      return;
    }

    console.log('🔌 Conectando ao broker MQTT...');
    console.log('📡 Tópicos fixos:', MQTT_CONFIG.topics);
    
    const client = mqtt.connect(MQTT_CONFIG.broker, {
      clientId: MQTT_CONFIG.clientId,
      username: MQTT_CONFIG.username,
      password: MQTT_CONFIG.password,
      clean: true,
      reconnectPeriod: 5000,
      connectTimeout: 30000,
      keepalive: 60,
    });

    client.on('connect', () => {
      console.log('✅ MQTT conectado!');
      setIsConnected(true);
      
      // Subscribe nos tópicos fixos
      const topics = [
        MQTT_CONFIG.topics.sensors,      // aquasys/sensors/all
        MQTT_CONFIG.topics.relayStatus,  // aquasys/relay/status
      ];
      
      client.subscribe(topics, { qos: 1 }, (err) => {
        if (err) {
          console.error('❌ Erro ao subscrever:', err);
        } else {
          console.log('✅ Inscrito nos tópicos:', topics);
          toast({
            title: "MQTT Conectado",
            description: "Comunicação com ESP32 estabelecida",
          });
        }
      });
    });

    client.on('message', async (topic, payload) => {
      try {
        const message = JSON.parse(payload.toString());
        console.log(`📨 [${topic}]`, message);
        
        const mqttMessage: MqttMessage = {
          topic,
          payload: message,
          timestamp: new Date(),
        };
        setLastMessage(mqttMessage);

        // Processar mensagens de sensores
        if (topic === MQTT_CONFIG.topics.sensors) {
          setLastSensorUpdate(Date.now());
          setSensorTimeout(false);
          await saveSensorData(message);
        }
        
        // Processar status dos relés
        if (topic === MQTT_CONFIG.topics.relayStatus) {
          await saveRelayStatus(message);
        }
      } catch (error) {
        console.error('❌ Erro ao processar mensagem:', error);
      }
    });

    client.on('error', (error) => {
      console.error('❌ Erro MQTT:', error);
      setIsConnected(false);
    });

    client.on('offline', () => {
      console.log('📴 MQTT offline');
      setIsConnected(false);
    });

    client.on('reconnect', () => {
      console.log('🔄 Reconectando ao MQTT...');
    });

    client.on('close', () => {
      console.log('🔌 Conexão MQTT fechada');
      setIsConnected(false);
    });

    clientRef.current = client;
  }, [toast]);

  const disconnect = useCallback(() => {
    if (clientRef.current) {
      console.log('🔌 Desconectando MQTT...');
      clientRef.current.end();
      clientRef.current = null;
      setIsConnected(false);
    }
  }, []);

  const publish = useCallback(async (topic: string, message: any, options = { qos: 1 as 0 | 1 | 2 }) => {
    return new Promise<void>((resolve, reject) => {
      if (!clientRef.current?.connected) {
        console.error('❌ MQTT não conectado');
        reject(new Error('MQTT não conectado'));
        return;
      }

      const payload = typeof message === 'string' ? message : JSON.stringify(message);
      
      clientRef.current.publish(topic, payload, options, (error) => {
        if (error) {
          console.error('❌ Erro ao publicar:', error);
          reject(error);
        } else {
          console.log(`📤 Publicado em [${topic}]:`, message);
          resolve();
        }
      });
    });
  }, []);

  // ✅ Publicar comando de relé usando tópico fixo
  const publishRelayCommand = useCallback(async (relayIndex: number, command: boolean) => {
    const relayNumber = relayIndex + 1;
    const message = {
      relay: relayNumber,
      command: command,
      timestamp: new Date().toISOString(),
    };

    try {
      await publish(MQTT_CONFIG.topics.relayCommand, message);
      
      toast({
        title: command ? "Relé Ativado" : "Relé Desativado",
        description: `Relé ${relayNumber} ${command ? 'ligado' : 'desligado'}`,
      });

      // Retry com backoff se falhar
      let retries = 0;
      const maxRetries = 3;
      while (retries < maxRetries) {
        await new Promise(resolve => setTimeout(resolve, 1000 * (retries + 1)));
        try {
          await publish(MQTT_CONFIG.topics.relayCommand, message);
          break;
        } catch (error) {
          retries++;
          if (retries === maxRetries) {
            throw error;
          }
        }
      }
    } catch (error) {
      console.error('❌ Erro ao publicar comando:', error);
      toast({
        title: "Erro ao Controlar Relé",
        description: "Verifique a conexão MQTT",
        variant: "destructive",
      });
    }
  }, [publish, toast]);

  // ✅ Publicar configuração de relé usando tópico fixo
  const publishRelayConfig = useCallback(async (relayIndex: number, config: any) => {
    const relayNumber = relayIndex + 1;
    const message = {
      relay: relayNumber,
      config: config,
      timestamp: new Date().toISOString(),
    };

    try {
      await publish(MQTT_CONFIG.topics.relayCommand, message);
      
      toast({
        title: "Configuração Atualizada",
        description: `Relé ${relayNumber} configurado com sucesso`,
      });
    } catch (error) {
      console.error('❌ Erro ao publicar config:', error);
      toast({
        title: "Erro ao Configurar Relé",
        description: "Verifique a conexão MQTT",
        variant: "destructive",
      });
    }
  }, [publish, toast]);

  // ✅ Ativar modo automático do relé
  const setRelayAuto = useCallback(async (relayIndex: number) => {
    const relayNumber = relayIndex + 1;
    const message = {
      relay: relayNumber,
      auto: true,
      timestamp: new Date().toISOString(),
    };

    try {
      await publish(MQTT_CONFIG.topics.relayCommand, message);
      
      toast({
        title: "Modo Automático",
        description: `Relé ${relayNumber} em modo automático`,
      });
    } catch (error) {
      console.error('❌ Erro ao ativar modo auto:', error);
      toast({
        title: "Erro ao Ativar Automático",
        description: "Verifique a conexão MQTT",
        variant: "destructive",
      });
    }
  }, [publish, toast]);

  // Salvar dados de sensores no Supabase
  const saveSensorData = async (data: any) => {
    try {
      const { error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          topic: MQTT_CONFIG.topics.sensors,
          payload: data,
        },
      });

      if (error) {
        console.error('❌ Erro ao salvar dados de sensores:', error);
      } else {
        console.log('✅ Dados de sensores salvos');
      }
    } catch (error) {
      console.error('❌ Erro ao invocar mqtt-collector:', error);
    }
  };

  // Salvar status dos relés no Supabase
  const saveRelayStatus = async (data: any) => {
    try {
      const { error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          topic: MQTT_CONFIG.topics.relayStatus,
          payload: data,
        },
      });

      if (error) {
        console.error('❌ Erro ao salvar status dos relés:', error);
      } else {
        console.log('✅ Status dos relés salvo');
      }
    } catch (error) {
      console.error('❌ Erro ao invocar mqtt-collector:', error);
    }
  };

  // Timeout de sensores (30 segundos sem dados)
  useEffect(() => {
    if (lastSensorUpdate === 0) return;

    const timeout = setTimeout(() => {
      setSensorTimeout(true);
      toast({
        title: "Timeout de Sensores",
        description: "Nenhum dado recebido nos últimos 30 segundos",
        variant: "destructive",
      });
    }, 30000);

    return () => clearTimeout(timeout);
  }, [lastSensorUpdate, toast]);

  // Auto-conectar ao montar
  useEffect(() => {
    connect();
    return () => disconnect();
  }, [connect, disconnect]);

  return {
    isConnected,
    lastMessage,
    lastSensorUpdate,
    sensorTimeout,
    deviceTopics: MQTT_CONFIG.topics, // Retornar tópicos fixos
    publish,
    publishRelayCommand,
    publishRelayConfig,
    setRelayAuto,
    connect,
    disconnect,
    client: clientRef.current,
  };
};
