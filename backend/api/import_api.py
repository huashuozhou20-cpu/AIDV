"""SQL file import: parse and execute .sql files against RMDB."""
import re
import json
from fastapi import APIRouter, UploadFile, File
from pydantic import BaseModel
from core.database import execute_sql

router = APIRouter()


def _split_sql_statements(text: str) -> list[str]:
    """Split SQL text into individual statements, preserving multi-line statements.
    Handles -- comments, /* */ block comments, and semicolons inside quotes."""
    # Remove block comments
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # Remove line comments (but not inside strings — simple heuristic)
    lines = []
    for line in text.split('\n'):
        # Remove -- comments (not inside quotes)
        stripped = re.sub(r'--.*$', '', line)
        lines.append(stripped)
    text = '\n'.join(lines)

    # Split by semicolons, but respect quoted strings
    statements = []
    current = []
    in_single = False
    in_double = False
    for char in text:
        if char == "'" and not in_double:
            in_single = not in_single
        elif char == '"' and not in_single:
            in_double = not in_double
        elif char == ';' and not in_single and not in_double:
            stmt = ''.join(current).strip()
            if stmt:
                statements.append(stmt)
            current = []
            continue
        current.append(char)
    # Last statement (no trailing semicolon)
    stmt = ''.join(current).strip()
    if stmt:
        statements.append(stmt)
    return statements


@router.post("/import/parse")
async def api_parse_sql(file: UploadFile = File(...)):
    """Parse an uploaded .sql file and return structured results for preview."""
    content = await file.read()
    text = content.decode("utf-8-sig", errors="replace")
    if not text.strip():
        return {"status": "error", "message": "文件为空"}

    statements = _split_sql_statements(text)
    creates = []
    inserts = []
    others = []

    for stmt in statements:
        upper = stmt.strip().upper()
        if upper.startswith("CREATE TABLE"):
            # Extract table name
            m = re.match(r'CREATE\s+TABLE\s+(\w+)', stmt, re.IGNORECASE)
            table_name = m.group(1) if m else "unknown"
            # Count columns
            col_count = len(re.findall(r'\b(INT|FLOAT|CHAR|DATE|TEXT|VARCHAR)\b', stmt, re.IGNORECASE))
            creates.append({"table_name": table_name, "col_count": col_count, "sql": stmt.strip()})
        elif upper.startswith("INSERT"):
            m = re.match(r'INSERT\s+INTO\s+(\w+)', stmt, re.IGNORECASE)
            table_name = m.group(1) if m else "unknown"
            # Count value tuples
            row_count = len(re.findall(r'\([^)]+\)', stmt)) - (1 if 'VALUES' in upper else 0)
            inserts.append({"table_name": table_name, "row_count": max(1, row_count), "sql": stmt.strip()})
        elif upper.startswith(("DROP", "ALTER", "TRUNCATE", "DELETE", "UPDATE")):
            others.append({"type": "unsupported", "sql": stmt.strip()[:100]})
        elif upper.startswith(("SELECT", "SET", "SHOW", "DESC")):
            others.append({"type": "skipped", "sql": stmt.strip()[:100]})
        else:
            pass  # empty/whitespace

    return {
        "status": "success",
        "creates": creates,
        "inserts": inserts,
        "others": others,
        "filename": file.filename,
    }


class ExecuteRequest(BaseModel):
    creates: list[str] = []
    inserts: list[str] = []


@router.post("/import/execute")
def api_execute_import(req: ExecuteRequest):
    """Execute parsed CREATE TABLE and INSERT statements from an import."""
    results = {"created": 0, "inserted": 0, "errors": []}

    # Execute CREATE TABLEs first
    for sql in req.creates:
        result = execute_sql(sql)
        if result["status"] == "success":
            results["created"] += 1
        else:
            err = result.get("message", "Unknown")
            if "already exists" in err.lower():
                results["created"] += 1  # already exists = success
            else:
                m = re.match(r'CREATE\s+TABLE\s+(\w+)', sql, re.IGNORECASE)
                results["errors"].append({"table": m.group(1) if m else "?", "error": err, "type": "create"})

    # Execute INSERTs
    for sql in req.inserts:
        result = execute_sql(sql)
        if result["status"] == "success":
            results["inserted"] += 1
        else:
            m = re.match(r'INSERT\s+INTO\s+(\w+)', sql, re.IGNORECASE)
            results["errors"].append({"table": m.group(1) if m else "?", "error": result.get("message", "Unknown"), "type": "insert"})

    results["status"] = "success" if not results["errors"] else "partial"
    return results
