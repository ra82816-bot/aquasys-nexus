import { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.lovable.aquasysnexus',
  appName: 'aquasys-nexus',
  webDir: 'dist',
  server: {
    url: 'https://7e63af5c-2336-45ef-8229-2ff1e2827bff.lovableproject.com?forceHideBadge=true',
    cleartext: true
  }
};

export default config;
