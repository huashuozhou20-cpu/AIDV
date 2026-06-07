import { createContext, useContext, useState, useCallback, type ReactNode } from 'react';
import zh from './zh.json';
import en from './en.json';

type Lang = 'zh' | 'en';
const dicts: Record<Lang, Record<string, string>> = { zh, en };

const LangCtx = createContext<{ lang: Lang; t: (key: string, fallback?: string) => string; toggle: () => void }>(null!);

export function LangProvider({ children }: { children: ReactNode }) {
  const [lang, setLang] = useState<Lang>(() => (localStorage.getItem('aidv_lang') as Lang) || 'zh');

  const toggle = useCallback(() => setLang(prev => {
    const next = prev === 'zh' ? 'en' : 'zh';
    localStorage.setItem('aidv_lang', next);
    return next;
  }), []);

  const t = useCallback((key: string, fallback?: string) => {
    const dict = dicts[lang];
    return dict[key] || fallback || key;
  }, [lang]);

  return <LangCtx.Provider value={{ lang, t, toggle }}>{children}</LangCtx.Provider>;
}

export function useT() {
  const ctx = useContext(LangCtx);
  if (!ctx) throw new Error('useT must be used within LangProvider');
  return ctx;
}
