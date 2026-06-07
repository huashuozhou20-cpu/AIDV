import { useState } from 'react';
import { chatQuery, executeQuery } from '../api';

export default function SQLDiffView({ onBack }: { onBack: () => void }) {
  const [originalSQL, setOriginalSQL] = useState('');
  const [nlInput, setNlInput] = useState(
    '输入自然语言描述你想查什么，例如：查询所有员工'
  );
  const [generatedSQL, setGeneratedSQL] = useState<string | null>(null);
  const [isGenerating, setIsGenerating] = useState(false);
  const [isApplying, setIsApplying] = useState(false);
  const [genError, setGenError] = useState<string | null>(null);
  const [toastMessage, setToastMessage] = useState<string | null>(null);

  const showToast = (msg: string) => {
    setToastMessage(msg);
    setTimeout(() => setToastMessage((c) => c === msg ? null : c), 3000);
  };

  const handleGenerate = async () => {
    if (!nlInput.trim() || isGenerating) return;
    setIsGenerating(true);
    setGenError(null);
    try {
      const result = await chatQuery(nlInput.trim());
      if (result.status === 'error') {
        setGenError(result.message ?? 'Generation failed');
      } else if (result.generated_sql) {
        setGeneratedSQL(result.generated_sql);
      }
    } catch (err: any) {
      setGenError(err.message ?? 'Failed to generate SQL');
    } finally {
      setIsGenerating(false);
    }
  };

  const handleApply = async () => {
    if (!generatedSQL || isApplying) return;
    setIsApplying(true);
    try {
      const result = await executeQuery(generatedSQL);
      if (result.status === 'error') {
        showToast(`Query failed: ${result.message}`);
      } else {
        showToast(`Query executed: ${result.data.length} rows in ${result.execution_time_ms}ms`);
        setOriginalSQL(generatedSQL);
        setGeneratedSQL(null);
      }
    } catch (err: any) {
      showToast(`Error: ${err.message}`);
    } finally {
      setIsApplying(false);
    }
  };

  const handleCopy = () => {
    if (!generatedSQL) return;
    navigator.clipboard.writeText(generatedSQL).then(() => showToast('SQL copied to clipboard'));
  };

  const originalLines = originalSQL.split('\n');
  const generatedLines = (generatedSQL || '// Generating...').split('\n');
  const maxLines = Math.max(originalLines.length, generatedLines.length, 6);

  return (
    <div className="flex-1 overflow-y-auto h-full flex flex-col items-center justify-center p-gutter relative">
      <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[600px] h-[600px] bg-primary/5 rounded-full blur-[100px] pointer-events-none"></div>

      <div className="w-full max-w-5xl flex flex-col h-full z-10 glass-panel rounded-xl border border-outline-variant/30 overflow-hidden shadow-2xl">
        <div className="h-14 border-b border-outline-variant/30 px-4 flex items-center justify-between shrink-0 bg-surface-container/50">
          <div className="flex items-center gap-3">
            <button onClick={onBack} className="p-1.5 rounded-full hover:bg-white/10 text-on-surface-variant hover:text-on-surface transition-colors flex items-center justify-center">
               <span className="material-symbols-outlined text-[20px]">arrow_back</span>
            </button>
            <span className="font-code-md text-code-md text-on-surface font-bold">Query Generator</span>
          </div>
          <div className="flex gap-2 text-[10px] uppercase font-bold text-outline tracking-wider">
             <span>Model: DeepSeek</span>
          </div>
        </div>

        <div className="flex-1 flex flex-col p-6 gap-6 overflow-hidden">

           {/* Input Area */}
           <div className="shrink-0 relative">
              <div className="absolute inset-0 bg-gradient-to-r from-primary/20 via-tertiary/20 to-secondary/20 rounded-lg blur-md opacity-50"></div>
              <div className="relative bg-background border border-outline-variant/50 rounded-lg p-1 block focus-within:border-primary/50 focus-within:ring-1 focus-within:ring-primary/20 transition-all flex items-start shadow-inner">
                 <span className="material-symbols-outlined text-primary p-2">auto_awesome</span>
                 <textarea
                    className="flex-1 bg-transparent border-none resize-none px-2 py-2.5 font-code-md text-code-md text-on-surface focus:outline-none min-h-[60px]"
                    placeholder="Describe the query you need. E.g., 'Find all users who made a transaction over $500 in the last 30 days...'"
                    value={nlInput}
                    onChange={(e) => setNlInput(e.target.value)}
                    onKeyDown={(e) => {
                      if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') handleGenerate();
                    }}
                 ></textarea>
                 <button
                    className="m-1 bg-primary text-black px-4 py-2 rounded font-bold font-code-sm hover:bg-primary-fixed transition-colors flex items-center gap-1 shadow-[0_0_15px_rgba(152,203,255,0.4)] disabled:opacity-50"
                    onClick={handleGenerate}
                    disabled={isGenerating || !nlInput.trim()}
                 >
                    {isGenerating ? (
                      <><span className="material-symbols-outlined text-[16px] animate-spin">sync</span> Generating...</>
                    ) : (
                      'Generate'
                    )}
                 </button>
              </div>
              {genError && (
                <div className="mt-2 text-error font-code-sm text-[11px] flex items-center gap-1">
                  <span className="material-symbols-outlined text-[14px]">error</span> {genError}
                </div>
              )}
           </div>

           {/* Diff Viewer */}
           <div className="flex-1 flex flex-col min-h-0 border border-white/10 rounded-lg overflow-hidden bg-black/30 shadow-[0_8px_32px_rgba(0,0,0,0.3)] relative">
              <div className="h-9 flex items-center justify-between px-4 bg-black/40 shrink-0">
                 <div className="font-code-sm text-code-sm text-on-surface flex gap-6">
                    <div className="flex items-center gap-2 text-error"><span className="material-symbols-outlined text-[16px]">remove_circle</span> Original <span className="opacity-50 text-[10px]">query_editor.sql</span></div>
                    <div className="flex items-center gap-2 text-secondary"><span className="material-symbols-outlined text-[16px]">add_circle</span> Generated</div>
                 </div>
                 <div className="flex gap-2">
                    <button onClick={handleCopy} disabled={!generatedSQL} className="text-on-surface-variant hover:text-on-surface font-code-sm px-2 py-1 rounded hover:bg-white/10 transition-colors flex items-center gap-1 text-[11px] disabled:opacity-30"><span className="material-symbols-outlined text-[14px]">content_copy</span> Copy</button>
                    <button onClick={handleApply} disabled={!generatedSQL || isApplying} className="bg-primary/20 text-primary border border-primary/30 px-3 py-1 rounded font-code-sm hover:bg-primary/30 transition-colors font-bold flex items-center gap-1 text-[11px] disabled:opacity-30"><span className="material-symbols-outlined text-[14px]">{isApplying ? 'sync' : 'check'}</span> {isApplying ? 'Applying...' : 'Apply'}</button>
                 </div>
              </div>

              <div className="flex-1 overflow-auto flex text-[13px] font-mono leading-[1.6]">
                  {/* Left Column (Original) */}
                  <div className="w-1/2 border-r border-white/10 relative bg-[#ff0000]/10 flex selection:bg-[#ff0000]/20">
                     <div className="bg-black/20 w-12 border-r border-white/5 flex flex-col items-end pr-3 pt-4 text-[11px] text-outline/40 select-none">
                        {Array.from({ length: maxLines }, (_, i) => <span key={i}>{i + 1}</span>)}
                     </div>
                     <div className="p-4 pt-3 flex flex-col w-full text-[#d4d4d4] overflow-x-auto">
                        <textarea
                          className="w-full h-full bg-transparent border-none resize-none font-mono text-[13px] text-[#d4d4d4] outline-none leading-[1.6]"
                          value={originalSQL}
                          onChange={(e) => setOriginalSQL(e.target.value)}
                          spellCheck={false}
                        />
                     </div>
                  </div>

                  {/* Right Column (New) */}
                  <div className="w-1/2 relative bg-[#4edea3]/10 flex selection:bg-[#4edea3]/20">
                     <div className="bg-black/20 w-12 border-r border-white/5 flex flex-col items-end pr-3 pt-4 text-[11px] text-outline/40 select-none">
                        {Array.from({ length: maxLines }, (_, i) => <span key={i}>{i + 1}</span>)}
                     </div>
                     <div className="p-4 pt-3 flex flex-col w-full text-[#d4d4d4] overflow-x-auto">
                        {isGenerating ? (
                          <div className="flex items-center gap-2 text-on-surface-variant">
                            <span className="material-symbols-outlined text-[16px] animate-spin">sync</span>
                            Generating SQL...
                          </div>
                        ) : generatedSQL ? (
                          <pre className="font-mono text-[13px] whitespace-pre-wrap leading-[1.6] m-0">{generatedSQL}</pre>
                        ) : (
                          <div className="text-on-surface-variant/50 font-code-sm italic">
                            Enter a natural language description above and click Generate.
                          </div>
                        )}
                     </div>
                  </div>
              </div>
           </div>
        </div>
      </div>

      {/* Global Toast */}
      {toastMessage && (
        <div className="absolute bottom-6 left-1/2 -translate-x-1/2 bg-surface text-on-surface border border-white/10 shadow-[0_4px_24px_rgba(0,0,0,0.5)] rounded-full px-6 py-2 font-code-sm text-code-sm flex items-center gap-2 animate-in slide-in-from-bottom-4 fade-in duration-300 z-[100]">
          <span className="material-symbols-outlined text-[16px] text-primary">info</span>
          {toastMessage}
        </div>
      )}
    </div>
  );
}
