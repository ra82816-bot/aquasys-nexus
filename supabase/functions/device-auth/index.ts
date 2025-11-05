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

  const clientIP = req.headers.get('x-forwarded-for') || 
                   req.headers.get('x-real-ip') || 
                   'unknown';

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

    // Rate limiting: 10 requests per minute per IP
    const rateLimitWindow = new Date(Date.now() - 60000); // 1 minute ago
    const { data: rateCheck, error: rateError } = await supabaseClient
      .from('mqtt_rate_limits')
      .select('request_count, blocked_until')
      .eq('endpoint', 'device-auth')
      .eq('device_id', clientIP)
      .gte('window_start', rateLimitWindow.toISOString())
      .single();

    // Check if IP is blocked
    if (rateCheck?.blocked_until && new Date(rateCheck.blocked_until) > new Date()) {
      console.warn(`⚠️  Rate limit: IP ${clientIP} is blocked until ${rateCheck.blocked_until}`);
      return new Response(
        JSON.stringify({ error: 'Too many requests. Please try again later.' }),
        { status: 429, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Update or insert rate limit record
    if (rateCheck && rateCheck.request_count >= 10) {
      const blockUntil = new Date(Date.now() + 300000); // Block for 5 minutes
      await supabaseClient
        .from('mqtt_rate_limits')
        .update({ 
          blocked_until: blockUntil.toISOString(),
          request_count: rateCheck.request_count + 1
        })
        .eq('endpoint', 'device-auth')
        .eq('device_id', clientIP);
      
      console.warn(`⚠️  Rate limit exceeded: Blocking IP ${clientIP} until ${blockUntil.toISOString()}`);
      return new Response(
        JSON.stringify({ error: 'Too many requests. Blocked for 5 minutes.' }),
        { status: 429, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    } else if (rateCheck) {
      await supabaseClient
        .from('mqtt_rate_limits')
        .update({ request_count: rateCheck.request_count + 1 })
        .eq('endpoint', 'device-auth')
        .eq('device_id', clientIP);
    } else {
      await supabaseClient
        .from('mqtt_rate_limits')
        .insert({
          endpoint: 'device-auth',
          device_id: clientIP,
          request_count: 1,
          window_start: new Date().toISOString()
        });
    }

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
      .select('id, device_uuid, firmware_version')
      .eq('device_uuid', device_uuid)
      .single();

    if (deviceError || !device) {
      console.error(`❌ Device not found: ${device_uuid} from IP ${clientIP}`);
      console.error('Device error:', deviceError);
      return new Response(
        JSON.stringify({ 
          error: 'Device not registered',
          message: 'Please register this device first through the web interface'
        }),
        { status: 404, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // ✅ SOLUÇÃO A: Usar credenciais compartilhadas do HiveMQ
    const hivemqUsername = Deno.env.get('HIVEMQ_USERNAME');
    const hivemqPassword = Deno.env.get('HIVEMQ_PASSWORD');

    if (!hivemqUsername || !hivemqPassword) {
      console.error('❌ HiveMQ credentials not configured');
      return new Response(
        JSON.stringify({ 
          error: 'Server configuration error',
          message: 'MQTT credentials not available'
        }),
        { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // ✅ FASE 1: Sempre atualizar last_seen_at e firmware_version na autenticação
    const updateData: any = {
      last_seen_at: new Date().toISOString()
    };
    
    if (firmware_version) {
      updateData.firmware_version = firmware_version;
    }
    
    const { error: updateError } = await supabaseClient
      .from('devices')
      .update(updateData)
      .eq('device_uuid', device_uuid);
    
    if (updateError) {
      console.error('Error updating device:', updateError);
    } else {
      console.log(`✅ Auth success: ${device_uuid} (v${firmware_version || device.firmware_version}) from IP ${clientIP}`);
    }

    // ✅ Return MQTT credentials - Shared credentials for all devices
    const response = {
      success: true,
      mqtt_config: {
        broker: Deno.env.get('MQTT_BROKER') || 'wss://8cda72f06f464778bc53751d7cc88ac2.s1.eu.hivemq.cloud:8884/mqtt',
        username: hivemqUsername,
        password: hivemqPassword,
        client_id: `${device_uuid}_${Date.now()}`,
        topics: {
          sensors: `aquasys/${device_uuid}/sensors`,
          relay_status: `aquasys/${device_uuid}/relay/status`,
          relay_command: `aquasys/${device_uuid}/relay/command`,
          relay_config: `aquasys/${device_uuid}/relay/config`,
          calibration: `aquasys/${device_uuid}/calibration`,
          heartbeat: `aquasys/${device_uuid}/heartbeat`
        }
      }
    };

    console.log(`🔐 Device authenticated: ${device_uuid} - last_seen updated`);

    return new Response(
      JSON.stringify(response),
      { status: 200, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Error in device-auth:', error);
    const errorMessage = error instanceof Error ? error.message : 'Unknown error';
    return new Response(
      JSON.stringify({ error: errorMessage }),
      { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );
  }
})