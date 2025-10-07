-- Criar enum para tipo de origem da planta
CREATE TYPE plant_origin AS ENUM ('seed', 'clone');

-- Criar enum para sexo da planta
CREATE TYPE plant_sex AS ENUM ('unknown', 'female', 'male', 'hermaphrodite');

-- Criar enum para status da planta
CREATE TYPE plant_status AS ENUM ('germinating', 'vegetative', 'flowering', 'harvested', 'discontinued');

-- Tabela de plantas
CREATE TABLE public.plants (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id UUID REFERENCES auth.users(id) ON DELETE CASCADE NOT NULL,
  registration_number TEXT,
  nickname TEXT NOT NULL,
  species TEXT,
  genetics TEXT,
  origin plant_origin NOT NULL DEFAULT 'seed',
  germination_date DATE,
  transplant_date DATE,
  sex plant_sex DEFAULT 'unknown',
  harvest_date DATE,
  yield_grams NUMERIC(10,2),
  status plant_status DEFAULT 'germinating',
  
  -- Dados de cultivo
  substrate_type TEXT,
  nutrients_type TEXT,
  nutrients_frequency TEXT,
  light_cycle TEXT,
  avg_temperature NUMERIC(5,2),
  avg_humidity NUMERIC(5,2),
  avg_ph NUMERIC(4,2),
  
  -- Problemas e ajustes
  pests_diseases TEXT,
  stress_events TEXT,
  adjustments_made TEXT,
  
  -- Qualidade final
  quality_aroma INTEGER CHECK (quality_aroma >= 1 AND quality_aroma <= 10),
  quality_density INTEGER CHECK (quality_density >= 1 AND quality_density <= 10),
  quality_color INTEGER CHECK (quality_color >= 1 AND quality_color <= 10),
  quality_resin INTEGER CHECK (quality_resin >= 1 AND quality_resin <= 10),
  
  general_notes TEXT,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Tabela de observações diárias/periódicas das plantas
CREATE TABLE public.plant_observations (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  plant_id UUID REFERENCES public.plants(id) ON DELETE CASCADE NOT NULL,
  observation_date DATE NOT NULL,
  height_cm NUMERIC(6,2),
  notes TEXT,
  images TEXT[], -- URLs de imagens
  ph_level NUMERIC(4,2),
  ec_level NUMERIC(10,2),
  temperature NUMERIC(5,2),
  humidity NUMERIC(5,2),
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Tabela de posts do fórum
CREATE TABLE public.forum_posts (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id UUID REFERENCES auth.users(id) ON DELETE CASCADE NOT NULL,
  title TEXT NOT NULL,
  content TEXT NOT NULL,
  images TEXT[], -- URLs de imagens
  plant_id UUID REFERENCES public.plants(id) ON DELETE SET NULL,
  sensor_data JSONB, -- Dados de sensores compartilhados
  ai_report JSONB, -- Relatórios de IA compartilhados
  is_anonymous BOOLEAN DEFAULT FALSE,
  views_count INTEGER DEFAULT 0,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Tabela de comentários
CREATE TABLE public.forum_comments (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  post_id UUID REFERENCES public.forum_posts(id) ON DELETE CASCADE NOT NULL,
  user_id UUID REFERENCES auth.users(id) ON DELETE CASCADE NOT NULL,
  parent_comment_id UUID REFERENCES public.forum_comments(id) ON DELETE CASCADE,
  content TEXT NOT NULL,
  is_anonymous BOOLEAN DEFAULT FALSE,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Tabela de curtidas
CREATE TABLE public.forum_likes (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  post_id UUID REFERENCES public.forum_posts(id) ON DELETE CASCADE,
  comment_id UUID REFERENCES public.forum_comments(id) ON DELETE CASCADE,
  user_id UUID REFERENCES auth.users(id) ON DELETE CASCADE NOT NULL,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  UNIQUE(user_id, post_id),
  UNIQUE(user_id, comment_id),
  CHECK ((post_id IS NOT NULL AND comment_id IS NULL) OR (post_id IS NULL AND comment_id IS NOT NULL))
);

-- Tabela de dados compartilhados anonimamente para IA
CREATE TABLE public.shared_cultivation_data (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  plant_id UUID REFERENCES public.plants(id) ON DELETE SET NULL,
  genetics TEXT,
  substrate_type TEXT,
  nutrients_data JSONB,
  environmental_data JSONB,
  growth_data JSONB,
  final_yield NUMERIC(10,2),
  quality_metrics JSONB,
  issues_encountered TEXT[],
  successful_adjustments TEXT[],
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Habilitar RLS em todas as tabelas
ALTER TABLE public.plants ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.plant_observations ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.forum_posts ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.forum_comments ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.forum_likes ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.shared_cultivation_data ENABLE ROW LEVEL SECURITY;

-- Políticas RLS para plants
CREATE POLICY "Users can view their own plants"
  ON public.plants FOR SELECT
  USING (auth.uid() = user_id);

CREATE POLICY "Users can insert their own plants"
  ON public.plants FOR INSERT
  WITH CHECK (auth.uid() = user_id);

CREATE POLICY "Users can update their own plants"
  ON public.plants FOR UPDATE
  USING (auth.uid() = user_id);

CREATE POLICY "Users can delete their own plants"
  ON public.plants FOR DELETE
  USING (auth.uid() = user_id);

-- Políticas RLS para plant_observations
CREATE POLICY "Users can view their own plant observations"
  ON public.plant_observations FOR SELECT
  USING (EXISTS (
    SELECT 1 FROM public.plants 
    WHERE plants.id = plant_observations.plant_id 
    AND plants.user_id = auth.uid()
  ));

CREATE POLICY "Users can insert their own plant observations"
  ON public.plant_observations FOR INSERT
  WITH CHECK (EXISTS (
    SELECT 1 FROM public.plants 
    WHERE plants.id = plant_observations.plant_id 
    AND plants.user_id = auth.uid()
  ));

CREATE POLICY "Users can update their own plant observations"
  ON public.plant_observations FOR UPDATE
  USING (EXISTS (
    SELECT 1 FROM public.plants 
    WHERE plants.id = plant_observations.plant_id 
    AND plants.user_id = auth.uid()
  ));

CREATE POLICY "Users can delete their own plant observations"
  ON public.plant_observations FOR DELETE
  USING (EXISTS (
    SELECT 1 FROM public.plants 
    WHERE plants.id = plant_observations.plant_id 
    AND plants.user_id = auth.uid()
  ));

-- Políticas RLS para forum_posts
CREATE POLICY "Anyone can view forum posts"
  ON public.forum_posts FOR SELECT
  USING (true);

CREATE POLICY "Authenticated users can create posts"
  ON public.forum_posts FOR INSERT
  WITH CHECK (auth.uid() = user_id);

CREATE POLICY "Users can update their own posts"
  ON public.forum_posts FOR UPDATE
  USING (auth.uid() = user_id);

CREATE POLICY "Users can delete their own posts"
  ON public.forum_posts FOR DELETE
  USING (auth.uid() = user_id);

-- Políticas RLS para forum_comments
CREATE POLICY "Anyone can view comments"
  ON public.forum_comments FOR SELECT
  USING (true);

CREATE POLICY "Authenticated users can create comments"
  ON public.forum_comments FOR INSERT
  WITH CHECK (auth.uid() = user_id);

CREATE POLICY "Users can update their own comments"
  ON public.forum_comments FOR UPDATE
  USING (auth.uid() = user_id);

CREATE POLICY "Users can delete their own comments"
  ON public.forum_comments FOR DELETE
  USING (auth.uid() = user_id);

-- Políticas RLS para forum_likes
CREATE POLICY "Anyone can view likes"
  ON public.forum_likes FOR SELECT
  USING (true);

CREATE POLICY "Authenticated users can like"
  ON public.forum_likes FOR INSERT
  WITH CHECK (auth.uid() = user_id);

CREATE POLICY "Users can remove their own likes"
  ON public.forum_likes FOR DELETE
  USING (auth.uid() = user_id);

-- Políticas RLS para shared_cultivation_data
CREATE POLICY "Anyone can view shared data"
  ON public.shared_cultivation_data FOR SELECT
  USING (true);

CREATE POLICY "Service role can insert shared data"
  ON public.shared_cultivation_data FOR INSERT
  WITH CHECK (true);

-- Trigger para atualizar updated_at
CREATE OR REPLACE FUNCTION public.update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
  NEW.updated_at = NOW();
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_plants_updated_at
  BEFORE UPDATE ON public.plants
  FOR EACH ROW
  EXECUTE FUNCTION public.update_updated_at_column();

CREATE TRIGGER update_forum_posts_updated_at
  BEFORE UPDATE ON public.forum_posts
  FOR EACH ROW
  EXECUTE FUNCTION public.update_updated_at_column();

CREATE TRIGGER update_forum_comments_updated_at
  BEFORE UPDATE ON public.forum_comments
  FOR EACH ROW
  EXECUTE FUNCTION public.update_updated_at_column();

-- Índices para performance
CREATE INDEX idx_plants_user_id ON public.plants(user_id);
CREATE INDEX idx_plants_status ON public.plants(status);
CREATE INDEX idx_plant_observations_plant_id ON public.plant_observations(plant_id);
CREATE INDEX idx_forum_posts_user_id ON public.forum_posts(user_id);
CREATE INDEX idx_forum_posts_created_at ON public.forum_posts(created_at DESC);
CREATE INDEX idx_forum_comments_post_id ON public.forum_comments(post_id);
CREATE INDEX idx_forum_likes_post_id ON public.forum_likes(post_id);
CREATE INDEX idx_forum_likes_user_id ON public.forum_likes(user_id);