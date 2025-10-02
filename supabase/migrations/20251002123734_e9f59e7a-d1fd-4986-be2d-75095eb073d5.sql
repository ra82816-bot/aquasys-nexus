-- Create enum for relay modes
CREATE TYPE public.relay_mode AS ENUM (
  'unused',
  'led',
  'cycle',
  'ph_up',
  'ph_down',
  'temperature',
  'humidity',
  'ec'
);

-- Create readings table for sensor data
CREATE TABLE public.readings (
  id SERIAL PRIMARY KEY,
  timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  ph DOUBLE PRECISION NOT NULL,
  ec DOUBLE PRECISION NOT NULL,
  air_temp DOUBLE PRECISION NOT NULL,
  humidity DOUBLE PRECISION NOT NULL,
  water_temp DOUBLE PRECISION NOT NULL
);

-- Create relay_configs table for 8 relays configuration
CREATE TABLE public.relay_configs (
  relay_index INTEGER PRIMARY KEY CHECK (relay_index >= 0 AND relay_index <= 7),
  mode relay_mode NOT NULL DEFAULT 'unused',
  -- LED mode parameters
  led_on_hour INTEGER,
  led_off_hour INTEGER,
  -- Cycle mode parameters
  cycle_on_min INTEGER,
  cycle_off_min INTEGER,
  -- pH Up mode parameters
  ph_threshold_low DOUBLE PRECISION,
  ph_pulse_sec INTEGER,
  -- pH Down mode parameters
  ph_threshold_high DOUBLE PRECISION,
  -- Temperature mode parameters
  temp_threshold_on DOUBLE PRECISION,
  temp_threshold_off DOUBLE PRECISION,
  -- Humidity mode parameters
  humidity_threshold_on DOUBLE PRECISION,
  humidity_threshold_off DOUBLE PRECISION,
  -- EC mode parameters
  ec_threshold DOUBLE PRECISION,
  ec_pulse_sec INTEGER,
  updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Create event_logs table for future events and alerts
CREATE TABLE public.event_logs (
  id SERIAL PRIMARY KEY,
  timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  type TEXT NOT NULL,
  message TEXT NOT NULL
);

-- Create profiles table for user authentication
CREATE TABLE public.profiles (
  id UUID PRIMARY KEY REFERENCES auth.users(id) ON DELETE CASCADE,
  email TEXT NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Insert default configuration for 8 relays
INSERT INTO public.relay_configs (relay_index, mode) VALUES
  (0, 'unused'),
  (1, 'unused'),
  (2, 'unused'),
  (3, 'unused'),
  (4, 'unused'),
  (5, 'unused'),
  (6, 'unused'),
  (7, 'unused');

-- Enable Row Level Security
ALTER TABLE public.readings ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.relay_configs ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.event_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.profiles ENABLE ROW LEVEL SECURITY;

-- Create policies for readings (all authenticated users can read, only system can insert)
CREATE POLICY "Anyone can view readings"
  ON public.readings
  FOR SELECT
  USING (true);

CREATE POLICY "Service role can insert readings"
  ON public.readings
  FOR INSERT
  WITH CHECK (true);

-- Create policies for relay_configs (all authenticated users can read and update)
CREATE POLICY "Anyone can view relay configs"
  ON public.relay_configs
  FOR SELECT
  USING (true);

CREATE POLICY "Authenticated users can update relay configs"
  ON public.relay_configs
  FOR UPDATE
  USING (true);

-- Create policies for event_logs (all can read, system can insert)
CREATE POLICY "Anyone can view event logs"
  ON public.event_logs
  FOR SELECT
  USING (true);

CREATE POLICY "Service role can insert event logs"
  ON public.event_logs
  FOR INSERT
  WITH CHECK (true);

-- Create policies for profiles
CREATE POLICY "Users can view their own profile"
  ON public.profiles
  FOR SELECT
  USING (auth.uid() = id);

CREATE POLICY "Users can update their own profile"
  ON public.profiles
  FOR UPDATE
  USING (auth.uid() = id);

CREATE POLICY "Users can insert their own profile"
  ON public.profiles
  FOR INSERT
  WITH CHECK (auth.uid() = id);

-- Create function to handle new user creation
CREATE OR REPLACE FUNCTION public.handle_new_user()
RETURNS TRIGGER
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
BEGIN
  INSERT INTO public.profiles (id, email)
  VALUES (NEW.id, NEW.email);
  RETURN NEW;
END;
$$;

-- Create trigger for new user
CREATE TRIGGER on_auth_user_created
  AFTER INSERT ON auth.users
  FOR EACH ROW
  EXECUTE FUNCTION public.handle_new_user();

-- Enable realtime for readings
ALTER PUBLICATION supabase_realtime ADD TABLE public.readings;