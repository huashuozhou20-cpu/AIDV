"""Query execution, EXPLAIN, and schema tree endpoints."""
import json
import re
from fastapi import APIRouter
from pydantic import BaseModel
from openai import OpenAI
from core import config
from core.database import execute_sql, db_query_raw, _parse_rmdb_ascii_table, get_query_log
from core import schema as schema_mod

router = APIRouter()


class QueryRequest(BaseModel):
    sql: str


class ExplainAIRequest(BaseModel):
    sql: str
    plan_raw: str
    simulated: bool = False


def _get_scenario_table_names() -> set:
    """Return set of all table names registered in any scenario."""
    try:
        import sqlite3, os
        db_path = os.path.join(os.path.dirname(__file__), "..", "users.db")
        conn = sqlite3.connect(db_path)
        rows = conn.execute("SELECT table_name FROM scenario_tables").fetchall()
        conn.close()
        return {r[0] for r in rows}
    except Exception:
        return set()


def _is_exercise_table(name: str, scenario_tables: set) -> bool:
    """Only show exercise/practice tables in Dashboard. Filter out scenario tables."""
    name = name.strip()
    if not name:
        return False
    # Filter out tables that belong to any scenario
    if name in scenario_tables:
        return False
    # Filter out test/junk tables
    junk = {"a1", "a2", "a3", "a4", "a5", "b", "t1", "t2", "t3", "t4",
            "test1", "test2", "test3", "upd_test", "ai_test", "test_tbl"}
    if name.lower() in junk:
        return False
    return True


@router.get("/schema/tree")
def api_schema_tree():
    """Return the database table/column tree for the Object Explorer (exercise tables only)."""
    try:
        tables_raw = db_query_raw("SHOW TABLES")
        table_names: list[str] = []
        if tables_raw.startswith("{"):
            j = json.loads(tables_raw)
            if "result" in j:
                _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                # Filter: only show exercise tables, skip scenario-managed tables
                scn_tables = _get_scenario_table_names()
                table_names = [r[0] for r in rows if r and _is_exercise_table(r[0], scn_tables)]
        if not table_names:
            return {"tables": []}

        tables = []
        for tname in table_names:
            tname = tname.strip()
            if not tname:
                continue
            cols = []
            try:
                desc_raw = db_query_raw(f"DESC {tname}")
                if desc_raw.startswith("{"):
                    j = json.loads(desc_raw)
                    if "result" in j:
                        _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                        for row in rows:
                            if len(row) >= 2 and row[0] and row[0] != "Field":
                                cols.append({"name": row[0], "type": row[1]})
            except Exception:
                pass
            tables.append({"name": tname, "columns": cols})

        return {"tables": tables}
    except Exception as e:
        return {"tables": [], "error": str(e)}


@router.post("/query")
def api_query(req: QueryRequest):
    sql = req.sql.strip()
    if not sql:
        return {"status": "error", "message": "SQL 不能为空", "columns": [], "data": [], "execution_time_ms": 0}
    return execute_sql(sql)


@router.delete("/schema/tables/{table_name}")
def api_drop_table(table_name: str):
    """Drop an exercise table. Refuses to drop scenario tables (scn_*)."""
    safe_name = table_name.strip()
    if not safe_name or not safe_name[0].isalpha():
        return {"status": "error", "message": "无效的表名"}
    if safe_name.startswith("scn_"):
        return {"status": "error", "message": "场景表请在场景管理中删除"}
    result = execute_sql(f"DROP TABLE {safe_name}")
    return result


@router.get("/history")
def api_query_history(limit: int = 50):
    return get_query_log(min(limit, 200))


@router.get("/logs")
def api_logs(limit: int = 100):
    """Return query logs formatted for the Logs viewer."""
    entries = get_query_log(min(limit, 200))
    logs = []
    for e in entries:
        level = "INFO" if e["status"] == "success" else "ERROR" if e["status"] == "error" else "WARN"
        logs.append({
            "time": e["time"],
            "level": level,
            "module": "Query",
            "message": f"{e['sql'][:120]}{'...' if len(e['sql']) > 120 else ''} ({e['elapsed_ms']}ms, {e['rows']} rows)" if level == "INFO" else f"Failed: {e['sql'][:100]} ({e['status']})",
        })
    return logs


