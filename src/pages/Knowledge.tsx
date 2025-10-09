import { useState, useEffect } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Session } from "@supabase/supabase-js";
import { FileText, Upload, Brain } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { AppHeader } from "@/components/dashboard/AppHeader";
import { MqttProvider } from "@/contexts/MqttContext";
import { MqttFooter } from "@/components/dashboard/MqttFooter";
import { KnowledgeList } from "@/components/knowledge/KnowledgeList";
import { UploadKnowledgeDialog } from "@/components/knowledge/UploadKnowledgeDialog";
import { AddArticleDialog } from "@/components/knowledge/AddArticleDialog";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Progress } from "@/components/ui/progress";

const Knowledge = () => {
  const [session, setSession] = useState<Session | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [storageUsed, setStorageUsed] = useState(0);
  const [showUploadDialog, setShowUploadDialog] = useState(false);
  const [showArticleDialog, setShowArticleDialog] = useState(false);
  const navigate = useNavigate();
  const { toast } = useToast();

  const STORAGE_LIMIT = 524288000; // 500MB em bytes

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
          await fetchStorageUsage(session.user.id);
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
        if (session) fetchStorageUsage(session.user.id);
        setIsLoading(false);
      }
    });

    return () => {
      mounted = false;
      subscription.unsubscribe();
    };
  }, [navigate]);

  const fetchStorageUsage = async (userId: string) => {
    try {
      const { data, error } = await supabase
        .rpc('get_user_storage_usage', { p_user_id: userId });

      if (error) throw error;
      setStorageUsed(data || 0);
    } catch (error) {
      console.error("Error fetching storage:", error);
    }
  };

  const handleLogout = async () => {
    await supabase.auth.signOut();
    toast({ title: "Logout realizado com sucesso" });
    navigate("/auth", { replace: true });
  };

  const handleNavigate = (path: string) => {
    navigate(path);
  };

  const handleUploadSuccess = async () => {
    if (session) {
      await fetchStorageUsage(session.user.id);
    }
  };

  if (isLoading || !session) {
    return (
      <div className="min-h-screen flex items-center justify-center">
        <div className="text-center space-y-4">
          <Brain className="h-12 w-12 animate-spin mx-auto text-primary" />
          <p className="text-muted-foreground">Carregando...</p>
        </div>
      </div>
    );
  }

  const storagePercentage = (storageUsed / STORAGE_LIMIT) * 100;

  return (
    <MqttProvider>
      <div className="min-h-screen bg-gradient-to-br from-background via-secondary/20 to-primary/5 pb-16">
        <AppHeader onLogout={handleLogout} onNavigate={handleNavigate} />

        <main className="container mx-auto px-4 py-6 space-y-6">
          {/* Header */}
          <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4">
            <div className="flex items-center gap-3">
              <div className="h-12 w-12 rounded-lg bg-primary/10 flex items-center justify-center">
                <Brain className="h-6 w-6 text-primary" />
              </div>
              <div>
                <h1 className="text-2xl sm:text-3xl font-bold">Treinamento da IA</h1>
                <p className="text-sm text-muted-foreground">Personalize a IA com seus próprios materiais</p>
              </div>
            </div>
            <div className="flex gap-2 flex-wrap">
              <Button onClick={() => setShowArticleDialog(true)} variant="outline" className="gap-2">
                <FileText className="h-4 w-4" />
                Adicionar Link
              </Button>
              <Button onClick={() => setShowUploadDialog(true)} className="gap-2">
                <Upload className="h-4 w-4" />
                Upload de Arquivo
              </Button>
            </div>
          </div>

          {/* Storage Usage */}
          <Card>
            <CardHeader>
              <CardTitle className="text-sm font-medium">Armazenamento Utilizado</CardTitle>
              <CardDescription>
                {(storageUsed / 1048576).toFixed(2)} MB de {(STORAGE_LIMIT / 1048576).toFixed(0)} MB
              </CardDescription>
            </CardHeader>
            <CardContent>
              <Progress value={storagePercentage} className="h-2" />
              {storagePercentage > 90 && (
                <p className="text-xs text-destructive mt-2">
                  ⚠️ Você está próximo do limite de armazenamento
                </p>
              )}
            </CardContent>
          </Card>

          {/* Knowledge List */}
          <KnowledgeList onStorageUpdate={handleUploadSuccess} />
        </main>

        <MqttFooter />

        <UploadKnowledgeDialog 
          open={showUploadDialog}
          onOpenChange={setShowUploadDialog}
          onSuccess={handleUploadSuccess}
        />

        <AddArticleDialog
          open={showArticleDialog}
          onOpenChange={setShowArticleDialog}
          onSuccess={handleUploadSuccess}
        />
      </div>
    </MqttProvider>
  );
};

export default Knowledge;
