-- ============================================
-- HYDRO SMART AI ENHANCED - DATABASE SCHEMA
-- ============================================

-- Habilitar extensão pgvector para embeddings vetoriais
CREATE EXTENSION IF NOT EXISTS vector;

-- ============================================
-- 1. TIPOS ENUMERADOS
-- ============================================

-- Fases de crescimento das plantas
CREATE TYPE growth_stage AS ENUM (
  'germination',      -- Germinação
  'seedling',         -- Muda
  'vegetative_early', -- Vegetativo inicial
  'vegetative_late',  -- Vegetativo tardio
  'pre_flowering',    -- Pré-floração
  'flowering_early',  -- Floração inicial
  'flowering_mid',    -- Floração média
  'flowering_late',   -- Floração tardia
  'ripening',         -- Maturação
  'harvest'           -- Colheita
);

-- Tipos de conteúdo de conhecimento
CREATE TYPE knowledge_content_type AS ENUM (
  'article',          -- Artigo
  'scientific_paper', -- Paper científico
  'forum_post',       -- Post de fórum
  'video_transcript', -- Transcrição de vídeo
  'pdf_document',     -- Documento PDF
  'manual',           -- Manual
  'guide',            -- Guia
  'case_study',       -- Estudo de caso
  'research',         -- Pesquisa
  'user_experience'   -- Experiência de usuário
);

-- Status de processamento
CREATE TYPE processing_status AS ENUM (
  'pending',    -- Pendente
  'processing', -- Processando
  'completed',  -- Completo
  'failed',     -- Falhou
  'archived'    -- Arquivado
);

-- ============================================
-- 2. TABELA DE PERFIS DE ESPÉCIES
-- ============================================

CREATE TABLE plant_species_profiles (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  
  -- Informações básicas
  species_name TEXT NOT NULL UNIQUE,
  common_names TEXT[],
  scientific_name TEXT,
  description TEXT,
  
  -- Parâmetros gerais ideais
  default_ph_min DECIMAL(3,1),
  default_ph_max DECIMAL(3,1),
  default_ec_min DECIMAL(5,1),
  default_ec_max DECIMAL(5,1),
  default_temp_min DECIMAL(4,1),
  default_temp_max DECIMAL(4,1),
  default_humidity_min DECIMAL(4,1),
  default_humidity_max DECIMAL(4,1),
  default_water_temp_min DECIMAL(4,1),
  default_water_temp_max DECIMAL(4,1),
  
  -- Características específicas
  growth_cycle_days INTEGER,
  difficulty_level TEXT CHECK (difficulty_level IN ('beginner', 'intermediate', 'advanced', 'expert')),
  
  -- Nutrientes específicos
  nitrogen_requirements JSONB, -- {stage: value}
  phosphorus_requirements JSONB,
  potassium_requirements JSONB,
  micronutrients JSONB,
  
  -- Sensibilidades e tolerâncias
  ph_sensitivity TEXT CHECK (ph_sensitivity IN ('low', 'medium', 'high', 'very_high')),
  nutrient_sensitivity TEXT CHECK (nutrient_sensitivity IN ('low', 'medium', 'high', 'very_high')),
  stress_tolerance TEXT CHECK (stress_tolerance IN ('low', 'medium', 'high', 'very_high')),
  
  -- Problemas comuns
  common_deficiencies TEXT[],
  common_pests TEXT[],
  common_diseases TEXT[],
  
  -- Notas e observações
  cultivation_notes TEXT,
  harvest_indicators TEXT[],
  
  -- Metadados
  source_references TEXT[],
  verified BOOLEAN DEFAULT FALSE,
  verification_date TIMESTAMP WITH TIME ZONE
);

-- ============================================
-- 3. PARÂMETROS POR FASE DE CRESCIMENTO
-- ============================================

CREATE TABLE species_stage_parameters (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  species_id UUID REFERENCES plant_species_profiles(id) ON DELETE CASCADE,
  growth_stage growth_stage NOT NULL,
  
  -- Parâmetros ideais para esta fase
  ph_min DECIMAL(3,1) NOT NULL,
  ph_max DECIMAL(3,1) NOT NULL,
  ec_min DECIMAL(5,1) NOT NULL,
  ec_max DECIMAL(5,1) NOT NULL,
  temp_min DECIMAL(4,1) NOT NULL,
  temp_max DECIMAL(4,1) NOT NULL,
  humidity_min DECIMAL(4,1) NOT NULL,
  humidity_max DECIMAL(4,1) NOT NULL,
  water_temp_min DECIMAL(4,1) NOT NULL,
  water_temp_max DECIMAL(4,1) NOT NULL,
  
  -- Ciclo de luz
  light_hours INTEGER,
  dark_hours INTEGER,
  light_intensity_min INTEGER, -- PPFD
  light_intensity_max INTEGER,
  
  -- Duração típica da fase
  duration_days_min INTEGER,
  duration_days_max INTEGER,
  
  -- Notas específicas da fase
  phase_notes TEXT,
  critical_parameters TEXT[], -- Parâmetros críticos nesta fase
  
  -- Recomendações
  feeding_schedule JSONB,
  maintenance_tasks TEXT[],
  
  UNIQUE(species_id, growth_stage)
);

