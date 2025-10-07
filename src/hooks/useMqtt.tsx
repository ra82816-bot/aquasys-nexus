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
  const clientRef = useRef<MqttClient | null>(null);
  const { toast } = useToast();

  const connect = useCallback(() => {
    if (clientRef.current?.connected) {
      console.log('MQTT já conectado');
      return;
    }

    console.log('Conectando ao broker MQTT...');
    
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
      
      // Subscribe nos tópicos relevantes
      const topics = [
        MQTT_CONFIG.topics.sensors,
        MQTT_CONFIG.topics.relayStatus,
      ];
      
      client.subscribe(topics, { qos: 1 }, (err) => {
        if (err) {
          console.error('Erro ao subscrever:', err);
          toast({
            title: 'Erro MQTT',
            description: 'Falha ao subscrever nos tópicos',
            variant: 'destructive',
          });
        } else {
          console.log('✅ Inscrito nos tópicos:', topics);
        }
      });
    });

    client.on('message', async (topic, payload) => {
      try {
        const data = JSON.parse(payload.toString());
        const message: MqttMessage = {
          topic,
          payload: data,
          timestamp: new Date(),
        };
        
        console.log('📩 Mensagem recebida:', { topic, data });
        setLastMessage(message);

        // Salvar dados automaticamente no banco via Edge Function
        if (topic === MQTT_CONFIG.topics.sensors) {
          await saveSensorData(data);
        } else if (topic === MQTT_CONFIG.topics.relayStatus) {
          await saveRelayStatus(data);
        }
      } catch (error) {
        console.error('Erro ao processar mensagem:', error);
      }
    });

    client.on('error', (error) => {
      console.error('❌ Erro MQTT:', error);
      toast({
        title: 'Erro de Conexão',
        description: 'Falha na conexão MQTT',
        variant: 'destructive',
      });
    });

    client.on('disconnect', () => {
      console.log('⚠️ MQTT desconectado');
      setIsConnected(false);
    });

    client.on('reconnect', () => {
      console.log('🔄 Tentando reconectar...');
    });

    client.on('offline', () => {
      console.log('📡 Cliente offline');
      setIsConnected(false);
    });

    clientRef.current = client;
  }, [toast]);

  const disconnect = useCallback(() => {
    if (clientRef.current) {
      clientRef.current.end();
      clientRef.current = null;
      setIsConnected(false);
    }
  }, []);

  const publish = useCallback(
    (topic: string, message: any, options = { qos: 1 as 0 | 1 | 2 }) => {
      return new Promise<void>((resolve, reject) => {
        if (!clientRef.current?.connected) {
          reject(new Error('MQTT não conectado'));
          return;
        }

        const payload = typeof message === 'string' ? message : JSON.stringify(message);
        
        clientRef.current.publish(topic, payload, options, (error) => {
          if (error) {
            console.error('Erro ao publicar:', error);
            reject(error);
          } else {
            console.log('✅ Mensagem publicada:', { topic, message });
            resolve();
          }
        });
      });
    },
    []
  );

  const saveSensorData = useCallback(async (data: any) => {
    try {
      console.log('💾 Salvando dados de sensores no banco...');
      
      const { data: result, error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          action: 'process_sensors',
          data: {
            ph: data.ph,
            ec: data.ec,
            airTemp: data.air_temp || data.airTemp,
            humidity: data.humidity,
            waterTemp: data.water_temp || data.waterTemp
          }
        }
      });

      if (error) {
        console.error('❌ Erro ao salvar sensores:', error);
      } else {
        console.log('✅ Dados de sensores salvos:', result);
      }
    } catch (error) {
      console.error('❌ Erro ao chamar edge function:', error);
    }
  }, []);

  const saveRelayStatus = useCallback(async (data: any) => {
    try {
      console.log('💾 Salvando status dos relés no banco...');
      
      const { data: result, error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          action: 'process_relay_status',
          data: {
            relay1_led: data.relay1_led ?? false,
            relay2_pump: data.relay2_pump ?? false,
            relay3_ph_up: data.relay3_ph_up ?? false,
            relay4_fan: data.relay4_fan ?? false,
            relay5_humidity: data.relay5_humidity ?? false,
            relay6_ec: data.relay6_ec ?? false,
            relay7_co2: data.relay7_co2 ?? false,
            relay8_generic: data.relay8_generic ?? false
          }
        }
      });

      if (error) {
        console.error('❌ Erro ao salvar status dos relés:', error);
      } else {
        console.log('✅ Status dos relés salvo:', result);
      }
    } catch (error) {
      console.error('❌ Erro ao chamar edge function:', error);
    }
  }, []);

  const publishRelayCommand = useCallback(
    async (relayIndex: number, command: boolean) => {
      const message = {
        relay_index: relayIndex,
        command: command,
        timestamp: new Date().toISOString(),
      };

      try {
        await publish(MQTT_CONFIG.topics.relayCommands, message);
        toast({
          title: 'Comando enviado',
          description: `Relé ${relayIndex} → ${command ? 'ON' : 'OFF'}`,
        });
      } catch (error) {
        toast({
          title: 'Erro ao enviar comando',
          description: 'Falha na comunicação MQTT',
          variant: 'destructive',
        });
        throw error;
      }
    },
    [publish, toast]
  );

  useEffect(() => {
    connect();
    return () => disconnect();
  }, [connect, disconnect]);

  return {
    isConnected,
    lastMessage,
    publish,
    publishRelayCommand,
    connect,
    disconnect,
    client: clientRef.current,
  };
};
