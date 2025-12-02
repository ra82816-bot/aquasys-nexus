import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Eye, Edit, Trash2 } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { format } from "date-fns";
import { ptBR } from "date-fns/locale";

interface PlantsListProps {
  userId?: string;
  onUpdate?: () => void;
}

const statusLabels = {
  germinating: 'Germinando',
  vegetative: 'Vegetativo',
  flowering: 'Floração',
  harvested: 'Colhida',
  discontinued: 'Descontinuada'
};

const statusColors = {
  germinating: 'bg-blue-500',
  vegetative: 'bg-green-500',
  flowering: 'bg-purple-500',
  harvested: 'bg-yellow-500',
  discontinued: 'bg-gray-500'
};

export const PlantsList = ({ userId, onUpdate }: PlantsListProps) => {
  const [plants, setPlants] = useState<any[]>([]);
  const [loading, setLoading] = useState(true);
  const { toast } = useToast();

  useEffect(() => {
    if (userId) {
      loadPlants();
    }
  }, [userId]);

  async function loadPlants() {
    try {
      setLoading(true);
      
      const { data, error } = await supabase
        .from('plants')
        .select('*')
        .eq('user_id', userId)
        .order('created_at', { ascending: false });

      if (error) throw error;
      setPlants(data || []);
    } catch (error: any) {
      toast({
        variant: "destructive",
        title: "Erro ao carregar plantas",
        description: error.message,
      });
    } finally {
      setLoading(false);
    }
  }

  async function handleDelete(plantId: string) {
    if (!confirm('Tem certeza que deseja excluir esta planta?')) {
      return;
    }

    try {
      const { error } = await supabase
        .from('plants')
        .delete()
        .eq('id', plantId);

      if (error) throw error;

      toast({
        title: "Planta excluída",
        description: "A planta foi removida com sucesso.",
      });

      loadPlants();
      onUpdate?.();
    } catch (error: any) {
      toast({
        variant: "destructive",
        title: "Erro ao excluir planta",
        description: error.message,
      });
    }
  }

  if (loading) {
    return (
      <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
        {[1, 2, 3].map((i) => (
          <Card key={i} className="animate-pulse">
            <CardHeader>
              <div className="h-6 bg-muted rounded w-3/4"></div>
              <div className="h-4 bg-muted rounded w-1/2 mt-2"></div>
            </CardHeader>
            <CardContent>
              <div className="h-20 bg-muted rounded"></div>
            </CardContent>
          </Card>
        ))}
      </div>
    );
  }

  if (plants.length === 0) {
    return (
      <Card>
        <CardContent className="py-12 text-center">
          <p className="text-muted-foreground">
            Você ainda não registrou nenhuma planta.
          </p>
        </CardContent>
      </Card>
    );
  }

  return (
    <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
      {plants.map((plant) => (
        <Card key={plant.id} className="hover:shadow-lg transition-shadow">
          <CardHeader>
            <div className="flex items-start justify-between">
              <div>
                <CardTitle className="text-lg">{plant.nickname}</CardTitle>
                <CardDescription>
                  {plant.genetics || plant.species || 'Genética não especificada'}
                </CardDescription>
              </div>
              <Badge className={statusColors[plant.status as keyof typeof statusColors]}>
                {statusLabels[plant.status as keyof typeof statusLabels]}
              </Badge>
            </div>
          </CardHeader>
          <CardContent>
            <div className="space-y-2 text-sm">
              {plant.registration_number && (
                <p className="text-muted-foreground">
                  <span className="font-medium">Nº:</span> {plant.registration_number}
                </p>
              )}
              {plant.germination_date && (
                <p className="text-muted-foreground">
                  <span className="font-medium">Germinação:</span>{' '}
                  {format(new Date(plant.germination_date), 'dd/MM/yyyy', { locale: ptBR })}
                </p>
              )}
              {plant.yield_grams && (
                <p className="text-muted-foreground">
                  <span className="font-medium">Rendimento:</span> {plant.yield_grams}g
                </p>
              )}
            </div>

            <div className="flex gap-2 mt-4 pt-4 border-t">
              <Button variant="outline" size="sm" className="flex-1">
                <Eye className="h-4 w-4 mr-1" />
                Ver
              </Button>
              <Button variant="outline" size="sm" className="flex-1">
                <Edit className="h-4 w-4 mr-1" />
                Editar
              </Button>
              <Button 
                variant="outline" 
                size="sm"
                onClick={() => handleDelete(plant.id)}
              >
                <Trash2 className="h-4 w-4 text-destructive" />
              </Button>
            </div>
          </CardContent>
        </Card>
      ))}
    </div>
  );
};
