import { useEffect } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Button } from "@/components/ui/button";
import { Leaf } from "lucide-react";

const Index = () => {
  const navigate = useNavigate();

  useEffect(() => {
    supabase.auth.getSession().then(({ data: { session } }) => {
      if (session) navigate("/dashboard");
    });
  }, [navigate]);

  return (
    <div className="flex min-h-screen items-center justify-center bg-gradient-to-br from-background via-secondary/30 to-primary/10">
      <div className="text-center space-y-8 px-4">
        <div className="flex justify-center">
          <div className="p-6 rounded-full bg-primary/10 animate-pulse">
            <Leaf className="h-24 w-24 text-primary" />
          </div>
        </div>
        <div>
          <h1 className="mb-4 text-5xl font-bold text-primary">AquaSys</h1>
          <p className="text-xl text-muted-foreground mb-8">
            Sistema de Monitoramento Hidropônico Inteligente
          </p>
        </div>
        <Button size="lg" onClick={() => navigate("/auth")} className="text-lg px-8">
          Acessar Sistema
        </Button>
      </div>
    </div>
  );
};

export default Index;
