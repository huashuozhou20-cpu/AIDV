import { useState } from 'react';
import type { ColumnInfo } from '../api';
import { useT } from '../i18n';

interface Props {
  row: string[];
  columns: ColumnInfo[];
  tableName: string;
  onSave: (data: Record<string, string>) => void;
  onClose: () => void;
}

export default function RowEditDrawer({ row, columns, tableName, onSave, onClose }: Props) {
  const { t } = useT();
  const [formData, setFormData] = useState<Record<string, string>>(() => {
    const data: Record<string, string> = {};
    columns.forEach((col, i) => {
      data[col.name] = row[i] || '';
    });
    return data;
  });

  const handleSave = () => {
    const cleaned: Record<string, string> = {};
    for (const [k, v] of Object.entries(formData)) {
      if (k === columns[0]?.name) continue; // skip PK
      cleaned[k] = v;
    }
    onSave(cleaned);
  };

  return (
    <div className="fixed inset-0 z-50 flex justify-end">
      <div className="absolute inset-0 bg-black/40 backdrop-blur-sm" onClick={onClose} />
      <div className="relative w-[420px] bg-surface border-l border-white/10 shadow-2xl h-full overflow-y-auto">
        <div className="sticky top-0 bg-surface border-b border-outline-variant/20 px-5 py-4 flex items-center justify-between">
          <div>
            <h3 className="font-headline-sm text-headline-sm text-on-surface font-bold">{t('data.editRecord')}</h3>
            <span className="font-code-sm text-[10px] text-outline">{tableName}</span>
          </div>
          <button onClick={onClose} className="text-on-surface-variant hover:text-on-surface p-1">
            <span className="material-symbols-outlined text-[20px]">close</span></button>
        </div>

        <div className="p-5 space-y-4">
          {columns.map((col, i) => {
            const isPK = i === 0; // first column is usually PK
            const isInt = col.type?.toUpperCase().includes('INT');
            const isFloat = col.type?.toUpperCase().includes('FLOAT');
            const isNumeric = isInt || isFloat;

            return (
              <div key={col.name}>
                <label className="block text-[11px] font-code-sm text-on-surface-variant mb-1.5">
                  {col.name}
                  {isPK && <span className="ml-1.5 text-[9px] bg-primary/20 text-primary rounded px-1 py-0.5">{t('data.pk')}</span>}
                  {col.type && <span className="ml-1.5 text-[9px] text-outline">{col.type}</span>}
                </label>
                <input
                  type={isNumeric ? 'number' : 'text'}
                  step={isFloat ? 'any' : undefined}
                  value={formData[col.name] || ''}
                  onChange={e => setFormData(prev => ({ ...prev, [col.name]: e.target.value }))}
                  disabled={isPK}
                  className={`w-full bg-black/40 border border-outline-variant/30 rounded-lg px-3 py-2 font-code-sm text-[12px] text-on-surface focus:outline-none focus:border-primary ${
                    isPK ? 'opacity-50 cursor-not-allowed' : ''}`}
                />
              </div>
            );
          })}
        </div>

        <div className="sticky bottom-0 bg-surface border-t border-outline-variant/20 px-5 py-4 flex gap-2">
          <button onClick={handleSave}
            className="flex-1 bg-primary text-black font-bold py-2.5 rounded-lg hover:bg-primary-fixed transition-colors font-code-sm">
            {t('common.save')}</button>
          <button onClick={onClose}
            className="flex-1 bg-surface-container border border-outline-variant/30 py-2.5 rounded-lg font-code-sm text-on-surface-variant hover:bg-white/10 transition-colors">
            {t('common.cancel')}</button>
        </div>
      </div>
    </div>
  );
}
