import { useState, useEffect, useCallback } from 'react';
import { BleClient, BleDevice, ScanResult } from '@capacitor-community/bluetooth-le';
import { Capacitor } from '@capacitor/core';
import { toast } from '@/hooks/use-toast';

interface BluetoothState {
  isEnabled: boolean;
  isScanning: boolean;
  devices: ScanResult[];
  connectedDevice: BleDevice | null;
  receivedData: string[];
}

export const useBluetoothLE = () => {
  const [state, setState] = useState<BluetoothState>({
    isEnabled: false,
    isScanning: false,
    devices: [],
    connectedDevice: null,
    receivedData: [],
  });

  // Request permissions
  const requestPermissions = useCallback(async (): Promise<boolean> => {
    try {
      if (Capacitor.getPlatform() === 'web') {
        return true; // Web doesn't need permissions
      }
      
      // Request BLE permissions (includes location on Android)
      await BleClient.initialize();
      return true;
    } catch (error) {
      console.error('Permission request error:', error);
      toast({
        title: 'Permissão Negada',
        description: 'Habilite as permissões Bluetooth nas configurações do dispositivo',
        variant: 'destructive',
      });
      return false;
    }
  }, []);

  // Initialize Bluetooth
  const initialize = useCallback(async () => {
    try {
      await BleClient.initialize();
      
      // Request permissions explicitly
      const hasPermissions = await requestPermissions();
      if (!hasPermissions) {
        return false;
      }
      
      const enabled = await BleClient.isEnabled();
      setState(prev => ({ ...prev, isEnabled: enabled }));
      
      if (!enabled) {
        toast({
          title: 'Bluetooth Desativado',
          description: 'Ative o Bluetooth para continuar',
        });
      }
      
      return enabled;
    } catch (error) {
      console.error('Bluetooth initialization error:', error);
      toast({
        title: 'Erro ao inicializar Bluetooth',
        description: 'Verifique as permissões nas configurações do dispositivo',
        variant: 'destructive',
      });
      return false;
    }
  }, [requestPermissions]);

  // Request Bluetooth enable
  const requestEnable = useCallback(async (): Promise<boolean> => {
    try {
      // Request permissions first
      const hasPermissions = await requestPermissions();
      if (!hasPermissions) {
        return false;
      }
      
      await BleClient.requestEnable();
      const enabled = await BleClient.isEnabled();
      setState(prev => ({ ...prev, isEnabled: enabled }));
      
      if (enabled) {
        toast({
          title: 'Bluetooth Ativado',
          description: 'O Bluetooth foi ativado com sucesso',
        });
      }
      
      return enabled;
    } catch (error) {
      console.error('Error enabling Bluetooth:', error);
      toast({
        title: 'Erro',
        description: 'Não foi possível ativar o Bluetooth. Verifique as permissões.',
        variant: 'destructive',
      });
      return false;
    }
  }, [requestPermissions]);

  // Start scanning
  const startScan = useCallback(async () => {
    try {
      setState(prev => ({ ...prev, isScanning: true, devices: [] }));
      
      await BleClient.requestLEScan({}, (result) => {
        setState(prev => {
          const exists = prev.devices.find(d => d.device.deviceId === result.device.deviceId);
          if (exists) return prev;
          return { ...prev, devices: [...prev.devices, result] };
        });
      });

      // Auto-stop scan after 10 seconds
      setTimeout(() => stopScan(), 10000);
      
      toast({
        title: 'Escaneando',
        description: 'Procurando dispositivos Bluetooth próximos...',
      });
    } catch (error) {
      console.error('Scan error:', error);
      setState(prev => ({ ...prev, isScanning: false }));
      toast({
        title: 'Erro ao escanear',
        description: 'Verifique as permissões do app',
        variant: 'destructive',
      });
    }
  }, []);

  // Stop scanning
  const stopScan = useCallback(async () => {
    try {
      await BleClient.stopLEScan();
      setState(prev => ({ ...prev, isScanning: false }));
    } catch (error) {
      console.error('Stop scan error:', error);
    }
  }, []);

  // Connect to device
  const connect = useCallback(async (deviceId: string) => {
    try {
      await BleClient.connect(deviceId, (disconnectedDeviceId) => {
        if (disconnectedDeviceId === deviceId) {
          setState(prev => ({ ...prev, connectedDevice: null }));
          toast({
            title: 'Dispositivo Desconectado',
            description: 'A conexão foi perdida',
            variant: 'destructive',
          });
        }
      });

      const device = state.devices.find(d => d.device.deviceId === deviceId);
      if (device) {
        setState(prev => ({ ...prev, connectedDevice: device.device }));
        toast({
          title: 'Conectado',
          description: `Conectado a ${device.device.name || 'dispositivo'}`,
        });
      }
    } catch (error) {
      console.error('Connection error:', error);
      toast({
        title: 'Erro de Conexão',
        description: 'Não foi possível conectar ao dispositivo',
        variant: 'destructive',
      });
    }
  }, [state.devices]);

  // Disconnect
  const disconnect = useCallback(async () => {
    if (!state.connectedDevice) return;
    
    try {
      await BleClient.disconnect(state.connectedDevice.deviceId);
      setState(prev => ({ ...prev, connectedDevice: null, receivedData: [] }));
      toast({
        title: 'Desconectado',
        description: 'Dispositivo desconectado com sucesso',
      });
    } catch (error) {
      console.error('Disconnect error:', error);
    }
  }, [state.connectedDevice]);

  // Write data
  const writeData = useCallback(async (
    serviceUUID: string,
    characteristicUUID: string,
    data: string
  ) => {
    if (!state.connectedDevice) {
      toast({
        title: 'Erro',
        description: 'Nenhum dispositivo conectado',
        variant: 'destructive',
      });
      return;
    }

    try {
      const dataView = new DataView(new ArrayBuffer(data.length));
      for (let i = 0; i < data.length; i++) {
        dataView.setUint8(i, data.charCodeAt(i));
      }

      await BleClient.write(
        state.connectedDevice.deviceId,
        serviceUUID,
        characteristicUUID,
        dataView
      );

      toast({
        title: 'Dados Enviados',
        description: `Enviado: ${data}`,
      });
    } catch (error) {
      console.error('Write error:', error);
      toast({
        title: 'Erro ao Enviar',
        description: 'Não foi possível enviar os dados',
        variant: 'destructive',
      });
    }
  }, [state.connectedDevice]);

  // Read data
  const readData = useCallback(async (
    serviceUUID: string,
    characteristicUUID: string
  ) => {
    if (!state.connectedDevice) return;

    try {
      const result = await BleClient.read(
        state.connectedDevice.deviceId,
        serviceUUID,
        characteristicUUID
      );

      const decoder = new TextDecoder();
      const text = decoder.decode(result);
      
      setState(prev => ({
        ...prev,
        receivedData: [...prev.receivedData, text],
      }));

      return text;
    } catch (error) {
      console.error('Read error:', error);
      toast({
        title: 'Erro ao Ler',
        description: 'Não foi possível ler os dados',
        variant: 'destructive',
      });
    }
  }, [state.connectedDevice]);

  // Start notifications
  const startNotifications = useCallback(async (
    serviceUUID: string,
    characteristicUUID: string
  ) => {
    if (!state.connectedDevice) return;

    try {
      await BleClient.startNotifications(
        state.connectedDevice.deviceId,
        serviceUUID,
        characteristicUUID,
        (value) => {
          const decoder = new TextDecoder();
          const text = decoder.decode(value);
          setState(prev => ({
            ...prev,
            receivedData: [...prev.receivedData, text],
          }));
        }
      );
    } catch (error) {
      console.error('Notification error:', error);
    }
  }, [state.connectedDevice]);

  // Initialize on mount
  useEffect(() => {
    initialize();
  }, [initialize]);

  return {
    ...state,
    initialize,
    requestEnable,
    startScan,
    stopScan,
    connect,
    disconnect,
    writeData,
    readData,
    startNotifications,
  };
};
