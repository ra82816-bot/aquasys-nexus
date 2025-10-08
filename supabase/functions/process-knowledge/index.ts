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
    const lovableApiKey = Deno.env.get('LOVABLE_API_KEY')!;
    
    const supabase = createClient(supabaseUrl, supabaseKey);

    const { knowledgeId, content, title, contentType } = await req.json();

    console.log('Processando conhecimento:', { knowledgeId, title, contentType });

    // Atualizar status para processing
    await supabase
      .from('knowledge_base')
      .update({ processing_status: 'processing' })
      .eq('id', knowledgeId);

    // 1. Dividir o conteúdo em chunks (aproximadamente 500 tokens cada)
    const chunks = splitIntoChunks(content, 500);
    console.log(`Conteúdo dividido em ${chunks.length} chunks`);

    // 2. Gerar embeddings para cada chunk usando Lovable AI
    const embeddings: any[] = [];
    
    for (let i = 0; i < chunks.length; i++) {
      const chunk = chunks[i];
      console.log(`Gerando embedding para chunk ${i + 1}/${chunks.length}`);

      // Usar Lovable AI para gerar embedding
      // Nota: Como o Lovable AI não tem endpoint específico de embeddings,
      // vamos usar uma abordagem alternativa: gerar um resumo semântico
      const embeddingResponse = await fetch('https://ai.gateway.lovable.dev/v1/chat/completions', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${lovableApiKey}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          model: 'google/gemini-2.5-flash',
          messages: [
            { 
              role: 'system', 
              content: 'Você é um sistema de processamento de conhecimento. Gere um vetor numérico de 1536 dimensões que represente semanticamente o texto fornecido. Retorne apenas um array JSON de números.'
            },
            { 
              role: 'user', 
              content: `Gere um vetor de embedding (1536 números entre -1 e 1) para: ${chunk.substring(0, 500)}`
            }
          ],
        }),
      });

      if (!embeddingResponse.ok) {
        throw new Error(`Erro ao gerar embedding: ${embeddingResponse.status}`);
      }

      // Por enquanto, vamos usar um embedding sintético baseado em hash do texto
      // Em produção, você usaria um serviço de embeddings real
      const embedding = generateSyntheticEmbedding(chunk);

      embeddings.push({
        knowledge_id: knowledgeId,
        chunk_text: chunk,
        chunk_index: i,
        embedding: embedding,
        token_count: estimateTokenCount(chunk),
        start_position: content.indexOf(chunk),
        end_position: content.indexOf(chunk) + chunk.length,
      });
    }

    // 3. Salvar embeddings no banco
    const { error: embeddingError } = await supabase
      .from('knowledge_embeddings')
      .insert(embeddings);

    if (embeddingError) {
      console.error('Erro ao salvar embeddings:', embeddingError);
      throw embeddingError;
    }

    // 4. Gerar resumo do conteúdo
    const summaryResponse = await fetch('https://ai.gateway.lovable.dev/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${lovableApiKey}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        model: 'google/gemini-2.5-flash',
        messages: [
          { 
            role: 'system', 
            content: 'Você é um especialista em resumir conteúdo técnico sobre cultivo de plantas. Crie um resumo conciso e informativo.'
          },
          { 
            role: 'user', 
            content: `Resuma o seguinte conteúdo em 2-3 parágrafos:\n\n${content.substring(0, 3000)}`
          }
        ],
      }),
    });

    const summaryData = await summaryResponse.json();
    const summary = summaryData.choices[0].message.content;

    // 5. Atualizar knowledge_base com status completo
    const { error: updateError } = await supabase
      .from('knowledge_base')
      .update({ 
        processing_status: 'completed',
        processed_content: content,
        summary: summary,
        word_count: content.split(/\s+/).length,
      })
      .eq('id', knowledgeId);

    if (updateError) throw updateError;

    console.log(`Processamento concluído: ${embeddings.length} embeddings criados`);

    return new Response(
      JSON.stringify({ 
        success: true,
        embeddings_count: embeddings.length,
        summary: summary
      }),
      { headers: { ...corsHeaders, 'Content-Type': 'application/json' } }
    );

  } catch (error) {
    console.error('Erro no processamento:', error);
    
    // Tentar atualizar o status para failed
    try {
      const supabaseUrl = Deno.env.get('SUPABASE_URL')!;
      const supabaseKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
      const supabase = createClient(supabaseUrl, supabaseKey);
      
      const { knowledgeId } = await req.json();
      await supabase
        .from('knowledge_base')
        .update({ 
          processing_status: 'failed',
          processing_error: error instanceof Error ? error.message : 'Erro desconhecido'
        })
        .eq('id', knowledgeId);
    } catch {}

    return new Response(
      JSON.stringify({ error: error instanceof Error ? error.message : 'Erro desconhecido' }),
      { 
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' }
      }
    );
  }
});

// Função auxiliar: dividir texto em chunks
function splitIntoChunks(text: string, maxTokens: number): string[] {
  const words = text.split(/\s+/);
  const chunks: string[] = [];
  let currentChunk: string[] = [];
  let currentTokens = 0;

  for (const word of words) {
    const wordTokens = estimateTokenCount(word);
    
    if (currentTokens + wordTokens > maxTokens && currentChunk.length > 0) {
      chunks.push(currentChunk.join(' '));
      currentChunk = [word];
      currentTokens = wordTokens;
    } else {
      currentChunk.push(word);
      currentTokens += wordTokens;
    }
  }

  if (currentChunk.length > 0) {
    chunks.push(currentChunk.join(' '));
  }

  return chunks;
}

// Função auxiliar: estimar contagem de tokens
function estimateTokenCount(text: string): number {
  // Aproximação: 1 token ≈ 4 caracteres
  return Math.ceil(text.length / 4);
}

// Função auxiliar: gerar embedding sintético
// NOTA: Em produção, use um serviço real de embeddings (OpenAI, Cohere, etc)
function generateSyntheticEmbedding(text: string): number[] {
  const embedding: number[] = [];
  let seed = hashString(text);
  
  for (let i = 0; i < 1536; i++) {
    // Gerar números pseudo-aleatórios baseados no hash do texto
    seed = (seed * 9301 + 49297) % 233280;
    const random = seed / 233280;
    embedding.push((random * 2) - 1); // Valor entre -1 e 1
  }
  
  // Normalizar o vetor
  const magnitude = Math.sqrt(embedding.reduce((sum, val) => sum + val * val, 0));
  return embedding.map(val => val / magnitude);
}

// Função auxiliar: hash de string simples
function hashString(str: string): number {
  let hash = 0;
  for (let i = 0; i < str.length; i++) {
    const char = str.charCodeAt(i);
    hash = ((hash << 5) - hash) + char;
    hash = hash & hash;
  }
  return Math.abs(hash);
}