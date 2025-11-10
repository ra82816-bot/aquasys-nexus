import { useEffect, useState } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { useMqttContext } from '@/contexts/MqttContext';
import { Activity, Wifi, HardDrive, Zap } from 'lucide-react';

interface DeviceHealth {
  uuid: string;
  firmware: string;
  uptime: number;
  wifi: {
    ssid: string;
    rssi: number;
    reconnects: number;
  };
  mqtt: {
    connected: boolean;
    failed_attempts: number;
  };
  memory: {
    free_heap: number;
    min_free_heap: number;
  };
  type: 'sensor' | 'actuator';
  lastSeen: number;
  status: 'online' | 'offline'; // ✅ PRIORIDADE I.2: Campo de status LWT
}

export const DeviceStatus = () => {
  const [devices, setDevices] = useState<Map<string, DeviceHealth>>(new Map());
  const { lastMessage } = useMqttContext();

  useEffect(() => {
    // ✅ PRIORIDADE I.2: Processar heartbeat e LWT status
    if (lastMessage?.topic === 'aquasys/heartbeat') {
      const data = lastMessage.payload;
      
      // Determinar tipo de dispositivo pelo UUID ou campo device
      const deviceType = data.device?.includes('sensor') ? 'sensor' : 'actuator';
      
      const deviceHealth: DeviceHealth = {
        uuid: data.device_uuid || data.deviceUUID || 'unknown',
        firmware: data.firmware || 'unknown',
        uptime: data.uptime || 0,
        wifi: {
          ssid: data.wifi?.ssid || 'N/A',
          rssi: data.wifi?.rssi || 0,
          reconnects: data.wifi?.reconnects || 0
        },
        mqtt: {
          connected: data.mqtt?.connected !== false,
          failed_attempts: data.mqtt?.failed_attempts || 0
        },
        memory: {
          free_heap: data.memory?.free_heap || 0,
          min_free_heap: data.memory?.min_free_heap || 0
        },
        type: deviceType,
        lastSeen: Date.now(),
        status: 'online' // Se recebemos heartbeat, está online
      };

      setDevices(prev => {
        const updated = new Map(prev);
        updated.set(deviceHealth.uuid, deviceHealth);
        return updated;
      });
    } else if (lastMessage?.topic?.endsWith('/status')) {
      // ✅ PRIORIDADE I.2: Processar mensagens LWT de status
      const data = lastMessage.payload;
      const uuid = data.uuid;
      const status = data.status as 'online' | 'offline';
      
      if (uuid && status) {
        setDevices(prev => {
          const updated = new Map(prev);
          const existing = updated.get(uuid);
          
          if (existing) {
            // Atualizar status do dispositivo existente
            updated.set(uuid, {
              ...existing,
              status,
              lastSeen: status === 'online' ? Date.now() : existing.lastSeen
            });
          } else if (status === 'online') {
            // Criar entrada mínima para dispositivo novo que ficou online
            updated.set(uuid, {
              uuid,
              firmware: 'unknown',
              uptime: 0,
              wifi: { ssid: 'N/A', rssi: 0, reconnects: 0 },
              mqtt: { connected: true, failed_attempts: 0 },
              memory: { free_heap: 0, min_free_heap: 0 },
              type: data.type === 'sensor' ? 'sensor' : 'actuator',
              lastSeen: Date.now(),
              status: 'online'
            });
          }
          
          return updated;
        });
      }
    }
  }, [lastMessage]);

  // Remover dispositivos offline (sem heartbeat há mais de 5 minutos)
  useEffect(() => {
    const interval = setInterval(() => {
      const now = Date.now();
      setDevices(prev => {
        const updated = new Map(prev);
        for (const [uuid, device] of updated.entries()) {
          if (now - device.lastSeen > 300000) { // 5 minutos
            updated.delete(uuid);
          }
        }
        return updated;
      });
    }, 60000); // Verificar a cada 1 minuto

    return () => clearInterval(interval);
  }, []);

  const formatUptime = (seconds: number) => {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    return `${hours}h ${minutes}m`;
  };

  const getSignalStrength = (rssi: number) => {
    if (rssi >= -50) return { label: 'Excelente', color: 'bg-green-500' };
    if (rssi >= -60) return { label: 'Bom', color: 'bg-blue-500' };
    if (rssi >= -70) return { label: 'Regular', color: 'bg-yellow-500' };
    return { label: 'Fraco', color: 'bg-red-500' };
  };

  if (devices.size === 0) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Activity className="h-5 w-5" />
            Dispositivos Conectados
          </CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-sm text-muted-foreground">
            Aguardando heartbeat dos dispositivos...
          </p>
        </CardContent>
      </Card>
    );
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Activity className="h-5 w-5" />
          Dispositivos Conectados ({devices.size})
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        {Array.from(devices.values()).map(device => {
          const signal = getSignalStrength(device.wifi.rssi);
          const isHealthy = device.memory.free_heap > 10000; // > 10KB

          return (
            <div 
              key={device.uuid} 
              className="border rounded-lg p-4 space-y-3 bg-card/50"
            >
              {/* Header */}
              <div className="flex items-start justify-between">
                <div className="space-y-1">
                  <p className="font-mono text-xs text-muted-foreground">
                    {device.uuid}
                  </p>
                  <div className="flex items-center gap-2">
                    <Badge variant={device.type === 'sensor' ? 'default' : 'secondary'}>
                      {device.type === 'sensor' ? '📊 Sensor' : '⚡ Atuador'}
                    </Badge>
                    <Badge variant="outline">{device.firmware}</Badge>
                    {/* ✅ PRIORIDADE I.2: Badge de status LWT */}
                    <Badge variant={device.status === 'online' ? 'default' : 'destructive'}>
                      {device.status === 'online' ? '🟢 Online' : '🔴 Offline'}
                    </Badge>
                  </div>
                </div>
                <Badge variant={isHealthy ? 'default' : 'destructive'}>
                  {isHealthy ? '✓ OK' : '⚠ Atenção'}
                </Badge>
              </div>

              {/* Stats Grid */}
              <div className="grid grid-cols-2 gap-3 text-sm">
                {/* WiFi */}
                <div className="flex items-center gap-2">
                  <Wifi className="h-4 w-4 text-muted-foreground" />
                  <div>
                    <p className="text-xs text-muted-foreground">WiFi</p>
                    <p className="font-medium">{device.wifi.ssid}</p>
                    <div className="flex items-center gap-1 mt-1">
                      <div className={`h-2 w-2 rounded-full ${signal.color}`} />
                      <span className="text-xs">{signal.label} ({device.wifi.rssi} dBm)</span>
                    </div>
                  </div>
                </div>

                {/* Memory */}
                <div className="flex items-center gap-2">
                  <HardDrive className="h-4 w-4 text-muted-foreground" />
                  <div>
                    <p className="text-xs text-muted-foreground">Memória</p>
                    <p className="font-medium">{(device.memory.free_heap / 1024).toFixed(1)} KB</p>
                    <p className="text-xs text-muted-foreground">
                      Min: {(device.memory.min_free_heap / 1024).toFixed(1)} KB
                    </p>
                  </div>
                </div>

                {/* Uptime */}
                <div className="flex items-center gap-2">
                  <Zap className="h-4 w-4 text-muted-foreground" />
                  <div>
                    <p className="text-xs text-muted-foreground">Uptime</p>
                    <p className="font-medium">{formatUptime(device.uptime)}</p>
                  </div>
                </div>

                {/* MQTT Status */}
                <div className="flex items-center gap-2">
                  <Activity className="h-4 w-4 text-muted-foreground" />
                  <div>
                    <p className="text-xs text-muted-foreground">MQTT</p>
                    <Badge variant={device.mqtt.connected ? 'default' : 'destructive'} className="text-xs">
                      {device.mqtt.connected ? 'Conectado' : 'Desconectado'}
                    </Badge>
                    {device.mqtt.failed_attempts > 0 && (
                      <p className="text-xs text-destructive">
                        Falhas: {device.mqtt.failed_attempts}
                      </p>
                    )}
                  </div>
                </div>
              </div>

              {/* Warnings */}
              {device.wifi.reconnects > 5 && (
                <div className="text-xs text-yellow-600 bg-yellow-50 dark:bg-yellow-950/30 p-2 rounded">
                  ⚠️ WiFi instável: {device.wifi.reconnects} reconexões
                </div>
              )}
              {!isHealthy && (
                <div className="text-xs text-red-600 bg-red-50 dark:bg-red-950/30 p-2 rounded">
                  ⚠️ Memória baixa: {(device.memory.free_heap / 1024).toFixed(1)} KB disponíveis
                </div>
              )}
            </div>
          );
        })}
      </CardContent>
    </Card>
  );
};
