import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

serve(async (req) => {
  // Handle CORS preflight
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const supabaseUrl = Deno.env.get('SUPABASE_URL')!;
    const supabaseServiceKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
    
    // Get user from authorization header
    const authHeader = req.headers.get('Authorization');
    if (!authHeader) {
      return new Response(
        JSON.stringify({ error: 'Não autorizado' }),
        { status: 401, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Create client with user's token to get user ID
    const supabaseUser = createClient(supabaseUrl, Deno.env.get('SUPABASE_ANON_KEY')!, {
      global: { headers: { Authorization: authHeader } }
    });
    
    const { data: { user }, error: userError } = await supabaseUser.auth.getUser();
    if (userError || !user) {
      console.error('[REGISTER-DEVICE] Auth error:', userError);
      return new Response(
        JSON.stringify({ error: 'Usuário não autenticado' }),
        { status: 401, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    console.log(`[REGISTER-DEVICE] User ${user.id} attempting to register device`);

    // Parse request body
    const { device_uuid, claim_token, device_type, device_name } = await req.json();

    if (!device_uuid || !claim_token || !device_type) {
      return new Response(
        JSON.stringify({ error: 'UUID, token de claim e tipo do dispositivo são obrigatórios' }),
        { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Validate device_type
    if (!['sensor', 'actuator'].includes(device_type)) {
      return new Response(
        JSON.stringify({ error: 'Tipo de dispositivo inválido. Use "sensor" ou "actuator"' }),
        { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    console.log(`[REGISTER-DEVICE] Looking for device: ${device_uuid}, type: ${device_type}`);

    // Use service role client for database operations
    const supabase = createClient(supabaseUrl, supabaseServiceKey);

    // Check if device already exists
    const { data: existingDevice, error: findError } = await supabase
      .from('devices')
      .select('id, device_uuid, mqtt_password_hash, device_type')
      .eq('device_uuid', device_uuid)
      .single();

    let deviceId: string;

    if (existingDevice) {
      console.log(`[REGISTER-DEVICE] Device found: ${existingDevice.id}`);
      
      // Verify claim token matches
      if (existingDevice.mqtt_password_hash !== claim_token) {
        console.log('[REGISTER-DEVICE] Invalid claim token');
        return new Response(
          JSON.stringify({ error: 'Token de claim inválido' }),
          { status: 403, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      // Check if already owned by someone
      const { data: existingOwner } = await supabase
        .from('device_owners')
        .select('user_id')
        .eq('device_id', existingDevice.id)
        .single();

      if (existingOwner) {
        if (existingOwner.user_id === user.id) {
          return new Response(
            JSON.stringify({ error: 'Este dispositivo já está vinculado à sua conta' }),
            { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
          );
        } else {
          return new Response(
            JSON.stringify({ error: 'Este dispositivo já está vinculado a outro usuário' }),
            { status: 403, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
          );
        }
      }

      deviceId = existingDevice.id;
    } else {
      console.log('[REGISTER-DEVICE] Device not found, creating new entry');
      
      // Create new device entry
      const { data: newDevice, error: createError } = await supabase
        .from('devices')
        .insert({
          device_uuid: device_uuid,
          device_type: device_type,
          mqtt_password_hash: claim_token,
          first_seen_at: new Date().toISOString(),
        })
        .select('id')
        .single();

      if (createError) {
        console.error('[REGISTER-DEVICE] Error creating device:', createError);
        return new Response(
          JSON.stringify({ error: 'Erro ao registrar dispositivo' }),
          { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      deviceId = newDevice.id;
      console.log(`[REGISTER-DEVICE] New device created: ${deviceId}`);
    }

    // Create ownership record
    const { error: ownerError } = await supabase
      .from('device_owners')
      .insert({
        device_id: deviceId,
        user_id: user.id,
        paired_at: new Date().toISOString(),
      });

    if (ownerError) {
      console.error('[REGISTER-DEVICE] Error creating ownership:', ownerError);
      return new Response(
        JSON.stringify({ error: 'Erro ao vincular dispositivo' }),
        { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    console.log(`[REGISTER-DEVICE] Device ${deviceId} successfully registered to user ${user.id}`);

    return new Response(
      JSON.stringify({ 
        success: true, 
        message: 'Dispositivo registrado com sucesso',
        device_id: deviceId
      }),
      { status: 200, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('[REGISTER-DEVICE] Unexpected error:', error);
    return new Response(
      JSON.stringify({ error: 'Erro interno do servidor' }),
      { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );
  }
});
