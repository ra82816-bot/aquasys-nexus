import { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { supabase } from '@/integrations/supabase/client';
import { AppHeader } from '@/components/dashboard/AppHeader';
import { ESP32WifiManager } from '@/components/wifi/ESP32WifiManager';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Wifi, Bluetooth, Signal } from 'lucide-react';
import { Network } from '@capacitor/network';
import { useBluetoothLE } from '@/hooks/useBluetoothLE';
import { BluetoothDeviceList } from '@/components/bluetooth/BluetoothDeviceList';
import { BluetoothConnection } from '@/components/bluetooth/BluetoothConnection';
import { BluetoothDataExchange } from '@/components/bluetooth/BluetoothDataExchange';
import { Button } from '@/components/ui/button';
import { Alert, AlertDescription } from '@/components/ui/alert';
import { Play, StopCircle, Power, RefreshCw } from 'lucide-react';
import { toast } from '@/hooks/use-toast';

const Connections = () => {
  const navigate = useNavigate();
  const [isOnline, setIsOnline] = useState(true);
  const [connectionType, setConnectionType] = useState('unknown');
  const bluetooth = useBluetoothLE();

  useEffect(() => {
    checkAuth();
    getNetworkStatus();

    const networkListener = Network.addListener('networkStatusChange', (status) => {
      setIsOnline(status.connected);
      setConnectionType(status.connectionType);
    });

    return () => {
      networkListener.then(listener => listener.remove());
    };
  }, []);

  const checkAuth = async () => {
    const { data: { session } } = await supabase.auth.getSession();
    if (!session) {
      navigate('/auth');
    }
  };

  const getNetworkStatus = async () => {
    const status = await Network.getStatus();
    setIsOnline(status.connected);
    setConnectionType(status.connectionType);
  };

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
      
      <main className="container mx-auto px-4 py-8">
        <div className="mb-6">
          <h1 className="text-3xl font-bold flex items-center gap-2">
            <Signal className="h-8 w-8" />
            Conexões
          </h1>
          <p className="text-muted-foreground mt-1">
            Gerencie conexões Wi-Fi e Bluetooth dos módulos ESP32 e do seu dispositivo
          </p>
        </div>

        <Tabs defaultValue="wifi" className="w-full">
          <TabsList className="grid w-full grid-cols-3">
            <TabsTrigger value="wifi">
              <Wifi className="h-4 w-4 mr-2" />
              Módulos ESP32
            </TabsTrigger>
            <TabsTrigger value="smartphone">
              <Signal className="h-4 w-4 mr-2" />
              Smartphone
            </TabsTrigger>
            <TabsTrigger value="bluetooth">
              <Bluetooth className="h-4 w-4 mr-2" />
              Bluetooth
            </TabsTrigger>
          </TabsList>

          <TabsContent value="wifi" className="space-y-6 mt-6">
            <ESP32WifiManager />
          </TabsContent>

          <TabsContent value="smartphone" className="space-y-6 mt-6">
            <Card>
              <CardHeader>
                <CardTitle className="flex items-center gap-2">
                  <Signal className="h-5 w-5" />
                  Status da Rede do Smartphone
                </CardTitle>
                <CardDescription>
                  Informações sobre a conexão atual do seu dispositivo
                </CardDescription>
              </CardHeader>
              <CardContent className="space-y-4">
                <div className="flex items-center justify-between">
                  <span className="font-medium">Status:</span>
                  <Badge variant={isOnline ? "default" : "destructive"}>
                    {isOnline ? "Conectado" : "Desconectado"}
                  </Badge>
                </div>
                <div className="flex items-center justify-between">
                  <span className="font-medium">Tipo de Conexão:</span>
                  <Badge variant="secondary">
                    {connectionType === 'wifi' ? 'Wi-Fi' : 
                     connectionType === 'cellular' ? 'Celular' : 
                     connectionType === 'none' ? 'Sem conexão' : 'Desconhecido'}
                  </Badge>
                </div>
              </CardContent>
            </Card>

            <Card className="border-dashed">
              <CardHeader>
                <CardTitle className="text-lg">ℹ️ Como alterar a rede Wi-Fi</CardTitle>
              </CardHeader>
              <CardContent className="text-sm text-muted-foreground space-y-2">
                <p>Para alterar a rede Wi-Fi do seu <strong>smartphone</strong>:</p>
                <ol className="list-decimal list-inside space-y-1 ml-2">
                  <li>Abra as <strong>Configurações</strong> do seu dispositivo</li>
                  <li>Toque em <strong>Wi-Fi</strong> ou <strong>Redes e Internet</strong></li>
                  <li>Selecione a rede desejada</li>
                  <li>Insira a senha e conecte-se</li>
                </ol>
                <p className="mt-4">
                  <strong>Nota:</strong> Esta configuração é para o seu smartphone. 
                  Para alterar a rede dos módulos ESP32, use a aba "Módulos ESP32".
                </p>
              </CardContent>
            </Card>
          </TabsContent>

          <TabsContent value="bluetooth" className="space-y-6 mt-6">
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
                  <Bluetooth className="h-5 w-5" />
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
          </TabsContent>
        </Tabs>
      </main>
    </div>
  );
};

export default Connections;
