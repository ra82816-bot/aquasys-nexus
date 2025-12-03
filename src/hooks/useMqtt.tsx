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

interface ConnectionState {
  attempts: number;
  lastAttempt: Date | null;
  nextRetryIn: number;
}

const MAX_RETRY_ATTEMPTS = 10;
const INITIAL_RETRY_DELAY = 1000; // 1 second
const MAX_RETRY_DELAY = 60000; // 60 seconds

export const useMqtt = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [lastMessage, setLastMessage] = useState<MqttMessage | null>(null);
  const [connectionState, setConnectionState] = useState<ConnectionState>({
    attempts: 0,
    lastAttempt: null,
    nextRetryIn: INITIAL_RETRY_DELAY,
  });
  const clientRef = useRef<MqttClient | null>(null);
  const retryTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const isConnectingRef = useRef(false);
  const { toast } = useToast();

  // Calculate exponential backoff delay
  const getBackoffDelay = useCallback((attempt: number): number => {
    const delay = Math.min(
      INITIAL_RETRY_DELAY * Math.pow(2, attempt),
      MAX_RETRY_DELAY
    );
    // Add jitter (±20%)
    const jitter = delay * 0.2 * (Math.random() - 0.5);
    return Math.round(delay + jitter);
  }, []);

  const clearRetryTimeout = useCallback(() => {
    if (retryTimeoutRef.current) {
      clearTimeout(retryTimeoutRef.current);
      retryTimeoutRef.current = null;
    }
  }, []);

  const connect = useCallback(() => {
    // Prevent multiple simultaneous connection attempts
    if (isConnectingRef.current || clientRef.current?.connected) {
      console.log('MQTT: Conexão já em andamento ou conectado');
      return;
    }

    isConnectingRef.current = true;
    clearRetryTimeout();

    console.log(`🔌 MQTT: Tentativa de conexão #${connectionState.attempts + 1}`);
    
    const client = mqtt.connect(MQTT_CONFIG.broker, {
      clientId: MQTT_CONFIG.clientId,
      username: MQTT_CONFIG.username,
      password: MQTT_CONFIG.password,
      clean: true,
      reconnectPeriod: 0, // Disable auto-reconnect, we'll handle it manually
      connectTimeout: 15000, // Reduced from 30s to 15s
      keepalive: 60,
    });

    const connectionTimeout = setTimeout(() => {
      if (!client.connected) {
        console.log('⏱️ MQTT: Timeout de conexão');
        client.end(true);
        handleConnectionFailure();
      }
    }, 20000);

    client.on('connect', () => {
      clearTimeout(connectionTimeout);
      isConnectingRef.current = false;
      
      console.log('✅ MQTT conectado!');
      setIsConnected(true);
      setConnectionState({
        attempts: 0,
        lastAttempt: new Date(),
        nextRetryIn: INITIAL_RETRY_DELAY,
      });
      
      // Subscribe to relevant topics
      const topics = [
        MQTT_CONFIG.topics.sensors,
        MQTT_CONFIG.topics.relayStatus,
      ];
      
      client.subscribe(topics, { qos: 1 }, (err) => {
        if (err) {
          console.error('Erro ao subscrever:', err);
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
      clearTimeout(connectionTimeout);
      console.error('❌ Erro MQTT:', error.message);
      setIsConnected(false);
    });

    client.on('close', () => {
      clearTimeout(connectionTimeout);
      console.log('🔌 MQTT: Conexão fechada');
      setIsConnected(false);
      isConnectingRef.current = false;
      
      // Schedule reconnection with backoff
      if (connectionState.attempts < MAX_RETRY_ATTEMPTS) {
        scheduleReconnect();
      }
    });

    client.on('offline', () => {
      console.log('📡 Cliente offline');
      setIsConnected(false);
    });

    clientRef.current = client;

    function handleConnectionFailure() {
      isConnectingRef.current = false;
      setIsConnected(false);
      
      setConnectionState(prev => {
        const newAttempts = prev.attempts + 1;
        const nextDelay = getBackoffDelay(newAttempts);
        
        console.log(`⚠️ MQTT: Falha na conexão. Tentativa ${newAttempts}/${MAX_RETRY_ATTEMPTS}. Próxima em ${nextDelay/1000}s`);
        
        return {
          attempts: newAttempts,
          lastAttempt: new Date(),
          nextRetryIn: nextDelay,
        };
      });
    }

    function scheduleReconnect() {
      const delay = getBackoffDelay(connectionState.attempts);
      console.log(`🔄 MQTT: Reagendando reconexão em ${delay/1000}s`);
      
      retryTimeoutRef.current = setTimeout(() => {
        if (!clientRef.current?.connected) {
          connect();
        }
      }, delay);
    }
  }, [connectionState.attempts, clearRetryTimeout, getBackoffDelay]);

  const disconnect = useCallback(() => {
    clearRetryTimeout();
    isConnectingRef.current = false;
    
    if (clientRef.current) {
      clientRef.current.end(true);
      clientRef.current = null;
      setIsConnected(false);
    }
  }, [clearRetryTimeout]);

  // Manual reconnect - resets retry counter
  const reconnect = useCallback(() => {
    console.log('🔄 MQTT: Reconexão manual solicitada');
    disconnect();
    setConnectionState({
      attempts: 0,
      lastAttempt: null,
      nextRetryIn: INITIAL_RETRY_DELAY,
    });
    // Small delay before reconnecting
    setTimeout(() => connect(), 500);
  }, [disconnect, connect]);

  const publish = useCallback(
    (topic: string, message: any, options = { qos: 1 as 0 | 1 | 2 }) => {
      return new Promise<void>((resolve, reject) => {
        if (!clientRef.current?.connected) {
          console.error('🔴 MQTT: Não conectado - tentando reconectar');
          reconnect();
          reject(new Error('MQTT não conectado'));
          return;
        }

        const payload = typeof message === 'string' ? message : JSON.stringify(message);
        
        console.log('📤 MQTT PUBLISH:', topic, payload);
        
        clientRef.current.publish(topic, payload, options, (error) => {
          if (error) {
            console.error('❌ MQTT: Erro ao publicar:', error);
            reject(error);
          } else {
            console.log('✅ MQTT: Publicado com sucesso');
            resolve();
          }
        });
      });
    },
    [reconnect]
  );

  const saveSensorData = useCallback(async (data: any) => {
    try {
      console.log('💾 Salvando dados de sensores...');
      
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
        console.log('✅ Sensores salvos:', result);
      }
    } catch (error) {
      console.error('❌ Erro edge function:', error);
    }
  }, []);

  const saveRelayStatus = useCallback(async (data: any) => {
    try {
      console.log('💾 Salvando status dos relés...', data);
      
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

      console.log('💾 Dados mapeados:', mappedData);
      
      const { data: result, error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          action: 'process_relay_status',
          data: mappedData
        }
      });

      if (error) {
        console.error('❌ Erro ao salvar relés:', error);
      } else {
        console.log('✅ Relés salvos:', result);
      }
    } catch (error) {
      console.error('❌ Erro edge function:', error);
    }
  }, []);

  const publishRelayCommand = useCallback(
    async (relayIndex: number, command: boolean) => {
      const message = {
        relay: relayIndex + 1,
        command: command
      };

      console.log('🎯 RELAY CMD:', JSON.stringify(message));

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
      const message = {
        relay: relayIndex + 1,
        config: config
      };

      console.log('⚙️ RELAY CONFIG:', JSON.stringify(message));

      try {
        await publish(MQTT_CONFIG.topics.relayCommand, message);
        toast({
          title: 'Configuração enviada',
          description: `Relé ${relayIndex + 1} configurado`,
        });
      } catch (error) {
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
      const message = {
        relay: relayIndex + 1,
        auto: true
      };

      console.log('🔄 RELAY AUTO:', JSON.stringify(message));

      try {
        await publish(MQTT_CONFIG.topics.relayCommand, message);
        toast({
          title: 'Modo automático',
          description: `Relé ${relayIndex + 1} em modo automático`,
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
  }, []);

  return {
    isConnected,
    lastMessage,
    connectionState,
    publish,
    publishRelayCommand,
    publishRelayConfig,
    setRelayAuto,
    connect: reconnect, // Use reconnect for manual calls (resets counter)
    disconnect,
    client: clientRef.current,
  };
};
