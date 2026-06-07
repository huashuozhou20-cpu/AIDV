import { useState, useRef } from 'react';
import { cn } from './lib/utils';
import { useT } from './i18n';
import LoginView from './views/LoginView';
import DashboardView from './views/DashboardView';
import QueryAnalyzerView from './views/QueryAnalyzerView';
import AIEngineView from './views/AIEngineView';
import SQLDiffView from './views/SQLDiffView';
import PracticeView from './views/PracticeView';
import ScenariosView from './views/ScenariosView';
import ScenarioDesignerView from './views/ScenarioDesignerView';
import DataTableView from './views/DataTableView';

type ViewId = 'dashboard' | 'query-analyzer' | 'ai-engine' | 'sql-diff' | 'practice' | 'scenarios' | 'scenario-designer' | 'scenario-data';

const COLORS = ['#1a1a2e', '#16213e', '#0f3460', '#1a1a1a', '#2d132c', '#1b262c', '#0d1117', '#1c0b2b'];
const GRADIENTS = [
  'linear-gradient(135deg, #1a1a2e 0%, #16213e 100%)',
  'linear-gradient(135deg, #0f3460 0%, #1a1a2e 100%)',
  'linear-gradient(135deg, #2d132c 0%, #1b262c 100%)',
  'linear-gradient(135deg, #1c0b2b 0%, #0d1117 100%)',
  'linear-gradient(135deg, #1b262c 0%, #16213e 100%)',
  'linear-gradient(135deg, #0d1117 0%, #1a1a2e 100%)',
];

