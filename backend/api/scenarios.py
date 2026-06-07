"""Application scenario management — create, list, delete scenarios and their tables."""
import json
import os
import re
import sqlite3
from fastapi import APIRouter, Header
from pydantic import BaseModel
from openai import OpenAI
from core import config
from core.auth import verify_token
from core.database import execute_sql, db_query_raw, _parse_rmdb_ascii_table

router = APIRouter()

_DB_PATH = os.path.join(os.path.dirname(__file__), "..", "users.db")


def _get_conn() -> sqlite3.Connection:
    conn = sqlite3.connect(_DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def _get_current_user(authorization: str = Header("")) -> dict | None:
    if not authorization.startswith("Bearer "):
        return None
    return verify_token(authorization[7:])


def _slugify(name: str, custom_slug: str = "") -> str:
    """Generate a readable slug. Uses custom_slug if provided, otherwise timestamp."""
    if custom_slug and re.match(r'^[a-z][a-z0-9_]*$', custom_slug.lower()):
        return custom_slug.lower()[:20]
    # Fallback: timestamp-based short slug
    import time
    ts = str(int(time.time()))[-4:]
    return f"app_{ts}"


# ── Scenario CRUD ──────────────────────────────────────────────────────────

class CreateScenarioRequest(BaseModel):
    name: str
    description: str = ""
    icon: str = "database"
    slug: str = ""  # optional custom English identifier


@router.get("/scenarios")
def api_list_scenarios(authorization: str = Header("")):
    """List all scenarios for the current user."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    rows = conn.execute(
        "SELECT id, name, slug, description, icon, is_active, created_at FROM scenarios WHERE user_id = ? ORDER BY created_at DESC",
        (user["user_id"],),
    ).fetchall()
    conn.close()
    return {"status": "success", "scenarios": [dict(r) for r in rows]}


@router.post("/scenarios")
def api_create_scenario(req: CreateScenarioRequest, authorization: str = Header("")):
    """Create a new application scenario."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    name = req.name.strip()
    if not name or len(name) < 2:
        return {"status": "error", "message": "场景名称至少2个字符"}

    slug = _slugify(name, req.slug.strip().lower())
    conn = _get_conn()
    try:
        cursor = conn.execute(
            "INSERT INTO scenarios (user_id, name, slug, description, icon) VALUES (?, ?, ?, ?, ?)",
            (user["user_id"], name, slug, req.description.strip(), req.icon),
        )
        conn.commit()
        sid = cursor.lastrowid
        return {"status": "success", "scenario": {"id": sid, "name": name, "slug": slug, "description": req.description.strip(), "icon": req.icon}}
    except sqlite3.IntegrityError:
        # slug conflict — add a random suffix
        import random, string
        slug = slug[:20] + "_" + "".join(random.choices(string.ascii_lowercase, k=3))
        try:
            cursor = conn.execute(
                "INSERT INTO scenarios (user_id, name, slug, description, icon) VALUES (?, ?, ?, ?, ?)",
                (user["user_id"], name, slug, req.description.strip(), req.icon),
            )
            conn.commit()
            sid = cursor.lastrowid
            return {"status": "success", "scenario": {"id": sid, "name": name, "slug": slug, "description": req.description.strip(), "icon": req.icon}}
        except Exception:
            return {"status": "error", "message": "创建场景失败，请换个名称重试"}
    finally:
        conn.close()


@router.get("/scenarios/{scenario_id}")
def api_get_scenario(scenario_id: int, authorization: str = Header("")):
    """Get scenario details with all its tables and their column metadata."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    scenario = conn.execute(
        "SELECT * FROM scenarios WHERE id = ? AND user_id = ?", (scenario_id, user["user_id"])
    ).fetchone()
    if not scenario:
        conn.close()
        return {"status": "error", "message": "场景不存在"}
    tables = conn.execute(
        "SELECT * FROM scenario_tables WHERE scenario_id = ? ORDER BY created_at", (scenario_id,)
    ).fetchall()
    conn.close()

    # Enrich tables with column info from RMDB
    table_list = []
    for t in tables:
        t = dict(t)
        cols = _fetch_table_columns(t["table_name"])
        t["columns"] = cols
        t["row_count"] = _fetch_row_count(t["table_name"])
        table_list.append(t)

    return {"status": "success", "scenario": dict(scenario), "tables": table_list}


@router.delete("/scenarios/{scenario_id}")
def api_delete_scenario(scenario_id: int, authorization: str = Header("")):
    """Delete a scenario and DROP all its RMDB tables."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    scenario = conn.execute(
        "SELECT * FROM scenarios WHERE id = ? AND user_id = ?", (scenario_id, user["user_id"])
    ).fetchone()
    if not scenario:
        conn.close()
        return {"status": "error", "message": "场景不存在"}

    # Drop all RMDB tables belonging to this scenario
    tables = conn.execute(
        "SELECT table_name FROM scenario_tables WHERE scenario_id = ?", (scenario_id,)
    ).fetchall()
    for t in tables:
        try:
            db_query_raw(f"DROP TABLE {t['table_name']}")
        except Exception:
            pass  # table may already be gone

    conn.execute("DELETE FROM scenarios WHERE id = ?", (scenario_id,))
    conn.commit()
    conn.close()
    return {"status": "success", "message": "场景已删除"}


