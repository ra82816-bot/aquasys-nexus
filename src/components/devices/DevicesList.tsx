import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Skeleton } from "@/components/ui/skeleton";
import { Cpu, Radio, Trash2, Clock, Wifi } from "lucide-react";
import { format } from "date-fns";
import { ptBR } from "date-fns/locale";
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
  AlertDialogTrigger,
} from "@/components/ui/alert-dialog";

interface Device {
  id: string;
  device_uuid: string;
  device_type: 'sensor' | 'actuator';
  firmware_version: string | null;
  first_seen_at: string | null;
  last_seen_at: string | null;
  paired_at: string;
}

interface DevicesListProps {
  devices: Device[];
  loading: boolean;
  onRemove: (deviceId: string) => void;
}

export const DevicesList = ({ devices, loading, onRemove }: DevicesListProps) => {
  if (loading) {
    return (
      <div className="grid gap-4 md:grid-cols-2">
        {[1, 2].map((i) => (
          <Card key={i}>
            <CardHeader>
              <Skeleton className="h-6 w-32" />
            </CardHeader>
            <CardContent>
              <Skeleton className="h-4 w-full mb-2" />
              <Skeleton className="h-4 w-3/4" />
            </CardContent>
          </Card>
        ))}
      </div>
    );
  }

  if (devices.length === 0) {
    return (
      <Card className="border-dashed">
        <CardContent className="flex flex-col items-center justify-center py-12">
          <div className="rounded-full bg-muted p-4 mb-4">
            <Cpu className="h-8 w-8 text-muted-foreground" />
          </div>
          <h3 className="text-lg font-semibold mb-2">Nenhum dispositivo</h3>
          <p className="text-muted-foreground text-center max-w-sm">
            Você ainda não tem dispositivos vinculados. Clique em "Adicionar" para registrar seu primeiro módulo.
          </p>
        </CardContent>
      </Card>
    );
  }

  return (
    <div className="grid gap-4 md:grid-cols-2">
      {devices.map((device) => (
        <Card key={device.id} className="relative overflow-hidden">
          <div className={`absolute top-0 left-0 w-1 h-full ${
            device.device_type === 'sensor' ? 'bg-blue-500' : 'bg-orange-500'
          }`} />
          
          <CardHeader className="pb-2">
            <div className="flex items-start justify-between">
              <div className="flex items-center gap-2">
                {device.device_type === 'sensor' ? (
                  <Radio className="h-5 w-5 text-blue-500" />
                ) : (
                  <Cpu className="h-5 w-5 text-orange-500" />
                )}
                <CardTitle className="text-lg">
                  Módulo {device.device_type === 'sensor' ? 'Sensor' : 'Atuador'}
                </CardTitle>
              </div>
              <Badge variant={device.device_type === 'sensor' ? 'default' : 'secondary'}>
                {device.device_type}
              </Badge>
            </div>
          </CardHeader>
          
          <CardContent className="space-y-3">
            <div className="space-y-2 text-sm">
              <div className="flex items-center gap-2 text-muted-foreground">
                <span className="font-medium">UUID:</span>
                <code className="text-xs bg-muted px-2 py-0.5 rounded">
                  {device.device_uuid.slice(0, 8)}...{device.device_uuid.slice(-4)}
                </code>
              </div>
              
              {device.firmware_version && (
                <div className="flex items-center gap-2 text-muted-foreground">
                  <span className="font-medium">Firmware:</span>
                  <span>{device.firmware_version}</span>
                </div>
              )}
              
              <div className="flex items-center gap-2 text-muted-foreground">
                <Clock className="h-3.5 w-3.5" />
                <span>Vinculado em {format(new Date(device.paired_at), "dd/MM/yyyy", { locale: ptBR })}</span>
              </div>
              
              {device.last_seen_at && (
                <div className="flex items-center gap-2 text-muted-foreground">
                  <Wifi className="h-3.5 w-3.5" />
                  <span>Última conexão: {format(new Date(device.last_seen_at), "dd/MM HH:mm", { locale: ptBR })}</span>
                </div>
              )}
            </div>

            <div className="pt-2 border-t">
              <AlertDialog>
                <AlertDialogTrigger asChild>
                  <Button variant="ghost" size="sm" className="text-destructive hover:text-destructive hover:bg-destructive/10">
                    <Trash2 className="h-4 w-4 mr-2" />
                    Remover
                  </Button>
                </AlertDialogTrigger>
                <AlertDialogContent>
                  <AlertDialogHeader>
                    <AlertDialogTitle>Remover dispositivo?</AlertDialogTitle>
                    <AlertDialogDescription>
                      Esta ação irá desvincular o dispositivo da sua conta. Você poderá registrá-lo novamente usando o UUID e token.
                    </AlertDialogDescription>
                  </AlertDialogHeader>
                  <AlertDialogFooter>
                    <AlertDialogCancel>Cancelar</AlertDialogCancel>
                    <AlertDialogAction 
                      onClick={() => onRemove(device.id)}
                      className="bg-destructive text-destructive-foreground hover:bg-destructive/90"
                    >
                      Remover
                    </AlertDialogAction>
                  </AlertDialogFooter>
                </AlertDialogContent>
              </AlertDialog>
            </div>
          </CardContent>
        </Card>
      ))}
    </div>
  );
};
