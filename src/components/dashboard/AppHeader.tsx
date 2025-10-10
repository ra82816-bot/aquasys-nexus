import { Button } from "@/components/ui/button";
import { LogOut, Users, Sprout, Menu, Camera, Activity, BarChart3, Brain, Settings } from "lucide-react";
import hydroSmartLogo from "@/assets/hydro-smart-logo.webp";
import { useLocation } from "react-router-dom";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";

interface AppHeaderProps {
  onLogout: () => void;
  onNavigate: (path: string) => void;
  currentTab?: string;
  onTabChange?: (tab: string) => void;
}

export const AppHeader = ({ onLogout, onNavigate, currentTab, onTabChange }: AppHeaderProps) => {
  const location = useLocation();
  const isDashboard = location.pathname === "/" || location.pathname === "/dashboard";

  const menuItems = [
    { id: "sensors", label: "Sensores", icon: Activity, isDashboardTab: true },
    { id: "charts", label: "Gráficos", icon: BarChart3, isDashboardTab: true },
    { id: "ai", label: "IA", icon: Brain, isDashboardTab: true },
    { id: "relays", label: "Relés", icon: Settings, isDashboardTab: true },
    { id: "plants", label: "Plantas", icon: Sprout, route: "/plants" },
    { id: "camera", label: "Câmera", icon: Camera, route: "/camera" },
    { id: "community", label: "Comunidade", icon: Users, route: "/community" },
  ];

  const handleMenuClick = (item: typeof menuItems[0]) => {
    if (item.isDashboardTab && onTabChange) {
      if (!isDashboard) {
        onNavigate("/dashboard");
        setTimeout(() => onTabChange(item.id), 100);
      } else {
        onTabChange(item.id);
      }
    } else if (item.route) {
      onNavigate(item.route);
    }
  };

  const isActive = (item: typeof menuItems[0]) => {
    if (item.isDashboardTab) {
      return isDashboard && currentTab === item.id;
    }
    return location.pathname === item.route;
  };

  return (
    <header className="border-b border-border bg-card/95 backdrop-blur-md sticky top-0 z-50 shadow-lg">
      <div className="container mx-auto px-4 py-4">
        {/* Logo, Title and Creator */}
        <div className="flex flex-col items-center gap-3 mb-4">
          <div className="flex items-center gap-3">
            <div className="relative">
              <div className="absolute inset-0 bg-primary/20 blur-xl rounded-full animate-pulse" />
              <img 
                src={hydroSmartLogo} 
                alt="HydroSmart" 
                className="h-14 w-14 rounded-xl shadow-lg relative z-10 ring-2 ring-primary/30"
              />
            </div>
            <div className="text-center">
              <h1 className="text-2xl sm:text-3xl font-bold bg-gradient-to-r from-primary to-primary/60 bg-clip-text text-transparent leading-tight tracking-tight">
                HydroSmart
              </h1>
              <p className="text-xs text-muted-foreground mt-0.5">
                Agricultura de Precisão
              </p>
            </div>
          </div>
          <p className="text-xs text-muted-foreground/70 text-center">
            Idealizado e desenvolvido por <span className="font-semibold text-primary">André Crepaldi</span>
          </p>
        </div>

        {/* Desktop Navigation */}
        <nav className="hidden md:flex items-center justify-center gap-2 flex-wrap">
          {menuItems.map((item) => {
            const Icon = item.icon;
            const active = isActive(item);
            return (
              <Button
                key={item.id}
                onClick={() => handleMenuClick(item)}
                variant={active ? "default" : "ghost"}
                size="sm"
                className="gap-2 transition-all"
              >
                <Icon className="h-4 w-4" />
                <span>{item.label}</span>
              </Button>
            );
          })}
          <Button 
            onClick={onLogout} 
            variant="ghost" 
            size="sm" 
            className="gap-2 text-destructive hover:text-destructive hover:bg-destructive/10"
          >
            <LogOut className="h-4 w-4" />
            Sair
          </Button>
        </nav>

        {/* Mobile Menu */}
        <div className="flex md:hidden justify-center">
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <Button variant="outline" size="sm" className="gap-2">
                <Menu className="h-4 w-4" />
                Menu
              </Button>
            </DropdownMenuTrigger>
            <DropdownMenuContent align="center" className="w-56">
              {menuItems.map((item) => {
                const Icon = item.icon;
                return (
                  <DropdownMenuItem 
                    key={item.id}
                    onClick={() => handleMenuClick(item)} 
                    className="gap-2 cursor-pointer"
                  >
                    <Icon className="h-4 w-4" />
                    <span>{item.label}</span>
                  </DropdownMenuItem>
                );
              })}
              <DropdownMenuItem onClick={onLogout} className="gap-2 cursor-pointer text-destructive focus:text-destructive">
                <LogOut className="h-4 w-4" />
                <span>Sair</span>
              </DropdownMenuItem>
            </DropdownMenuContent>
          </DropdownMenu>
        </div>
      </div>
    </header>
  );
};
