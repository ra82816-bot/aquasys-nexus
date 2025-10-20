import { serve } from "https://deno.land/std@0.168.0/http/server.ts";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

serve(async (req) => {
  // Handle CORS preflight requests
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const { url, username, password } = await req.json();

    console.log(`[Camera Proxy] Fetching stream from: ${url}`);

    // Construir headers de autenticação se fornecidos
    const headers: Record<string, string> = {};
    
    if (username && password) {
      const auth = btoa(`${username}:${password}`);
      headers['Authorization'] = `Basic ${auth}`;
      console.log(`[Camera Proxy] Using authentication for user: ${username}`);
    }

    // Fazer requisição para a câmera
    const response = await fetch(url, {
      headers,
      method: 'GET',
    });

    if (!response.ok) {
      console.error(`[Camera Proxy] Error fetching camera: ${response.status} ${response.statusText}`);
      return new Response(
        JSON.stringify({ 
          error: `Failed to fetch camera stream: ${response.status} ${response.statusText}` 
        }),
        {
          status: response.status,
          headers: { ...corsHeaders, 'Content-Type': 'application/json' },
        }
      );
    }

    console.log(`[Camera Proxy] Successfully fetched stream, Content-Type: ${response.headers.get('content-type')}`);

    // Retornar o stream com headers CORS corretos
    const contentType = response.headers.get('content-type') || 'image/jpeg';
    
    return new Response(response.body, {
      headers: {
        ...corsHeaders,
        'Content-Type': contentType,
        'Cache-Control': 'no-cache, no-store, must-revalidate',
        'Pragma': 'no-cache',
        'Expires': '0',
      },
    });

  } catch (error) {
    console.error('[Camera Proxy] Error:', error);
    const errorMessage = error instanceof Error ? error.message : 'Internal server error';
    return new Response(
      JSON.stringify({ 
        error: errorMessage,
        details: 'Failed to proxy camera stream'
      }),
      {
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' },
      }
    );
  }
});