-- ============================================
-- 4. BASE DE CONHECIMENTO
-- ============================================

CREATE TABLE knowledge_base (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  
  -- Metadados do conteúdo
  title TEXT NOT NULL,
  content_type knowledge_content_type NOT NULL,
  source_url TEXT,
  author TEXT,
  publication_date DATE,
  
  -- Conteúdo
  original_content TEXT, -- Conteúdo original completo
  processed_content TEXT, -- Conteúdo processado/limpo
  summary TEXT,
  
  -- Categorização
  topics TEXT[],
  species_related UUID[] DEFAULT '{}', -- IDs de espécies relacionadas
  growth_stages_related growth_stage[] DEFAULT '{}',
  
  -- Qualidade e relevância
  quality_score DECIMAL(3,2) CHECK (quality_score >= 0 AND quality_score <= 1),
  relevance_score DECIMAL(3,2) CHECK (relevance_score >= 0 AND relevance_score <= 1),
  verified BOOLEAN DEFAULT FALSE,
  verified_by UUID REFERENCES auth.users(id),
  verified_at TIMESTAMP WITH TIME ZONE,
  
  -- Status de processamento
  processing_status processing_status DEFAULT 'pending',
  processing_error TEXT,
  
  -- Metadados adicionais
  language TEXT DEFAULT 'pt',
  word_count INTEGER,
  metadata JSONB,
  
  -- Controle de uso
  usage_count INTEGER DEFAULT 0,
  last_used_at TIMESTAMP WITH TIME ZONE
);

-- ============================================
-- 5. EMBEDDINGS VETORIAIS (RAG)
-- ============================================

CREATE TABLE knowledge_embeddings (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  knowledge_id UUID REFERENCES knowledge_base(id) ON DELETE CASCADE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  
  -- Texto do chunk
  chunk_text TEXT NOT NULL,
  chunk_index INTEGER NOT NULL,
  
  -- Embedding vetorial (1536 dimensões para modelos comuns)
  embedding vector(1536) NOT NULL,
  
  -- Metadados do chunk
  token_count INTEGER,
  start_position INTEGER,
  end_position INTEGER,
  
  -- Índice para busca vetorial eficiente
  UNIQUE(knowledge_id, chunk_index)
);

-- Criar índice HNSW para busca vetorial rápida
CREATE INDEX knowledge_embeddings_vector_idx ON knowledge_embeddings 
USING hnsw (embedding vector_cosine_ops);

-- ============================================
-- 6. CONTEXTO DE CULTIVO
-- ============================================

CREATE TABLE cultivation_contexts (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  plant_id UUID REFERENCES plants(id) ON DELETE CASCADE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  
  -- Espécie e fase atual
  species_profile_id UUID REFERENCES plant_species_profiles(id),
  current_stage growth_stage NOT NULL,
  stage_started_at TIMESTAMP WITH TIME ZONE NOT NULL,
  
  -- Histórico de fases
  stage_history JSONB DEFAULT '[]'::jsonb,
  
  -- Objetivos do cultivo
  target_yield_grams DECIMAL(6,2),
  target_quality_metrics JSONB,
  cultivation_goals TEXT[],
  
  -- Ambiente específico
  system_type TEXT, -- hidropônico, aeropônico, aquapônico, etc.
  growing_medium TEXT,
  container_size_liters DECIMAL(6,2),
  
  -- Dados contextuais
  environmental_conditions JSONB,
  nutrient_regime JSONB,
  light_setup JSONB,
  
  -- Observações e ajustes
  grower_notes TEXT,
  adjustments_made JSONB DEFAULT '[]'::jsonb,
  issues_encountered JSONB DEFAULT '[]'::jsonb,
  
  -- Performance tracking
  metrics_tracking JSONB DEFAULT '{}'::jsonb,
  
  UNIQUE(plant_id)
);

