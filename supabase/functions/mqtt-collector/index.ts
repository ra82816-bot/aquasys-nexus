import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.39.3";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

// Rate limiting configuration
const RATE_LIMIT_WINDOW = 60000; // 1 minuto
const MAX_REQUESTS_PER_WINDOW = 100; // 100 requisições por minuto por dispositivo
const BLOCK_DURATION = 300000; // 5 minutos de bloqueio

async function checkRateLimit(supabase: any, deviceUuid: string, endpoint: string): Promise<{ allowed: boolean; message?: string }> {
  try {
    // Buscar device_id
    const { data: device } = await supabase
      .from('devices')
      .select('id')
      .eq('device_uuid', deviceUuid)
      .single();

    if (!device) return { allowed: true }; // Dispositivo não cadastrado, permitir

    const now = new Date();
    
    // Verificar se está bloqueado
    const { data: existingLimit } = await supabase
      .from('mqtt_rate_limits')
      .select('*')
      .eq('device_id', device.id)
      .eq('endpoint', endpoint)
      .single();

    if (existingLimit) {
      // Verificar se está bloqueado
      if (existingLimit.blocked_until && new Date(existingLimit.blocked_until) > now) {
        return {
          allowed: false,
          message: `Dispositivo bloqueado até ${new Date(existingLimit.blocked_until).toISOString()}`
        };
      }

      // Verificar janela de tempo
      const windowStart = new Date(existingLimit.window_start);
      const timeSinceWindowStart = now.getTime() - windowStart.getTime();

      if (timeSinceWindowStart < RATE_LIMIT_WINDOW) {
        // Dentro da janela, incrementar contador
        const newCount = existingLimit.request_count + 1;

        if (newCount > MAX_REQUESTS_PER_WINDOW) {
          // Excedeu o limite, bloquear
          const blockedUntil = new Date(now.getTime() + BLOCK_DURATION);
          
          await supabase
            .from('mqtt_rate_limits')
            .update({
              request_count: newCount,
              blocked_until: blockedUntil.toISOString()
            })
            .eq('id', existingLimit.id);

          console.warn(`Dispositivo ${deviceUuid} bloqueado por excesso de requisições`);

          return {
            allowed: false,
            message: `Taxa excedida. Bloqueado até ${blockedUntil.toISOString()}`
          };
        }

        // Incrementar contador
        await supabase
          .from('mqtt_rate_limits')
          .update({ request_count: newCount })
          .eq('id', existingLimit.id);

        return { allowed: true };
      } else {
        // Nova janela, resetar contador
        await supabase
          .from('mqtt_rate_limits')
          .update({
            request_count: 1,
            window_start: now.toISOString(),
            blocked_until: null
          })
          .eq('id', existingLimit.id);

        return { allowed: true };
      }
    } else {
      // Primeiro acesso, criar registro
      await supabase
        .from('mqtt_rate_limits')
        .insert({
          device_id: device.id,
          endpoint,
          request_count: 1,
          window_start: now.toISOString()
        });

      return { allowed: true };
    }
  } catch (error) {
    console.error('Erro no rate limiting:', error);
    return { allowed: true }; // Em caso de erro, permitir para não bloquear operação
  }
}