@router.post("/scenarios/{scenario_id}/activate")
def api_activate_scenario(scenario_id: int, authorization: str = Header("")):
    """Set a scenario as the active one for the current user."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    conn.execute("UPDATE scenarios SET is_active = 0 WHERE user_id = ?", (user["user_id"],))
    conn.execute("UPDATE scenarios SET is_active = 1 WHERE id = ? AND user_id = ?", (scenario_id, user["user_id"]))
    conn.commit()
    conn.close()
    return {"status": "success", "message": "已切换场景"}


# ── Table Management within a Scenario ──────────────────────────────────────

class CreateTableRequest(BaseModel):
    display_name: str
    description: str = ""
    columns: list[dict]  # [{"name": "id", "type": "INT", "pk": true, "not_null": true, "auto_increment": true}, ...]


@router.get("/scenarios/{scenario_id}/tables")
def api_list_scenario_tables(scenario_id: int, authorization: str = Header("")):
    """List all tables in a scenario with column metadata and row counts."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    scenario = conn.execute("SELECT id, slug FROM scenarios WHERE id = ? AND user_id = ?", (scenario_id, user["user_id"])).fetchone()
    if not scenario:
        conn.close()
        return {"status": "error", "message": "场景不存在"}

    tables = conn.execute("SELECT * FROM scenario_tables WHERE scenario_id = ? ORDER BY created_at", (scenario_id,)).fetchall()
    conn.close()

    result = []
    for t in tables:
        t = dict(t)
        t["columns"] = _fetch_table_columns(t["table_name"])
        t["row_count"] = _fetch_row_count(t["table_name"])
        result.append(t)
    return {"status": "success", "tables": result}


