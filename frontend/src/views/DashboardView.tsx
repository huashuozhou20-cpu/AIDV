import { useState, useRef, useEffect, useCallback, type MouseEvent } from 'react';
import { executeQuery, fetchSchemaTree, fetchQueryHistory, dropTable, parseSQLFile, executeImport,
  fetchWorkspaceTree, createWorkspaceFolder, renameWorkspaceFolder, deleteWorkspaceFolder,
  createWorkspaceTable, renameWorkspaceTable, moveWorkspaceTable, deleteWorkspaceTable,
} from '../api';
import type { QueryResult, SchemaTree, TableInfo, HistoryEntry, ParsedSQL, ImportResult, WorkspaceFolder, WorkspaceTable, WorkspaceTree } from '../api';
import ChatPanel from '../components/ChatPanel';
import { useT } from '../i18n';

export default function DashboardView() {
  const { t } = useT();
  const [leftPaneWidth, setLeftPaneWidth] = useState(280);
  const isDragging = useRef(false);
  const [showChat, setShowChat] = useState(false);

  // ── Workspace tree state ──
  const [expandedNodes, setExpandedNodes] = useState<Record<string, boolean>>({});
  const [selectedNode, setSelectedNode] = useState<string>('');
  const [workspace, setWorkspace] = useState<WorkspaceTree | null>(null);
  const [schemaTree, setSchemaTree] = useState<SchemaTree | null>(null);
  const [editingNode, setEditingNode] = useState<string | null>(null); // "folder:3" or "table:5"
  const [editValue, setEditValue] = useState('');
  const [dragOverFolder, setDragOverFolder] = useState<number | null>(null);
  const [activeFolder, setActiveFolder] = useState<{ id: number; name: string } | null>(null); // folder context for SQL isolation
  const [editorHeight, setEditorHeight] = useState(() => {
    const saved = localStorage.getItem('dashboard_editor_h');
    return saved ? Number(saved) : 280;
  });

  const [activeTab, setActiveTab] = useState<'editor' | 'history'>('editor');
  const [isQueryRunning, setIsQueryRunning] = useState(false);
  const [queryResult, setQueryResult] = useState<QueryResult | null>(null);
  const [queryError, setQueryError] = useState<string | null>(null);
  const [queryText, setQueryText] = useState('SELECT * FROM emp LIMIT 100;');
  const [toastMessage, setToastMessage] = useState<string | null>(null);
  const [queryHistory, setQueryHistory] = useState<HistoryEntry[]>([]);
  const [filterText, setFilterText] = useState('');
  const [currentPage, setCurrentPage] = useState(0);
  const PAGE_SIZE = 50;

  // ── Import state ──
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [showImport, setShowImport] = useState(false);
  const [importData, setImportData] = useState<ParsedSQL | null>(null);
  const [importing, setImporting] = useState(false);

  const showToast = (msg: string) => { setToastMessage(msg); setTimeout(() => setToastMessage(c => c === msg ? null : c), 3000); };
  const toggleNode = (id: string, e: MouseEvent) => { e.stopPropagation(); setExpandedNodes(p => ({ ...p, [id]: !p[id] })); };

  const runQuery = useCallback(async () => {
    if (isQueryRunning) return;
    setIsQueryRunning(true); setQueryError(null); setQueryResult(null);

    let sql = queryText;
    const trimmed = sql.trim().toUpperCase();

    // In folder context, rewrite workspace table display_names → RMDB table_names
    // Only for DML, never for DDL (CREATE/DROP/ALTER/DESC/SHOW)
    const isDDL = /^(CREATE|DROP|ALTER|DESC|SHOW|DESCRIBE)\s/i.test(trimmed);
    if (!isDDL && activeFolder && workspace) {
      const folderTables = workspace.tables.filter(t => t.folder_id === activeFolder.id);
      for (const t of folderTables) {
        if (t.display_name !== t.table_name) {
          const escaped = t.display_name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
          const before = sql;
          sql = sql.replace(new RegExp(`\\b${escaped}\\b`, 'gi'), t.table_name);
        }
      }
    }

    try {
      const result = await executeQuery(sql);
      setQueryResult(result);
      if (result.status === 'error') setQueryError(result.message ?? 'Unknown error');
    } catch (err: any) { setQueryError(err.message ?? 'Query failed'); }
    finally { setIsQueryRunning(false); setCurrentPage(0); setFilterText(''); }
  }, [isQueryRunning, queryText, activeFolder, workspace]);

  useEffect(() => {
    const h = (e: KeyboardEvent) => { if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') { e.preventDefault(); runQuery(); } };
    window.addEventListener('keydown', h);
    return () => window.removeEventListener('keydown', h);
  }, [runQuery]);

  const refreshWorkspace = useCallback(async () => {
    try {
      const [ws, st] = await Promise.all([fetchWorkspaceTree(), fetchSchemaTree()]);
      setWorkspace(ws);
      setSchemaTree(st);
      const exp: Record<string, boolean> = {};
      ws.folders.forEach(f => { exp[`folder:${f.id}`] = true; });
      ws.tables.forEach(t => { exp[`table:${t.id}`] = false; });
      setExpandedNodes(exp);
    } catch {}
  }, []);

  useEffect(() => { refreshWorkspace(); }, []);

  const handleTableClick = async (tableName: string, e: MouseEvent) => {
    e.stopPropagation(); setSelectedNode(tableName);
    const sql = `SELECT * FROM ${tableName} LIMIT 100;`;
    setQueryText(sql); setActiveTab('editor');
    setIsQueryRunning(true); setQueryError(null); setQueryResult(null);
    try {
      const result = await executeQuery(sql);
      setQueryResult(result);
      if (result.status === 'error') setQueryError(result.message ?? 'Error');
    } catch (err: any) { setQueryError(err.message ?? 'Query failed'); }
    finally { setIsQueryRunning(false); }
  };

  // ── Workspace actions bundle (passed to recursive tree nodes) ──
  const wsActions = {
    toggleNode: (id: string) => setExpandedNodes(p => ({ ...p, [id]: !p[id] })),
    selectNode: (id: string) => setSelectedNode(id),
    selectFolder: (id: number, name: string) => {
      setActiveFolder(prev => prev?.id === id ? null : { id, name });
      setSelectedNode(`folder:${id}`);
    },
    startRename: (id: string, val: string) => { setEditingNode(id); setEditValue(val); },
    setEditValue: (v: string) => setEditValue(v),
    cancelEdit: () => setEditingNode(null),
    setDragOverFolder: (id: number | null) => setDragOverFolder(id),
    selectTable: (tableName: string, tableId: number, e: MouseEvent) => handleTableClick(tableName, e),
    showSchema: (tableName: string) => { setQueryText(`DESC ${tableName};`); setActiveTab('editor'); },
    async renameFolder(id: number) {
      if (editValue.trim()) { await renameWorkspaceFolder(id, editValue.trim()); refreshWorkspace(); }
      setEditingNode(null);
    },
    async renameTable(id: number) {
      if (editValue.trim()) { await renameWorkspaceTable(id, editValue.trim()); refreshWorkspace(); }
      setEditingNode(null);
    },
    async deleteFolder(id: number, name: string) {
      if (confirm(`Delete folder "${name}"?`)) { await deleteWorkspaceFolder(id); refreshWorkspace(); }
    },
    async deleteTable(id: number, name: string) {
      if (confirm(`Delete table "${name}"?`)) { await deleteWorkspaceTable(id); refreshWorkspace(); }
    },
    async dropOnFolder(tableId: number, folderId: number) {
      setDragOverFolder(null);
      await moveWorkspaceTable(tableId, folderId);
      refreshWorkspace();
    },
    async createTableInFolder(folderId: number, folderName: string) {
      const name = prompt(`New table in "${folderName}":`);
      if (name?.trim()) { await createWorkspaceTable(name.trim(), folderId); refreshWorkspace(); }
    },
  };

  const loadHistory = useCallback(async () => {
    try { setQueryHistory(await fetchQueryHistory(50)); } catch {}
  }, []);

  const handleTabChange = (tab: 'editor' | 'history') => { setActiveTab(tab); if (tab === 'history') loadHistory(); };

  const filteredData = queryResult?.data.filter(row =>
    !filterText || row.some(cell => cell.toLowerCase().includes(filterText.toLowerCase()))
  ) ?? [];
  const totalPages = Math.max(1, Math.ceil(filteredData.length / PAGE_SIZE));
  const pageData = filteredData.slice(currentPage * PAGE_SIZE, (currentPage + 1) * PAGE_SIZE);

  const handleExport = () => {
    if (!queryResult || queryResult.columns.length === 0) return;
    const csv = [queryResult.columns.join(','), ...queryResult.data.map(row => row.map(v => `"${v.replace(/"/g, '""')}"`).join(','))].join('\n');
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a'); a.href = url;
    a.download = `query_${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.csv`;
    a.click(); URL.revokeObjectURL(url);
  };

  useEffect(() => {
    const mm = (e: any) => { if (!isDragging.current) return; setLeftPaneWidth(Math.max(200, Math.min(e.clientX - 80, 800))); };
    const mu = () => { if (isDragging.current) { isDragging.current = false; document.body.style.cursor = 'default'; document.body.style.userSelect = 'auto'; } };
    window.addEventListener('mousemove', mm); window.addEventListener('mouseup', mu);
    return () => { window.removeEventListener('mousemove', mm); window.removeEventListener('mouseup', mu); };
  }, []);

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      {/* Status Bar */}
      <div className="flex justify-between items-center mb-4 glass-panel rounded-lg p-2 px-4 shrink-0">
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full bg-secondary shadow-[0_0_8px_rgba(78,222,163,0.5)] animate-pulse"></div>
          <span className="font-code-sm text-code-sm text-on-surface">{t('dashboard.connected')}</span>
          <span className="text-[10px] text-outline ml-2">{schemaTree?.tables.length ?? 0} {t('dashboard.tables')}</span>
        </div>
        <button onClick={() => setShowChat(!showChat)}
          className={`px-2 py-0.5 rounded font-label-caps text-label-caps transition-all cursor-pointer flex items-center gap-1 ${showChat ? 'bg-primary/20 text-primary border border-primary/30' : 'bg-surface-variant border border-outline-variant/50 text-on-surface hover:bg-white/20'}`}>
          <span className="material-symbols-outlined text-[12px]">psychology</span> AI
        </button>
      </div>

      <div className="flex gap-gutter pb-gutter flex-1">
        {/* Object Explorer — VS Code-style tree */}
        <div className="glass-panel rounded-lg p-panel-padding flex flex-col relative shrink-0 overflow-hidden" style={{ width: `${leftPaneWidth}px` }}>
          <div className="absolute top-0 -right-[12px] w-[24px] h-full cursor-col-resize z-50 flex items-center justify-center group"
            onMouseDown={e => { e.preventDefault(); isDragging.current = true; document.body.style.cursor = 'col-resize'; document.body.style.userSelect = 'none'; }}>
            <div className="w-[3px] h-16 bg-white/10 group-hover:bg-primary/50 group-active:bg-primary rounded-full transition-colors"></div>
          </div>
          <div className="flex items-center justify-between mb-3 border-b border-outline-variant/30 pb-2">
            <h2 className="font-code-md text-code-md text-on-surface font-bold flex items-center gap-1.5">
              <span className="material-symbols-outlined text-[18px] shrink-0">account_tree</span> EXPLORER
            </h2>
            <div className="flex items-center gap-0.5 shrink-0">
              <button className="text-on-surface-variant hover:text-primary transition-colors p-0.5" title="New Folder"
                onClick={async () => {
                  const name = prompt('Folder name:');
                  if (name?.trim()) { await createWorkspaceFolder(name.trim()); refreshWorkspace(); }
                }}>
                <span className="material-symbols-outlined text-[16px]">create_new_folder</span></button>
              <button className="text-on-surface-variant hover:text-primary transition-colors p-0.5" title="New Table"
                onClick={async () => {
                  const name = prompt('Table name:');
                  if (name?.trim()) { await createWorkspaceTable(name.trim()); refreshWorkspace(); }
                }}>
                <span className="material-symbols-outlined text-[16px]">note_add</span></button>
              <button className="text-on-surface-variant hover:text-primary transition-colors p-0.5" title={t('dashboard.refresh')}
                onClick={() => refreshWorkspace()}>
                <span className="material-symbols-outlined text-[16px]">refresh</span></button>
              <button className="text-on-surface-variant hover:text-primary transition-colors p-0.5" title={t('import.title')}
                onClick={() => fileInputRef.current?.click()}>
                <span className="material-symbols-outlined text-[16px]">upload_file</span></button>
            </div>
          </div>
          {/* Hidden file input */}
          <input ref={fileInputRef} type="file" accept=".sql" className="hidden"
            onChange={async (e) => {
              const file = e.target.files?.[0];
              if (!file) return;
              try { const data = await parseSQLFile(file); setImportData(data); setShowImport(true); } catch { showToast('Failed to parse SQL file'); }
              if (fileInputRef.current) fileInputRef.current.value = '';
            }} />
          <div className="flex-1 overflow-auto pr-1 font-code-sm text-code-sm text-on-surface-variant">
            {workspace?.folders && workspace?.tables ? (
              <ul className="space-y-0.5 min-w-max">
                <li>
                  <div className={`flex items-center gap-1.5 py-1 px-1.5 rounded cursor-pointer transition-colors ${selectedNode === '__root__' ? 'bg-primary/20 text-primary' : 'hover:bg-white/5 text-on-surface'}`}
                    onClick={e => { wsActions.toggleNode('__root__'); setSelectedNode('__root__'); }}>
                    <span className={`material-symbols-outlined text-[16px] shrink-0 transition-transform ${expandedNodes['__root__'] !== false ? 'rotate-90' : ''}`}>chevron_right</span>
                    <span className="material-symbols-outlined text-[16px] shrink-0 text-primary">dns</span>
                    <span className="whitespace-nowrap">RMDB</span>
                  </div>
                  {expandedNodes['__root__'] !== false && (
                    <ul className="ml-5 mt-0.5 border-l border-outline-variant/30 pl-2 space-y-0.5">
                      {workspace.folders.filter(f => !f.parent_id).map(folder => (
                        <WorkspaceFolderNode key={`folder:${folder.id}`} folder={folder}
                          allFolders={workspace.folders} tables={workspace.tables}
                          state={{ expandedNodes, selectedNode, editingNode, editValue, dragOverFolder, activeFolderId: activeFolder?.id }}
                          actions={wsActions} />
                      ))}
                      {workspace.tables.filter(t => !t.folder_id).map(table => (
                        <WorkspaceTableRow key={`table:${table.id}`} table={table}
                          state={{ expandedNodes, selectedNode, editingNode, editValue, activeFolderId: activeFolder?.id }}
                          actions={wsActions} />
                      ))}
                    </ul>
                  )}
                </li>
              </ul>
            ) : (
              <div className="flex items-center justify-center h-full text-on-surface-variant/50 text-[11px]">{t('dashboard.loading')}</div>
            )}
          </div>
        </div>

        {/* Editor + Results + Chat */}
        <div className="flex-1 flex gap-gutter min-w-0">
          <div className="flex-1 flex flex-col gap-gutter min-w-0">
            {/* Folder context banner */}
            {activeFolder && (
              <div className="bg-primary/10 border border-primary/20 rounded-lg px-3 py-1.5 flex items-center gap-2 shrink-0">
                <span className="material-symbols-outlined text-[16px] text-primary">folder_open</span>
                <span className="font-code-sm text-[11px] text-primary">当前文件夹: <b>{activeFolder.name}</b></span>
                <span className="text-[10px] text-outline ml-1">— DML 自动改写表名到 f{activeFolder.id}_*，DDL 请用实际表名（见侧栏）</span>
                <button onClick={() => setActiveFolder(null)} className="ml-auto text-on-surface-variant hover:text-on-surface p-0.5">
                  <span className="material-symbols-outlined text-[14px]">close</span></button>
              </div>
            )}
            {/* Editor */}
            <div className="glass-panel border border-white/5 rounded-lg flex flex-col overflow-hidden" style={{ height: `${editorHeight}px`, flexShrink: 0 }}>
              <div className="bg-black/20 px-0 flex justify-between items-center shrink-0">
                <div className="flex h-9 pt-1">
                  <div className={`px-4 flex items-center gap-2 rounded-t-lg mx-1 cursor-pointer transition-colors ${activeTab === 'editor' ? 'bg-black/40 border-t border-primary' : 'hover:bg-white/5'}`}
                    onClick={() => handleTabChange('editor')}>
                    <span className={`material-symbols-outlined text-[16px] ${activeTab === 'editor' ? 'text-primary' : 'text-outline'}`}>description</span>
                    <span className={`font-code-sm text-code-sm ${activeTab === 'editor' ? 'text-on-surface' : 'text-outline'}`}>{t('dashboard.editor')}</span>
                  </div>
                  <div className={`px-4 flex items-center gap-2 rounded-t-lg mx-1 cursor-pointer transition-colors ${activeTab === 'history' ? 'bg-black/40 border-t border-primary' : 'hover:bg-white/5'}`}
                    onClick={() => handleTabChange('history')}>
                    <span className={`material-symbols-outlined text-[16px] ${activeTab === 'history' ? 'text-primary' : 'text-outline'}`}>history</span>
                    <span className={`font-code-sm text-code-sm ${activeTab === 'history' ? 'text-on-surface' : 'text-outline'}`}>{t('dashboard.history')}</span>
                  </div>
                  <div className="px-3 flex items-center justify-center hover:bg-white/10 rounded-md my-1 cursor-pointer text-outline transition-colors"
                    onClick={() => { setQueryText(''); setActiveTab('editor'); }} title={t('dashboard.newQuery')}>
                    <span className="material-symbols-outlined text-[18px]">add</span></div>
                </div>
                <div className="flex items-center gap-1 pr-2">
                  <button className="p-1 rounded hover:bg-white/10 text-on-surface-variant hover:text-tertiary transition-all" title={t('dashboard.aiExplain')}
                    onClick={async () => {
                      if (!queryText.trim()) return;
                      showToast(t('dashboard.aiExplain') + '...');
                      try {
                        const res = await fetch('/api/practice/explain', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ sql: queryText }) });
                        const d = await res.json();
                        setQueryError(null); setQueryResult({ status: 'success', columns: [t('dashboard.aiExplain')], data: d.explanation?.split('\n').filter((l: string) => l.trim()).map((l: string) => [l]) ?? [], execution_time_ms: 0 });
                        setActiveTab('editor');
                      } catch { showToast(t('analyzer.aiFailed')); }
                    }}>
                    <span className="material-symbols-outlined text-[16px]">lightbulb</span></button>
                  {queryResult && !isQueryRunning && (
                    <span className={`font-code-sm text-code-sm mr-1 ${queryResult.status === 'error' ? 'text-error' : 'text-primary'}`}>
                      {queryResult.status === 'error' ? t('common.error') : `${queryResult.execution_time_ms}ms`}</span>)}
                  <button className={`p-1 rounded flex items-center justify-center transition-all ${isQueryRunning ? 'text-primary' : 'hover:bg-white/10 text-on-surface-variant hover:text-primary'}`}
                    onClick={runQuery} disabled={isQueryRunning} title={t('dashboard.run')}>
                    <span className={`material-symbols-outlined text-[18px] ${isQueryRunning ? 'animate-spin' : ''}`}>{isQueryRunning ? 'sync' : 'play_arrow'}</span></button>
                </div>
              </div>
              <div className="p-0 font-code-md text-code-md bg-black/40 flex-1 leading-relaxed flex min-h-0">
                {/* Line numbers — scroll-locked to textarea */}
                <div className="bg-black/20 w-12 border-r border-white/5 flex flex-col items-end pr-3 pt-3 text-[11px] text-outline/40 select-none shrink-0 overflow-hidden">
                  {queryText.split('\n').map((_, i) => <span key={i} className="leading-[1.625rem]">{i + 1}</span>)}</div>
                {/* Editor body */}
                <div className="flex-1 flex flex-col min-h-0 min-w-0">
                  {activeTab === 'editor' ? (
                    <textarea className="flex-1 w-full bg-transparent text-[#b5cea8] font-code-sm border-none outline-none resize-none px-2 pt-3 pb-2 leading-[1.625rem]"
                      value={queryText} onChange={e => setQueryText(e.target.value)} spellCheck={false}
                      style={{ overflowY: 'auto' }} />
                  ) : (
                    <div className="flex-1 text-on-surface-variant/80 font-code-sm text-sm px-2 pt-3 overflow-y-auto">
                      {queryHistory.length > 0 ? queryHistory.map((entry, i) => (
                        <div key={i} className="mb-1.5 hover:bg-white/5 rounded px-1 py-0.5 cursor-pointer transition-colors"
                          onClick={() => { setQueryText(entry.sql + (entry.sql.endsWith(';') ? '' : ';')); setActiveTab('editor'); }}>
                          <div className="flex items-center gap-2">
                            <span className="text-outline text-[10px] shrink-0">{entry.time.slice(11, 19)}</span>
                            <span className={`text-[10px] font-bold shrink-0 ${entry.status === 'success' ? 'text-secondary' : 'text-error'}`}>
                              {entry.status === 'success' ? `${entry.elapsed_ms}ms` : entry.status}</span>
                          </div>
                          <div className="text-on-surface-variant text-[12px] truncate">{entry.sql.length > 80 ? entry.sql.slice(0, 80) + '...' : entry.sql}</div>
                        </div>
                      )) : <div className="text-outline italic">{t('dashboard.noHistory')}</div>}
                    </div>
                  )}
                </div>
              </div>
            </div>

            {/* Drag handle: editor ↔ results */}
            <div
              onMouseDown={e => {
                e.preventDefault();
                const startY = e.clientY;
                const startH = editorHeight;
                const onMove = (ev: MouseEvent) => {
                  const newH = Math.max(150, Math.min(800, startH + (ev.clientY - startY)));
                  setEditorHeight(newH);
                };
                const onUp = () => {
                  document.body.style.cursor = '';
                  document.body.style.userSelect = '';
                  document.removeEventListener('mousemove', onMove);
                  document.removeEventListener('mouseup', onUp);
                  localStorage.setItem('dashboard_editor_h', String(editorHeight));
                };
                document.body.style.cursor = 'row-resize';
                document.body.style.userSelect = 'none';
                document.addEventListener('mousemove', onMove);
                document.addEventListener('mouseup', onUp);
              }}
              className="h-[6px] shrink-0 cursor-row-resize hover:bg-primary/40 transition-colors rounded-full mx-2 relative group"
              title="拖动调整编辑器大小"
            >
              <div className="absolute inset-x-4 top-1/2 -translate-y-1/2 h-[2px] bg-outline-variant/30 group-hover:bg-primary/60 rounded-full transition-colors" />
            </div>

            {/* Results */}
            {/* Results */}
            <div className="glass-panel rounded-lg flex flex-col flex-1 min-h-[120px] overflow-hidden">
              <div className="px-4 h-10 border-b border-outline-variant/30 flex items-center justify-between shrink-0">
                <div className="flex items-center gap-2">
                  <span className={`material-symbols-outlined text-[18px] text-primary ${isQueryRunning ? 'animate-spin' : ''}`}>{isQueryRunning ? 'sync' : 'database'}</span>
                  <h3 className="font-code-md text-code-md text-on-surface font-bold">{t('dashboard.results')}</h3>
                  {queryResult && !isQueryRunning && <span className="font-code-sm text-[10px] text-on-surface-variant">({queryResult.data.length} rows, {queryResult.execution_time_ms}ms)</span>}
                </div>
                <div className="flex items-center gap-3">
                  <div className="relative"><span className="material-symbols-outlined absolute left-2 top-1/2 -translate-y-1/2 text-on-surface-variant text-[14px]">filter_alt</span>
                    <input type="text" placeholder={t('dashboard.filter')} value={filterText} onChange={e => { setFilterText(e.target.value); setCurrentPage(0); }}
                      className="bg-surface-container-highest border border-outline-variant/30 rounded px-7 py-0.5 font-code-sm text-[11px] text-on-surface focus:outline-none focus:border-primary w-40 transition-colors" /></div>
                  <button onClick={handleExport} className="flex items-center gap-1 font-label-caps text-label-caps text-on-surface-variant hover:text-primary transition-colors">
                    <span className="material-symbols-outlined text-[16px]">download</span> {t('dashboard.export')}</button>
                </div>
              </div>
              <div className={`flex-1 overflow-auto transition-opacity duration-300 ${isQueryRunning ? 'opacity-50' : 'opacity-100'}`}>
                {queryError && (
                  <div className="p-4 text-error font-code-sm text-[13px] bg-error/5 border-b border-error/10">
                    <span className="material-symbols-outlined text-[14px] mr-1 align-middle">error</span>{queryError}</div>)}
                {queryResult && queryResult.status === 'success' && queryResult.data.length === 0 && queryResult.columns.length <= 1 ? (
                  <div className="flex items-center justify-center h-full text-secondary font-code-sm flex-col gap-2">
                    <span className="material-symbols-outlined text-[32px]">check_circle</span>
                    <span>{t('dashboard.querySuccess')}</span></div>
                ) : queryResult && queryResult.columns.length > 0 && queryResult.data.length > 0 ? (
                  <table className="w-full text-left font-code-sm text-[11px] border-collapse relative">
                    <thead className="sticky top-0 bg-surface-container/80 backdrop-blur z-10">
                      <tr className="border-b border-outline-variant/20 text-outline">
                        {queryResult.columns.map((col, i) => <th key={i} className="px-4 py-2 font-medium uppercase tracking-wider">{col}</th>)}</tr></thead>
                    <tbody className="divide-y divide-outline-variant/10">
                      {pageData.map((row, ri) => (
                        <tr key={ri} className="hover:bg-white/10 transition-colors text-on-surface cursor-pointer group">
                          {row.map((val, ci) => <td key={ci} className="px-4 py-1.5 group-hover:text-primary transition-colors">{val}</td>)}</tr>))}</tbody>
                  </table>
                ) : !queryError && (
                  <div className="flex items-center justify-center h-full text-on-surface-variant font-code-sm">
                    {queryResult && queryResult.columns.length > 0 ? `Query returned no rows.` : queryResult ? 'Query returned no results.' : t('dashboard.noResults')}</div>
                )}
              </div>
              <div className="h-6 px-4 bg-surface-container/50 border-t border-outline-variant/10 flex items-center justify-between text-[9px] text-outline shrink-0">
                <span>{filterText ? `Filtered ${filteredData.length} of ${queryResult?.data.length ?? 0} rows` : `Showing ${filteredData.length} rows`}{totalPages > 1 && ` — page ${currentPage + 1}/${totalPages}`}</span>
                <span className="flex gap-1">
                  <button className="material-symbols-outlined text-[10px] hover:text-primary disabled:opacity-30" disabled={currentPage === 0} onClick={() => setCurrentPage(p => Math.max(0, p - 1))}>chevron_left</button>
                  <button className="material-symbols-outlined text-[10px] hover:text-primary disabled:opacity-30" disabled={currentPage >= totalPages - 1} onClick={() => setCurrentPage(p => Math.min(totalPages - 1, p + 1))}>chevron_right</button></span>
              </div>
            </div>
          </div>

          {/* AI Chat Panel */}
          <ChatPanel isOpen={showChat} onToggle={() => setShowChat(false)} />
        </div>
      </div>

      {/* Import Preview Modal */}
      {showImport && importData && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm" onClick={() => setShowImport(false)}>
          <div className="bg-surface border border-white/10 rounded-2xl p-6 w-[640px] max-h-[80vh] overflow-y-auto shadow-2xl" onClick={e => e.stopPropagation()}>
            <h3 className="font-headline-sm text-headline-sm text-on-surface font-bold mb-1">{t('import.title')}</h3>
            <p className="font-code-sm text-[11px] text-outline mb-4">{importData.filename}</p>

            {(importData.creates.length === 0 && importData.inserts.length === 0 && importData.others.length === 0) ? (
              <p className="text-on-surface-variant font-code-sm text-center py-8">{t('import.empty')}</p>
            ) : (
              <>
                {/* CREATE TABLE */}
                {importData.creates.length > 0 && (
                  <div className="mb-4">
                    <h4 className="font-code-sm text-code-sm text-secondary font-bold mb-2">{t('import.creates')} ({importData.creates.length})</h4>
                    <div className="space-y-1.5">
                      {importData.creates.map((c, i) => (
                        <div key={i} className="bg-black/20 rounded-lg px-3 py-2 flex items-center gap-3">
                          <span className="material-symbols-outlined text-[16px] text-primary">table_view</span>
                          <span className="font-code-sm text-on-surface">{c.table_name}</span>
                          <span className="text-[10px] text-outline ml-auto">{c.col_count} columns</span>
                        </div>
                      ))}
                    </div>
                  </div>
                )}
                {/* INSERT */}
                {importData.inserts.length > 0 && (
                  <div className="mb-4">
                    <h4 className="font-code-sm text-code-sm text-tertiary font-bold mb-2">{t('import.inserts')} ({importData.inserts.length})</h4>
                    <div className="space-y-1.5">
                      {importData.inserts.map((ins, i) => (
                        <div key={i} className="bg-black/20 rounded-lg px-3 py-2 flex items-center gap-3">
                          <span className="material-symbols-outlined text-[16px] text-tertiary">edit_note</span>
                          <span className="font-code-sm text-on-surface">{ins.table_name}</span>
                          <span className="text-[10px] text-outline ml-auto">{ins.row_count} rows</span>
                        </div>
                      ))}
                    </div>
                  </div>
                )}
                {/* Other / skipped */}
                {importData.others.length > 0 && (
                  <div className="mb-4">
                    <h4 className="font-code-sm text-code-sm text-outline font-bold mb-2">{t('import.others')} ({importData.others.length})</h4>
                    <div className="space-y-1.5 opacity-50">
                      {importData.others.map((o, i) => (
                        <div key={i} className="bg-black/10 rounded-lg px-3 py-2 flex items-center gap-3">
                          <span className="material-symbols-outlined text-[16px] text-outline">block</span>
                          <span className="font-code-sm text-on-surface-variant uppercase text-[10px]">{o.type}</span>
                          <span className="text-[10px] text-outline ml-auto truncate max-w-[200px]">{o.sql.slice(0, 60)}</span>
                        </div>
                      ))}
                    </div>
                  </div>
                )}
              </>
            )}

            <div className="flex gap-2 mt-6">
              <button onClick={() => setShowImport(false)} className="px-4 py-2 rounded-lg font-code-sm text-on-surface-variant hover:bg-white/10 transition-colors">{t('common.cancel')}</button>
              <button disabled={importing || (importData.creates.length === 0 && importData.inserts.length === 0)}
                onClick={async () => {
                  setImporting(true);
                  try {
                    const result = await executeImport(
                      importData!.creates.map(c => c.sql),
                      importData!.inserts.map(ins => ins.sql)
                    );
                    showToast(`Created ${result.created} tables, inserted ${result.inserted} rows`);
                    setShowImport(false);
                    setImportData(null);
                    // refresh schema tree
                    fetchSchemaTree().then(setSchemaTree).catch(() => {});
                  } catch { showToast('Import failed'); }
                  setImporting(false);
                }}
                className="flex-1 bg-primary text-black font-bold px-4 py-2 rounded-lg font-code-sm hover:bg-primary-fixed transition-colors disabled:opacity-50">
                {importing ? <span className="material-symbols-outlined text-[14px] animate-spin align-middle mr-1">sync</span> : null}
                {t('import.confirm')}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Toast */}
      {toastMessage && (
        <div className="absolute bottom-6 left-1/2 -translate-x-1/2 bg-surface text-on-surface border border-white/10 shadow-[0_4px_24px_rgba(0,0,0,0.5)] rounded-full px-6 py-2 font-code-sm text-code-sm flex items-center gap-2 z-[100]">
          <span className="material-symbols-outlined text-[16px] text-primary">info</span>{toastMessage}</div>)}
    </div>
  );
}

