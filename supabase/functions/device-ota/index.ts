import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

interface OTARequest {
  device_uuid: string;
  firmware_url: string;
  firmware_version: string;
  force?: boolean;
}

serve(async (req) => {
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const supabase = createClient(
      Deno.env.get('SUPABASE_URL') ?? '',
      Deno.env.get('SUPABASE_ANON_KEY') ?? '',
      {
        auth: {
          persistSession: false,
        },
        global: {
          headers: { Authorization: req.headers.get('Authorization')! },
        },
      }
    );

    const { data: { user }, error: authError } = await supabase.auth.getUser();
    
    if (authError || !user) {
      return new Response(
        JSON.stringify({ error: 'Não autenticado' }),
        { status: 401, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    const { device_uuid, firmware_url, firmware_version, force }: OTARequest = await req.json();

    console.log(`OTA update solicitado para ${device_uuid} -> v${firmware_version}`);

    // Verificar se o dispositivo pertence ao usuário
    const { data: device } = await supabase
      .from('devices')
      .select('id, firmware_version, device_owners(user_id)')
      .eq('device_uuid', device_uuid)
      .single();

    if (!device) {
      return new Response(
        JSON.stringify({ error: 'Dispositivo não encontrado' }),
        { status: 404, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    const owners = device.device_owners as any[];
    if (!owners || owners.length === 0 || owners[0].user_id !== user.id) {
      return new Response(
        JSON.stringify({ error: 'Você não possui este dispositivo' }),
        { status: 403, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Verificar se já está na versão mais recente
    if (!force && device.firmware_version === firmware_version) {
      return new Response(
        JSON.stringify({ 
          message: 'Dispositivo já está na versão mais recente',
          current_version: device.firmware_version
        }),
        { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Publicar comando OTA via MQTT (usando mqtt-collector como proxy)
    const otaCommand = {
      device_uuid,
      command: 'ota_update',
      firmware_url,
      firmware_version,
      timestamp: Date.now()
    };

    // Registrar comando OTA no banco
    const { error: insertError } = await supabase
      .from('device_commands')
      .insert({
        device_id: device.id,
        command_type: 'ota_update',
        command_data: otaCommand,
        status: 'pending'
      });

    if (insertError) {
      console.error('Erro ao registrar comando OTA:', insertError);
    }

    console.log(`Comando OTA enviado para ${device_uuid}`);

    return new Response(
      JSON.stringify({ 
        success: true,
        message: 'Atualização OTA iniciada',
        current_version: device.firmware_version,
        target_version: firmware_version
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Erro no OTA:', error);
    const errorMessage = error instanceof Error ? error.message : 'Erro desconhecido';
    return new Response(
      JSON.stringify({ error: 'Erro ao processar OTA', details: errorMessage }),
      { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );
  }
});