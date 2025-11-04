import { useEffect, useState } from "react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Progress } from "@/components/ui/progress";
import { Button } from "@/components/ui/button";
import { supabase } from "@/integrations/supabase/client";
import { Loader2, Activity, WifiOff, Cpu, Bluetooth, Wifi, Database, Unlink } from "lucide-react";
import { formatDistanceToNow } from "date-fns";
import { ptBR } from "date-fns/locale";
import { useToast } from "@/hooks/use-toast";
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
  AlertDialogTrigger,
} from "@/components/ui/alert-dialog";

interface Device {
  id: string;
  device_uuid: string;
  device_type: "sensor" | "actuator";
  firmware_version: string;
  first_seen_at: string;
  last_seen_at: string | null;
}

interface DeviceHealth {
  wifi_rssi: number | null;
  mqtt_connected: boolean | null;
  free_heap: number | null;
  sensor_ph_valid: boolean | null;
  sensor_ec_valid: boolean | null;
}

export const DeviceList = () => {
  const [devices, setDevices] = useState<Device[]>([]);
  const [deviceHealth, setDeviceHealth] = useState<Record<string, DeviceHealth>>({});
  const [isLoading, setIsLoading] = useState(true);
  const { toast } = useToast();

  useEffect(() => {
    loadDevices();
    loadDeviceHealth();
    
    // Atualizar a cada 30 segundos
    const interval = setInterval(() => {
      loadDevices();
      loadDeviceHealth();
    }, 30000);
    
    return () => clearInterval(interval);
  }, []);

  const loadDevices = async () => {
    try {
      // ✅ FASE 1: RLS já filtra dispositivos do usuário via device_owners
      const { data, error } = await supabase
        .from("devices")
        .select("*")
        .order("last_seen_at", { ascending: false, nullsFirst: false });

      if (error) throw error;

      setDevices(data || []);
    } catch (error) {
      console.error("Erro ao carregar dispositivos:", error);
    } finally {
      setIsLoading(false);
    }
  };

  const loadDeviceHealth = async () => {
    try {
      // ✅ FASE 1: Buscar último health de cada dispositivo
      const { data, error } = await supabase
        .from("device_health")
        .select("device_id, wifi_rssi, mqtt_connected, free_heap, sensor_ph_valid, sensor_ec_valid")
        .order("timestamp", { ascending: false });

      if (error) throw error;

      // Agrupar por device_id (pegar apenas o mais recente)
      const healthMap: Record<string, DeviceHealth> = {};
      data?.forEach((health) => {
        if (!healthMap[health.device_id]) {
          healthMap[health.device_id] = health;
        }
      });

      setDeviceHealth(healthMap);
    } catch (error) {
      console.error("Erro ao carregar health:", error);
    }
  };

  const isOnline = (lastSeen: string | null) => {
    if (!lastSeen) return false;
    const lastSeenTime = new Date(lastSeen).getTime();
    const now = Date.now();
    // ✅ FASE 1: Online se visto nos últimos 5 minutos (device-auth + heartbeats)
    return now - lastSeenTime < 300000; // 5 minutos
  };

  const getSignalStrength = (rssi: number | null) => {
    if (!rssi) return { label: "Desconhecido", value: 0, color: "text-muted-foreground" };
    if (rssi >= -50) return { label: "Excelente", value: 100, color: "text-green-500" };
    if (rssi >= -60) return { label: "Bom", value: 75, color: "text-green-500" };
    if (rssi >= -70) return { label: "Razoável", value: 50, color: "text-yellow-500" };
    return { label: "Fraco", value: 25, color: "text-red-500" };
  };

  const getMemoryStatus = (freeHeap: number | null) => {
    if (!freeHeap) return { label: "N/A", value: 0, color: "text-muted-foreground" };
    const heapKB = freeHeap / 1024;
    if (heapKB > 100) return { label: "Ótimo", value: 100, color: "text-green-500" };
    if (heapKB > 50) return { label: "Bom", value: 75, color: "text-yellow-500" };
    return { label: "Baixa", value: 25, color: "text-red-500" };
  };

  const handleUnpairDevice = async (deviceId: string, deviceUuid: string) => {
    try {
      const { error } = await supabase
        .from("device_owners")
        .delete()
        .eq("device_id", deviceId);

      if (error) throw error;

      toast({
        title: "Dispositivo desvinculado",
        description: `${deviceUuid} foi removido da sua conta`,
      });

      // Recarregar lista
      loadDevices();
    } catch (error: any) {
      console.error("Erro ao desvincular:", error);
      toast({
        title: "Erro ao desvincular",
        description: error.message || "Não foi possível desvincular o dispositivo",
        variant: "destructive",
      });
    }
  };

  if (isLoading) {
    return (
      <Card>
        <CardContent className="flex items-center justify-center py-8">
          <Loader2 className="h-8 w-8 animate-spin text-muted-foreground" />
        </CardContent>
      </Card>
    );
  }

  if (devices.length === 0) {
    return (
      <Card>
        <CardHeader>
          <CardTitle>Meus Dispositivos</CardTitle>
          <CardDescription>
            Nenhum dispositivo vinculado ainda
          </CardDescription>
        </CardHeader>
      </Card>
    );
  }

  return (
    <div className="space-y-4">
      <h2 className="text-2xl font-bold">Meus Dispositivos</h2>
      <div className="grid gap-4 md:grid-cols-2">
        {devices.map((device) => {
          const online = isOnline(device.last_seen_at);
          const health = deviceHealth[device.id];
          const signal = getSignalStrength(health?.wifi_rssi);
          const memory = getMemoryStatus(health?.free_heap);
          
          const lastSeenText = device.last_seen_at
            ? formatDistanceToNow(new Date(device.last_seen_at), {
                addSuffix: true,
                locale: ptBR,
              })
            : "Nunca visto";

          return (
            <Card key={device.id} className="hover:shadow-lg transition-shadow">
              <CardHeader>
                <div className="flex items-start justify-between">
                  <div className="space-y-1">
                    <CardTitle className="text-base flex items-center gap-2">
                      <Cpu className="h-4 w-4" />
                      {device.device_uuid}
                    </CardTitle>
                    <CardDescription>
                      Firmware v{device.firmware_version}
                      {device.firmware_version.includes("BLE") && (
                        <Badge variant="outline" className="ml-2 text-xs">
                          <Bluetooth className="h-3 w-3 mr-1" />
                          BLE
                        </Badge>
                      )}
                    </CardDescription>
                  </div>
                  <Badge
                    variant={online ? "default" : "secondary"}
                    className="flex items-center gap-1"
                  >
                    {online ? (
                      <>
                        <Activity className="h-3 w-3" />
                        Online
                      </>
                    ) : (
                      <>
                        <WifiOff className="h-3 w-3" />
                        Offline
                      </>
                    )}
                  </Badge>
                </div>
              </CardHeader>
              <CardContent className="space-y-3">
                <div className="flex items-center justify-between text-sm">
                  <span className="text-muted-foreground">Tipo:</span>
                  <Badge variant="outline">
                    {device.device_type === "sensor"
                      ? "Módulo de Sensores"
                      : "Módulo de Atuadores"}
                  </Badge>
                </div>
                
                {/* Status de conexão */}
                <div className="space-y-2 pt-2 border-t">
                  <div className="flex items-center justify-between text-sm">
                    <div className="flex items-center gap-2">
                      <Wifi className="h-4 w-4 text-muted-foreground" />
                      <span className="text-muted-foreground">WiFi:</span>
                    </div>
                    <span className={`font-medium ${signal.color}`}>
                      {signal.label} ({health?.wifi_rssi || "N/A"} dBm)
                    </span>
                  </div>
                  <Progress value={signal.value} className="h-2" />
                </div>

                <div className="space-y-2">
                  <div className="flex items-center justify-between text-sm">
                    <div className="flex items-center gap-2">
                      <Database className="h-4 w-4 text-muted-foreground" />
                      <span className="text-muted-foreground">Memória:</span>
                    </div>
                    <span className={`font-medium ${memory.color}`}>
                      {memory.label} ({health?.free_heap ? (health.free_heap / 1024).toFixed(0) : "N/A"} KB)
                    </span>
                  </div>
                  <Progress value={memory.value} className="h-2" />
                </div>

                {/* Status dos sensores (apenas para módulos de sensores) */}
                {device.device_type === "sensor" && health && (
                  <div className="space-y-1 pt-2 border-t">
                    <span className="text-sm text-muted-foreground">Sensores:</span>
                    <div className="flex gap-2 flex-wrap">
                      <Badge variant={health.sensor_ph_valid ? "default" : "secondary"} className="text-xs">
                        pH {health.sensor_ph_valid ? "✓" : "✗"}
                      </Badge>
                      <Badge variant={health.sensor_ec_valid ? "default" : "secondary"} className="text-xs">
                        EC {health.sensor_ec_valid ? "✓" : "✗"}
                      </Badge>
                    </div>
                  </div>
                )}

                <div className="flex items-center justify-between text-sm pt-2 border-t">
                  <span className="text-muted-foreground">Última atividade:</span>
                  <span className="font-medium">{lastSeenText}</span>
                </div>

                {/* Botão para desvincular */}
                <div className="pt-2 border-t">
                  <AlertDialog>
                    <AlertDialogTrigger asChild>
                      <Button variant="outline" size="sm" className="w-full">
                        <Unlink className="h-4 w-4 mr-2" />
                        Desvincular Dispositivo
                      </Button>
                    </AlertDialogTrigger>
                    <AlertDialogContent>
                      <AlertDialogHeader>
                        <AlertDialogTitle>Desvincular dispositivo?</AlertDialogTitle>
                        <AlertDialogDescription>
                          Tem certeza que deseja desvincular <strong>{device.device_uuid}</strong>?
                          <br /><br />
                          O dispositivo será removido da sua conta e poderá ser vinculado novamente ou por outro usuário.
                        </AlertDialogDescription>
                      </AlertDialogHeader>
                      <AlertDialogFooter>
                        <AlertDialogCancel>Cancelar</AlertDialogCancel>
                        <AlertDialogAction
                          onClick={() => handleUnpairDevice(device.id, device.device_uuid)}
                          className="bg-destructive text-destructive-foreground hover:bg-destructive/90"
                        >
                          Desvincular
                        </AlertDialogAction>
                      </AlertDialogFooter>
                    </AlertDialogContent>
                  </AlertDialog>
                </div>
              </CardContent>
            </Card>
          );
        })}
      </div>
    </div>
  );
};