import { useState, useEffect } from "react";
import { supabase } from "@/integrations/supabase/client";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Label } from "@/components/ui/label";
import { Input } from "@/components/ui/input";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { useToast } from "@/hooks/use-toast";

interface RelayConfigDialogProps {
  relayIndex: number;
  config: any;
  open: boolean;
  onOpenChange: (open: boolean) => void;
  onSave: () => void;
}

export const RelayConfigDialog = ({
  relayIndex,
  config,
  open,
  onOpenChange,
  onSave
}: RelayConfigDialogProps) => {
  const [mode, setMode] = useState(config?.mode || "unused");
  const [formData, setFormData] = useState<Record<string, any>>({});
  const [isSaving, setIsSaving] = useState(false);
  const { toast } = useToast();

  useEffect(() => {
    if (config) {
      setMode(config.mode);
      setFormData(config);
    }
  }, [config]);

  const handleSave = async () => {
    setIsSaving(true);
    try {
      const updateData = {
        mode,
        ...formData,
        updated_at: new Date().toISOString()
      };

      console.log('Salvando configuração do relé:', { relayIndex, updateData });

      const { data, error } = await supabase
        .from("relay_configs")
        .update(updateData)
        .eq("relay_index", relayIndex)
        .select()
        .single();

      if (error) throw error;

      console.log('Configuração salva com sucesso:', data);

      toast({
        title: "Sucesso",
        description: "Configuração salva com sucesso!"
      });

      await onSave();
      onOpenChange(false);
    } catch (error: any) {
      console.error('Erro ao salvar configuração:', error);
      toast({
        title: "Erro",
        description: error.message,
        variant: "destructive"
      });
    } finally {
      setIsSaving(false);
    }
  };

  const renderModeFields = () => {
    switch (mode) {
      case "led":
        return (
          <>
            <div className="space-y-2">
              <Label>Hora de Ligar (0-23)</Label>
              <Input
                type="number"
                min="0"
                max="23"
                value={formData.led_on_hour || ""}
                onChange={(e) => setFormData({ ...formData, led_on_hour: parseInt(e.target.value) })}
              />
            </div>
            <div className="space-y-2">
              <Label>Hora de Desligar (0-23)</Label>
              <Input
                type="number"
                min="0"
                max="23"
                value={formData.led_off_hour || ""}
                onChange={(e) => setFormData({ ...formData, led_off_hour: parseInt(e.target.value) })}
              />
            </div>
          </>
        );
      case "cycle":
        return (
          <>
            <div className="space-y-2">
              <Label>Tempo Ligado (minutos)</Label>
              <Input
                type="number"
                min="1"
                value={formData.cycle_on_min || ""}
                onChange={(e) => setFormData({ ...formData, cycle_on_min: parseInt(e.target.value) })}
              />
            </div>
            <div className="space-y-2">
              <Label>Tempo Desligado (minutos)</Label>
              <Input
                type="number"
                min="1"
                value={formData.cycle_off_min || ""}
                onChange={(e) => setFormData({ ...formData, cycle_off_min: parseInt(e.target.value) })}
              />
            </div>
          </>
        );
      case "ph_up":
        return (
          <>
            <div className="space-y-2">
              <Label>pH Mínimo</Label>
              <Input
                type="number"
                step="0.1"
                min="0"
                max="14"
                value={formData.ph_threshold_low || ""}
                onChange={(e) => setFormData({ ...formData, ph_threshold_low: parseFloat(e.target.value) })}
              />
            </div>
            <div className="space-y-2">
              <Label>Duração do Pulso (segundos)</Label>
              <Input
                type="number"
                min="1"
                value={formData.ph_pulse_sec || ""}
                onChange={(e) => setFormData({ ...formData, ph_pulse_sec: parseInt(e.target.value) })}
              />
            </div>
          </>
        );
      case "ph_down":
        return (
          <>
            <div className="space-y-2">
              <Label>pH Máximo</Label>
              <Input
                type="number"
                step="0.1"
                min="0"
                max="14"
                value={formData.ph_threshold_high || ""}
                onChange={(e) => setFormData({ ...formData, ph_threshold_high: parseFloat(e.target.value) })}
              />
            </div>
            <div className="space-y-2">
              <Label>Duração do Pulso (segundos)</Label>
              <Input
                type="number"
                min="1"
                value={formData.ph_pulse_sec || ""}
                onChange={(e) => setFormData({ ...formData, ph_pulse_sec: parseInt(e.target.value) })}
              />
            </div>
          </>
        );
      case "temperature":
        return (
          <>
            <div className="space-y-2">
              <Label>Temperatura Máxima (°C)</Label>
              <Input
                type="number"
                step="0.1"
                value={formData.temp_threshold_on || ""}
                onChange={(e) => setFormData({ ...formData, temp_threshold_on: parseFloat(e.target.value) })}
              />
            </div>
            <div className="space-y-2">
              <Label>Temperatura Mínima (°C)</Label>
              <Input
                type="number"
                step="0.1"
                value={formData.temp_threshold_off || ""}
                onChange={(e) => setFormData({ ...formData, temp_threshold_off: parseFloat(e.target.value) })}
              />
            </div>
          </>
        );
      case "humidity":
        return (
          <>
            <div className="space-y-2">
              <Label>Umidade Máxima (%)</Label>
              <Input
                type="number"
                step="0.1"
                min="0"
                max="100"
                value={formData.humidity_threshold_on || ""}
                onChange={(e) => setFormData({ ...formData, humidity_threshold_on: parseFloat(e.target.value) })}
              />
            </div>
            <div className="space-y-2">
              <Label>Umidade Mínima (%)</Label>
              <Input
                type="number"
                step="0.1"
                min="0"
                max="100"
                value={formData.humidity_threshold_off || ""}
                onChange={(e) => setFormData({ ...formData, humidity_threshold_off: parseFloat(e.target.value) })}
              />
            </div>
          </>
        );
      case "ec":
        return (
          <>
            <div className="space-y-2">
              <Label>EC Mínimo (µS/cm)</Label>
              <Input
                type="number"
                step="0.1"
                value={formData.ec_threshold || ""}
                onChange={(e) => setFormData({ ...formData, ec_threshold: parseFloat(e.target.value) })}
              />
            </div>
            <div className="space-y-2">
              <Label>Duração do Pulso (segundos)</Label>
              <Input
                type="number"
                min="1"
                value={formData.ec_pulse_sec || ""}
                onChange={(e) => setFormData({ ...formData, ec_pulse_sec: parseInt(e.target.value) })}
              />
            </div>
          </>
        );
      case "manual":
        return <p className="text-muted-foreground">Modo manual: use os botões Ligar/Desligar no card do relé.</p>;
      default:
        return <p className="text-muted-foreground">Selecione um modo para configurar.</p>;
    }
  };

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-md">
        <DialogHeader>
          <DialogTitle>Configurar Relé {relayIndex + 1}</DialogTitle>
          <DialogDescription>
            Selecione o modo de operação e configure os parâmetros
          </DialogDescription>
        </DialogHeader>
        <div className="space-y-4">
          <div className="space-y-2">
            <Label>Modo de Operação</Label>
            <Select value={mode} onValueChange={setMode}>
              <SelectTrigger>
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="unused">Não Usado</SelectItem>
                <SelectItem value="manual">Manual</SelectItem>
                <SelectItem value="led">Iluminação</SelectItem>
                <SelectItem value="cycle">Ciclo</SelectItem>
                <SelectItem value="ph_up">pH Up</SelectItem>
                <SelectItem value="ph_down">pH Down</SelectItem>
                <SelectItem value="temperature">Temperatura</SelectItem>
                <SelectItem value="humidity">Umidade</SelectItem>
                <SelectItem value="ec">Condutividade</SelectItem>
              </SelectContent>
            </Select>
          </div>
          {renderModeFields()}
          <div className="flex gap-2 pt-4">
            <Button onClick={handleSave} className="flex-1" disabled={isSaving}>
              {isSaving ? "Salvando..." : "Salvar"}
            </Button>
            <Button onClick={() => onOpenChange(false)} variant="outline" className="flex-1">
              Cancelar
            </Button>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  );
};
