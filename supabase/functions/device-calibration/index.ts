import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

interface CalibrationRequest {
  device_uuid: string;
  sensor_type: 'ph' | 'ec';
  calibration_data: {
    ph_cal4_voltage?: number;
    ph_cal7_voltage?: number;
    ph_cal10_voltage?: number;
    ec_factor?: number;
  };
  profile_name?: string;
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

    const { device_uuid, sensor_type, calibration_data, profile_name }: CalibrationRequest = await req.json();

    console.log(`Calibração ${sensor_type} solicitada para ${device_uuid}`);

    // Verificar ownership
    const { data: device } = await supabase
      .from('devices')
      .select('id, device_type, device_owners(user_id)')
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

    // Apenas módulos de sensores podem ser calibrados
    if (device.device_type !== 'sensor') {
      return new Response(
        JSON.stringify({ error: 'Apenas módulos de sensores podem ser calibrados' }),
        { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Validar dados de calibração
    if (sensor_type === 'ph') {
      if (!calibration_data.ph_cal4_voltage || !calibration_data.ph_cal7_voltage) {
        return new Response(
          JSON.stringify({ error: 'Calibração de pH requer voltagens para pH 4 e pH 7' }),
          { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }
    } else if (sensor_type === 'ec') {
      if (!calibration_data.ec_factor) {
        return new Response(
          JSON.stringify({ error: 'Calibração de EC requer fator de calibração' }),
          { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }
    }

    // Salvar perfil de calibração
    const { data: calibProfile, error: profileError } = await supabase
      .from('device_calibration_profiles')
      .insert({
        device_id: device.id,
        sensor_type,
        calibration_data,
        profile_name: profile_name || `Calibração ${new Date().toLocaleDateString('pt-BR')}`,
        created_by: user.id,
        is_active: true
      })
      .select()
      .single();

    if (profileError) {
      console.error('Erro ao salvar perfil:', profileError);
      return new Response(
        JSON.stringify({ error: 'Erro ao salvar perfil de calibração' }),
        { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Desativar perfis anteriores
    await supabase
      .from('device_calibration_profiles')
      .update({ is_active: false })
      .eq('device_id', device.id)
      .eq('sensor_type', sensor_type)
      .neq('id', calibProfile.id);

    // Enviar comando de calibração via MQTT
    const calibCommand = {
      device_uuid,
      command: 'calibrate',
      sensor_type,
      calibration_data,
      profile_id: calibProfile.id,
      timestamp: Date.now()
    };

    const { error: commandError } = await supabase
      .from('device_commands')
      .insert({
        device_id: device.id,
        command_type: 'calibration',
        command_data: calibCommand,
        status: 'pending'
      });

    if (commandError) {
      console.error('Erro ao registrar comando:', commandError);
    }

    console.log(`Comando de calibração enviado para ${device_uuid}`);

    return new Response(
      JSON.stringify({ 
        success: true,
        message: 'Calibração aplicada com sucesso',
        profile_id: calibProfile.id,
        profile_name: calibProfile.profile_name
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Erro na calibração:', error);
    const errorMessage = error instanceof Error ? error.message : 'Erro desconhecido';
    return new Response(
      JSON.stringify({ error: 'Erro ao processar calibração', details: errorMessage }),
      { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );
  }
});