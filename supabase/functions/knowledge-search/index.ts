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

    const { query, limit = 10, contentType, verifiedOnly = true } = await req.json();

    console.log('Buscando conhecimento:', { query, limit, contentType });

    // Busca por texto (fallback simples sem embeddings)
    let queryBuilder = supabase
      .from('knowledge_base')
      .select(`
        id,
        title,
        content_type,
        summary,
        processed_content,
        topics,
        quality_score,
        relevance_score,
        source_url,
        created_at
      `)
      .order('created_at', { ascending: false })
      .limit(limit);

    if (verifiedOnly) {
      queryBuilder = queryBuilder.eq('verified', true);
    }

    if (contentType) {
      queryBuilder = queryBuilder.eq('content_type', contentType);
    }

    // Busca por texto no título ou conteúdo processado
    if (query) {
      queryBuilder = queryBuilder.or(`title.ilike.%${query}%,processed_content.ilike.%${query}%`);
    }

    const { data, error } = await queryBuilder;

    if (error) throw error;

    // Incrementar contador de uso
    if (data && data.length > 0) {
      const ids = data.map(d => d.id);
      await supabase
        .from('knowledge_base')
        .update({ 
          usage_count: supabase.rpc('increment_usage_count'),
          last_used_at: new Date().toISOString()
        })
        .in('id', ids);
    }

    return new Response(
      JSON.stringify({ 
        results: data || [],
        count: data?.length || 0
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Erro na busca:', error);
    return new Response(
      JSON.stringify({ error: error instanceof Error ? error.message : 'Erro desconhecido' }),
      { 
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' }
      }
    );
  }
});