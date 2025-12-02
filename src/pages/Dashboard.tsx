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
import { PhControlPanel } from "@/components/dashboard/PhControlPanel";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { MqttFooter } from "@/components/dashboard/MqttFooter";

const Dashboard = () => {
  const [session, setSession] = useState<Session | null>(null);
  const [latestReading, setLatestReading] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [currentTab, setCurrentTab] = useState("sensors");
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

  const handleNavigate = (path: string) => {
    navigate(path);
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
    <div className="min-h-screen bg-gradient-to-br from-background via-secondary/20 to-primary/5 pb-16">
      <AppHeader 
        onLogout={handleLogout} 
        onNavigate={handleNavigate}
        currentTab={currentTab}
        onTabChange={setCurrentTab}
      />

      <main className="container mx-auto px-3 sm:px-4 py-4 sm:py-6">
        <Tabs value={currentTab} onValueChange={setCurrentTab} className="w-full">
          <TabsContent value="sensors" className="space-y-3 sm:space-y-4 mt-0">
            <h2 className="text-lg sm:text-2xl font-semibold text-foreground flex items-center gap-2">
              <Activity className="h-5 w-5 sm:h-6 sm:w-6 text-primary" />
              <span className="text-base sm:text-2xl">Monitoramento em Tempo Real</span>
            </h2>
            <SensorCard latestReading={latestReading} />
          </TabsContent>

          <TabsContent value="charts" className="space-y-3 sm:space-y-4 mt-0">
            <h2 className="text-lg sm:text-2xl font-semibold text-foreground flex items-center gap-2">
              <BarChart3 className="h-5 w-5 sm:h-6 sm:w-6 text-primary" />
              <span className="text-base sm:text-2xl">Histórico de Leituras</span>
            </h2>
            <SensorCharts />
          </TabsContent>

          <TabsContent value="ai" className="space-y-3 sm:space-y-4 mt-0">
            <h2 className="text-lg sm:text-2xl font-semibold text-foreground flex items-center gap-2">
              <Brain className="h-5 w-5 sm:h-6 sm:w-6 text-primary" />
              <span className="text-base sm:text-2xl">Insights Inteligentes</span>
            </h2>
            <AIInsights />
          </TabsContent>

          <TabsContent value="relays" className="space-y-3 sm:space-y-4 mt-0">
            <RelayControls />
            <PhControlPanel />
          </TabsContent>
        </Tabs>
      </main>

      <MqttFooter />
    </div>
  );
};

export default Dashboard;
