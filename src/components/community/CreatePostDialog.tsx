import { useState } from "react";
import { Dialog, DialogContent, DialogDescription, DialogHeader, DialogTitle } from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { Label } from "@/components/ui/label";
import { Switch } from "@/components/ui/switch";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";

interface CreatePostDialogProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  userId?: string;
}

export const CreatePostDialog = ({ open, onOpenChange, userId }: CreatePostDialogProps) => {
  const [title, setTitle] = useState("");
  const [content, setContent] = useState("");
  const [isAnonymous, setIsAnonymous] = useState(false);
  const [loading, setLoading] = useState(false);
  const { toast } = useToast();

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    
    if (!userId) {
      toast({
        variant: "destructive",
        title: "Erro",
        description: "Você precisa estar autenticado",
      });
      return;
    }

    if (!title.trim() || !content.trim()) {
      toast({
        variant: "destructive",
        title: "Campos obrigatórios",
        description: "Preencha título e conteúdo",
      });
      return;
    }

    try {
      setLoading(true);

      const { error } = await supabase
        .from('forum_posts')
        .insert({
          user_id: userId,
          title: title.trim(),
          content: content.trim(),
          is_anonymous: isAnonymous,
        });

      if (error) throw error;

      toast({
        title: "Publicação criada!",
        description: "Sua publicação foi compartilhada com a comunidade.",
      });

      setTitle("");
      setContent("");
      setIsAnonymous(false);
      onOpenChange(false);
      
      // Refresh the page to show new post
      window.location.reload();
    } catch (error: any) {
      toast({
        variant: "destructive",
        title: "Erro ao criar publicação",
        description: error.message,
      });
    } finally {
      setLoading(false);
    }
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-2xl">
        <DialogHeader>
          <DialogTitle>Nova Publicação</DialogTitle>
          <DialogDescription>
            Compartilhe suas experiências, dúvidas ou descobertas com a comunidade
          </DialogDescription>
        </DialogHeader>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div>
            <Label htmlFor="title">Título</Label>
            <Input
              id="title"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              placeholder="Ex: Problemas com pH na fase de floração"
              required
            />
          </div>

          <div>
            <Label htmlFor="content">Conteúdo</Label>
            <Textarea
              id="content"
              value={content}
              onChange={(e) => setContent(e.target.value)}
              placeholder="Descreva sua experiência, dúvida ou compartilhe seus dados..."
              rows={8}
              required
            />
          </div>

          <div className="flex items-center justify-between p-4 border rounded-lg">
            <div>
              <Label htmlFor="anonymous">Publicar anonimamente</Label>
              <p className="text-sm text-muted-foreground">
                Sua identidade não será revelada
              </p>
            </div>
            <Switch
              id="anonymous"
              checked={isAnonymous}
              onCheckedChange={setIsAnonymous}
            />
          </div>

          <div className="flex justify-end gap-2">
            <Button
              type="button"
              variant="outline"
              onClick={() => onOpenChange(false)}
              disabled={loading}
            >
              Cancelar
            </Button>
            <Button type="submit" disabled={loading}>
              {loading ? "Publicando..." : "Publicar"}
            </Button>
          </div>
        </form>
      </DialogContent>
    </Dialog>
  );
};
