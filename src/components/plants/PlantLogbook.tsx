import { useState } from "react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { Label } from "@/components/ui/label";
import { Dialog, DialogContent, DialogDescription, DialogHeader, DialogTitle, DialogTrigger } from "@/components/ui/dialog";
import { Calendar, Plus, Camera, Ruler, Droplet, Zap, Thermometer, Wind } from "lucide-react";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";

interface PlantLogbookProps {
  plantId: string;
  onEntryAdded: () => void;
}

export const PlantLogbook = ({ plantId, onEntryAdded }: PlantLogbookProps) => {
  const [open, setOpen] = useState(false);
  const [loading, setLoading] = useState(false);
  const { toast } = useToast();

  // Estados do formulário
  const [observationDate, setObservationDate] = useState(new Date().toISOString().split('T')[0]);
  const [notes, setNotes] = useState('');
  const [heightCm, setHeightCm] = useState('');
  const [phLevel, setPhLevel] = useState('');
  const [ecLevel, setEcLevel] = useState('');
  const [temperature, setTemperature] = useState('');
  const [humidity, setHumidity] = useState('');

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    
    if (!notes.trim()) {
      toast({
        title: "Erro",
        description: "Adicione pelo menos uma nota",
        variant: "destructive"
      });
      return;
    }

    setLoading(true);

    try {
      const observation: any = {
        plant_id: plantId,
        observation_date: observationDate,
        notes: notes.trim()
      };

      // Adicionar medições opcionais
      if (heightCm) observation.height_cm = parseFloat(heightCm);
      if (phLevel) observation.ph_level = parseFloat(phLevel);
      if (ecLevel) observation.ec_level = parseFloat(ecLevel);
      if (temperature) observation.temperature = parseFloat(temperature);
      if (humidity) observation.humidity = parseFloat(humidity);

      const { error } = await supabase
        .from('plant_observations')
        .insert(observation);

      if (error) throw error;

      toast({
        title: "✅ Entrada registrada",
        description: "Observação adicionada ao diário de bordo"
      });

      // Limpar formulário
      setNotes('');
      setHeightCm('');
      setPhLevel('');
      setEcLevel('');
      setTemperature('');
      setHumidity('');
      setObservationDate(new Date().toISOString().split('T')[0]);
      
      setOpen(false);
      onEntryAdded();
    } catch (error: any) {
      console.error('Erro ao adicionar observação:', error);
      toast({
        title: "Erro",
        description: "Não foi possível adicionar a observação",
        variant: "destructive"
      });
    } finally {
      setLoading(false);
    }
  };

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button className="gap-2">
          <Plus className="h-4 w-4" />
          Nova Entrada
        </Button>
      </DialogTrigger>
      <DialogContent className="max-w-2xl max-h-[90vh] overflow-y-auto">
        <DialogHeader>
          <DialogTitle className="flex items-center gap-2">
            <Calendar className="h-5 w-5 text-primary" />
            Adicionar Entrada ao Diário de Bordo
          </DialogTitle>
          <DialogDescription>
            Registre observações, medições e fotos do desenvolvimento da planta
          </DialogDescription>
        </DialogHeader>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div className="space-y-2">
            <Label htmlFor="observation_date">Data da Observação</Label>
            <Input
              id="observation_date"
              type="date"
              value={observationDate}
              onChange={(e) => setObservationDate(e.target.value)}
              required
            />
          </div>

          <div className="space-y-2">
            <Label htmlFor="notes">
              Notas e Observações <span className="text-destructive">*</span>
            </Label>
            <Textarea
              id="notes"
              placeholder="Descreva o estado da planta, mudanças observadas, ações tomadas..."
              value={notes}
              onChange={(e) => setNotes(e.target.value)}
              rows={4}
              required
            />
          </div>

          <div className="grid grid-cols-2 gap-4">
            <div className="space-y-2">
              <Label htmlFor="height" className="flex items-center gap-2">
                <Ruler className="h-4 w-4" />
                Altura (cm)
              </Label>
              <Input
                id="height"
                type="number"
                step="0.1"
                placeholder="Ex: 25.5"
                value={heightCm}
                onChange={(e) => setHeightCm(e.target.value)}
              />
            </div>

            <div className="space-y-2">
              <Label htmlFor="ph" className="flex items-center gap-2">
                <Droplet className="h-4 w-4" />
                pH
              </Label>
              <Input
                id="ph"
                type="number"
                step="0.01"
                placeholder="Ex: 6.2"
                value={phLevel}
                onChange={(e) => setPhLevel(e.target.value)}
              />
            </div>

            <div className="space-y-2">
              <Label htmlFor="ec" className="flex items-center gap-2">
                <Zap className="h-4 w-4" />
                EC (µS/cm)
              </Label>
              <Input
                id="ec"
                type="number"
                step="1"
                placeholder="Ex: 1500"
                value={ecLevel}
                onChange={(e) => setEcLevel(e.target.value)}
              />
            </div>

            <div className="space-y-2">
              <Label htmlFor="temp" className="flex items-center gap-2">
                <Thermometer className="h-4 w-4" />
                Temperatura (°C)
              </Label>
              <Input
                id="temp"
                type="number"
                step="0.1"
                placeholder="Ex: 24.5"
                value={temperature}
                onChange={(e) => setTemperature(e.target.value)}
              />
            </div>

            <div className="space-y-2 col-span-2">
              <Label htmlFor="humidity" className="flex items-center gap-2">
                <Wind className="h-4 w-4" />
                Umidade (%)
              </Label>
              <Input
                id="humidity"
                type="number"
                step="0.1"
                placeholder="Ex: 65.5"
                value={humidity}
                onChange={(e) => setHumidity(e.target.value)}
              />
            </div>
          </div>

          <div className="flex items-center gap-2 pt-4 border-t">
            <Button
              type="button"
              variant="outline"
              onClick={() => setOpen(false)}
              disabled={loading}
            >
              Cancelar
            </Button>
            <Button type="submit" disabled={loading} className="flex-1">
              {loading ? (
                <>
                  <div className="h-4 w-4 border-2 border-current border-t-transparent rounded-full animate-spin mr-2" />
                  Salvando...
                </>
              ) : (
                'Adicionar Entrada'
              )}
            </Button>
          </div>
        </form>
      </DialogContent>
    </Dialog>
  );
};
