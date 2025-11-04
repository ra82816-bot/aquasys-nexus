import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

interface PairRequest {
  device_uuid: string;
  device_type: 'sensor' | 'actuator';
  firmware_version: string;
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

    const { device_uuid, device_type, firmware_version }: PairRequest = await req.json();

    console.log(`Pairing device ${device_uuid} for user ${user.id}`);

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

    // Verificar se o dispositivo já está registrado
    const { data: existingDevice } = await supabase
      .from('devices')
      .select('id, device_owners(user_id)')
      .eq('device_uuid', device_uuid)
      .single();

    if (existingDevice) {
      // Verificar se já está vinculado a este usuário
      const owners = existingDevice.device_owners as any[];
      
      if (owners && owners.length > 0) {
        // Verificar se é o usuário atual
        const isCurrentUser = owners.some((owner: any) => owner.user_id === user.id);
        
        if (isCurrentUser) {
          // Dispositivo já vinculado a este usuário - retornar sucesso
          return new Response(
            JSON.stringify({ 
              success: true, 
              device_id: existingDevice.id,
              message: 'Dispositivo já está vinculado à sua conta'
            }),
            { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
          );
        } else {
          // Dispositivo vinculado a outro usuário
          return new Response(
            JSON.stringify({ 
              error: 'Dispositivo já vinculado a outra conta',
              code: 'DEVICE_ALREADY_PAIRED'
            }),
            { status: 409, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
          );
        }
      }

      // Dispositivo existe mas não tem owner, vincular
      const { error: ownerError } = await supabase
        .from('device_owners')
        .insert({
          device_id: existingDevice.id,
          user_id: user.id
        });

      if (ownerError) {
        console.error('Error creating ownership:', ownerError);
        return new Response(
          JSON.stringify({ error: 'Erro ao vincular dispositivo' }),
          { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
        );
      }

      return new Response(
        JSON.stringify({ 
          success: true, 
          device_id: existingDevice.id,
          message: 'Dispositivo vinculado com sucesso'
        }),
        { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Criar novo dispositivo
    // Gerar senha MQTT usando HMAC-SHA256 com Web Crypto API
    const userSecret = user.id;
    const encoder = new TextEncoder();
    const keyData = encoder.encode(userSecret);
    const messageData = encoder.encode(device_uuid);
    
    const cryptoKey = await crypto.subtle.importKey(
      'raw',
      keyData,
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['sign']
    );
    
    const signature = await crypto.subtle.sign('HMAC', cryptoKey, messageData);
    const hashArray = Array.from(new Uint8Array(signature));
    const mqttPassword = hashArray.map(b => b.toString(16).padStart(2, '0')).join('');

    const { data: newDevice, error: deviceError } = await supabase
      .from('devices')
      .insert({
        device_uuid,
        device_type,
        firmware_version,
        mqtt_password_hash: mqttPassword
      })
      .select()
      .single();

    if (deviceError || !newDevice) {
      console.error('Error creating device:', deviceError);
      return new Response(
        JSON.stringify({ error: 'Erro ao criar dispositivo' }),
        { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    // Vincular ao usuário
    const { error: ownerError } = await supabase
      .from('device_owners')
      .insert({
        device_id: newDevice.id,
        user_id: user.id
      });

    if (ownerError) {
      console.error('Error creating ownership:', ownerError);
      // Rollback: deletar o device
      await supabase.from('devices').delete().eq('id', newDevice.id);
      
      return new Response(
        JSON.stringify({ error: 'Erro ao vincular dispositivo' }),
        { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
      );
    }

    console.log(`Device ${device_uuid} paired successfully to user ${user.id}`);

    return new Response(
      JSON.stringify({ 
        success: true, 
        device_id: newDevice.id,
        mqtt_password: mqttPassword,
        message: 'Dispositivo registrado e vinculado com sucesso'
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Error in device-pair function:', error);
    const errorMessage = error instanceof Error ? error.message : 'Erro desconhecido';
    return new Response(
      JSON.stringify({ error: 'Erro interno do servidor', details: errorMessage }),
      { status: 500, headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );
  }
});