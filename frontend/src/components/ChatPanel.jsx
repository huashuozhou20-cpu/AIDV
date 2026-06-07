import { useState, useRef, useEffect } from 'react';
import { chatQuery, fetchSchemaTree } from '../api';

export default function ChatPanel({ isOpen, onToggle }) {
  const [messages, setMessages] = useState([]);
  const [input, setInput] = useState('');
  const [loading, setLoading] = useState(false);
  const [tables, setTables] = useState([]);
  const scrollRef = useRef(null);

  useEffect(() => {
    fetchSchemaTree().then(t => setTables(t.tables || [])).catch(() => {});
  }, []);

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [messages]);

  const handleSend = async () => {
    const text = input.trim();
    if (!text || loading) return;
    setInput('');
    setLoading(true);

    const userMsg = { role: 'user', text, time: new Date().toLocaleTimeString() };
    setMessages(prev => [...prev, userMsg]);

    try {
      const result = await chatQuery(text);
      const aiMsg = {
        role: 'ai',
        time: new Date().toLocaleTimeString(),
        explanation: result.explanation || null,
        sql: result.generated_sql || null,
        status: result.status,
        columns: result.columns || [],
        data: result.data || [],
        elapsed_ms: result.execution_time_ms || 0,
        error: result.status === 'error' ? (result.message || 'Unknown error') : null,
      };
      setMessages(prev => [...prev, aiMsg]);
    } catch (err) {
      setMessages(prev => [...prev, {
        role: 'ai',
        time: new Date().toLocaleTimeString(),
        status: 'error',
        error: err.message || 'Request failed',
      }]);
    } finally {
      setLoading(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="w-[380px] shrink-0 glass-panel border border-white/10 rounded-lg flex flex-col overflow-hidden h-full">
      {/* Header */}
      <div className="px-4 h-10 border-b border-outline-variant/30 flex items-center justify-between shrink-0 bg-surface-container/50">
        <div className="flex items-center gap-2">
          <span className="material-symbols-outlined text-[18px] text-primary icon-fill ai-glow">psychology</span>
          <span className="font-code-md text-code-md text-on-surface font-bold">AI Assistant</span>
        </div>
        <button onClick={onToggle} className="text-on-surface-variant hover:text-on-surface transition-colors">
          <span className="material-symbols-outlined text-[18px]">close</span>
        </button>
      </div>

      {/* Messages */}
      <div ref={scrollRef} className="flex-1 overflow-y-auto p-3 space-y-3 font-code-sm text-[11px]">
        {messages.length === 0 && (
          <div className="flex flex-col items-center justify-center h-full text-on-surface-variant/50 gap-2 px-4 text-center">
            <span className="material-symbols-outlined text-[32px]">psychology</span>
            <span className="text-on-surface-variant font-bold">AI 数据助手</span>
            {tables.length > 0 ? (
              <div className="text-[10px] text-outline">
                <div className="mb-2">当前数据库有 {tables.length} 张表：</div>
                {tables.map(t => (
                  <div key={t.name} className="mb-1">
                    <span className="text-secondary font-bold">{t.name}</span>
                    <span className="text-outline/70">（{t.columns.map(c => c.name).join('、')}）</span>
                  </div>
                ))}
                <div className="mt-3 text-outline/50">
                  <div>试试问我：</div>
                  <div className="text-primary/70 mt-1">"查询所有{tables[0]?.name || '数据'}"</div>
                  {tables[0]?.columns.length > 1 && (
                    <div className="text-primary/70">"{tables[0]?.name}里有多少条记录"</div>
                  )}
                </div>
              </div>
            ) : (
              <div className="text-[10px]">
                <div>数据库里还没有表。</div>
                <div className="mt-1">在左侧 SQL 编辑器中执行 CREATE TABLE 创建表。</div>
              </div>
            )}
          </div>
        )}

        {messages.map((msg, i) => (
          <div key={i} className={`flex ${msg.role === 'user' ? 'justify-end' : 'justify-start'}`}>
            <div className={`max-w-[90%] rounded-lg px-3 py-2 ${
              msg.role === 'user'
                ? 'bg-primary/20 text-on-surface border border-primary/30'
                : 'bg-surface-container border border-outline-variant/30'
            }`}>
              <div className="flex items-center gap-2 mb-1">
                <span className="text-[9px] text-outline">{msg.time}</span>
                <span className="text-[9px] font-bold text-primary">
                  {msg.role === 'user' ? 'You' : 'AI'}
                </span>
              </div>

              {msg.role === 'user' ? (
                <div className="text-on-surface">{msg.text}</div>
              ) : msg.error ? (
                <div className="text-error">{msg.error}</div>
              ) : (
                <div>
                  {msg.explanation && (
                    <div className="text-on-surface-variant mb-2 leading-relaxed">{msg.explanation}</div>
                  )}
                  {msg.sql && (
                    <div className="bg-black/30 rounded px-2 py-1 mb-2 text-[#b5cea8] font-mono text-[10px] break-all">
                      {msg.sql}
                    </div>
                  )}
                  <div className="flex items-center gap-2 text-[10px]">
                    <span className={`font-bold ${msg.status === 'success' ? 'text-secondary' : 'text-error'}`}>
                      {msg.status === 'success' ? '✓' : '✗'}
                    </span>
                    <span className="text-outline">{msg.elapsed_ms}ms</span>
                    {msg.status === 'success' && <span className="text-outline">{msg.data.length} rows</span>}
                  </div>
                  {msg.status === 'success' && msg.data.length > 0 && (
                    <div className="mt-1.5 max-h-[120px] overflow-auto bg-black/20 rounded">
                      <table className="w-full text-[10px] border-collapse">
                        <thead>
                          <tr className="text-outline border-b border-outline-variant/20">
                            {msg.columns.map((c, ci) => (
                              <th key={ci} className="px-2 py-1 text-left font-medium">{c}</th>
                            ))}
                          </tr>
                        </thead>
                        <tbody>
                          {msg.data.slice(0, 10).map((row, ri) => (
                            <tr key={ri} className="border-b border-outline-variant/10 text-on-surface-variant">
                              {row.map((v, ci) => (
                                <td key={ci} className="px-2 py-0.5">{v}</td>
                              ))}
                            </tr>
                          ))}
                        </tbody>
                      </table>
                      {msg.data.length > 10 && (
                        <div className="text-center text-[9px] text-outline py-1">+{msg.data.length - 10} more rows</div>
                      )}
                    </div>
                  )}
                </div>
              )}
            </div>
          </div>
        ))}

        {loading && (
          <div className="flex justify-start">
            <div className="bg-surface-container border border-outline-variant/30 rounded-lg px-3 py-2">
              <div className="flex items-center gap-2 text-on-surface-variant">
                <span className="material-symbols-outlined text-[14px] animate-spin">sync</span>
                <span className="text-[10px]">Thinking...</span>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* Input */}
      <div className="p-3 border-t border-outline-variant/20 shrink-0">
        <div className="flex gap-2">
          <input
            type="text"
            placeholder="Ask about your data..."
            className="flex-1 bg-black/30 border border-outline-variant/30 rounded-lg px-3 py-1.5 font-code-sm text-[12px] text-on-surface focus:outline-none focus:border-primary transition-colors placeholder:text-on-surface-variant/50"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={(e) => { if (e.key === 'Enter') handleSend(); }}
            disabled={loading}
          />
          <button
            onClick={handleSend}
            disabled={loading || !input.trim()}
            className="bg-primary/20 text-primary border border-primary/30 rounded-lg px-3 py-1.5 hover:bg-primary/30 transition-colors disabled:opacity-30 flex items-center"
          >
            <span className="material-symbols-outlined text-[18px]">send</span>
          </button>
        </div>
      </div>
    </div>
  );
}
