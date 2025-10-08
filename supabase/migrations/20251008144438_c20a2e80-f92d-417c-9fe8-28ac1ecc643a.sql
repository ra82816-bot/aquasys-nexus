-- ============================================
-- CORREÇÕES DE SEGURANÇA
-- ============================================

-- 1. Corrigir search_path das funções existentes
ALTER FUNCTION update_updated_at_column() SET search_path = public;
ALTER FUNCTION search_knowledge_by_vector(vector, DECIMAL, INTEGER) SET search_path = public;
ALTER FUNCTION get_ideal_parameters(UUID, growth_stage) SET search_path = public;

-- 2. Mover extensão vector para schema extensions (padrão Supabase)
CREATE SCHEMA IF NOT EXISTS extensions;
ALTER EXTENSION vector SET SCHEMA extensions;

-- Atualizar função que usa vector para referenciar o schema correto
DROP FUNCTION IF EXISTS search_knowledge_by_vector(vector, DECIMAL, INTEGER);

CREATE OR REPLACE FUNCTION search_knowledge_by_vector(
  query_embedding extensions.vector(1536),
  match_threshold DECIMAL DEFAULT 0.7,
  match_count INTEGER DEFAULT 10
)
RETURNS TABLE (
  id UUID,
  knowledge_id UUID,
  chunk_text TEXT,
  similarity DECIMAL
)
LANGUAGE sql
STABLE
SECURITY DEFINER
SET search_path = public, extensions
AS $$
  SELECT
    ke.id,
    ke.knowledge_id,
    ke.chunk_text,
    1 - (ke.embedding <=> query_embedding) as similarity
  FROM knowledge_embeddings ke
  WHERE 1 - (ke.embedding <=> query_embedding) > match_threshold
  ORDER BY ke.embedding <=> query_embedding
  LIMIT match_count;
$$;