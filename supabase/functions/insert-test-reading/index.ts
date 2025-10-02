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

    // Inserir leitura de teste
    const testReading = {
      ph: 6.5 + (Math.random() * 0.5 - 0.25),
      ec: 1200 + (Math.random() * 200 - 100),
      air_temp: 25 + (Math.random() * 4 - 2),
      humidity: 65 + (Math.random() * 10 - 5),
      water_temp: 23 + (Math.random() * 2 - 1)
    };

    const { data, error } = await supabase
      .from('readings')
      .insert(testReading)
      .select()
      .single();

    if (error) throw error;

    console.log('Leitura de teste inserida:', data);

    return new Response(
      JSON.stringify({ 
        message: 'Leitura de teste inserida com sucesso',
        data 
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