-- ============================================
-- 7. CONTEÚDO PARA TREINAMENTO DA IA
-- ============================================

CREATE TABLE ai_training_content (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  
  -- Tipo e fonte
  content_type TEXT NOT NULL,
  source_type TEXT, -- 'manual_upload', 'web_scrape', 'api', 'user_contribution'
  source_url TEXT,
  
  -- Conteúdo
  raw_content TEXT,
  processed_content TEXT,
  
  -- Metadados de processamento
  processing_date TIMESTAMP WITH TIME ZONE,
  processing_method TEXT,
  extracted_entities JSONB,
  
  -- Classificação
  categories TEXT[],
  tags TEXT[],
  
  -- Controle de qualidade
  approved BOOLEAN DEFAULT FALSE,
  approved_by UUID REFERENCES auth.users(id),
  approved_at TIMESTAMP WITH TIME ZONE,
  
  -- Status
  status processing_status DEFAULT 'pending',
  
  -- Uso em insights
  used_in_insights_count INTEGER DEFAULT 0,
  effectiveness_score DECIMAL(3,2)
);

-- ============================================
-- 8. HISTÓRICO DE ANÁLISES DA IA
-- ============================================

CREATE TABLE ai_analysis_history (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  
  -- Contexto da análise
  plant_id UUID REFERENCES plants(id),
  cultivation_context_id UUID REFERENCES cultivation_contexts(id),
  
  -- Dados usados na análise
  sensor_data_snapshot JSONB NOT NULL,
  context_data JSONB,
  knowledge_sources_used UUID[], -- IDs de knowledge_base usados
  
  -- Resultado da análise
  insights_generated JSONB,
  recommendations JSONB,
  predictions JSONB,
  
  -- Métricas da análise
  confidence_score DECIMAL(3,2),
  processing_time_ms INTEGER,
  
  -- Feedback
  user_feedback TEXT,
  feedback_rating INTEGER CHECK (feedback_rating >= 1 AND feedback_rating <= 5),
  action_taken BOOLEAN DEFAULT FALSE
);

-- ============================================
-- 9. MELHORIAS NA TABELA PLANTS
-- ============================================

-- Adicionar colunas à tabela plants existente para integração
ALTER TABLE plants ADD COLUMN IF NOT EXISTS species_profile_id UUID REFERENCES plant_species_profiles(id);
ALTER TABLE plants ADD COLUMN IF NOT EXISTS current_growth_stage growth_stage DEFAULT 'germination';
ALTER TABLE plants ADD COLUMN IF NOT EXISTS stage_started_at TIMESTAMP WITH TIME ZONE DEFAULT NOW();

-- ============================================
-- 10. TRIGGERS PARA UPDATED_AT
-- ============================================

CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_plant_species_profiles_updated_at
    BEFORE UPDATE ON plant_species_profiles
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_knowledge_base_updated_at
    BEFORE UPDATE ON knowledge_base
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_cultivation_contexts_updated_at
    BEFORE UPDATE ON cultivation_contexts
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_ai_training_content_updated_at
    BEFORE UPDATE ON ai_training_content
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ============================================
-- 11. ROW LEVEL SECURITY (RLS)
-- ============================================

-- plant_species_profiles (público para leitura, admin para escrita)
ALTER TABLE plant_species_profiles ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Anyone can view species profiles"
  ON plant_species_profiles FOR SELECT
  USING (true);

CREATE POLICY "Authenticated users can insert species profiles"
  ON plant_species_profiles FOR INSERT
  WITH CHECK (auth.uid() IS NOT NULL);

CREATE POLICY "Users can update their own or unverified profiles"
  ON plant_species_profiles FOR UPDATE
  USING (auth.uid() IS NOT NULL);

-- species_stage_parameters
ALTER TABLE species_stage_parameters ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Anyone can view stage parameters"
  ON species_stage_parameters FOR SELECT
  USING (true);

CREATE POLICY "Authenticated users can manage stage parameters"
  ON species_stage_parameters FOR ALL
  USING (auth.uid() IS NOT NULL);

-- knowledge_base
ALTER TABLE knowledge_base ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Anyone can view verified knowledge"
  ON knowledge_base FOR SELECT
  USING (verified = true OR auth.uid() IS NOT NULL);

CREATE POLICY "Authenticated users can insert knowledge"
  ON knowledge_base FOR INSERT
  WITH CHECK (auth.uid() IS NOT NULL);

CREATE POLICY "Authenticated users can update their content"
  ON knowledge_base FOR UPDATE
  USING (auth.uid() IS NOT NULL);

