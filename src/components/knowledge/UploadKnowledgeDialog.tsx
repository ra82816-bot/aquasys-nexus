import { useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { useToast } from "@/hooks/use-toast";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Textarea } from "@/components/ui/textarea";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Checkbox } from "@/components/ui/checkbox";
import { Upload, Loader2 } from "lucide-react";

interface UploadKnowledgeDialogProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  onSuccess: () => void;
}

export const UploadKnowledgeDialog = ({ open, onOpenChange, onSuccess }: UploadKnowledgeDialogProps) => {
  const { toast } = useToast();
  const [isLoading, setIsLoading] = useState(false);
  const [file, setFile] = useState<File | null>(null);
  const [title, setTitle] = useState("");
  const [description, setDescription] = useState("");
  const [category, setCategory] = useState("");
  const [isTrusted, setIsTrusted] = useState(false);

  const categories = [
    "Nutrientes",
    "Fotoperíodo",
    "Controle de pH",
    "Controle de EC",
    "Pragas e Doenças",
    "Genética",
    "Cultivo Geral",
    "Outro"
  ];

  const allowedTypes = [
    'application/pdf',
    'text/plain',
    'text/csv',
    'application/vnd.openxmlformats-officedocument.wordprocessingml.document',
    'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
    'application/msword',
    'application/vnd.ms-excel'
  ];

  const getFileType = (file: File): string => {
    const ext = file.name.split('.').pop()?.toLowerCase();
    const typeMap: Record<string, string> = {
      'pdf': 'pdf',
      'txt': 'txt',
      'csv': 'csv',
      'docx': 'docx',
      'doc': 'docx',
      'xlsx': 'xlsx',
      'xls': 'xlsx'
    };
    return typeMap[ext || ''] || 'txt';
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const selectedFile = e.target.files?.[0];
    if (selectedFile) {
      if (selectedFile.size > 52428800) { // 50MB
        toast({
          title: "Arquivo muito grande",
          description: "O tamanho máximo permitido é 50MB",
          variant: "destructive"
        });
        return;
      }

      if (!allowedTypes.includes(selectedFile.type)) {
        toast({
          title: "Tipo de arquivo não suportado",
          description: "Por favor, envie PDF, TXT, CSV, DOCX ou XLSX",
          variant: "destructive"
        });
        return;
      }

      setFile(selectedFile);
      if (!title) {
        setTitle(selectedFile.name);
      }
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!file) return;

    setIsLoading(true);

    try {
      const { data: { user } } = await supabase.auth.getUser();
      if (!user) throw new Error("User not authenticated");

      // Upload do arquivo
      const filePath = `${user.id}/${Date.now()}_${file.name}`;
      const { error: uploadError } = await supabase.storage
        .from('knowledge-materials')
        .upload(filePath, file);

      if (uploadError) throw uploadError;

      // Criar registro no banco
      const { data: knowledge, error: dbError } = await supabase
        .from('knowledge')
        .insert({
          user_id: user.id,
          title,
          description,
          category,
          source_type: getFileType(file),
          is_trusted: isTrusted,
          file_path: filePath,
          file_size: file.size,
          processing_status: 'pending'
        })
        .select()
        .single();

      if (dbError) throw dbError;

      // Processar em background
      supabase.functions.invoke('process-knowledge-upload', {
        body: { knowledgeId: knowledge.id }
      });

      toast({
        title: "Upload realizado",
        description: "O material está sendo processado pela IA"
      });

      setTitle("");
      setDescription("");
      setCategory("");
      setFile(null);
      setIsTrusted(false);
      onSuccess();
      onOpenChange(false);

    } catch (error) {
      console.error("Error uploading:", error);
      toast({
        title: "Erro no upload",
        description: error instanceof Error ? error.message : "Erro desconhecido",
        variant: "destructive"
      });
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-xl max-h-[90vh] overflow-y-auto">
        <DialogHeader>
          <DialogTitle>Upload de Material de Treinamento</DialogTitle>
          <DialogDescription>
            Envie documentos, PDFs ou planilhas para treinar a IA com seus próprios materiais de referência
          </DialogDescription>
        </DialogHeader>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div>
            <Label htmlFor="file">Arquivo (máx. 50MB)</Label>
            <Input
              id="file"
              type="file"
              accept=".pdf,.txt,.csv,.doc,.docx,.xls,.xlsx"
              onChange={handleFileChange}
              disabled={isLoading}
              required
            />
            {file && (
              <p className="text-xs text-muted-foreground mt-1">
                {file.name} ({(file.size / 1048576).toFixed(2)} MB)
              </p>
            )}
          </div>

          <div>
            <Label htmlFor="title">Título do Material</Label>
            <Input
              id="title"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              disabled={isLoading}
              required
            />
          </div>

          <div>
            <Label htmlFor="description">Descrição</Label>
            <Textarea
              id="description"
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              disabled={isLoading}
              rows={3}
              placeholder="Descreva brevemente o conteúdo deste material..."
            />
          </div>

          <div>
            <Label htmlFor="category">Categoria</Label>
            <Select value={category} onValueChange={setCategory} disabled={isLoading} required>
              <SelectTrigger>
                <SelectValue placeholder="Selecione uma categoria" />
              </SelectTrigger>
              <SelectContent>
                {categories.map((cat) => (
                  <SelectItem key={cat} value={cat}>
                    {cat}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>

          <div className="flex items-center space-x-2">
            <Checkbox
              id="trusted"
              checked={isTrusted}
              onCheckedChange={(checked) => setIsTrusted(checked as boolean)}
              disabled={isLoading}
            />
            <Label htmlFor="trusted" className="cursor-pointer">
              Marcar como fonte confiável
            </Label>
          </div>

          <div className="flex gap-2 justify-end pt-4">
            <Button type="button" variant="outline" onClick={() => onOpenChange(false)} disabled={isLoading}>
              Cancelar
            </Button>
            <Button type="submit" disabled={!file || isLoading}>
              {isLoading ? (
                <>
                  <Loader2 className="mr-2 h-4 w-4 animate-spin" />
                  Processando...
                </>
              ) : (
                <>
                  <Upload className="mr-2 h-4 w-4" />
                  Enviar
                </>
              )}
            </Button>
          </div>
        </form>
      </DialogContent>
    </Dialog>
  );
};
