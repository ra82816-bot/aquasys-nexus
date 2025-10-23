import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.39.3";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

serve(async (req) => {
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const supabaseUrl = Deno.env.get('SUPABASE_URL')!;
    const supabaseKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
    const supabase = createClient(supabaseUrl, supabaseKey);

    const { action, data } = await req.json();

    if (action === 'process_sensors') {
      // Mapear campos do firmware (temperature/waterTemp) para banco (air_temp/water_temp)
      const airTemp = data.airTemp || data.temperature;
      const waterTemp = data.waterTemp || data.water_temp;
      
      // Validar campos obrigatórios (pH, humidity e water_temp são essenciais)
      // EC e airTemp podem ser 0 ou undefined em algumas situações
      if (data.ph === undefined || data.ph === null || 
          data.humidity === undefined || data.humidity === null ||
          !waterTemp) {
        console.error('Dados de sensores inválidos (campos essenciais faltando):', data);
        
        await supabase.from('event_logs').insert({
          type: 'validation_error',
          message: `Dados essenciais faltando: ${JSON.stringify(data)}`
        });
        
        return new Response(
          JSON.stringify({ error: 'Dados essenciais faltando (pH, humidity, waterTemp)' }),
          { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      // Preparar dados com valores padrão para campos opcionais
      const readingData = {
        ph: data.ph,
        ec: data.ec !== undefined && data.ec !== null ? data.ec : 0,
        air_temp: airTemp !== undefined && airTemp !== null ? airTemp : waterTemp, // usa water_temp como fallback
        humidity: data.humidity,
        water_temp: waterTemp
      };

      console.log('📊 Inserindo leitura:', readingData);

      // Inserir leitura de sensores
      const { error: insertError } = await supabase
        .from('readings')
        .insert(readingData);

      if (insertError) {
        console.error('Erro ao inserir leitura:', insertError);
        
        await supabase.from('event_logs').insert({
          type: 'database_error',
          message: `Erro ao inserir leitura: ${insertError.message}`
        });
        
        throw insertError;
      }

      console.log('Leitura de sensores inserida com sucesso!');
      
      await supabase.from('event_logs').insert({
        type: 'reading_received',
        message: `Leitura processada: pH ${data.ph}, EC ${data.ec}`
      });

      return new Response(
        JSON.stringify({ message: 'Leitura de sensores processada com sucesso' }),
        { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    if (action === 'process_relay_status') {
      // Validar dados dos relés
      if (!data.relay1_led === undefined || !data.relay2_pump === undefined) {
        console.error('Dados de relés inválidos:', data);
        
        await supabase.from('event_logs').insert({
          type: 'validation_error',
          message: `Dados de relés inválidos: ${JSON.stringify(data)}`
        });
        
        return new Response(
          JSON.stringify({ error: 'Dados de relés inválidos' }),
          { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      // Inserir status dos relés
      const { error: insertError } = await supabase
        .from('relay_status')
        .insert({
          relay1_led: data.relay1_led,
          relay2_pump: data.relay2_pump,
          relay3_ph_up: data.relay3_ph_up,
          relay4_fan: data.relay4_fan,
          relay5_humidity: data.relay5_humidity,
          relay6_ec: data.relay6_ec,
          relay7_co2: data.relay7_co2,
          relay8_generic: data.relay8_generic
        });

      if (insertError) {
        console.error('Erro ao inserir status dos relés:', insertError);
        
        await supabase.from('event_logs').insert({
          type: 'database_error',
          message: `Erro ao inserir status dos relés: ${insertError.message}`
        });
        
        throw insertError;
      }

      console.log('Status dos relés inserido com sucesso!');
      
      await supabase.from('event_logs').insert({
        type: 'relay_status_received',
        message: `Status dos relés processado`
      });

      return new Response(
        JSON.stringify({ message: 'Status dos relés processado com sucesso' }),
        { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    return new Response(
      JSON.stringify({ error: 'Ação inválida' }),
      { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Erro:', error);
    return new Response(
      JSON.stringify({ error: error instanceof Error ? error.message : 'Erro desconhecido' }),
      { 
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' }
      }
    );
  }
});