@router.post("/explain")
def api_explain(req: QueryRequest):
    sql = req.sql.strip()
    if not sql:
        return {"status": "error", "message": "SQL 不能为空"}

    # Try RMDB's native EXPLAIN first
    try:
        explain_sql = "EXPLAIN " + sql if not sql.upper().strip().startswith("EXPLAIN") else sql
        raw = db_query_raw(explain_sql)
        if raw:
            if raw.startswith("{"):
                j = json.loads(raw)
                if "result" in j and "Query plan:" in str(j["result"]):
                    tree = _parse_rmdb_explain(str(j["result"]))
                    if tree:
                        return {"status": "success", "plan_tree": tree, "raw": str(j["result"]), "simulated": False}
    except Exception:
        pass

    # Fallback: simulated plan based on SQL structure
    tree = _simulate_explain_tree(sql)
    return {"status": "success", "plan_tree": tree, "raw": "(simulated)", "simulated": True}


# -- AI Explain endpoint -------------------------------------------------------

EXPLAIN_AI_SYSTEM_PROMPT = """你是一个数据库性能优化专家。用户会提供一条 SQL 语句和它的执行计划（EXPLAIN 输出），你需要用中文给出专业分析。

## 分析要求
1. **执行计划解读**：逐层解释执行计划中每个节点的含义（扫描方式、连接算法、排序/聚合策略等），说明数据是如何流动的。
2. **优化器决策分析**：解释为什么优化器选择了这种执行方式（例如为什么用 HashJoin 而不是 NestedLoopJoin，为什么用 SeqScan 而不是 IndexScan）。
3. **性能评估**：评估该查询的性能特征——预估成本、行数、潜在瓶颈。
4. **优化建议**：如果执行计划是模拟的（simulated），明确指出并给出通用的优化方向。如果不是模拟的，给出具体的索引建议、查询改写建议、或表结构调整建议。

## 输出格式
用 Markdown 组织你的回复，包含以下小节：
### 执行计划解读
### 优化器决策分析
### 性能评估
### 优化建议

每小节 2-4 句话，保持简洁专业。如果执行计划是模拟的，请在开头说明。"""


def _build_explain_ai_prompt(schema: str, simulated: bool) -> str:
    """Build the system prompt for AI explain analysis."""
    prompt = EXPLAIN_AI_SYSTEM_PROMPT + "\n\n## 数据库 Schema\n" + schema
    if simulated:
        prompt += "\n注意：当前执行计划是模拟的，并非真实 RMDB EXPLAIN 产出，以下分析仅供参考。"
    return prompt


@router.post("/explain/ai")
def api_explain_ai(req: ExplainAIRequest):
    """Generate AI-powered human-readable explanation of an execution plan."""
    if not req.sql.strip():
        return {"status": "error", "message": "SQL 不能为空"}
    if not config.LLM_API_KEY:
        return {"status": "not_configured", "message": "未配置 LLM_API_KEY 环境变量"}

    schema = schema_mod.get_table_schema()
    system_prompt = _build_explain_ai_prompt(schema, req.simulated)
    user_message = f"SQL 语句：\n```sql\n{req.sql}\n```\n\n执行计划：\n```\n{req.plan_raw}\n```"

    client = OpenAI(base_url=config.LLM_BASE_URL, api_key=config.LLM_API_KEY)
    try:
        resp = client.chat.completions.create(
            model=config.LLM_MODEL,
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_message},
            ],
            temperature=0.3,
        )
        explanation = resp.choices[0].message.content.strip()
        return {"status": "success", "explanation": explanation}
    except Exception as e:
        return {"status": "error", "message": f"AI 调用失败: {str(e)}"}


# -- EXPLAIN helpers -----------------------------------------------------------

