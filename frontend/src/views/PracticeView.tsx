import { useState, useEffect, useRef, useCallback } from 'react';
import { useT } from '../i18n';

type Mode = 'sql' | 'choice';

// Simple markdown renderer for tables and headers in questions
function renderQuestion(text: string) {
  if (!text) return null;
  const lines = text.split('\n');
  const elements: JSX.Element[] = [];
  let tableRows: string[][] = [];
  let inTable = false;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const trimmed = line.trim();

    // Header
    if (trimmed.startsWith('### ')) {
      if (inTable) { elements.push(renderTable(tableRows)); tableRows = []; inTable = false; }
      elements.push(<h4 key={i} className="font-headline-sm text-headline-sm text-on-surface font-bold mt-3 mb-1">{trimmed.slice(4)}</h4>);
      continue;
    }
    if (trimmed.startsWith('## ')) {
      if (inTable) { elements.push(renderTable(tableRows)); tableRows = []; inTable = false; }
      elements.push(<h3 key={i} className="font-headline-md text-headline-md text-on-surface font-bold mt-3 mb-1">{trimmed.slice(3)}</h3>);
      continue;
    }

    // Table row
    if (trimmed.startsWith('|') && trimmed.endsWith('|')) {
      // Skip separator rows like |----|----|
      if (/^\|[\s\-:]+\|$/.test(trimmed.replace(/\|/g, '|').replace(/[|\-:\s]/g, '')) || trimmed.replace(/[|\-:\s]/g, '').length === 0) {
        continue;
      }
      const cells = trimmed.split('|').filter(c => c.trim() !== '').map(c => c.trim());
      tableRows.push(cells);
      inTable = true;
      continue;
    }

    // Non-table line ends current table
    if (inTable) {
      elements.push(renderTable(tableRows));
      tableRows = [];
      inTable = false;
    }

    // Regular text
    if (trimmed) {
      elements.push(<p key={i} className="mb-1">{trimmed}</p>);
    } else {
      elements.push(<br key={i} />);
    }
  }

  // Flush any remaining table
  if (inTable) {
    elements.push(renderTable(tableRows));
  }

  return <>{elements}</>;
}