// ── VS Code-style tree node components ──────────────────────────────────────

/** Reusable folder node — accepts all Dashboard callbacks explicitly. */
function WorkspaceFolderNode({ folder, allFolders, tables, state, actions }: {
  folder: WorkspaceFolder; allFolders: WorkspaceFolder[]; tables: WorkspaceTable[];
  state: { expandedNodes: Record<string,boolean>; selectedNode: string; editingNode: string|null; editValue: string; dragOverFolder: number|null };
  actions: Record<string, (...args: any[]) => any>;
}) {
  const isExpanded = state.expandedNodes[`folder:${folder.id}`] !== false;
  const isEditing = state.editingNode === `folder:${folder.id}`;
  const isSelected = state.selectedNode === `folder:${folder.id}`;
  const isActive = (state as any).activeFolderId === folder.id;
  const childFolders = allFolders.filter(f => f.parent_id === folder.id);
  const childTables = tables.filter(t => t.folder_id === folder.id);

  return (
    <li>
      <div
        className={`flex items-center justify-between py-0.5 px-1.5 rounded cursor-pointer transition-colors group/folder ${
          state.dragOverFolder === folder.id ? 'bg-primary/20 border border-primary/40' :
          isActive ? 'bg-primary/15 text-primary border border-primary/30' :
          isSelected ? 'bg-white/10 text-on-surface' : 'hover:bg-white/5 text-on-surface-variant'
        }`}
        onClick={() => actions.selectFolder(folder.id, folder.name)}
        onDragOver={e => { e.preventDefault(); actions.setDragOverFolder(folder.id); }}
        onDragLeave={() => actions.setDragOverFolder(null)}
        onDrop={e => { e.preventDefault(); const id = e.dataTransfer.getData('text/plain'); if (id) actions.dropOnFolder(Number(id), folder.id); }}
      >
        <div className="flex items-center gap-1.5 pr-1 min-w-0">
          <span className={`material-symbols-outlined text-[14px] shrink-0 transition-transform ${isExpanded ? 'rotate-90' : ''} hover:text-on-surface`}
            onClick={e => { e.stopPropagation(); actions.toggleNode(`folder:${folder.id}`); }}>chevron_right</span>
          <span className={`material-symbols-outlined text-[14px] shrink-0 ${isExpanded ? 'text-tertiary' : 'text-outline'} icon-fill`}>
            {isExpanded ? 'folder_open' : 'folder'}</span>
          {isEditing ? (
            <input value={state.editValue}
              onChange={e => actions.setEditValue(e.target.value)}
              onBlur={() => actions.renameFolder(folder.id)}
              onKeyDown={e => { if (e.key === 'Enter') actions.renameFolder(folder.id); if (e.key === 'Escape') actions.cancelEdit(); }}
              className="bg-black/40 border border-primary/50 rounded px-1 py-0 text-[11px] text-on-surface w-[120px] focus:outline-none"
              autoFocus onClick={e => e.stopPropagation()} />
          ) : (
            <span className="whitespace-nowrap truncate"
              onDoubleClick={e => { e.stopPropagation(); actions.startRename(`folder:${folder.id}`, folder.name); }}>{folder.name}</span>
          )}
        </div>
        <div className="flex items-center gap-0.5 shrink-0 opacity-0 group-hover/folder:opacity-100 transition-opacity">
          <button onClick={e => { e.stopPropagation(); actions.createTableInFolder(folder.id, folder.name); }}
            className="text-on-surface-variant hover:text-primary p-0.5" title="New Table"><span className="material-symbols-outlined text-[12px]">note_add</span></button>
          <button onClick={e => { e.stopPropagation(); actions.startRename(`folder:${folder.id}`, folder.name); }}
            className="text-on-surface-variant hover:text-primary p-0.5" title="Rename"><span className="material-symbols-outlined text-[12px]">edit</span></button>
          <button onClick={e => { e.stopPropagation(); actions.deleteFolder(folder.id, folder.name); }}
            className="text-on-surface-variant hover:text-error p-0.5" title="Delete"><span className="material-symbols-outlined text-[12px]">delete</span></button>
        </div>
      </div>
      {isExpanded && (childFolders.length > 0 || childTables.length > 0) && (
        <ul className="ml-4 mt-0.5 border-l border-outline-variant/20 pl-2 space-y-0.5">
          {childFolders.map(f => (
            <WorkspaceFolderNode key={`folder:${f.id}`} folder={f} allFolders={allFolders} tables={tables} state={state} actions={actions} />
          ))}
          {childTables.map(t => (
            <WorkspaceTableRow key={`table:${t.id}`} table={t} state={state} actions={actions} />
          ))}
        </ul>
      )}
    </li>
  );
}

