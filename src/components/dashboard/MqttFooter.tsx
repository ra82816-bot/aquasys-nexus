import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Wifi, WifiOff, RefreshCw } from "lucide-react";
import { useMqttContext } from "@/contexts/MqttContext";
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from "@/components/ui/tooltip";

type ConnectionStatus = "connected" | "disconnected";

export const MqttFooter = () => {
  const { isConnected, lastMessage, connect } = useMqttContext();
  const [status, setStatus] = useState<ConnectionStatus>("disconnected");
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);

  useEffect(() => {
    setStatus(isConnected ? "connected" : "disconnected");
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
    return status === "connected" ? "text-green-500" : "text-destructive";
  };

  const getStatusIcon = () => {
    return status === "connected" ? (
      <Wifi className="h-4 w-4" />
    ) : (
      <WifiOff className="h-4 w-4" />
    );
  };

  const getStatusText = () => {
    return status === "connected" ? "Conectado" : "Desconectado";
  };

  return (
    <footer className="fixed bottom-0 left-0 right-0 bg-card/95 backdrop-blur-md border-t border-border z-40 px-4 py-2">
      <div className="container mx-auto flex items-center justify-between">
        <div className={`flex items-center gap-2 text-sm ${getStatusColor()}`}>
          {getStatusIcon()}
          <span className="font-medium">{getStatusText()}</span>
          {lastUpdate && (
            <span className="text-xs text-muted-foreground ml-2">
              {lastUpdate.toLocaleTimeString("pt-BR")}
            </span>
          )}
        </div>
        
        <TooltipProvider>
          <Tooltip>
            <TooltipTrigger asChild>
              <Button
                variant="ghost"
                size="sm"
                onClick={handleReconnect}
                disabled={isConnected}
                className="gap-2 h-8"
              >
                <RefreshCw className="h-3.5 w-3.5" />
                <span className="hidden sm:inline">Reconectar</span>
              </Button>
            </TooltipTrigger>
            <TooltipContent>
              <p>Reconectar ao servidor MQTT</p>
            </TooltipContent>
          </Tooltip>
        </TooltipProvider>
      </div>
    </footer>
  );
};
