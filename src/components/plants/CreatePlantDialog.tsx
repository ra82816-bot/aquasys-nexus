import { useState, useEffect } from "react";
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
  editPlantId?: string;
}

export const CreatePlantDialog = ({ open, onOpenChange, userId, onSuccess, editPlantId }: CreatePlantDialogProps) => {
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
  const [loadingPlant, setLoadingPlant] = useState(false);
  const { toast } = useToast();

  // Load plant data if editing
  useEffect(() => {
    if (editPlantId && open) {
      loadPlantData();
    } else if (!open) {
      // Reset form when dialog closes
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
    }
  }, [editPlantId, open]);

  async function loadPlantData() {
    setLoadingPlant(true);
    try {
      const { data, error } = await supabase
        .from('plants')
        .select('*')
        .eq('id', editPlantId)
        .single();

      if (error) throw error;

      setFormData({
        nickname: data.nickname || "",
        species: data.species || "",
        genetics: data.genetics || "",
        origin: data.origin || "seed",
        registration_number: data.registration_number || "",
        germination_date: data.germination_date ? new Date(data.germination_date) : undefined,
        general_notes: data.general_notes || "",
        nutrients_type: data.nutrients_type || "",
        substrate_type: data.substrate_type || "",
        light_cycle: data.light_cycle || "",
      });
    } catch (error: any) {
      toast({
        title: "Erro",
        description: "Não foi possível carregar os dados da planta",
        variant: "destructive",
      });
    } finally {
      setLoadingPlant(false);
    }
  }

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

      const plantData = {
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
      };

      if (editPlantId) {
        // Update existing plant
        const { error } = await supabase
          .from('plants')
          .update(plantData)
          .eq('id', editPlantId);

        if (error) throw error;

        toast({
          title: "Planta atualizada!",
          description: "As informações da planta foram atualizadas com sucesso.",
        });
      } else {
        // Create new plant
        const { error } = await supabase
          .from('plants')
          .insert({
            ...plantData,
            status: 'germinating',
          });

        if (error) throw error;

        toast({
          title: "Planta registrada!",
          description: "A planta foi adicionada ao seu histórico.",
        });
      }

      onOpenChange(false);
      onSuccess?.();
    } catch (error: any) {
      toast({
        variant: "destructive",
        title: editPlantId ? "Erro ao atualizar planta" : "Erro ao registrar planta",
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
          <DialogTitle>{editPlantId ? 'Editar Planta' : 'Registrar Nova Planta'}</DialogTitle>
          <DialogDescription>
            {editPlantId ? 'Atualize os dados da planta' : 'Preencha os dados para adicionar uma nova planta ao seu cultivo'}
          </DialogDescription>
        </DialogHeader>

        {loadingPlant ? (
          <div className="flex items-center justify-center py-8">
            <div className="h-8 w-8 border-4 border-primary border-t-transparent rounded-full animate-spin" />
          </div>
        ) : (
          <form onSubmit={handleSubmit} className="space-y-6">
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div className="space-y-2">
                <Label htmlFor="nickname">Apelido da Planta *</Label>
                <Input
                  id="nickname"
                  value={formData.nickname}
                  onChange={(e) => setFormData({ ...formData, nickname: e.target.value })}
                  placeholder="Ex: Purple Haze #1"
                  required
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="registration_number">Número de Registro</Label>
                <Input
                  id="registration_number"
                  value={formData.registration_number}
                  onChange={(e) => setFormData({ ...formData, registration_number: e.target.value })}
                  placeholder="Ex: PH-2024-001"
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="species">Espécie</Label>
                <Input
                  id="species"
                  value={formData.species}
                  onChange={(e) => setFormData({ ...formData, species: e.target.value })}
                  placeholder="Ex: Cannabis Indica"
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="genetics">Genética</Label>
                <Input
                  id="genetics"
                  value={formData.genetics}
                  onChange={(e) => setFormData({ ...formData, genetics: e.target.value })}
                  placeholder="Ex: Purple Haze x Skunk"
                />
              </div>

              <div className="space-y-2">
                <Label>Origem</Label>
                <Select 
                  value={formData.origin} 
                  onValueChange={(value: "seed" | "clone") => setFormData({ ...formData, origin: value })}
                >
                  <SelectTrigger>
                    <SelectValue />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="seed">Semente</SelectItem>
                    <SelectItem value="clone">Clone</SelectItem>
                  </SelectContent>
                </Select>
              </div>

              <div className="space-y-2">
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
                      {formData.germination_date ? format(formData.germination_date, "PPP", { locale: ptBR }) : "Selecione a data"}
                    </Button>
                  </PopoverTrigger>
                  <PopoverContent className="w-auto p-0">
                    <Calendar
                      mode="single"
                      selected={formData.germination_date}
                      onSelect={(date) => setFormData({ ...formData, germination_date: date })}
                      initialFocus
                    />
                  </PopoverContent>
                </Popover>
              </div>

              <div className="space-y-2">
                <Label htmlFor="substrate_type">Tipo de Substrato</Label>
                <Select 
                  value={formData.substrate_type} 
                  onValueChange={(value) => setFormData({ ...formData, substrate_type: value })}
                >
                  <SelectTrigger>
                    <SelectValue placeholder="Selecione" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="hydroponics">Hidroponia (NFT)</SelectItem>
                    <SelectItem value="dwc">DWC (Deep Water Culture)</SelectItem>
                    <SelectItem value="soil">Solo Orgânico</SelectItem>
                    <SelectItem value="coco">Fibra de Coco</SelectItem>
                    <SelectItem value="perlite">Perlita/Vermiculita</SelectItem>
                    <SelectItem value="none">Nenhum</SelectItem>
                  </SelectContent>
                </Select>
              </div>

              <div className="space-y-2">
                <Label htmlFor="light_cycle">Ciclo de Luz</Label>
                <Select 
                  value={formData.light_cycle} 
                  onValueChange={(value) => setFormData({ ...formData, light_cycle: value })}
                >
                  <SelectTrigger>
                    <SelectValue placeholder="Selecione" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="18/6">18/6 (Vegetativo)</SelectItem>
                    <SelectItem value="12/12">12/12 (Floração)</SelectItem>
                    <SelectItem value="20/4">20/4 (Vegetativo intenso)</SelectItem>
                    <SelectItem value="24/0">24/0 (Contínuo)</SelectItem>
                    <SelectItem value="none">Nenhum</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>

            <div className="space-y-2">
              <Label htmlFor="nutrients_type">Tipo de Nutrientes</Label>
              <Input
                id="nutrients_type"
                value={formData.nutrients_type}
                onChange={(e) => setFormData({ ...formData, nutrients_type: e.target.value })}
                placeholder="Ex: General Hydroponics Flora Series"
              />
            </div>

            <div className="space-y-2">
              <Label htmlFor="general_notes">Observações Gerais</Label>
              <Textarea
                id="general_notes"
                value={formData.general_notes}
                onChange={(e) => setFormData({ ...formData, general_notes: e.target.value })}
                placeholder="Anotações sobre o cultivo, observações importantes, etc."
                rows={4}
              />
            </div>

            <div className="flex gap-3 justify-end">
              <Button type="button" variant="outline" onClick={() => onOpenChange(false)}>
                Cancelar
              </Button>
              <Button type="submit" disabled={loading}>
                {loading ? "Salvando..." : (editPlantId ? "Atualizar Planta" : "Registrar Planta")}
              </Button>
            </div>
          </form>
        )}
      </DialogContent>
    </Dialog>
  );
};
