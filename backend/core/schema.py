"""Schema discovery — auto-fetch table definitions from RMDB."""
import json
import threading
from . import database as db

_schema_cache: str | None = None
_schema_lock = threading.Lock()

DEFAULT_SCHEMA = """
-- 数据库表结构元数据 (供 AI 生成 SQL 时参考)
-- (启动后首次查询时将自动从数据库获取真实 schema)

CREATE TABLE students (
    id          INT,
    name        CHAR(100),
    age         INT,
    gender      CHAR(10),
    class_id    INT
);

CREATE TABLE classes (
    id          INT,
    class_name  CHAR(100),
    teacher     CHAR(100),
    room        CHAR(50)
);

CREATE TABLE courses (
    id          INT,
    course_name CHAR(100),
    credit      INT,
    teacher     CHAR(100)
);

CREATE TABLE scores (
    id          INT,
    student_id  INT,
    course_id   INT,
    score       FLOAT,
    exam_date   DATE
);
"""


def build_system_prompt(force_refresh: bool = False) -> str:
    """Build the AI system prompt with current schema context."""
    if force_refresh:
        global _schema_cache
        with _schema_lock:
            _schema_cache = None  # force re-fetch
    schema = get_table_schema()
    return f"""你是一个 SQL 专家。以下是当前数据库的唯一表结构，你只能使用这些表和列。

重要：用户说的是中文，请根据语义匹配最相关的表。例如"用户"对应 users，"课程"对应 courses。

规则：
1. 只返回纯 SQL 语句，不要包含任何解释、注释或 markdown 格式。
2. 只生成 SELECT 查询。
3. 只能使用上面列出的表和列名，绝对不允许编造。
4. 无法转换时返回：ERROR: 无法理解

{schema}
"""


def get_table_schema() -> str:
    """Return cached schema, fetching from RMDB on first call."""
    global _schema_cache
    with _schema_lock:
        if _schema_cache is not None:
            return _schema_cache
    schema = _fetch_schema_from_db()
    with _schema_lock:
        _schema_cache = schema
    return schema


def _fetch_schema_from_db() -> str:
    """Fetch SHOW TABLES + DESC <table> from RMDB to build schema DDL."""
    try:
        tables_raw = db.db_query_raw("SHOW TABLES")
        if not tables_raw:
            return DEFAULT_SCHEMA

        table_names: list[str] = []
        if tables_raw.startswith("{"):
            try:
                j = json.loads(tables_raw)
                if "result" in j:
                    _, rows = db._parse_rmdb_ascii_table(str(j["result"]))
                    table_names = [r[0] for r in rows if r]
                elif "columns" in j and "data" in j:
                    for row in j["data"]:
                        if row:
                            table_names.append(str(row[0]))
            except Exception:
                pass

        # Filter out scenario tables (scn_*) and test junk
        table_names = [n for n in table_names if not n.startswith("scn_") and n not in {"a1","a2","a3","a4","a5","b","t1","t2","t3","t4","test1","test2","test3","upd_test","ai_test","test_tbl"}]
        if not table_names:
            for line in tables_raw.split("\n"):
                line = line.strip()
                if line and not line.startswith("+") and not line.startswith("|"):
                    if "table" not in line.lower():
                        table_names.append(line.split("|")[0].strip())

        if not table_names:
            return DEFAULT_SCHEMA

        lines = ["-- 数据库表结构元数据 (自动获取)\n"]
        for tname in table_names:
            tname = tname.split("|")[0].strip()
            if not tname:
                continue
            desc_raw = db.db_query_raw(f"DESC {tname}")
            cols: list[tuple[str, str]] = []
            if desc_raw:
                if desc_raw.startswith("{"):
                    try:
                        j = json.loads(desc_raw)
                        if "result" in j:
                            _, rows = db._parse_rmdb_ascii_table(str(j["result"]))
                            for row in rows:
                                if len(row) >= 2 and row[0] and row[0] != "Field":
                                    cols.append((row[0], row[1]))
                    except Exception:
                        pass
                else:
                    for row in desc_raw.strip().split("\n"):
                        row = row.strip()
                        if not row or row.startswith("+") or "Field" in row:
                            continue
                        parts = [p.strip() for p in row.split("|")]
                        if len(parts) >= 2:
                            cols.append((parts[0], parts[1]))

            if cols:
                lines.append(f"CREATE TABLE {tname} (")
                for col_name, col_type in cols:
                    lines.append(f"    {col_name} {col_type},")
                lines[-1] = lines[-1].rstrip(",")
                lines.append(");\n")

        result = "\n".join(lines)
        if len(result.strip()) > 10:
            return result
    except Exception:
        pass
    return DEFAULT_SCHEMA


