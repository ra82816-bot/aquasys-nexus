import { useState, useEffect, useRef } from "react";
import { useNavigate } from "react-router-dom";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { MqttProvider } from "@/contexts/MqttContext";
import { MqttFooter } from "@/components/dashboard/MqttFooter";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Badge } from "@/components/ui/badge";
import { Camera as CameraIcon, RefreshCw, Settings, Video, VideoOff } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { supabase } from "@/integrations/supabase/client";

export default function Camera() {
  const navigate = useNavigate();
  const [cameraUrl, setCameraUrl] = useState("http://esp32cam.local/stream");
  const [isConnected, setIsConnected] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [showSettings, setShowSettings] = useState(false);
  const [customUrl, setCustomUrl] = useState("");
  const [username, setUsername] = useState("admin");
  const [password, setPassword] = useState("");
  const imgRef = useRef<HTMLImageElement>(null);
  const { toast } = useToast();

  useEffect(() => {
    // Carregar configurações salvas do localStorage
    const savedUrl = localStorage.getItem("esp32cam_url");
    const savedUsername = localStorage.getItem("esp32cam_username");
    const savedPassword = localStorage.getItem("esp32cam_password");
    
    if (savedUrl) {
      setCustomUrl(savedUrl);
    }
    if (savedUsername) {
      setUsername(savedUsername);
    }
    if (savedPassword) {
      setPassword(savedPassword);
    }
    
    // Construir URL do proxy se necessário
    if (savedUrl) {
      buildProxyUrl(savedUrl, savedUsername || "admin", savedPassword || "");
    }
  }, []);

  useEffect(() => {
    // Atualizar stream automaticamente a cada 2 segundos
    const interval = setInterval(() => {
      if (isConnected && !isLoading) {
        handleRefresh();
      }
    }, 2000);
    
    return () => clearInterval(interval);
  }, [isConnected, isLoading]);

  const checkConnection = () => {
    if (imgRef.current) {
      const img = imgRef.current;
      if (img.complete && img.naturalHeight !== 0) {
        setIsConnected(true);
      } else {
        setIsConnected(false);
      }
    }
  };

  const handleRefresh = async () => {
    setIsLoading(true);
    
    try {
      const url = sessionStorage.getItem('camera_url') || customUrl;
      const user = sessionStorage.getItem('camera_username') || username;
      const pass = sessionStorage.getItem('camera_password') || password;

      // Fazer requisição via proxy
      const response = await fetch(`${import.meta.env.VITE_SUPABASE_URL}/functions/v1/camera-proxy`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'apikey': import.meta.env.VITE_SUPABASE_ANON_KEY,
        },
        body: JSON.stringify({ url, username: user, password: pass }),
      });

      if (response.ok) {
        const blob = await response.blob();
        const objectUrl = URL.createObjectURL(blob);
        
        if (imgRef.current) {
          // Revogar URL anterior para evitar vazamento de memória
          if (imgRef.current.src.startsWith('blob:')) {
            URL.revokeObjectURL(imgRef.current.src);
          }
          imgRef.current.src = objectUrl;
        }
      } else {
        throw new Error(`HTTP ${response.status}`);
      }
    } catch (error) {
      console.error('Erro ao atualizar câmera:', error);
      toast({
        title: "Erro ao conectar",
        description: "Verifique a URL e as credenciais",
        variant: "destructive"
      });
    } finally {
      setIsLoading(false);
    }
  };

  const buildProxyUrl = (url: string, user: string, pass: string) => {
    // Usar a Edge Function como proxy para evitar problemas de CORS
    const proxyUrl = `${import.meta.env.VITE_SUPABASE_URL}/functions/v1/camera-proxy`;
    
    // Armazenar as credenciais para uso posterior
    sessionStorage.setItem('camera_url', url);
    sessionStorage.setItem('camera_username', user);
    sessionStorage.setItem('camera_password', pass);
    
    setCameraUrl(proxyUrl);
  };

  const handleSaveUrl = () => {
    if (!customUrl.trim()) {
      toast({
        title: "URL inválida",
        description: "Por favor, insira uma URL válida",
        variant: "destructive"
      });
      return;
    }

    // Salvar configurações
    localStorage.setItem("esp32cam_url", customUrl);
    localStorage.setItem("esp32cam_username", username);
    localStorage.setItem("esp32cam_password", password);
    
    // Construir URL do proxy
    buildProxyUrl(customUrl, username, password);
    
    setShowSettings(false);
    
    toast({
      title: "Configurações salvas",
      description: "Câmera configurada com sucesso"
    });
    
    handleRefresh();
  };

  const handleImageLoad = () => {
    setIsConnected(true);
    setIsLoading(false);
  };

  const handleImageError = () => {
    setIsConnected(false);
    setIsLoading(false);
  };

  const handleLogout = async () => {
    await supabase.auth.signOut();
    navigate("/auth");
  };

  const handleNavigate = (path: string) => {
    navigate(path);
  };

  return (
    <MqttProvider>
      <div className="min-h-screen bg-background pb-16">
        <AppHeader onLogout={handleLogout} onNavigate={handleNavigate} />
        
        <main className="container mx-auto px-4 py-6 space-y-6">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <CameraIcon className="h-8 w-8 text-primary" />
              <div>
                <h1 className="text-3xl font-bold">Câmera ESP32-CAM</h1>
                <p className="text-muted-foreground">Monitoramento em tempo real</p>
              </div>
            </div>
            <div className="flex items-center gap-2">
              <Badge 
                variant="outline"
                className={`border ${
                  isConnected 
                    ? 'bg-green-500/10 text-green-500 border-green-500/20' 
                    : 'bg-red-500/10 text-red-500 border-red-500/20'
                }`}
              >
                {isConnected ? (
                  <><Video className="h-3 w-3 mr-1" /> Conectado</>
                ) : (
                  <><VideoOff className="h-3 w-3 mr-1" /> Desconectado</>
                )}
              </Badge>
            </div>
          </div>

          {showSettings ? (
            <Card>
              <CardHeader>
                <CardTitle className="flex items-center gap-2">
                  <Settings className="h-5 w-5" />
                  Configurações da Câmera
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-4">
                <div>
                  <Label htmlFor="cameraUrl">URL da Câmera ESP32-CAM</Label>
                  <p className="text-xs text-muted-foreground mb-2">
                    Exemplo: http://192.168.1.100/stream (sem incluir usuário e senha)
                  </p>
                  <Input
                    id="cameraUrl"
                    value={customUrl}
                    onChange={(e) => setCustomUrl(e.target.value)}
                    placeholder="http://192.168.1.100/stream"
                  />
                </div>
                
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <Label htmlFor="username">Usuário</Label>
                    <p className="text-xs text-muted-foreground mb-2">
                      Padrão: admin
                    </p>
                    <Input
                      id="username"
                      value={username}
                      onChange={(e) => setUsername(e.target.value)}
                      placeholder="admin"
                    />
                  </div>
                  
                  <div>
                    <Label htmlFor="password">Senha</Label>
                    <p className="text-xs text-muted-foreground mb-2">
                      Deixe vazio se não houver
                    </p>
                    <Input
                      id="password"
                      type="password"
                      value={password}
                      onChange={(e) => setPassword(e.target.value)}
                      placeholder="••••••••"
                    />
                  </div>
                </div>
                
                <div className="flex gap-2">
                  <Button onClick={handleSaveUrl}>Salvar Configurações</Button>
                  <Button variant="outline" onClick={() => setShowSettings(false)}>
                    Cancelar
                  </Button>
                </div>
              </CardContent>
            </Card>
          ) : (
            <Card>
              <CardHeader>
                <div className="flex items-center justify-between">
                  <CardTitle>Stream de Vídeo</CardTitle>
                  <div className="flex gap-2">
                    <Button
                      onClick={handleRefresh}
                      disabled={isLoading}
                      variant="outline"
                      size="sm"
                    >
                      {isLoading ? (
                        <div className="h-4 w-4 border-2 border-current border-t-transparent rounded-full animate-spin" />
                      ) : (
                        <RefreshCw className="h-4 w-4" />
                      )}
                    </Button>
                    <Button
                      onClick={() => setShowSettings(true)}
                      variant="outline"
                      size="sm"
                    >
                      <Settings className="h-4 w-4" />
                    </Button>
                  </div>
                </div>
              </CardHeader>
              <CardContent>
                <div className="relative aspect-video bg-black rounded-lg overflow-hidden">
                  {isLoading && (
                    <div className="absolute inset-0 flex items-center justify-center bg-black/50 z-10">
                      <div className="h-12 w-12 border-4 border-primary border-t-transparent rounded-full animate-spin" />
                    </div>
                  )}
                  
                  {!isConnected && !isLoading && (
                    <div className="absolute inset-0 flex flex-col items-center justify-center text-muted-foreground">
                      <VideoOff className="h-16 w-16 mb-4" />
                      <p className="text-lg font-medium">Câmera não conectada</p>
                      <p className="text-sm">Verifique a URL e tente novamente</p>
                      <Button
                        onClick={() => setShowSettings(true)}
                        variant="outline"
                        className="mt-4"
                      >
                        <Settings className="h-4 w-4 mr-2" />
                        Configurar
                      </Button>
                    </div>
                  )}
                  
                  <img
                    ref={imgRef}
                    src={cameraUrl}
                    alt="ESP32-CAM Stream"
                    className="w-full h-full object-contain"
                    onLoad={handleImageLoad}
                    onError={handleImageError}
                    style={{ display: isConnected ? 'block' : 'none' }}
                  />
                </div>
                
                {isConnected && (
                  <div className="mt-4 text-sm text-muted-foreground flex items-center justify-between">
                    <span>Stream ativo: {cameraUrl}</span>
                    <span className="flex items-center gap-2">
                      <div className="h-2 w-2 rounded-full bg-green-500 animate-pulse" />
                      Ao vivo
                    </span>
                  </div>
                )}
              </CardContent>
            </Card>
          )}

          <Card>
            <CardHeader>
              <CardTitle className="text-sm">Instruções</CardTitle>
            </CardHeader>
            <CardContent className="text-sm text-muted-foreground space-y-2">
              <p>• Certifique-se de que o módulo ESP32-CAM está ligado e conectado à mesma rede Wi-Fi</p>
              <p>• Configure a URL base da câmera (ex: http://192.168.1.100/stream)</p>
              <p>• Adicione usuário e senha se a câmera estiver protegida</p>
              <p>• O usuário padrão da maioria das ESP32-CAM é "admin"</p>
              <p>• Use o botão de atualizar para forçar um novo carregamento do stream</p>
            </CardContent>
          </Card>
        </main>

        <MqttFooter />
      </div>
    </MqttProvider>
  );
}
