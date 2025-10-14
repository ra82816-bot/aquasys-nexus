import { BleDevice } from '@capacitor-community/bluetooth-le';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { BluetoothConnected, X } from 'lucide-react';

interface BluetoothConnectionProps {
  device: BleDevice | null;
  onDisconnect: () => void;
}

export const BluetoothConnection = ({
  device,
  onDisconnect,
}: BluetoothConnectionProps) => {
  if (!device) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <BluetoothConnected className="h-5 w-5" />
            Conexão Ativa
          </CardTitle>
          <CardDescription>Nenhum dispositivo conectado</CardDescription>
        </CardHeader>
        <CardContent>
          <div className="text-center py-8 text-muted-foreground">
            <BluetoothConnected className="h-12 w-12 mx-auto mb-2 opacity-50" />
            <p>Conecte-se a um dispositivo para começar</p>
          </div>
        </CardContent>
      </Card>
    );
  }

  return (
    <Card className="border-primary bg-primary/5">
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <BluetoothConnected className="h-5 w-5 text-primary" />
          Conexão Ativa
        </CardTitle>
        <CardDescription>Dispositivo conectado</CardDescription>
      </CardHeader>
      <CardContent>
        <div className="space-y-4">
          <div>
            <h4 className="font-medium text-lg mb-2">
              {device.name || 'Dispositivo Sem Nome'}
            </h4>
            <p className="text-sm text-muted-foreground mb-3">{device.deviceId}</p>
            <Badge className="bg-green-500">Conectado</Badge>
          </div>
          
          <Button
            onClick={onDisconnect}
            variant="destructive"
            className="w-full"
          >
            <X className="h-4 w-4 mr-2" />
            Desconectar
          </Button>
        </div>
      </CardContent>
    </Card>
  );
};
