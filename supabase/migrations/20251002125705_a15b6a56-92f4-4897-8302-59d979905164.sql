-- Criar tabela para armazenar histórico de status dos relés
CREATE TABLE public.relay_status (
  id SERIAL PRIMARY KEY,
  timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
  relay1_led BOOLEAN NOT NULL,
  relay2_pump BOOLEAN NOT NULL,
  relay3_ph_up BOOLEAN NOT NULL,
  relay4_fan BOOLEAN NOT NULL,
  relay5_humidity BOOLEAN NOT NULL,
  relay6_ec BOOLEAN NOT NULL,
  relay7_co2 BOOLEAN NOT NULL,
  relay8_generic BOOLEAN NOT NULL
);

-- Habilitar RLS
ALTER TABLE public.relay_status ENABLE ROW LEVEL SECURITY;

-- Políticas de acesso
CREATE POLICY "Qualquer um pode visualizar status dos relés"
  ON public.relay_status
  FOR SELECT
  USING (true);

CREATE POLICY "Service role pode inserir status dos relés"
  ON public.relay_status
  FOR INSERT
  WITH CHECK (true);

-- Criar tabela para armazenar comandos de controle manual
CREATE TABLE public.relay_commands (
  id SERIAL PRIMARY KEY,
  timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
  relay_index INTEGER NOT NULL,
  command BOOLEAN NOT NULL,
  executed BOOLEAN NOT NULL DEFAULT FALSE
);

-- Habilitar RLS
ALTER TABLE public.relay_commands ENABLE ROW LEVEL SECURITY;

-- Políticas de acesso
CREATE POLICY "Usuários autenticados podem inserir comandos"
  ON public.relay_commands
  FOR INSERT
  WITH CHECK (auth.uid() IS NOT NULL);

CREATE POLICY "Qualquer um pode visualizar comandos"
  ON public.relay_commands
  FOR SELECT
  USING (true);

CREATE POLICY "Service role pode atualizar comandos"
  ON public.relay_commands
  FOR UPDATE
  USING (true);

-- Habilitar realtime para relay_status
ALTER PUBLICATION supabase_realtime ADD TABLE public.relay_status;