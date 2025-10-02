import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { ReactNode } from "react";

interface SensorCardProps {
  title: string;
  value: number | null | undefined;
  icon: ReactNode;
  unit: string;
  color: string;
  bgColor: string;
}

export const SensorCard = ({ title, value, icon, unit, color, bgColor }: SensorCardProps) => {
  return (
    <Card className="border-primary/20 hover:border-primary/40 transition-all">
      <CardHeader className="pb-3">
        <CardTitle className="text-sm font-medium flex items-center gap-2">
          <div className={`p-2 rounded-lg ${bgColor} ${color}`}>
            {icon}
          </div>
          {title}
        </CardTitle>
      </CardHeader>
      <CardContent>
        <div className="text-3xl font-bold">
          {value !== null && value !== undefined ? (
            <>
              {value.toFixed(1)}
              <span className="text-lg text-muted-foreground ml-1">{unit}</span>
            </>
          ) : (
            <span className="text-muted-foreground text-lg">Aguardando dados...</span>
          )}
        </div>
      </CardContent>
    </Card>
  );
};