import { useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Wifi, WifiOff, RefreshCw, Loader2 } from "lucide-react";
import { useMqttContext } from "@/contexts/MqttContext";
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from "@/components/ui/tooltip";

export const MqttFooter = () => {
  const { isConnected, lastMessage, connectionState, connect } = useMqttContext();
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);
  const [isReconnecting, setIsReconnecting] = useState(false);

  useEffect(() => {
    if (lastMessage) {
      setLastUpdate(lastMessage.timestamp);
    }
  }, [lastMessage]);

  const handleReconnect = () => {
    setIsReconnecting(true);
    connect();
    setTimeout(() => setIsReconnecting(false), 3000);
  };

  const getStatusColor = () => {
    if (isConnected) return "text-green-500";
    if (connectionState.attempts > 0) return "text-yellow-500";
    return "text-destructive";
  };

  const getStatusIcon = () => {
    if (isReconnecting || (connectionState.attempts > 0 && !isConnected)) {
      return <Loader2 className="h-4 w-4 animate-spin" />;
    }
    return isConnected ? <Wifi className="h-4 w-4" /> : <WifiOff className="h-4 w-4" />;
  };

  const getStatusText = () => {
    if (isConnected) return "Conectado";
    if (connectionState.attempts > 0) {
      return `Reconectando (${connectionState.attempts}/10)`;
    }
    return "Desconectado";
  };

  return (
    <footer className="fixed bottom-0 left-0 right-0 bg-card/95 backdrop-blur-md border-t border-border z-40 px-4 py-2">
      <div className="container mx-auto flex items-center justify-between">
        <div className={`flex items-center gap-2 text-sm ${getStatusColor()}`}>
          {getStatusIcon()}
          <span className="font-medium">{getStatusText()}</span>
          {lastUpdate && isConnected && (
            <span className="text-xs text-muted-foreground ml-2">
              {lastUpdate.toLocaleTimeString("pt-BR")}
            </span>
          )}
          {!isConnected && connectionState.attempts > 0 && (
            <span className="text-xs text-muted-foreground ml-2">
              Próxima em {Math.round(connectionState.nextRetryIn / 1000)}s
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
                disabled={isConnected || isReconnecting}
                className="gap-2 h-8"
              >
                <RefreshCw className={`h-3.5 w-3.5 ${isReconnecting ? 'animate-spin' : ''}`} />
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