def _parse_rmdb_explain(raw: str) -> dict | None:
    """Parse RMDB's indentation-based plan text into a nested tree."""
    lines = raw.split("\n")
    plan_lines = []
    found_start = False
    for line in lines:
        if "Query plan:" in line:
            found_start = True
            continue
        if found_start and line.strip():
            plan_lines.append(line)
    if not plan_lines:
        return None

    def get_indent(line: str) -> int:
        return len(line) - len(line.lstrip())

    _PLAN_TYPES = {"projection", "seqscan", "indexscan", "hashjoin", "nestedloopjoin",
                    "sortmergejoin", "insert", "delete", "update", "sort", "aggregation",
                    "distinct", "limit", "root"}

    def is_detail_line(line: str) -> bool:
        """Return True if this is a key:value detail line, not a plan node."""
        stripped = line.strip().lower()
        # If the line contains a known plan type name, it's a plan node
        for pt in _PLAN_TYPES:
            if pt in stripped:
                return False
        # Lines like "cols: 3" or "type: INNER" are detail lines
        return bool(re.match(r'^\s*\w+\s*:', line))

    def parse_node(idx: int, depth: int) -> tuple[dict | None, int]:
        if idx >= len(plan_lines):
            return None, idx
        line = plan_lines[idx]
        indent = get_indent(line)
        if indent < depth:
            return None, idx
        # Skip detail lines at this level — they belong to the parent
        if is_detail_line(line):
            return None, idx

        text = line.strip()
        label_part = text.split("  ")[0] if "  " in text else text
        detail_str = text[len(label_part):].strip()

        details = {}
        for kv in detail_str.split("  "):
            kv = kv.strip()
            if ":" in kv:
                k, v = kv.split(":", 1)
                details[k.strip()] = v.strip()

        # Gather detail lines immediately following this node at the next indent
        next_idx = idx + 1
        while next_idx < len(plan_lines):
            next_line = plan_lines[next_idx]
            if not next_line.strip():
                next_idx += 1
                continue
            next_indent = get_indent(next_line)
            if next_indent <= indent:
                break
            if is_detail_line(next_line) and next_indent == indent + 2:
                text2 = next_line.strip()
                if ":" in text2:
                    k, v = text2.split(":", 1)
                    details[k.strip()] = v.strip()
                next_idx += 1
            else:
                break

        node = {
            "id": f"n{idx}",
            "label": label_part.rstrip(":"),
            "type": label_part.rstrip(":").lower().replace(" ", "_"),
            "cost": float(details.get("conds", details.get("cols", 0))),
            "rows": int(details.get("rows", details.get("cols", 100))),
            "children": [],
        }
        # Parse children at deeper indent
        while next_idx < len(plan_lines):
            next_line = plan_lines[next_idx]
            if not next_line.strip():
                next_idx += 1
                continue
            next_indent = get_indent(next_line)
            if next_indent <= indent:
                break
            child, next_idx = parse_node(next_idx, indent + 2)
            if child:
                node["children"].append(child)
        return node, next_idx

    root, _ = parse_node(0, 0)
    return root


def _get_real_row_count(table_name: str) -> int:
    """Get actual row count from RMDB for a table. Returns -1 on failure.

    Uses db_query_raw directly to avoid re-entering the RMDB lock
    (this is called from within an EXPLAIN which already holds the lock).
    """
    try:
        raw = db_query_raw(f"SELECT count(*) FROM {table_name}")
        if raw and raw.startswith("{"):
            j = json.loads(raw)
            if "result" in j:
                _, rows_data = _parse_rmdb_ascii_table(str(j["result"]))
                if rows_data and rows_data[0]:
                    return int(rows_data[0][0])
    except Exception:
        pass
    return -1


def _extract_table_names(sql: str) -> list[str]:
    """Extract table names from a SQL SELECT statement."""
    upper = sql.upper()
    tables = []
    # Match FROM clause: FROM table1, table2 JOIN table3 ON ...
    from_idx = upper.find("FROM ")
    if from_idx < 0:
        return ["unknown"]
    from_part = sql[from_idx + 5:]
    # Remove WHERE / GROUP BY / ORDER BY / LIMIT etc.
    for kw in [" WHERE ", " GROUP ", " ORDER ", " LIMIT ", " HAVING "]:
        idx = from_part.upper().find(kw)
        if idx >= 0:
            from_part = from_part[:idx]
    # Split by JOIN keywords
    parts = re.split(r'\b(?:JOIN|INNER\s+JOIN|LEFT\s+JOIN|RIGHT\s+JOIN|CROSS\s+JOIN|ON|,)\b', from_part, flags=re.IGNORECASE)
    for p in parts:
        p = p.strip()
        if p and not p.upper().startswith("JOIN"):
            # Take the first word as table name
            word = p.split()[0] if p.split() else ""
            word = word.strip('`"\'')
            if word and not word.upper() in ("ON", "AS", "AND", "OR", "SET", "INTO", "VALUES"):
                tables.append(word)
    return tables if tables else ["unknown"]


