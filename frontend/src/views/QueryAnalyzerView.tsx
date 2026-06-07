import { useState } from 'react';
import { fetchExplainPlan, fetchExplainAI } from '../api';
import type { ExplainResult, ExplainNode } from '../api';
import { cn } from '../lib/utils';
import { useT } from '../i18n';

export default function QueryAnalyzerView() {
  const { t } = useT();
  const [sql, setSql] = useState('SELECT * FROM emp;');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [explainResult, setExplainResult] = useState<ExplainResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [aiExplanation, setAiExplanation] = useState<string | null>(null);
  const [isLoadingAI, setIsLoadingAI] = useState(false);
  const [aiError, setAiError] = useState<string | null>(null);
  const [rightTab, setRightTab] = useState<'raw' | 'ai'>('ai');

  const handleAnalyze = async () => {
    if (!sql.trim() || isAnalyzing) return;
    setIsAnalyzing(true);
    setError(null);
    setExplainResult(null);
    setAiExplanation(null);
    setAiError(null);
    setIsLoadingAI(false);
    try {
      const result = await fetchExplainPlan(sql.trim());
      setExplainResult(result);
      if (result.status === 'error') { setError(result.message ?? t('analyzer.aiFailed')); return; }

      // Fetch AI explanation asynchronously after plan loads
      setIsLoadingAI(true);
      setRightTab('ai');
      try {
        const aiResult = await fetchExplainAI(sql.trim(), result.raw ?? '', result.simulated ?? false);
        if (aiResult.status === 'success') setAiExplanation(aiResult.explanation ?? null);
        else if (aiResult.status === 'not_configured') setAiError(t('analyzer.aiUnavailable'));
        else setAiError(aiResult.message ?? t('analyzer.aiFailed'));
      } catch (aiErr: any) { setAiError(aiErr.message ?? t('analyzer.aiFailed')); }
      finally { setIsLoadingAI(false); }
    } catch (err: any) {
      setError(err.message === 'Failed to fetch'
        ? t('analyzer.noConnect')
        : err.message ?? 'Failed to analyze query');
    } finally {
      setIsAnalyzing(false);
    }
  };

  return (
    <div className="flex-col p-gutter overflow-y-auto h-full flex">
      <div className="glass-panel rounded-lg h-full flex flex-col p-panel-padding w-full border border-white/5">
        <div className="flex items-center gap-2 mb-4 border-b border-outline-variant/30 pb-2 shrink-0">
          <span className="material-symbols-outlined text-[24px] text-primary">query_stats</span>
          <h2 className="font-headline-md text-headline-md text-on-surface font-bold">{t('analyzer.title')}</h2>
        </div>

        {/* SQL Input */}
        <div className="shrink-0 mb-4">
          <div className="relative bg-background border border-outline-variant/50 rounded-lg p-1 focus-within:border-primary/50 focus-within:ring-1 focus-within:ring-primary/20 transition-all flex items-start shadow-inner">
            <span className="material-symbols-outlined text-primary p-2">analytics</span>
            <textarea
              className="flex-1 bg-transparent border-none resize-none px-2 py-2.5 font-code-md text-code-md text-on-surface focus:outline-none min-h-[40px]"
              placeholder={t('analyzer.placeholder')}
              value={sql}
              onChange={(e) => setSql(e.target.value)}
              onKeyDown={(e) => {
                if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') handleAnalyze();
              }}
            />
            <button
              className="m-1 bg-primary text-black px-4 py-2 rounded font-bold font-code-sm hover:bg-primary-fixed transition-colors flex items-center gap-1 shadow-[0_0_15px_rgba(152,203,255,0.4)] disabled:opacity-50"
              onClick={handleAnalyze}
              disabled={isAnalyzing || !sql.trim()}
            >
              {isAnalyzing ? (
                <><span className="material-symbols-outlined text-[16px] animate-spin">sync</span> {t('analyzer.analyzing')}</>
              ) : (
                t('analyzer.analyze')
              )}
            </button>
          </div>
          {error && (
            <div className="mt-2 text-error font-code-sm text-[11px] flex items-center gap-1">
              <span className="material-symbols-outlined text-[14px]">error</span> {error}
            </div>
          )}
        </div>

        <div className="flex-1 flex gap-4 min-h-0">
          {/* Execution Plan Tree */}
          <div className="flex-1 overflow-auto">
            {explainResult?.plan_tree ? (
              <div className="space-y-2">
                <h3 className="font-code-md text-code-md text-on-surface font-bold mb-3 flex items-center gap-2">
                  <span className="material-symbols-outlined text-[16px]">account_tree</span>
                  {t('analyzer.plan')}
                  {explainResult?.simulated && (
                    <span className="text-[10px] bg-yellow-500/10 text-yellow-400 border border-yellow-500/20 rounded px-1.5 py-0.5 font-code-sm">{t('analyzer.simulated')}</span>
                  )}
                </h3>
                <PlanTreeNode node={explainResult.plan_tree} depth={0} />
              </div>
            ) : !isAnalyzing && (
              <div className="flex-1 flex flex-col items-center justify-center text-on-surface-variant h-full">
                <span className="material-symbols-outlined text-4xl mb-2 opacity-50">analytics</span>
                <p className="font-code-md text-code-md">{t('analyzer.plan')}</p>
                <p className="font-code-sm text-code-sm opacity-50 mt-1">{t('analyzer.empty')}</p>
              </div>
            )}
            {isAnalyzing && (
              <div className="flex items-center justify-center h-full">
                <span className="material-symbols-outlined text-[32px] text-primary animate-spin">sync</span>
              </div>
            )}
          </div>

          {/* Right Panel — tabbed: AI Analysis | Raw Output */}
          {explainResult?.raw && (
            <div className="w-80 shrink-0 border-l border-outline-variant/20 pl-4 overflow-auto flex flex-col min-h-0">
              <div className="flex gap-1 mb-3 border-b border-outline-variant/20 pb-2 shrink-0">
                <button onClick={() => setRightTab('ai')}
                  className={cn("px-3 py-1 text-[11px] font-code-sm rounded-t transition-colors",
                    rightTab === 'ai' ? "text-primary border-b-2 border-primary font-bold" : "text-on-surface-variant hover:text-on-surface")}>
                  <span className="material-symbols-outlined text-[14px] align-middle mr-1">psychology</span>{t('analyzer.aiTab')}
                </button>
                <button onClick={() => setRightTab('raw')}
                  className={cn("px-3 py-1 text-[11px] font-code-sm rounded-t transition-colors",
                    rightTab === 'raw' ? "text-primary border-b-2 border-primary font-bold" : "text-on-surface-variant hover:text-on-surface")}>
                  <span className="material-symbols-outlined text-[14px] align-middle mr-1">terminal</span>{t('analyzer.rawOutput')}
                </button>
              </div>
              {rightTab === 'raw' ? (
                <pre className="font-code-sm text-[11px] text-on-surface-variant whitespace-pre-wrap bg-black/20 rounded p-3 overflow-auto">{explainResult.raw}</pre>
              ) : (
                <div className="overflow-auto flex-1">
                  {isLoadingAI && (
                    <div className="flex items-center gap-2 text-on-surface-variant font-code-sm text-[11px] p-3">
                      <span className="material-symbols-outlined text-[16px] text-primary animate-spin">sync</span>{t('analyzer.aiLoading')}
                    </div>
                  )}
                  {aiError && (
                    <div className="text-[11px] text-on-surface-variant bg-black/20 rounded p-3 font-code-sm">
                      <span className="material-symbols-outlined text-[14px] text-error align-middle mr-1">info</span>
                      {aiError}
                    </div>
                  )}
                  {aiExplanation && (
                    <div className="font-code-sm text-[11px] text-on-surface-variant whitespace-pre-wrap bg-black/20 rounded p-3 leading-relaxed"
                      style={{ whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
                      {aiExplanation}
                    </div>
                  )}
                  {!isLoadingAI && !aiError && !aiExplanation && (
                    <div className="text-[11px] text-on-surface-variant/50 p-3 font-code-sm italic">{t('analyzer.aiPlaceholder')}</div>
                  )}
                </div>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

function PlanTreeNode({ node, depth }: { node: ExplainNode; depth: number; key?: string }) {
  const { t } = useT();
  const typeColors: Record<string, string> = {
    result: 'border-primary text-primary',
    sort: 'border-tertiary text-tertiary',
    aggregate: 'border-tertiary text-tertiary',
    join: 'border-secondary text-secondary',
    filter: 'border-[#569cd6] text-[#569cd6]',
    scan: 'border-on-surface-variant text-on-surface-variant',
  };

  const colors = typeColors[node.type] ?? 'border-outline-variant text-on-surface-variant';

  return (
    <div>
      <div className={`border-l-2 ${colors} pl-3 py-1`} style={{ marginLeft: `${depth * 24}px` }}>
        <div className="flex items-center gap-3 glass-panel rounded px-3 py-2 border border-white/5 hover:border-white/10 transition-colors">
          <span className={`font-code-sm text-[10px] font-bold uppercase ${colors.split(' ')[1]}`}>{node.type}</span>
          <span className="font-code-sm text-[12px] text-on-surface font-bold">{node.label}</span>
          <div className="flex gap-3 ml-auto text-[10px] text-outline">
            <span>{t('analyzer.cost')}: <span className="text-on-surface-variant">{node.cost.toFixed(1)}</span></span>
            <span>{t('analyzer.rows')}: <span className="text-on-surface-variant">{node.rows}</span></span>
          </div>
        </div>
      </div>
      {node.children?.map((child) => (
        <PlanTreeNode key={child.id} node={child} depth={depth + 1} />
      ))}
    </div>
  );
}
