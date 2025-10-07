import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Session } from "@supabase/supabase-js";
import { Button } from "@/components/ui/button";
import { LogOut, Activity, BarChart3, Settings, Brain, Users, Sprout } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { SensorCard } from "@/components/dashboard/SensorCard";
import { SensorCharts } from "@/components/dashboard/SensorCharts";
import { RelayControls } from "@/components/dashboard/RelayControls";
import { MqttStatus } from "@/components/dashboard/MqttStatus";
import { AIInsights } from "@/components/dashboard/AIInsights";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { MqttProvider } from "@/contexts/MqttContext";

const CannabisLeaf = () => (
  <svg viewBox="0 0 24 24" fill="currentColor" className="h-6 w-6">
    <path d="M12 2L10.5 4.5C10 5.5 9 6.5 8 7C7 7.5 6 8 5.5 9L4 11.5C4 12 4.5 12.5 5 12.5C5.5 12.5 6 12 6 11.5L7 9.5C7.5 9 8 8.5 8.5 8C9 7.5 9.5 7.5 10 8L11 9.5C11.5 10 12 10.5 12 11.5V14C12 15 11.5 16 11 16.5L9.5 18C9 18.5 8.5 19 8.5 19.5C8.5 20 9 20.5 9.5 20.5C10 20.5 10.5 20 11 19.5L12 18V20.5C12 21 12.5 21.5 13 21.5C13.5 21.5 14 21 14 20.5V18L15 19.5C15.5 20 16 20.5 16.5 20.5C17 20.5 17.5 20 17.5 19.5C17.5 19 17 18.5 16.5 18L15 16.5C14.5 16 14 15 14 14V11.5C14 10.5 14.5 10 15 9.5L16 8C16.5 7.5 17 7.5 17.5 8C18 8.5 18.5 9 19 9.5L20 11.5C20 12 20.5 12.5 21 12.5C21.5 12.5 22 12 22 11.5L20.5 9C20 8 19 7.5 18 7C17 6.5 16 5.5 15.5 4.5L14 2H12M12 4L12.5 5C13 6 13.5 6.5 14 7L15 8L13.5 9.5C13 10 13 10.5 13 11.5V14C13 14.5 13 15 13.5 15.5L14.5 17L13 18.5V16C13 15.5 12.5 15 12 15C11.5 15 11 15.5 11 16V18.5L9.5 17L10.5 15.5C11 15 11 14.5 11 14V11.5C11 10.5 11 10 10.5 9.5L9 8L10 7C10.5 6.5 11 6 11.5 5L12 4Z"/>
  </svg>
);

