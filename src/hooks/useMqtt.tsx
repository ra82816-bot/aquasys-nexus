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
        'aquasys/relay/status/wifi',
      ];
      
      client.subscribe(topics, { qos: 1 }, (err) => {
        if (err) {
          console.error('Erro ao subscrever:', err);
          // Não mostrar toast de erro automaticamente
          // O status será exibido no componente MqttStatus
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
      // Não mostrar toast de erro automaticamente
      // O status será exibido no componente MqttStatus
      setIsConnected(false);
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
      
      // Mapear campos do firmware (temperature/waterTemp) para o formato esperado
      const sensorPayload = {
        ph: data.ph,
        ec: data.ec,
        // Aceitar: temperature (firmware v2.5), airTemp (legado), air_temp (banco)
        airTemp: data.temperature || data.airTemp || data.air_temp,
        humidity: data.humidity,
        // Aceitar: waterTemp (firmware v2.5), water_temp (banco)
        waterTemp: data.waterTemp || data.water_temp
      };
      
      const { data: result, error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          action: 'process_sensors',
          data: sensorPayload
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
      console.log('💾 Salvando status dos relés no banco...', data);
      
      // Mapear relay1, relay2, etc. para relay1_led, relay2_pump, etc.
      const mappedData = {
        relay1_led: data.relay1 ?? false,
        relay2_pump: data.relay2 ?? false,
        relay3_ph_up: data.relay3 ?? false,
        relay4_fan: data.relay4 ?? false,
        relay5_humidity: data.relay5 ?? false,
        relay6_ec: data.relay6 ?? false,
        relay7_co2: data.relay7 ?? false,
        relay8_generic: data.relay8 ?? false
      };

      console.log('💾 Dados mapeados para salvar:', mappedData);
      
      const { data: result, error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          action: 'process_relay_status',
          data: mappedData
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
      // Formato simplificado compatível com firmware v3.4+
      const message = {
        relay: relayIndex + 1, // ESP32 usa 1-8, não 0-7
        command: command // boolean direto
      };

      try {
        await publish(MQTT_CONFIG.topics.relayCommand, message);
        toast({
          title: 'Comando enviado',
          description: `Relé ${relayIndex + 1} → ${command ? 'LIGADO' : 'DESLIGADO'}`,
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

  const publishRelayConfig = useCallback(
    async (relayIndex: number, config: any) => {
      // Formato simplificado compatível com firmware v3.4+
      const message = {
        relay: relayIndex + 1, // ESP32 usa 1-8
        config: config
      };

      try {
        await publish(MQTT_CONFIG.topics.relayCommand, message);
        console.log('✅ Configuração enviada via MQTT para relé', relayIndex);
        toast({
          title: 'Configuração enviada',
          description: `Relé ${relayIndex + 1} configurado com sucesso`,
        });
      } catch (error) {
        console.error('❌ Erro ao enviar configuração:', error);
        toast({
          title: 'Erro ao enviar configuração',
          description: 'Falha na comunicação MQTT',
          variant: 'destructive',
        });
        throw error;
      }
    },
    [publish, toast]
  );

  const setRelayAuto = useCallback(
    async (relayIndex: number) => {
      // Formato simplificado compatível com firmware v3.4+
      const message = {
        relay: relayIndex + 1, // ESP32 usa 1-8
        auto: true
      };

      try {
        await publish(MQTT_CONFIG.topics.relayCommand, message);
        toast({
          title: 'Modo automático',
          description: `Relé ${relayIndex + 1} retornado ao modo automático`,
        });
      } catch (error) {
        toast({
          title: 'Erro',
          description: 'Falha ao definir modo automático',
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
    publishRelayConfig,
    setRelayAuto,
    connect,
    disconnect,
    client: clientRef.current,
  };
};
