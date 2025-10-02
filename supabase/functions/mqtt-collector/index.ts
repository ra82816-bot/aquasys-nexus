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

    if (action === 'process') {
      // Validar campos obrigatórios
      if (!data.ph || !data.ec || !data.airTemp || !data.humidity || !data.waterTemp) {
        console.error('Dados inválidos - campos obrigatórios ausentes:', data);
        
        await supabase.from('event_logs').insert({
          type: 'validation_error',
          message: `Dados inválidos recebidos: ${JSON.stringify(data)}`
        });
        
        return new Response(
          JSON.stringify({ error: 'Dados inválidos' }),
          { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      // Inserir leitura no banco
      const { error: insertError } = await supabase
        .from('readings')
        .insert({
          ph: data.ph,
          ec: data.ec,
          air_temp: data.airTemp,
          humidity: data.humidity,
          water_temp: data.waterTemp
        });

      if (insertError) {
        console.error('Erro ao inserir leitura:', insertError);
        
        await supabase.from('event_logs').insert({
          type: 'database_error',
          message: `Erro ao inserir leitura: ${insertError.message}`
        });
        
        throw insertError;
      }

      console.log('Leitura inserida com sucesso!');
      
      await supabase.from('event_logs').insert({
        type: 'reading_received',
        message: `Leitura processada: pH ${data.ph}, EC ${data.ec}`
      });

      return new Response(
        JSON.stringify({ message: 'Leitura processada com sucesso' }),
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