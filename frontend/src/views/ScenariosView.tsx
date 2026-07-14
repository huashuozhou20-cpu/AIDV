import { useState, useEffect } from 'react';
import { useT } from '../i18n';
import { fetchScenarios, createScenario, deleteScenario } from '../api';
import type { Scenario } from '../api';

export default function ScenariosView({ onEnterScenario }: { onEnterScenario: (id: number, name: string) => void }) {
  const { t } = useT();
  const [scenarios, setScenarios] = useState<Scenario[]>([]);
  const [showCreate, setShowCreate] = useState(false);
  const [name, setName] = useState('');
  const [description, setDescription] = useState('');
  const [slug, setSlug] = useState('');
  const [loading, setLoading] = useState(false);

  // Check if API response indicates auth failure — clear token to force re-login
  const checkAuth = (res: { status: string; message?: string }) => {
    if (res.message === '未登录') {
      localStorage.removeItem('aidv_token');
      localStorage.removeItem('aidv_user');
      window.location.reload();
      return false;
    }
    return true;
  };

  const load = async () => {
    try {
      const res = await fetchScenarios();
      if (res.status === 'success') setScenarios(res.scenarios);
      else checkAuth(res);
    } catch { console.error('加载场景列表失败'); }
  };

  useEffect(() => { load(); }, []);

  const handleCreate = async () => {
    if (!name.trim()) return;
    setLoading(true);
    try {
      const res = await createScenario(name.trim(), description.trim(), 'database', slug.trim());
      if (res.status === 'success') {
        setShowCreate(false);
        setName(''); setDescription(''); setSlug('');
        load();
      } else if (checkAuth(res)) {
        alert('创建场景失败：' + (res.message || '未知错误'));
      }
    } catch {
      alert('创建场景失败：网络错误，请确认后端服务是否在运行');
    }
    setLoading(false);
  };

  const handleDelete = async (id: number, e: React.MouseEvent) => {
    e.stopPropagation();
    if (!confirm(t('scenarios.confirmDelete'))) return;
    try {
      await deleteScenario(id);
      load();
    } catch { alert('删除场景失败，请重试'); }
  };

  const iconMap: Record<string, string> = {
    database: 'database', store: 'store', school: 'school', inventory: 'inventory',
    people: 'group', business: 'business_center', health: 'ecg_heart',
  };

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      <div className="flex items-center justify-between mb-6">
        <h1 className="font-headline-lg text-headline-lg text-on-surface font-bold flex items-center gap-2">
          <span className="material-symbols-outlined text-[28px] text-primary">dashboard</span>{t('scenarios.title')}
        </h1>
        <button onClick={() => setShowCreate(true)}
          className="bg-primary text-black font-bold px-4 py-2 rounded-lg hover:bg-primary-fixed transition-colors font-code-sm flex items-center gap-1">
          <span className="material-symbols-outlined text-[16px]">add</span>{t('scenarios.create')}
        </button>
      </div>

      <p className="font-code-sm text-on-surface-variant mb-6 -mt-3">{t('scenarios.desc')}</p>

      {/* Create Dialog */}
      {showCreate && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm" onClick={() => setShowCreate(false)}>
          <div className="bg-surface border border-white/10 rounded-2xl p-6 w-[440px] shadow-2xl" onClick={e => e.stopPropagation()}>
            <h2 className="font-headline-md text-headline-md text-on-surface font-bold mb-1">{t('scenarios.create')}</h2>
            <p className="font-code-sm text-[11px] text-on-surface-variant mb-4">{t('scenarios.desc')}</p>
            <input type="text" placeholder={t('scenarios.namePlaceholder')} value={name}
              onChange={e => setName(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleCreate()}
              className="w-full bg-black/40 border border-outline-variant/30 rounded-lg px-4 py-2.5 font-code-sm text-on-surface focus:outline-none focus:border-primary mb-3" />
            <input type="text" placeholder="英文标识（可选，如 bookstore）" value={slug}
              onChange={e => setSlug(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleCreate()}
              className="w-full bg-black/40 border border-outline-variant/30 rounded-lg px-4 py-2.5 font-code-sm text-on-surface focus:outline-none focus:border-primary mb-3 text-[11px]" />
            <input type="text" placeholder={t('scenarios.descPlaceholder')} value={description}
              onChange={e => setDescription(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleCreate()}
              className="w-full bg-black/40 border border-outline-variant/30 rounded-lg px-4 py-2.5 font-code-sm text-on-surface focus:outline-none focus:border-primary mb-4" />
            <div className="flex gap-2 justify-end">
              <button onClick={() => setShowCreate(false)} className="px-4 py-2 rounded-lg font-code-sm text-on-surface-variant hover:bg-white/10 transition-colors">{t('common.cancel')}</button>
              <button onClick={handleCreate} disabled={loading || !name.trim()}
                className="bg-primary text-black font-bold px-4 py-2 rounded-lg hover:bg-primary-fixed transition-colors disabled:opacity-50 font-code-sm">
                {loading ? t('scenarios.creating') : t('scenarios.createAndDesign')}</button>
            </div>
          </div>
        </div>
      )}

      {/* Scenario Cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {scenarios.map(s => (
          <div key={s.id} onClick={() => onEnterScenario(s.id, s.name)}
            className="glass-panel border border-white/10 hover:border-primary/30 rounded-xl p-5 cursor-pointer transition-all hover:scale-[1.02] group relative">
            <button onClick={e => handleDelete(s.id, e)}
              className="absolute top-3 right-3 opacity-0 group-hover:opacity-100 text-on-surface-variant hover:text-error transition-all p-1">
              <span className="material-symbols-outlined text-[16px]">delete</span></button>
            <div className="flex items-center gap-3 mb-3">
              <span className="material-symbols-outlined text-[32px] text-primary icon-fill">{iconMap[s.icon] || 'database'}</span>
              <div>
                <h3 className="font-headline-sm text-headline-sm text-on-surface font-bold">{s.name}</h3>
                <span className="font-code-sm text-[10px] text-outline">{s.slug}</span>
              </div>
            </div>
            <p className="font-code-sm text-[11px] text-on-surface-variant leading-relaxed">{s.description || t('scenarios.noDesc')}</p>
            <div className="mt-3 flex items-center gap-2 text-[10px] font-code-sm text-outline">
              <span>{s.created_at?.slice(0, 10)}</span>
            </div>
          </div>
        ))}
      </div>

      {scenarios.length === 0 && (
        <div className="flex-1 flex flex-col items-center justify-center text-on-surface-variant/50 gap-3">
          <span className="material-symbols-outlined text-[48px]">dashboard</span>
          <span className="font-code-md">{t('scenarios.empty')}</span>
        </div>
      )}
    </div>
  );
}
