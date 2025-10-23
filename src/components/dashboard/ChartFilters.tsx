import { useState } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Popover, PopoverContent, PopoverTrigger } from "@/components/ui/popover";
import { Calendar as CalendarComponent } from "@/components/ui/calendar";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { Label } from "@/components/ui/label";
import { Calendar, RefreshCw, Filter } from "lucide-react";
import { format, subDays, subMonths } from "date-fns";
import { ptBR } from "date-fns/locale";
import { cn } from "@/lib/utils";

interface ChartFiltersProps {
  startDate: Date;
  endDate: Date;
  onStartDateChange: (date: Date) => void;
  onEndDateChange: (date: Date) => void;
  onRefresh: () => void;
  onQuickFilter: (days: number) => void;
}

export const ChartFilters = ({
  startDate,
  endDate,
  onStartDateChange,
  onEndDateChange,
  onRefresh,
  onQuickFilter,
}: ChartFiltersProps) => {
  const [resolution, setResolution] = useState<'raw' | 'hourly' | 'daily'>('raw');

  const quickFilters = [
    { label: '24h', days: 1 },
    { label: '3 dias', days: 3 },
    { label: '7 dias', days: 7 },
    { label: '30 dias', days: 30 },
    { label: '3 meses', days: 90 },
  ];

  return (
    <Card className="bg-card/50 backdrop-blur-sm">
      <CardHeader>
        <CardTitle className="text-base sm:text-lg flex items-center gap-2">
          <Filter className="h-5 w-5" />
          Filtros e Período
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        {/* Quick Filters */}
        <div className="space-y-2">
          <Label className="text-xs">Filtros Rápidos</Label>
          <div className="flex flex-wrap gap-2">
            {quickFilters.map((filter) => (
              <Button
                key={filter.label}
                variant="outline"
                size="sm"
                onClick={() => onQuickFilter(filter.days)}
                className="text-xs"
              >
                {filter.label}
              </Button>
            ))}
          </div>
        </div>

        {/* Date Range */}
        <div className="space-y-2">
          <Label className="text-xs">Período Personalizado</Label>
          <div className="flex flex-col sm:flex-row gap-2">
            <Popover>
              <PopoverTrigger asChild>
                <Button
                  variant="outline"
                  className={cn("justify-start text-left font-normal flex-1")}
                  size="sm"
                >
                  <Calendar className="mr-2 h-4 w-4" />
                  {format(startDate, "dd/MM/yy", { locale: ptBR })}
                </Button>
              </PopoverTrigger>
              <PopoverContent className="w-auto p-0" align="start">
                <CalendarComponent
                  mode="single"
                  selected={startDate}
                  onSelect={(date) => date && onStartDateChange(date)}
                  disabled={(date) => date > endDate}
                  initialFocus
                />
              </PopoverContent>
            </Popover>

            <span className="self-center text-sm text-muted-foreground">até</span>

            <Popover>
              <PopoverTrigger asChild>
                <Button
                  variant="outline"
                  className={cn("justify-start text-left font-normal flex-1")}
                  size="sm"
                >
                  <Calendar className="mr-2 h-4 w-4" />
                  {format(endDate, "dd/MM/yy", { locale: ptBR })}
                </Button>
              </PopoverTrigger>
              <PopoverContent className="w-auto p-0" align="start">
                <CalendarComponent
                  mode="single"
                  selected={endDate}
                  onSelect={(date) => date && onEndDateChange(date)}
                  disabled={(date) => date < startDate || date > new Date()}
                  initialFocus
                />
              </PopoverContent>
            </Popover>
          </div>
        </div>

        {/* Resolution */}
        <div className="space-y-2">
          <Label className="text-xs">Resolução dos Dados</Label>
          <Select value={resolution} onValueChange={(value: any) => setResolution(value)}>
            <SelectTrigger className="h-9">
              <SelectValue />
            </SelectTrigger>
            <SelectContent>
              <SelectItem value="raw">Dados Brutos</SelectItem>
              <SelectItem value="hourly">Média por Hora</SelectItem>
              <SelectItem value="daily">Média Diária</SelectItem>
            </SelectContent>
          </Select>
        </div>

        <Button onClick={onRefresh} className="w-full gap-2" size="sm">
          <RefreshCw className="h-4 w-4" />
          Atualizar Dados
        </Button>
      </CardContent>
    </Card>
  );
};
