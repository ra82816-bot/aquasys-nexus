import { useState } from 'react';
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Settings2, Droplets, Zap, AlertTriangle, CheckCircle2 } from 'lucide-react';
import { useMqttContext } from '@/contexts/MqttContext';
import { useToast } from '@/hooks/use-toast';

// Calibração dois pontos (pH 4.0 e pH 7.0) - padrão industrial
const PH_CALIBRATION_POINTS = [
  { value: '7.00', label: 'pH 7.0', description: 'Solução neutra - CALIBRAR PRIMEIRO' },
  { value: '4.0', label: 'pH 4.0', description: 'Solução ácida - calibrar segundo' },
];

export const SensorCalibrationDialog = () => {
  const [open, setOpen] = useState(false);
  const [ecStandard, setEcStandard] = useState('1413');
  const [calibrating, setCalibrating] = useState<string | null>(null);
  const [lastCalibration, setLastCalibration] = useState<{ sensor: string; point: string; time: Date } | null>(null);
  const { publish, isConnected } = useMqttContext();
  const { toast } = useToast();

  const sendCalibrationCommand = async (sensor: 'ph' | 'ec', point?: string, standard?: number) => {
    if (!isConnected) {
      toast({
        title: 'MQTT não conectado',
        description: 'Aguarde a conexão com o broker MQTT',
        variant: 'destructive',
      });
      return;
    }

    const calibrationId = sensor === 'ph' ? `ph-${point}` : 'ec';
    setCalibrating(calibrationId);

    try {
      const message = sensor === 'ph' 
        ? { sensor: 'ph', action: 'calibrate', point }
        : { sensor: 'ec', action: 'calibrate', standard_us: standard };

      console.log('🔧 Calibração:', JSON.stringify(message));
      
      await publish('aquasys/sensors/calibrate', message);
      
      setLastCalibration({
        sensor,
        point: sensor === 'ph' ? point! : `${standard} µS/cm`,
        time: new Date()
      });

      toast({
        title: 'Comando de calibração enviado',
        description: sensor === 'ph' 
          ? `Calibrando pH no ponto ${point}`
          : `Calibrando EC com padrão ${standard} µS/cm`,
      });
    } catch (error) {
      toast({
        title: 'Erro na calibração',
        description: 'Falha ao enviar comando MQTT',
        variant: 'destructive',
      });
    } finally {
      setTimeout(() => setCalibrating(null), 2000);
    }
  };

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button variant="outline" size="sm" className="gap-2">
          <Settings2 className="h-4 w-4" />
          Calibrar Sensores
        </Button>
      </DialogTrigger>
      <DialogContent className="sm:max-w-[500px]">
        <DialogHeader>
          <DialogTitle className="flex items-center gap-2">
            <Settings2 className="h-5 w-5" />
            Calibração de Sensores
          </DialogTitle>
        </DialogHeader>

        {!isConnected && (
          <div className="flex items-center gap-2 p-3 bg-destructive/10 text-destructive rounded-lg">
            <AlertTriangle className="h-4 w-4" />
            <span className="text-sm">MQTT desconectado - calibração indisponível</span>
          </div>
        )}

        <Tabs defaultValue="ph" className="w-full">
          <TabsList className="grid w-full grid-cols-2">
            <TabsTrigger value="ph" className="gap-2">
              <Droplets className="h-4 w-4" />
              pH
            </TabsTrigger>
            <TabsTrigger value="ec" className="gap-2">
              <Zap className="h-4 w-4" />
              EC
            </TabsTrigger>
          </TabsList>

          <TabsContent value="ph" className="space-y-4">
            <Card>
              <CardHeader className="pb-3">
                <CardTitle className="text-base">Calibração de pH (Dois Pontos)</CardTitle>
                <CardDescription>
                  Mergulhe o sensor na solução padrão e clique no botão correspondente.
                  Calibre primeiro pH 7.0 (neutro), depois pH 4.0 (ácido).
                </CardDescription>
              </CardHeader>
              <CardContent className="space-y-3">
                {PH_CALIBRATION_POINTS.map((point) => (
                  <div 
                    key={point.value} 
                    className="flex items-center justify-between p-3 bg-muted/50 rounded-lg"
                  >
                    <div>
                      <p className="font-medium">{point.label}</p>
                      <p className="text-xs text-muted-foreground">{point.description}</p>
                    </div>
                    <Button
                      size="sm"
                      variant={point.value === '7.00' ? 'default' : 'secondary'}
                      disabled={!isConnected || calibrating !== null}
                      onClick={() => sendCalibrationCommand('ph', point.value)}
                    >
                      {calibrating === `ph-${point.value}` ? (
                        <span className="flex items-center gap-2">
                          <span className="animate-spin">⏳</span>
                          Calibrando...
                        </span>
                      ) : (
                        'Calibrar'
                      )}
                    </Button>
                  </div>
                ))}
              </CardContent>
            </Card>
          </TabsContent>

          <TabsContent value="ec" className="space-y-4">
            <Card>
              <CardHeader className="pb-3">
                <CardTitle className="text-base">Calibração de EC</CardTitle>
                <CardDescription>
                  Mergulhe o sensor na solução padrão de EC e informe o valor em µS/cm.
                </CardDescription>
              </CardHeader>
              <CardContent className="space-y-4">
                <div className="space-y-2">
                  <Label htmlFor="ec-standard">Valor padrão (µS/cm)</Label>
                  <Input
                    id="ec-standard"
                    type="number"
                    value={ecStandard}
                    onChange={(e) => setEcStandard(e.target.value)}
                    placeholder="Ex: 1413"
                    min="0"
                    max="10000"
                  />
                  <p className="text-xs text-muted-foreground">
                    Valores comuns: 1413 µS/cm, 2764 µS/cm, 12880 µS/cm
                  </p>
                </div>
                
                <div className="flex gap-2">
                  <Button
                    className="flex-1"
                    disabled={!isConnected || calibrating !== null || !ecStandard}
                    onClick={() => sendCalibrationCommand('ec', undefined, parseFloat(ecStandard))}
                  >
                    {calibrating === 'ec' ? (
                      <span className="flex items-center gap-2">
                        <span className="animate-spin">⏳</span>
                        Calibrando...
                      </span>
                    ) : (
                      'Calibrar EC'
                    )}
                  </Button>
                </div>

                <div className="flex flex-wrap gap-2 pt-2">
                  <p className="text-xs text-muted-foreground w-full">Padrões rápidos:</p>
                  {['1413', '2764', '12880'].map((value) => (
                    <Button
                      key={value}
                      variant="outline"
                      size="sm"
                      onClick={() => setEcStandard(value)}
                    >
                      {value} µS/cm
                    </Button>
                  ))}
                </div>
              </CardContent>
            </Card>
          </TabsContent>
        </Tabs>

        {lastCalibration && (
          <div className="flex items-center gap-2 p-3 bg-green-500/10 text-green-700 dark:text-green-400 rounded-lg">
            <CheckCircle2 className="h-4 w-4" />
            <span className="text-sm">
              Última calibração: {lastCalibration.sensor.toUpperCase()} {lastCalibration.point} às{' '}
              {lastCalibration.time.toLocaleTimeString()}
            </span>
          </div>
        )}
      </DialogContent>
    </Dialog>
  );
};
