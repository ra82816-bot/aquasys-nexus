import { useState, useEffect } from 'react';
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger } from '@/components/ui/dialog';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Settings2, Droplets, Zap, AlertTriangle, CheckCircle2, Wifi, WifiOff } from 'lucide-react';
import { useMqttContext } from '@/contexts/MqttContext';
import { useToast } from '@/hooks/use-toast';

// Calibração dois pontos (pH 4.0 e pH 6.86) - padrão industrial
const PH_CALIBRATION_POINTS = [
  { value: '6.86', label: 'pH 6.86', description: 'Solução neutra - CALIBRAR PRIMEIRO' },
  { value: '4.0', label: 'pH 4.0', description: 'Solução ácida - calibrar segundo' },
];

export const SensorCalibrationDialog = () => {
  const [open, setOpen] = useState(false);
  const [ecStandard, setEcStandard] = useState('1413');
  const [calibrating, setCalibrating] = useState<string | null>(null);
  const [lastCalibration, setLastCalibration] = useState<{ sensor: string; point: string; time: Date; confirmed: boolean; voltage?: number } | null>(null);
  const { publish, isConnected, lastMessage } = useMqttContext();
  const { toast } = useToast();

  // Monitorar respostas de calibração do ESP32
  useEffect(() => {
    if (lastMessage?.topic === 'aquasys/sensors/calibrate/response') {
      const response = lastMessage.payload;
      console.log('📡 Resposta de calibração recebida:', response);
      
      if (response.success) {
        setLastCalibration({
          sensor: response.sensor,
          point: response.point,
          time: new Date(),
          confirmed: true,
          voltage: response.value
        });
        
        toast({
          title: '✓ Calibração confirmada pelo ESP32',
          description: `${response.sensor.toUpperCase()} ${response.point}: ${response.value?.toFixed(3)}V`,
        });
      } else {
        toast({
          title: 'Calibração falhou no ESP32',
          description: `Sensor: ${response.sensor}, Ponto: ${response.point}`,
          variant: 'destructive',
        });
      }
      
      setCalibrating(null);
    }
  }, [lastMessage, toast]);

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

      console.log('🔧 Enviando calibração:', JSON.stringify(message));
      console.log('📤 Tópico: aquasys/sensors/calibrate');
      
      await publish('aquasys/sensors/calibrate', message);
      
      // Marcar como pendente até confirmação do ESP32
      setLastCalibration({
        sensor,
        point: sensor === 'ph' ? point! : `${standard} µS/cm`,
        time: new Date(),
        confirmed: false
      });

      toast({
        title: 'Comando enviado',
        description: 'Aguardando confirmação do módulo de sensores...',
      });

      // Timeout se não receber resposta em 10 segundos
      setTimeout(() => {
        if (calibrating === calibrationId) {
          setCalibrating(null);
          toast({
            title: 'Sem resposta do ESP32',
            description: 'Verifique se o módulo de sensores está conectado ao WiFi/MQTT',
            variant: 'destructive',
          });
        }
      }, 10000);

    } catch (error) {
      console.error('❌ Erro ao enviar calibração:', error);
      toast({
        title: 'Erro na calibração',
        description: 'Falha ao enviar comando MQTT',
        variant: 'destructive',
      });
      setCalibrating(null);
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

        {/* Status da conexão MQTT */}
        <div className={`flex items-center gap-2 p-3 rounded-lg ${isConnected ? 'bg-green-500/10 text-green-700 dark:text-green-400' : 'bg-destructive/10 text-destructive'}`}>
          {isConnected ? (
            <>
              <Wifi className="h-4 w-4" />
              <span className="text-sm">MQTT conectado - pronto para calibrar</span>
            </>
          ) : (
            <>
              <WifiOff className="h-4 w-4" />
              <span className="text-sm">MQTT desconectado - calibração indisponível</span>
            </>
          )}
        </div>

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
                  Calibre primeiro pH 6.86 (neutro), depois pH 4.0 (ácido).
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
                      variant={point.value === '6.86' ? 'default' : 'secondary'}
                      disabled={!isConnected || calibrating !== null}
                      onClick={() => sendCalibrationCommand('ph', point.value)}
                    >
                      {calibrating === `ph-${point.value}` ? (
                        <span className="flex items-center gap-2">
                          <span className="animate-spin">⏳</span>
                          Aguardando...
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
                        Aguardando...
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

        {/* Status da última calibração */}
        {lastCalibration && (
          <div className={`flex items-center gap-2 p-3 rounded-lg ${
            lastCalibration.confirmed 
              ? 'bg-green-500/10 text-green-700 dark:text-green-400' 
              : 'bg-yellow-500/10 text-yellow-700 dark:text-yellow-400'
          }`}>
            {lastCalibration.confirmed ? (
              <CheckCircle2 className="h-4 w-4" />
            ) : (
              <AlertTriangle className="h-4 w-4" />
            )}
            <span className="text-sm">
              {lastCalibration.confirmed 
                ? `✓ Calibrado: ${lastCalibration.sensor.toUpperCase()} ${lastCalibration.point}${lastCalibration.voltage ? ` (${lastCalibration.voltage.toFixed(3)}V)` : ''}`
                : `⏳ Pendente: ${lastCalibration.sensor.toUpperCase()} ${lastCalibration.point}`
              }
              {' às '}
              {lastCalibration.time.toLocaleTimeString()}
            </span>
          </div>
        )}

        {/* Dica de diagnóstico */}
        <div className="text-xs text-muted-foreground p-2 bg-muted/30 rounded">
          <strong>Debug:</strong> Verifique o Serial Monitor do ESP32 para logs detalhados de calibração.
          O módulo de sensores deve estar conectado ao WiFi (não em modo AP).
        </div>
      </DialogContent>
    </Dialog>
  );
};
