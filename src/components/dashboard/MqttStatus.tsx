import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Wifi, WifiOff, RefreshCw } from "lucide-react";
import { useToast } from "@/hooks/use-toast";

type ConnectionStatus = "connected" | "warning" | "disconnected";

export const MqttStatus = () => {
  const [status, setStatus] = useState<ConnectionStatus>("disconnected");
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);
  const [isPinging, setIsPinging] = useState(false);
  const { toast } = useToast();

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
      
      if (diffMinutes < 2) {
        setStatus("connected");
      } else if (diffMinutes < 5) {
        setStatus("warning");
      } else {
        setStatus("disconnected");
      }
      
      setLastUpdate(lastReading);
    } else {
      setStatus("disconnected");
    }
  };

  const handlePing = async () => {
    setIsPinging(true);
    try {
      const { error } = await supabase.functions.invoke("mqtt-ping");
      
      if (error) throw error;
      
      toast({
        title: "Ping enviado",
        description: "Aguardando resposta do ESP32...",
      });
      
      setTimeout(checkConnection, 3000);
    } catch (error) {
      toast({
        title: "Erro ao enviar ping",
        description: "Não foi possível conectar com o sistema",
        variant: "destructive",
      });
    } finally {
      setIsPinging(false);
    }
  };

  useEffect(() => {
    checkConnection();
    const interval = setInterval(checkConnection, 30000);
    return () => clearInterval(interval);
  }, []);

  const getStatusColor = () => {
    switch (status) {
      case "connected":
        return "default";
      case "warning":
        return "secondary";
      case "disconnected":
        return "destructive";
    }
  };

  const getStatusText = () => {
    switch (status) {
      case "connected":
        return "MQTT Conectado";
      case "warning":
        return "MQTT Instável";
      case "disconnected":
        return "MQTT Desconectado";
    }
  };

  const getStatusIcon = () => {
    if (status === "connected") {
      return <Wifi className="h-3 w-3" />;
    }
    return <WifiOff className="h-3 w-3" />;
  };

  return (
    <div className="flex items-center gap-2">
      <Badge variant={getStatusColor()} className="gap-2">
        {getStatusIcon()}
        {getStatusText()}
        {lastUpdate && (
          <span className="text-xs ml-1">
            {lastUpdate.toLocaleTimeString("pt-BR")}
          </span>
        )}
      </Badge>
      <Button
        variant="outline"
        size="sm"
        onClick={handlePing}
        disabled={isPinging}
        className="gap-2"
      >
        <RefreshCw className={`h-3 w-3 ${isPinging ? "animate-spin" : ""}`} />
        Forçar Atualização
      </Button>
    </div>
  );
};
