import { useState, useEffect } from 'react';
import { fetchMetrics, fetchSchemaTree } from '../api';
import type { Metrics, SchemaTree } from '../api';

export default function AutomationView() {
  const [metrics, setMetrics] = useState<Metrics | null>(null);
  const [schemaTree, setSchemaTree] = useState<SchemaTree | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  useEffect(() => {
    const load = async () => {
      try {
        const [m, s] = await Promise.all([fetchMetrics(), fetchSchemaTree()]);
        setMetrics(m);
        setSchemaTree(s);
      } catch { /* ignore */ }
    };
    load();
    const interval = setInterval(load, 5000);
    return () => clearInterval(interval);
  }, []);

  const handleRefreshSchema = async () => {
    setRefreshing(true);
    try {
      const s = await fetchSchemaTree();
      setSchemaTree(s);
    } catch { /* ignore */ }
    setRefreshing(false);
  };

  const uptimeH = metrics ? Math.floor(metrics.uptime_seconds / 3600) : 0;
  const uptimeM = metrics ? Math.floor((metrics.uptime_seconds % 3600) / 60) : 0;
  const tableCount = schemaTree?.tables.length ?? 0;

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      <div className="flex justify-between items-end mb-6">
        <div>
          <h1 className="font-headline-lg text-headline-lg text-on-surface font-bold flex items-center gap-2">
            <span className="material-symbols-outlined text-[28px] text-primary">bolt</span>
            Active Tasks
          </h1>
          <p className="font-code-md text-code-md text-on-surface-variant mt-1">System status and maintenance routines.</p>
        </div>
        <button
          className="bg-primary text-black font-bold font-code-sm px-4 py-2 rounded flex items-center gap-2 hover:bg-primary-fixed transition-colors"
          onClick={handleRefreshSchema}
          disabled={refreshing}
        >
          <span className={`material-symbols-outlined text-[18px] ${refreshing ? 'animate-spin' : ''}`}>
            {refreshing ? 'sync' : 'add'}
          </span>
          Refresh Schema
        </button>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-gutter">
        {/* Card 1 - Schema Status */}
        <div className="glass-panel border border-outline-variant/30 rounded-xl p-5 hover:border-primary/50 transition-colors group cursor-pointer">
          <div className="flex justify-between items-start mb-4">
            <div className="w-10 h-10 rounded-lg bg-primary/10 flex items-center justify-center text-primary group-hover:scale-110 transition-transform">
              <span className="material-symbols-outlined">schema</span>
            </div>
            <span className="bg-secondary/10 text-secondary border border-secondary/30 px-2 py-0.5 rounded-full font-code-sm text-[10px] flex items-center gap-1">
              <span className="w-1.5 h-1.5 rounded-full bg-secondary animate-pulse"></span> Active
            </span>
          </div>
          <h3 className="font-headline-md text-headline-md text-on-surface font-bold mb-1">Schema Monitor</h3>
          <p className="font-code-sm text-code-sm text-on-surface-variant mb-4">
            {tableCount > 0 ? `${tableCount} tables discovered from RMDB.` : 'No tables loaded.'}
          </p>
          <div className="flex items-center gap-2 font-code-sm text-[10px] text-outline pt-3 border-t border-outline-variant/20">
            <span className="material-symbols-outlined text-[14px]">database</span>
            {schemaTree?.tables.slice(0, 3).map(t => t.name).join(', ') ?? '--'}
            {tableCount > 3 ? ` +${tableCount - 3} more` : ''}
          </div>
        </div>

        {/* Card 2 - Server Uptime */}
        <div className="glass-panel border border-outline-variant/30 rounded-xl p-5 hover:border-tertiary/50 transition-colors group cursor-pointer">
          <div className="flex justify-between items-start mb-4">
            <div className="w-10 h-10 rounded-lg bg-tertiary/10 flex items-center justify-center text-tertiary group-hover:scale-110 transition-transform">
              <span className="material-symbols-outlined">schedule</span>
            </div>
            <span className={`px-2 py-0.5 rounded-full font-code-sm text-[10px] flex items-center gap-1 ${
              uptimeH > 0 ? 'bg-secondary/10 text-secondary border border-secondary/30' : 'bg-tertiary/10 text-tertiary border border-tertiary/30'
            }`}>
              <span className={`w-1.5 h-1.5 rounded-full ${uptimeH > 0 ? 'bg-secondary animate-pulse' : 'bg-tertiary'}`}></span>
              {uptimeH > 0 ? 'Running' : 'Starting'}
            </span>
          </div>
          <h3 className="font-headline-md text-headline-md text-on-surface font-bold mb-1">Server Uptime</h3>
          <p className="font-code-sm text-code-sm text-on-surface-variant mb-4">
            RMDB has been running for {uptimeH}h {uptimeM}m.
          </p>
          <div className="mb-4">
            <div className="flex justify-between font-code-sm text-[10px] text-on-surface-variant mb-1">
              <span>Total Queries</span>
              <span>{metrics?.total_queries ?? 0}</span>
            </div>
            <div className="h-1.5 w-full bg-surface-container-highest rounded-full overflow-hidden">
              <div className="h-full bg-tertiary" style={{ width: `${Math.min(100, (metrics?.total_queries ?? 0) * 2)}%` }}></div>
            </div>
          </div>
          <div className="flex items-center gap-2 font-code-sm text-[10px] text-outline pt-3 border-t border-outline-variant/20">
            <span className="material-symbols-outlined text-[14px]">speed</span>
            QPS: {metrics?.qps ?? '--'}
          </div>
        </div>

        {/* Card 3 - Buffer Pool */}
        <div className="glass-panel border border-outline-variant/30 rounded-xl p-5 hover:border-outline-variant/60 transition-colors group cursor-pointer">
          <div className="flex justify-between items-start mb-4">
            <div className="w-10 h-10 rounded-lg bg-surface-variant flex items-center justify-center text-on-surface-variant group-hover:scale-110 transition-transform">
              <span className="material-symbols-outlined">memory</span>
            </div>
            <span className="bg-surface-variant text-on-surface-variant border border-outline-variant/30 px-2 py-0.5 rounded-full font-code-sm text-[10px]">
              Estimated
            </span>
          </div>
          <h3 className="font-headline-md text-headline-md text-on-surface font-bold mb-1">Buffer Pool</h3>
          <p className="font-code-sm text-code-sm text-on-surface-variant mb-4">
            Hit rate and connection monitoring.
          </p>
          <div className="mb-4">
            <div className="flex justify-between font-code-sm text-[10px] text-on-surface-variant mb-1">
              <span>Buffer Pool Hit Rate</span>
              <span>{metrics?.buffer_pool_hit_rate ?? '--'}%</span>
            </div>
            <div className="h-1.5 w-full bg-surface-container-highest rounded-full overflow-hidden">
              <div className="h-full bg-secondary" style={{ width: `${metrics?.buffer_pool_hit_rate ?? 0}%` }}></div>
            </div>
          </div>
          <div className="flex items-center gap-2 font-code-sm text-[10px] text-outline pt-3 border-t border-outline-variant/20">
            <span className="material-symbols-outlined text-[14px]">link</span>
            {metrics?.active_connections ?? '--'} active connections
          </div>
        </div>
      </div>
    </div>
  );
}
