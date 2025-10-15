import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Wifi, WifiOff, Signal } from "lucide-react";
import { Network } from "@capacitor/network";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { ESP32WifiManager } from "@/components/wifi/ESP32WifiManager";

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
    <div className="min-h-screen bg-gradient-to-b from-background to-muted/20">
      <AppHeader 
        onLogout={handleLogout}
        onNavigate={navigate}
      />
      
      <main className="container mx-auto px-4 py-8">
        <div className="space-y-6">
          <div className="flex items-center gap-3">
            <Wifi className="h-8 w-8 text-primary" />
            <div>
              <h1 className="text-3xl font-bold">Gerenciamento Wi-Fi</h1>
              <p className="text-muted-foreground">Gerencie redes Wi-Fi dos módulos ESP32 e do smartphone</p>
            </div>
          </div>

          <Tabs defaultValue="esp32" className="w-full">
            <TabsList className="grid w-full grid-cols-2">
              <TabsTrigger value="esp32">Módulos ESP32</TabsTrigger>
              <TabsTrigger value="smartphone">Smartphone</TabsTrigger>
            </TabsList>

            <TabsContent value="esp32" className="space-y-6 mt-6">
              <ESP32WifiManager />
            </TabsContent>

            <TabsContent value="smartphone" className="space-y-6 mt-6">
              <Card>
                <CardHeader>
                  <CardTitle className="flex items-center gap-2">
                    {isOnline ? (
                      <>
                        <Signal className="h-5 w-5 text-green-500" />
                        Smartphone Conectado
                      </>
                    ) : (
                      <>
                        <WifiOff className="h-5 w-5 text-destructive" />
                        Smartphone Desconectado
                      </>
                    )}
                  </CardTitle>
                  <CardDescription>
                    Status atual da conexão do smartphone
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

              <Card className="border-dashed">
                <CardHeader>
                  <CardTitle className="text-lg">Como alterar a rede do smartphone</CardTitle>
                </CardHeader>
                <CardContent className="space-y-2 text-sm text-muted-foreground">
                  <p>• <strong>Android:</strong> Configurações → Rede e Internet → WiFi</p>
                  <p>• <strong>iOS:</strong> Ajustes → WiFi</p>
                  <p className="pt-2">Para conectar a uma rede diferente, acesse as configurações do sistema do seu dispositivo.</p>
                </CardContent>
              </Card>
            </TabsContent>
          </Tabs>
        </div>
      </main>
    </div>
  );
};

export default WiFiPage;
