-- Criar tabela knowledge para armazenar materiais de treinamento
CREATE TABLE knowledge (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE CASCADE,
  title TEXT NOT NULL,
  description TEXT,
  category TEXT,
  source_type TEXT CHECK (source_type IN ('pdf', 'txt', 'csv', 'docx', 'xlsx', 'link')),
  is_trusted BOOLEAN DEFAULT false,
  processing_status TEXT DEFAULT 'pending' CHECK (processing_status IN ('pending', 'processing', 'indexed', 'error')),
  file_path TEXT,
  file_size BIGINT,
  error_message TEXT,
  content TEXT,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT now(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

-- Habilitar RLS
ALTER TABLE knowledge ENABLE ROW LEVEL SECURITY;

-- Políticas RLS para knowledge
CREATE POLICY "Users can view their own knowledge"
ON knowledge
FOR SELECT
TO authenticated
USING (auth.uid() = user_id);

CREATE POLICY "Users can insert their own knowledge"
ON knowledge
FOR INSERT
TO authenticated
WITH CHECK (auth.uid() = user_id);

CREATE POLICY "Users can update their own knowledge"
ON knowledge
FOR UPDATE
TO authenticated
USING (auth.uid() = user_id);

CREATE POLICY "Users can delete their own knowledge"
ON knowledge
FOR DELETE
TO authenticated
USING (auth.uid() = user_id);

-- Criar índices
CREATE INDEX idx_knowledge_category ON knowledge(category);
CREATE INDEX idx_knowledge_status ON knowledge(processing_status);
CREATE INDEX idx_knowledge_user_created ON knowledge(user_id, created_at DESC);

-- Criar bucket para armazenamento de materiais de treinamento
INSERT INTO storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
VALUES (
  'knowledge-materials',
  'knowledge-materials',
  false,
  52428800, -- 50MB por arquivo
  ARRAY[
    'application/pdf',
    'text/plain',
    'text/csv',
    'application/vnd.openxmlformats-officedocument.wordprocessingml.document',
    'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
    'application/msword',
    'application/vnd.ms-excel'
  ]
);

-- Políticas de storage para materiais
CREATE POLICY "Users can upload their own materials"
ON storage.objects
FOR INSERT
TO authenticated
WITH CHECK (
  bucket_id = 'knowledge-materials' AND
  auth.uid()::text = (storage.foldername(name))[1]
);

CREATE POLICY "Users can view their own materials"
ON storage.objects
FOR SELECT
TO authenticated
USING (
  bucket_id = 'knowledge-materials' AND
  auth.uid()::text = (storage.foldername(name))[1]
);

CREATE POLICY "Users can delete their own materials"
ON storage.objects
FOR DELETE
TO authenticated
USING (
  bucket_id = 'knowledge-materials' AND
  auth.uid()::text = (storage.foldername(name))[1]
);

-- Função para calcular total de storage usado por usuário
CREATE OR REPLACE FUNCTION get_user_storage_usage(p_user_id UUID)
RETURNS BIGINT
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
BEGIN
  RETURN COALESCE(
    (SELECT SUM(file_size) FROM knowledge WHERE user_id = p_user_id),
    0
  );
END;
$$;

-- Trigger para atualizar timestamp
CREATE TRIGGER update_knowledge_updated_at
  BEFORE UPDATE ON knowledge
  FOR EACH ROW
  EXECUTE FUNCTION update_updated_at_column();