import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Session } from "@supabase/supabase-js";
import { Button } from "@/components/ui/button";
import { LogOut, Droplets, Thermometer, Wind, Zap, Activity } from "lucide-react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { useToast } from "@/hooks/use-toast";
import { SensorCard } from "@/components/dashboard/SensorCard";
import { RelayControls } from "@/components/dashboard/RelayControls";
import { TestDataButton } from "@/components/dashboard/TestDataButton";

const CannabisLeaf = () => (
  <svg viewBox="0 0 24 24" fill="currentColor" className="h-6 w-6">
    <path d="M12 2C11.5 2 11 2.19 10.59 2.59C10.2 3 10 3.5 10 4V6.17C9.33 6.06 8.66 6 8 6C7.34 6 6.67 6.06 6 6.17V4C6 3.5 5.81 3 5.41 2.59C5 2.19 4.5 2 4 2C3.5 2 3 2.19 2.59 2.59C2.19 3 2 3.5 2 4V8C2 8.88 2.29 9.67 2.76 10.34C3.24 11 3.88 11.5 4.67 11.84C4.23 12.41 4 13.17 4 14C4 14.83 4.23 15.59 4.67 16.16C3.88 16.5 3.24 17 2.76 17.66C2.29 18.33 2 19.12 2 20V24H6C6.88 24 7.67 23.71 8.34 23.24C9 22.76 9.5 22.12 9.84 21.33C10.41 21.77 11.17 22 12 22C12.83 22 13.59 21.77 14.16 21.33C14.5 22.12 15 22.76 15.66 23.24C16.33 23.71 17.12 24 18 24H22V20C22 19.12 21.71 18.33 21.24 17.66C20.76 17 20.12 16.5 19.33 16.16C19.77 15.59 20 14.83 20 14C20 13.17 19.77 12.41 19.33 11.84C20.12 11.5 20.76 11 21.24 10.34C21.71 9.67 22 8.88 22 8V4C22 3.5 21.81 3 21.41 2.59C21 2.19 20.5 2 20 2C19.5 2 19 2.19 18.59 2.59C18.19 3 18 3.5 18 4V6.17C17.33 6.06 16.66 6 16 6C15.34 6 14.67 6.06 14 6.17V4C14 3.5 13.81 3 13.41 2.59C13 2.19 12.5 2 12 2M12 8C13.1 8 14 8.9 14 10V14C14 15.1 13.1 16 12 16C10.9 16 10 15.1 10 14V10C10 8.9 10.9 8 12 8Z"/>
  </svg>
);

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
              <CannabisLeaf />
            </div>
            <div>
              <h1 className="text-xl font-bold text-primary">HydroSmart Crepaldi</h1>
              <p className="text-xs text-muted-foreground">Cannabis de precisão</p>
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

        <RelayControls />
      </main>
    </div>
  );
};

export default Dashboard;