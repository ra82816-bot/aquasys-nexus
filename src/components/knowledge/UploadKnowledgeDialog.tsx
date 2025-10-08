import { useState } from "react";
import { Button } from "@/components/ui/button";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { Textarea } from "@/components/ui/textarea";
import { Upload, Loader2 } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { supabase } from "@/integrations/supabase/client";

export const UploadKnowledgeDialog = () => {
  const [open, setOpen] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [file, setFile] = useState<File | null>(null);
  const [title, setTitle] = useState("");
  const [contentType, setContentType] = useState<"scientific_paper" | "guide" | "manual" | "research">("scientific_paper");
  const [topics, setTopics] = useState("");
  const { toast } = useToast();

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    if (e.target.files && e.target.files[0]) {
      const selectedFile = e.target.files[0];
      if (selectedFile.type !== 'application/pdf') {
        toast({
          title: "Erro",
          description: "Por favor, selecione um arquivo PDF",
          variant: "destructive",
        });
        return;
      }
      setFile(selectedFile);
      if (!title) {
        setTitle(selectedFile.name.replace('.pdf', ''));
      }
    }
  };

  const handleUpload = async () => {
    if (!file || !title) {
      toast({
        title: "Erro",
        description: "Por favor, selecione um arquivo e forneça um título",
        variant: "destructive",
      });
      return;
    }

    setUploading(true);

    try {
      // Read file as text (simplified - in production use proper PDF parser)
      const text = await file.text();
      
      const topicsArray = topics ? topics.split(',').map(t => t.trim()) : [];

      // Insert into knowledge_base
      const { data, error } = await supabase
        .from('knowledge_base')
        .insert({
          title,
          content_type: contentType,
          original_content: text,
          topics: topicsArray,
          processing_status: 'pending',
        })
        .select()
        .single();

      if (error) throw error;

      // Call processing function
      const { error: processError } = await supabase.functions.invoke('process-knowledge', {
        body: {
          knowledgeId: data.id,
          content: text,
          title,
          contentType,
        }
      });

      if (processError) {
        console.error('Processing error:', processError);
      }

      toast({
        title: "Sucesso!",
        description: "Documento adicionado e será processado em breve",
      });

      setOpen(false);
      setFile(null);
      setTitle("");
      setTopics("");
    } catch (error) {
      console.error('Upload error:', error);
      toast({
        title: "Erro",
        description: "Não foi possível fazer upload do documento",
        variant: "destructive",
      });
    } finally {
      setUploading(false);
    }
  };

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button className="gap-2">
          <Upload className="h-4 w-4" />
          Upload PDF
        </Button>
      </DialogTrigger>
      <DialogContent className="sm:max-w-[500px]">
        <DialogHeader>
          <DialogTitle>Upload de Documento</DialogTitle>
          <DialogDescription>
            Faça upload de PDFs científicos ou guias técnicos
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-4 py-4">
          <div className="space-y-2">
            <Label htmlFor="file">Arquivo PDF</Label>
            <Input
              id="file"
              type="file"
              accept=".pdf"
              onChange={handleFileChange}
              disabled={uploading}
            />
            {file && (
              <p className="text-xs text-muted-foreground">
                Arquivo selecionado: {file.name}
              </p>
            )}
          </div>

          <div className="space-y-2">
            <Label htmlFor="title">Título</Label>
            <Input
              id="title"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              placeholder="Ex: Guia de cultivo hidropônico"
              disabled={uploading}
            />
          </div>

          <div className="space-y-2">
            <Label htmlFor="contentType">Tipo de Conteúdo</Label>
            <Select value={contentType} onValueChange={(value) => setContentType(value as typeof contentType)} disabled={uploading}>
              <SelectTrigger id="contentType">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="scientific_paper">Artigo Científico</SelectItem>
                <SelectItem value="guide">Guia de Cultivo</SelectItem>
                <SelectItem value="manual">Manual Técnico</SelectItem>
                <SelectItem value="research">Trabalho de Pesquisa</SelectItem>
              </SelectContent>
            </Select>
          </div>

          <div className="space-y-2">
            <Label htmlFor="topics">Tópicos (separados por vírgula)</Label>
            <Textarea
              id="topics"
              value={topics}
              onChange={(e) => setTopics(e.target.value)}
              placeholder="Ex: hidroponia, nutrientes, pH"
              disabled={uploading}
            />
          </div>
        </div>

        <div className="flex justify-end gap-2">
          <Button variant="outline" onClick={() => setOpen(false)} disabled={uploading}>
            Cancelar
          </Button>
          <Button onClick={handleUpload} disabled={uploading || !file || !title}>
            {uploading ? (
              <>
                <Loader2 className="mr-2 h-4 w-4 animate-spin" />
                Processando...
              </>
            ) : (
              "Upload"
            )}
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  );
};
