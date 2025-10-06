import { useEffect, useState } from "react";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Wifi, WifiOff, RefreshCw } from "lucide-react";
import { useMqttContext } from "@/contexts/MqttContext";

type ConnectionStatus = "connected" | "warning" | "disconnected";

export const MqttStatus = () => {
  const { isConnected, lastMessage, connect } = useMqttContext();
  const [status, setStatus] = useState<ConnectionStatus>("disconnected");
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);

  useEffect(() => {
    if (isConnected) {
      setStatus("connected");
    } else {
      setStatus("disconnected");
    }
  }, [isConnected]);

  useEffect(() => {
    if (lastMessage) {
      setLastUpdate(lastMessage.timestamp);
    }
  }, [lastMessage]);

  const handleReconnect = () => {
    connect();
  };

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
        onClick={handleReconnect}
        disabled={isConnected}
        className="gap-2"
      >
        <RefreshCw className="h-3 w-3" />
        Reconectar
      </Button>
    </div>
  );
};
