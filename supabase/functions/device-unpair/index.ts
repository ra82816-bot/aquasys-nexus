import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

interface UnpairRequest {
  device_uuid: string;
}

serve(async (req) => {
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    // Cliente para verificar autenticação do usuário
    const authToken = req.headers.get('Authorization');
    if (!authToken) {
      return new Response(
        JSON.stringify({ error: 'Não autenticado' }),
        { status: 401, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    const supabaseAuth = createClient(
      Deno.env.get('SUPABASE_URL') ?? '',
      Deno.env.get('SUPABASE_ANON_KEY') ?? '',
      {
        auth: {
          persistSession: false,
        },
        global: {
          headers: { Authorization: authToken },
        },
      }
    );

    // Verificar autenticação
    const { data: { user }, error: authError } = await supabaseAuth.auth.getUser();
    
    if (authError || !user) {
      console.error('Auth error:', authError);
      return new Response(
        JSON.stringify({ error: 'Não autenticado' }),
        { status: 401, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    const { device_uuid }: UnpairRequest = await req.json();

    console.log(`Unpairing device ${device_uuid} for user ${user.id}`);

    // Cliente com service role para operações no banco
    const supabase = createClient(
      Deno.env.get('SUPABASE_URL') ?? '',
      Deno.env.get('SUPABASE_SERVICE_ROLE_KEY') ?? '',
      {
        auth: {
          persistSession: false,
        },
      }
    );

    // Verificar se o dispositivo existe
    const { data: device } = await supabase
      .from('devices')
      .select('id')
      .eq('device_uuid', device_uuid)
      .maybeSingle();

    if (!device) {
      return new Response(
        JSON.stringify({ error: 'Dispositivo não encontrado' }),
        { status: 404, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Verificar se o dispositivo pertence ao usuário
    const { data: ownership } = await supabase
      .from('device_owners')
      .select('user_id')
      .eq('device_id', device.id)
      .maybeSingle();
    
    if (!ownership) {
      return new Response(
        JSON.stringify({ error: 'Dispositivo não está vinculado' }),
        { status: 404, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    if (ownership.user_id !== user.id) {
      return new Response(
        JSON.stringify({ error: 'Você não tem permissão para desvincular este dispositivo' }),
        { status: 403, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Desvincular o dispositivo
    const { error: deleteError } = await supabase
      .from('device_owners')
      .delete()
      .eq('device_id', device.id)
      .eq('user_id', user.id);

    if (deleteError) {
      console.error('Error deleting ownership:', deleteError);
      return new Response(
        JSON.stringify({ error: 'Erro ao desvincular dispositivo' }),
        { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    console.log(`Device ${device_uuid} successfully unpaired from user ${user.id}`);

    return new Response(
      JSON.stringify({ 
        success: true,
        message: 'Dispositivo desvinculado com sucesso'
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Error in device-unpair function:', error);
    const errorMessage = error instanceof Error ? error.message : 'Erro desconhecido';
    return new Response(
      JSON.stringify({ error: 'Erro interno do servidor', details: errorMessage }),
      { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );
  }
});