"""Dynamic data CRUD API — auto-generated operations based on table metadata."""
import csv
import io
import json
import re
from fastapi import APIRouter, Header, UploadFile, File
from fastapi.responses import StreamingResponse
from pydantic import BaseModel
from core.database import execute_sql, db_query_raw, _parse_rmdb_ascii_table
from core.auth import verify_token

router = APIRouter()

import sqlite3, os
_DB_PATH = os.path.join(os.path.dirname(__file__), "..", "users.db")


def _get_conn():
    conn = sqlite3.connect(_DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def _get_current_user(authorization: str = Header("")) -> dict | None:
    if not authorization.startswith("Bearer "):
        return None
    return verify_token(authorization[7:])


def _validate_table_ownership(table_name: str, user_id: int) -> bool:
    """Check that the given RMDB table belongs to one of the user's scenarios."""
    conn = _get_conn()
    row = conn.execute(
        """SELECT 1 FROM scenario_tables st
           JOIN scenarios s ON st.scenario_id = s.id
           WHERE st.table_name = ? AND s.user_id = ?""",
        (table_name, user_id),
    ).fetchone()
    conn.close()
    return row is not None


def _get_pk_column(table_name: str) -> str:
    """Discover the primary key column for a table."""
    try:
        raw = db_query_raw(f"DESC {table_name}")
        if raw and raw.startswith("{"):
            j = json.loads(raw)
            if "result" in j:
                _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                for row in rows:
                    if len(row) >= 5 and row[4] == "PRI":
                        return row[0]
    except Exception:
        pass
    return "id"  # fallback


def _get_string_columns(table_name: str) -> list[str]:
    """Get all text-compatible columns for search. RMDB stores text as TYPE_STRING
    internally but the DESC output shows the original type keyword."""
    try:
        raw = db_query_raw(f"DESC {table_name}")
        cols = []
        if raw and raw.startswith("{"):
            j = json.loads(raw)
            if "result" in j:
                _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                for row in rows:
                    if len(row) >= 2 and row[0] and row[0] != "Field":
                        t = row[1].upper() if len(row) > 1 else ""
                        # RMDB internally maps CHAR/VARCHAR/TEXT/DATE → TYPE_STRING
                        # DESC shows "STRING" for all text types
                        if t in ("STRING", "CHAR", "VARCHAR", "TEXT", "DATE") or "CHAR" in t:
                            cols.append(row[0])
        return cols
    except Exception:
        return []


# ── Row CRUD ────────────────────────────────────────────────────────────────

class InsertRowRequest(BaseModel):
    data: dict  # {col_name: value, ...}


class UpdateRowRequest(BaseModel):
    data: dict  # {col_name: new_value, ...}


@router.get("/data/{table_name}/rows")
def api_get_rows(table_name: str, page: int = 1, size: int = 50,
                 search: str = "", sort: str = "", order: str = "asc",
                 authorization: str = Header("")):
    """Paginated row query with search and sort."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    if not _validate_table_ownership(table_name, user["user_id"]):
        return {"status": "error", "message": "表不存在或无权访问"}

    pk = _get_pk_column(table_name)
    page = max(1, page)
    size = min(200, max(1, size))

    # RMDB has a bug with OR in WHERE clauses — multiple OR conditions cause
    # the WHERE to be ignored entirely.  Work around it by fetching all rows and
    # filtering / sorting / paginating in Python.
    str_cols = _get_string_columns(table_name) if search else []
    search_lower = search.lower().replace("'", "") if search else ""

    # Fetch all rows (no WHERE — RMDB OR bug workaround)
    order_col = ''.join(c for c in (sort or pk) if c.isalnum() or c == '_')
    order_dir = "DESC" if (order.upper() == "DESC" if sort else True) else "ASC"
    query_sql = f"SELECT * FROM {table_name} ORDER BY {order_col} {order_dir}"
    result = execute_sql(query_sql)

    all_data = result.get("data", [])
    columns = result.get("columns", [])

    # Filter in Python
    if search_lower and str_cols:
        col_indices = [columns.index(c) for c in str_cols if c in columns]
        filtered = []
        for row in all_data:
            if any(search_lower in str(row[i]).lower().replace("'", "") for i in col_indices):
                filtered.append(row)
        all_data = filtered

    total = len(all_data)
    offset = (page - 1) * size
    paged_data = all_data[offset:offset + size]

    return {
        "status": result.get("status", "success"),
        "columns": columns,
        "data": paged_data,
        "total": total,
        "page": page,
        "size": size,
        "total_pages": max(1, (total + size - 1) // size),
    }


@router.post("/data/{table_name}/rows")
def api_insert_row(table_name: str, req: InsertRowRequest, authorization: str = Header("")):
    """Insert a new row into a table."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    if not _validate_table_ownership(table_name, user["user_id"]):
        return {"status": "error", "message": "表不存在或无权访问"}

    if not req.data:
        return {"status": "error", "message": "数据不能为空"}

    # Fetch column metadata to determine which cols to quote
    col_meta = _get_column_metadata(table_name)
    col_types = {c["name"]: c["type"] for c in col_meta}

    cols = []
    vals = []
    for k, v in req.data.items():
        safe_k = ''.join(c for c in k if c.isalnum() or c == '_')
        cols.append(safe_k)
        col_type = col_types.get(safe_k, "STRING").upper()
        if v is None or str(v).strip() == "":
            vals.append("NULL")
        elif "INT" in col_type or "FLOAT" in col_type:
            clean_v = str(v).replace("'", "").replace('"', "")
            if not clean_v:
                vals.append("NULL")
            else:
                vals.append(clean_v)
        else:
            safe_v = str(v).replace("'", "")
            vals.append(f"'{safe_v}'")

    sql = f"INSERT INTO {table_name} ({', '.join(cols)}) VALUES ({', '.join(vals)})"
    result = execute_sql(sql)
    return result


@router.put("/data/{table_name}/rows/{pk_value}")
def api_update_row(table_name: str, pk_value: str, req: UpdateRowRequest, authorization: str = Header("")):
    """Update a row by primary key."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    if not _validate_table_ownership(table_name, user["user_id"]):
        return {"status": "error", "message": "表不存在或无权访问"}

    pk_col = _get_pk_column(table_name)
    col_meta = _get_column_metadata(table_name)
    col_types = {c["name"]: c["type"] for c in col_meta}

    # Fetch current row data (need all column values for DELETE+INSERT workaround)
    # RMDB UPDATE has a known issue — we use DELETE + INSERT instead
    fetch_sql = f"SELECT * FROM {table_name} WHERE {pk_col} IN ("
    pk_type = col_types.get(pk_col, "STRING").upper()
    pk_safe = pk_value.replace("'", "")
    if "INT" in pk_type or "FLOAT" in pk_type:
        fetch_sql += pk_value + ")"
    else:
        fetch_sql += f"'{pk_safe}')"
    old_result = execute_sql(fetch_sql)
    if old_result["status"] != "success" or not old_result.get("data"):
        return {"status": "error", "message": "记录不存在"}

    old_row = old_result["data"][0]
    col_names = old_result["columns"]

    # Merge old values with new values
    new_row = list(old_row)
    for k, v in req.data.items():
        try:
            idx = col_names.index(k)
            new_row[idx] = str(v) if v is not None else ""
        except ValueError:
            pass  # column not found, skip

    # Build DELETE + INSERT (RMDB UPDATE workaround)
    # DELETE old row (use IN instead of = due to RMDB equality bug)
    if "INT" in pk_type or "FLOAT" in pk_type:
        del_sql = f"DELETE FROM {table_name} WHERE {pk_col} IN ({pk_value})"
    else:
        del_sql = f"DELETE FROM {table_name} WHERE {pk_col} IN ('{pk_safe}')"
    del_result = execute_sql(del_sql)
    if del_result["status"] != "success":
        return {"status": "error", "message": f"删除原记录失败: {del_result.get('message', '')}"}

    # INSERT updated row (all columns)
    insert_vals = []
    for i, val in enumerate(new_row):
        col_name = col_names[i]
        col_type = col_types.get(col_name, "STRING").upper()
        if not val or val == "NULL":
            insert_vals.append("NULL")
        elif "INT" in col_type or "FLOAT" in col_type:
            clean_v = val.replace("'", "").replace('"', "")
            insert_vals.append(clean_v if clean_v else "NULL")
        else:
            safe_v = val.replace("'", "")
            insert_vals.append(f"'{safe_v}'")

    insert_sql = f"INSERT INTO {table_name} ({', '.join(col_names)}) VALUES ({', '.join(insert_vals)})"
    result = execute_sql(insert_sql)
    return result


@router.delete("/data/{table_name}/rows/{pk_value}")
def api_delete_row(table_name: str, pk_value: str, authorization: str = Header("")):
    """Delete a row by primary key."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    if not _validate_table_ownership(table_name, user["user_id"]):
        return {"status": "error", "message": "表不存在或无权访问"}

    pk_col = _get_pk_column(table_name)
    pk_safe = pk_value.replace("'", "")
    # Use IN instead of = due to RMDB equality bug on INT columns
    try:
        int(pk_value)
        sql = f"DELETE FROM {table_name} WHERE {pk_col} IN ({pk_value})"
    except ValueError:
        sql = f"DELETE FROM {table_name} WHERE {pk_col} IN ('{pk_safe}')"
    result = execute_sql(sql)
    return result


# ── CSV Import / Export ─────────────────────────────────────────────────────

@router.post("/data/{table_name}/import")
async def api_import_csv(table_name: str, file: UploadFile = File(...), authorization: str = Header("")):
    """Import rows from a CSV file."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    if not _validate_table_ownership(table_name, user["user_id"]):
        return {"status": "error", "message": "表不存在或无权访问"}

    content = await file.read()
    text = content.decode("utf-8-sig", errors="replace")

    reader = csv.reader(io.StringIO(text))
    rows = list(reader)
    if len(rows) < 2:
        return {"status": "error", "message": "CSV 文件至少需要表头行和一行数据"}

    headers = [h.strip() for h in rows[0]]
    data_rows = rows[1:]

    col_meta = _get_column_metadata(table_name)
    col_types = {c["name"]: c["type"] for c in col_meta}

    success = 0
    errors = []
    for i, row in enumerate(data_rows):
        if not any(cell.strip() for cell in row):
            continue
        vals = []
        for j, header in enumerate(headers):
            val = row[j] if j < len(row) else ""
            col_type = col_types.get(header, "STRING").upper()
            if not val.strip():
                vals.append("NULL")
            elif "INT" in col_type or "FLOAT" in col_type:
                clean = val.strip().replace("'", "").replace('"', "")
                vals.append(clean if clean else "NULL")
            else:
                safe = val.strip().replace("'", "")  # RMDB does not support '' escaping
                vals.append(f"'{safe}'")

        try:
            sql = f"INSERT INTO {table_name} ({', '.join(headers)}) VALUES ({', '.join(vals)})"
            result = execute_sql(sql)
            if result["status"] == "success":
                success += 1
            else:
                errors.append({"row": i + 2, "error": result.get("message", "Unknown")})
        except Exception as e:
            errors.append({"row": i + 2, "error": str(e)})

    return {"status": "success", "imported": success, "errors": errors}


@router.get("/data/{table_name}/export")
def api_export_csv(table_name: str, authorization: str = Header("")):
    """Export all rows as CSV."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    if not _validate_table_ownership(table_name, user["user_id"]):
        return {"status": "error", "message": "表不存在或无权访问"}

    result = execute_sql(f"SELECT * FROM {table_name}")
    if result["status"] != "success":
        return {"status": "error", "message": result.get("message", "查询失败")}

    output = io.StringIO()
    output.write('﻿')  # UTF-8 BOM for Excel compatibility
    writer = csv.writer(output)
    writer.writerow(result["columns"])
    for row in result["data"]:
        writer.writerow(row)

    output.seek(0)
    return StreamingResponse(
        iter([output.getvalue().encode('utf-8')]),
        media_type="text/csv; charset=utf-8",
        headers={"Content-Disposition": f"attachment; filename={table_name}.csv"},
    )


# ── Helpers ─────────────────────────────────────────────────────────────────

def _get_column_metadata(table_name: str) -> list[dict]:
    """Fetch column name + type from RMDB."""
    try:
        raw = db_query_raw(f"DESC {table_name}")
        cols = []
        if raw and raw.startswith("{"):
            j = json.loads(raw)
            if "result" in j:
                _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                for row in rows:
                    if len(row) >= 2 and row[0] and row[0] != "Field":
                        cols.append({"name": row[0], "type": row[1] if len(row) > 1 else "STRING"})
        return cols
    except Exception:
        return []