def _build_create_table_sql(table_name: str, columns: list[dict]) -> str:
    """Build a CREATE TABLE SQL statement from column definitions.

    RMDB type constraints:
    - Valid types: INT, INTEGER, FLOAT, CHAR(n), VARCHAR(n), TEXT, DATE
    - STRING is NOT a valid RMDB keyword — use CHAR(100) instead
    - Constraint order MUST be: TYPE [NOT NULL] [AUTO_INCREMENT] [PRIMARY KEY]
      or: TYPE [PRIMARY KEY] [AUTO_INCREMENT]
    - PRIMARY KEY implies NOT NULL; don't use both together
    """
    col_defs = []
    for col in columns:
        name = col["name"].strip()
        col_type = col.get("type", "CHAR(100)").upper()
        # Normalize type to RMDB-compatible keywords
        if col_type in ("STRING", "VARCHAR", "TEXT"):
            col_type = "CHAR(100)"
        elif col_type == "INTEGER":
            col_type = "INT"
        elif col_type in ("DECIMAL", "NUMERIC"):
            col_type = "FLOAT"
        elif col_type == "DATE":
            pass  # RMDB accepts DATE (maps to STRING)
        # Validate — unrecognized types default to CHAR(100)
        if not re.match(r'^(INT|FLOAT|CHAR\(\d+\)|VARCHAR\(\d+\)|TEXT|DATE)$', col_type, re.IGNORECASE):
            col_type = "CHAR(100)"

        parts = [name, col_type]

        # RMDB constraint ordering (CRITICAL):
        #   PRIMARY KEY must come BEFORE AUTO_INCREMENT: id INT PRIMARY KEY AUTO_INCREMENT
        #   NOT NULL must not be combined with PRIMARY KEY (PK implies NOT NULL)
        has_pk = col.get("pk")
        is_auto = col.get("auto_increment")

        if has_pk:
            parts.append("PRIMARY KEY")
        elif col.get("not_null"):
            parts.append("NOT NULL")
        if is_auto:
            parts.append("AUTO_INCREMENT")

        col_defs.append(" ".join(parts))

    if not col_defs:
        col_defs = ["id INT PRIMARY KEY AUTO_INCREMENT"]

    # Validate record size (RMDB limit 2048 bytes, minus 24 hidden = 2024 usable; INT=4, FLOAT=4, CHAR(n)=n*4)
    est_size = 0
    for col in columns:
        ct = col.get("type", "CHAR(100)").upper()
        if "INT" in ct:
            est_size += 4
        elif "FLOAT" in ct:
            est_size += 4
        elif ct == "DATE":
            est_size += 80  # DATE maps to CHAR(20) internally
        else:
            # CHAR(n) or STRING — extract n
            m = re.search(r'(\d+)', ct)
            est_size += int(m.group(1)) * 4 if m else 400
    if est_size > 2024:
        raise ValueError(f"记录大小预估为 {est_size} 字节，超过 RMDB 上限 (2024)。请减少 CHAR 字段的长度。")

    return f"CREATE TABLE {table_name} (\n    " + ",\n    ".join(col_defs) + "\n)"


@router.post("/scenarios/{scenario_id}/tables")
def api_create_table(scenario_id: int, req: CreateTableRequest, authorization: str = Header("")):
    """Create a new table within a scenario."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    scenario = conn.execute("SELECT id, slug FROM scenarios WHERE id = ? AND user_id = ?", (scenario_id, user["user_id"])).fetchone()
    if not scenario:
        conn.close()
        return {"status": "error", "message": "场景不存在"}

    display_name = req.display_name.strip().replace(" ", "_")
    # Allow Unicode (Chinese etc.) in display name alongside ASCII
    if not display_name or not display_name[0].isalnum() and display_name[0] != '_':
        conn.close()
        return {"status": "error", "message": "表名只能包含字母、数字、下划线或中文字符，且以字母、数字或中文开头"}
    for ch in display_name:
        if not (ch.isalnum() or ch == '_'):
            conn.close()
            return {"status": "error", "message": "表名只能包含字母、数字、下划线或中文字符"}
    # Also keep original check for pure ASCII names that they start with a letter
    if display_name[0].isascii() and not display_name[0].isalpha() and display_name[0] != '_':
        conn.close()
        return {"status": "error", "message": "表名以字母或中文开头"}

    table_name = f"{scenario['slug']}_{display_name}"
    if len(table_name) > 64:
        conn.close()
        return {"status": "error", "message": "表名过长（前缀+表名不超过64字符）"}

    # Build and execute CREATE TABLE
    sql = _build_create_table_sql(table_name, req.columns)
    try:
        result = execute_sql(sql)
        if result["status"] == "error":
            conn.close()
            return {"status": "error", "message": f"创建表失败: {result.get('message', 'Unknown error')}"}

        # Record in scenario_tables
        cursor = conn.execute(
            "INSERT INTO scenario_tables (scenario_id, table_name, display_name, description) VALUES (?, ?, ?, ?)",
            (scenario_id, table_name, req.display_name.strip(), req.description.strip()),
        )
        conn.commit()
        tid = cursor.lastrowid
        conn.close()
        return {"status": "success", "table": {"id": tid, "table_name": table_name, "display_name": req.display_name.strip()}}
    except Exception as e:
        conn.close()
        return {"status": "error", "message": f"创建表失败: {str(e)}"}


@router.delete("/scenarios/{scenario_id}/tables/{table_id}")
def api_delete_table(scenario_id: int, table_id: int, authorization: str = Header("")):
    """Delete a table from a scenario (drops the RMDB table too)."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    table = conn.execute(
        """SELECT st.* FROM scenario_tables st
           JOIN scenarios s ON st.scenario_id = s.id
           WHERE st.id = ? AND s.id = ? AND s.user_id = ?""",
        (table_id, scenario_id, user["user_id"]),
    ).fetchone()
    if not table:
        conn.close()
        return {"status": "error", "message": "表不存在"}

    try:
        db_query_raw(f"DROP TABLE {table['table_name']}")
    except Exception:
        pass
    conn.execute("DELETE FROM scenario_tables WHERE id = ?", (table_id,))
    conn.commit()
    conn.close()
    return {"status": "success", "message": "表已删除"}