# ── Exercise table initialization ─────────────────────────────────────────

EXERCISE_TABLES = {
    "emp": """CREATE TABLE emp (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name CHAR(20) NOT NULL,
    dept_id INT,
    salary FLOAT,
    hire_date CHAR(20)
)""",
    "dept": """CREATE TABLE dept (
    id INT PRIMARY KEY AUTO_INCREMENT,
    dept_name CHAR(30) NOT NULL,
    location CHAR(30)
)""",
    "students": """CREATE TABLE students (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name CHAR(20) NOT NULL,
    age INT,
    gender CHAR(10),
    class_id INT
)""",
    "classes": """CREATE TABLE classes (
    id INT PRIMARY KEY AUTO_INCREMENT,
    class_name CHAR(30) NOT NULL,
    teacher CHAR(20)
)""",
    "courses": """CREATE TABLE courses (
    id INT PRIMARY KEY AUTO_INCREMENT,
    course_name CHAR(30) NOT NULL,
    credit INT,
    teacher CHAR(20)
)""",
    "scores": """CREATE TABLE scores (
    id INT PRIMARY KEY AUTO_INCREMENT,
    student_id INT,
    course_id INT,
    score FLOAT,
    exam_date CHAR(20)
)""",
}

EXERCISE_DATA = {
    "dept": [
        ("技术部", "北京"),
        ("市场部", "上海"),
        ("人事部", "广州"),
    ],
    "emp": [
        ("张三", 1, 15000, "2022-03-15"),
        ("李四", 1, 18000, "2021-07-01"),
        ("王五", 2, 12000, "2023-01-10"),
        ("赵六", 2, 13000, "2022-09-20"),
        ("孙七", 3, 11000, "2023-06-01"),
        ("周八", 1, 16000, "2020-11-15"),
    ],
    "classes": [
        ("一班", "王老师"),
        ("二班", "李老师"),
        ("三班", "张老师"),
    ],
    "students": [
        ("小明", 18, "男", 1),
        ("小红", 17, "女", 1),
        ("小刚", 19, "男", 2),
        ("小丽", 18, "女", 2),
        ("小华", 17, "男", 3),
    ],
    "courses": [
        ("数学", 4, "陈老师"),
        ("语文", 3, "刘老师"),
        ("英语", 4, "吴老师"),
        ("物理", 3, "赵老师"),
    ],
    "scores": [
        (1, 1, 85.5, "2025-01-15"),
        (1, 2, 90.0, "2025-01-16"),
        (2, 1, 78.0, "2025-01-15"),
        (2, 3, 92.5, "2025-01-17"),
        (3, 1, 88.0, "2025-01-15"),
        (4, 2, 95.0, "2025-01-16"),
        (5, 4, 72.0, "2025-01-18"),
    ],
}


def init_exercise_tables() -> int:
    """Create exercise tables and insert sample data if they don't exist.
    Returns the number of tables created."""
    import json
    from . import database as db
    created = 0
    # Batch-create tables in dependency order
    table_order = ["dept", "emp", "classes", "students", "courses", "scores"]
    for tname in table_order:
        sql = EXERCISE_TABLES.get(tname)
        if not sql:
            continue
        # Check if table already exists
        raw = db.db_query_raw("SHOW TABLES")
        exists = False
        if raw and raw.startswith("{"):
            try:
                j = json.loads(raw)
                if "result" in j:
                    _, rows = db._parse_rmdb_ascii_table(str(j["result"]))
                    exists = any(r and r[0].strip() == tname for r in rows)
            except Exception:
                pass
        if exists:
            continue
        result = db.execute_sql(sql)
        if result["status"] == "success":
            created += 1
            # Insert sample data
            data_rows = EXERCISE_DATA.get(tname, [])
            if data_rows:
                cols = ["id"]  # placeholder — RMDB INSERT without id auto-increments
                for row in data_rows:
                    vals = []
                    for v in row:
                        if isinstance(v, (int, float)):
                            vals.append(str(v))
                        else:
                            vals.append(f"'{str(v).replace(chr(39), chr(39)+chr(39))}'")
                    insert_sql = f"INSERT INTO {tname} VALUES ({', '.join(vals)})"
                    db.execute_sql(insert_sql)
    return created
