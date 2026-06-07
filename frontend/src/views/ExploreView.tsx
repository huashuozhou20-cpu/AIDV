import { useState, useEffect } from 'react';
import { fetchSchemaTree } from '../api';
import type { SchemaTree, TableInfo } from '../api';

export default function ExploreView() {
  const [schemaTree, setSchemaTree] = useState<SchemaTree | null>(null);
  const [selectedTable, setSelectedTable] = useState<TableInfo | null>(null);

  useEffect(() => {
    fetchSchemaTree().then(setSchemaTree).catch(() => {});
  }, []);

  if (!schemaTree || schemaTree.tables.length === 0) {
    return (
      <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col items-center justify-center">
        <div className="flex flex-col items-center justify-center text-on-surface-variant max-w-lg text-center gap-4">
          <div className="w-24 h-24 rounded-full bg-primary/10 flex items-center justify-center text-primary shadow-[0_0_20px_rgba(152,203,255,0.1)]">
            <span className="material-symbols-outlined text-[48px]">travel_explore</span>
          </div>
          <h2 className="font-headline-lg text-headline-lg text-on-surface font-bold">Data Hub Explorer</h2>
          <p className="font-code-md text-code-md opacity-80">
            {schemaTree?.error ?? 'No tables found. Create tables in RMDB to see them here.'}
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="flex-1 p-gutter overflow-y-auto h-full flex flex-col">
      <div className="flex justify-between items-end mb-6">
        <div>
          <h1 className="font-headline-lg text-headline-lg text-on-surface font-bold flex items-center gap-2">
            <span className="material-symbols-outlined text-[28px] text-primary">travel_explore</span>
            Schema Explorer
          </h1>
          <p className="font-code-md text-code-md text-on-surface-variant mt-1">
            {schemaTree.tables.length} tables · {schemaTree.tables.reduce((s, t) => s + t.columns.length, 0)} columns total
          </p>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-gutter">
        {schemaTree.tables.map(table => (
          <div
            key={table.name}
            className={`glass-panel border rounded-xl p-5 cursor-pointer transition-all group ${
              selectedTable?.name === table.name ? 'border-primary/50 bg-primary/5' : 'border-outline-variant/30 hover:border-white/20'
            }`}
            onClick={() => setSelectedTable(selectedTable?.name === table.name ? null : table)}
          >
            <div className="flex items-center gap-3 mb-3">
              <div className="w-10 h-10 rounded-lg bg-secondary/10 flex items-center justify-center text-secondary group-hover:scale-110 transition-transform">
                <span className="material-symbols-outlined">table_view</span>
              </div>
              <div>
                <h3 className="font-headline-md text-headline-md text-on-surface font-bold">{table.name}</h3>
                <span className="font-code-sm text-[10px] text-outline">{table.columns.length} columns</span>
              </div>
            </div>
            {selectedTable?.name === table.name && (
              <div className="mt-3 pt-3 border-t border-outline-variant/20 space-y-1">
                {table.columns.map(col => (
                  <div key={col.name} className="flex items-center justify-between text-[11px] font-code-sm py-0.5">
                    <span className="text-on-surface">{col.name}</span>
                    <span className="text-outline bg-surface-container px-1.5 py-0.5 rounded">{col.type}</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  );
}
