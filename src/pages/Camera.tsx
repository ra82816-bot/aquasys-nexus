import { useState, useEffect, useRef } from "react";
import { useNavigate } from "react-router-dom";
import { AppHeader } from "@/components/dashboard/AppHeader";
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
  const imgRef = useRef<HTMLImageElement>(null);
  const { toast } = useToast();

  useEffect(() => {
    // Carregar URL salva do localStorage
    const savedUrl = localStorage.getItem("esp32cam_url");
    if (savedUrl) {
      setCameraUrl(savedUrl);
      setCustomUrl(savedUrl);
    }
  }, []);

  useEffect(() => {
    checkConnection();
    const interval = setInterval(checkConnection, 10000); // Verifica a cada 10s
    return () => clearInterval(interval);
  }, [cameraUrl]);

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

  const handleRefresh = () => {
    setIsLoading(true);
    if (imgRef.current) {
      const timestamp = new Date().getTime();
      imgRef.current.src = `${cameraUrl}?t=${timestamp}`;
    }
    setTimeout(() => setIsLoading(false), 1000);
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

    setCameraUrl(customUrl);
    localStorage.setItem("esp32cam_url", customUrl);
    setShowSettings(false);
    
    toast({
      title: "URL salva",
      description: "Configuração da câmera atualizada"
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
    <div className="min-h-screen bg-background">
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
                  Exemplos: http://192.168.1.100/stream ou http://esp32cam.local/stream
                </p>
                <Input
                  id="cameraUrl"
                  value={customUrl}
                  onChange={(e) => setCustomUrl(e.target.value)}
                  placeholder="http://esp32cam.local/stream"
                />
              </div>
              <div className="flex gap-2">
                <Button onClick={handleSaveUrl}>Salvar</Button>
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
            <p>• Configure a URL correta da câmera nas configurações</p>
            <p>• O stream pode usar MJPEG (Motion JPEG) ou snapshots periódicos</p>
            <p>• Use o botão de atualizar para forçar um novo carregamento do stream</p>
          </CardContent>
        </Card>
      </main>
    </div>
  );
}
