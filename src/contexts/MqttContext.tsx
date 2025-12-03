import React, { createContext, useContext, ReactNode } from 'react';
import { useMqtt } from '@/hooks/useMqtt';
import type { MqttMessage } from '@/hooks/useMqtt';

interface ConnectionState {
  attempts: number;
  lastAttempt: Date | null;
  nextRetryIn: number;
}

interface MqttContextType {
  isConnected: boolean;
  lastMessage: MqttMessage | null;
  connectionState: ConnectionState;
  publish: (topic: string, message: any, options?: any) => Promise<void>;
  publishRelayCommand: (relayIndex: number, command: boolean) => Promise<void>;
  publishRelayConfig: (relayIndex: number, config: any) => Promise<void>;
  setRelayAuto: (relayIndex: number) => Promise<void>;
  connect: () => void;
  disconnect: () => void;
}

const MqttContext = createContext<MqttContextType | undefined>(undefined);

export const MqttProvider = ({ children }: { children: ReactNode }) => {
  const mqtt = useMqtt();

  return (
    <MqttContext.Provider value={mqtt}>
      {children}
    </MqttContext.Provider>
  );
};

const fallbackContext: MqttContextType = {
  isConnected: false,
  lastMessage: null,
  connectionState: { attempts: 0, lastAttempt: null, nextRetryIn: 1000 },
  publish: async () => { console.warn('MQTT: Provider não disponível'); },
  publishRelayCommand: async () => { console.warn('MQTT: Provider não disponível'); },
  publishRelayConfig: async () => { console.warn('MQTT: Provider não disponível'); },
  setRelayAuto: async () => { console.warn('MQTT: Provider não disponível'); },
  connect: () => { console.warn('MQTT: Provider não disponível'); },
  disconnect: () => { console.warn('MQTT: Provider não disponível'); },
};

export const useMqttContext = () => {
  const context = useContext(MqttContext);
  if (!context) {
    console.warn('useMqttContext: Contexto não encontrado, usando fallback');
    return fallbackContext;
  }
  return context;
};
