import { useState } from "react";
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import * as z from "zod";
import { supabase } from "@/integrations/supabase/client";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import {
  Form,
  FormControl,
  FormDescription,
  FormField,
  FormItem,
  FormLabel,
  FormMessage,
} from "@/components/ui/form";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Input } from "@/components/ui/input";
import { Button } from "@/components/ui/button";
import { Loader2, Cpu, Radio } from "lucide-react";
import { useToast } from "@/hooks/use-toast";

const formSchema = z.object({
  device_uuid: z.string()
    .min(8, "UUID deve ter pelo menos 8 caracteres")
    .max(64, "UUID muito longo"),
  claim_token: z.string()
    .min(4, "Token deve ter pelo menos 4 caracteres")
    .max(32, "Token muito longo"),
  device_type: z.enum(["sensor", "actuator"], {
    required_error: "Selecione o tipo de dispositivo",
  }),
});

type FormValues = z.infer<typeof formSchema>;

interface AddDeviceDialogProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  onSuccess: () => void;
}

export const AddDeviceDialog = ({ open, onOpenChange, onSuccess }: AddDeviceDialogProps) => {
  const [loading, setLoading] = useState(false);
  const { toast } = useToast();

  const form = useForm<FormValues>({
    resolver: zodResolver(formSchema),
    defaultValues: {
      device_uuid: "",
      claim_token: "",
      device_type: undefined,
    },
  });

  const onSubmit = async (values: FormValues) => {
    setLoading(true);
    try {
      const { data, error } = await supabase.functions.invoke('register-device', {
        body: {
          device_uuid: values.device_uuid.trim(),
          claim_token: values.claim_token.trim(),
          device_type: values.device_type,
        },
      });

      if (error) {
        throw new Error(error.message || 'Erro ao registrar dispositivo');
      }

      if (data?.error) {
        throw new Error(data.error);
      }

      form.reset();
      onSuccess();
    } catch (error: any) {
      console.error('Error registering device:', error);
      toast({
        title: "Erro no registro",
        description: error.message || "Não foi possível registrar o dispositivo",
        variant: "destructive",
      });
    } finally {
      setLoading(false);
    }
  };

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="sm:max-w-md">
        <DialogHeader>
          <DialogTitle>Adicionar Dispositivo</DialogTitle>
          <DialogDescription>
            Insira o UUID e token exibidos no display do dispositivo para vinculá-lo à sua conta.
          </DialogDescription>
        </DialogHeader>

        <Form {...form}>
          <form onSubmit={form.handleSubmit(onSubmit)} className="space-y-4">
            <FormField
              control={form.control}
              name="device_type"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>Tipo de Dispositivo</FormLabel>
                  <Select onValueChange={field.onChange} defaultValue={field.value}>
                    <FormControl>
                      <SelectTrigger>
                        <SelectValue placeholder="Selecione o tipo" />
                      </SelectTrigger>
                    </FormControl>
                    <SelectContent>
                      <SelectItem value="sensor">
                        <div className="flex items-center gap-2">
                          <Radio className="h-4 w-4 text-blue-500" />
                          <span>Módulo Sensor</span>
                        </div>
                      </SelectItem>
                      <SelectItem value="actuator">
                        <div className="flex items-center gap-2">
                          <Cpu className="h-4 w-4 text-orange-500" />
                          <span>Módulo Atuador</span>
                        </div>
                      </SelectItem>
                    </SelectContent>
                  </Select>
                  <FormMessage />
                </FormItem>
              )}
            />

            <FormField
              control={form.control}
              name="device_uuid"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>UUID do Dispositivo</FormLabel>
                  <FormControl>
                    <Input 
                      placeholder="Ex: A1B2C3D4-E5F6-7890..." 
                      {...field}
                      className="font-mono"
                    />
                  </FormControl>
                  <FormDescription>
                    Código único exibido no display OLED do dispositivo
                  </FormDescription>
                  <FormMessage />
                </FormItem>
              )}
            />

            <FormField
              control={form.control}
              name="claim_token"
              render={({ field }) => (
                <FormItem>
                  <FormLabel>Token de Vinculação</FormLabel>
                  <FormControl>
                    <Input 
                      placeholder="Ex: 123456" 
                      {...field}
                      className="font-mono"
                    />
                  </FormControl>
                  <FormDescription>
                    Código de 6 dígitos exibido junto ao UUID
                  </FormDescription>
                  <FormMessage />
                </FormItem>
              )}
            />

            <div className="flex justify-end gap-2 pt-4">
              <Button
                type="button"
                variant="outline"
                onClick={() => onOpenChange(false)}
                disabled={loading}
              >
                Cancelar
              </Button>
              <Button type="submit" disabled={loading}>
                {loading && <Loader2 className="mr-2 h-4 w-4 animate-spin" />}
                Registrar
              </Button>
            </div>
          </form>
        </Form>
      </DialogContent>
    </Dialog>
  );
};