def _simulate_explain_tree(sql: str) -> dict:
    """Build a simulated query plan based on SQL structure analysis. No DB calls."""
    upper = sql.upper()
    has_join = "JOIN" in upper
    has_where = "WHERE" in upper or ">" in upper or "<" in upper or "=" in upper
    has_aggregate = any(kw in upper for kw in ["COUNT", "SUM", "AVG", "MAX", "MIN", "GROUP BY"])
    has_sort = "ORDER BY" in upper

    table_names = _extract_table_names(sql)
    primary_table = table_names[0] if table_names else "table"

    nodes = [{"id": "root", "label": "Result", "type": "result", "cost": 0, "rows": 0}]
    node_id = 1
    parent = "root"

    if has_sort:
        sid = f"n{node_id}"; node_id += 1
        nodes.append({"id": sid, "label": "Sort", "type": "sort", "cost": 28.5, "rows": 100})
        nodes.append({"from": sid, "to": parent, "label": ""})
        parent = sid

    if has_aggregate:
        aid = f"n{node_id}"; node_id += 1
        nodes.append({"id": aid, "label": "HashAggregate", "type": "aggregate", "cost": 22.1, "rows": 50})
        nodes.append({"from": aid, "to": parent, "label": ""})
        parent = aid

    if has_join:
        jid = f"n{node_id}"; node_id += 1
        nodes.append({"id": jid, "label": "HashJoin (INNER)", "type": "join", "cost": 18.3, "rows": 500})
        nodes.append({"from": jid, "to": parent, "label": ""})
        lid = f"n{node_id}"; node_id += 1
        left_table = table_names[0] if len(table_names) > 0 else "t1"
        nodes.append({"id": lid, "label": f"SeqScan {left_table}", "type": "scan", "cost": 5.2, "rows": 1000})
        nodes.append({"from": lid, "to": jid, "label": "Left"})
        rid = f"n{node_id}"; node_id += 1
        right_table = table_names[1] if len(table_names) > 1 else "t2"
        nodes.append({"id": rid, "label": f"SeqScan {right_table}", "type": "scan", "cost": 8.1, "rows": 800})
        nodes.append({"from": rid, "to": jid, "label": "Right"})
    elif has_where:
        fid = f"n{node_id}"; node_id += 1
        nodes.append({"id": fid, "label": "Filter (WHERE)", "type": "filter", "cost": 12.4, "rows": 300})
        nodes.append({"from": fid, "to": parent, "label": ""})
        sid = f"n{node_id}"; node_id += 1
        nodes.append({"id": sid, "label": f"SeqScan {primary_table}", "type": "scan", "cost": 8.1, "rows": 1000})
        nodes.append({"from": sid, "to": fid, "label": ""})
    else:
        sid = f"n{node_id}"; node_id += 1
        nodes.append({"id": sid, "label": f"SeqScan {primary_table}", "type": "scan", "cost": 8.1, "rows": 1000})
        nodes.append({"from": sid, "to": parent, "label": ""})

    children_map: dict[str, list[str]] = {}
    for n in nodes:
        if "from" in n:
            children_map.setdefault(n["to"], []).append(n["from"])

    def build_tree(node_id: str) -> dict:
        node = next((n for n in nodes if n.get("id") == node_id and "from" not in n), None)
        if not node:
            node = {"id": node_id, "label": node_id, "type": "unknown", "cost": 0, "rows": 0}
        node["children"] = [build_tree(c) for c in children_map.get(node_id, [])]
        for c in node["children"]:
            c.pop("children", None) if not c.get("children") else None
        return node

    return build_tree("root")
