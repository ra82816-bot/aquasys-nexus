-- Criar tabela para armazenar insights gerados pela IA
CREATE TABLE public.ai_insights (
  id UUID NOT NULL DEFAULT gen_random_uuid() PRIMARY KEY,
  created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT now(),
  insight_type TEXT NOT NULL,
  title TEXT NOT NULL,
  description TEXT NOT NULL,
  severity TEXT NOT NULL CHECK (severity IN ('info', 'warning', 'critical')),
  recommendations TEXT[],
  data_points JSONB,
  is_active BOOLEAN NOT NULL DEFAULT true
);

-- Enable RLS
ALTER TABLE public.ai_insights ENABLE ROW LEVEL SECURITY;

-- Policy para permitir visualização dos insights
CREATE POLICY "Anyone can view active insights"
ON public.ai_insights
FOR SELECT
USING (is_active = true);

-- Policy para service role inserir insights
CREATE POLICY "Service role can insert insights"
ON public.ai_insights
FOR INSERT
WITH CHECK (true);

-- Policy para service role atualizar insights
CREATE POLICY "Service role can update insights"
ON public.ai_insights
FOR UPDATE
USING (true);

-- Criar índices para melhor performance
CREATE INDEX idx_ai_insights_created_at ON public.ai_insights(created_at DESC);
CREATE INDEX idx_ai_insights_active ON public.ai_insights(is_active) WHERE is_active = true;
CREATE INDEX idx_ai_insights_severity ON public.ai_insights(severity);