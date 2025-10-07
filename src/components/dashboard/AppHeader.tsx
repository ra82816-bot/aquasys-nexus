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
    <header className="border-b border-border bg-gradient-to-r from-card via-card/98 to-primary/5 backdrop-blur-md sticky top-0 z-50 shadow-lg">
      <div className="container mx-auto px-3 sm:px-4 py-2.5 sm:py-3">
        <div className="flex items-center justify-between">
          {/* Logo and Title */}
          <div className="flex items-center gap-2 sm:gap-3">
            <div className="relative">
              <div className="absolute inset-0 bg-primary/20 blur-xl rounded-full animate-pulse" />
              <img 
                src={hydroSmartLogo} 
                alt="Hydro Smart" 
                className="h-10 w-10 sm:h-12 sm:w-12 rounded-xl shadow-lg relative z-10 ring-2 ring-primary/30"
              />
            </div>
            <div>
              <h1 className="text-base sm:text-lg font-bold text-primary leading-tight tracking-tight">
                Hydro Smart
              </h1>
              <p className="text-[10px] sm:text-xs text-muted-foreground">
                Agricultura de Precisão
              </p>
            </div>
          </div>

          {/* Desktop Navigation */}
          <div className="hidden md:flex items-center gap-1.5">
            <MqttStatus />
            <Button 
              onClick={() => onNavigate('/plants')} 
              variant="outline" 
              size="sm" 
              className="gap-2 hover:bg-primary/10 hover:border-primary/50 transition-all"
            >
              <Sprout className="h-4 w-4" />
              Plantas
            </Button>
            <Button 
              onClick={() => onNavigate('/community')} 
              variant="outline" 
              size="sm" 
              className="gap-2 hover:bg-primary/10 hover:border-primary/50 transition-all"
            >
              <Users className="h-4 w-4" />
              Comunidade
            </Button>
            <Button 
              onClick={onLogout} 
              variant="outline" 
              size="sm" 
              className="gap-2 hover:bg-destructive/10 hover:border-destructive/50 transition-all"
            >
              <LogOut className="h-4 w-4" />
              Sair
            </Button>
          </div>

          {/* Mobile Menu */}
          <div className="flex md:hidden items-center gap-1.5">
            <MqttStatus />
            <DropdownMenu>
              <DropdownMenuTrigger asChild>
                <Button variant="outline" size="sm" className="h-9 w-9 p-0">
                  <Menu className="h-4 w-4" />
                </Button>
              </DropdownMenuTrigger>
              <DropdownMenuContent align="end" className="w-52 mr-2">
                <DropdownMenuItem onClick={() => onNavigate('/plants')} className="gap-2 cursor-pointer">
                  <Sprout className="h-4 w-4" />
                  <span>Plantas</span>
                </DropdownMenuItem>
                <DropdownMenuItem onClick={() => onNavigate('/community')} className="gap-2 cursor-pointer">
                  <Users className="h-4 w-4" />
                  <span>Comunidade</span>
                </DropdownMenuItem>
                <DropdownMenuItem onClick={onLogout} className="gap-2 cursor-pointer text-destructive focus:text-destructive">
                  <LogOut className="h-4 w-4" />
                  <span>Sair</span>
                </DropdownMenuItem>
              </DropdownMenuContent>
            </DropdownMenu>
          </div>
        </div>

        {/* Creator Credit - Hidden on small mobile */}
        <div className="mt-2 text-center hidden sm:block">
          <p className="text-[10px] sm:text-xs text-muted-foreground/80">
            Idealizado e desenvolvido por <span className="font-semibold text-primary">André Crepaldi</span>
          </p>
        </div>
      </div>
    </header>
  );
};
