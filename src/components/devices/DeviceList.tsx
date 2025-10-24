import { useEffect, useState } from "react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { supabase } from "@/integrations/supabase/client";
import { Loader2, Activity, WifiOff, Cpu } from "lucide-react";
import { formatDistanceToNow } from "date-fns";
import { ptBR } from "date-fns/locale";

interface Device {
  id: string;
  device_uuid: string;
  device_type: "sensor" | "actuator";
  firmware_version: string;
  first_seen_at: string;
  last_seen_at: string | null;
}

export const DeviceList = () => {
  const [devices, setDevices] = useState<Device[]>([]);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    loadDevices();
  }, []);

  const loadDevices = async () => {
    try {
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

  const isOnline = (lastSeen: string | null) => {
    if (!lastSeen) return false;
    const lastSeenTime = new Date(lastSeen).getTime();
    const now = Date.now();
    return now - lastSeenTime < 120000; // Online se visto nos últimos 2 minutos
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
          const lastSeenText = device.last_seen_at
            ? formatDistanceToNow(new Date(device.last_seen_at), {
                addSuffix: true,
                locale: ptBR,
              })
            : "Nunca visto";

          return (
            <Card key={device.id}>
              <CardHeader>
                <div className="flex items-start justify-between">
                  <div className="space-y-1">
                    <CardTitle className="text-base flex items-center gap-2">
                      <Cpu className="h-4 w-4" />
                      {device.device_uuid}
                    </CardTitle>
                    <CardDescription>
                      Firmware v{device.firmware_version}
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
              <CardContent className="space-y-2">
                <div className="flex items-center justify-between text-sm">
                  <span className="text-muted-foreground">Tipo:</span>
                  <Badge variant="outline">
                    {device.device_type === "sensor"
                      ? "Módulo de Sensores"
                      : "Módulo de Atuadores"}
                  </Badge>
                </div>
                <div className="flex items-center justify-between text-sm">
                  <span className="text-muted-foreground">Última atividade:</span>
                  <span className="font-medium">{lastSeenText}</span>
                </div>
                <div className="flex items-center justify-between text-sm">
                  <span className="text-muted-foreground">Conectado em:</span>
                  <span className="font-medium">
                    {new Date(device.first_seen_at).toLocaleDateString("pt-BR")}
                  </span>
                </div>
              </CardContent>
            </Card>
          );
        })}
      </div>
    </div>
  );
};