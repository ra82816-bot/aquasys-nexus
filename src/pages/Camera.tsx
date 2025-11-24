import { useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { MqttFooter } from "@/components/dashboard/MqttFooter";
import { MqttProvider } from "@/contexts/MqttContext";
import { supabase } from "@/integrations/supabase/client";
import { Camera as CameraIcon, RefreshCw, AlertCircle, Settings, Eye, EyeOff } from "lucide-react";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { toast } from "sonner";
import { useCameraConfig } from "@/hooks/useCameraConfig";

const Camera = () => {
  const navigate = useNavigate();
  const imgRef = useRef<HTMLImageElement>(null);
  const intervalRef = useRef<NodeJS.Timeout | null>(null);
  
  const [isConnected, setIsConnected] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string>("");
  const [showSettings, setShowSettings] = useState(false);
  const [showPassword, setShowPassword] = useState(false);
  const { config, updateConfig, resetToDefaults, getMjpegUrl, getSnapshotUrl } = useCameraConfig();

  const fetchCameraStream = async () => {
    try {
      setIsLoading(true);
      setError("");

      const { data: { session } } = await supabase.auth.getSession();
      
      const url = config.streamType === 'snapshot' ? getSnapshotUrl() : getMjpegUrl();
      const supabaseUrl = import.meta.env.VITE_SUPABASE_URL;
      
      const response = await fetch(`${supabaseUrl}/functions/v1/camera-proxy`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${session?.access_token || ''}`,
        },
        body: JSON.stringify({
          url,
          username: config.username,
          password: config.password,
        }),
      });

      if (!response.ok) {
        throw new Error(`Erro ao conectar: ${response.status}`);
      }

      const contentType = response.headers.get('content-type');
      
      if (contentType?.includes('multipart/x-mixed-replace')) {
        // MJPEG stream
        const blob = await response.blob();
        if (imgRef.current) {
          imgRef.current.src = URL.createObjectURL(blob);
          setIsConnected(true);
        }
      } else if (contentType?.includes('image/')) {
        // Single snapshot
        const blob = await response.blob();
        if (imgRef.current) {
          imgRef.current.src = URL.createObjectURL(blob);
          setIsConnected(true);
        }
      } else {
        throw new Error('Formato de stream não suportado');
      }

      setIsLoading(false);
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : "Erro ao conectar à câmera";
      console.error("Erro:", err);
      setError(errorMessage);
      setIsConnected(false);
      setIsLoading(false);
      toast.error(errorMessage);
    }
  };

  const startSnapshotPolling = () => {
    stopSnapshotPolling();
    
    fetchCameraStream();
    
    // Atualizar snapshot a cada 1 segundo
    intervalRef.current = setInterval(() => {
      fetchCameraStream();
    }, 1000);
  };

  const stopSnapshotPolling = () => {
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
  };

  useEffect(() => {
    if (config.streamType === 'snapshot') {
      startSnapshotPolling();
    } else {
      fetchCameraStream();
    }

    return () => {
      stopSnapshotPolling();
      if (imgRef.current?.src) {
        URL.revokeObjectURL(imgRef.current.src);
      }
    };
  }, [config]);

  const handleLogout = async () => {
    await supabase.auth.signOut();
    navigate("/auth");
  };

  const handleNavigate = (path: string) => {
    navigate(path);
  };

  const handleReconnect = () => {
    setError("");
    setIsConnected(false);
    if (config.streamType === 'snapshot') {
      startSnapshotPolling();
    } else {
      fetchCameraStream();
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
              <CameraIcon className="h-6 w-6" />
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
                onClick={handleReconnect}
                disabled={isLoading}
              >
                <RefreshCw className={`h-4 w-4 ${isLoading ? "animate-spin" : ""}`} />
              </Button>
            </div>
          </div>

          {showSettings && (
            <Card>
              <CardHeader>
                <CardTitle>Configurações da Câmera</CardTitle>
                <CardDescription>
                  Configure o acesso à sua câmera IP
                </CardDescription>
              </CardHeader>
              <CardContent className="space-y-4">
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <Label htmlFor="ip">IP da Câmera</Label>
                    <Input
                      id="ip"
                      value={config.ip}
                      onChange={(e) => updateConfig({ ip: e.target.value })}
                      placeholder="192.168.0.17"
                    />
                  </div>
                  
                  <div>
                    <Label htmlFor="port">Porta</Label>
                    <Input
                      id="port"
                      type="number"
                      value={config.port}
                      onChange={(e) => updateConfig({ port: parseInt(e.target.value) })}
                      placeholder="80"
                    />
                  </div>
                </div>

                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <Label htmlFor="username">Usuário</Label>
                    <Input
                      id="username"
                      value={config.username}
                      onChange={(e) => updateConfig({ username: e.target.value })}
                      placeholder="admin"
                    />
                  </div>
                  
                  <div>
                    <Label htmlFor="password">Senha</Label>
                    <div className="flex gap-2">
                      <Input
                        id="password"
                        type={showPassword ? "text" : "password"}
                        value={config.password}
                        onChange={(e) => updateConfig({ password: e.target.value })}
                        placeholder="senha"
                        className="flex-1"
                      />
                      <Button
                        variant="outline"
                        size="icon"
                        onClick={() => setShowPassword(!showPassword)}
                        type="button"
                      >
                        {showPassword ? <EyeOff className="w-4 h-4" /> : <Eye className="w-4 h-4" />}
                      </Button>
                    </div>
                  </div>
                </div>

                <div>
                  <Label htmlFor="streamType">Tipo de Stream</Label>
                  <Select
                    value={config.streamType}
                    onValueChange={(value: 'mjpeg' | 'snapshot') => updateConfig({ streamType: value })}
                  >
                    <SelectTrigger>
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      <SelectItem value="mjpeg">MJPEG (Stream Contínuo)</SelectItem>
                      <SelectItem value="snapshot">Snapshot (Imagens)</SelectItem>
                    </SelectContent>
                  </Select>
                </div>

                <div>
                  <Label htmlFor="streamPath">
                    {config.streamType === 'mjpeg' ? 'Caminho do Stream' : 'Caminho do Snapshot'}
                  </Label>
                  <Input
                    id="streamPath"
                    value={config.streamType === 'mjpeg' ? config.streamPath : config.snapshotPath}
                    onChange={(e) => updateConfig({ 
                      [config.streamType === 'mjpeg' ? 'streamPath' : 'snapshotPath']: e.target.value 
                    })}
                    placeholder={config.streamType === 'mjpeg' ? '/video/mjpeg.cgi' : '/snapshot.jpg'}
                  />
                  <p className="text-sm text-muted-foreground mt-1">
                    Caminhos comuns: /video/mjpeg.cgi, /mjpeg, /snapshot.jpg, /snap.jpg
                  </p>
                </div>

                <div className="flex gap-2">
                  <Button onClick={handleReconnect} className="flex-1">
                    Aplicar e Conectar
                  </Button>
                  <Button onClick={resetToDefaults} variant="outline">
                    Restaurar Padrões
                  </Button>
                </div>
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
                      <p className="text-sm text-muted-foreground">Conectando à câmera...</p>
                    </div>
                  </div>
                )}
                
                {!isConnected && !isLoading && !error && (
                  <div className="absolute inset-0 flex items-center justify-center">
                    <div className="text-center space-y-4">
                      <CameraIcon className="h-12 w-12 mx-auto text-muted-foreground" />
                      <div className="space-y-2">
                        <p className="text-sm text-muted-foreground">Configure sua câmera nas configurações acima</p>
                        <Button onClick={() => setShowSettings(true)} variant="outline" size="sm">
                          Abrir Configurações
                        </Button>
                      </div>
                    </div>
                  </div>
                )}

                <img
                  ref={imgRef}
                  className="w-full h-full object-contain"
                  alt="Stream da câmera"
                  style={{ display: isConnected ? 'block' : 'none' }}
                />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardHeader>
              <CardTitle>Como Usar</CardTitle>
            </CardHeader>
            <CardContent className="space-y-2 text-sm text-muted-foreground">
              <p>✅ <strong>Sem bridge Python necessário</strong> - Conexão direta via edge function</p>
              <p>✅ <strong>Suporte a MJPEG e Snapshot</strong> - Funciona com a maioria das câmeras IP</p>
              <p>✅ <strong>Configuração fácil</strong> - Apenas IP, porta e credenciais</p>
              <div className="mt-4 p-3 bg-muted rounded-lg">
                <p className="font-medium mb-2">💡 Dica: Caminhos comuns de câmeras IP</p>
                <ul className="list-disc list-inside space-y-1 text-xs">
                  <li>Hikvision: /Streaming/channels/1/preview</li>
                  <li>Dahua: /cgi-bin/snapshot.cgi</li>
                  <li>Genérico MJPEG: /video/mjpeg.cgi ou /mjpeg</li>
                  <li>Genérico Snapshot: /snapshot.jpg ou /snap.jpg</li>
                </ul>
              </div>
            </CardContent>
          </Card>
        </main>

        <MqttFooter />
      </div>
    </MqttProvider>
  );
};

export default Camera;
