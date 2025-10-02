import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Power, PowerOff } from "lucide-react";
import { useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";

interface RelayCardProps {
  relayIndex: number;
  name: string;
  mode: string;
  isOn: boolean;
}

export const RelayCard = ({ relayIndex, name, mode, isOn }: RelayCardProps) => {
  const [isLoading, setIsLoading] = useState(false);
  const { toast } = useToast();

  const handleToggle = async () => {
    if (mode !== 'manual') {
      toast({
        title: "Modo incorreto",
        description: "O relé deve estar em modo manual para controle direto",
        variant: "destructive"
      });
      return;
    }

    setIsLoading(true);

    try {
      const { error } = await supabase.functions.invoke('relay-control', {
        body: {
          relay_index: relayIndex,
          command: !isOn
        }
      });

      if (error) throw error;

      toast({
        title: "Comando enviado",
        description: `Relé ${relayIndex} - ${!isOn ? 'LIGAR' : 'DESLIGAR'}`
      });
    } catch (error) {
      console.error('Erro ao enviar comando:', error);
      toast({
        title: "Erro",
        description: "Falha ao enviar comando",
        variant: "destructive"
      });
    } finally {
      setIsLoading(false);
    }
  };

  const getModeLabel = (mode: string) => {
    const labels: { [key: string]: string } = {
      unused: 'Não usado',
      manual: 'Manual',
      ph_control: 'Controle de pH',
      temp_control: 'Controle de Temp.',
      humidity_control: 'Controle de Umidade',
      led_schedule: 'LED Programado',
      ec_pulse: 'Pulso EC',
      cycle_timer: 'Timer Cíclico'
    };
    return labels[mode] || mode;
  };

  return (
    <Card className="border-primary/20 hover:border-primary/40 transition-all">
      <CardHeader className="pb-3">
        <div className="flex items-center justify-between">
          <CardTitle className="text-sm font-medium">
            Relé {relayIndex}: {name}
          </CardTitle>
          <Badge 
            variant={isOn ? "default" : "secondary"}
            className={isOn ? "bg-green-500" : "bg-gray-500"}
          >
            {isOn ? "LIGADO" : "DESLIGADO"}
          </Badge>
        </div>
      </CardHeader>
      <CardContent>
        <div className="space-y-3">
          <div className="text-sm text-muted-foreground">
            Modo: {getModeLabel(mode)}
          </div>
          
          {mode === 'manual' && (
            <Button
              onClick={handleToggle}
              disabled={isLoading}
              className="w-full gap-2"
              variant={isOn ? "destructive" : "default"}
            >
              {isOn ? (
                <>
                  <PowerOff className="h-4 w-4" />
                  Desligar
                </>
              ) : (
                <>
                  <Power className="h-4 w-4" />
                  Ligar
                </>
              )}
            </Button>
          )}

          {mode !== 'manual' && mode !== 'unused' && (
            <div className="flex items-center gap-2 text-sm text-muted-foreground">
              <div className={`w-2 h-2 rounded-full ${isOn ? 'bg-green-500 animate-pulse' : 'bg-gray-400'}`} />
              {isOn ? 'Automação ativa' : 'Automação inativa'}
            </div>
          )}
        </div>
      </CardContent>
    </Card>
  );
};
