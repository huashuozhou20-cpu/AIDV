# AIDV — 数据库学习平台

交互式 SQL 学习与数据库管理平台，内置 C++ 数据库引擎 RMDB（中国人民大学数据库系统竞赛作品），支持可视化建表、AI 出题、SQL 练习等场景。

## 技术栈

| 层级 | 技术 |
|------|------|
| 前端 | React + TypeScript + Vite + Tailwind CSS |
| 后端 | Python FastAPI |
| 数据库引擎 | RMDB (C++17, Flex/Bison SQL 解析, B+树索引, WAL 日志, MVCC) |
| AI | OpenAI 兼容接口 (DeepSeek 等) |

## 快速启动

### 1. 安装依赖

```bash
# 后端
cd backend && pip install -r requirements.txt

# 前端
cd frontend && npm install
```

### 2. 配置 AI（可选）

```bash
# 编辑 backend/.env
LLM_API_KEY=your_api_key
LLM_MODEL=deepseek-chat
LLM_BASE_URL=https://api.deepseek.com/v1
```

### 3. 一键启动

```bash
./start.sh
```

浏览器访问 `http://localhost:8000`

## 功能模块

### 数据工作台 (Dashboard)
- VS Code 风格资源管理器：文件夹层级管理、新建/重命名/拖拽移动
- 文件夹级数据隔离：不同文件夹同名表互不干扰
- SQL 编辑器：含行号、AI 解释、查询历史
- 可拖拽面板：编辑器与结果面板自由调整大小
- CSV 导入/导出

### 场景管理 (Scenarios)
- AI 智能建表：描述需求 → AI 自动生成表结构
- 模板库：图书管理、学生成绩、CRM、库存、HR 等预制模板
- 手动设计器：可视化定义列名、类型、约束
- 表结构修改：模拟 ALTER TABLE（导出→删→建→回插）

### SQL 练习 (Practice)
- AI 出题：根据数据库结构自动生成练习题
- 选择题 + 手写 SQL 两种模式
- 三级难度：简单/中等/困难
- 排行榜、答题统计
- 可拖拽分割面板：题目区与答题区自由调整

### 查询分析 (Query Analyzer)
- SQL 执行计划展示
- AI 解读执行计划

### 数据管理 (Data Table)
- 场景内表数据 CRUD
- 行内编辑、CSV 导入导出

## RMDB 增强

在原版 RMDB 基础上做了以下改进：

- **中文标识符支持**：修改 Flex 词法器，支持 UTF-8 表名/列名
- **CJK 字符宽度修复**：ASCII 表格输出对齐中日韩字符
- **缓冲池修复**：DROP TABLE 后正确清理缓冲池页面，避免 "Duplicate key error"
- **前端约束顺序修复**：自动生成 `PRIMARY KEY AUTO_INCREMENT` 符合 RMDB 语法

## 项目结构

```
AIDV/
├── start.sh                 # 一键启动脚本
├── backend/                 # Python FastAPI 后端
│   ├── main.py
│   ├── api/
│   │   ├── scenarios.py     # 场景 + ALTER TABLE
│   │   ├── practice.py      # AI 出题 + 练习
│   │   ├── workspace.py     # 工作区文件夹管理
│   │   ├── data.py          # 数据 CRUD
│   │   ├── query.py         # SQL 执行 + Schema
│   │   └── auth.py          # 用户认证
│   └── core/
│       ├── database.py      # RMDB 连接层
│       ├── schema.py        # Schema 缓存
│       └── auth.py          # JWT + SQLite 用户库
├── frontend/                # React TypeScript 前端
│   └── src/
│       ├── views/
│       │   ├── DashboardView.tsx    # 数据工作台
│       │   ├── PracticeView.tsx     # SQL 练习
│       │   ├── ScenarioDesignerView.tsx
│       │   └── DataTableView.tsx
│       └── components/
│           ├── TableDesigner.tsx
│           └── TemplateLibrary.tsx
└── rmdb/                    # C++ RMDB 数据库引擎
    └── src/
        ├── parser/          # Flex/Bison SQL 解析器
        ├── storage/         # 缓冲池 + 磁盘管理
        ├── index/           # B+树索引
        ├── system/          # 表/索引管理
        └── execution/       # 查询执行器
```
