import { Button } from "@/components/ui/button";
import { LogOut, Users, Sprout, Menu } from "lucide-react";
import { MqttStatus } from "./MqttStatus";
import hydroSmartLogo from "@/assets/hydro-smart-logo.webp";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";

interface AppHeaderProps {
  onLogout: () => void;
  onNavigate: (path: string) => void;
}

export const AppHeader = ({ onLogout, onNavigate }: AppHeaderProps) => {
  return (
    <header className="border-b border-border bg-card/95 backdrop-blur-sm sticky top-0 z-50 shadow-sm">
      <div className="container mx-auto px-4 py-3">
        <div className="flex items-center justify-between">
          {/* Logo and Title */}
          <div className="flex items-center gap-3">
            <div className="relative">
              <img 
                src={hydroSmartLogo} 
                alt="Hydro Smart" 
                className="h-12 w-12 rounded-lg shadow-md"
              />
            </div>
            <div className="hidden sm:block">
              <h1 className="text-lg font-bold text-primary leading-tight">
                Hydro Smart
              </h1>
              <p className="text-xs text-muted-foreground">
                Agricultura de Precisão
              </p>
            </div>
          </div>

          {/* Desktop Navigation */}
          <div className="hidden md:flex items-center gap-2">
            <MqttStatus />
            <Button 
              onClick={() => onNavigate('/plants')} 
              variant="outline" 
              size="sm" 
              className="gap-2"
            >
              <Sprout className="h-4 w-4" />
              Plantas
            </Button>
            <Button 
              onClick={() => onNavigate('/community')} 
              variant="outline" 
              size="sm" 
              className="gap-2"
            >
              <Users className="h-4 w-4" />
              Comunidade
            </Button>
            <Button 
              onClick={onLogout} 
              variant="outline" 
              size="sm" 
              className="gap-2"
            >
              <LogOut className="h-4 w-4" />
              Sair
            </Button>
          </div>

          {/* Mobile Menu */}
          <div className="flex md:hidden items-center gap-2">
            <MqttStatus />
            <DropdownMenu>
              <DropdownMenuTrigger asChild>
                <Button variant="outline" size="sm">
                  <Menu className="h-4 w-4" />
                </Button>
              </DropdownMenuTrigger>
              <DropdownMenuContent align="end" className="w-48">
                <DropdownMenuItem onClick={() => onNavigate('/plants')}>
                  <Sprout className="h-4 w-4 mr-2" />
                  Plantas
                </DropdownMenuItem>
                <DropdownMenuItem onClick={() => onNavigate('/community')}>
                  <Users className="h-4 w-4 mr-2" />
                  Comunidade
                </DropdownMenuItem>
                <DropdownMenuItem onClick={onLogout}>
                  <LogOut className="h-4 w-4 mr-2" />
                  Sair
                </DropdownMenuItem>
              </DropdownMenuContent>
            </DropdownMenu>
          </div>
        </div>

        {/* Creator Credit */}
        <div className="mt-2 text-center">
          <p className="text-xs text-muted-foreground">
            Idealizado e desenvolvido por <span className="font-medium text-primary">André Crepaldi</span>
          </p>
        </div>
      </div>
    </header>
  );
};
