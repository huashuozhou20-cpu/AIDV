import { useState, useRef, useEffect } from 'react';
import { useT } from '../i18n';
import { fetchAIDesign, confirmAIDesign, getScenario, modifyTableStructure } from '../api';
import TemplateLibrary from '../components/TemplateLibrary';
import type { Template } from '../components/TemplateLibrary';
import TableDesigner from '../components/TableDesigner';
import type { TableDef } from '../components/TableDesigner';

type TabId = 'ai' | 'templates' | 'manual';

interface Message {
  role: 'user' | 'ai';
  content: string;
  sqls?: string[];
  confirmed?: boolean;
}

function tablesToSQL(tables: TableDef[]): string[] {
  return tables.map(t => {
    const cols = t.columns.map(c => {
      let def = `    ${c.name} ${c.type}`;
      // RMDB constraint order: PRIMARY KEY must come BEFORE AUTO_INCREMENT
      // PK implies NOT NULL, so skip NOT NULL when PK is set
      if (c.pk) {
        def += ' PRIMARY KEY';
      } else if (c.not_null) {
        def += ' NOT NULL';
      }
      if (c.auto_increment) def += ' AUTO_INCREMENT';
      return def;
    }).join(',\n');
    return `CREATE TABLE ${t.name} (\n${cols}\n);`;
  });
}

