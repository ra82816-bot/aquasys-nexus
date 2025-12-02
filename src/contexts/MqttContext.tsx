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

// Custom hook to use MQTT context with error checking
export const useMqttContext = () => {
  const context = useContext(MqttContext);
  if (!context) {
    throw new Error('useMqttContext deve ser usado dentro de MqttProvider');
  }
  return context;
};
