import { useT } from '../i18n';

interface ColumnDef {
  name: string;
  type: string;
  pk?: boolean;
  not_null?: boolean;
  auto_increment?: boolean;
}

interface TableDef {
  name: string;
  display: string;
  columns: ColumnDef[];
}

interface Template {
  id: string;
  name: string;
  nameEn: string;
  desc: string;
  icon: string;
  tables: TableDef[];
}

const TEMPLATES: Template[] = [
  {
    id: 'bookstore',
    name: '图书管理系统',
    nameEn: 'Bookstore',
    desc: '管理书籍、读者和借阅记录',
    icon: 'book',
    tables: [
      {
        name: 'books', display: '书籍',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'title', type: 'CHAR(50)', not_null: true },
          { name: 'author', type: 'CHAR(30)', not_null: true },
          { name: 'isbn', type: 'CHAR(20)' },
          { name: 'price', type: 'FLOAT' },
          { name: 'stock', type: 'INT', not_null: true },
        ],
      },
      {
        name: 'readers', display: '读者',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'name', type: 'CHAR(20)', not_null: true },
          { name: 'phone', type: 'CHAR(20)' },
          { name: 'email', type: 'CHAR(30)' },
          { name: 'reg_date', type: 'CHAR(20)' },
        ],
      },
      {
        name: 'borrow_records', display: '借阅记录',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'reader_id', type: 'INT', not_null: true },
          { name: 'book_id', type: 'INT', not_null: true },
          { name: 'borrow_date', type: 'CHAR(20)', not_null: true },
          { name: 'return_date', type: 'CHAR(20)' },
          { name: 'status', type: 'CHAR(10)', not_null: true },
        ],
      },
    ],
  },
  {
    id: 'school',
    name: '学生成绩管理',
    nameEn: 'School',
    desc: '管理学生、课程和考试成绩',
    icon: 'school',
    tables: [
      {
        name: 'students', display: '学生',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'name', type: 'CHAR(20)', not_null: true },
          { name: 'student_no', type: 'CHAR(20)', not_null: true },
          { name: 'age', type: 'INT' },
          { name: 'gender', type: 'CHAR(10)' },
          { name: 'class_id', type: 'INT' },
        ],
      },
      {
        name: 'courses', display: '课程',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'course_name', type: 'CHAR(30)', not_null: true },
          { name: 'credit', type: 'INT' },
          { name: 'teacher', type: 'CHAR(20)' },
        ],
      },
      {
        name: 'scores', display: '成绩',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'student_id', type: 'INT', not_null: true },
          { name: 'course_id', type: 'INT', not_null: true },
          { name: 'score', type: 'FLOAT' },
          { name: 'exam_date', type: 'CHAR(20)' },
        ],
      },
      {
        name: 'classes', display: '班级',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'class_name', type: 'CHAR(30)', not_null: true },
          { name: 'teacher', type: 'CHAR(20)' },
        ],
      },
    ],
  },
  {
    id: 'crm',
    name: '客户关系管理',
    nameEn: 'CRM',
    desc: '管理客户、联系人和销售机会',
    icon: 'business_center',
    tables: [
      {
        name: 'customers', display: '客户',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'company', type: 'CHAR(50)', not_null: true },
          { name: 'contact', type: 'CHAR(20)', not_null: true },
          { name: 'phone', type: 'CHAR(20)' },
          { name: 'email', type: 'CHAR(30)' },
          { name: 'level', type: 'CHAR(10)' },
        ],
      },
      {
        name: 'deals', display: '交易',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'customer_id', type: 'INT', not_null: true },
          { name: 'amount', type: 'FLOAT', not_null: true },
          { name: 'stage', type: 'CHAR(20)', not_null: true },
          { name: 'close_date', type: 'CHAR(20)' },
        ],
      },
    ],
  },
  {
    id: 'inventory',
    name: '库存管理系统',
    nameEn: 'Inventory',
    desc: '管理产品、仓库和库存流动',
    icon: 'inventory',
    tables: [
      {
        name: 'products', display: '产品',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'name', type: 'CHAR(40)', not_null: true },
          { name: 'sku', type: 'CHAR(20)', not_null: true },
          { name: 'price', type: 'FLOAT' },
          { name: 'qty', type: 'INT', not_null: true },
          { name: 'warehouse_id', type: 'INT' },
        ],
      },
      {
        name: 'warehouses', display: '仓库',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'name', type: 'CHAR(30)', not_null: true },
          { name: 'location', type: 'CHAR(50)' },
        ],
      },
      {
        name: 'movements', display: '出入库',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'product_id', type: 'INT', not_null: true },
          { name: 'qty_change', type: 'INT', not_null: true },
          { name: 'type', type: 'CHAR(10)', not_null: true },
          { name: 'move_date', type: 'CHAR(20)', not_null: true },
        ],
      },
    ],
  },
  {
    id: 'hr',
    name: '人力资源',
    nameEn: 'HR',
    desc: '管理员工、部门和薪资',
    icon: 'group',
    tables: [
      {
        name: 'employees', display: '员工',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'name', type: 'CHAR(20)', not_null: true },
          { name: 'dept_id', type: 'INT' },
          { name: 'position', type: 'CHAR(30)' },
          { name: 'hire_date', type: 'CHAR(20)' },
          { name: 'salary', type: 'FLOAT' },
        ],
      },
      {
        name: 'departments', display: '部门',
        columns: [
          { name: 'id', type: 'INT', pk: true, auto_increment: true },
          { name: 'dept_name', type: 'CHAR(30)', not_null: true },
          { name: 'manager', type: 'CHAR(20)' },
        ],
      },
    ],
  },
];

export default function TemplateLibrary({ onSelect }: { onSelect: (template: Template) => void }) {
  const { t } = useT();

  return (
    <div className="flex-1 overflow-y-auto">
      <h2 className="font-headline-sm text-headline-sm text-on-surface font-bold mb-4 flex items-center gap-2">
        <span className="material-symbols-outlined text-[24px] text-primary">dashboard</span>
        {t('template.title')}
      </h2>
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {TEMPLATES.map(tmpl => (
          <div key={tmpl.id}
            className="glass-panel border border-white/5 rounded-xl p-4 hover:border-primary/30 hover:bg-white/5 transition-all cursor-pointer group"
            onClick={() => onSelect(tmpl)}>
            <div className="flex items-start gap-3 mb-3">
              <span className="material-symbols-outlined text-[32px] text-primary shrink-0">{tmpl.icon}</span>
              <div className="min-w-0">
                <h3 className="font-code-md text-code-md text-on-surface font-bold group-hover:text-primary transition-colors">{tmpl.name}</h3>
                <p className="text-[10px] text-outline">{tmpl.nameEn}</p>
              </div>
            </div>
            <p className="font-code-sm text-[11px] text-on-surface-variant mb-3">{tmpl.desc}</p>
            <div className="flex flex-wrap gap-1.5 mb-3">
              {tmpl.tables.map(tbl => (
                <span key={tbl.name} className="bg-black/20 border border-outline-variant/20 rounded px-2 py-0.5 text-[10px] text-on-surface-variant font-code-sm">
                  {tbl.display} ({tbl.columns.length})
                </span>
              ))}
            </div>
            <div className="flex items-center gap-1 text-primary font-label-caps text-label-caps opacity-0 group-hover:opacity-100 transition-opacity">
              <span className="material-symbols-outlined text-[14px]">arrow_forward</span>
              {t('template.use')}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

export type { Template, TableDef, ColumnDef };
