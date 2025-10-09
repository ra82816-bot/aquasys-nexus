import "https://deno.land/x/xhr@0.1.0/mod.ts";
import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
};

serve(async (req) => {
  if (req.method === 'OPTIONS') {
    return new Response(null, { headers: corsHeaders });
  }

  try {
    const { knowledgeId } = await req.json();

    if (!knowledgeId) {
      throw new Error('Knowledge ID is required');
    }

    const supabaseUrl = Deno.env.get('SUPABASE_URL')!;
    const supabaseKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
    const lovableApiKey = Deno.env.get('LOVABLE_API_KEY');

    if (!lovableApiKey) {
      throw new Error('LOVABLE_API_KEY not configured');
    }

    const supabase = createClient(supabaseUrl, supabaseKey);

    // Buscar o material de conhecimento
    const { data: knowledge, error: fetchError } = await supabase
      .from('knowledge')
      .select('*')
      .eq('id', knowledgeId)
      .single();

    if (fetchError || !knowledge) {
      throw new Error('Knowledge not found');
    }

    // Atualizar status para "processing"
    await supabase
      .from('knowledge')
      .update({ processing_status: 'processing' })
      .eq('id', knowledgeId);

    let content = '';

    // Se tiver arquivo, baixar e extrair conteúdo
    if (knowledge.file_path) {
      const { data: fileData, error: downloadError } = await supabase
        .storage
        .from('knowledge-materials')
        .download(knowledge.file_path);

      if (downloadError) {
        throw new Error(`Failed to download file: ${downloadError.message}`);
      }

      // Converter o arquivo para texto
      const fileType = knowledge.source_type;
      
      if (fileType === 'txt' || fileType === 'csv') {
        content = await fileData.text();
      } else if (fileType === 'pdf' || fileType === 'docx' || fileType === 'xlsx') {
        // Para PDFs e documentos complexos, usar AI para extrair conteúdo
        const arrayBuffer = await fileData.arrayBuffer();
        const base64 = btoa(String.fromCharCode(...new Uint8Array(arrayBuffer)));
        
        const extractResponse = await fetch('https://ai.gateway.lovable.dev/v1/chat/completions', {
          method: 'POST',
          headers: {
            'Authorization': `Bearer ${lovableApiKey}`,
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            model: 'google/gemini-2.5-flash',
            messages: [
              {
                role: 'user',
                content: `Extraia todo o texto e informações relevantes deste documento sobre cultivo hidropônico de Cannabis. 
                Organize o conteúdo de forma estruturada, mantendo todas as informações técnicas, parâmetros, e recomendações.
                
                Documento: ${knowledge.title}
                Descrição: ${knowledge.description || 'N/A'}`
              }
            ]
          })
        });

        const extractData = await extractResponse.json();
        content = extractData.choices?.[0]?.message?.content || '';
      }
    } else if (knowledge.source_type === 'link' && knowledge.description) {
      // Para links, usar o conteúdo da descrição como base
      content = knowledge.description;
    }

    if (!content) {
      throw new Error('No content to process');
    }

    // Criar embeddings usando Lovable AI
    const embeddingResponse = await fetch('https://ai.gateway.lovable.dev/v1/embeddings', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${lovableApiKey}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        model: 'text-embedding-3-small',
        input: content.substring(0, 8000) // Limitar para não exceder tokens
      })
    });

    if (!embeddingResponse.ok) {
      throw new Error('Failed to create embeddings');
    }

    const embeddingData = await embeddingResponse.json();
    const embedding = embeddingData.data?.[0]?.embedding;

    if (!embedding) {
      throw new Error('No embedding generated');
    }

    // Salvar embedding
    const { error: embeddingError } = await supabase
      .from('knowledge_embeddings')
      .insert({
        knowledge_id: knowledgeId,
        chunk_text: content.substring(0, 8000),
        chunk_index: 0,
        embedding: embedding,
        token_count: content.length
      });

    if (embeddingError) {
      console.error('Embedding error:', embeddingError);
    }

    // Atualizar conhecimento com conteúdo e status
    await supabase
      .from('knowledge')
      .update({
        content: content,
        processing_status: 'indexed',
        updated_at: new Date().toISOString()
      })
      .eq('id', knowledgeId);

    console.log('Knowledge processed successfully:', knowledgeId);

    return new Response(
      JSON.stringify({ 
        success: true,
        message: 'Knowledge processed successfully',
        contentLength: content.length
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Error processing knowledge:', error);

    // Tentar atualizar o status para erro
    try {
      const supabaseUrl = Deno.env.get('SUPABASE_URL')!;
      const supabaseKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
      const supabase = createClient(supabaseUrl, supabaseKey);

      const { knowledgeId } = await req.json();
      if (knowledgeId) {
        await supabase
          .from('knowledge')
          .update({
            processing_status: 'error',
            error_message: error instanceof Error ? error.message : 'Unknown error'
          })
          .eq('id', knowledgeId);
      }
    } catch (updateError) {
      console.error('Failed to update error status:', updateError);
    }

    return new Response(
      JSON.stringify({ 
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error'
      }),
      { 
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' }
      }
    );
  }
});
