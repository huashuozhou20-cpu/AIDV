import { useState, useEffect, useCallback, useRef } from 'react';
import { getScenario, fetchRows, insertRow, updateRow, deleteRow, importCSV, downloadExport, activateScenario } from '../api';
import type { ScenarioTable, ColumnInfo } from '../api';
import RowEditDrawer from '../components/RowEditDrawer';
import { useT } from '../i18n';

export default function DataTableView({ scenarioId, scenarioName, onBack, onEditStructure }: {
  scenarioId: number;
  scenarioName: string;
  onBack: () => void;
  onEditStructure?: (tableId: number, tableName: string, displayName: string) => void;
}) {
  const { t } = useT();
  const [tables, setTables] = useState<ScenarioTable[]>([]);
  const [activeTable, setActiveTable] = useState<string>('');
  const [columns, setColumns] = useState<ColumnInfo[]>([]);
  const [rows, setRows] = useState<string[][]>([]);
  const [total, setTotal] = useState(0);
  const [page, setPage] = useState(1);
  const [search, setSearch] = useState('');
  const [loading, setLoading] = useState(false);
  const [editingCell, setEditingCell] = useState<{ row: number; col: number } | null>(null);
  const [editValue, setEditValue] = useState('');
  const [showAdd, setShowAdd] = useState(false);
  const [newRowData, setNewRowData] = useState<Record<string, string>>({});
  const [editRow, setEditRow] = useState<{ index: number; data: string[] } | null>(null);
  const [toast, setToast] = useState<string | null>(null);
  const fileRef = useRef<HTMLInputElement>(null);
  const PAGE_SIZE = 50;

  const toast_ = (msg: string) => { setToast(msg); setTimeout(() => setToast(c => c === msg ? null : c), 3000); };

  // Activate scenario + load
  useEffect(() => {
    activateScenario(scenarioId).catch(() => {});
    getScenario(scenarioId).then(res => {
      if (res.tables) {
        setTables(res.tables);
        if (res.tables.length > 0 && !activeTable) {
          setActiveTable(res.tables[0].table_name);
        }
      }
    }).catch(() => {});
  }, [scenarioId]);

  const loadRows = useCallback(async () => {
    if (!activeTable) return;
    setLoading(true);
    try {
      const res = await fetchRows(activeTable, page, PAGE_SIZE, search);
      if (res.status === 'success') {
        setColumns((res.columns || []).map(c => ({ name: c, type: 'STRING' })));
        setRows(res.data || []);
        setTotal(res.total || 0);
      }
    } catch {}
    setLoading(false);
  }, [activeTable, page, search]);

  useEffect(() => { loadRows(); }, [loadRows]);

  // Also fetch accurate column types from the scenario metadata
  useEffect(() => {
    const t = tables.find(t => t.table_name === activeTable);
    if (t?.columns) {
      setColumns(t.columns.map(c => ({ name: c.name, type: c.type || 'STRING' })));
    }
  }, [activeTable, tables]);

  const totalPages = Math.max(1, Math.ceil(total / PAGE_SIZE));

  // Inline cell edit
  const startEdit = (rowIdx: number, colIdx: number, value: string) => {
    setEditingCell({ row: rowIdx, col: colIdx });
    setEditValue(value);
  };

  const saveEdit = async () => {
    if (!editingCell) return;
    const colName = columns[editingCell.col]?.name;
    if (!colName) return;
    const pkCol = columns[0]?.name || 'id';
    const pkVal = rows[editingCell.row]?.[0];
    if (!pkVal) return;

    try {
      const res = await updateRow(activeTable, pkVal, { [colName]: editValue });
      if (res.status === 'success') {
        setRows(prev => prev.map((r, i) => i === editingCell.row ? r.map((c, j) => j === editingCell.col ? editValue : c) : r));
        toast_(t('data.saved'));
      } else {
        toast_(res.message || t('data.saveFailed'));
      }
    } catch { toast_(t('data.saveFailed')); }
    setEditingCell(null);
  };

  const handleDeleteRow = async (rowIdx: number) => {
    const pkVal = rows[rowIdx]?.[0];
    if (!pkVal || !confirm(t('data.confirmDelete'))) return;
    try {
      const res = await deleteRow(activeTable, pkVal);
      if (res.status === 'success') {
        setRows(prev => prev.filter((_, i) => i !== rowIdx));
        setTotal(prev => prev - 1);
        toast_(t('data.deleted'));
      }
    } catch { toast_(t('data.deleteFailed')); }
  };

  const handleAdd = async () => {
    if (!Object.values(newRowData).some(v => v.trim())) return;
    try {
      const res = await insertRow(activeTable, newRowData);
      if (res.status === 'success') {
        setShowAdd(false); setNewRowData({});
        loadRows(); toast_(t('data.added'));
      } else {
        toast_(res.message || t('data.addFailed'));
      }
    } catch { toast_(t('data.addFailed')); }
  };

  const handleImport = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    if (!activeTable) { toast_('未选中数据表，请先点击表名标签'); if (fileRef.current) fileRef.current.value = ''; return; }
    try {
      const res = await importCSV(activeTable, file);
      let msg = t('data.importDone').replace('N', String(res.imported)).replace('M', String(res.errors.length));
      if (res.errors.length > 0) {
        msg += ' — ' + res.errors.slice(0, 3).map((e: any) => `[行${e.row}] ${e.error}`).join('; ');
        if (res.errors.length > 3) msg += ` ...等${res.errors.length}条`;
      }
      toast_(msg);
      loadRows();
    } catch (err: any) {
      console.error('Import failed:', err);
      const msg = err?.message || String(err);
      toast_(t('data.importFailed') + (msg ? ': ' + msg : ''));
    }
    if (fileRef.current) fileRef.current.value = '';
  };

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      {/* Header */}
      <div className="flex items-center justify-between mb-4 shrink-0">
        <div className="flex items-center gap-3">
          <button onClick={onBack} className="text-on-surface-variant hover:text-on-surface transition-colors">
            <span className="material-symbols-outlined text-[20px]">arrow_back</span></button>
          <h1 className="font-headline-md text-headline-md text-on-surface font-bold">{scenarioName}</h1>
        </div>
        <div className="flex items-center gap-2">
          <input type="file" ref={fileRef} accept=".csv" className="hidden" onChange={handleImport} />
          <button onClick={() => {
            const tbl = tables.find(t => t.table_name === activeTable);
            if (tbl) onEditStructure?.(tbl.id, tbl.table_name, tbl.display_name);
          }}
            className="px-3 py-1.5 rounded-lg font-code-sm text-[10px] text-on-surface-variant border border-outline-variant/30 hover:border-primary/50 hover:text-primary transition-colors flex items-center gap-1">
            <span className="material-symbols-outlined text-[14px]">edit_square</span>{t('data.editStructure')}</button>
          <button onClick={() => fileRef.current?.click()}
            className="px-3 py-1.5 rounded-lg font-code-sm text-[10px] text-on-surface-variant border border-outline-variant/30 hover:border-white/30 transition-colors flex items-center gap-1">
            <span className="material-symbols-outlined text-[14px]">upload_file</span>{t('data.import')}</button>
          <button onClick={async () => {
            try { await downloadExport(activeTable); } catch { toast_(t('data.exportFailed')); }
          }}
            className="px-3 py-1.5 rounded-lg font-code-sm text-[10px] text-on-surface-variant border border-outline-variant/30 hover:border-white/30 transition-colors flex items-center gap-1">
            <span className="material-symbols-outlined text-[14px]">download</span>{t('data.export')}</button>
        </div>
      </div>

      {/* Table Tabs */}
      <div className="flex gap-1 mb-3 shrink-0 border-b border-outline-variant/20 pb-0 overflow-x-auto">
        {tables.map(t => (
          <button key={t.id} onClick={() => { setActiveTable(t.table_name); setPage(1); setSearch(''); }}
            className={`px-4 py-2 rounded-t-lg font-code-sm text-[11px] whitespace-nowrap transition-colors ${
              activeTable === t.table_name
                ? 'bg-primary/10 text-primary border-t border-primary font-bold'
                : 'text-on-surface-variant hover:text-on-surface hover:bg-white/5'}`}>
            <span className="material-symbols-outlined text-[14px] align-middle mr-1">table_view</span>
            {t.display_name}
            <span className="ml-1 text-[10px] text-outline">({t.row_count ?? '?'})</span>
          </button>
        ))}
        {tables.length === 0 && (
          <div className="text-on-surface-variant/50 font-code-sm text-[11px] py-2">{t('data.noTables')}</div>
        )}
      </div>

      {/* Search & Actions */}
      {activeTable && (
        <div className="flex items-center gap-3 mb-3 shrink-0">
          <div className="relative flex-1 max-w-xs">
            <span className="material-symbols-outlined absolute left-2 top-1/2 -translate-y-1/2 text-on-surface-variant text-[14px]">search</span>
            <input type="text" placeholder={t('data.search')} value={search}
              onChange={e => { setSearch(e.target.value); setPage(1); }}
              className="w-full bg-black/30 border border-outline-variant/30 rounded-lg pl-8 pr-3 py-1.5 font-code-sm text-[11px] text-on-surface focus:outline-none focus:border-primary" />
          </div>
          <button onClick={() => { setShowAdd(!showAdd); setNewRowData({}); }}
            className="bg-primary/20 text-primary border border-primary/30 px-3 py-1.5 rounded-lg font-code-sm text-[10px] hover:bg-primary/30 transition-colors flex items-center gap-1">
            <span className="material-symbols-outlined text-[14px]">add</span>{t('data.addRow')}</button>
          <span className="font-code-sm text-[10px] text-outline ml-auto">{total} rows</span>
        </div>
      )}

      {/* Add Row Form */}
      {showAdd && activeTable && (
        <div className="glass-panel border border-primary/30 rounded-xl p-4 mb-3 shrink-0">
          <h3 className="font-code-md text-code-md text-on-surface font-bold mb-3">{t('data.addForm')}</h3>
          <div className="grid grid-cols-4 gap-3">
            {columns.filter(c => c.name !== 'id').map(col => (
              <div key={col.name}>
                <label className="block text-[10px] font-code-sm text-on-surface-variant mb-1">{col.name}</label>
                <input type="text" value={newRowData[col.name] || ''}
                  onChange={e => setNewRowData(prev => ({ ...prev, [col.name]: e.target.value }))}
                  className="w-full bg-black/40 border border-outline-variant/30 rounded px-2 py-1 font-code-sm text-[11px] text-on-surface focus:outline-none focus:border-primary" />
              </div>
            ))}
          </div>
          <div className="flex gap-2 mt-3">
            <button onClick={handleAdd} className="bg-primary text-black font-bold px-4 py-1.5 rounded text-[10px] font-code-sm hover:bg-primary-fixed transition-colors">{t('data.confirmAdd')}</button>
            <button onClick={() => setShowAdd(false)} className="px-4 py-1.5 rounded text-[10px] font-code-sm text-on-surface-variant hover:bg-white/10 transition-colors">{t('data.cancel')}</button>
          </div>
        </div>
      )}

      {/* Data Table */}
      {activeTable && (
        <div className="glass-panel border border-white/5 rounded-lg flex-1 overflow-auto min-h-0">
          <table className="w-full text-left font-code-sm text-[11px] border-collapse">
            <thead className="sticky top-0 bg-surface-container/80 backdrop-blur z-10">
              <tr className="border-b border-outline-variant/20 text-outline">
                <th className="px-3 py-2 w-16 font-medium">#</th>
                {columns.map(c => <th key={c.name} className="px-3 py-2 font-medium uppercase tracking-wider">{c.name}</th>)}
                <th className="px-3 py-2 w-20 font-medium text-right">{t('common.edit')}</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-outline-variant/10">
              {loading ? (
                <tr><td colSpan={columns.length + 2} className="text-center py-8 text-outline">
                  <span className="material-symbols-outlined text-[20px] animate-spin align-middle mr-2">sync</span>{t('data.loading')}</td></tr>
              ) : rows.length === 0 ? (
                <tr><td colSpan={columns.length + 2} className="text-center py-8 text-on-surface-variant/50">
                  {search ? t('data.noMatch') : t('data.noData')}</td></tr>
              ) : (
                rows.map((row, ri) => (
                  <tr key={ri} className="hover:bg-white/5 transition-colors group">
                    <td className="px-3 py-1.5 text-outline text-[10px]">{ri + 1 + (page - 1) * PAGE_SIZE}</td>
                    {row.map((cell, ci) => (
                      <td key={ci} className="px-3 py-1.5 text-on-surface cursor-pointer hover:text-primary transition-colors"
                        onDoubleClick={() => startEdit(ri, ci, cell)}>
                        {editingCell?.row === ri && editingCell?.col === ci ? (
                          <input type="text" value={editValue}
                            onChange={e => setEditValue(e.target.value)}
                            onBlur={saveEdit}
                            onKeyDown={e => { if (e.key === 'Enter') saveEdit(); if (e.key === 'Escape') setEditingCell(null); }}
                            className="bg-black/60 border border-primary rounded px-1 py-0 text-[11px] text-on-surface w-full focus:outline-none"
                            autoFocus />
                        ) : (
                          <span className={!cell || cell === 'NULL' ? 'text-outline/50 italic' : ''}>{cell || 'NULL'}</span>
                        )}
                      </td>
                    ))}
                    <td className="px-3 py-1.5 text-right whitespace-nowrap">
                      <button onClick={() => setEditRow({ index: ri, data: row })}
                        className="text-on-surface-variant hover:text-primary transition-colors p-0.5 mr-1" title={t('data.edit')}>
                        <span className="material-symbols-outlined text-[14px]">edit</span></button>
                      <button onClick={() => handleDeleteRow(ri)}
                        className="text-on-surface-variant hover:text-error transition-colors p-0.5 opacity-0 group-hover:opacity-100" title={t('data.delete')}>
                        <span className="material-symbols-outlined text-[14px]">delete</span></button>
                    </td>
                  </tr>
                ))
              )}
            </tbody>
          </table>
        </div>
      )}

      {/* Pagination */}
      {totalPages > 1 && (
        <div className="flex items-center justify-between mt-2 shrink-0 text-[10px] text-outline font-code-sm">
          <span>{t('data.pageInfo').replace('N', String(total)).replace('P', `${page}/${totalPages}`)}</span>
          <span className="flex gap-1">
            <button disabled={page <= 1} onClick={() => setPage(p => Math.max(1, p - 1))}
              className="px-2 py-0.5 rounded hover:text-primary disabled:opacity-30">{t('data.prev')}</button>
            <button disabled={page >= totalPages} onClick={() => setPage(p => Math.min(totalPages, p + 1))}
              className="px-2 py-0.5 rounded hover:text-primary disabled:opacity-30">{t('data.next')}</button>
          </span>
        </div>
      )}

      {/* Edit Drawer */}
      {editRow && (
        <RowEditDrawer row={editRow.data} columns={columns}
          tableName={activeTable}
          onSave={async (data) => {
            const pkCol = columns[0]?.name || 'id';
            const pkVal = editRow.data[0];
            try {
              const res = await updateRow(activeTable, pkVal, data);
              if (res.status === 'success') {
                setRows(prev => prev.map((r, i) => i === editRow.index
                  ? columns.map(c => data[c.name] ?? r[columns.findIndex(cc => cc.name === c.name)] ?? '')
                  : r));
                toast_(t('data.saved'));
              }
            } catch {}
            setEditRow(null);
          }}
          onClose={() => setEditRow(null)} />
      )}

      {/* Toast */}
      {toast && (
        <div className="fixed bottom-6 left-1/2 -translate-x-1/2 bg-surface text-on-surface border border-white/10 shadow-[0_4px_24px_rgba(0,0,0,0.5)] rounded-full px-6 py-2 font-code-sm text-[11px] z-[100]">
          {toast}</div>
      )}
    </div>
  );
}