serve(async (req) => {
  if (req.method === 'OPTIONS') {
    return new Response('ok', { headers: corsHeaders });
  }

  try {
    const supabaseAdmin = createClient(
      Deno.env.get('SUPABASE_URL') ?? '',
      Deno.env.get('SUPABASE_SERVICE_ROLE_KEY') ?? ''
    );

    const { topic, payload } = await req.json();
    
    // Extrair device_uuid do payload
    const data = typeof payload === 'string' ? JSON.parse(payload) : payload;
    const deviceUuid = data.device_uuid || 'unknown';
    
    // Rate limiting
    const rateLimitCheck = await checkRateLimit(supabaseAdmin, deviceUuid, topic);
    if (!rateLimitCheck.allowed) {
      console.warn(`Rate limit excedido para ${deviceUuid}: ${rateLimitCheck.message}`);
      return new Response(
        JSON.stringify({ error: 'Rate limit excedido', details: rateLimitCheck.message }),
        { status: 429, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    console.log(`Processando mensagem do tópico: ${topic} de ${deviceUuid}`);

    // Processar leituras de sensores
    if (topic === 'aquasys/sensors/all') {
      const data = typeof payload === 'string' ? JSON.parse(payload) : payload;
      console.log('Dados de sensores recebidos:', JSON.stringify(data));

      // Validação: pelo menos um campo válido
      const hasValidData = 
        (typeof data.ph === 'number' && !isNaN(data.ph)) ||
        (typeof data.ec === 'number' && !isNaN(data.ec)) ||
        (typeof data.airTemp === 'number' && !isNaN(data.airTemp)) ||
        (typeof data.humidity === 'number' && !isNaN(data.humidity)) ||
        (typeof data.waterTemp === 'number' && !isNaN(data.waterTemp));

      if (!hasValidData) {
        console.error('Nenhum dado de sensor válido encontrado');
        return new Response(
          JSON.stringify({ error: 'Nenhum dado de sensor válido' }),
          { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      const insertData: any = {};
      
      if (typeof data.ph === 'number' && !isNaN(data.ph)) insertData.ph = data.ph;
      if (typeof data.ec === 'number' && !isNaN(data.ec)) insertData.ec = data.ec;
      if (typeof data.humidity === 'number' && !isNaN(data.humidity)) insertData.humidity = data.humidity;
      if (typeof data.waterTemp === 'number' && !isNaN(data.waterTemp)) insertData.water_temp = data.waterTemp;
      
      // airTemp com fallback para waterTemp
      if (typeof data.airTemp === 'number' && !isNaN(data.airTemp)) {
        insertData.air_temp = data.airTemp;
      } else if (typeof data.waterTemp === 'number' && !isNaN(data.waterTemp)) {
        insertData.air_temp = data.waterTemp;
        console.log('Usando waterTemp como fallback para airTemp');
      }

      const { error: insertError } = await supabaseAdmin
        .from('readings')
        .insert(insertData);

      if (insertError) {
        console.error('Erro ao inserir leituras:', insertError);
        return new Response(
          JSON.stringify({ error: insertError.message }),
          { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      console.log('Leituras inseridas com sucesso!');
    }

    // Processar heartbeat com health data
    if (topic === 'aquasys/heartbeat') {
      const data = typeof payload === 'string' ? JSON.parse(payload) : payload;
      console.log('Heartbeat recebido:', JSON.stringify(data));

      // Extrair device_uuid da mensagem
      let deviceUuid = data.device_uuid;
      if (!deviceUuid && data.device) {
        const match = data.device.match(/HYDRO-([A-F0-9-]+)/i);
        if (match) deviceUuid = `HYDRO-${match[1]}`;
      }

      if (deviceUuid) {
        // Buscar device_id
        const { data: device } = await supabaseAdmin
          .from('devices')
          .select('id')
          .eq('device_uuid', deviceUuid)
          .single();

        if (device) {
          // Inserir health data
          const healthData: any = {
            device_id: device.id,
            uptime_seconds: data.uptime || 0,
            free_heap: data.free_heap || data.freeHeap,
            min_free_heap: data.min_free_heap || data.minFreeHeap,
          };

          // Dados WiFi
          if (data.wifi) {
            healthData.wifi_ssid = data.wifi.ssid;
            healthData.wifi_rssi = data.wifi.rssi || data.wifi_rssi;
            healthData.wifi_ip = data.wifi.ip;
            healthData.wifi_reconnects = data.wifi.reconnects || 0;
          } else if (data.wifi_rssi) {
            healthData.wifi_rssi = data.wifi_rssi;
          }

          // Dados MQTT
          if (data.mqtt) {
            healthData.mqtt_connected = data.mqtt.connected !== false;
            healthData.mqtt_failed_attempts = data.mqtt.failed_attempts || 0;
            healthData.mqtt_last_message_age_ms = data.mqtt.last_message_age_ms;
          }

          // Dados dos sensores
          if (data.sensors) {
            healthData.sensor_ph_valid = data.sensors.ph_valid;
            healthData.sensor_ec_valid = data.sensors.ec_valid;
            healthData.sensor_temp_valid = data.sensors.temp_valid;
            healthData.sensor_humidity_valid = data.sensors.humidity_valid;
            healthData.sensor_water_temp_valid = data.sensors.water_temp_valid;
          }

          const { error: healthError } = await supabaseAdmin
            .from('device_health')
            .insert(healthData);

          if (healthError) {
            console.error('Erro ao inserir health data:', healthError);
          } else {
            console.log('Health data inserida com sucesso!');
          }

          // Atualizar last_seen_at do device
          await supabaseAdmin
            .from('devices')
            .update({ last_seen_at: new Date().toISOString() })
            .eq('id', device.id);
        }
      }
    }

    // Processar status dos relés
    if (topic === 'aquasys/relay/status') {
      const data = typeof payload === 'string' ? JSON.parse(payload) : payload;
      console.log('📥 Status dos relés recebido:', JSON.stringify(data));

      // ✅ CORREÇÃO: Aceitar AMBOS os formatos (relay0-7 direto do ESP32 OU relay1_led-relay8_generic do useMqtt)
      const insertData = {
        relay1_led: data.relay1_led !== undefined ? data.relay1_led : (data.relay0 ?? false),
        relay2_pump: data.relay2_pump !== undefined ? data.relay2_pump : (data.relay1 ?? false),
        relay3_ph_up: data.relay3_ph_up !== undefined ? data.relay3_ph_up : (data.relay2 ?? false),
        relay4_fan: data.relay4_fan !== undefined ? data.relay4_fan : (data.relay3 ?? false),
        relay5_humidity: data.relay5_humidity !== undefined ? data.relay5_humidity : (data.relay4 ?? false),
        relay6_ec: data.relay6_ec !== undefined ? data.relay6_ec : (data.relay5 ?? false),
        relay7_co2: data.relay7_co2 !== undefined ? data.relay7_co2 : (data.relay6 ?? false),
        relay8_generic: data.relay8_generic !== undefined ? data.relay8_generic : (data.relay7 ?? false),
      };

      console.log('💾 Inserindo no banco:', JSON.stringify(insertData));

      const { error: insertError } = await supabaseAdmin
        .from('relay_status')
        .insert(insertData);

      if (insertError) {
        console.error('❌ Erro ao inserir status dos relés:', insertError);
        return new Response(
          JSON.stringify({ error: insertError.message }),
          { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      console.log('✅ Status dos relés inserido com sucesso!');
    }

    return new Response(JSON.stringify({ success: true }), {
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });
  } catch (error) {
    console.error('Erro ao processar mensagem MQTT:', error);
    const errorMessage = error instanceof Error ? error.message : 'Erro desconhecido';
    return new Response(JSON.stringify({ error: errorMessage }), {
      status: 500,
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });
  }
});