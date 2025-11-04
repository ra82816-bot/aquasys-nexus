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
        'aquasys/heartbeat', // ✅ FASE 1: Subscribe em heartbeat
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
        } else if (topic === 'aquasys/heartbeat') {
          await saveDeviceHealth(data);
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
      
      // ✅ Extrair device_uuid do firmware v4.3-F1 (HYDRO-XXYY-ZZWW-AABB)
      const deviceUuid = data.device_uuid || data.deviceUUID || 'unknown';
      
      // ✅ CORREÇÃO CRÍTICA: Mapear "temperature" do sensor para "airTemp" E "temperature"
      // Sensor publica "temperature", atuador espera "airTemp"
      const airTempValue = data.temperature || data.airTemp || data.air_temp;
      
      const sensorPayload = {
        ph: data.ph,
        ec: data.ec,
        airTemp: airTempValue,
        temperature: airTempValue, // ✅ Publicar ambos os campos para compatibilidade total
        humidity: data.humidity,
        waterTemp: data.waterTemp || data.water_temp,
        device_uuid: deviceUuid
      };
      
      const { data: result, error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          topic: 'aquasys/sensors/all',
          payload: sensorPayload
        }
      });

      if (error) {
        console.error('❌ Erro ao salvar sensores:', error);
      } else {
        console.log('✅ Dados de sensores salvos:', result);
        setLastSensorUpdate(Date.now()); // ✅ Atualizar timestamp
      }
    } catch (error) {
      console.error('❌ Erro ao chamar edge function:', error);
    }
  }, []);

  const saveRelayStatus = useCallback(async (data: any) => {
    try {
      console.log('💾 Salvando status dos relés no banco...', data);
      
      // ✅ Extrair device_uuid do firmware v4.3-F1
      const deviceUuid = data.device_uuid || data.deviceUUID || 'unknown';
      
      // Formato esperado pela edge function mqtt-collector
      const relayPayload = {
        relay1: data.relay1 ?? false,
        relay2: data.relay2 ?? false,
        relay3: data.relay3 ?? false,
        relay4: data.relay4 ?? false,
        relay5: data.relay5 ?? false,
        relay6: data.relay6 ?? false,
        relay7: data.relay7 ?? false,
        relay8: data.relay8 ?? false,
        device_uuid: deviceUuid
      };

      console.log('💾 Dados mapeados para salvar:', relayPayload);
      
      const { data: result, error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          topic: 'aquasys/relay/status',
          payload: relayPayload
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

  // ✅ FASE 1: Salvar device health (heartbeat)
  const saveDeviceHealth = useCallback(async (data: any) => {
    try {
      console.log('💾 Processando heartbeat...', data);
      
      const deviceUuid = data.device_uuid || 'unknown';
      
      // 1. Mapear UUID → device_id
      const { data: device, error: deviceError } = await supabase
        .from('devices')
        .select('id')
        .eq('device_uuid', deviceUuid)
        .single();
      
      if (deviceError || !device) {
        console.error('❌ Device não encontrado:', deviceUuid);
        return;
      }
      
      // 2. Extrair métricas do heartbeat
      const healthData = {
        device_id: device.id,
        uptime_seconds: Math.floor((data.uptime_ms || 0) / 1000),
        wifi_rssi: data.status?.rssi || 0,
        wifi_ssid: data.status?.ip_address || null, // Temporário até firmware enviar SSID
        wifi_reconnects: 0, // Firmware v4.2.3 não envia ainda
        mqtt_connected: data.status?.mqtt_connected ?? false,
        mqtt_failed_attempts: 0,
        free_heap: data.memory?.free_heap || 0,
        min_free_heap: data.memory?.min_free_heap || 0,
        sensor_ph_valid: null,
        sensor_ec_valid: null,
        sensor_temp_valid: null,
        sensor_humidity_valid: null,
        sensor_water_temp_valid: null,
      };
      
      // 3. Salvar no banco
      const { error: insertError } = await supabase
        .from('device_health')
        .insert(healthData);
      
      if (insertError) {
        console.error('❌ Erro ao salvar device health:', insertError);
      } else {
        console.log('✅ Device health salvo:', deviceUuid);
      }
    } catch (error) {
      console.error('❌ Erro ao processar heartbeat:', error);
    }
  }, []);

  const publishRelayCommand = useCallback(
    async (relayIndex: number, command: boolean) => {
      // ✅ Firmware v4.3-F1 espera índice 0-7 (não 1-8)
      const message = {
        relay: relayIndex - 1, // Converte de 1-8 para 0-7
        state: command
      };

      console.log(`📤 Enviando comando para relé ${relayIndex} (índice ${relayIndex - 1}):`, message);
      try {
        await publish(MQTT_CONFIG.topics.relayCommand, message);
        console.log('✅ Comando enviado com sucesso');
      } catch (error) {
        console.error('❌ Erro ao enviar comando:', error);
        throw error;
      }
    },
    [publish]
  );

  const publishRelayConfig = useCallback(
    async (relayIndex: number, config: any) => {
      // ✅ Firmware v4.3-F1 espera índice 0-7 (não 1-8)
      const message = {
        relay: relayIndex - 1, // Converte de 1-8 para 0-7
        config: config
      };

      console.log(`📤 Enviando configuração para relé ${relayIndex} (índice ${relayIndex - 1}):`, message);
      try {
        await publish(MQTT_CONFIG.topics.relayCommand, message);
        console.log('✅ Configuração enviada com sucesso');
      } catch (error) {
        console.error('❌ Erro ao enviar configuração:', error);
        throw error;
      }
    },
    [publish]
  );

  const setRelayAuto = useCallback(
    async (relayIndex: number) => {
      // ✅ Firmware v4.3-F1 espera índice 0-7 (não 1-8)
      const message = {
        auto: relayIndex - 1 // Converte de 1-8 para 0-7
      };

      console.log(`📤 Definindo modo automático para relé ${relayIndex} (índice ${relayIndex - 1})`);
      try {
        await publish(MQTT_CONFIG.topics.relayCommand, message);
        console.log('✅ Modo automático definido com sucesso');
      } catch (error) {
        console.error('❌ Erro ao definir modo auto:', error);
        throw error;
      }
    },
    [publish]
  );

  // ✅ VALIDAÇÃO DE TIMEOUT DE DADOS (Prioridade ALTA)
  useEffect(() => {
    const interval = setInterval(() => {
      const now = Date.now();
      const elapsed = now - lastSensorUpdate;
      
      if (lastSensorUpdate > 0 && elapsed > 180000) { // 3 minutos
        if (!sensorTimeout) {
          setSensorTimeout(true);
          toast({
            title: "⚠️ Sensores offline",
            description: "Não há dados atualizados há mais de 3 minutos. Verifique a conexão dos dispositivos.",
            variant: "destructive"
          });
        }
      } else {
        if (sensorTimeout) {
          setSensorTimeout(false);
          toast({
            title: "✅ Sensores reconectados",
            description: "Dados de sensores voltaram ao normal.",
          });
        }
      }
    }, 30000); // Verificar a cada 30 segundos
    
    return () => clearInterval(interval);
  }, [lastSensorUpdate, sensorTimeout, toast]);

  useEffect(() => {
    connect();
    return () => disconnect();
  }, [connect, disconnect]);

  return {
    isConnected,
    lastMessage,
    lastSensorUpdate,
    sensorTimeout,
    publish,
    publishRelayCommand,
    publishRelayConfig,
    setRelayAuto,
    connect,
    disconnect,
    client: clientRef.current,
  };
};