function renderTable(rows: string[][]) {
  if (rows.length === 0) return null;
  const header = rows[0];
  const data = rows.slice(1);
  return (
    <div key={Math.random()} className="overflow-x-auto my-2">
      <table className="w-auto text-[11px] font-code-sm border-collapse">
        <thead>
          <tr className="border-b border-outline-variant/40">
            {header.map((h, i) => (
              <th key={i} className="px-2 py-1 text-left text-on-surface-variant font-bold whitespace-nowrap">{h}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {data.map((row, ri) => (
            <tr key={ri} className="border-b border-outline-variant/10 hover:bg-white/5">
              {row.map((cell, ci) => (
                <td key={ci} className="px-2 py-0.5 text-on-surface whitespace-nowrap">{cell}</td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export default function PracticeView({ token }: { token: string }) {
  const { t } = useT();
  const [mode, setMode] = useState<Mode>('sql');
  const [stats, setStats] = useState({ total: 0, correct: 0, accuracy: 0 });
  const [leaderboard, setLeaderboard] = useState<any[]>([]);
  const [difficulty, setDifficulty] = useState<'easy' | 'medium' | 'hard'>('easy');
  const [question, setQuestion] = useState('');
  const [answer, setAnswer] = useState('');
  const [options, setOptions] = useState<Record<string, string>>({});
  const [schema, setSchema] = useState('');
  const [userAnswer, setUserAnswer] = useState('');
  const [generating, setGenerating] = useState(false);
  const [result, setResult] = useState<{ result?: string; message?: string; user_output?: any[] } | null>(null);
  const [validating, setValidating] = useState(false);

  // ── Draggable split state ──
  const containerRef = useRef<HTMLDivElement>(null);
  const dragging = useRef(false);
  const [splitPct, setSplitPct] = useState(() => {
    const saved = localStorage.getItem('practice_split');
    return saved ? Number(saved) : 40; // default 40% question, 60% editor
  });

  const onDragStart = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    dragging.current = true;
    document.body.style.cursor = 'row-resize';
    document.body.style.userSelect = 'none';

    const onMove = (ev: MouseEvent) => {
      if (!dragging.current || !containerRef.current) return;
      const rect = containerRef.current.getBoundingClientRect();
      const y = ev.clientY - rect.top;
      const pct = Math.max(15, Math.min(70, (y / rect.height) * 100));
      setSplitPct(Math.round(pct));
    };

    const onUp = () => {
      dragging.current = false;
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', onUp);
      localStorage.setItem('practice_split', String(splitPct));
    };

    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', onUp);
  }, [splitPct]);

  const diffColor: Record<string, string> = { easy: 'bg-secondary/20 text-secondary border-secondary/30', medium: 'bg-tertiary/20 text-tertiary border-tertiary/30', hard: 'bg-error/20 text-error border-error/30' };

  const generateQuestion = async () => {
    setGenerating(true); setResult(null); setUserAnswer(''); setOptions({}); setSchema('');
    const endpoint = mode === 'choice' ? '/api/practice/generate-choice' : '/api/practice/generate';
    try {
      const res = await fetch(endpoint, {
        method: 'POST', headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
        body: JSON.stringify({ difficulty }),
      });
      const data = await res.json();
      if (data.status === 'success') {
        setQuestion(data.question);
        setAnswer(data.answer);
        if (data.options) setOptions(data.options);
        if (data.schema) setSchema(data.schema);
      }
    } catch { }
    setGenerating(false);
  };

  const fetchStats = async () => {
    try {
      const res = await fetch('/api/practice/stats', { headers: { Authorization: `Bearer ${token}` } });
      const d = await res.json();
      if (d.status === 'success') setStats(d.stats);
    } catch {}
  };

  // Fetch stats on mount
  useEffect(() => { fetchStats(); fetch('/api/practice/leaderboard').then(r => r.json()).then(d => { if (d.status === 'success') setLeaderboard(d.leaderboard); }).catch(() => {}); }, []);

  const submitAnswer = async (choice?: string) => {
    const ans = choice || userAnswer.trim();
    if (!ans) return;
    setValidating(true);
    const body = mode === 'choice'
      ? { question, correct_answer: answer, user_answer: ans }
      : { question, correct_answer: answer, user_answer: ans };
    try {
      const res = await fetch('/api/practice/validate', {
        method: 'POST', headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
        body: JSON.stringify(body),
      });
      setResult(await res.json());
      fetchStats();
    } catch { }
    setValidating(false);
  };

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      <div className="flex items-center gap-4 mb-4">
        <h1 className="font-headline-lg text-headline-lg text-on-surface font-bold flex items-center gap-2">
          <span className="material-symbols-outlined text-[28px] text-tertiary">school</span>{t('practice.title')}
        </h1>
        <div className="flex bg-surface-container rounded-lg p-0.5">
          <button onClick={() => { setMode('sql'); setQuestion(''); setResult(null); }}
            className={`px-3 py-1 rounded font-code-sm text-[11px] transition-colors ${mode === 'sql' ? 'bg-primary/20 text-primary' : 'text-on-surface-variant hover:text-on-surface'}`}>
            {t('practice.mode.sql')}</button>
          <button onClick={() => { setMode('choice'); setQuestion(''); setResult(null); }}
            className={`px-3 py-1 rounded font-code-sm text-[11px] transition-colors ${mode === 'choice' ? 'bg-primary/20 text-primary' : 'text-on-surface-variant hover:text-on-surface'}`}>
            {t('practice.mode.choice')}</button>
        </div>
      </div>

      {stats.total > 0 && (
        <div className="flex gap-4 mb-4 text-[11px] font-code-sm">
          <div className="glass-panel rounded-lg px-3 py-1.5 flex items-center gap-2">
            <span className="text-on-surface-variant">{t('practice.answered')}</span><span className="text-on-surface font-bold">{stats.total}</span></div>
          <div className="glass-panel rounded-lg px-3 py-1.5 flex items-center gap-2">
            <span className="text-on-surface-variant">{t('practice.correct')}</span><span className="text-secondary font-bold">{stats.correct}</span></div>
          <div className="glass-panel rounded-lg px-3 py-1.5 flex items-center gap-2">
            <span className="text-on-surface-variant">{t('practice.accuracy')}</span><span className="text-primary font-bold">{stats.accuracy}%</span></div>
        </div>
      )}

      <div className="flex gap-4 mb-4">
        {(['easy', 'medium', 'hard'] as const).map(d => (
          <button key={d} onClick={() => setDifficulty(d)}
            className={`px-4 py-1.5 rounded-full font-code-sm text-[11px] border transition-all ${difficulty === d ? diffColor[d] : 'border-outline-variant/30 text-on-surface-variant hover:border-white/20'}`}>
            {t('practice.' + d)}</button>))}
        <button onClick={generateQuestion} disabled={generating}
          className="ml-auto bg-primary text-black font-bold px-4 py-1.5 rounded-lg hover:bg-primary-fixed transition-colors disabled:opacity-50 font-code-sm flex items-center gap-1">
          {generating ? <span className="material-symbols-outlined text-[16px] animate-spin">sync</span> : <span className="material-symbols-outlined text-[16px]">auto_awesome</span>}
          {generating ? t('practice.generating') : t('practice.generate')}</button>
      </div>

      {question ? (
        <div className="flex-1 flex gap-4 min-h-0">
          {/* Left: question + editor with draggable split */}
          <div ref={containerRef} className="flex-1 flex flex-col min-h-0 min-w-0 gap-0">
            {/* ── Question panel ── */}
            <div className="glass-panel border border-white/10 rounded-xl overflow-hidden flex flex-col min-h-[120px]" style={{ height: `${splitPct}%` }}>
              <div className="px-5 pt-4 pb-2 shrink-0 flex items-center gap-2">
                <span className={`px-2 py-0.5 rounded-full text-[10px] font-code-sm border ${diffColor[difficulty]}`}>{t('practice.' + difficulty)}</span>
                <span className={`px-2 py-0.5 rounded-full text-[10px] font-code-sm border ${mode === 'choice' ? 'bg-primary/10 text-primary border-primary/30' : 'bg-secondary/10 text-secondary border-secondary/30'}`}>
                  {mode === 'choice' ? t('practice.mode.choice') : t('practice.mode.sql')}</span>
              </div>
              <div className="flex-1 overflow-y-auto px-5 pb-4 font-code-md text-code-md text-on-surface leading-relaxed whitespace-pre-wrap">
                {renderQuestion(question)}
              </div>
              {schema && (
                <details className="px-5 pb-3 shrink-0">
                  <summary className="text-[10px] text-outline cursor-pointer hover:text-on-surface-variant font-code-sm select-none">📋 查看可用表结构</summary>
                  <pre className="mt-2 text-[10px] text-on-surface-variant bg-black/20 rounded p-2 overflow-x-auto font-mono leading-relaxed max-h-[140px]">{schema}</pre>
                </details>
              )}
            </div>

            {/* ── Drag handle ── */}
            <div
              onMouseDown={onDragStart}
              className="h-[6px] shrink-0 cursor-row-resize hover:bg-primary/40 transition-colors rounded-full mx-2 my-0.5 relative group"
              title="拖动调整题目和编辑区大小"
            >
              <div className="absolute inset-x-0 top-1/2 -translate-y-1/2 h-[2px] bg-outline-variant/30 group-hover:bg-primary/60 rounded-full transition-colors" />
            </div>

            {/* ── Editor + result panel ── */}
            <div className="flex-1 flex gap-3 min-h-[150px]">
              {mode === 'choice' ? (
                <div className="flex-1 space-y-2">
                  {['A', 'B', 'C', 'D'].map(letter => options[letter] && (
                    <button key={letter} onClick={() => !result && submitAnswer(letter)}
                      disabled={!!result}
                      className={`w-full text-left p-4 rounded-xl border font-code-md transition-colors ${
                        result
                          ? letter === answer ? 'bg-secondary/20 border-secondary text-secondary' : 'bg-surface-container border-outline-variant/30 text-on-surface-variant'
                          : 'bg-surface-container border-outline-variant/30 text-on-surface hover:border-primary/50 hover:bg-primary/5'
                      }`}>
                      <span className="font-bold mr-3 text-primary">{letter}</span>{options[letter]}
                    </button>))}
                </div>
              ) : (
                <>
                  {/* SQL Editor */}
                  <div className="flex-1 flex flex-col min-h-0 min-w-0">
                    <textarea value={userAnswer} onChange={e => setUserAnswer(e.target.value)}
                      onKeyDown={e => { if (e.key === 'Enter' && (e.metaKey || e.ctrlKey)) submitAnswer(); }}
                      className="flex-1 bg-black/40 border border-outline-variant/30 rounded-xl p-4 font-mono text-[13px] text-[#b5cea8] focus:outline-none focus:border-primary resize-none"
                      placeholder={t('practice.placeholder')} />
                    <button onClick={() => submitAnswer()} disabled={validating || !userAnswer.trim()}
                      className="mt-2 bg-secondary/20 text-secondary border border-secondary/30 font-bold px-4 py-2 rounded-lg hover:bg-secondary/30 transition-colors disabled:opacity-30 font-code-sm self-start">
                      {validating ? t('practice.checking') : t('practice.submit')}</button>
                  </div>

                  {/* Result panel — right side of editor */}
                  {result && (
                    <div className={`w-[280px] shrink-0 glass-panel border rounded-xl p-4 self-stretch overflow-y-auto ${
                      result.result === 'correct' || result.status === 'success'
                        ? 'border-secondary/30 bg-secondary/5'
                        : 'border-error/30 bg-error/5'
                    }`}>
                      <div className="flex items-center gap-2 mb-2">
                        <span className={`material-symbols-outlined text-[20px] ${result.result === 'correct' ? 'text-secondary' : 'text-error'}`}>
                          {result.result === 'correct' ? 'check_circle' : 'cancel'}
                        </span>
                        <span className={`font-code-md text-code-md font-bold ${result.result === 'correct' ? 'text-secondary' : 'text-error'}`}>
                          {result.result === 'correct' ? t('practice.correctMsg') : t('practice.wrongMsg')}</span>
                      </div>
                      <p className="font-code-sm text-[11px] text-on-surface-variant leading-relaxed">{result.message}</p>
                      {result.user_output && result.user_output.length > 0 && (
                        <div className="mt-3 pt-3 border-t border-outline-variant/20">
                          <p className="text-[10px] text-outline mb-1 font-code-sm">查询结果</p>
                          <div className="overflow-x-auto max-h-[200px]">
                            <table className="w-auto text-[10px] font-code-sm border-collapse">
                              {result.user_output.map((row: any[], ri: number) => (
                                <tr key={ri} className={ri === 0 ? 'border-b border-outline-variant/40' : 'border-b border-outline-variant/10'}>
                                  {row.map((cell: any, ci: number) => (
                                    <td key={ci} className="px-1.5 py-0.5 text-on-surface whitespace-nowrap">{String(cell)}</td>
                                  ))}
                                </tr>
                              ))}
                            </table>
                          </div>
                        </div>
                      )}
                    </div>
                  )}
                </>
              )}

              {/* Result for choice mode */}
              {mode === 'choice' && result && (
                <div className={`w-[280px] shrink-0 glass-panel border rounded-xl p-4 self-stretch overflow-y-auto ${
                  result.result === 'correct' ? 'border-secondary/30 bg-secondary/5' : 'border-error/30 bg-error/5'
                }`}>
                  <div className="flex items-center gap-2 mb-2">
                    <span className={`material-symbols-outlined text-[20px] ${result.result === 'correct' ? 'text-secondary' : 'text-error'}`}>
                      {result.result === 'correct' ? 'check_circle' : 'cancel'}
                    </span>
                    <span className={`font-code-md text-code-md font-bold ${result.result === 'correct' ? 'text-secondary' : 'text-error'}`}>
                      {result.result === 'correct' ? t('practice.correctMsg') : t('practice.wrongMsg')}</span>
                  </div>
                  <p className="font-code-sm text-[11px] text-on-surface-variant">{result.message}</p>
                  {result.result !== 'correct' && (
                    <p className="mt-2 text-[11px] text-secondary">{t('practice.correctAnswer')}：{answer}. {options[answer]}</p>
                  )}
                </div>
              )}
            </div>
          </div>

          {/* Right sidebar: leaderboard */}
          {leaderboard.length > 0 && (
            <div className="w-[240px] shrink-0 glass-panel border border-white/10 rounded-xl p-4 self-start sticky top-4">
              <h3 className="font-code-md text-code-md text-on-surface font-bold mb-3 flex items-center gap-2">
                <span className="material-symbols-outlined text-[18px] text-tertiary">leaderboard</span>{t('practice.leaderboard')}</h3>
              <div className="space-y-1">
                {leaderboard.map((entry: any, i: number) => (
                  <div key={i} className="flex items-center gap-2 text-[11px] font-code-sm py-1">
                    <span className="w-5 text-center font-bold text-outline">{i + 1}</span>
                    <span className="flex-1 text-on-surface truncate">{entry.username}</span>
                    <span className="text-on-surface-variant text-[10px]">{entry.total}题</span>
                    <span className="text-primary font-bold w-10 text-right text-[10px]">{entry.accuracy}%</span>
                  </div>))}
              </div>
            </div>
          )}
        </div>
      ) : (
        <div className="flex-1 flex gap-4 min-h-0">
          <div className="flex-1 flex items-center justify-center text-on-surface-variant/50 flex-col gap-3">
            <span className="material-symbols-outlined text-[48px]">school</span>
            <span className="font-code-md">{t('practice.empty')}</span>
          </div>
          {leaderboard.length > 0 && (
            <div className="w-[240px] shrink-0 glass-panel border border-white/10 rounded-xl p-4 self-start sticky top-4">
              <h3 className="font-code-md text-code-md text-on-surface font-bold mb-3 flex items-center gap-2">
                <span className="material-symbols-outlined text-[18px] text-tertiary">leaderboard</span>{t('practice.leaderboard')}</h3>
              <div className="space-y-1">
                {leaderboard.map((entry: any, i: number) => (
                  <div key={i} className="flex items-center gap-2 text-[11px] font-code-sm py-1">
                    <span className="w-5 text-center font-bold text-outline">{i + 1}</span>
                    <span className="flex-1 text-on-surface truncate">{entry.username}</span>
                    <span className="text-on-surface-variant text-[10px]">{entry.total}题</span>
                    <span className="text-primary font-bold w-10 text-right text-[10px]">{entry.accuracy}%</span>
                  </div>))}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