# ── Table Structure Modification (Simulated ALTER TABLE) ──────────────────

class ModifyTableStructureRequest(BaseModel):
    columns: list[dict]  # [{"name": "id", "type": "INT", "pk": true, ...}]


@router.put("/scenarios/{scenario_id}/tables/{table_id}/structure")
def api_modify_table_structure(
    scenario_id: int,
    table_id: int,
    req: ModifyTableStructureRequest,
    authorization: str = Header(""),
):
    """Modify an existing table's column structure via simulated ALTER TABLE.

    Since RMDB does not natively support ALTER TABLE, this endpoint:
      1. Exports all existing data (SELECT *)
      2. Drops the old table
      3. Creates a new table with the new column set
      4. Re-inserts data for columns that still exist
    New columns get NULL values; dropped columns are discarded.

    Known limitations:
      - AUTO_INCREMENT IDs will be reassigned (educational use, ok)
      - Cross-table foreign key references may break
      - The drop-and-recreate is NOT atomic (process interruption = data loss)
    """
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}

    conn = _get_conn()
    table = conn.execute(
        """SELECT st.* FROM scenario_tables st
           JOIN scenarios s ON st.scenario_id = s.id
           WHERE st.id = ? AND s.id = ? AND s.user_id = ?""",
        (table_id, scenario_id, user["user_id"]),
    ).fetchone()
    if not table:
        conn.close()
        return {"status": "error", "message": "表不存在"}
    table = dict(table)
    table_name = table["table_name"]

    # ── Step 1: Export existing data ──
    old_columns = _fetch_table_columns(table_name)
    if not old_columns:
        conn.close()
        return {"status": "error", "message": "无法读取表结构"}

    old_col_names = [c["name"] for c in old_columns]
    exported_rows = []
    try:
        raw = db_query_raw(f"SELECT * FROM {table_name}")
        if raw and raw.startswith("{"):
            j = json.loads(raw)
            if "result" in j:
                _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                exported_rows = rows
    except Exception as e:
        conn.close()
        return {"status": "error", "message": f"导出数据失败: {str(e)}"}

    # ── Step 2: Drop old table ──
    try:
        db_query_raw(f"DROP TABLE {table_name}")
    except Exception as e:
        conn.close()
        return {"status": "error", "message": f"删除旧表失败: {str(e)}"}

    # ── Step 3: Create new table ──
    try:
        create_sql = _build_create_table_sql(table_name, req.columns)
    except ValueError as e:
        conn.close()
        return {"status": "error", "message": str(e)}

    result = execute_sql(create_sql)
    if result["status"] == "error":
        # Recovery: try to recreate with the old column definitions
        try:
            old_col_defs = []
            for c in old_columns:
                col_type = c.get("type", "CHAR(100)").upper()
                is_pk = c.get("key") == "PRI"
                old_col_defs.append({
                    "name": c["name"],
                    "type": col_type,
                    "pk": is_pk,
                    "not_null": not c.get("nullable", True),
                    "auto_increment": is_pk and ("INT" in col_type or "FLOAT" in col_type),
                })
            recovery_sql = _build_create_table_sql(table_name, old_col_defs)
            execute_sql(recovery_sql)
        except Exception:
            pass
        conn.close()
        return {"status": "error", "message": f"创建新表失败: {result.get('message', 'Unknown error')}"}

    # ── Step 4: Re-insert data for columns that still exist ──
    new_col_names = [c.get("name", "").strip() for c in req.columns]
    col_index_map = {}  # old_col_index -> new_col_index (None if dropped)
    for old_idx, old_name in enumerate(old_col_names):
        col_index_map[old_idx] = new_col_names.index(old_name) if old_name in new_col_names else None

    inserted = 0
    errors = []
    for row in exported_rows:
        insert_cols = []
        insert_vals = []
        for old_idx, val in enumerate(row):
            new_idx = col_index_map.get(old_idx)
            if new_idx is None:
                continue  # column was dropped
            col_name = new_col_names[new_idx]
            col_type = "STRING"
            for nc in req.columns:
                if nc.get("name", "").strip() == col_name:
                    col_type = nc.get("type", "CHAR(100)").upper()
                    break
            insert_cols.append(col_name)
            if val is None or str(val).strip() == "" or str(val) == "NULL":
                insert_vals.append("NULL")
            elif "INT" in col_type or "FLOAT" in col_type:
                clean_v = str(val).replace("'", "").replace('"', "")
                insert_vals.append(clean_v if clean_v else "NULL")
            else:
                safe_v = str(val).replace("'", "''")
                insert_vals.append(f"'{safe_v}'")

        if not insert_cols:
            continue
        try:
            insert_sql = f"INSERT INTO {table_name} ({', '.join(insert_cols)}) VALUES ({', '.join(insert_vals)})"
            ins_result = execute_sql(insert_sql)
            if ins_result["status"] == "success":
                inserted += 1
            else:
                errors.append(f"行 {inserted + len(errors) + 1}: {ins_result.get('message', 'Unknown')}")
        except Exception as e:
            errors.append(f"行 {inserted + len(errors) + 1}: {str(e)}")

    conn.close()
    return {
        "status": "success",
        "message": f"表结构已更新。重新插入 {inserted} 行" + (f"，{len(errors)} 行失败" if errors else ""),
        "table": {"id": table_id, "table_name": table_name, "display_name": table["display_name"]},
        "inserted": inserted,
        "errors": errors,
    }


