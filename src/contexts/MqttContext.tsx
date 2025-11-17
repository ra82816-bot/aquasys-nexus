import React, { createContext, useContext, ReactNode } from 'react';
import { useMqtt } from '@/hooks/useMqtt';
import type { MqttMessage } from '@/hooks/useMqtt';

interface MqttContextType {
  isConnected: boolean;
  lastMessage: MqttMessage | null;
  lastSensorUpdate: number;
  sensorTimeout: boolean;
  deviceTopics: { sensors: string; relayStatus: string; relayCommand: string };
  publish: (topic: string, message: any, options?: any) => Promise<void>;
  publishRelayCommand: (relayIndex: number, command: boolean) => Promise<void>;
  publishRelayConfig: (relayIndex: number, config: any) => Promise<void>;
  setRelayAuto: (relayIndex: number) => Promise<void>;
  connect: () => void;
  disconnect: () => void;
  client?: any;
}

// Create context with undefined default to enforce provider usage
const MqttContext = createContext<MqttContextType | undefined>(undefined);

// Provider component that initializes MQTT connection
export const MqttProvider = ({ children }: { children: ReactNode }) => {
  const mqtt = useMqtt();

  const contextValue: MqttContextType = {
    isConnected: mqtt.isConnected,
    lastMessage: mqtt.lastMessage,
    lastSensorUpdate: mqtt.lastSensorUpdate,
    sensorTimeout: mqtt.sensorTimeout,
    deviceTopics: mqtt.deviceTopics,
    publish: mqtt.publish,
    publishRelayCommand: mqtt.publishRelayCommand,
    publishRelayConfig: mqtt.publishRelayConfig,
    setRelayAuto: mqtt.setRelayAuto,
    connect: mqtt.connect,
    disconnect: mqtt.disconnect,
    client: mqtt.client,
  };

  return (
    <MqttContext.Provider value={contextValue}>
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