export default function ScenarioDesignerView({ scenarioId, scenarioName, onDone, editTable }: {
  scenarioId: number;
  scenarioName: string;
  onDone: () => void;
  editTable?: { tableId: number; tableName: string; displayName: string } | null;
}) {
  const { t } = useT();

  const isEditMode = !!editTable;

  // ── Tab state ──
  const [activeTab, setActiveTab] = useState<TabId>(isEditMode ? 'manual' : 'ai');
  const [designedTables, setDesignedTables] = useState<TableDef[]>([]);
  const [creatingAll, setCreatingAll] = useState(false);

  // ── Load existing table schema in edit mode ──
  useEffect(() => {
    if (!editTable) return;
    (async () => {
      try {
        const detail = await getScenario(scenarioId);
        if (detail?.tables) {
          const tbl = detail.tables.find(t => t.id === editTable.tableId);
          if (tbl && tbl.columns) {
            const colDefs = tbl.columns.map((c: { name: string; type: string }) => ({
              name: c.name,
              type: (c.type || 'CHAR(100)').toUpperCase().replace(/^STRING$/, 'CHAR(100)'),
              pk: false,
              not_null: false,
              auto_increment: false,
            }));
            setDesignedTables([{
              name: editTable.tableName,
              display: editTable.displayName,
              columns: colDefs,
            }]);
          }
        }
      } catch {}
    })();
  }, [editTable?.tableId]);

  // ── AI chat state ──
  const [messages, setMessages] = useState<Message[]>([{
    role: 'ai',
    content: `你好！请描述你希望「${scenarioName}」管理哪些数据？\n\n例如：\n- "我需要管理书籍信息、读者信息、借阅和归还记录"\n- "我需要管理客户、订单、产品和库存"`,
  }]);
  const [input, setInput] = useState('');
  const [thinking, setThinking] = useState(false);
  const [confirming, setConfirming] = useState(false);
  const [editingSQL, setEditingSQL] = useState<{ msgIdx: number; sqlIdx: number; text: string } | null>(null);
  const [showPreview, setShowPreview] = useState(false);
  const chatEnd = useRef<HTMLDivElement>(null);

  useEffect(() => { chatEnd.current?.scrollIntoView({ behavior: 'smooth' }); }, [messages]);

  // ── AI chat handlers ──

  const handleSend = async () => {
    if (!input.trim() || thinking) return;
    const userMsg = input.trim();
    setInput('');
    setMessages(prev => [...prev, { role: 'user', content: userMsg }]);
    setThinking(true);

    try {
      const history = messages
        .filter(m => m.role === 'user' || m.role === 'ai')
        .slice(-6)
        .map(m => ({ role: m.role === 'ai' ? 'assistant' : 'user', content: m.content }));
      history.push({ role: 'user', content: userMsg });

      const res = await fetchAIDesign(scenarioId, userMsg, history);
      if (res.status === 'success' && res.response) {
        const sqls = extractSQLs(res.response);
        setMessages(prev => [...prev, { role: 'ai', content: res.response!, sqls }]);
      } else {
        setMessages(prev => [...prev, { role: 'ai', content: `❌ ${res.message || 'AI 调用失败，请重试'}` }]);
      }
    } catch {
      setMessages(prev => [...prev, { role: 'ai', content: '❌ 网络错误，请重试' }]);
    }
    setThinking(false);
  };

  const handleConfirmTable = async (sql: string, idx: number) => {
    setConfirming(true);
    try {
      const res = await confirmAIDesign(scenarioId, [sql]);
      setMessages(prev => prev.map((msg, mi) => {
        if (mi === prev.length - 1 && msg.sqls) {
          const newSqls = [...msg.sqls];
          newSqls[idx] = '✅ ' + newSqls[idx];
          return { ...msg, sqls: newSqls, confirmed: msg.sqls.every((s, i) => i === idx || s.startsWith('✅')) || msg.confirmed };
        }
        return msg;
      }));
      if (res.errors?.length) {
        setMessages(prev => [...prev, { role: 'ai', content: `⚠️ 部分表创建失败: ${res.errors.map(e => e.error).join(', ')}` }]);
      }
    } catch {}
    setConfirming(false);
  };

  const handleConfirmAll = async () => {
    const lastMsg = messages[messages.length - 1];
    if (!lastMsg?.sqls) return;
    const unconfirmed = lastMsg.sqls.filter(s => !s.startsWith('✅'));
    if (!unconfirmed.length) return;
    setConfirming(true);
    try {
      const sqls = unconfirmed.map(s => s.replace(/^编辑中:\s*/, '').trim());
      const res = await confirmAIDesign(scenarioId, sqls);
      if (res.created?.length) {
        setMessages(prev => prev.map(msg => {
          if (msg.sqls) return { ...msg, sqls: msg.sqls.map(s => '✅ ' + s), confirmed: true };
          return msg;
        }));
        setMessages(prev => [...prev, { role: 'ai', content: `🎉 成功创建 ${res.created.length} 张表！点击下方按钮进入数据管理。` }]);
      }
      if (res.errors?.length) {
        setMessages(prev => [...prev, { role: 'ai', content: `⚠️ ${res.errors.map(e => e.error).join('; ')}` }]);
      }
    } catch {}
    setConfirming(false);
  };

  // ── Template → Manual handoff ──

  const handleTemplateSelect = (tmpl: Template) => {
    setDesignedTables(tmpl.tables.map(t => ({
      name: t.name,
      display: t.display,
      columns: t.columns.map(c => ({ ...c })),
    })));
    setActiveTab('manual');
  };

  // ── Manual designer → create all ──

  const handleCreateAll = async (tables: TableDef[]) => {
    setCreatingAll(true);
    try {
      const sqls = tablesToSQL(tables);
      const res = await confirmAIDesign(scenarioId, sqls);
      if (res.created?.length) {
        alert(`🎉 成功创建 ${res.created.length} 张表！`);
      }
      if (res.errors?.length) {
        alert(`⚠️ ${res.errors.map(e => e.error).join('; ')}`);
      }
    } catch {}
    setCreatingAll(false);
  };

  // ── Edit mode: save modified table structure ──

  const handleModifyTable = async (table: TableDef) => {
    if (!editTable) return;
    setCreatingAll(true);
    try {
      const res = await modifyTableStructure(
        scenarioId,
        editTable.tableId,
        table.columns.map(c => ({
          name: c.name,
          type: c.type,
          pk: c.pk,
          not_null: c.not_null,
          auto_increment: c.auto_increment,
        }))
      );
      if (res.status === 'success') {
        alert(`✅ ${res.message || '表结构已更新'}`);
        onDone();
      } else {
        alert(`⚠️ ${res.message || '修改失败'}`);
      }
    } catch {
      alert('⚠️ 网络错误，请重试');
    }
    setCreatingAll(false);
  };

  // ── Tab config ──

  const TABS: { id: TabId; icon: string; label: string }[] = [
    { id: 'ai', icon: 'psychology', label: 'AI Design' },
    { id: 'templates', icon: 'dashboard', label: t('template.title') },
    { id: 'manual', icon: 'table_edit', label: t('designer.title') },
  ];

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      {/* Header */}
      <div className="flex items-center justify-between mb-4 shrink-0">
        <div>
          <h1 className="font-headline-lg text-headline-lg text-on-surface font-bold flex items-center gap-2">
            <span className="material-symbols-outlined text-[28px] text-primary">{isEditMode ? 'edit_square' : 'psychology'}</span>
            {isEditMode ? `${t('data.editStructure')}: ${editTable?.displayName || ''}` : t('designer.title')}
          </h1>
          <p className="font-code-sm text-on-surface-variant mt-1">{scenarioName}</p>
        </div>
        <div className="flex gap-2">
          {!isEditMode && (
            <button onClick={() => { setMessages([messages[0]]); setDesignedTables([]); }}
              className="px-3 py-1.5 rounded-lg font-code-sm text-[11px] text-on-surface-variant hover:bg-white/10 transition-colors flex items-center gap-1">
              <span className="material-symbols-outlined text-[14px]">refresh</span>{t('designer.redesign')}</button>
          )}
          <button onClick={onDone}
            className="bg-primary/20 text-primary border border-primary/30 px-4 py-1.5 rounded-lg font-code-sm text-[11px] hover:bg-primary/30 transition-colors flex items-center gap-1">
            <span className="material-symbols-outlined text-[14px]">table_view</span>{isEditMode ? t('common.cancel') : t('designer.enterData')}</button>
        </div>
      </div>

      {/* Tab bar (hidden in edit mode) */}
      {!isEditMode && (
      <div className="flex gap-0 mb-4 shrink-0">
        {TABS.map(tab => (
          <button key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className={`px-5 py-2 font-code-sm text-[12px] flex items-center gap-2 rounded-t-lg transition-colors border-b-2 ${
              activeTab === tab.id
                ? 'bg-black/20 border-primary text-primary'
                : 'border-transparent text-on-surface-variant hover:text-on-surface hover:bg-white/5'
            }`}>
            <span className={`material-symbols-outlined text-[16px] ${activeTab === tab.id ? 'text-primary' : ''}`}>{tab.icon}</span>
            {tab.label}
          </button>
        ))}
        <div className="flex-1 border-b-2 border-transparent" />
      </div>
      )}

      {/* Tab content */}
      <div className="flex-1 flex flex-col min-h-0">
        {/* ── AI Design tab ── */}
        {activeTab === 'ai' && (
          <>
            {/* Chat Area */}
            <div className="flex-1 overflow-y-auto space-y-4 mb-4 min-h-0">
              {messages.map((msg, i) => (
                <div key={i} className={`flex gap-3 ${msg.role === 'user' ? 'justify-end' : ''}`}>
                  {msg.role === 'ai' && (
                    <span className="material-symbols-outlined text-[24px] text-primary shrink-0 mt-1">psychology</span>
                  )}
                  <div className={`max-w-[80%] ${msg.role === 'user' ? 'bg-primary/20 text-on-surface border border-primary/30 rounded-2xl rounded-tr-sm' : 'glass-panel border border-white/10 rounded-2xl rounded-tl-sm'} px-4 py-3`}>
                    <div className="font-code-sm text-[12px] leading-relaxed whitespace-pre-wrap">{msg.content.split('```sql').map((part, pi) => {
                      if (pi === 0) return part;
                      const [sql, ...rest] = part.split('```');
                      const displaySQL = msg.confirmed || msg.sqls?.every(s => s.startsWith('✅')) ? null : sql.trim();
                      return displaySQL ? (
                        <span key={pi}>
                          <pre className="bg-black/30 rounded p-2 my-2 text-[11px] text-[#b5cea8] overflow-x-auto">{sql.trim()}</pre>
                          {rest.join('```')}
                        </span>
                      ) : (
                        <span key={pi}>{rest.join('```')}</span>
                      );
                    })}</div>

                    {/* Action buttons for each SQL */}
                    {msg.sqls && !msg.confirmed && (
                      <div className="mt-3 space-y-2">
                        {msg.sqls.filter(s => !s.startsWith('✅')).map((sql, si) => {
                          const isEditing = editingSQL?.msgIdx === i && editingSQL?.sqlIdx === si;
                          return (
                          <div key={si} className="flex items-start gap-2 bg-black/20 rounded-lg p-2">
                            {isEditing ? (
                              <textarea value={editingSQL!.text}
                                onChange={e => setEditingSQL({ ...editingSQL!, text: e.target.value })}
                                className="flex-1 text-[10px] text-[#b5cea8] bg-black/40 border border-primary/50 rounded p-1 font-mono resize-none min-h-[60px] focus:outline-none" />
                            ) : (
                              <pre className="flex-1 text-[10px] text-[#b5cea8] whitespace-pre-wrap font-mono overflow-x-auto">{sql.replace(/^编辑中:\s*/, '')}</pre>
                            )}
                            <div className="flex flex-col gap-1 shrink-0">
                              {isEditing ? (
                                <button onClick={() => {
                                  const newSqls = [...(msg.sqls || [])];
                                  newSqls[si] = editingSQL!.text;
                                  setMessages(prev => prev.map((m, mi) => mi === i ? { ...m, sqls: newSqls } : m));
                                  setEditingSQL(null);
                                }} className="bg-primary/20 text-primary border border-primary/30 rounded px-2 py-0.5 text-[10px] font-code-sm hover:bg-primary/30">{t('common.save')}</button>
                              ) : (
                                <button onClick={() => setEditingSQL({ msgIdx: i, sqlIdx: si, text: sql })}
                                  className="bg-surface-variant/50 text-on-surface-variant border border-outline-variant/30 rounded px-2 py-0.5 text-[10px] font-code-sm hover:bg-white/20">✏️</button>
                              )}
                              <button onClick={() => handleConfirmTable(sql, si)} disabled={confirming}
                                className="shrink-0 bg-secondary/20 text-secondary border border-secondary/30 rounded px-2 py-0.5 text-[10px] font-code-sm hover:bg-secondary/30 transition-colors">
                                {t('common.confirm')}</button>
                            </div>
                          </div>
                        )})}
                        {msg.sqls.filter(s => !s.startsWith('✅')).length > 1 && (
                          <button onClick={() => setShowPreview(true)} disabled={confirming}
                            className="w-full bg-primary text-black font-bold py-1.5 rounded-lg text-[10px] font-code-sm hover:bg-primary-fixed transition-colors">
                            {confirming ? t('designer.confirming') : t('designer.confirmAll')}</button>
                        )}
                      </div>
                    )}
                  </div>
                  {msg.role === 'user' && (
                    <span className="material-symbols-outlined text-[20px] text-on-surface-variant shrink-0 mt-1">person</span>
                  )}
                </div>
              ))}
              {thinking && (
                <div className="flex items-center gap-2 text-on-surface-variant font-code-sm text-[11px]">
                  <span className="material-symbols-outlined text-[16px] text-primary animate-spin">sync</span>{t('designer.thinking')}
                </div>
              )}
              <div ref={chatEnd} />
            </div>

            {/* Preview Modal */}
            {showPreview && (() => {
              const lastMsg = messages.filter(m => m.sqls && !m.confirmed).pop();
              const sqls = lastMsg?.sqls?.filter((s: string) => !s.startsWith('✅')) || [];
              const estimateSize = (sql: string) => {
                let bytes = 0;
                const charMatches = sql.matchAll(/CHAR\((\d+)\)/gi);
                for (const m of charMatches) bytes += parseInt(m[1]) * 4;
                const ints = (sql.match(/INT/gi) || []).length;
                const floats = (sql.match(/FLOAT/gi) || []).length;
                bytes += (ints + floats) * 4;
                return bytes;
              };
              return (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm" onClick={() => setShowPreview(false)}>
                  <div className="bg-surface border border-white/10 rounded-2xl p-6 w-[600px] max-h-[80vh] overflow-y-auto shadow-2xl" onClick={e => e.stopPropagation()}>
                    <h3 className="font-headline-sm text-headline-sm text-on-surface font-bold mb-3">预览建表语句</h3>
                    {sqls.map((sql: string, i: number) => {
                      const size = estimateSize(sql);
                      const over = size > 2000;
                      return (
                        <div key={i} className={`mb-3 p-3 rounded-lg ${over ? 'bg-error/10 border border-error/30' : 'bg-black/20'}`}>
                          <pre className="text-[10px] text-[#b5cea8] whitespace-pre-wrap font-mono mb-2">{sql}</pre>
                          <span className={`text-[10px] font-code-sm ${over ? 'text-error font-bold' : 'text-outline'}`}>
                            预估 {size} 字节 {over ? '⚠️ 超限！' : '✓'}
                          </span>
                        </div>
                      );
                    })}
                    <div className="flex gap-2 mt-4">
                      <button onClick={() => setShowPreview(false)} className="px-4 py-2 rounded-lg font-code-sm text-on-surface-variant hover:bg-white/10">{t('common.cancel')}</button>
                      <button onClick={() => { setShowPreview(false); handleConfirmAll(); }}
                        className="flex-1 bg-primary text-black font-bold px-4 py-2 rounded-lg font-code-sm hover:bg-primary-fixed">
                        {t('common.confirm')}</button>
                    </div>
                  </div>
                </div>
              );
            })()}

            {/* Input */}
            <div className="shrink-0 flex gap-2">
              <input type="text" value={input}
                onChange={e => setInput(e.target.value)}
                onKeyDown={e => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); handleSend(); } }}
                placeholder={t('designer.placeholder')}
                className="flex-1 bg-black/40 border border-outline-variant/30 rounded-xl px-4 py-2.5 font-code-sm text-on-surface focus:outline-none focus:border-primary" />
              <button onClick={handleSend} disabled={thinking || !input.trim()}
                className="bg-primary text-black font-bold px-4 py-2.5 rounded-xl hover:bg-primary-fixed transition-colors disabled:opacity-50 font-code-sm flex items-center gap-1">
                {thinking ? <span className="material-symbols-outlined text-[16px] animate-spin">sync</span> : <span className="material-symbols-outlined text-[16px]">send</span>}
              </button>
            </div>
          </>
        )}

        {/* ── Templates tab ── */}
        {activeTab === 'templates' && (
          <TemplateLibrary onSelect={handleTemplateSelect} />
        )}

        {/* ── Manual tab ── */}
        {(activeTab === 'manual' || isEditMode) && (
          <TableDesigner
            initialTables={designedTables}
            onChange={setDesignedTables}
            onCreate={handleCreateAll}
            editMode={isEditMode}
            onModify={isEditMode ? handleModifyTable : undefined}
          />
        )}
      </div>

      {/* Creating overlay */}
      {creatingAll && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
          <div className="bg-surface border border-white/10 rounded-2xl p-8 flex flex-col items-center gap-3">
            <span className="material-symbols-outlined text-[40px] text-primary animate-spin">sync</span>
            <p className="font-code-sm text-on-surface">{t('designer.confirming')}</p>
          </div>
        </div>
      )}
    </div>
  );
}

function extractSQLs(text: string): string[] {
  const sqls: string[] = [];
  const regex = /```sql\s*([\s\S]*?)```/g;
  let match;
  while ((match = regex.exec(text)) !== null) {
    const sql = match[1].trim();
    if (sql.toUpperCase().startsWith('CREATE TABLE')) {
      sqls.push(sql);
    }
  }
  return sqls;
}
