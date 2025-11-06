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
  const [deviceUuid, setDeviceUuid] = useState<string | null>(null);
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
      
      // ✅ CORREÇÃO: Aguardar confirmação de conexão antes de subscrever
      setTimeout(() => {
        if (!client.connected) {
          console.warn('⚠️ Cliente desconectou antes de subscrever');
          return;
        }
        
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
      }, 100); // Pequeno delay para garantir estabilidade da conexão
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
    async (topic: string, message: any, options = { qos: 1 as 0 | 1 | 2 }) => {
      let attempts = 0;
      const maxRetries = 3;
      
      while (attempts < maxRetries) {
        try {
          await new Promise<void>((resolve, reject) => {
            if (!clientRef.current?.connected) {
              reject(new Error('MQTT não conectado'));
              return;
            }

            const payload = typeof message === 'string' ? message : JSON.stringify(message);
            
            console.log(`📤 [Tentativa ${attempts + 1}/${maxRetries}] Publicando no tópico ${topic}:`, payload);
            
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
          
          return; // Sucesso - sair do loop
        } catch (error) {
          attempts++;
          if (attempts >= maxRetries) {
            console.error(`❌ Falha após ${maxRetries} tentativas`);
            throw error;
          }
          
          const backoffMs = 1000 * attempts;
          console.warn(`⚠️ Tentativa ${attempts}/${maxRetries} falhou, aguardando ${backoffMs}ms...`);
          await new Promise(resolve => setTimeout(resolve, backoffMs));
        }
      }
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
      
      // ✅ CORREÇÃO CRÍTICA: NÃO fazer mapeamento aqui - deixar mqtt-collector fazer
      // Passar os dados exatamente como vêm do ESP32 (relay0-relay7)
      const relayPayload = {
        ...data, // Manter todos os campos originais (relay0-relay7)
        device_uuid: deviceUuid
      };

      console.log('💾 Enviando para mqtt-collector:', relayPayload);
      
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
        
        // ✅ Atualizar device ativo se este for diferente do atual
        if (deviceUuid !== 'unknown') {
          setDeviceUuid(deviceUuid);
          console.log('🔄 Device ativo confirmado:', deviceUuid);
        }
      }
    } catch (error) {
      console.error('❌ Erro ao chamar edge function:', error);
    }
  }, []);

  // ✅ FASE 1: Salvar device health (heartbeat) via Edge Function
  const saveDeviceHealth = useCallback(async (data: any) => {
    try {
      console.log('💾 Processando heartbeat via Edge Function:', data.device_uuid);
      
      const deviceUuid = data.device_uuid || 'unknown';
      
      if (deviceUuid === 'unknown') {
        console.error('❌ Heartbeat sem device_uuid');
        return;
      }
      
      // ✅ Enviar para Edge Function (tem service role para UPDATE/INSERT)
      const { data: result, error } = await supabase.functions.invoke('mqtt-collector', {
        body: {
          topic: 'aquasys/heartbeat',
          payload: data
        }
      });

      if (error) {
        console.error('❌ Erro ao processar heartbeat via Edge Function:', error);
      } else {
        console.log('✅ Heartbeat processado com sucesso:', result);
      }
    } catch (error) {
      console.error('❌ Erro ao chamar Edge Function para heartbeat:', error);
    }
  }, []);

  const publishRelayCommand = useCallback(
    async (relayIndex: number, command: boolean) => {
      if (!deviceUuid) {
        console.error('❌ Device UUID não disponível');
        toast({
          title: "Erro",
          description: "Nenhum dispositivo conectado",
          variant: "destructive"
        });
        return;
      }

      // ✅ DIAGNÓSTICO COMPLETO
      console.log('📤 COMANDO ENVIADO:');
      console.log('  Device UUID:', deviceUuid);
      console.log('  Relé Index (0-7):', relayIndex);
      console.log('  Estado desejado:', command ? 'LIGADO' : 'DESLIGADO');
      console.log('  Relé no ESP32:', `relay${relayIndex}`);
      console.log('  Relé no banco:', `relay${relayIndex + 1}_*`);

      // ✅ relayIndex já vem como 0-7 do RelayCard
      const message = {
        relay: relayIndex, // Índice 0-7 direto do banco
        state: command
      };

      // ✅ Publicar no tópico específico do dispositivo
      const deviceTopic = `aquasys/${deviceUuid}/relay/command`;
      console.log(`📡 Tópico MQTT: ${deviceTopic}`);
      console.log('📦 Payload MQTT:', JSON.stringify(message));
      
      try {
        await publish(deviceTopic, message);
        
        // Registrar no event_logs para auditoria
        await supabase.from('event_logs').insert({
          type: 'relay_command',
          message: `Relé ${relayIndex} → ${command ? 'LIGADO' : 'DESLIGADO'}`,
          metadata: { 
            device_uuid: deviceUuid, 
            relay_index: relayIndex, 
            command, 
            mqtt_topic: deviceTopic,
            timestamp: new Date().toISOString() 
          }
        });
        
        console.log(`✅ Comando publicado para ${deviceTopic}`);
      } catch (error) {
        console.error('❌ Erro ao publicar comando de relé:', error);
        throw error;
      }
    },
    [publish, deviceUuid, toast]
  );

  const publishRelayConfig = useCallback(
    async (relayIndex: number, config: any) => {
      if (!deviceUuid) {
        console.error('❌ Device UUID não disponível');
        return;
      }

      // ✅ CORREÇÃO: Enviar config na raiz do JSON (não aninhado)
      const message = {
        relay: relayIndex, // Índice 0-7 direto
        ...config // Spread dos parâmetros na raiz
      };

      const deviceTopic = `aquasys/${deviceUuid}/relay/config`;
      console.log(`📤 Enviando configuração para ${deviceUuid}, relé ${relayIndex + 1}:`, message);
      try {
        await publish(deviceTopic, message);
        console.log('✅ Configuração enviada com sucesso');
      } catch (error) {
        console.error('❌ Erro ao enviar configuração:', error);
        throw error;
      }
    },
    [publish, deviceUuid]
  );

  const setRelayAuto = useCallback(
    async (relayIndex: number) => {
      if (!deviceUuid) {
        console.error('❌ Device UUID não disponível');
        return;
      }

      // ✅ relayIndex já vem como 0-7 do componente
      const message = {
        auto: relayIndex // Índice 0-7 direto
      };

      const deviceTopic = `aquasys/${deviceUuid}/relay/command`;
      console.log(`📤 Definindo modo automático para ${deviceUuid}, relé ${relayIndex + 1}`);
      try {
        await publish(deviceTopic, message);
        console.log('✅ Modo automático definido com sucesso');
      } catch (error) {
        console.error('❌ Erro ao definir modo auto:', error);
        throw error;
      }
    },
    [publish, deviceUuid]
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

  // ✅ Carregar device UUID ativo ao iniciar
  useEffect(() => {
    const loadActiveDevice = async () => {
      try {
        // ✅ Buscar device mais recente baseado em last_seen_at
        const { data, error } = await supabase
          .from('devices')
          .select('device_uuid, last_seen_at')
          .order('last_seen_at', { ascending: false, nullsFirst: false })
          .limit(1)
          .maybeSingle();
        
        if (!error && data) {
          setDeviceUuid(data.device_uuid);
          console.log('✅ Device ativo identificado:', data.device_uuid, 
                      'último visto:', data.last_seen_at || 'nunca');
          return;
        }

        // Fallback: buscar último device cadastrado
        const { data: fallbackData, error: fallbackError } = await supabase
          .from('devices')
          .select('device_uuid')
          .order('created_at', { ascending: false })
          .limit(1)
          .maybeSingle();
        
        if (!fallbackError && fallbackData) {
          setDeviceUuid(fallbackData.device_uuid);
          console.log('✅ Device ativo carregado via cadastro:', fallbackData.device_uuid);
        } else {
          console.warn('⚠️ Nenhum device encontrado');
        }
      } catch (error) {
        console.error('❌ Erro ao carregar device:', error);
      }
    };
    
    loadActiveDevice();
    
    // ✅ Atualizar device ativo a cada 30 segundos
    const interval = setInterval(loadActiveDevice, 30000);
    return () => clearInterval(interval);
  }, []);

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
