import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { supabase } from "@/integrations/supabase/client";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Plus, ArrowLeft, TrendingUp, MessageCircle } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { ForumPostList } from "@/components/community/ForumPostList";
import { CreatePostDialog } from "@/components/community/CreatePostDialog";

const Community = () => {
  const navigate = useNavigate();
  const { toast } = useToast();
  const [user, setUser] = useState<any>(null);
  const [loading, setLoading] = useState(true);
  const [createPostOpen, setCreatePostOpen] = useState(false);

  useEffect(() => {
    checkUser();
  }, []);

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

  if (loading) {
    return (
      <div className="min-h-screen flex items-center justify-center bg-background">
        <div className="text-center">
          <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary mx-auto mb-4"></div>
          <p className="text-muted-foreground">Carregando comunidade...</p>
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
                <h1 className="text-2xl font-bold">Comunidade AquaSys</h1>
                <p className="text-sm text-muted-foreground">
                  Compartilhe experiências e aprenda com outros cultivadores
                </p>
              </div>
            </div>
            <Button onClick={() => setCreatePostOpen(true)}>
              <Plus className="mr-2 h-4 w-4" />
              Nova Publicação
            </Button>
          </div>
        </div>
      </header>

      <main className="container mx-auto px-4 py-8">
        <div className="grid gap-6 md:grid-cols-3 mb-8">
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Posts Ativos</CardTitle>
              <MessageCircle className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">Carregando...</div>
              <p className="text-xs text-muted-foreground">
                Discussões em andamento
              </p>
            </CardContent>
          </Card>
          
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Dados Compartilhados</CardTitle>
              <TrendingUp className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">Carregando...</div>
              <p className="text-xs text-muted-foreground">
                Cultivos anônimos na base
              </p>
            </CardContent>
          </Card>

          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Membros Ativos</CardTitle>
              <MessageCircle className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">Carregando...</div>
              <p className="text-xs text-muted-foreground">
                Contribuindo esta semana
              </p>
            </CardContent>
          </Card>
        </div>

        <Tabs defaultValue="recentes" className="space-y-6">
          <TabsList>
            <TabsTrigger value="recentes">Recentes</TabsTrigger>
            <TabsTrigger value="populares">Populares</TabsTrigger>
            <TabsTrigger value="meus-posts">Minhas Publicações</TabsTrigger>
          </TabsList>

          <TabsContent value="recentes">
            <ForumPostList filter="recent" />
          </TabsContent>

          <TabsContent value="populares">
            <ForumPostList filter="popular" />
          </TabsContent>

          <TabsContent value="meus-posts">
            <ForumPostList filter="my-posts" userId={user?.id} />
          </TabsContent>
        </Tabs>
      </main>

      <CreatePostDialog 
        open={createPostOpen} 
        onOpenChange={setCreatePostOpen}
        userId={user?.id}
      />
    </div>
  );
};

export default Community;
