import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Badge } from "@/components/ui/badge";
import { Wifi, WifiOff } from "lucide-react";

export const MqttStatus = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);

  useEffect(() => {
    const checkConnection = async () => {
      const { data, error } = await supabase
        .from("readings")
        .select("timestamp")
        .order("timestamp", { ascending: false })
        .limit(1)
        .maybeSingle();

      if (data && !error) {
        const lastReading = new Date(data.timestamp);
        const now = new Date();
        const diffMinutes = (now.getTime() - lastReading.getTime()) / 1000 / 60;
        
        setIsConnected(diffMinutes < 5);
        setLastUpdate(lastReading);
      }
    };

    checkConnection();
    const interval = setInterval(checkConnection, 30000);

    return () => clearInterval(interval);
  }, []);

  return (
    <Badge
      variant={isConnected ? "default" : "destructive"}
      className="gap-2"
    >
      {isConnected ? (
        <>
          <Wifi className="h-3 w-3" />
          MQTT Conectado
        </>
      ) : (
        <>
          <WifiOff className="h-3 w-3" />
          MQTT Desconectado
        </>
      )}
      {lastUpdate && (
        <span className="text-xs">
          {lastUpdate.toLocaleTimeString("pt-BR")}
        </span>
      )}
    </Badge>
  );
};
