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

    const { relay_index, command } = await req.json();

    // Validar entrada
    if (relay_index === undefined || command === undefined) {
      return new Response(
        JSON.stringify({ error: 'relay_index e command são obrigatórios' }),
        { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Registrar comando no banco
    const { data, error } = await supabase
      .from('relay_commands')
      .insert({
        relay_index,
        command,
        executed: false
      })
      .select()
      .single();

    if (error) {
      console.error('Erro ao inserir comando:', error);
      throw error;
    }

    console.log('Comando registrado:', data);

    await supabase.from('event_logs').insert({
      type: 'relay_command',
      message: `Comando registrado: Relé ${relay_index} - ${command ? 'LIGAR' : 'DESLIGAR'}`
    });

    return new Response(
      JSON.stringify({ 
        message: 'Comando registrado com sucesso',
        command_id: data.id,
        info: 'Configure um bridge MQTT para buscar e executar este comando'
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
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
