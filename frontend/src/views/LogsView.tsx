import { useState, useEffect, useRef, useCallback } from 'react';

interface LogEntry {
  time: string;
  level: string;
  module: string;
  message: string;
}

export default function LogsView() {
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [searchText, setSearchText] = useState('');
  const [isLive, setIsLive] = useState(true);
  const scrollRef = useRef<HTMLDivElement>(null);

  const fetchLogs = useCallback(async () => {
    try {
      const res = await fetch('/api/logs?limit=100');
      if (res.ok) {
        const data = await res.json();
        setLogs(data);
      }
    } catch { /* ignore */ }
  }, []);

  useEffect(() => {
    fetchLogs();
    const interval = setInterval(fetchLogs, 3000);
    return () => clearInterval(interval);
  }, [fetchLogs]);

  useEffect(() => {
    if (isLive && scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [logs, isLive]);

  const filtered = logs.filter(e =>
    !searchText || e.message.toLowerCase().includes(searchText.toLowerCase()) ||
    e.module.toLowerCase().includes(searchText.toLowerCase()) ||
    e.level.toLowerCase().includes(searchText.toLowerCase())
  );

  const levelColor: Record<string, string> = {
    INFO: 'text-on-surface-variant',
    WARN: 'text-tertiary',
    ERROR: 'text-error',
  };
  const levelBg: Record<string, string> = {
    INFO: '',
    WARN: 'bg-tertiary/10',
    ERROR: 'bg-error/10',
  };

  return (
    <div className="flex-col p-gutter overflow-y-auto h-full flex">
      <div className="glass-panel rounded-lg h-full flex flex-col overflow-hidden border border-white/5">
        <div className="p-4 border-b border-outline-variant/30 flex items-center justify-between shrink-0 bg-surface-container/50">
          <div className="flex items-center gap-2">
            <span className="material-symbols-outlined text-[20px] text-tertiary">terminal</span>
            <h2 className="font-headline-md text-headline-md text-on-surface font-bold">System Logs</h2>
            <span className="text-[10px] text-outline ml-2">({filtered.length} entries)</span>
          </div>
          <div className="flex items-center gap-2">
            <div className="relative">
              <span className="material-symbols-outlined absolute left-2 top-1/2 -translate-y-1/2 text-on-surface-variant text-[14px]">search</span>
              <input
                type="text"
                placeholder="Grep logs..."
                className="bg-surface-container-highest border border-outline-variant/30 rounded px-7 py-1 font-code-sm text-code-sm text-on-surface focus:outline-none focus:border-tertiary focus:ring-1 focus:ring-tertiary/50 w-64"
                value={searchText}
                onChange={(e) => setSearchText(e.target.value)}
              />
            </div>
            <button className="flex items-center gap-1 font-code-sm text-code-sm text-on-surface-variant hover:text-on-surface transition-colors border border-outline-variant/30 rounded px-2 py-1">
              <span className="material-symbols-outlined text-[16px]">filter_list</span> Filter
            </button>
            <button
              className={`flex items-center gap-1 font-code-sm text-code-sm px-2 py-1 rounded transition-colors ${
                isLive ? 'bg-tertiary/10 text-tertiary' : 'bg-surface-variant text-on-surface-variant'
              }`}
              onClick={() => setIsLive(!isLive)}
            >
              <span className={`material-symbols-outlined text-[16px] ${isLive ? 'animate-pulse' : ''}`}>
                {isLive ? 'pause_circle' : 'play_circle'}
              </span>
              {isLive ? 'Live' : 'Paused'}
            </button>
          </div>
        </div>
        <div ref={scrollRef} className="flex-1 bg-[#050505] font-code-sm text-[11px] p-4 overflow-y-auto font-mono text-outline leading-relaxed flex flex-col gap-1">
          {filtered.length > 0 ? filtered.map((entry, i) => (
            <div key={i} className={`hover:bg-white/5 px-2 py-0.5 rounded -mx-2 ${levelBg[entry.level] ?? ''}`}>
              <span className="text-on-surface-variant">{entry.time} [{entry.level}]</span>{' '}
              <span className={levelColor[entry.level] ?? 'text-on-surface-variant'}>[{entry.module}]</span>{' '}
              <span className={entry.level === 'ERROR' ? 'text-error' : entry.level === 'WARN' ? 'text-tertiary' : 'text-on-surface-variant'}>
                {entry.message}
              </span>
            </div>
          )) : (
            <div className="flex items-center justify-center h-full text-on-surface-variant/50">
              {searchText ? 'No matching log entries.' : 'No logs yet. Run queries to generate log entries.'}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
