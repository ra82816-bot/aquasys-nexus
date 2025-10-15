import { useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { supabase } from '@/integrations/supabase/client';
import { useBluetoothLE } from '@/hooks/useBluetoothLE';
import { BluetoothDeviceList } from '@/components/bluetooth/BluetoothDeviceList';
import { BluetoothConnection } from '@/components/bluetooth/BluetoothConnection';
import { BluetoothDataExchange } from '@/components/bluetooth/BluetoothDataExchange';
import { AppHeader } from '@/components/dashboard/AppHeader';
import { Button } from '@/components/ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Bluetooth as BluetoothIcon, Play, StopCircle, Power, RefreshCw } from 'lucide-react';
import { Alert, AlertDescription } from '@/components/ui/alert';
import { toast } from '@/hooks/use-toast';

const Bluetooth = () => {
  const navigate = useNavigate();
  const bluetooth = useBluetoothLE();

  useEffect(() => {
    const checkUser = async () => {
      const { data: { session } } = await supabase.auth.getSession();
      if (!session) {
        navigate('/auth');
      }
    };
    checkUser();
  }, [navigate]);

  const handleLogout = async () => {
    await supabase.auth.signOut();
    navigate('/auth');
  };

  const handleEnableBluetooth = async () => {
    if (!bluetooth.isEnabled) {
      const enabled = await bluetooth.requestEnable();
      if (!enabled) {
        toast({
          title: "Permissão Negada",
          description: "Por favor, habilite as permissões Bluetooth nas configurações do dispositivo.",
          variant: "destructive"
        });
      }
    }
  };

  return (
    <div className="min-h-screen bg-gradient-to-b from-background to-muted/20">
      <AppHeader onLogout={handleLogout} onNavigate={navigate} />
      
      <main className="container mx-auto px-4 py-8 space-y-6">
        <div className="flex items-center justify-between">
          <div>
            <h1 className="text-3xl font-bold flex items-center gap-2">
              <BluetoothIcon className="h-8 w-8" />
              Bluetooth Manager
            </h1>
            <p className="text-muted-foreground mt-1">
              Gerencie conexões e troque dados com dispositivos Bluetooth
            </p>
          </div>
        </div>

        {!bluetooth.isEnabled && (
          <Alert>
            <Power className="h-4 w-4" />
            <AlertDescription className="flex items-center justify-between">
              <span>O Bluetooth está desativado. Ative para continuar.</span>
              <Button onClick={handleEnableBluetooth} size="sm">
                <Power className="h-4 w-4 mr-2" />
                Ativar Bluetooth
              </Button>
            </AlertDescription>
          </Alert>
        )}

        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <BluetoothIcon className="h-5 w-5" />
              Controles de Escaneamento
            </CardTitle>
            <CardDescription>
              Procure dispositivos Bluetooth próximos
            </CardDescription>
          </CardHeader>
          <CardContent className="flex gap-2">
            <Button
              onClick={bluetooth.startScan}
              disabled={!bluetooth.isEnabled || bluetooth.isScanning}
              className="flex-1"
            >
              <Play className="h-4 w-4 mr-2" />
              {bluetooth.isScanning ? 'Escaneando...' : 'Iniciar Escaneamento'}
            </Button>
            <Button
              onClick={bluetooth.stopScan}
              disabled={!bluetooth.isScanning}
              variant="outline"
              className="flex-1"
            >
              <StopCircle className="h-4 w-4 mr-2" />
              Parar Escaneamento
            </Button>
            <Button
              onClick={() => {
                bluetooth.stopScan();
                setTimeout(() => bluetooth.startScan(), 500);
              }}
              disabled={!bluetooth.isEnabled}
              variant="secondary"
            >
              <RefreshCw className="h-4 w-4" />
            </Button>
          </CardContent>
        </Card>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          <div className="space-y-6">
            <BluetoothDeviceList
              devices={bluetooth.devices}
              isScanning={bluetooth.isScanning}
              onConnect={bluetooth.connect}
              connectedDeviceId={bluetooth.connectedDevice?.deviceId}
            />
          </div>

          <div className="space-y-6">
            <BluetoothConnection
              device={bluetooth.connectedDevice}
              onDisconnect={bluetooth.disconnect}
            />
            
            <BluetoothDataExchange
              isConnected={!!bluetooth.connectedDevice}
              receivedData={bluetooth.receivedData}
              onSendData={bluetooth.writeData}
              onReadData={bluetooth.readData}
            />
          </div>
        </div>
      </main>
    </div>
  );
};

export default Bluetooth;