-- knowledge_embeddings
ALTER TABLE knowledge_embeddings ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Service role can manage embeddings"
  ON knowledge_embeddings FOR ALL
  USING (true);

-- cultivation_contexts
ALTER TABLE cultivation_contexts ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Users can view their own contexts"
  ON cultivation_contexts FOR SELECT
  USING (
    EXISTS (
      SELECT 1 FROM plants
      WHERE plants.id = cultivation_contexts.plant_id
      AND plants.user_id = auth.uid()
    )
  );

CREATE POLICY "Users can manage their own contexts"
  ON cultivation_contexts FOR ALL
  USING (
    EXISTS (
      SELECT 1 FROM plants
      WHERE plants.id = cultivation_contexts.plant_id
      AND plants.user_id = auth.uid()
    )
  );

-- ai_training_content
ALTER TABLE ai_training_content ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Anyone can view approved content"
  ON ai_training_content FOR SELECT
  USING (approved = true OR auth.uid() IS NOT NULL);

CREATE POLICY "Authenticated users can contribute content"
  ON ai_training_content FOR INSERT
  WITH CHECK (auth.uid() IS NOT NULL);

-- ai_analysis_history
ALTER TABLE ai_analysis_history ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Users can view their own analysis history"
  ON ai_analysis_history FOR SELECT
  USING (
    EXISTS (
      SELECT 1 FROM plants
      WHERE plants.id = ai_analysis_history.plant_id
      AND plants.user_id = auth.uid()
    )
  );

CREATE POLICY "Service role can insert analysis history"
  ON ai_analysis_history FOR INSERT
  WITH CHECK (true);

-- ============================================
-- 12. ÍNDICES PARA PERFORMANCE
-- ============================================

-- Índices para buscas comuns
CREATE INDEX idx_knowledge_base_content_type ON knowledge_base(content_type);
CREATE INDEX idx_knowledge_base_verified ON knowledge_base(verified);
CREATE INDEX idx_knowledge_base_topics ON knowledge_base USING GIN(topics);
CREATE INDEX idx_knowledge_base_species_related ON knowledge_base USING GIN(species_related);

CREATE INDEX idx_cultivation_contexts_plant_id ON cultivation_contexts(plant_id);
CREATE INDEX idx_cultivation_contexts_species_profile_id ON cultivation_contexts(species_profile_id);
CREATE INDEX idx_cultivation_contexts_current_stage ON cultivation_contexts(current_stage);

CREATE INDEX idx_ai_analysis_history_plant_id ON ai_analysis_history(plant_id);
CREATE INDEX idx_ai_analysis_history_created_at ON ai_analysis_history(created_at DESC);

CREATE INDEX idx_species_stage_parameters_species_id ON species_stage_parameters(species_id);
CREATE INDEX idx_species_stage_parameters_growth_stage ON species_stage_parameters(growth_stage);

-- ============================================
-- 13. FUNÇÕES AUXILIARES
-- ============================================

-- Função para buscar conhecimento similar usando embeddings
CREATE OR REPLACE FUNCTION search_knowledge_by_vector(
  query_embedding vector(1536),
  match_threshold DECIMAL DEFAULT 0.7,
  match_count INTEGER DEFAULT 10
)
RETURNS TABLE (
  id UUID,
  knowledge_id UUID,
  chunk_text TEXT,
  similarity DECIMAL
)
LANGUAGE sql STABLE
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

-- Função para obter parâmetros ideais para uma espécie e fase
CREATE OR REPLACE FUNCTION get_ideal_parameters(
  p_species_id UUID,
  p_growth_stage growth_stage
)
RETURNS TABLE (
  ph_min DECIMAL,
  ph_max DECIMAL,
  ec_min DECIMAL,
  ec_max DECIMAL,
  temp_min DECIMAL,
  temp_max DECIMAL,
  humidity_min DECIMAL,
  humidity_max DECIMAL,
  water_temp_min DECIMAL,
  water_temp_max DECIMAL
)
LANGUAGE sql STABLE
AS $$
  SELECT
    ssp.ph_min,
    ssp.ph_max,
    ssp.ec_min,
    ssp.ec_max,
    ssp.temp_min,
    ssp.temp_max,
    ssp.humidity_min,
    ssp.humidity_max,
    ssp.water_temp_min,
    ssp.water_temp_max
  FROM species_stage_parameters ssp
  WHERE ssp.species_id = p_species_id
    AND ssp.growth_stage = p_growth_stage;
$$;

-- ============================================
-- MIGRAÇÃO COMPLETA!
-- ============================================