export default function App() {
  const { t } = useT();
  const [token, setToken] = useState<string | null>(localStorage.getItem('aidv_token'));
  const [username, setUsername] = useState<string | null>(localStorage.getItem('aidv_user'));
  const [currentView, setCurrentView] = useState<ViewId>('dashboard');
  const [bgImage, setBgImage] = useState<string | null>(localStorage.getItem('aidv_bg'));
  const [bgFit, setBgFit] = useState<string>(localStorage.getItem('aidv_bg_fit') || 'cover');
  const [bgOpacity, setBgOpacity] = useState<number>(Number(localStorage.getItem('aidv_bg_opacity') || 0.4));
  const [isBgPickerOpen, setIsBgPickerOpen] = useState(false);
  const [customBgUrl, setCustomBgUrl] = useState('');
  const fileRef = useRef<HTMLInputElement>(null);

  const handleLogin = (t: string, u: string) => {
    localStorage.setItem('aidv_token', t);
    localStorage.setItem('aidv_user', u);
    setToken(t); setUsername(u);
  };

  const handleLogout = () => {
    localStorage.removeItem('aidv_token');
    localStorage.removeItem('aidv_user');
    setToken(null); setUsername(null);
  };

  const [scenarioCtx, setScenarioCtx] = useState<{ id: number; name: string } | null>(null);
  const [editTableCtx, setEditTableCtx] = useState<{ tableId: number; tableName: string; displayName: string } | null>(null);

  const handleEnterScenario = (id: number, name: string) => {
    setScenarioCtx({ id, name });
    setCurrentView('scenario-designer');
  };

  const handleDesignDone = () => {
    setEditTableCtx(null);
    setCurrentView('scenario-data');
  };

  const handleEditStructure = (tableId: number, tableName: string, displayName: string) => {
    setEditTableCtx({ tableId, tableName, displayName });
    setCurrentView('scenario-designer');
  };

  if (!token) return <LoginView onLogin={handleLogin} />;

  return (
    <div className={cn("w-full h-full text-on-surface relative", !bgImage && "bg-industrial")}
      style={bgImage ? { backgroundImage: `url(${bgImage})`, backgroundSize: bgFit, backgroundPosition: 'center', backgroundRepeat: 'no-repeat' } : undefined}>
      {bgImage && <div className="absolute inset-0 pointer-events-none" style={{ backgroundColor: `rgba(0,0,0,${bgOpacity})` }} />}

      {/* SideNavBar */}
      <nav className="bg-black/10 backdrop-blur-[40px] backdrop-saturate-200 border-r border-white/5 shadow-[4px_0_24px_rgba(0,0,0,0.1)] font-code-sm text-code-sm fixed left-0 top-0 h-full w-[64px] hover:w-[240px] transition-all duration-300 z-50 flex text-left flex-col py-panel-padding overflow-hidden group">
        <div className="px-4 mb-8 flex items-center h-8 shrink-0 overflow-hidden">
          <span className="material-symbols-outlined text-primary text-[24px] shrink-0 icon-fill ai-glow">database</span>
          <div className="ml-4 opacity-0 group-hover:opacity-100 transition-opacity duration-300 whitespace-nowrap">
            <div className="font-headline-md text-headline-md font-bold text-on-surface">{t('app.name')}</div>
            <div className="text-on-surface-variant font-label-caps text-[9px] uppercase tracking-wider mt-0.5">{t('app.tagline')}</div>
          </div>
        </div>

        <div className="flex-1 flex flex-col gap-1 px-2">
          <NavItem icon="dashboard" label={t('nav.dashboard')} isActive={currentView === 'dashboard'} onClick={() => setCurrentView('dashboard')} />
          <NavItem icon="query_stats" label={t('nav.analyzer')} isActive={currentView === 'query-analyzer'} onClick={() => setCurrentView('query-analyzer')} />
          <NavItem icon="school" label={t('nav.practice')} isActive={currentView === 'practice'} onClick={() => setCurrentView('practice')} />
          <NavItem icon="psychology" label={t('nav.ai')} isActive={['ai-engine', 'sql-diff'].includes(currentView)} onClick={() => setCurrentView('ai-engine')} activeIconClass="icon-fill" />
          <NavItem icon="dashboard" label={t('nav.scenarios')} isActive={['scenarios', 'scenario-designer', 'scenario-data'].includes(currentView)} onClick={() => setCurrentView('scenarios')} />
        </div>

        <div className="mt-auto px-2 pt-4 border-t border-outline-variant/20">
          <div className="flex items-center h-10 px-2 w-full overflow-hidden">
            <div className="w-6 h-6 rounded-full bg-primary/20 border border-primary/30 flex items-center justify-center shrink-0">
              <span className="material-symbols-outlined text-[14px] text-primary">person</span>
            </div>
            <div className="ml-3 opacity-0 group-hover:opacity-100 transition-opacity duration-300 whitespace-nowrap">
              <span className="text-on-surface text-[11px] font-medium">{username}</span>
            </div>
          </div>
          <button onClick={handleLogout} className="flex items-center h-8 px-2 w-full rounded hover:bg-white/5 transition-all duration-150 overflow-hidden mt-1">
            <span className="material-symbols-outlined text-[16px] text-on-surface-variant shrink-0">logout</span>
            <span className="ml-4 text-on-surface-variant text-[10px] opacity-0 group-hover:opacity-100 transition-opacity duration-300 whitespace-nowrap">{t('nav.logout')}</span>
          </button>
        </div>
      </nav>

      <div className="flex-1 flex flex-col ml-[64px] relative w-[calc(100vw-64px)] h-full">
        <header className="fixed top-0 right-0 left-[64px] flex justify-between items-center px-gutter z-40 h-12 bg-black/10 backdrop-blur-[40px] backdrop-saturate-200 border-b border-white/5 shadow-[0_4px_24px_rgba(0,0,0,0.1)]">
          <div className="flex items-center gap-6">
            <div className="font-headline-md text-headline-md tracking-tighter text-on-surface border-r border-outline-variant/30 pr-4">{t('app.name')}</div>
            <nav className="hidden md:flex items-center gap-1 font-code-md text-code-md">
              <TopTab label={t('nav.dashboard')} isActive={currentView === 'dashboard'} onClick={() => setCurrentView('dashboard')} />
              <TopTab label={t('nav.analyzer')} isActive={currentView === 'query-analyzer'} onClick={() => setCurrentView('query-analyzer')} />
              <TopTab label={t('nav.practice')} isActive={currentView === 'practice'} onClick={() => setCurrentView('practice')} />
              <TopTab label={t('nav.ai')} isActive={['ai-engine', 'sql-diff'].includes(currentView)} onClick={() => setCurrentView('ai-engine')} />
              <TopTab label={t('nav.scenarios')} isActive={['scenarios', 'scenario-designer', 'scenario-data'].includes(currentView)} onClick={() => setCurrentView('scenarios')} />
            </nav>
          </div>
          <div className="flex items-center gap-4">
            <LangToggle />
            <div className="relative">
              <IconButton icon="palette" className={`text-on-surface-variant hover:text-primary ${isBgPickerOpen ? 'text-primary bg-white/5' : ''}`} onClick={() => setIsBgPickerOpen(!isBgPickerOpen)} />
              {isBgPickerOpen && (
                <div className="absolute top-10 right-0 w-72 bg-black/30 backdrop-blur-[40px] backdrop-saturate-200 border border-white/10 shadow-[0_16px_40px_rgba(0,0,0,0.4)] rounded-2xl p-4 z-50">
                  <div className="text-[11px] font-code-sm font-bold text-on-surface-variant mb-3 uppercase tracking-wider">{t('bg.title')}</div>

                  {/* Color presets — instant, no network */}
                  <div className="grid grid-cols-4 gap-2 mb-2">
                    {COLORS.map(c => (
                      <button key={c} onClick={() => { setBgImage(null); localStorage.removeItem('aidv_bg'); }}
                        className="h-10 rounded-lg border transition-all hover:scale-105"
                        style={{ backgroundColor: c }} title={c} />
                    ))}
                  </div>
                  <div className="grid grid-cols-3 gap-2 mb-3">
                    {GRADIENTS.map((g, i) => (
                      <button key={i} onClick={() => { setBgImage(null); localStorage.setItem('aidv_bg', g); }}
                        className={`h-10 rounded-lg border transition-all hover:scale-105 ${!bgImage && localStorage.getItem('aidv_bg') === g ? 'border-primary outline outline-1 outline-primary' : 'border-white/10'}`}
                        style={{ background: g }} />
                    ))}
                  </div>

                  {/* URL input */}
                  <div className="flex gap-1 mb-2">
                    <input type="text" placeholder={t('bg.url')} value={customBgUrl}
                      onChange={e => setCustomBgUrl(e.target.value)}
                      className="flex-1 bg-black/40 border border-outline-variant/30 rounded px-2 py-1 text-[10px] font-code-sm text-on-surface focus:outline-none focus:border-primary" />
                    <button onClick={() => { if (customBgUrl.trim()) { setBgImage(customBgUrl.trim()); localStorage.setItem('aidv_bg', customBgUrl.trim()); setCustomBgUrl(''); } }}
                      className="bg-primary/20 text-primary border border-primary/30 rounded px-3 text-[10px] font-code-sm hover:bg-primary/30 transition-colors">{t('bg.urlSet')}</button>
                  </div>

                  {/* File upload — big & obvious */}
                  <input type="file" ref={fileRef} accept="image/*" className="hidden"
                    onChange={e => {
                      const file = e.target.files?.[0];
                      if (!file) return;
                      const r = new FileReader();
                      r.onload = ev => { const u = ev.target?.result as string; setBgImage(u); localStorage.setItem('aidv_bg', u); };
                      r.readAsDataURL(file);
                    }} />
                  <button onClick={() => fileRef.current?.click()}
                    className="w-full flex items-center justify-center gap-2 bg-primary/10 border border-dashed border-primary/30 rounded-lg py-2.5 cursor-pointer hover:bg-primary/20 transition-colors text-[11px] font-code-sm text-primary font-bold">
                    <span className="material-symbols-outlined text-[16px]">upload_file</span> {t('bg.upload')}
                  </button>

                  {/* Opacity + Fit mode */}
                  {bgImage && (
                    <div className="mt-3 pt-3 border-t border-white/10 space-y-3">
                      <div>
                        <div className="flex items-center justify-between mb-1">
                          <span className="text-[10px] font-code-sm text-on-surface-variant">{t('bg.opacity')}</span>
                          <span className="text-[10px] font-code-sm text-outline">{Math.round(bgOpacity * 100)}%</span>
                        </div>
                        <input type="range" min="0" max="0.9" step="0.05" value={bgOpacity}
                          onChange={e => { const v = parseFloat(e.target.value); setBgOpacity(v); localStorage.setItem('aidv_bg_opacity', String(v)); }}
                          className="w-full h-1 bg-surface-container-highest rounded-full appearance-none cursor-pointer [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3 [&::-webkit-slider-thumb]:h-3 [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:bg-primary" />
                      </div>
                      <div>
                        <div className="text-[10px] font-code-sm text-on-surface-variant mb-2">{t('bg.fit')}</div>
                      <div className="flex gap-1">
                        {[
                          { v: 'cover', l: t('bg.fitFill') },
                          { v: 'contain', l: t('bg.fitContain') },
                          { v: '100% 100%', l: t('bg.fitStretch') },
                          { v: 'auto 100%', l: t('bg.fitHeight') },
                          { v: '100% auto', l: t('bg.fitWidth') },
                        ].map(m => (
                          <button key={m.v} onClick={() => { setBgFit(m.v); localStorage.setItem('aidv_bg_fit', m.v); }}
                            className={`flex-1 py-1 rounded text-[9px] font-code-sm transition-colors ${bgFit === m.v ? 'bg-primary/20 text-primary border border-primary/30' : 'bg-surface-variant/50 text-on-surface-variant border border-outline-variant/20 hover:border-white/30'}`}>
                            {m.l}</button>
                        ))}
                      </div>
                    </div>
                    </div>
                  )}

                  {/* Clear */}
                  {bgImage && (
                    <button onClick={() => { setBgImage(null); localStorage.removeItem('aidv_bg'); localStorage.removeItem('aidv_bg_fit'); }}
                      className="w-full mt-2 text-[10px] text-on-surface-variant hover:text-error transition-colors text-center py-1">{t('bg.clear')}</button>
                  )}
                </div>
              )}
            </div>
          </div>
        </header>

        <main className="flex-1 mt-12 overflow-hidden h-[calc(100vh-48px)] bg-transparent relative z-10 transition-colors duration-500">
          {currentView === 'dashboard' && <DashboardView />}
          {currentView === 'query-analyzer' && <QueryAnalyzerView />}
          {currentView === 'practice' && <PracticeView token={token} />}
          {currentView === 'ai-engine' && <AIEngineView onNavigateToDiff={() => setCurrentView('sql-diff')} />}
          {currentView === 'sql-diff' && <SQLDiffView onBack={() => setCurrentView('ai-engine')} />}
          {currentView === 'scenarios' && <ScenariosView onEnterScenario={handleEnterScenario} />}
          {currentView === 'scenario-designer' && scenarioCtx && (
            <ScenarioDesignerView
              scenarioId={scenarioCtx.id}
              scenarioName={scenarioCtx.name}
              onDone={handleDesignDone}
              editTable={editTableCtx}
            />)}
          {currentView === 'scenario-data' && scenarioCtx && (
            <DataTableView
              scenarioId={scenarioCtx.id}
              scenarioName={scenarioCtx.name}
              onBack={() => setCurrentView('scenarios')}
              onEditStructure={handleEditStructure}
            />)}
        </main>
      </div>
    </div>
  );
}

