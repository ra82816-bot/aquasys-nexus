import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Input } from "@/components/ui/input";
import { Power, PowerOff, Pencil, Check, X } from "lucide-react";
import { useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";

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
    console.log('RelayCard: Enviando comando para relé', { relayIndex, command: !isOn });

    try {
      const { data, error } = await supabase.functions.invoke('relay-control', {
        body: {
          relay_index: relayIndex,
          command: !isOn
        }
      });

      console.log('RelayCard: Resposta recebida', { data, error });

      if (error) throw error;

      toast({
        title: "Comando enviado",
        description: `Relé ${relayIndex + 1} - ${!isOn ? 'LIGAR' : 'DESLIGAR'}`
      });
    } catch (error) {
      console.error('RelayCard: Erro ao enviar comando:', error);
      toast({
        title: "Erro",
        description: "Falha ao enviar comando",
        variant: "destructive"
      });
    } finally {
      setIsLoading(false);
    }
  };

  const handleBadgeClick = async () => {
    if (mode !== 'manual') {
      toast({
        title: "Modo incorreto",
        description: "O relé deve estar em modo manual para controle direto",
        variant: "destructive"
      });
      return;
    }
    await handleToggle();
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
            variant={isOn ? "default" : "secondary"}
            className={`${isOn ? "bg-green-500" : "bg-gray-500"} ${mode === 'manual' ? 'cursor-pointer hover:opacity-80' : ''}`}
            onClick={mode === 'manual' ? handleBadgeClick : undefined}
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
