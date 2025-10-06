import React, { createContext, useContext, ReactNode } from 'react';
import { useMqtt } from '@/hooks/useMqtt';
import type { MqttMessage } from '@/hooks/useMqtt';

interface MqttContextType {
  isConnected: boolean;
  lastMessage: MqttMessage | null;
  publish: (topic: string, message: any, options?: any) => Promise<void>;
  publishRelayCommand: (relayIndex: number, command: boolean) => Promise<void>;
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

export const useMqttContext = () => {
  const context = useContext(MqttContext);
  if (!context) {
    throw new Error('useMqttContext deve ser usado dentro de MqttProvider');
  }
  return context;
};
