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
import { useMqttContext } from "@/contexts/MqttContext";

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
  const { publishRelayConfig, isConnected } = useMqttContext();

  useEffect(() => {
    if (config) {
      setMode(config.mode);
      setFormData(config);
    }
  }, [config]);

  const prepareMqttConfig = (mode: string, formData: Record<string, any>) => {
    const modeMap: Record<string, number> = {
      'unused': 0,
      'led': 1,
      'cycle': 2,
      'ph_up': 3,
      'temperature': 4,
      'humidity': 5,
      'ec': 6,
      'co2': 7,
      'ph_down': 8,
      'manual': 0
    };

    return {
      mode: modeMap[mode] || 0,
      led_on_hour: formData.led_on_hour || 6,
      led_off_hour: formData.led_off_hour || 0,
      cycle_on_min: formData.cycle_on_min || 15,
      cycle_off_min: formData.cycle_off_min || 15,
      ph_pulse_sec: formData.ph_pulse_sec || 5,
      ph_threshold_low: formData.ph_threshold_low || 5.8,
      ph_threshold_high: formData.ph_threshold_high || 6.5,
      temp_threshold_on: formData.temp_threshold_on || 28.0,
      temp_threshold_off: formData.temp_threshold_off || 26.0,
      humidity_threshold_on: formData.humidity_threshold_on || 75.0,
      humidity_threshold_off: formData.humidity_threshold_off || 65.0,
      ec_threshold: formData.ec_threshold || 1200.0,
      ec_pulse_sec: formData.ec_pulse_sec || 5
    };
  };

  const handleSave = async () => {
    setIsSaving(true);
    try {
      const updateData = {
        mode,
        ...formData,
        updated_at: new Date().toISOString()
      };

      console.log('Salvando configuração do relé:', { relayIndex, updateData });

      // Salvar no banco
      const { data, error } = await supabase
        .from("relay_configs")
        .update(updateData)
        .eq("relay_index", relayIndex)
        .select()
        .single();

      if (error) throw error;

      console.log('Configuração salva no banco:', data);

      // Enviar via MQTT para o ESP32
      if (isConnected) {
        const mqttConfig = prepareMqttConfig(mode, formData);
        await publishRelayConfig(relayIndex, mqttConfig);
        console.log('Configuração enviada via MQTT:', mqttConfig);
      } else {
        toast({
          title: "Aviso",
          description: "Configuração salva, mas MQTT desconectado. Será enviada ao ESP32 quando reconectar.",
          variant: "default"
        });
      }

      toast({
        title: "Sucesso",
        description: "Configuração salva e enviada ao ESP32!"
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
