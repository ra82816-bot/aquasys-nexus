-- Create enum for device types
CREATE TYPE device_type AS ENUM ('sensor', 'actuator');

-- Create devices table
CREATE TABLE public.devices (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  device_uuid VARCHAR(20) UNIQUE NOT NULL,
  device_type device_type NOT NULL,
  firmware_version VARCHAR(10),
  first_seen_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  last_seen_at TIMESTAMP WITH TIME ZONE,
  mqtt_password_hash TEXT NOT NULL,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Create device_owners table (1 device = 1 user)
CREATE TABLE public.device_owners (
  device_id UUID REFERENCES public.devices(id) ON DELETE CASCADE,
  user_id UUID REFERENCES auth.users(id) ON DELETE CASCADE,
  paired_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  PRIMARY KEY (device_id),
  UNIQUE (device_id)
);

-- Create device_health table for diagnostics
CREATE TABLE public.device_health (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  device_id UUID REFERENCES public.devices(id) ON DELETE CASCADE,
  timestamp TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  uptime_seconds BIGINT,
  wifi_ssid TEXT,
  wifi_rssi INTEGER,
  wifi_ip TEXT,
  wifi_reconnects INTEGER DEFAULT 0,
  mqtt_connected BOOLEAN DEFAULT false,
  mqtt_failed_attempts INTEGER DEFAULT 0,
  mqtt_last_message_age_ms INTEGER,
  free_heap INTEGER,
  min_free_heap INTEGER,
  sensor_ph_valid BOOLEAN,
  sensor_ec_valid BOOLEAN,
  sensor_temp_valid BOOLEAN,
  sensor_humidity_valid BOOLEAN,
  sensor_water_temp_valid BOOLEAN
);

-- Enable RLS
ALTER TABLE public.devices ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.device_owners ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.device_health ENABLE ROW LEVEL SECURITY;

-- Create security definer function to check device ownership
CREATE OR REPLACE FUNCTION public.user_owns_device(_user_id UUID, _device_id UUID)
RETURNS BOOLEAN
LANGUAGE SQL
STABLE
SECURITY DEFINER
SET search_path = public
AS $$
  SELECT EXISTS (
    SELECT 1
    FROM public.device_owners
    WHERE user_id = _user_id
      AND device_id = _device_id
  )
$$;

-- RLS Policies for devices
CREATE POLICY "Users can view their own devices"
  ON public.devices FOR SELECT
  USING (id IN (
    SELECT device_id FROM public.device_owners WHERE user_id = auth.uid()
  ));

CREATE POLICY "Service role can insert devices"
  ON public.devices FOR INSERT
  WITH CHECK (true);

CREATE POLICY "Service role can update devices"
  ON public.devices FOR UPDATE
  USING (true);

-- RLS Policies for device_owners
CREATE POLICY "Users can view their own device ownership"
  ON public.device_owners FOR SELECT
  USING (user_id = auth.uid());

CREATE POLICY "Users can insert their own device ownership"
  ON public.device_owners FOR INSERT
  WITH CHECK (user_id = auth.uid());

CREATE POLICY "Users can delete their own device ownership"
  ON public.device_owners FOR DELETE
  USING (user_id = auth.uid());

-- RLS Policies for device_health
CREATE POLICY "Users can view health of their own devices"
  ON public.device_health FOR SELECT
  USING (device_id IN (
    SELECT device_id FROM public.device_owners WHERE user_id = auth.uid()
  ));

CREATE POLICY "Service role can insert device health"
  ON public.device_health FOR INSERT
  WITH CHECK (true);

-- Create trigger for updated_at
CREATE TRIGGER update_devices_updated_at
  BEFORE UPDATE ON public.devices
  FOR EACH ROW
  EXECUTE FUNCTION public.update_updated_at_column();

-- Create index for performance
CREATE INDEX idx_device_owners_user_id ON public.device_owners(user_id);
CREATE INDEX idx_device_health_device_id ON public.device_health(device_id);
CREATE INDEX idx_device_health_timestamp ON public.device_health(timestamp DESC);