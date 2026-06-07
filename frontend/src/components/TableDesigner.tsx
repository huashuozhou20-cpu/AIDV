import { useState, useEffect } from 'react';
import { useT } from '../i18n';

export interface ColumnDef {
  name: string;
  type: string;
  pk?: boolean;
  not_null?: boolean;
  auto_increment?: boolean;
}

export interface TableDef {
  name: string;
  display: string;
  columns: ColumnDef[];
}

interface Props {
  initialTables: TableDef[];
  onChange: (tables: TableDef[]) => void;
  onCreate: (tables: TableDef[]) => void;
  // Edit mode: single-table editing for existing tables
  editMode?: boolean;
  onModify?: (table: TableDef) => Promise<void>;
}

const COLUMN_TYPES = ['INT', 'FLOAT', 'CHAR(20)', 'CHAR(50)', 'CHAR(100)', 'DATE'];

function emptyColumn(): ColumnDef {
  return { name: '', type: 'INT', pk: false, not_null: false, auto_increment: false };
}

export default function TableDesigner({ initialTables, onChange, onCreate, editMode, onModify }: Props) {
  const { t } = useT();
  const [tables, setTables] = useState<TableDef[]>(initialTables);
  const [editingIdx, setEditingIdx] = useState<number | null>(null);
  const [saving, setSaving] = useState(false);

  // Draft state for the table currently being edited
  const [draftName, setDraftName] = useState('');
  const [draftDisplay, setDraftDisplay] = useState('');
  const [draftCols, setDraftCols] = useState<ColumnDef[]>([]);

  useEffect(() => {
    setTables(initialTables);
    // In edit mode, auto-start editing the first table
    if (editMode && initialTables.length > 0) {
      const tbl = initialTables[0];
      setDraftName(tbl.name);
      setDraftDisplay(tbl.display);
      setDraftCols(tbl.columns.map(c => ({ ...c })));
      setEditingIdx(0);
    }
  }, [initialTables]);

  const notify = (next: TableDef[]) => {
    setTables(next);
    onChange(next);
  };

  // ── Sidebar actions ──

  const startNewTable = () => {
    setDraftName('');
    setDraftDisplay('');
    setDraftCols([emptyColumn()]);
    setEditingIdx(-1); // -1 means new table
  };

  const startEditTable = (idx: number) => {
    const tbl = tables[idx];
    setDraftName(tbl.name);
    setDraftDisplay(tbl.display);
    setDraftCols(tbl.columns.map(c => ({ ...c })));
    setEditingIdx(idx);
  };

  const deleteTable = (idx: number) => {
    const next = tables.filter((_, i) => i !== idx);
    notify(next);
    if (editingIdx === idx) setEditingIdx(null);
    else if (editingIdx !== null && editingIdx > idx) setEditingIdx(editingIdx - 1);
  };

  // ── Draft column helpers ──

  const updateCol = (ci: number, patch: Partial<ColumnDef>) => {
    setDraftCols(prev => prev.map((c, i) => (i === ci ? { ...c, ...patch } : c)));
  };

  const addCol = () => {
    setDraftCols(prev => [...prev, emptyColumn()]);
  };

  const deleteCol = (ci: number) => {
    if (draftCols.length <= 1) return;
    setDraftCols(prev => prev.filter((_, i) => i !== ci));
  };

  // ── Save ──

  const saveTable = async () => {
    const cleaned = draftCols.filter(c => c.name.trim());
    if (!draftName.trim() || cleaned.length === 0) return;

    const saved: TableDef = {
      name: draftName.trim(),
      display: draftDisplay.trim() || draftName.trim(),
      columns: cleaned,
    };

    // Edit mode: call onModify callback
    if (editMode && onModify) {
      setSaving(true);
      try {
        await onModify(saved);
      } finally {
        setSaving(false);
      }
      return;
    }

    if (editingIdx === -1) {
      notify([...tables, saved]);
    } else if (editingIdx !== null) {
      const next = tables.map((t, i) => (i === editingIdx ? saved : t));
      notify(next);
    }
    setEditingIdx(null);
  };

  // ── PK toggle – uncheck others ──

  const togglePK = (ci: number) => {
    const current = draftCols[ci].pk;
    setDraftCols(prev => prev.map((c, i) => {
      if (i === ci) return { ...c, pk: !current };
      return { ...c, pk: false }; // only one PK
    }));
  };

  return (
    <div className="flex-1 flex gap-4 min-h-0">
      {/* Left sidebar – table list (hidden in edit mode) */}
      {!editMode && (
        <div className="w-[240px] shrink-0 glass-panel border border-white/5 rounded-xl p-3 flex flex-col overflow-hidden">
          <div className="flex items-center justify-between mb-3">
            <h3 className="font-code-sm text-code-sm text-on-surface font-bold">{t('designer.title')}</h3>
            <button onClick={startNewTable}
              className="text-on-surface-variant hover:text-primary transition-colors"
              title={t('designer.addTable')}>
              <span className="material-symbols-outlined text-[18px]">add_circle</span>
            </button>
          </div>
          <div className="flex-1 overflow-y-auto space-y-1.5">
            {tables.length === 0 && (
              <p className="text-[10px] text-outline text-center py-8">{t('designer.addTable')}</p>
            )}
            {tables.map((tbl, i) => (
              <div key={i}
                className={`flex items-center gap-2 px-2 py-1.5 rounded-lg cursor-pointer transition-colors group ${
                  editingIdx === i ? 'bg-primary/20 border border-primary/30' : 'hover:bg-white/5 border border-transparent'
                }`}
                onClick={() => startEditTable(i)}>
                <span className="material-symbols-outlined text-[16px] text-secondary shrink-0">table_view</span>
                <div className="min-w-0 flex-1">
                  <div className="font-code-sm text-[11px] text-on-surface truncate">{tbl.display || tbl.name}</div>
                  <div className="text-[9px] text-outline">{tbl.columns.length} cols</div>
                </div>
                <div className="flex gap-0.5 opacity-0 group-hover:opacity-100 transition-opacity">
                  <button onClick={e => { e.stopPropagation(); startEditTable(i); }}
                    className="text-on-surface-variant hover:text-primary p-0.5"
                    title={t('common.edit')}>
                    <span className="material-symbols-outlined text-[12px]">edit</span></button>
                  <button onClick={e => { e.stopPropagation(); deleteTable(i); }}
                    className="text-on-surface-variant hover:text-error p-0.5"
                    title={t('common.delete')}>
                    <span className="material-symbols-outlined text-[12px]">delete</span></button>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Right panel – table editor */}
      <div className="flex-1 glass-panel border border-white/5 rounded-xl p-4 flex flex-col overflow-hidden min-w-0">
        {editingIdx === null ? (
          <div className="flex-1 flex items-center justify-center text-on-surface-variant font-code-sm text-[12px]">
            <div className="text-center">
              <span className="material-symbols-outlined text-[48px] text-outline block mb-2">table_edit</span>
              <p>{t('designer.addTable')}</p>
              <button onClick={startNewTable}
                className="mt-3 px-4 py-1.5 bg-primary/20 text-primary border border-primary/30 rounded-lg font-code-sm text-[11px] hover:bg-primary/30 transition-colors">
                + {t('designer.addTable')}
              </button>
            </div>
          </div>
        ) : (
          <>
            {/* Table identity */}
            <div className="flex gap-3 mb-4 shrink-0">
              <div className="flex-1">
                <label className="block text-[10px] text-outline font-code-sm mb-1">{t('designer.tableName')}</label>
                <input type="text" value={draftName}
                  onChange={e => setDraftName(e.target.value)}
                  placeholder="table_name"
                  className="w-full bg-black/30 border border-outline-variant/30 rounded-lg px-3 py-1.5 font-code-sm text-on-surface text-[12px] focus:outline-none focus:border-primary" />
              </div>
              <div className="flex-1">
                <label className="block text-[10px] text-outline font-code-sm mb-1">{t('designer.displayName')}</label>
                <input type="text" value={draftDisplay}
                  onChange={e => setDraftDisplay(e.target.value)}
                  placeholder={t('designer.displayName')}
                  className="w-full bg-black/30 border border-outline-variant/30 rounded-lg px-3 py-1.5 font-code-sm text-on-surface text-[12px] focus:outline-none focus:border-primary" />
              </div>
            </div>

            {/* Column header */}
            <div className="grid grid-cols-[1fr_140px_auto_40px] gap-2 mb-2 px-2 text-[9px] text-outline font-label-caps shrink-0">
              <span>Name</span>
              <span>Type</span>
              <span className="flex gap-3">
                <span className="w-[72px] text-center">NOT NULL</span>
                <span className="w-[88px] text-center">AUTO INC</span>
                <span className="w-[30px] text-center">PK</span>
              </span>
              <span />
            </div>

            {/* Columns */}
            <div className="flex-1 overflow-y-auto space-y-1.5 min-h-0">
              {draftCols.map((col, ci) => (
                <div key={ci} className="grid grid-cols-[1fr_140px_auto_40px] gap-2 items-center bg-black/20 rounded-lg px-2 py-1.5">
                  <input type="text" value={col.name}
                    onChange={e => updateCol(ci, { name: e.target.value })}
                    placeholder="column_name"
                    className="bg-transparent border-b border-outline-variant/20 px-1 py-0.5 font-code-sm text-on-surface text-[11px] focus:outline-none focus:border-primary" />
                  <select value={col.type}
                    onChange={e => updateCol(ci, { type: e.target.value })}
                    className="bg-black/30 border border-outline-variant/20 rounded px-1 py-0.5 font-code-sm text-on-surface text-[11px] focus:outline-none focus:border-primary">
                    {COLUMN_TYPES.map(ct => (
                      <option key={ct} value={ct}>{ct}</option>
                    ))}
                  </select>
                  <div className="flex gap-3 items-center justify-start">
                    <label className="flex items-center justify-center w-[72px] cursor-pointer">
                      <input type="checkbox" checked={!!col.not_null}
                        onChange={e => updateCol(ci, { not_null: e.target.checked })}
                        className="w-3 h-3 accent-primary" />
                    </label>
                    <label className="flex items-center justify-center w-[88px] cursor-pointer">
                      <input type="checkbox" checked={!!col.auto_increment}
                        onChange={e => updateCol(ci, { auto_increment: e.target.checked })}
                        className="w-3 h-3 accent-primary" />
                    </label>
                    <label className="flex items-center justify-center w-[30px] cursor-pointer">
                      <input type="checkbox" checked={!!col.pk}
                        onChange={() => togglePK(ci)}
                        className="w-3 h-3 accent-tertiary" />
                    </label>
                  </div>
                  <button onClick={() => deleteCol(ci)}
                    className="text-on-surface-variant hover:text-error transition-colors p-0.5"
                    title={t('common.delete')}>
                    <span className="material-symbols-outlined text-[14px]">close</span></button>
                </div>
              ))}
            </div>

            {/* Add column + actions */}
            <div className="flex items-center justify-between mt-3 pt-3 border-t border-outline-variant/20 shrink-0">
              <button onClick={addCol}
                className="text-on-surface-variant hover:text-primary transition-colors font-code-sm text-[11px] flex items-center gap-1">
                <span className="material-symbols-outlined text-[14px]">add</span>
                {t('designer.addCol')}
              </button>
              <div className="flex gap-2">
                <button onClick={() => setEditingIdx(null)}
                  className="px-3 py-1.5 rounded-lg font-code-sm text-[11px] text-on-surface-variant hover:bg-white/10 transition-colors">
                  {t('common.cancel')}
                </button>
                <button onClick={saveTable} disabled={saving}
                  className={`px-3 py-1.5 rounded-lg font-code-sm text-[11px] transition-colors flex items-center gap-1.5 ${
                    editMode
                      ? 'bg-primary text-black font-bold hover:bg-primary-fixed'
                      : 'bg-primary/20 text-primary border border-primary/30 hover:bg-primary/30'
                  }`}>
                  {saving && <span className="material-symbols-outlined text-[14px] animate-spin">progress_activity</span>}
                  {editMode ? t('designer.saveChanges') : t('designer.saveTable')}
                </button>
              </div>
            </div>
          </>
        )}
      </div>

      {/* Bottom bar – Create All (hidden in edit mode) */}
      {!editMode && (
        <div className="fixed bottom-6 left-1/2 -translate-x-1/2 z-40">
          {tables.length > 0 && (
            <button onClick={() => onCreate(tables)}
              className="bg-primary text-black font-bold px-6 py-2.5 rounded-full font-code-sm text-[12px] hover:bg-primary-fixed transition-colors shadow-[0_4px_24px_rgba(0,0,0,0.5)] flex items-center gap-2">
              <span className="material-symbols-outlined text-[16px]">rocket_launch</span>
              {t('designer.createAll')} ({tables.length})
            </button>
          )}
        </div>
      )}
    </div>
  );
}