const Dashboard = () => {
  const [session, setSession] = useState<Session | null>(null);
  const [latestReading, setLatestReading] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);
  const navigate = useNavigate();
  const { toast } = useToast();

  useEffect(() => {
    let mounted = true;

    const initAuth = async () => {
      try {
        const { data: { session }, error } = await supabase.auth.getSession();
        
        if (error) throw error;
        
        if (!session && mounted) {
          navigate("/auth", { replace: true });
          return;
        }
        
        if (mounted) {
          setSession(session);
          setIsLoading(false);
        }
      } catch (error) {
        console.error("Auth error:", error);
        if (mounted) {
          navigate("/auth", { replace: true });
        }
      }
    };

    initAuth();

    const { data: { subscription } } = supabase.auth.onAuthStateChange((_event, session) => {
      if (!session && mounted) {
        navigate("/auth", { replace: true });
      } else if (mounted) {
        setSession(session);
        setIsLoading(false);
      }
    });

    return () => {
      mounted = false;
      subscription.unsubscribe();
    };
  }, [navigate]);

  useEffect(() => {
    if (!session) return;

    const fetchLatestReading = async () => {
      const { data, error } = await supabase
        .from("readings")
        .select("*")
        .order("timestamp", { ascending: false })
        .limit(1)
        .maybeSingle();

      if (error) {
        console.error("Error fetching readings:", error);
        return;
      }
      
      if (data) setLatestReading(data);
    };

    fetchLatestReading();

    const channel = supabase
      .channel("readings-changes")
      .on(
        "postgres_changes",
        { event: "INSERT", schema: "public", table: "readings" },
        (payload) => setLatestReading(payload.new)
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, [session]);

  const handleLogout = async () => {
    await supabase.auth.signOut();
    toast({ title: "Logout realizado com sucesso" });
    navigate("/auth", { replace: true });
  };

  if (isLoading || !session) {
    return (
      <div className="min-h-screen flex items-center justify-center">
        <div className="text-center space-y-4">
          <Activity className="h-12 w-12 animate-spin mx-auto text-primary" />
          <p className="text-muted-foreground">Carregando...</p>
        </div>
      </div>
    );
  }

  return (
    <MqttProvider>
      <div className="min-h-screen bg-gradient-to-br from-background via-secondary/20 to-primary/5">
        <header className="border-b border-primary/20 bg-card/80 backdrop-blur-sm sticky top-0 z-10">
          <div className="container mx-auto px-4 py-4 flex items-center justify-between">
            <div className="flex items-center gap-3">
              <div className="p-2 rounded-lg bg-primary/10">
                <CannabisLeaf />
              </div>
              <div>
                <h1 className="text-xl font-bold text-primary">HydroSmart Crepaldi</h1>
                <p className="text-xs text-muted-foreground">Cannabis de precisão</p>
              </div>
            </div>
            <div className="flex items-center gap-2">
              <MqttStatus />
              <Button onClick={() => navigate('/plants')} variant="outline" size="sm" className="gap-2">
                <Sprout className="h-4 w-4" />
                Plantas
              </Button>
              <Button onClick={() => navigate('/community')} variant="outline" size="sm" className="gap-2">
                <Users className="h-4 w-4" />
                Comunidade
              </Button>
              <Button onClick={handleLogout} variant="outline" size="sm" className="gap-2">
                <LogOut className="h-4 w-4" />
                Sair
              </Button>
            </div>
          </div>
        </header>

        <main className="container mx-auto px-4 py-8">
          <Tabs defaultValue="sensors" className="w-full">
            <TabsList className="grid w-full grid-cols-4 mb-8">
              <TabsTrigger value="sensors" className="gap-2">
                <Activity className="h-4 w-4" />
                Sensores
              </TabsTrigger>
              <TabsTrigger value="charts" className="gap-2">
                <BarChart3 className="h-4 w-4" />
                Gráficos
              </TabsTrigger>
              <TabsTrigger value="ai" className="gap-2">
                <Brain className="h-4 w-4" />
                Análise IA
              </TabsTrigger>
              <TabsTrigger value="relays" className="gap-2">
                <Settings className="h-4 w-4" />
                Controles
              </TabsTrigger>
            </TabsList>

            <TabsContent value="sensors" className="space-y-4">
              <h2 className="text-2xl font-semibold text-foreground flex items-center gap-2">
                <Activity className="h-6 w-6 text-primary" />
                Monitoramento em Tempo Real
              </h2>
              <SensorCard latestReading={latestReading} />
            </TabsContent>

            <TabsContent value="charts" className="space-y-4">
              <h2 className="text-2xl font-semibold text-foreground flex items-center gap-2">
                <BarChart3 className="h-6 w-6 text-primary" />
                Histórico de Leituras
              </h2>
              <SensorCharts />
            </TabsContent>

            <TabsContent value="ai" className="space-y-4">
              <h2 className="text-2xl font-semibold text-foreground flex items-center gap-2">
                <Brain className="h-6 w-6 text-primary" />
                Insights Inteligentes
              </h2>
              <AIInsights />
            </TabsContent>

            <TabsContent value="relays" className="space-y-4">
              <RelayControls />
            </TabsContent>
          </Tabs>
        </main>
      </div>
    </MqttProvider>
  );
};

export default Dashboard;
