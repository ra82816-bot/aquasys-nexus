import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Wifi, WifiOff, Signal, AlertCircle } from "lucide-react";
import { Alert, AlertDescription } from "@/components/ui/alert";
import { Network } from "@capacitor/network";

const WiFiPage = () => {
  const navigate = useNavigate();
  const [isOnline, setIsOnline] = useState(true);
  const [connectionType, setConnectionType] = useState<string>("unknown");

  useEffect(() => {
    const checkAuth = async () => {
      const { data: { session } } = await supabase.auth.getSession();
      if (!session) navigate("/auth");
    };
    checkAuth();

    const getNetworkStatus = async () => {
      const status = await Network.getStatus();
      setIsOnline(status.connected);
      setConnectionType(status.connectionType);
    };

    getNetworkStatus();

    const networkListener = Network.addListener('networkStatusChange', (status) => {
      setIsOnline(status.connected);
      setConnectionType(status.connectionType);
    });

    return () => {
      networkListener.then(listener => listener.remove());
    };
  }, [navigate]);

  const handleLogout = async () => {
    await supabase.auth.signOut();
    navigate("/auth");
  };

  return (
    <div className="min-h-screen bg-background">
      <AppHeader 
        onLogout={handleLogout}
        onNavigate={navigate}
      />
      
      <main className="container mx-auto px-4 py-8 max-w-4xl">
        <div className="space-y-6">
          <div className="flex items-center gap-3">
            <Wifi className="h-8 w-8 text-primary" />
            <div>
              <h1 className="text-3xl font-bold">Configurações WiFi</h1>
              <p className="text-muted-foreground">Status da conexão de rede</p>
            </div>
          </div>

          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                {isOnline ? (
                  <>
                    <Signal className="h-5 w-5 text-green-500" />
                    Conectado
                  </>
                ) : (
                  <>
                    <WifiOff className="h-5 w-5 text-destructive" />
                    Desconectado
                  </>
                )}
              </CardTitle>
              <CardDescription>
                Status atual da conexão de rede
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex items-center justify-between">
                <span className="text-sm font-medium">Status:</span>
                <Badge variant={isOnline ? "default" : "destructive"}>
                  {isOnline ? "Online" : "Offline"}
                </Badge>
              </div>
              
              <div className="flex items-center justify-between">
                <span className="text-sm font-medium">Tipo de Conexão:</span>
                <Badge variant="outline">
                  {connectionType === 'wifi' ? 'WiFi' : 
                   connectionType === 'cellular' ? 'Dados Móveis' : 
                   connectionType === 'none' ? 'Sem Conexão' : 
                   'Desconhecido'}
                </Badge>
              </div>
            </CardContent>
          </Card>

          <Alert>
            <AlertCircle className="h-4 w-4" />
            <AlertDescription>
              <strong>Como alterar a rede WiFi:</strong>
              <ul className="mt-2 space-y-1 text-sm">
                <li>• <strong>Android:</strong> Configurações → Rede e Internet → WiFi</li>
                <li>• <strong>iOS:</strong> Ajustes → WiFi</li>
              </ul>
              <p className="mt-2 text-sm">
                Para conectar a uma rede diferente, acesse as configurações do sistema do seu dispositivo.
              </p>
            </AlertDescription>
          </Alert>

          <Card className="border-dashed">
            <CardHeader>
              <CardTitle className="text-lg">Informações Técnicas</CardTitle>
            </CardHeader>
            <CardContent className="space-y-2 text-sm text-muted-foreground">
              <p>• O aplicativo monitora automaticamente o status da conexão</p>
              <p>• Algumas funcionalidades podem ser limitadas quando offline</p>
              <p>• Os dados são sincronizados automaticamente quando a conexão é restaurada</p>
            </CardContent>
          </Card>
        </div>
      </main>
    </div>
  );
};

export default WiFiPage;
