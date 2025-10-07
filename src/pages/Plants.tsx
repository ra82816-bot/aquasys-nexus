import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Plus, ArrowLeft, Sprout, Calendar, TrendingUp } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { PlantsList } from "@/components/plants/PlantsList";
import { CreatePlantDialog } from "@/components/plants/CreatePlantDialog";

const Plants = () => {
  const navigate = useNavigate();
  const { toast } = useToast();
  const [user, setUser] = useState<any>(null);
  const [loading, setLoading] = useState(true);
  const [createPlantOpen, setCreatePlantOpen] = useState(false);
  const [stats, setStats] = useState({
    total: 0,
    active: 0,
    harvested: 0
  });

  useEffect(() => {
    checkUser();
  }, []);

  useEffect(() => {
    if (user) {
      loadStats();
    }
  }, [user]);

  async function checkUser() {
    try {
      const { data: { session } } = await supabase.auth.getSession();
      
      if (!session) {
        navigate('/auth');
        return;
      }
      
      setUser(session.user);
    } catch (error: any) {
      toast({
        variant: "destructive",
        title: "Erro ao verificar autenticação",
        description: error.message,
      });
      navigate('/auth');
    } finally {
      setLoading(false);
    }
  }

  async function loadStats() {
    try {
      const { data: plants, error } = await supabase
        .from('plants')
        .select('status')
        .eq('user_id', user.id);

      if (error) throw error;

      setStats({
        total: plants?.length || 0,
        active: plants?.filter(p => ['germinating', 'vegetative', 'flowering'].includes(p.status)).length || 0,
        harvested: plants?.filter(p => p.status === 'harvested').length || 0
      });
    } catch (error: any) {
      console.error('Error loading stats:', error);
    }
  }

  if (loading) {
    return (
      <div className="min-h-screen flex items-center justify-center bg-background">
        <div className="text-center">
          <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary mx-auto mb-4"></div>
          <p className="text-muted-foreground">Carregando plantas...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-background">
      <header className="border-b bg-card/50 backdrop-blur">
        <div className="container mx-auto px-4 py-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-4">
              <Button
                variant="ghost"
                size="icon"
                onClick={() => navigate('/dashboard')}
              >
                <ArrowLeft className="h-5 w-5" />
              </Button>
              <div>
                <h1 className="text-2xl font-bold">Minhas Plantas</h1>
                <p className="text-sm text-muted-foreground">
                  Gerencie o histórico e acompanhe seus cultivos
                </p>
              </div>
            </div>
            <Button onClick={() => setCreatePlantOpen(true)}>
              <Plus className="mr-2 h-4 w-4" />
              Nova Planta
            </Button>
          </div>
        </div>
      </header>

      <main className="container mx-auto px-4 py-8">
        <div className="grid gap-6 md:grid-cols-3 mb-8">
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Total de Plantas</CardTitle>
              <Sprout className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{stats.total}</div>
              <p className="text-xs text-muted-foreground">
                Registradas no sistema
              </p>
            </CardContent>
          </Card>
          
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Cultivos Ativos</CardTitle>
              <Calendar className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{stats.active}</div>
              <p className="text-xs text-muted-foreground">
                Em fase de crescimento
              </p>
            </CardContent>
          </Card>

          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Colheitas</CardTitle>
              <TrendingUp className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{stats.harvested}</div>
              <p className="text-xs text-muted-foreground">
                Cultivos finalizados
              </p>
            </CardContent>
          </Card>
        </div>

        <PlantsList userId={user?.id} onUpdate={loadStats} />
      </main>

      <CreatePlantDialog 
        open={createPlantOpen} 
        onOpenChange={setCreatePlantOpen}
        userId={user?.id}
        onSuccess={loadStats}
      />
    </div>
  );
};

export default Plants;
