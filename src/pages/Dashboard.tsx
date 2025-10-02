import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Session } from "@supabase/supabase-js";
import { Button } from "@/components/ui/button";
import { Leaf, LogOut, Droplets, Thermometer, Wind, Zap, Activity } from "lucide-react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { useToast } from "@/hooks/use-toast";
import { SensorCard } from "@/components/dashboard/SensorCard";
import { RelayControls } from "@/components/dashboard/RelayControls";
import { TestDataButton } from "@/components/dashboard/TestDataButton";
import { SensorHistoryChart } from "@/components/dashboard/SensorHistoryChart";

const Dashboard = () => {
  const [session, setSession] = useState<Session | null>(null);
  const [latestReading, setLatestReading] = useState<any>(null);
  const navigate = useNavigate();
  const { toast } = useToast();

  useEffect(() => {
    supabase.auth.getSession().then(({ data: { session } }) => {
      setSession(session);
      if (!session) navigate("/auth");
    });

    const { data: { subscription } } = supabase.auth.onAuthStateChange((_event, session) => {
      setSession(session);
      if (!session) navigate("/auth");
    });

    return () => subscription.unsubscribe();
  }, [navigate]);

  useEffect(() => {
    if (!session) return;

    const fetchLatestReading = async () => {
      const { data, error } = await supabase
        .from("readings")
        .select("*")
        .order("timestamp", { ascending: false })
        .limit(1)
        .single();

      if (error && error.code !== "PGRST116") {
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
    navigate("/auth");
  };

  if (!session) return null;

  return (
    <div className="min-h-screen bg-gradient-to-br from-background via-secondary/20 to-primary/5">
      <header className="border-b border-primary/20 bg-card/80 backdrop-blur-sm sticky top-0 z-10">
        <div className="container mx-auto px-4 py-4 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="p-2 rounded-lg bg-primary/10">
              <Leaf className="h-6 w-6 text-primary" />
            </div>
            <div>
              <h1 className="text-xl font-bold text-primary">AquaSys</h1>
              <p className="text-xs text-muted-foreground">Sistema Hidropônico</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <TestDataButton />
            <Button onClick={handleLogout} variant="outline" size="sm" className="gap-2">
              <LogOut className="h-4 w-4" />
              Sair
            </Button>
          </div>
        </div>
      </header>

      <main className="container mx-auto px-4 py-8 space-y-8">
        <section>
          <h2 className="text-2xl font-semibold mb-4 text-foreground flex items-center gap-2">
            <Activity className="h-6 w-6 text-primary" />
            Monitoramento em Tempo Real
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
            <SensorCard
              title="pH"
              value={latestReading?.ph}
              icon={<Droplets className="h-5 w-5" />}
              unit=""
              color="text-blue-600"
              bgColor="bg-blue-50"
            />
            <SensorCard
              title="EC"
              value={latestReading?.ec}
              icon={<Zap className="h-5 w-5" />}
              unit="µS/cm"
              color="text-yellow-600"
              bgColor="bg-yellow-50"
            />
            <SensorCard
              title="Temperatura do Ar"
              value={latestReading?.air_temp}
              icon={<Thermometer className="h-5 w-5" />}
              unit="°C"
              color="text-orange-600"
              bgColor="bg-orange-50"
            />
            <SensorCard
              title="Umidade"
              value={latestReading?.humidity}
              icon={<Wind className="h-5 w-5" />}
              unit="%"
              color="text-cyan-600"
              bgColor="bg-cyan-50"
            />
            <SensorCard
              title="Temperatura da Água"
              value={latestReading?.water_temp}
              icon={<Thermometer className="h-5 w-5" />}
              unit="°C"
              color="text-teal-600"
              bgColor="bg-teal-50"
            />
          </div>
        </section>

        <section>
          <h2 className="text-2xl font-semibold mb-4 text-foreground">Histórico de Sensores</h2>
          <div className="grid grid-cols-1 gap-4">
            <SensorHistoryChart
              sensorKey="ph"
              title="pH"
              unit=""
              color="#2563eb"
            />
            <SensorHistoryChart
              sensorKey="ec"
              title="Condutividade Elétrica"
              unit="µS/cm"
              color="#ca8a04"
            />
            <SensorHistoryChart
              sensorKey="air_temp"
              title="Temperatura do Ar"
              unit="°C"
              color="#ea580c"
            />
            <SensorHistoryChart
              sensorKey="humidity"
              title="Umidade"
              unit="%"
              color="#0891b2"
            />
            <SensorHistoryChart
              sensorKey="water_temp"
              title="Temperatura da Água"
              unit="°C"
              color="#0d9488"
            />
          </div>
        </section>

        <RelayControls />
      </main>
    </div>
  );
};

export default Dashboard;