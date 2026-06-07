import { useState } from 'react';

interface Anomaly {
  type: string;
  severity: string;
  time: string;
  detail: string;
  sql: string;
}

export default function AIEngineView({ onNavigateToDiff }: { onNavigateToDiff: () => void }) {
  const [anomalies, setAnomalies] = useState<Anomaly[] | null>(null);
  const [loadingAnomalies, setLoadingAnomalies] = useState(false);

  const loadAnomalies = async () => {
    setLoadingAnomalies(true);
    try {
      const res = await fetch('/api/anomalies');
      if (res.ok) {
        const data = await res.json();
        setAnomalies(data.anomalies ?? []);
      }
    } catch { /* ignore */ }
    setLoadingAnomalies(false);
  };

  const sevColor: Record<string, string> = {
    high: 'text-error border-error/30 bg-error/10',
    medium: 'text-tertiary border-tertiary/30 bg-tertiary/10',
    low: 'text-on-surface-variant border-outline-variant/30 bg-surface-variant/30',
  };

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      <div className="flex justify-between items-end mb-6">
        <div>
          <h1 className="font-headline-lg text-headline-lg text-on-surface font-bold flex items-center gap-2">
            <span className="material-symbols-outlined text-[28px] text-tertiary">psychology</span>
            AI Engine
          </h1>
          <p className="font-code-md text-code-md text-on-surface-variant mt-1">Machine learning powered automations and insights.</p>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-gutter">
        {/* Natural Language to SQL */}
        <div
          onClick={onNavigateToDiff}
          className="glass-panel border border-outline-variant/30 rounded-xl p-6 hover:border-primary/50 transition-colors group cursor-pointer relative overflow-hidden"
        >
          <div className="absolute top-0 right-0 p-4 opacity-10 group-hover:opacity-20 transition-opacity">
            <span className="material-symbols-outlined text-[100px] text-primary">chat_spark</span>
          </div>
          <div className="w-12 h-12 rounded-lg bg-primary/10 flex items-center justify-center text-primary group-hover:scale-110 transition-transform mb-4 shadow-[0_0_15px_rgba(152,203,255,0.2)]">
            <span className="material-symbols-outlined text-[24px]">auto_awesome</span>
          </div>
          <h2 className="font-headline-md text-headline-md text-on-surface font-bold mb-2">Query Generator</h2>
          <p className="font-code-md text-code-md text-on-surface-variant max-w-md relative z-10">
            Translate natural language business questions into optimized SQL queries utilizing your schema context.
          </p>
          <div className="mt-8 flex items-center gap-2 font-code-sm text-code-sm text-primary font-bold group-hover:translate-x-2 transition-transform">
            Launch Tool <span className="material-symbols-outlined text-[16px]">arrow_forward</span>
          </div>
        </div>

        {/* Anomaly Detection */}
        <div
          onClick={() => { if (!anomalies) loadAnomalies(); }}
          className="glass-panel border border-outline-variant/30 rounded-xl p-6 hover:border-secondary/50 transition-colors group cursor-pointer relative overflow-hidden"
        >
          <div className="absolute top-0 right-0 p-4 opacity-10 group-hover:opacity-20 transition-opacity">
            <span className="material-symbols-outlined text-[100px] text-secondary">monitoring</span>
          </div>
          <div className="w-12 h-12 rounded-lg bg-secondary/10 flex items-center justify-center text-secondary group-hover:scale-110 transition-transform mb-4 shadow-[0_0_15px_rgba(78,222,163,0.2)]">
            <span className={`material-symbols-outlined text-[24px] ${loadingAnomalies ? 'animate-spin' : ''}`}>
              {loadingAnomalies ? 'sync' : 'troubleshoot'}
            </span>
          </div>
          <h2 className="font-headline-md text-headline-md text-on-surface font-bold mb-2">Anomaly Detection</h2>
          <p className="font-code-md text-code-md text-on-surface-variant max-w-md relative z-10">
            {anomalies === null
              ? 'Automatically identify slow queries, errors, and unexpected patterns. Click to scan.'
              : anomalies.length === 0
                ? 'No anomalies detected — all queries running normally.'
                : `Found ${anomalies.length} issue(s) — slow queries and errors detected.`}
          </p>
          {anomalies !== null && anomalies.length > 0 && (
            <div className="mt-4 space-y-2 max-h-[200px] overflow-auto relative z-10">
              {anomalies.slice(0, 5).map((a, i) => (
                <div key={i} className={`text-[10px] font-code-sm px-2 py-1 rounded border ${sevColor[a.severity] ?? ''}`}>
                  <span className="font-bold uppercase mr-1">[{a.severity}]</span>
                  <span className="opacity-70 mr-1">{a.time?.slice(11, 19)}</span>
                  {a.detail}
                </div>
              ))}
              {anomalies.length > 5 && (
                <div className="text-[10px] text-outline text-center">+{anomalies.length - 5} more</div>
              )}
            </div>
          )}
          <div className="mt-6 flex items-center gap-2 font-code-sm text-code-sm text-secondary font-bold group-hover:translate-x-2 transition-transform">
            {anomalies === null ? 'Click to Scan' : 'Scan Again'} <span className="material-symbols-outlined text-[16px]">arrow_forward</span>
          </div>
        </div>
      </div>
    </div>
  );
}
