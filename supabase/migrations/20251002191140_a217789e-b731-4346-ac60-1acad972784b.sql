-- Add name column to relay_configs table to allow custom relay naming
ALTER TABLE public.relay_configs
ADD COLUMN IF NOT EXISTS name TEXT DEFAULT '';