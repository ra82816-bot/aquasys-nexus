import { useState, useEffect } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Session } from "@supabase/supabase-js";
import { Button } from "@/components/ui/button";
import { LogOut, Upload, BookOpen, FileText, Link as LinkIcon } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { KnowledgeList } from "@/components/knowledge/KnowledgeList";
import { UploadKnowledgeDialog } from "@/components/knowledge/UploadKnowledgeDialog";
import { AddArticleDialog } from "@/components/knowledge/AddArticleDialog";
import hydroSmartLogo from "@/assets/hydro-smart-logo.webp";

const Knowledge = () => {
  const [session, setSession] = useState<Session | null>(null);
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
      if (mounted) {
        setSession(session);
        if (!session) {
          navigate("/auth", { replace: true });
        }
      }
    });

    return () => {
      mounted = false;
      subscription.unsubscribe();
    };
  }, [navigate]);

  const handleLogout = async () => {
    await supabase.auth.signOut();
    navigate("/auth");
  };

  if (isLoading || !session) {
    return (
      <div className="min-h-screen flex items-center justify-center bg-background">
        <div className="text-center">
          <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary mx-auto"></div>
          <p className="mt-4 text-muted-foreground">Carregando...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-background">
      {/* Header */}
      <header className="border-b bg-card/50 backdrop-blur supports-[backdrop-filter]:bg-card/50 sticky top-0 z-50">
        <div className="container mx-auto px-4 py-3">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <img 
                src={hydroSmartLogo} 
                alt="Hydro Smart" 
                className="h-10 w-10 object-contain"
              />
              <div>
                <h1 className="text-xl font-bold bg-gradient-to-r from-primary to-primary/60 bg-clip-text text-transparent">
                  Base de Conhecimento
                </h1>
                <p className="text-xs text-muted-foreground">
                  Gerencie artigos e documentos
                </p>
              </div>
            </div>

            <Button
              variant="ghost"
              size="sm"
              onClick={handleLogout}
              className="gap-2"
            >
              <LogOut className="h-4 w-4" />
              Sair
            </Button>
          </div>
        </div>
      </header>

      {/* Navigation Tabs */}
      <div className="border-b bg-card/30">
        <div className="container mx-auto px-4">
          <Tabs defaultValue="knowledge" className="w-full">
            <TabsList className="w-full justify-start h-12 bg-transparent border-0 rounded-none gap-1">
              <TabsTrigger 
                value="dashboard" 
                onClick={() => navigate("/dashboard")}
                className="data-[state=active]:bg-background data-[state=active]:shadow-sm rounded-t-lg"
              >
                <BookOpen className="h-4 w-4 mr-2" />
                Dashboard
              </TabsTrigger>
              <TabsTrigger 
                value="plants"
                onClick={() => navigate("/plants")}
                className="data-[state=active]:bg-background data-[state=active]:shadow-sm rounded-t-lg"
              >
                <FileText className="h-4 w-4 mr-2" />
                Plantas
              </TabsTrigger>
              <TabsTrigger 
                value="community"
                onClick={() => navigate("/community")}
                className="data-[state=active]:bg-background data-[state=active]:shadow-sm rounded-t-lg"
              >
                <LinkIcon className="h-4 w-4 mr-2" />
                Comunidade
              </TabsTrigger>
              <TabsTrigger 
                value="knowledge"
                className="data-[state=active]:bg-background data-[state=active]:shadow-sm rounded-t-lg"
              >
                <BookOpen className="h-4 w-4 mr-2" />
                Conhecimento
              </TabsTrigger>
            </TabsList>
          </Tabs>
        </div>
      </div>

      {/* Main Content */}
      <main className="container mx-auto px-4 py-6">
        <div className="mb-6 flex flex-col sm:flex-row gap-4 justify-between items-start sm:items-center">
          <div>
            <h2 className="text-2xl font-bold">Base de Conhecimento</h2>
            <p className="text-muted-foreground">
              Adicione artigos científicos, PDFs e guias para melhorar as análises de IA
            </p>
          </div>
          
          <div className="flex gap-2">
            <UploadKnowledgeDialog />
            <AddArticleDialog />
          </div>
        </div>

        <KnowledgeList />
      </main>
    </div>
  );
};

export default Knowledge;
