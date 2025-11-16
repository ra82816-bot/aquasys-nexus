import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.39.3";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

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
    const data = typeof payload === 'string' ? JSON.parse(payload) : payload;
    
    console.log(`📥 Tópico: ${topic} | Modo: fallback`);

    // Processar sensores
    if (topic === 'aquasys/sensors/all') {
      const insertData: any = {};
      if (typeof data.ph === 'number') insertData.ph = data.ph;
      if (typeof data.ec === 'number') insertData.ec = data.ec;
      if (typeof data.humidity === 'number') insertData.humidity = data.humidity;
      if (typeof data.water_temp === 'number') insertData.water_temp = data.water_temp;
      if (typeof data.air_temp === 'number') insertData.air_temp = data.air_temp;

      await supabaseAdmin.from('readings').insert(insertData);
      console.log('✅ Leituras inseridas');
    }

    // Processar heartbeat
    if (topic === 'aquasys/heartbeat' || topic === 'aquasys/heartbeat/sensor') {
      console.log('💓 Heartbeat recebido');
    }

    // Processar relés
    if (topic === 'aquasys/relay/status') {
      const insertData = {
        relay1_led: data.relay0 ?? false,
        relay2_pump: data.relay1 ?? false,
        relay3_ph_up: data.relay2 ?? false,
        relay4_fan: data.relay3 ?? false,
        relay5_humidity: data.relay4 ?? false,
        relay6_ec: data.relay5 ?? false,
        relay7_co2: data.relay6 ?? false,
        relay8_generic: data.relay7 ?? false,
      };
      await supabaseAdmin.from('relay_status').insert(insertData);
      console.log('✅ Status relés inserido');
    }

    return new Response(JSON.stringify({ success: true }), {
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });
  } catch (error) {
    console.error('Erro:', error);
    return new Response(JSON.stringify({ error: String(error) }), {
      status: 500,
      headers: { ...corsHeaders, 'Content-Type': 'application/json' },
    });
  }
});
