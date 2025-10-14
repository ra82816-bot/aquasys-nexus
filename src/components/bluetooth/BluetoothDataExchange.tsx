import { useState } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Send, Download } from 'lucide-react';
import { Badge } from '@/components/ui/badge';

interface BluetoothDataExchangeProps {
  isConnected: boolean;
  receivedData: string[];
  onSendData: (serviceUUID: string, characteristicUUID: string, data: string) => void;
  onReadData: (serviceUUID: string, characteristicUUID: string) => void;
}

export const BluetoothDataExchange = ({
  isConnected,
  receivedData,
  onSendData,
  onReadData,
}: BluetoothDataExchangeProps) => {
  const [serviceUUID, setServiceUUID] = useState('');
  const [characteristicUUID, setCharacteristicUUID] = useState('');
  const [message, setMessage] = useState('');

  const handleSend = () => {
    if (!serviceUUID || !characteristicUUID || !message) return;
    onSendData(serviceUUID, characteristicUUID, message);
    setMessage('');
  };

  const handleRead = () => {
    if (!serviceUUID || !characteristicUUID) return;
    onReadData(serviceUUID, characteristicUUID);
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Send className="h-5 w-5" />
          Troca de Dados
        </CardTitle>
        <CardDescription>
          Envie e receba dados do dispositivo conectado
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-4">
        {!isConnected ? (
          <div className="text-center py-8 text-muted-foreground">
            <Send className="h-12 w-12 mx-auto mb-2 opacity-50" />
            <p>Conecte-se a um dispositivo para trocar dados</p>
          </div>
        ) : (
          <>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div>
                <Label htmlFor="serviceUUID">Service UUID</Label>
                <Input
                  id="serviceUUID"
                  placeholder="Ex: 0000180d-0000-1000-8000-00805f9b34fb"
                  value={serviceUUID}
                  onChange={(e) => setServiceUUID(e.target.value)}
                />
              </div>
              <div>
                <Label htmlFor="characteristicUUID">Characteristic UUID</Label>
                <Input
                  id="characteristicUUID"
                  placeholder="Ex: 00002a37-0000-1000-8000-00805f9b34fb"
                  value={characteristicUUID}
                  onChange={(e) => setCharacteristicUUID(e.target.value)}
                />
              </div>
            </div>

            <div className="space-y-2">
              <Label htmlFor="message">Mensagem para Enviar</Label>
              <div className="flex gap-2">
                <Input
                  id="message"
                  placeholder="Digite sua mensagem..."
                  value={message}
                  onChange={(e) => setMessage(e.target.value)}
                  onKeyPress={(e) => e.key === 'Enter' && handleSend()}
                />
                <Button onClick={handleSend} disabled={!message || !serviceUUID || !characteristicUUID}>
                  <Send className="h-4 w-4" />
                </Button>
              </div>
            </div>

            <Button
              onClick={handleRead}
              variant="outline"
              className="w-full"
              disabled={!serviceUUID || !characteristicUUID}
            >
              <Download className="h-4 w-4 mr-2" />
              Ler Dados
            </Button>

            <div>
              <Label className="mb-2 block">Dados Recebidos</Label>
              <Card className="bg-muted/50">
                <ScrollArea className="h-[200px] p-4">
                  {receivedData.length === 0 ? (
                    <p className="text-sm text-muted-foreground text-center py-4">
                      Nenhum dado recebido ainda
                    </p>
                  ) : (
                    <div className="space-y-2">
                      {receivedData.map((data, index) => (
                        <div
                          key={index}
                          className="flex items-start gap-2 text-sm bg-background p-2 rounded"
                        >
                          <Badge variant="outline" className="shrink-0">
                            {index + 1}
                          </Badge>
                          <code className="flex-1 break-all">{data}</code>
                        </div>
                      ))}
                    </div>
                  )}
                </ScrollArea>
              </Card>
            </div>
          </>
        )}
      </CardContent>
    </Card>
  );
};
