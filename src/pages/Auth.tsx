import { useState, useEffect } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { useToast } from "@/hooks/use-toast";
import { Leaf } from "lucide-react";

const Auth = () => {
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [deviceUuid, setDeviceUuid] = useState("");
  const [isLogin, setIsLogin] = useState(true);
  const [loading, setLoading] = useState(false);
  const navigate = useNavigate();
  const { toast } = useToast();

  useEffect(() => {
    supabase.auth.getSession().then(({ data: { session } }) => {
      if (session) navigate("/dashboard");
    });
  }, [navigate]);

  const handleAuth = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);

    try {
      if (isLogin) {
        const { error } = await supabase.auth.signInWithPassword({ email, password });
        if (error) throw error;
        toast({ title: "Login realizado com sucesso!" });
        navigate("/dashboard");
      } else {
        // Validar UUID do dispositivo
        if (!deviceUuid.trim()) {
          throw new Error("UUID do dispositivo é obrigatório para criar uma conta");
        }

        // Verificar se o dispositivo existe
        const { data: device, error: deviceError } = await supabase
          .from("devices")
          .select("id, device_uuid")
          .eq("device_uuid", deviceUuid.trim())
          .maybeSingle();

        if (deviceError) throw deviceError;
        if (!device) {
          throw new Error("Dispositivo não encontrado. Verifique o UUID e tente novamente.");
        }

        // Verificar se o dispositivo já está vinculado
        const { data: existingOwner } = await supabase
          .from("device_owners")
          .select("user_id")
          .eq("device_id", device.id)
          .maybeSingle();

        if (existingOwner) {
          throw new Error("Este dispositivo já está vinculado a outra conta.");
        }

        // Criar conta
        const { data: authData, error: signUpError } = await supabase.auth.signUp({
          email,
          password,
          options: { emailRedirectTo: `${window.location.origin}/dashboard` }
        });

        if (signUpError) throw signUpError;
        if (!authData.user) throw new Error("Erro ao criar conta");

        // Vincular dispositivo automaticamente
        const { error: pairError } = await supabase
          .from("device_owners")
          .insert({
            device_id: device.id,
            user_id: authData.user.id
          });

        if (pairError) throw pairError;

        toast({ 
          title: "Conta criada com sucesso!", 
          description: "Dispositivo vinculado automaticamente." 
        });
        navigate("/dashboard");
      }
    } catch (error: any) {
      toast({
        title: "Erro",
        description: error.message,
        variant: "destructive"
      });
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-background via-secondary/30 to-primary/10 p-4">
      <Card className="w-full max-w-md border-primary/20 shadow-xl">
        <CardHeader className="text-center space-y-4">
          <div className="flex justify-center">
            <div className="p-4 rounded-full bg-primary/10">
              <Leaf className="h-12 w-12 text-primary" />
            </div>
          </div>
          <CardTitle className="text-3xl font-bold text-primary">AquaSys</CardTitle>
          <CardDescription className="text-base">
            Sistema de Monitoramento Hidropônico
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form onSubmit={handleAuth} className="space-y-4">
            <div className="space-y-2">
              <Label htmlFor="email">Email</Label>
              <Input
                id="email"
                type="email"
                placeholder="seu@email.com"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                required
                className="border-primary/20 focus:border-primary"
              />
            </div>
            <div className="space-y-2">
              <Label htmlFor="password">Senha</Label>
              <Input
                id="password"
                type="password"
                placeholder="••••••••"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                required
                className="border-primary/20 focus:border-primary"
              />
            </div>
            {!isLogin && (
              <div className="space-y-2">
                <Label htmlFor="deviceUuid">UUID do Dispositivo</Label>
                <Input
                  id="deviceUuid"
                  type="text"
                  placeholder="Ex: ESP32-SENSOR-ABC123"
                  value={deviceUuid}
                  onChange={(e) => setDeviceUuid(e.target.value)}
                  required={!isLogin}
                  className="border-primary/20 focus:border-primary font-mono text-sm"
                />
                <p className="text-xs text-muted-foreground">
                  Digite o UUID do seu módulo ESP32 para vincular à sua conta
                </p>
              </div>
            )}
            <Button type="submit" className="w-full" disabled={loading}>
              {loading ? "Processando..." : isLogin ? "Entrar" : "Criar Conta"}
            </Button>
            <Button
              type="button"
              variant="ghost"
              className="w-full"
              onClick={() => setIsLogin(!isLogin)}
            >
              {isLogin ? "Criar nova conta" : "Já tenho uma conta"}
            </Button>
          </form>
        </CardContent>
      </Card>
    </div>
  );
};

export default Auth;