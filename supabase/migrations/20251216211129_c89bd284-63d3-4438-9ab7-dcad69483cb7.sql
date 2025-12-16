-- Aumentar tamanho da coluna device_uuid para suportar UUIDs completos (36 caracteres)
ALTER TABLE public.devices 
ALTER COLUMN device_uuid TYPE character varying(64);