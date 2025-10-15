import { useState } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Input } from '@/components/ui/input';
import { Button } from '@/components/ui/button';
import { Label } from '@/components/ui/label';
import { Badge } from '@/components/ui/badge';
import { Wifi, Signal, AlertCircle, Save, RefreshCw } from 'lucide-react';
import { Alert, AlertDescription } from '@/components/ui/alert';
import { useMqttContext } from '@/contexts/MqttContext';
import { toast } from '@/hooks/use-toast';

interface ESP32NetworkInfo {
  ssid: string;
  signal: number;
  ip: string;
  connected: boolean;
}

export const ESP32WifiManager = () => {
  const { publish, isConnected } = useMqttContext();
  const [module1Network, setModule1Network] = useState<ESP32NetworkInfo>({
    ssid: 'Carregando...',
    signal: 0,
    ip: '0.0.0.0',
    connected: false
  });
  
  const [module2Network, setModule2Network] = useState<ESP32NetworkInfo>({
    ssid: 'Carregando...',
    signal: 0,
    ip: '0.0.0.0',
    connected: false
  });

  const [newSSID1, setNewSSID1] = useState('');
  const [newPassword1, setNewPassword1] = useState('');
  const [newSSID2, setNewSSID2] = useState('');
  const [newPassword2, setNewPassword2] = useState('');

  const getSignalStrength = (rssi: number): string => {
    if (rssi >= -50) return 'Excelente';
    if (rssi >= -60) return 'Bom';
    if (rssi >= -70) return 'Razoável';
    return 'Fraco';
  };

  const getSignalColor = (rssi: number): "default" | "destructive" | "secondary" => {
    if (rssi >= -60) return 'default';
    if (rssi >= -70) return 'secondary';
    return 'destructive';
  };

  const handleRefreshNetwork = async (moduleNumber: 1 | 2) => {
    if (!isConnected) {
      toast({
        title: "Erro",
        description: "Não conectado ao MQTT",
        variant: "destructive"
      });
      return;
    }

    try {
      await publish(`esp32/module${moduleNumber}/wifi/status`, { command: 'get_status' });
      toast({
        title: "Solicitação enviada",
        description: `Atualizando status do Módulo ${moduleNumber}...`
      });
    } catch (error) {
      toast({
        title: "Erro",
        description: "Falha ao solicitar status de rede",
        variant: "destructive"
      });
    }
  };

  const handleUpdateWifi = async (moduleNumber: 1 | 2) => {
    const ssid = moduleNumber === 1 ? newSSID1 : newSSID2;
    const password = moduleNumber === 1 ? newPassword1 : newPassword2;

    if (!ssid || !password) {
      toast({
        title: "Erro",
        description: "Preencha SSID e senha",
        variant: "destructive"
      });
      return;
    }

    if (!isConnected) {
      toast({
        title: "Erro",
        description: "Não conectado ao MQTT",
        variant: "destructive"
      });
      return;
    }

    try {
      await publish(`esp32/module${moduleNumber}/wifi/config`, {
        ssid,
        password
      });
      
      toast({
        title: "Configuração enviada",
        description: `Módulo ${moduleNumber} tentará conectar à nova rede`
      });

      if (moduleNumber === 1) {
        setNewSSID1('');
        setNewPassword1('');
      } else {
        setNewSSID2('');
        setNewPassword2('');
      }
    } catch (error) {
      toast({
        title: "Erro",
        description: "Falha ao enviar configuração",
        variant: "destructive"
      });
    }
  };

  const renderModuleCard = (
    moduleNumber: 1 | 2,
    network: ESP32NetworkInfo,
    ssid: string,
    setSSID: (value: string) => void,
    password: string,
    setPassword: (value: string) => void
  ) => (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Wifi className="h-5 w-5" />
            Módulo ESP32 #{moduleNumber}
          </div>
          <Button
            variant="outline"
            size="sm"
            onClick={() => handleRefreshNetwork(moduleNumber)}
            disabled={!isConnected}
          >
            <RefreshCw className="h-4 w-4" />
          </Button>
        </CardTitle>
        <CardDescription>
          Status da conexão Wi-Fi do módulo
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="space-y-3">
          <div className="flex items-center justify-between">
            <span className="text-sm font-medium">Status:</span>
            <Badge variant={network.connected ? "default" : "destructive"}>
              {network.connected ? "Conectado" : "Desconectado"}
            </Badge>
          </div>

          <div className="flex items-center justify-between">
            <span className="text-sm font-medium">Rede:</span>
            <span className="text-sm text-muted-foreground">{network.ssid}</span>
          </div>

          <div className="flex items-center justify-between">
            <span className="text-sm font-medium">IP:</span>
            <span className="text-sm text-muted-foreground font-mono">{network.ip}</span>
          </div>

          <div className="flex items-center justify-between">
            <span className="text-sm font-medium">Sinal:</span>
            <div className="flex items-center gap-2">
              <Signal className="h-4 w-4" />
              <Badge variant={getSignalColor(network.signal)}>
                {getSignalStrength(network.signal)} ({network.signal} dBm)
              </Badge>
            </div>
          </div>
        </div>

        <div className="pt-4 border-t space-y-3">
          <h4 className="text-sm font-medium">Reconfigurar Wi-Fi</h4>
          
          <div className="space-y-2">
            <Label htmlFor={`ssid-${moduleNumber}`}>Novo SSID</Label>
            <Input
              id={`ssid-${moduleNumber}`}
              placeholder="Nome da rede"
              value={ssid}
              onChange={(e) => setSSID(e.target.value)}
              disabled={!isConnected}
            />
          </div>

          <div className="space-y-2">
            <Label htmlFor={`password-${moduleNumber}`}>Senha</Label>
            <Input
              id={`password-${moduleNumber}`}
              type="password"
              placeholder="Senha da rede"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              disabled={!isConnected}
            />
          </div>

          <Button
            className="w-full"
            onClick={() => handleUpdateWifi(moduleNumber)}
            disabled={!isConnected}
          >
            <Save className="h-4 w-4 mr-2" />
            Atualizar Configuração
          </Button>
        </div>
      </CardContent>
    </Card>
  );

  return (
    <div className="space-y-6">
      {!isConnected && (
        <Alert>
          <AlertCircle className="h-4 w-4" />
          <AlertDescription>
            Você precisa estar conectado ao MQTT para gerenciar as redes Wi-Fi dos módulos ESP32.
            Conecte-se primeiro através do Dashboard.
          </AlertDescription>
        </Alert>
      )}

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {renderModuleCard(1, module1Network, newSSID1, setNewSSID1, newPassword1, setNewPassword1)}
        {renderModuleCard(2, module2Network, newSSID2, setNewSSID2, newPassword2, setNewPassword2)}
      </div>

      <Card className="border-dashed">
        <CardHeader>
          <CardTitle className="text-lg">Como funciona</CardTitle>
        </CardHeader>
        <CardContent className="space-y-2 text-sm text-muted-foreground">
          <p>• Esta interface gerencia as redes Wi-Fi dos <strong>módulos ESP32</strong>, não do seu smartphone</p>
          <p>• Use o botão de atualizar para obter o status atual de cada módulo</p>
          <p>• Ao alterar SSID e senha, o módulo tentará reconectar à nova rede</p>
          <p>• O módulo pode ficar temporariamente offline durante a reconexão</p>
        </CardContent>
      </Card>
    </div>
  );
};
