import { useState } from 'react';
import { useT } from '../i18n';

export default function LoginView({ onLogin }: { onLogin: (token: string, username: string) => void }) {
  const { t } = useT();
  const [isRegister, setIsRegister] = useState(false);
  const [username, setUsername] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const handleSubmit = async () => {
    if (!username.trim() || !password.trim()) { setError(t('login.fillRequired')); return; }
    if (isRegister && !email.trim()) { setError(t('login.fillEmail')); return; }
    setLoading(true); setError('');

    const endpoint = isRegister ? '/api/auth/register' : '/api/auth/login';
    const body: any = { username: username.trim(), password };
    if (isRegister) body.email = email.trim();

    try {
      const res = await fetch(endpoint, {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
      const data = await res.json();
      if (data.status === 'success') {
        onLogin(data.token, data.user.username);
      } else {
        setError(data.message || '操作失败');
      }
    } catch { setError(t('login.netError')); }
    finally { setLoading(false); }
  };

  return (
    <div className="flex-1 flex items-center justify-center bg-transparent">
      <div className="glass-panel border border-white/10 rounded-2xl p-8 w-[400px] shadow-2xl">
        <div className="text-center mb-6">
          <span className="material-symbols-outlined text-[48px] text-primary icon-fill ai-glow">database</span>
          <h1 className="font-headline-lg text-headline-lg text-on-surface font-bold mt-2">{t('login.title')}</h1>
          <p className="font-code-sm text-code-sm text-on-surface-variant mt-1">{t('login.subtitle')}</p>
        </div>

        <h2 className="font-headline-md text-headline-md text-on-surface font-bold mb-4">
          {isRegister ? t('login.register') : t('login.login')}
        </h2>

        <div className="space-y-3">
          <input type="text" placeholder={t('login.username')} value={username}
            onChange={e => setUsername(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && handleSubmit()}
            className="w-full bg-black/30 border border-outline-variant/30 rounded-lg px-4 py-2.5 font-code-sm text-on-surface focus:outline-none focus:border-primary transition-colors" />
          {isRegister && (
            <input type="email" placeholder={t('login.email')} value={email}
              onChange={e => setEmail(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleSubmit()}
              className="w-full bg-black/30 border border-outline-variant/30 rounded-lg px-4 py-2.5 font-code-sm text-on-surface focus:outline-none focus:border-primary transition-colors" />
          )}
          <input type="password" placeholder={t('login.password')} value={password}
            onChange={e => setPassword(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && handleSubmit()}
            className="w-full bg-black/30 border border-outline-variant/30 rounded-lg px-4 py-2.5 font-code-sm text-on-surface focus:outline-none focus:border-primary transition-colors" />

          {error && <div className="text-error text-[11px] font-code-sm">{error}</div>}

          <button onClick={handleSubmit} disabled={loading}
            className="w-full bg-primary text-black font-bold py-2.5 rounded-lg hover:bg-primary-fixed transition-colors disabled:opacity-50 font-code-sm">
            {loading ? '...' : isRegister ? t('login.register') : t('login.login')}
          </button>
        </div>

        <div className="mt-4 text-center">
          <button onClick={() => { setIsRegister(!isRegister); setError(''); }}
            className="text-primary text-[11px] font-code-sm hover:underline">
            {isRegister ? t('login.hasAccount') : t('login.noAccount')}
          </button>
        </div>
      </div>
    </div>
  );
}
