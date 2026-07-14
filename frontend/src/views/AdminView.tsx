import { useState, useEffect } from 'react';
import { useT } from '../i18n';
import { fetchAdminUsers, deleteAdminUser, toggleAdminUser } from '../api';
import type { AdminUser } from '../api';

export default function AdminView({ token, currentUsername }: { token: string; currentUsername: string }) {
  const { t } = useT();
  const [users, setUsers] = useState<AdminUser[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadUsers = async () => {
    setLoading(true);
    setError(null);
    try {
      const res = await fetchAdminUsers();
      if (res.status === 'success' && res.users) {
        setUsers(res.users);
      } else {
        if (res.message === '无权访问，需要管理员权限') {
          setError(res.message);
        } else {
          setError(res.message || 'Failed to load users');
        }
      }
    } catch {
      setError('网络错误，请确认后端服务是否在运行');
    }
    setLoading(false);
  };

  useEffect(() => { loadUsers(); }, [token]);

  const handleDelete = async (user: AdminUser) => {
    if (user.username === currentUsername) {
      alert(t('admin.cannotSelf'));
      return;
    }
    if (!confirm(t('admin.confirmDelete'))) return;
    try {
      const res = await deleteAdminUser(user.id);
      if (res.status === 'success') {
        setUsers(prev => prev.filter(u => u.id !== user.id));
      } else {
        alert(res.message || t('admin.deleteFailed'));
      }
    } catch {
      alert(t('admin.deleteFailed'));
    }
  };

  const handleToggleAdmin = async (user: AdminUser) => {
    if (user.username === currentUsername) {
      alert(t('admin.cannotSelf'));
      return;
    }
    const newAdmin = !user.is_admin;
    if (!newAdmin && !confirm(t('admin.confirmDemote'))) return;
    try {
      const res = await toggleAdminUser(user.id, newAdmin);
      if (res.status === 'success') {
        setUsers(prev => prev.map(u => u.id === user.id ? { ...u, is_admin: newAdmin ? 1 : 0 } : u));
      } else {
        alert(res.message || '操作失败');
      }
    } catch {
      alert('操作失败：网络错误');
    }
  };

  const isSelf = (user: AdminUser) => user.username === currentUsername;

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      <div className="flex items-center justify-between mb-6">
        <h1 className="font-headline-lg text-headline-lg text-on-surface font-bold flex items-center gap-2">
          <span className="material-symbols-outlined text-[28px] text-primary">admin_panel_settings</span>
          {t('admin.title')}
        </h1>
        <span className="font-code-sm text-on-surface-variant">
          {users.length} {t('admin.userCount')}
        </span>
      </div>

      {loading ? (
        <div className="flex-1 flex items-center justify-center text-on-surface-variant">
          <span className="material-symbols-outlined text-[24px] animate-spin mr-2">progress_activity</span>
          {t('common.loading')}
        </div>
      ) : error ? (
        <div className="flex-1 flex flex-col items-center justify-center text-on-surface-variant gap-2">
          <span className="material-symbols-outlined text-[48px] text-error">error</span>
          <p className="font-code-sm">{error}</p>
          <button onClick={loadUsers}
            className="mt-2 px-4 py-1.5 bg-primary/20 text-primary border border-primary/30 rounded-lg font-code-sm text-[11px] hover:bg-primary/30 transition-colors">
            重试
          </button>
        </div>
      ) : users.length === 0 ? (
        <div className="flex-1 flex items-center justify-center text-on-surface-variant/50">
          {t('admin.noUsers')}
        </div>
      ) : (
        <div className="glass-panel border border-white/5 rounded-xl overflow-hidden">
          <table className="w-full">
            <thead>
              <tr className="border-b border-white/10 bg-black/20">
                <th className="px-4 py-3 text-left font-label-caps text-[10px] text-outline">{t('admin.id')}</th>
                <th className="px-4 py-3 text-left font-label-caps text-[10px] text-outline">{t('admin.username')}</th>
                <th className="px-4 py-3 text-left font-label-caps text-[10px] text-outline">{t('admin.email')}</th>
                <th className="px-4 py-3 text-left font-label-caps text-[10px] text-outline">{t('admin.role')}</th>
                <th className="px-4 py-3 text-left font-label-caps text-[10px] text-outline">{t('admin.createdAt')}</th>
                <th className="px-4 py-3 text-center font-label-caps text-[10px] text-outline">{t('admin.actions')}</th>
              </tr>
            </thead>
            <tbody>
              {users.map(u => (
                <tr key={u.id} className={`border-b border-white/5 hover:bg-white/5 transition-colors ${isSelf(u) ? 'bg-primary/5' : ''}`}>
                  <td className="px-4 py-2.5 font-code-sm text-[11px] text-on-surface-variant">{u.id}</td>
                  <td className="px-4 py-2.5 font-code-sm text-[12px] text-on-surface flex items-center gap-2">
                    {u.username}
                    {isSelf(u) && <span className="text-[9px] bg-primary/20 text-primary px-1.5 py-0.5 rounded font-code-sm">you</span>}
                  </td>
                  <td className="px-4 py-2.5 font-code-sm text-[11px] text-on-surface-variant">{u.email}</td>
                  <td className="px-4 py-2.5">
                    <span className={`text-[10px] font-code-sm px-2 py-0.5 rounded-full ${
                      u.is_admin ? 'bg-secondary/20 text-secondary border border-secondary/30' : 'bg-surface-variant/50 text-on-surface-variant border border-outline-variant/20'
                    }`}>
                      {u.is_admin ? t('admin.admin') : t('admin.user')}
                    </span>
                  </td>
                  <td className="px-4 py-2.5 font-code-sm text-[10px] text-outline">{u.created_at?.slice(0, 10)}</td>
                  <td className="px-4 py-2.5">
                    <div className="flex items-center justify-center gap-2">
                      <button
                        onClick={() => handleToggleAdmin(u)}
                        disabled={isSelf(u)}
                        className={`text-[10px] font-code-sm px-2 py-1 rounded transition-colors ${
                          isSelf(u)
                            ? 'text-outline/30 cursor-not-allowed'
                            : u.is_admin
                              ? 'text-error/80 hover:text-error hover:bg-error/10 border border-error/20'
                              : 'text-secondary/80 hover:text-secondary hover:bg-secondary/10 border border-secondary/20'
                        }`}
                        title={u.is_admin ? t('admin.demote') : t('admin.promote')}>
                        {u.is_admin ? t('admin.demote') : t('admin.promote')}
                      </button>
                      <button
                        onClick={() => handleDelete(u)}
                        disabled={isSelf(u)}
                        className={`text-[10px] font-code-sm px-2 py-1 rounded transition-colors ${
                          isSelf(u)
                            ? 'text-outline/30 cursor-not-allowed'
                            : 'text-error/80 hover:text-error hover:bg-error/10 border border-error/20'
                        }`}>
                        {t('common.delete')}
                      </button>
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
