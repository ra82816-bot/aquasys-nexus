import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Input } from "@/components/ui/input";
import { Power, PowerOff, Pencil, Check, X, RefreshCcw } from "lucide-react";
import { useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";
import { useMqttContext } from "@/contexts/MqttContext";

interface RelayCardProps {
  relayIndex: number;
  name: string;
  mode: string;
  isOn: boolean;
  onNameUpdate?: () => void;
}

export const RelayCard = ({ relayIndex, name, mode, isOn, onNameUpdate }: RelayCardProps) => {
  const [isLoading, setIsLoading] = useState(false);
  const [isEditingName, setIsEditingName] = useState(false);
  const [newName, setNewName] = useState(name);
  const { toast } = useToast();
  const { publish, isConnected } = useMqttContext();

  const handleToggle = async () => {
    if (!isConnected) {
      toast({
        title: "MQTT Desconectado",
        description: "Aguarde a conexão MQTT ser restabelecida",
        variant: "destructive"
      });
      return;
    }

    setIsLoading(true);

    try {
      // Formato do comando conforme o firmware ESP32
      const command = {
        command: "manual_override",
        payload: {
          relay: relayIndex + 1, // Firmware usa 1-8
          state: !isOn ? "on" : "off"
        }
      };
      
      await publish('aquasys/relay/command', command);
      
      toast({
        title: "Comando enviado",
        description: `Relé ${relayIndex + 1} alterado para ${!isOn ? 'LIGADO' : 'DESLIGADO'}`,
      });
    } catch (error) {
      console.error('Erro ao enviar comando:', error);
      toast({
        title: "Erro",
        description: "Falha ao enviar comando ao relé",
        variant: "destructive"
      });
    } finally {
      setTimeout(() => setIsLoading(false), 500);
    }
  };

  const handleSetAuto = async () => {
    if (!isConnected) {
      toast({
        title: "MQTT Desconectado",
        description: "Aguarde a conexão MQTT ser restabelecida",
        variant: "destructive"
      });
      return;
    }

    try {
      const command = {
        command: "set_auto",
        payload: {
          relay: relayIndex + 1
        }
      };
      
      await publish('aquasys/relay/command', command);
      
      toast({
        title: "Modo alterado",
        description: `Relé ${relayIndex + 1} retornado ao modo AUTOMÁTICO`,
      });
    } catch (error) {
      console.error('Erro ao definir modo auto:', error);
      toast({
        title: "Erro",
        description: "Falha ao alterar modo do relé",
        variant: "destructive"
      });
    }
  };

  const handleSaveName = async () => {
    if (!newName.trim()) {
      toast({
        title: "Erro",
        description: "O nome não pode estar vazio",
        variant: "destructive"
      });
      return;
    }

    try {
      const { error } = await supabase
        .from("relay_configs")
        .update({ name: newName })
        .eq("relay_index", relayIndex);

      if (error) throw error;

      toast({
        title: "Sucesso",
        description: "Nome do relé atualizado"
      });
      setIsEditingName(false);
      onNameUpdate?.();
    } catch (error) {
      console.error("Erro ao atualizar nome:", error);
      toast({
        title: "Erro",
        description: "Falha ao atualizar nome",
        variant: "destructive"
      });
    }
  };

  const getModeLabel = (mode: string) => {
    const labels: { [key: string]: string } = {
      unused: 'Não usado',
      manual: 'Manual',
      ph_up: 'pH (Subir)',
      ph_down: 'pH (Baixar)',
      temperature: 'Temperatura',
      humidity: 'Umidade',
      led: 'LED',
      ec: 'EC',
      cycle: 'Ciclo',
      co2: 'CO2'
    };
    return labels[mode] || mode;
  };

  // Cor do badge baseada no estado e conexão
  const getBadgeColor = () => {
    if (!isConnected) return 'bg-yellow-500/20 text-yellow-600 border-yellow-500/30';
    if (isOn) return 'bg-green-500/20 text-green-600 border-green-500/30';
    return 'bg-muted text-muted-foreground border-border';
  };

  return (
    <Card className="border-primary/20 hover:border-primary/40 transition-all">
      <CardHeader className="pb-3">
        <div className="flex items-center justify-between">
          {isEditingName ? (
            <div className="flex items-center gap-2 flex-1">
              <Input
                value={newName}
                onChange={(e) => setNewName(e.target.value)}
                className="h-8"
                autoFocus
              />
              <Button size="sm" variant="ghost" onClick={handleSaveName}>
                <Check className="h-4 w-4" />
              </Button>
              <Button size="sm" variant="ghost" onClick={() => {
                setNewName(name);
                setIsEditingName(false);
              }}>
                <X className="h-4 w-4" />
              </Button>
            </div>
          ) : (
            <CardTitle className="text-sm font-medium flex items-center gap-2">
              Relé {relayIndex + 1}: {name}
              <Button 
                size="sm" 
                variant="ghost" 
                className="h-6 w-6 p-0"
                onClick={() => setIsEditingName(true)}
              >
                <Pencil className="h-3 w-3" />
              </Button>
            </CardTitle>
          )}
          <Badge 
            variant="outline"
            className={`border ${getBadgeColor()}`}
          >
            {!isConnected ? 'AGUARDANDO' : isOn ? "LIGADO" : "DESLIGADO"}
          </Badge>
        </div>
      </CardHeader>
      <CardContent>
        <div className="space-y-3">
          <div className="text-sm text-muted-foreground flex items-center justify-between">
            <span>Modo: {getModeLabel(mode)}</span>
            {mode !== 'manual' && mode !== 'unused' && (
              <div className={`w-2 h-2 rounded-full ${isOn ? 'bg-green-500 animate-pulse' : 'bg-gray-400'}`} />
            )}
          </div>

          {mode === 'unused' && (
            <div className="text-xs text-amber-500 bg-amber-500/10 p-2 rounded">
              ⚙️ Configure este relé clicando no ícone de engrenagem acima
            </div>
          )}
          
          <div className="flex gap-2">
            <Button
              onClick={handleToggle}
              disabled={isLoading || !isConnected}
              className="flex-1 gap-2"
              variant={isOn ? "destructive" : "default"}
            >
              {isLoading ? (
                <span className="flex items-center gap-2">
                  <div className="h-4 w-4 border-2 border-current border-t-transparent rounded-full animate-spin" />
                  Enviando...
                </span>
              ) : (
                <>
                  {isOn ? <PowerOff className="h-4 w-4" /> : <Power className="h-4 w-4" />}
                  {isOn ? 'Desligar' : 'Ligar'}
                </>
              )}
            </Button>
            
            <Button
              onClick={handleSetAuto}
              disabled={isLoading || !isConnected}
              variant="outline"
              size="sm"
              className="px-3"
              title="Retornar ao modo automático"
            >
              <RefreshCcw className="h-4 w-4" />
            </Button>
          </div>

          {!isConnected && (
            <div className="text-xs text-yellow-600 bg-yellow-500/10 p-2 rounded flex items-center justify-center gap-2">
              <div className="h-2 w-2 rounded-full bg-yellow-500 animate-pulse" />
              Aguardando conexão MQTT
            </div>
          )}
        </div>
      </CardContent>
    </Card>
  );
};