# ── AI Schema Designer ──────────────────────────────────────────────────────

AI_DESIGN_PROMPT = """你是一个数据库设计专家。用户描述了他们的数据管理需求，请设计合理的表结构。

## 命名规则
- 所有表名必须以 {slug}_ 为前缀
- 表名和字段名使用英文小写 + 下划线（如 {slug}_books, {slug}_borrow_records）
- 每张表必须有主键 id，类型 INT，设置为 PRIMARY KEY AUTO_INCREMENT

## 可用数据类型（重要：只能用这些类型！）
- INT — 整数（年龄、数量、编号等）
- FLOAT — 小数（价格、比率等）
- CHAR(n) — 文本，n 为最大长度。注意：每条记录总大小不能超过 512 字节
  建议小的 n 值：姓名用 CHAR(20)，标题用 CHAR(50)，描述用 CHAR(80)
  不要超过 CHAR(100)，否则记录会过大
- DATE — 日期字段用 CHAR(20)

## 约束语法（严格顺序：类型 → PRIMARY KEY → AUTO_INCREMENT → NOT NULL）
- id 列写法：id INT PRIMARY KEY AUTO_INCREMENT
- 必填列写法：name CHAR(50) NOT NULL
- PRIMARY KEY 隐含 NOT NULL，不要同时使用

## 输出格式
每张表输出一个 SQL 代码块，后面用一行中文说明用途：
```sql
CREATE TABLE {slug}_xxx (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name CHAR(100) NOT NULL,
    age INT,
    price FLOAT
);
```
用途：这张表用于...

## ⚠️ 记录大小限制（重要！）
每条记录的总大小不能超过 **2000 字节**：
- INT = 4 字节，FLOAT = 4 字节
- CHAR(n) = n × 4 字节（因为 UTF-8 编码）
- 示例计算：CHAR(50) = 200 字节，CHAR(100) = 400 字节
- **一般每表 5-8 个 CHAR 字段都是安全的，CHAR(100) 也问题不大**
- 如果字段特别多（10+ CHAR），缩减到 CHAR(20)~CHAR(50)

## 设计原则
- 表名和字段名要见名知义
- **尽量精简字段**，宁少勿多。用户说"一张表"就要尽量设计在一张表内
- 文本字段优先用 CHAR(20)~CHAR(50)，**不要超过 CHAR(50)**
- 数值字段用 INT 或 FLOAT
- 日期用 CHAR(20)
"""


