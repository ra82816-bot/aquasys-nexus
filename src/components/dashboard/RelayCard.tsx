import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Input } from "@/components/ui/input";
import { Power, PowerOff, Pencil, Check, X, RefreshCcw } from "lucide-react";
import { useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";
import { useMqttContext } from "@/contexts/MqttContext";

interface RelayConfig {
  relay_index: number;
  mode: string;
  name?: string | null;
  led_on_hour?: number | null;
  led_off_hour?: number | null;
  cycle_on_min?: number | null;
  cycle_off_min?: number | null;
  ph_pulse_sec?: number | null;
  ph_threshold_low?: number | null;
  ph_threshold_high?: number | null;
  temp_threshold_on?: number | null;
  temp_threshold_off?: number | null;
  humidity_threshold_on?: number | null;
  humidity_threshold_off?: number | null;
  ec_threshold?: number | null;
  ec_pulse_sec?: number | null;
}

interface RelayCardProps {
  relayIndex: number;
  name: string;
  mode: string;
  isOn: boolean;
  config?: RelayConfig;
  onNameUpdate?: () => void;
}

export const RelayCard = ({ relayIndex, name, mode, isOn, config, onNameUpdate }: RelayCardProps) => {
  const [isLoading, setIsLoading] = useState(false);
  const [isEditingName, setIsEditingName] = useState(false);
  const [newName, setNewName] = useState(name);
  const { toast } = useToast();
  const { publishRelayCommand, setRelayAuto, isConnected } = useMqttContext();

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
      const newState = !isOn;
      
      console.log('Enviando comando de toggle:', { relay: relayIndex + 1, command: newState });
      await publishRelayCommand(relayIndex, newState);
      
      toast({
        title: "Comando enviado",
        description: `Relé ${relayIndex + 1} ${newState ? 'LIGADO' : 'DESLIGADO'}`,
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
      await setRelayAuto(relayIndex);
      
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
      ph_up: 'pH Up',
      ph_down: 'pH Down',
      temperature: 'Temperatura',
      humidity: 'Umidade',
      led: 'Iluminação',
      ec: 'Condutividade',
      cycle: 'Ciclo',
      co2: 'CO2'
    };
    return labels[mode] || mode;
  };

  // Renderiza os detalhes do modo configurado
  const renderModeDetails = () => {
    if (!config) return null;

    switch (mode) {
      case 'led':
        if (config.led_on_hour != null && config.led_off_hour != null) {
          return (
            <span className="text-xs text-muted-foreground">
              {String(config.led_on_hour).padStart(2, '0')}:00 - {String(config.led_off_hour).padStart(2, '0')}:00
            </span>
          );
        }
        break;
      case 'cycle':
        if (config.cycle_on_min && config.cycle_off_min) {
          return (
            <span className="text-xs text-muted-foreground">
              {config.cycle_on_min}min ON / {config.cycle_off_min}min OFF
            </span>
          );
        }
        break;
      case 'ph_up':
        if (config.ph_threshold_low != null) {
          return (
            <span className="text-xs text-muted-foreground">
              pH &lt; {config.ph_threshold_low} → Pulso {config.ph_pulse_sec}s
            </span>
          );
        }
        break;
      case 'ph_down':
        if (config.ph_threshold_high != null) {
          return (
            <span className="text-xs text-muted-foreground">
              pH &gt; {config.ph_threshold_high} → Pulso {config.ph_pulse_sec}s
            </span>
          );
        }
        break;
      case 'temperature':
        if (config.temp_threshold_on != null && config.temp_threshold_off != null) {
          return (
            <span className="text-xs text-muted-foreground">
              {config.temp_threshold_off}°C - {config.temp_threshold_on}°C
            </span>
          );
        }
        break;
      case 'humidity':
        if (config.humidity_threshold_on != null && config.humidity_threshold_off != null) {
          return (
            <span className="text-xs text-muted-foreground">
              {config.humidity_threshold_off}% - {config.humidity_threshold_on}%
            </span>
          );
        }
        break;
      case 'ec':
        if (config.ec_threshold != null) {
          return (
            <span className="text-xs text-muted-foreground">
              EC &lt; {config.ec_threshold} → Pulso {config.ec_pulse_sec}s
            </span>
          );
        }
        break;
    }
    return null;
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
          <div className="text-sm text-muted-foreground">
            <div className="flex items-center justify-between">
              <span className="font-medium">Modo: {getModeLabel(mode)}</span>
              {mode !== 'manual' && mode !== 'unused' && (
                <div 
                  className={`w-2 h-2 rounded-full ${isOn ? 'bg-green-500 animate-pulse' : 'bg-gray-400'}`}
                  title={isOn ? 'Ativo' : 'Inativo'}
                />
              )}
            </div>
            {renderModeDetails()}
          </div>

          
          <div className="flex gap-2">
            <Button
              onClick={handleToggle}
              disabled={isLoading || !isConnected}
              className="flex-1 gap-2"
              variant={isOn ? "destructive" : "default"}
              title={isOn ? "Desligar relé" : "Ligar relé"}
            >
              {isLoading ? (
                <span className="flex items-center gap-2">
                  <div className="h-4 w-4 border-2 border-current border-t-transparent rounded-full animate-spin" />
                  Processando...
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
