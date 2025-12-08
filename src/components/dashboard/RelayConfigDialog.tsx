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
  const { publishRelayConfig, setRelayAuto, isConnected } = useMqttContext();

  // Inicializa formData quando config muda
  useEffect(() => {
    if (config) {
      setMode(config.mode || "unused");
      setFormData({
        led_on_hour: config.led_on_hour ?? 6,
        led_off_hour: config.led_off_hour ?? 0,
        cycle_on_min: config.cycle_on_min ?? 15,
        cycle_off_min: config.cycle_off_min ?? 15,
        ph_pulse_sec: config.ph_pulse_sec ?? 5,
        ph_threshold_low: config.ph_threshold_low ?? 5.8,
        ph_threshold_high: config.ph_threshold_high ?? 6.5,
        temp_threshold_on: config.temp_threshold_on ?? 28.0,
        temp_threshold_off: config.temp_threshold_off ?? 26.0,
        humidity_threshold_on: config.humidity_threshold_on ?? 75.0,
        humidity_threshold_off: config.humidity_threshold_off ?? 65.0,
        ec_threshold: config.ec_threshold ?? 1.2,
        ec_pulse_sec: config.ec_pulse_sec ?? 5,
      });
    }
  }, [config]);

  // Helper para lidar com inputs numéricos
  const handleNumberChange = (field: string, value: string, isFloat = false) => {
    if (value === '') {
      setFormData({ ...formData, [field]: null });
    } else {
      const parsed = isFloat ? parseFloat(value) : parseInt(value, 10);
      if (!isNaN(parsed)) {
        setFormData({ ...formData, [field]: parsed });
      }
    }
  };

  // Retorna valor para exibição no input (trata null/undefined vs 0)
  const getInputValue = (field: string): string => {
    const val = formData[field];
    if (val === null || val === undefined) return '';
    return String(val);
  };

  const prepareMqttConfig = (mode: string, data: Record<string, any>) => {
    // Mapeamento de modos do app para firmware
    // IMPORTANTE: Os valores devem corresponder ao enum RelayMode no firmware ESP32
    // MODE_MANUAL não existe no firmware, então usa MODE_UNUSED (0) - a diferença é que
    // não chamamos setRelayAuto() depois, mantendo manual_override = true
    const modeMap: Record<string, number> = {
      'unused': 0,      // MODE_UNUSED - não responde a comandos
      'manual': 0,      // Tratado como unused no firmware, mas sem resetar manual_override
      'led': 1,         // MODE_LED - schedule baseado em horário
      'cycle': 2,       // MODE_CYCLE - alternância ligado/desligado
      'ph_up': 3,       // MODE_PH_UP - pulso quando pH baixo
      'temperature': 4, // MODE_TEMPERATURE - controle por histerese
      'humidity': 5,    // MODE_HUMIDITY - controle por histerese
      'ec': 6,          // MODE_EC - pulso quando EC baixo
      'co2': 7,         // MODE_CO2 - não implementado no firmware
      'ph_down': 8      // MODE_PH_DOWN - pulso quando pH alto
    };

    // LED schedule: firmware espera led_off_hour > led_on_hour para schedule overnight
    // Se led_off_hour for 0 (meia-noite), converter para 24
    let ledOnHour = data.led_on_hour ?? 6;
    let ledOffHour = data.led_off_hour ?? 0;
    if (ledOffHour === 0 && ledOnHour > 0) {
      ledOffHour = 24; // Meia-noite como 24 para firmware
    }

    // EC: App usa mS/cm, firmware usa µS/cm (multiplicar por 1000)
    const ecThresholdMicroS = (data.ec_threshold ?? 1.2) * 1000;

    // Cycle: garantir valores mínimos
    const cycleOnMin = Math.max(1, data.cycle_on_min ?? 15);
    const cycleOffMin = Math.max(1, data.cycle_off_min ?? 15);

    const mqttConfig = {
      mode: modeMap[mode] ?? 0,
      led_on_hour: ledOnHour,
      led_off_hour: ledOffHour,
      cycle_on_min: cycleOnMin,
      cycle_off_min: cycleOffMin,
      ph_pulse_sec: data.ph_pulse_sec ?? 5,
      ph_threshold_low: data.ph_threshold_low ?? 5.8,
      ph_threshold_high: data.ph_threshold_high ?? 6.5,
      temp_threshold_on: data.temp_threshold_on ?? 28.0,
      temp_threshold_off: data.temp_threshold_off ?? 26.0,
      humidity_threshold_on: data.humidity_threshold_on ?? 75.0,
      humidity_threshold_off: data.humidity_threshold_off ?? 65.0,
      ec_threshold: ecThresholdMicroS,
      ec_pulse_sec: data.ec_pulse_sec ?? 5
    };

    console.log('🔧 prepareMqttConfig - Input:', { 
      inputMode: mode, 
      formData: data 
    });
    console.log('🔧 prepareMqttConfig - Output:', { 
      mappedMode: mqttConfig.mode, 
      ledSchedule: `${mqttConfig.led_on_hour}h → ${mqttConfig.led_off_hour}h`,
      cycleSchedule: `${mqttConfig.cycle_on_min}min ON / ${mqttConfig.cycle_off_min}min OFF`,
      ecThreshold: `${ecThresholdMicroS} µS/cm`
    });

    return mqttConfig;
  };

  const handleSave = async () => {
    setIsSaving(true);
    try {
      // Prepara dados para salvar - inclui valores 0 mas remove null/undefined/NaN
      const updateData: Record<string, any> = { mode, updated_at: new Date().toISOString() };
      
      // Campos numéricos relevantes para cada modo
      const numericFields = [
        'led_on_hour', 'led_off_hour', 'cycle_on_min', 'cycle_off_min',
        'ph_pulse_sec', 'ph_threshold_low', 'ph_threshold_high',
        'temp_threshold_on', 'temp_threshold_off',
        'humidity_threshold_on', 'humidity_threshold_off',
        'ec_threshold', 'ec_pulse_sec'
      ];
      
      numericFields.forEach(field => {
        const val = formData[field];
        // Inclui 0, exclui null/undefined/NaN
        if (val !== null && val !== undefined && !Number.isNaN(val)) {
          updateData[field] = val;
        }
      });

      console.log('⚙️ CONFIG DEBUG - Salvando configuração do relé:', { 
        relayIndex, 
        mode,
        formData,
        updateData 
      });

      // Salvar no banco
      const { data, error } = await supabase
        .from("relay_configs")
        .update(updateData)
        .eq("relay_index", relayIndex)
        .select()
        .single();

      if (error) throw error;

      console.log('✅ CONFIG DEBUG - Configuração salva no banco:', data);

      // Enviar via MQTT para o ESP32
      if (isConnected) {
        const mqttConfig = prepareMqttConfig(mode, formData);
        console.log('📤 CONFIG DEBUG - Enviando via MQTT:', {
          relayIndex,
          mqttConfig
        });
        await publishRelayConfig(relayIndex, mqttConfig);
        console.log('✅ CONFIG DEBUG - Configuração enviada via MQTT');
        
        // IMPORTANTE: Desativar manual_override no ESP32 para que o modo automático funcione
        if (mode !== 'manual') {
          await setRelayAuto(relayIndex);
          console.log('✅ CONFIG DEBUG - Modo automático ativado no ESP32');
        }
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
      console.error('❌ CONFIG DEBUG - Erro ao salvar configuração:', error);
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
                value={getInputValue('led_on_hour')}
                onChange={(e) => handleNumberChange('led_on_hour', e.target.value)}
              />
            </div>
            <div className="space-y-2">
              <Label>Hora de Desligar (0-23)</Label>
              <Input
                type="number"
                min="0"
                max="23"
                value={getInputValue('led_off_hour')}
                onChange={(e) => handleNumberChange('led_off_hour', e.target.value)}
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
                value={getInputValue('cycle_on_min')}
                onChange={(e) => handleNumberChange('cycle_on_min', e.target.value)}
              />
            </div>
            <div className="space-y-2">
              <Label>Tempo Desligado (minutos)</Label>
              <Input
                type="number"
                min="1"
                value={getInputValue('cycle_off_min')}
                onChange={(e) => handleNumberChange('cycle_off_min', e.target.value)}
              />
            </div>
          </>
        );
      case "ph_up":
        return (
          <>
            <div className="space-y-2">
              <Label>pH Mínimo (ativa quando pH menor)</Label>
              <Input
                type="number"
                step="0.1"
                min="0"
                max="14"
                value={getInputValue('ph_threshold_low')}
                onChange={(e) => handleNumberChange('ph_threshold_low', e.target.value, true)}
              />
            </div>
            <div className="space-y-2">
              <Label>Duração do Pulso (segundos)</Label>
              <Input
                type="number"
                min="1"
                value={getInputValue('ph_pulse_sec')}
                onChange={(e) => handleNumberChange('ph_pulse_sec', e.target.value)}
              />
            </div>
          </>
        );
      case "ph_down":
        return (
          <>
            <div className="space-y-2">
              <Label>pH Máximo (ativa quando pH maior)</Label>
              <Input
                type="number"
                step="0.1"
                min="0"
                max="14"
                value={getInputValue('ph_threshold_high')}
                onChange={(e) => handleNumberChange('ph_threshold_high', e.target.value, true)}
              />
            </div>
            <div className="space-y-2">
              <Label>Duração do Pulso (segundos)</Label>
              <Input
                type="number"
                min="1"
                value={getInputValue('ph_pulse_sec')}
                onChange={(e) => handleNumberChange('ph_pulse_sec', e.target.value)}
              />
            </div>
          </>
        );
      case "temperature":
        return (
          <>
            <div className="space-y-2">
              <Label>Ligar quando temperatura ≥ (°C)</Label>
              <Input
                type="number"
                step="0.1"
                value={getInputValue('temp_threshold_on')}
                onChange={(e) => handleNumberChange('temp_threshold_on', e.target.value, true)}
              />
            </div>
            <div className="space-y-2">
              <Label>Desligar quando temperatura ≤ (°C)</Label>
              <Input
                type="number"
                step="0.1"
                value={getInputValue('temp_threshold_off')}
                onChange={(e) => handleNumberChange('temp_threshold_off', e.target.value, true)}
              />
            </div>
          </>
        );
      case "humidity":
        return (
          <>
            <div className="space-y-2">
              <Label>Ligar quando umidade ≥ (%)</Label>
              <Input
                type="number"
                step="0.1"
                min="0"
                max="100"
                value={getInputValue('humidity_threshold_on')}
                onChange={(e) => handleNumberChange('humidity_threshold_on', e.target.value, true)}
              />
            </div>
            <div className="space-y-2">
              <Label>Desligar quando umidade ≤ (%)</Label>
              <Input
                type="number"
                step="0.1"
                min="0"
                max="100"
                value={getInputValue('humidity_threshold_off')}
                onChange={(e) => handleNumberChange('humidity_threshold_off', e.target.value, true)}
              />
            </div>
          </>
        );
      case "ec":
        return (
          <>
            <div className="space-y-2">
              <Label>EC Mínimo em mS/cm (ativa quando EC menor)</Label>
              <Input
                type="number"
                step="0.01"
                min="0"
                max="5"
                placeholder="ex: 1.2"
                value={getInputValue('ec_threshold')}
                onChange={(e) => handleNumberChange('ec_threshold', e.target.value, true)}
              />
              <p className="text-xs text-muted-foreground">Valores típicos: 0.8-2.0 mS/cm</p>
            </div>
            <div className="space-y-2">
              <Label>Duração do Pulso (segundos)</Label>
              <Input
                type="number"
                min="1"
                value={getInputValue('ec_pulse_sec')}
                onChange={(e) => handleNumberChange('ec_pulse_sec', e.target.value)}
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
