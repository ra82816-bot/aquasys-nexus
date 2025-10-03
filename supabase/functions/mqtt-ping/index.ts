import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

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

    console.log('🔔 Ping MQTT solicitado');

    // Inserir comando especial de ping (relay_index -1 indica ping)
    const { data, error } = await supabase
      .from('relay_commands')
      .insert({
        relay_index: -1,
        command: true,
        executed: false
      })
      .select()
      .single();

    if (error) {
      console.error('Erro ao inserir comando de ping:', error);
      throw error;
    }

    console.log('✓ Comando de ping inserido:', data.id);

    // Registrar evento
    await supabase
      .from('event_logs')
      .insert({
        type: 'mqtt_ping',
        message: 'Ping MQTT solicitado via dashboard'
      });

    return new Response(
      JSON.stringify({ 
        success: true, 
        message: 'Ping enviado',
        commandId: data.id 
      }),
      { 
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        status: 200 
      }
    );

  } catch (error) {
    console.error('Erro no mqtt-ping:', error);
    return new Response(
      JSON.stringify({ error: error.message }),
      { 
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        status: 500 
      }
    );
  }
});