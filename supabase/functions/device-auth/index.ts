import { serve } from "https://deno.land/std@0.168.0/http/server.ts"
import { createClient } from 'https://esm.sh/@supabase/supabase-js@2.7.1'

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
}

interface AuthRequest {
  device_uuid: string;
  firmware_version?: string;
}

serve(async (req) => {
  // Handle CORS preflight requests
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const supabaseClient = createClient(
      Deno.env.get('SUPABASE_URL') ?? '',
      Deno.env.get('SUPABASE_SERVICE_ROLE_KEY') ?? '',
      {
        auth: {
          autoRefreshToken: false,
          persistSession: false
        }
      }
    )

    const { device_uuid, firmware_version } = await req.json() as AuthRequest;

    if (!device_uuid) {
      return new Response(
        JSON.stringify({ error: 'device_uuid is required' }),
        { status: 400, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    console.log(`Auth request for device: ${device_uuid}`);

    // Check if device exists and is registered
    const { data: device, error: deviceError } = await supabaseClient
      .from('devices')
      .select('id, device_uuid, mqtt_password_hash, firmware_version')
      .eq('device_uuid', device_uuid)
      .single();

    if (deviceError || !device) {
      console.error('Device not found:', deviceError);
      return new Response(
        JSON.stringify({ 
          error: 'Device not registered',
          message: 'Please register this device first through the web interface'
        }),
        { status: 404, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Update firmware version if provided
    if (firmware_version && firmware_version !== device.firmware_version) {
      await supabaseClient
        .from('devices')
        .update({ firmware_version, last_seen_at: new Date().toISOString() })
        .eq('device_uuid', device_uuid);
      
      console.log(`Updated firmware version to ${firmware_version}`);
    }

    // Update last seen
    await supabaseClient
      .from('devices')
      .update({ last_seen_at: new Date().toISOString() })
      .eq('device_uuid', device_uuid);

    // Return MQTT credentials
    const response = {
      success: true,
      mqtt_config: {
        broker: Deno.env.get('MQTT_BROKER') || 'wss://8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud:8884/mqtt',
        username: device_uuid,
        password: device.mqtt_password_hash,
        client_id: `${device_uuid}_${Date.now()}`,
        topics: {
          sensors: `aquasys/${device_uuid}/sensors`,
          relay_status: `aquasys/${device_uuid}/relay/status`,
          relay_command: `aquasys/${device_uuid}/relay/command`,
          heartbeat: `aquasys/${device_uuid}/heartbeat`
        }
      }
    };

    console.log(`Auth successful for ${device_uuid}`);

    return new Response(
      JSON.stringify(response),
      { status: 200, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Error in device-auth:', error);
    return new Response(
      JSON.stringify({ error: error.message }),
      { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );
  }
})