function LangToggle() {
  const { lang, toggle } = useT();
  return (
    <button onClick={toggle} className="w-8 h-8 rounded hover:bg-white/10 flex items-center justify-center transition-colors text-on-surface-variant hover:text-on-surface font-code-sm text-[10px] font-bold"
      title={lang === 'zh' ? 'Switch to English' : '切换到中文'}>
      {lang === 'zh' ? 'EN' : '中'}
    </button>
  );
}

function NavItem({ icon, label, isActive, onClick, activeIconClass = "" }: { icon: string; label: string; isActive: boolean; onClick: () => void; activeIconClass?: string }) {
  return (
    <button onClick={onClick} className={cn(
      "flex items-center h-10 px-2 rounded w-full text-left transition-all duration-150 ease-in-out",
      isActive ? "text-primary border-l-2 border-primary bg-primary/20 backdrop-blur-sm font-bold" : "text-on-surface-variant hover:bg-white/10 hover:text-on-surface border-l-2 border-transparent"
    )}>
      <span className={cn("material-symbols-outlined shrink-0 text-[20px]", isActive ? activeIconClass : "")}>{icon}</span>
      <span className="ml-4 opacity-0 group-hover:opacity-100 transition-opacity duration-300 whitespace-nowrap">{label}</span>
    </button>
  );
}

function TopTab({ label, isActive, onClick }: { label: string; isActive: boolean; onClick: () => void }) {
  return (
    <button onClick={onClick} className={cn(
      "px-3 py-1.5 transition-colors scale-95 active:scale-100",
      isActive ? "text-primary font-bold border-b-2 border-primary pb-1" : "text-on-surface-variant hover:text-primary hover:bg-white/5 rounded-t"
    )}>{label}</button>
  );
}

function IconButton({ icon, className, onClick }: { icon: string; className?: string; onClick?: () => void }) {
  return (
    <button onClick={onClick} className={cn("w-8 h-8 rounded hover:bg-white/10 flex items-center justify-center transition-colors scale-95 active:scale-100 cursor-pointer", className)}>
      <span className="material-symbols-outlined text-[18px]">{icon}</span>
    </button>
  );
}
