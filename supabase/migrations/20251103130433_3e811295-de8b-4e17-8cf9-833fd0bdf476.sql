-- Increase firmware_version column limit to support longer version strings
ALTER TABLE public.devices 
ALTER COLUMN firmware_version TYPE VARCHAR(50);

-- Add comment for documentation
COMMENT ON COLUMN public.devices.firmware_version IS 'Device firmware version string (max 50 characters, e.g., 4.0.6-FINAL)';