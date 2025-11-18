import { useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import Hls from "hls.js";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { MqttFooter } from "@/components/dashboard/MqttFooter";
import { MqttProvider } from "@/contexts/MqttContext";
import { supabase } from "@/integrations/supabase/client";
import { Video, RefreshCw, AlertCircle, Settings } from "lucide-react";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";

const Camera = () => {
  const navigate = useNavigate();
  const videoRef = useRef<HTMLVideoElement>(null);
  const hlsRef = useRef<Hls | null>(null);
  
  const [isConnected, setIsConnected] = useState(false);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [showSettings, setShowSettings] = useState(false);
  const [streamUrl, setStreamUrl] = useState("http://localhost:5000/stream/stream.m3u8");
  const [customUrl, setCustomUrl] = useState("");

  useEffect(() => {
    const savedUrl = localStorage.getItem("camera_stream_url");
    if (savedUrl) {
      setStreamUrl(savedUrl);
      setCustomUrl(savedUrl);
    }
  }, []);

  const initializeStream = () => {
    if (!videoRef.current) return;

    setIsLoading(true);
    setError(null);

    if (hlsRef.current) {
      hlsRef.current.destroy();
    }

    if (Hls.isSupported()) {
      const hls = new Hls({
        enableWorker: true,
        lowLatencyMode: true,
        backBufferLength: 90,
      });

      hls.loadSource(streamUrl);
      hls.attachMedia(videoRef.current);

      hls.on(Hls.Events.MANIFEST_PARSED, () => {
        console.log("Stream HLS carregado");
        videoRef.current?.play();
        setIsConnected(true);
        setIsLoading(false);
      });

      hls.on(Hls.Events.ERROR, (event, data) => {
        console.error("Erro HLS:", data);
        if (data.fatal) {
          setIsConnected(false);
          setIsLoading(false);
          
          switch (data.type) {
            case Hls.ErrorTypes.NETWORK_ERROR:
              setError("Erro de conexão. Verifique se o bridge está rodando.");
              setTimeout(() => {
                console.log("Tentando reconectar...");
                hls.loadSource(streamUrl);
              }, 3000);
              break;
            case Hls.ErrorTypes.MEDIA_ERROR:
              setError("Erro de mídia. Tentando recuperar...");
              hls.recoverMediaError();
              break;
            default:
              setError("Erro fatal no stream.");
              hls.destroy();
              break;
          }
        }
      });

      hlsRef.current = hls;
    } else if (videoRef.current.canPlayType("application/vnd.apple.mpegurl")) {
      videoRef.current.src = streamUrl;
      videoRef.current.addEventListener("loadedmetadata", () => {
        setIsConnected(true);
        setIsLoading(false);
      });
      videoRef.current.addEventListener("error", () => {
        setError("Erro ao carregar stream");
        setIsConnected(false);
        setIsLoading(false);
      });
    } else {
      setError("Navegador não suporta HLS");
      setIsLoading(false);
    }
  };

  useEffect(() => {
    initializeStream();

    return () => {
      if (hlsRef.current) {
        hlsRef.current.destroy();
      }
    };
  }, [streamUrl]);

  const handleLogout = async () => {
    await supabase.auth.signOut();
    navigate("/auth");
  };

  const handleNavigate = (path: string) => {
    navigate(path);
  };

  const handleRefresh = () => {
    initializeStream();
  };

  const handleSaveUrl = () => {
    if (customUrl) {
      setStreamUrl(customUrl);
      localStorage.setItem("camera_stream_url", customUrl);
      setShowSettings(false);
    }
  };

  return (
    <MqttProvider>
      <div className="min-h-screen bg-background">
        <AppHeader 
          onLogout={handleLogout}
          onNavigate={handleNavigate}
        />
        
        <main className="container mx-auto p-4 space-y-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <Video className="h-6 w-6" />
              <h1 className="text-2xl font-bold">Câmera IP</h1>
            </div>
            <div className="flex items-center gap-2">
              <Badge variant={isConnected ? "default" : "destructive"}>
                {isConnected ? "Conectado" : "Desconectado"}
              </Badge>
              <Button
                variant="outline"
                size="icon"
                onClick={() => setShowSettings(!showSettings)}
              >
                <Settings className="h-4 w-4" />
              </Button>
              <Button
                variant="outline"
                size="icon"
                onClick={handleRefresh}
                disabled={isLoading}
              >
                <RefreshCw className={`h-4 w-4 ${isLoading ? "animate-spin" : ""}`} />
              </Button>
            </div>
          </div>

          {showSettings && (
            <Card>
              <CardHeader>
                <CardTitle>Configurações de Stream</CardTitle>
                <CardDescription>
                  Configure a URL do stream HLS
                </CardDescription>
              </CardHeader>
              <CardContent className="space-y-4">
                <div className="space-y-2">
                  <Label htmlFor="stream-url">URL do Stream</Label>
                  <Input
                    id="stream-url"
                    value={customUrl}
                    onChange={(e) => setCustomUrl(e.target.value)}
                    placeholder="http://localhost:5000/stream/stream.m3u8"
                  />
                </div>
                <Button onClick={handleSaveUrl}>Salvar e Conectar</Button>
              </CardContent>
            </Card>
          )}

          <Card>
            <CardContent className="p-0">
              {error && (
                <div className="p-4 bg-destructive/10 border-b border-destructive/20 flex items-center gap-2">
                  <AlertCircle className="h-4 w-4 text-destructive" />
                  <p className="text-sm text-destructive">{error}</p>
                </div>
              )}
              
              <div className="relative aspect-video bg-muted">
                {isLoading && (
                  <div className="absolute inset-0 flex items-center justify-center">
                    <div className="text-center space-y-2">
                      <RefreshCw className="h-8 w-8 animate-spin mx-auto text-muted-foreground" />
                      <p className="text-sm text-muted-foreground">Carregando stream...</p>
                    </div>
                  </div>
                )}
                
                {!isConnected && !isLoading && (
                  <div className="absolute inset-0 flex items-center justify-center">
                    <div className="text-center space-y-4">
                      <AlertCircle className="h-12 w-12 mx-auto text-muted-foreground" />
                      <div className="space-y-2">
                        <p className="text-sm text-muted-foreground">Stream desconectado</p>
                        <Button onClick={handleRefresh} variant="outline" size="sm">
                          Tentar Novamente
                        </Button>
                      </div>
                    </div>
                  </div>
                )}

                <video
                  ref={videoRef}
                  className="w-full h-full"
                  controls
                  autoPlay
                  muted
                  playsInline
                />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardHeader>
              <CardTitle>Instruções</CardTitle>
            </CardHeader>
            <CardContent className="space-y-2 text-sm text-muted-foreground">
              <p>1. Certifique-se de que o bridge Python está rodando com FFmpeg instalado</p>
              <p>2. O stream RTSP da câmera será convertido automaticamente para HLS</p>
              <p>3. Configure as variáveis de ambiente no bridge se necessário:</p>
              <ul className="list-disc list-inside pl-4 space-y-1">
                <li>CAMERA_IP (padrão: 192.168.0.17)</li>
                <li>CAMERA_USER (padrão: admin)</li>
                <li>CAMERA_PASS</li>
                <li>CAMERA_RTSP_PATH (padrão: stream1)</li>
              </ul>
            </CardContent>
          </Card>
        </main>

        <MqttFooter />
      </div>
    </MqttProvider>
  );
};

export default Camera;
