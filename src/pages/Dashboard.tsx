import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Session } from "@supabase/supabase-js";
import { Activity, BarChart3, Settings, Brain } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { SensorCard } from "@/components/dashboard/SensorCard";
import { SensorCharts } from "@/components/dashboard/SensorCharts";
import { RelayControls } from "@/components/dashboard/RelayControls";
import { AIInsights } from "@/components/dashboard/AIInsights";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { DashboardStats } from "@/components/dashboard/DashboardStats";
import { AlertsPanel } from "@/components/dashboard/AlertsPanel";

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
    <div className="min-h-screen bg-gradient-to-br from-background via-secondary/20 to-primary/5">
      <AppHeader onLogout={handleLogout} onNavigate={navigate} />

      <main className="container mx-auto px-4 py-6 space-y-6">
        {/* Dashboard Stats */}
        <DashboardStats latestReading={latestReading} />

        {/* Alerts Panel */}
        <AlertsPanel />

        {/* Main Tabs */}
        <Tabs defaultValue="sensors" className="w-full">
          <TabsList className="grid w-full grid-cols-2 lg:grid-cols-4 mb-6">
            <TabsTrigger value="sensors" className="gap-2">
              <Activity className="h-4 w-4" />
              <span className="hidden sm:inline">Sensores</span>
            </TabsTrigger>
            <TabsTrigger value="charts" className="gap-2">
              <BarChart3 className="h-4 w-4" />
              <span className="hidden sm:inline">Gráficos</span>
            </TabsTrigger>
            <TabsTrigger value="ai" className="gap-2">
              <Brain className="h-4 w-4" />
              <span className="hidden sm:inline">Análise IA</span>
            </TabsTrigger>
            <TabsTrigger value="relays" className="gap-2">
              <Settings className="h-4 w-4" />
              <span className="hidden sm:inline">Controles</span>
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
  );
};

export default Dashboard;
