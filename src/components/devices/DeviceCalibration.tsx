import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { useToast } from "@/hooks/use-toast";
import { supabase } from "@/integrations/supabase/client";
import { Loader2, FlaskConical } from "lucide-react";

interface Device {
  id: string;
  device_uuid: string;
  device_type: string;
}

export const DeviceCalibration = ({ devices }: { devices: Device[] }) => {
  const [selectedDevice, setSelectedDevice] = useState("");
  const [sensorType, setSensorType] = useState<"ph" | "ec">("ph");
  const [phCal4, setPhCal4] = useState("");
  const [phCal7, setPhCal7] = useState("");
  const [phCal10, setPhCal10] = useState("");
  const [ecFactor, setEcFactor] = useState("");
  const [profileName, setProfileName] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const { toast } = useToast();

  // Filtrar apenas sensores
  const sensorDevices = devices.filter(d => d.device_type === "sensor");

  const handleCalibrate = async () => {
    if (!selectedDevice) {
      toast({
        title: "Dispositivo não selecionado",
        description: "Selecione um módulo de sensores",
        variant: "destructive",
      });
      return;
    }

    const calibrationData: any = {};

    if (sensorType === "ph") {
      const cal4 = parseFloat(phCal4);
      const cal7 = parseFloat(phCal7);
      
      if (isNaN(cal4) || isNaN(cal7)) {
        toast({
          title: "Valores inválidos",
          description: "Insira as voltagens de pH 4 e pH 7",
          variant: "destructive",
        });
        return;
      }

      calibrationData.ph_cal4_voltage = cal4;
      calibrationData.ph_cal7_voltage = cal7;
      
      if (phCal10) {
        const cal10 = parseFloat(phCal10);
        if (!isNaN(cal10)) {
          calibrationData.ph_cal10_voltage = cal10;
        }
      }
    } else {
      const factor = parseFloat(ecFactor);
      
      if (isNaN(factor) || factor <= 0) {
        toast({
          title: "Valor inválido",
          description: "Insira um fator de calibração válido",
          variant: "destructive",
        });
        return;
      }

      calibrationData.ec_factor = factor;
    }

    setIsLoading(true);

    try {
      const device = devices.find(d => d.id === selectedDevice);
      
      const { data, error } = await supabase.functions.invoke("device-calibration", {
        body: {
          device_uuid: device?.device_uuid,
          sensor_type: sensorType,
          calibration_data: calibrationData,
          profile_name: profileName || undefined,
        },
      });

      if (error) throw error;

      if (data.error) {
        throw new Error(data.error);
      }

      toast({
        title: "Calibração aplicada!",
        description: `Perfil "${data.profile_name}" criado e enviado ao dispositivo`,
      });

      // Limpar form
      setPhCal4("");
      setPhCal7("");
      setPhCal10("");
      setEcFactor("");
      setProfileName("");

    } catch (error: any) {
      console.error("Erro na calibração:", error);
      toast({
        title: "Erro na calibração",
        description: error.message || "Não foi possível calibrar o dispositivo",
        variant: "destructive",
      });
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <FlaskConical className="h-5 w-5" />
          Calibração de Sensores
        </CardTitle>
        <CardDescription>
          Calibre os sensores de pH e EC remotamente
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="space-y-2">
          <Label htmlFor="device">Dispositivo</Label>
          <Select value={selectedDevice} onValueChange={setSelectedDevice}>
            <SelectTrigger id="device">
              <SelectValue placeholder="Selecione um módulo de sensores" />
            </SelectTrigger>
            <SelectContent>
              {sensorDevices.map((device) => (
                <SelectItem key={device.id} value={device.id}>
                  {device.device_uuid}
                </SelectItem>
              ))}
            </SelectContent>
          </Select>
        </div>

        <div className="space-y-2">
          <Label htmlFor="sensor-type">Tipo de Sensor</Label>
          <Select value={sensorType} onValueChange={(v) => setSensorType(v as "ph" | "ec")}>
            <SelectTrigger id="sensor-type">
              <SelectValue />
            </SelectTrigger>
            <SelectContent>
              <SelectItem value="ph">pH</SelectItem>
              <SelectItem value="ec">EC (Condutividade)</SelectItem>
            </SelectContent>
          </Select>
        </div>

        {sensorType === "ph" ? (
          <>
            <div className="grid grid-cols-2 gap-4">
              <div className="space-y-2">
                <Label htmlFor="ph-cal4">Voltagem pH 4.0 *</Label>
                <Input
                  id="ph-cal4"
                  type="number"
                  step="0.01"
                  placeholder="2.03"
                  value={phCal4}
                  onChange={(e) => setPhCal4(e.target.value)}
                  disabled={isLoading}
                />
              </div>
              <div className="space-y-2">
                <Label htmlFor="ph-cal7">Voltagem pH 7.0 *</Label>
                <Input
                  id="ph-cal7"
                  type="number"
                  step="0.01"
                  placeholder="1.50"
                  value={phCal7}
                  onChange={(e) => setPhCal7(e.target.value)}
                  disabled={isLoading}
                />
              </div>
            </div>
            <div className="space-y-2">
              <Label htmlFor="ph-cal10">Voltagem pH 10.0 (opcional)</Label>
              <Input
                id="ph-cal10"
                type="number"
                step="0.01"
                placeholder="0.97"
                value={phCal10}
                onChange={(e) => setPhCal10(e.target.value)}
                disabled={isLoading}
              />
            </div>
          </>
        ) : (
          <div className="space-y-2">
            <Label htmlFor="ec-factor">Fator de Calibração EC *</Label>
            <Input
              id="ec-factor"
              type="number"
              step="0.1"
              placeholder="1.0"
              value={ecFactor}
              onChange={(e) => setEcFactor(e.target.value)}
              disabled={isLoading}
            />
            <p className="text-xs text-muted-foreground">
              Usar solução padrão conhecida (ex: 1413 µS/cm)
            </p>
          </div>
        )}

        <div className="space-y-2">
          <Label htmlFor="profile-name">Nome do Perfil (opcional)</Label>
          <Input
            id="profile-name"
            placeholder="Ex: Calibração Mensal Junho"
            value={profileName}
            onChange={(e) => setProfileName(e.target.value)}
            disabled={isLoading}
          />
        </div>

        <Button 
          onClick={handleCalibrate} 
          disabled={isLoading || !selectedDevice}
          className="w-full"
        >
          {isLoading ? (
            <>
              <Loader2 className="mr-2 h-4 w-4 animate-spin" />
              Calibrando...
            </>
          ) : (
            "Aplicar Calibração"
          )}
        </Button>

        <div className="bg-muted p-4 rounded-lg space-y-2">
          <p className="text-sm font-medium">Como calibrar:</p>
          <ol className="text-sm text-muted-foreground space-y-1 list-decimal list-inside">
            <li>Mergulhe o sensor na solução padrão</li>
            <li>Aguarde estabilizar (1-2 minutos)</li>
            <li>Leia a voltagem no monitor serial</li>
            <li>Insira os valores acima</li>
            <li>Repita para cada ponto de calibração</li>
          </ol>
        </div>
      </CardContent>
    </Card>
  );
};