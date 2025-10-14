import { ScanResult } from '@capacitor-community/bluetooth-le';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { Bluetooth, Wifi } from 'lucide-react';
import { ScrollArea } from '@/components/ui/scroll-area';

interface BluetoothDeviceListProps {
  devices: ScanResult[];
  isScanning: boolean;
  onConnect: (deviceId: string) => void;
  connectedDeviceId?: string;
}

export const BluetoothDeviceList = ({
  devices,
  isScanning,
  onConnect,
  connectedDeviceId,
}: BluetoothDeviceListProps) => {
  const getSignalStrength = (rssi?: number) => {
    if (!rssi) return 'Desconhecido';
    if (rssi > -60) return 'Excelente';
    if (rssi > -70) return 'Bom';
    if (rssi > -80) return 'Regular';
    return 'Fraco';
  };

  const getSignalColor = (rssi?: number) => {
    if (!rssi) return 'default';
    if (rssi > -60) return 'default';
    if (rssi > -70) return 'secondary';
    if (rssi > -80) return 'outline';
    return 'destructive';
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Bluetooth className="h-5 w-5" />
          Dispositivos Encontrados
        </CardTitle>
        <CardDescription>
          {isScanning ? 'Escaneando...' : `${devices.length} dispositivo(s) encontrado(s)`}
        </CardDescription>
      </CardHeader>
      <CardContent>
        <ScrollArea className="h-[400px] pr-4">
          {devices.length === 0 ? (
            <div className="text-center py-8 text-muted-foreground">
              <Bluetooth className="h-12 w-12 mx-auto mb-2 opacity-50" />
              <p>Nenhum dispositivo encontrado</p>
              <p className="text-sm mt-1">Inicie o escaneamento para procurar dispositivos</p>
            </div>
          ) : (
            <div className="space-y-3">
              {devices.map((result) => (
                <Card
                  key={result.device.deviceId}
                  className={
                    connectedDeviceId === result.device.deviceId
                      ? 'border-primary bg-primary/5'
                      : ''
                  }
                >
                  <CardContent className="p-4">
                    <div className="flex items-center justify-between">
                      <div className="flex-1">
                        <div className="flex items-center gap-2 mb-1">
                          <Wifi className="h-4 w-4 text-muted-foreground" />
                          <h4 className="font-medium">
                            {result.device.name || 'Dispositivo Sem Nome'}
                          </h4>
                        </div>
                        <p className="text-sm text-muted-foreground mb-2">
                          {result.device.deviceId}
                        </p>
                        <div className="flex gap-2">
                          <Badge variant={getSignalColor(result.rssi)}>
                            {getSignalStrength(result.rssi)} ({result.rssi || 0} dBm)
                          </Badge>
                          {result.localName && (
                            <Badge variant="outline">{result.localName}</Badge>
                          )}
                        </div>
                      </div>
                      <Button
                        onClick={() => onConnect(result.device.deviceId)}
                        disabled={connectedDeviceId === result.device.deviceId}
                        size="sm"
                      >
                        {connectedDeviceId === result.device.deviceId ? 'Conectado' : 'Conectar'}
                      </Button>
                    </div>
                  </CardContent>
                </Card>
              ))}
            </div>
          )}
        </ScrollArea>
      </CardContent>
    </Card>
  );
};