/** Reusable table row — leaf node in the tree. */
function WorkspaceTableRow({ table, state, actions }: {
  table: WorkspaceTable;
  state: { expandedNodes: Record<string,boolean>; selectedNode: string; editingNode: string|null; editValue: string };
  actions: Record<string, (...args: any[]) => any>;
}) {
  const isExpanded = state.expandedNodes[`table:${table.id}`] === true;
  const isEditing = state.editingNode === `table:${table.id}`;
  const isSelected = state.selectedNode === `table:${table.id}`;
  const cols = table.columns || [];
  return (
    <li className="group/table" draggable onDragStart={e => e.dataTransfer.setData('text/plain', String(table.id))}>
      <div className={`flex items-center justify-between py-0.5 px-1.5 rounded cursor-pointer transition-colors ${isSelected ? 'bg-white/10 text-on-surface' : 'hover:bg-white/5 text-on-surface-variant'}`}
        onClick={e => actions.selectTable(table.table_name, table.id, e)}>
        <div className="flex items-center gap-1.5 pr-1 min-w-0">
          <span className={`material-symbols-outlined text-[12px] shrink-0 transition-transform ${isExpanded ? 'rotate-90' : ''} hover:text-on-surface`}
            onClick={e => { e.stopPropagation(); actions.toggleNode(`table:${table.id}`); }}>chevron_right</span>
          <span className="material-symbols-outlined text-[14px] text-secondary shrink-0">table_view</span>
          {isEditing ? (
            <input value={state.editValue}
              onChange={e => actions.setEditValue(e.target.value)}
              onBlur={() => actions.renameTable(table.id)}
              onKeyDown={e => { if (e.key === 'Enter') actions.renameTable(table.id); if (e.key === 'Escape') actions.cancelEdit(); }}
              className="bg-black/40 border border-primary/50 rounded px-1 py-0 text-[11px] text-on-surface w-[120px] focus:outline-none"
              autoFocus onClick={e => e.stopPropagation()} />
          ) : (
            <span className="whitespace-nowrap truncate"
              onDoubleClick={e => { e.stopPropagation(); actions.startRename(`table:${table.id}`, table.display_name); }}>
              {table.display_name}
              {(state as any).activeFolderId && table.table_name !== table.display_name && (
                <span className="text-[9px] text-outline/70 ml-1">({table.table_name})</span>
              )}
            </span>
          )}
        </div>
        <div className="flex items-center gap-0.5 shrink-0 opacity-0 group-hover/table:opacity-100 transition-opacity">
          <button onClick={e => { e.stopPropagation(); actions.showSchema(table.table_name); }} title="DESC"
            className="text-on-surface-variant hover:text-primary p-0.5"><span className="material-symbols-outlined text-[12px]">info</span></button>
          <button onClick={e => { e.stopPropagation(); actions.startRename(`table:${table.id}`, table.display_name); }} title="Rename"
            className="text-on-surface-variant hover:text-primary p-0.5"><span className="material-symbols-outlined text-[12px]">edit</span></button>
          <button onClick={e => { e.stopPropagation(); actions.deleteTable(table.id, table.display_name); }} title="Delete"
            className="text-on-surface-variant hover:text-error p-0.5"><span className="material-symbols-outlined text-[12px]">delete</span></button>
        </div>
      </div>
      {isExpanded && cols.length > 0 && (
        <ul className="ml-5 mt-0.5 border-l border-outline-variant/20 pl-3 space-y-0 text-[10px] text-outline">
          {cols.map((col: any) => (
            <li key={col.name} className="flex items-center gap-1.5 py-0.5">
              <span className="w-1 h-1 rounded-full bg-outline-variant/50 shrink-0"></span>
              <span className="text-on-surface-variant">{col.name}</span>
              <span className="text-outline/60">{col.type}</span></li>))}</ul>
      )}
    </li>
  );
}
