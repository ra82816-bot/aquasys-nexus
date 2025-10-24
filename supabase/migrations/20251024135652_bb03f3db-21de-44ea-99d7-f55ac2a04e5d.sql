-- Tabela de comandos para dispositivos
CREATE TABLE public.device_commands (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  device_id UUID REFERENCES public.devices(id) ON DELETE CASCADE,
  command_type TEXT NOT NULL,
  command_data JSONB NOT NULL,
  status TEXT DEFAULT 'pending',
  executed_at TIMESTAMP WITH TIME ZONE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Tabela de perfis de calibração
CREATE TABLE public.device_calibration_profiles (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  device_id UUID REFERENCES public.devices(id) ON DELETE CASCADE,
  sensor_type TEXT NOT NULL,
  calibration_data JSONB NOT NULL,
  profile_name TEXT NOT NULL,
  is_active BOOLEAN DEFAULT true,
  created_by UUID REFERENCES auth.users(id),
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Tabela de rate limiting
CREATE TABLE public.mqtt_rate_limits (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  device_id UUID REFERENCES public.devices(id) ON DELETE CASCADE,
  endpoint TEXT NOT NULL,
  request_count INTEGER DEFAULT 1,
  window_start TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  blocked_until TIMESTAMP WITH TIME ZONE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Enable RLS
ALTER TABLE public.device_commands ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.device_calibration_profiles ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.mqtt_rate_limits ENABLE ROW LEVEL SECURITY;

-- RLS Policies para device_commands
CREATE POLICY "Users can view commands for their devices"
  ON public.device_commands FOR SELECT
  USING (device_id IN (
    SELECT device_id FROM public.device_owners WHERE user_id = auth.uid()
  ));

CREATE POLICY "Users can insert commands for their devices"
  ON public.device_commands FOR INSERT
  WITH CHECK (device_id IN (
    SELECT device_id FROM public.device_owners WHERE user_id = auth.uid()
  ));

CREATE POLICY "Service role can manage all commands"
  ON public.device_commands FOR ALL
  USING (true);

-- RLS Policies para device_calibration_profiles
CREATE POLICY "Users can view calibration profiles for their devices"
  ON public.device_calibration_profiles FOR SELECT
  USING (device_id IN (
    SELECT device_id FROM public.device_owners WHERE user_id = auth.uid()
  ));

CREATE POLICY "Users can create calibration profiles for their devices"
  ON public.device_calibration_profiles FOR INSERT
  WITH CHECK (device_id IN (
    SELECT device_id FROM public.device_owners WHERE user_id = auth.uid()
  ));

CREATE POLICY "Users can update calibration profiles for their devices"
  ON public.device_calibration_profiles FOR UPDATE
  USING (device_id IN (
    SELECT device_id FROM public.device_owners WHERE user_id = auth.uid()
  ));

-- RLS Policies para mqtt_rate_limits
CREATE POLICY "Service role can manage rate limits"
  ON public.mqtt_rate_limits FOR ALL
  USING (true);

-- Índices para performance
CREATE INDEX idx_device_commands_device_id ON public.device_commands(device_id);
CREATE INDEX idx_device_commands_status ON public.device_commands(status);
CREATE INDEX idx_calibration_profiles_device_id ON public.device_calibration_profiles(device_id);
CREATE INDEX idx_calibration_profiles_active ON public.device_calibration_profiles(is_active);
CREATE INDEX idx_rate_limits_device_id ON public.mqtt_rate_limits(device_id);
CREATE INDEX idx_rate_limits_window ON public.mqtt_rate_limits(window_start);

-- Trigger para updated_at
CREATE TRIGGER update_device_commands_updated_at
  BEFORE UPDATE ON public.device_commands
  FOR EACH ROW
  EXECUTE FUNCTION public.update_updated_at_column();

CREATE TRIGGER update_calibration_profiles_updated_at
  BEFORE UPDATE ON public.device_calibration_profiles
  FOR EACH ROW
  EXECUTE FUNCTION public.update_updated_at_column();

-- Função para limpar rate limits antigos (executar via cron)
CREATE OR REPLACE FUNCTION public.cleanup_old_rate_limits()
RETURNS void
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
BEGIN
  DELETE FROM public.mqtt_rate_limits
  WHERE window_start < NOW() - INTERVAL '1 hour';
END;
$$;