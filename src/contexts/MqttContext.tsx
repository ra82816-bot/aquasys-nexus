import React, { createContext, useContext, ReactNode } from 'react';
import { useMqtt } from '@/hooks/useMqtt';
import type { MqttMessage } from '@/hooks/useMqtt';

interface MqttContextType {
  isConnected: boolean;
  lastMessage: MqttMessage | null;
  publish: (topic: string, message: any, options?: any) => Promise<void>;
  publishRelayCommand: (relayIndex: number, command: boolean) => Promise<void>;
  publishRelayConfig: (relayIndex: number, config: any) => Promise<void>;
  setRelayAuto: (relayIndex: number) => Promise<void>;
  connect: () => void;
  disconnect: () => void;
}

// Create context with undefined default to enforce provider usage
const MqttContext = createContext<MqttContextType | undefined>(undefined);

// Provider component that initializes MQTT connection
export const MqttProvider = ({ children }: { children: ReactNode }) => {
  const mqtt = useMqtt();

  return (
    <MqttContext.Provider value={mqtt}>
      {children}
    </MqttContext.Provider>
  );
};

// Fallback context for when provider is not available (e.g., during HMR)
const fallbackContext: MqttContextType = {
  isConnected: false,
  lastMessage: null,
  publish: async () => { console.warn('MQTT: Provider não disponível'); },
  publishRelayCommand: async () => { console.warn('MQTT: Provider não disponível'); },
  publishRelayConfig: async () => { console.warn('MQTT: Provider não disponível'); },
  setRelayAuto: async () => { console.warn('MQTT: Provider não disponível'); },
  connect: () => { console.warn('MQTT: Provider não disponível'); },
  disconnect: () => { console.warn('MQTT: Provider não disponível'); },
};

// Custom hook to use MQTT context with graceful fallback
export const useMqttContext = () => {
  const context = useContext(MqttContext);
  if (!context) {
    console.warn('useMqttContext: Contexto não encontrado, usando fallback');
    return fallbackContext;
  }
  return context;
};
