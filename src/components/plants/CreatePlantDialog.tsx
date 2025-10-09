import { useState } from "react";
import { Dialog, DialogContent, DialogDescription, DialogHeader, DialogTitle } from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { Textarea } from "@/components/ui/textarea";
import { Calendar } from "@/components/ui/calendar";
import { Popover, PopoverContent, PopoverTrigger } from "@/components/ui/popover";
import { CalendarIcon } from "lucide-react";
import { format } from "date-fns";
import { ptBR } from "date-fns/locale";
import { cn } from "@/lib/utils";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";

interface CreatePlantDialogProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  userId?: string;
  onSuccess?: () => void;
}

export const CreatePlantDialog = ({ open, onOpenChange, userId, onSuccess }: CreatePlantDialogProps) => {
  const [formData, setFormData] = useState<{
    nickname: string;
    species: string;
    genetics: string;
    origin: "seed" | "clone";
    registration_number: string;
    germination_date: Date | undefined;
    general_notes: string;
    nutrients_type: string;
    substrate_type: string;
    light_cycle: string;
  }>({
    nickname: "",
    species: "",
    genetics: "",
    origin: "seed",
    registration_number: "",
    germination_date: undefined,
    general_notes: "",
    nutrients_type: "",
    substrate_type: "",
    light_cycle: "",
  });
  const [loading, setLoading] = useState(false);
  const { toast } = useToast();

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    
    if (!userId) {
      toast({
        variant: "destructive",
        title: "Erro",
        description: "Você precisa estar autenticado",
      });
      return;
    }

    if (!formData.nickname.trim()) {
      toast({
        variant: "destructive",
        title: "Campo obrigatório",
        description: "Informe um apelido para a planta",
      });
      return;
    }

    try {
      setLoading(true);

      const { error } = await supabase
        .from('plants')
        .insert({
          user_id: userId,
          nickname: formData.nickname.trim(),
          species: formData.species.trim() || null,
          genetics: formData.genetics.trim() || null,
          origin: formData.origin,
          registration_number: formData.registration_number.trim() || null,
          germination_date: formData.germination_date?.toISOString().split('T')[0] || null,
          general_notes: formData.general_notes.trim() || null,
          nutrients_type: formData.nutrients_type.trim() || null,
          substrate_type: formData.substrate_type.trim() || null,
          light_cycle: formData.light_cycle.trim() || null,
          status: 'germinating',
        });

      if (error) throw error;

      toast({
        title: "Planta registrada!",
        description: "A planta foi adicionada ao seu histórico.",
      });

      setFormData({
        nickname: "",
        species: "",
        genetics: "",
        origin: "seed",
        registration_number: "",
        germination_date: undefined,
        general_notes: "",
        nutrients_type: "",
        substrate_type: "",
        light_cycle: "",
      });
      
      onOpenChange(false);
      onSuccess?.();
    } catch (error: any) {
      toast({
        variant: "destructive",
        title: "Erro ao registrar planta",
        description: error.message,
      });
    } finally {
      setLoading(false);
    }
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-2xl max-h-[90vh] overflow-y-auto">
        <DialogHeader>
          <DialogTitle>Registrar Nova Planta</DialogTitle>
          <DialogDescription>
            Preencha as informações básicas da planta para começar o acompanhamento
          </DialogDescription>
        </DialogHeader>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div className="grid gap-4 md:grid-cols-2">
            <div>
              <Label htmlFor="nickname">Apelido da Planta *</Label>
              <Input
                id="nickname"
                value={formData.nickname}
                onChange={(e) => setFormData({ ...formData, nickname: e.target.value })}
                placeholder="Ex: Purple Haze #1"
                required
              />
            </div>

            <div>
              <Label htmlFor="registration_number">Número de Registro</Label>
              <Input
                id="registration_number"
                value={formData.registration_number}
                onChange={(e) => setFormData({ ...formData, registration_number: e.target.value })}
                placeholder="Ex: PH-2024-001"
              />
            </div>
          </div>

          <div className="grid gap-4 md:grid-cols-2">
            <div>
              <Label htmlFor="species">Espécie</Label>
              <Input
                id="species"
                value={formData.species}
                onChange={(e) => setFormData({ ...formData, species: e.target.value })}
                placeholder="Ex: Cannabis Sativa"
              />
            </div>

            <div>
              <Label htmlFor="genetics">Genética</Label>
              <Input
                id="genetics"
                value={formData.genetics}
                onChange={(e) => setFormData({ ...formData, genetics: e.target.value })}
                placeholder="Ex: Northern Lights x Skunk"
              />
            </div>
          </div>

          <div className="grid gap-4 md:grid-cols-2">
            <div>
              <Label htmlFor="origin">Origem</Label>
              <Select 
                value={formData.origin} 
                onValueChange={(value: "seed" | "clone") => setFormData({ ...formData, origin: value })}
              >
                <SelectTrigger>
                  <SelectValue placeholder="Selecione" />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="seed">Semente</SelectItem>
                  <SelectItem value="clone">Clone</SelectItem>
                </SelectContent>
              </Select>
            </div>

            <div>
              <Label>Data de Germinação</Label>
              <Popover>
                <PopoverTrigger asChild>
                  <Button
                    variant="outline"
                    className={cn(
                      "w-full justify-start text-left font-normal",
                      !formData.germination_date && "text-muted-foreground"
                    )}
                  >
                    <CalendarIcon className="mr-2 h-4 w-4" />
                    {formData.germination_date ? (
                      format(formData.germination_date, "dd/MM/yyyy", { locale: ptBR })
                    ) : (
                      <span>Selecione a data</span>
                    )}
                  </Button>
                </PopoverTrigger>
                <PopoverContent className="w-auto p-0" align="start">
                  <Calendar
                    mode="single"
                    selected={formData.germination_date}
                    onSelect={(date) => setFormData({ ...formData, germination_date: date })}
                    disabled={(date) => date > new Date()}
                    initialFocus
                    className={cn("p-3 pointer-events-auto")}
                  />
                </PopoverContent>
              </Popover>
            </div>
          </div>

          <div className="grid gap-4 md:grid-cols-3">
            <div>
              <Label htmlFor="nutrients_type">Tipo de Nutentes</Label>
              <Input
                id="nutrients_type"
                value={formData.nutrients_type}
                onChange={(e) => setFormData({ ...formData, nutrients_type: e.target.value })}
                placeholder="Ex: BioBizz, General Hydroponics"
              />
            </div>

            <div>
              <Label htmlFor="substrate_type">Substrato</Label>
              <Select 
                value={formData.substrate_type} 
                onValueChange={(value) => setFormData({ ...formData, substrate_type: value })}
              >
                <SelectTrigger>
                  <SelectValue placeholder="Selecione" />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="">Nenhum</SelectItem>
                  <SelectItem value="hydroponics">Hidroponia (NFT)</SelectItem>
                  <SelectItem value="dwc">DWC (Deep Water Culture)</SelectItem>
                  <SelectItem value="soil">Solo Orgânico</SelectItem>
                  <SelectItem value="coco">Fibra de Coco</SelectItem>
                  <SelectItem value="perlite">Perlita/Vermiculita</SelectItem>
                </SelectContent>
              </Select>
            </div>

            <div>
              <Label htmlFor="light_cycle">Ciclo de Luz</Label>
              <Select 
                value={formData.light_cycle} 
                onValueChange={(value) => setFormData({ ...formData, light_cycle: value })}
              >
                <SelectTrigger>
                  <SelectValue placeholder="Selecione" />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="">Nenhum</SelectItem>
                  <SelectItem value="18/6">18/6 (Vegetativo)</SelectItem>
                  <SelectItem value="12/12">12/12 (Floração)</SelectItem>
                  <SelectItem value="20/4">20/4 (Vegetativo intenso)</SelectItem>
                  <SelectItem value="24/0">24/0 (Contínuo)</SelectItem>
                </SelectContent>
              </Select>
            </div>
          </div>

          <div>
            <Label htmlFor="general_notes">Observações Gerais</Label>
            <Textarea
              id="general_notes"
              value={formData.general_notes}
              onChange={(e) => setFormData({ ...formData, general_notes: e.target.value })}
              placeholder="Notas, objetivos do cultivo, etc..."
              rows={4}
            />
          </div>

          <div className="flex justify-end gap-2 pt-4">
            <Button
              type="button"
              variant="outline"
              onClick={() => onOpenChange(false)}
              disabled={loading}
            >
              Cancelar
            </Button>
            <Button type="submit" disabled={loading}>
              {loading ? "Registrando..." : "Registrar Planta"}
            </Button>
          </div>
        </form>
      </DialogContent>
    </Dialog>
  );
};