class AIDesignRequest(BaseModel):
    description: str
    history: list[dict] = []  # previous messages for refinement


@router.post("/scenarios/{scenario_id}/ai-design")
def api_ai_design(scenario_id: int, req: AIDesignRequest, authorization: str = Header("")):
    """AI generates table schemas based on user's natural language description."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    if not config.LLM_API_KEY:
        return {"status": "not_configured", "message": "未配置 LLM_API_KEY"}

    conn = _get_conn()
    scenario = conn.execute("SELECT id, slug, name FROM scenarios WHERE id = ? AND user_id = ?", (scenario_id, user["user_id"])).fetchone()
    if not scenario:
        conn.close()
        return {"status": "error", "message": "场景不存在"}

    # Fetch existing tables to avoid naming conflicts
    existing_tables = conn.execute(
        "SELECT table_name, display_name FROM scenario_tables WHERE scenario_id = ?", (scenario_id,)
    ).fetchall()
    conn.close()

    slug = scenario["slug"]
    system_prompt = AI_DESIGN_PROMPT.replace("{slug}", slug)

    # Add existing table info to prevent conflicts
    if existing_tables:
        existing_info = "\n## ⚠️ 已存在的表（不要重复创建！）\n以下表已经存在，**绝对不要**生成与这些表同名的 CREATE TABLE 语句。如果需要扩展功能，请设计**新的**表（不同的表名）：\n"
        for t in existing_tables:
            existing_info += f"- {t['table_name']}（{t['display_name']}）\n"
        existing_info += "\n你只需要设计**新增**的表，不要重复已存在的表。\n"
        system_prompt = system_prompt + existing_info

    messages = [{"role": "system", "content": system_prompt}]
    for h in req.history:
        messages.append({"role": h.get("role", "user"), "content": h.get("content", "")})
    messages.append({"role": "user", "content": req.description.strip()})

    client = OpenAI(base_url=config.LLM_BASE_URL, api_key=config.LLM_API_KEY)
    try:
        resp = client.chat.completions.create(
            model=config.LLM_MODEL,
            messages=messages,
            temperature=0.3,
        )
        content = resp.choices[0].message.content.strip()
        return {"status": "success", "response": content}
    except Exception as e:
        return {"status": "error", "message": f"AI 调用失败: {str(e)}"}


class AIConfirmRequest(BaseModel):
    sqls: list[str]  # list of CREATE TABLE statements to execute


@router.post("/scenarios/{scenario_id}/ai-confirm")
def api_ai_confirm(scenario_id: int, req: AIConfirmRequest, authorization: str = Header("")):
    """Execute the AI-generated CREATE TABLE statements and record them."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}

    conn = _get_conn()
    scenario = conn.execute("SELECT id, slug FROM scenarios WHERE id = ? AND user_id = ?", (scenario_id, user["user_id"])).fetchone()
    if not scenario:
        conn.close()
        return {"status": "error", "message": "场景不存在"}

    created = []
    errors = []
    for sql in req.sqls:
        sql = sql.strip()
        if not sql or not sql.upper().startswith("CREATE TABLE"):
            errors.append({"sql": sql[:80], "error": "不是 CREATE TABLE 语句"})
            continue

        # Extract table name from CREATE TABLE
        match = re.match(r'CREATE\s+TABLE\s+(\w+)', sql, re.IGNORECASE)
        if not match:
            errors.append({"sql": sql[:80], "error": "无法解析表名"})
            continue
        table_name = match.group(1)

        # Extract display name (remove prefix)
        prefix = scenario["slug"] + "_"
        display_name = table_name[len(prefix):] if table_name.startswith(prefix) else table_name

        # Execute the CREATE TABLE
        result = execute_sql(sql)
        if result["status"] == "error":
            err_msg = result.get("message", "Unknown error")
            # Translate RMDB errors to user-friendly Chinese messages
            if "Invalid record size" in err_msg or "record size" in err_msg.lower():
                # Extract byte count if present
                m = re.search(r'(\d+)', err_msg)
                size = m.group(1) if m else "?"
                err_msg = f"记录大小超限（{size} 字节）。RMDB 单条记录上限 500 字节。请减少 CHAR 字段数量或缩短长度（如 CHAR(20) 代替 CHAR(100)）"
            elif "syntax error" in err_msg.lower():
                err_msg = f"SQL 语法错误。请检查：1) 类型只支持 INT/FLOAT/CHAR(n)  2) 约束顺序为 PRIMARY KEY AUTO_INCREMENT  3) 括号是否匹配"
            # "Table already exists" is OK — it means this table was created before
            if "already exists" in err_msg.lower():
                # Check if already registered in scenario_tables
                existing = conn.execute(
                    "SELECT id FROM scenario_tables WHERE scenario_id = ? AND table_name = ?",
                    (scenario_id, table_name),
                ).fetchone()
                if existing:
                    created.append({"table_name": table_name, "display_name": display_name, "existed": True})
                    continue
                else:
                    # Table exists in RMDB but not registered — register it now
                    try:
                        conn.execute(
                            "INSERT INTO scenario_tables (scenario_id, table_name, display_name) VALUES (?, ?, ?)",
                            (scenario_id, table_name, display_name),
                        )
                        conn.commit()
                        created.append({"table_name": table_name, "display_name": display_name, "registered": True})
                        continue
                    except Exception:
                        pass
            errors.append({"table": table_name, "error": err_msg})
            continue

        try:
            conn.execute(
                "INSERT INTO scenario_tables (scenario_id, table_name, display_name) VALUES (?, ?, ?)",
                (scenario_id, table_name, display_name),
            )
            conn.commit()
            created.append({"table_name": table_name, "display_name": display_name})
        except sqlite3.IntegrityError:
            errors.append({"table": table_name, "error": "该表已在场景中注册"})

    conn.close()
    return {"status": "success", "created": created, "errors": errors}


# ── Helper Functions ────────────────────────────────────────────────────────

def _fetch_table_columns(table_name: str) -> list[dict]:
    """Fetch column metadata for a table from RMDB via DESC."""
    try:
        raw = db_query_raw(f"DESC {table_name}")
        if not raw:
            return []
        cols = []
        if raw.startswith("{"):
            j = json.loads(raw)
            if "result" in j:
                _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                for row in rows:
                    if len(row) >= 5 and row[0] and row[0] != "Field":
                        cols.append({
                            "name": row[0],
                            "type": row[1] if len(row) > 1 else "STRING",
                            "nullable": row[2] == "YES" if len(row) > 2 else True,
                            "default": row[3] if len(row) > 3 else None,
                            "key": row[4] if len(row) > 4 else "",
                        })
        return cols
    except Exception:
        return []


def _fetch_row_count(table_name: str) -> int:
    """Fetch the number of rows in a table."""
    try:
        raw = db_query_raw(f"SELECT COUNT(*) FROM {table_name}")
        if raw and raw.startswith("{"):
            j = json.loads(raw)
            if "result" in j:
                _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                if rows and rows[0]:
                    return int(rows[0][0])
    except Exception:
        pass
    return 0
