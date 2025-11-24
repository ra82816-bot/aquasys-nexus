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

      const url = config.streamType === 'snapshot' ? getSnapshotUrl() : getMjpegUrl();
      
      // Construir autenticação Basic
      const authHeader = `Basic ${btoa(`${config.username}:${config.password}`)}`;
      
      // Tentar acesso direto (funciona se câmera está na mesma rede)
      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'Authorization': authHeader,
        },
        mode: 'cors', // Tentará CORS primeiro
      });

      if (!response.ok) {
        if (response.status === 401) {
          throw new Error('Credenciais inválidas. Verifique usuário e senha.');
        }
        throw new Error(`Erro HTTP ${response.status}: ${response.statusText}`);
      }

      const contentType = response.headers.get('content-type');
      
      if (contentType?.includes('multipart/x-mixed-replace')) {
        // MJPEG stream contínuo
        if (imgRef.current) {
          // Para MJPEG, definir src diretamente com auth
          imgRef.current.src = url.replace('http://', `http://${config.username}:${config.password}@`);
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
        throw new Error('Formato não suportado. Tente outro caminho de stream.');
      }

      setIsLoading(false);
    } catch (err) {
      let errorMessage = "Erro ao conectar à câmera";
      
      if (err instanceof Error) {
        if (err.message.includes('Failed to fetch') || err.message.includes('NetworkError')) {
          errorMessage = "Não foi possível conectar. Certifique-se de que:\n1. A câmera está na mesma rede\n2. O IP está correto\n3. O caminho do stream está correto";
        } else if (err.message.includes('CORS')) {
          errorMessage = "Erro CORS. Você precisa:\n1. Ativar CORS nas configurações da câmera\nOU\n2. Usar o bridge Python local";
        } else {
          errorMessage = err.message;
        }
      }
      
      console.error("Erro completo:", err);
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
            <CardContent className="space-y-3 text-sm">
              <div className="space-y-2">
                <p className="font-medium text-foreground">✅ Requisitos:</p>
                <ul className="list-disc list-inside space-y-1 text-muted-foreground">
                  <li>Câmera e dispositivo na <strong>mesma rede local</strong></li>
                  <li>IP, porta e credenciais corretas</li>
                  <li>Caminho do stream configurado</li>
                </ul>
              </div>

              <div className="p-3 bg-muted rounded-lg space-y-2">
                <p className="font-medium text-foreground">💡 Caminhos comuns por marca:</p>
                <div className="space-y-1 text-xs text-muted-foreground">
                  <p><strong>Hikvision:</strong></p>
                  <p className="pl-4">• MJPEG: /Streaming/channels/1/preview</p>
                  <p className="pl-4">• Snapshot: /ISAPI/Streaming/channels/101/picture</p>
                  
                  <p className="mt-2"><strong>Dahua:</strong></p>
                  <p className="pl-4">• Snapshot: /cgi-bin/snapshot.cgi</p>
                  
                  <p className="mt-2"><strong>Genérico:</strong></p>
                  <p className="pl-4">• MJPEG: /video/mjpeg.cgi, /mjpeg, /video</p>
                  <p className="pl-4">• Snapshot: /snapshot.jpg, /snap.jpg</p>
                </div>
              </div>

              <div className="p-3 bg-yellow-500/10 border border-yellow-500/20 rounded-lg space-y-2">
                <p className="font-medium text-yellow-600 dark:text-yellow-500">⚠️ Se não conectar:</p>
                <ol className="list-decimal list-inside space-y-1 text-xs text-muted-foreground">
                  <li>Teste o IP no navegador: <code className="text-xs bg-muted px-1 py-0.5 rounded">http://{config.ip}</code></li>
                  <li>Verifique se o caminho está correto</li>
                  <li>Confirme usuário e senha</li>
                  <li>Se erro CORS: ative CORS nas configurações da câmera</li>
                </ol>
              </div>

              <div className="p-3 bg-blue-500/10 border border-blue-500/20 rounded-lg">
                <p className="text-xs text-muted-foreground">
                  <strong className="text-blue-600 dark:text-blue-500">ℹ️ Nota:</strong> A conexão é feita diretamente do seu navegador 
                  para a câmera (sem usar servidores externos). Por isso, ambos precisam estar na mesma rede local.
                </p>
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
