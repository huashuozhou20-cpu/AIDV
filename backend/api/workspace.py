"""Workspace management — VS Code-style folder+table explorer for Dashboard."""
import json
import os
import sqlite3
from fastapi import APIRouter, Header
from pydantic import BaseModel
from core.auth import verify_token
from core.database import execute_sql, db_query_raw, _parse_rmdb_ascii_table

router = APIRouter()

_DB_PATH = os.path.join(os.path.dirname(__file__), "..", "users.db")


def _get_conn() -> sqlite3.Connection:
    conn = sqlite3.connect(_DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def _get_current_user(authorization: str = Header("")) -> dict | None:
    if not authorization.startswith("Bearer "):
        return None
    return verify_token(authorization[7:])


# ── Tree endpoint ──────────────────────────────────────────────────────────

@router.get("/workspace/tree")
def api_workspace_tree(authorization: str = Header("")):
    """Return the full workspace hierarchy: folders + tables as a tree."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}

    conn = _get_conn()
    folders = [dict(r) for r in conn.execute(
        "SELECT * FROM workspace_folders ORDER BY sort_order").fetchall()]
    tables = [dict(r) for r in conn.execute(
        "SELECT * FROM workspace_tables ORDER BY sort_order").fetchall()]

    # Get RMDB tables for real-time column count
    try:
        raw = db_query_raw("SHOW TABLES")
        rmdb_tables = set()
        if raw and raw.startswith("{"):
            j = json.loads(raw)
            if "result" in j:
                _, rows = _parse_rmdb_ascii_table(str(j["result"]))
                for row in rows:
                    if row and row[0]:
                        rmdb_tables.add(row[0])
    except Exception:
        rmdb_tables = set()

    # Enhance tables with RMDB info
    for t in tables:
        tn = t["table_name"]
        t["exists_in_rmdb"] = tn in rmdb_tables
        t["columns"] = []
        if t["columns_json"]:
            try:
                t["columns"] = json.loads(t["columns_json"])
            except Exception:
                pass

    conn.close()
    return {
        "status": "success",
        "folders": folders,
        "tables": tables,
    }


# ── Folder CRUD ────────────────────────────────────────────────────────────

class CreateFolderRequest(BaseModel):
    name: str
    parent_id: int | None = None


@router.post("/workspace/folders")
def api_create_folder(req: CreateFolderRequest, authorization: str = Header("")):
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    name = req.name.strip()
    if not name:
        return {"status": "error", "message": "名称不能为空"}

    conn = _get_conn()
    max_sort = conn.execute(
        "SELECT COALESCE(MAX(sort_order), -1) FROM workspace_folders WHERE parent_id IS ?",
        (req.parent_id,)
    ).fetchone()[0]
    cur = conn.execute(
        "INSERT INTO workspace_folders (name, parent_id, sort_order) VALUES (?, ?, ?)",
        (name, req.parent_id, max_sort + 1),
    )
    conn.commit()
    folder_id = cur.lastrowid
    conn.close()
    return {"status": "success", "folder": {"id": folder_id, "name": name, "parent_id": req.parent_id}}


@router.put("/workspace/folders/{folder_id}")
def api_rename_folder(folder_id: int, req: CreateFolderRequest, authorization: str = Header("")):
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    name = req.name.strip()
    if not name:
        return {"status": "error", "message": "名称不能为空"}
    conn = _get_conn()
    conn.execute("UPDATE workspace_folders SET name = ? WHERE id = ?", (name, folder_id))
    conn.commit()
    conn.close()
    return {"status": "success"}


@router.delete("/workspace/folders/{folder_id}")
def api_delete_folder(folder_id: int, authorization: str = Header("")):
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    # Move tables in this folder to root
    conn.execute("UPDATE workspace_tables SET folder_id = NULL WHERE folder_id = ?", (folder_id,))
    # Move child folders to parent
    folder = conn.execute("SELECT parent_id FROM workspace_folders WHERE id = ?", (folder_id,)).fetchone()
    if folder:
        parent = folder["parent_id"]
        conn.execute("UPDATE workspace_folders SET parent_id = ? WHERE parent_id = ?", (parent, folder_id))
    conn.execute("DELETE FROM workspace_folders WHERE id = ?", (folder_id,))
    conn.commit()
    conn.close()
    return {"status": "success"}


# ── Table CRUD ─────────────────────────────────────────────────────────────

class CreateTableRequest(BaseModel):
    name: str
    folder_id: int | None = None


@router.post("/workspace/tables")
def api_create_workspace_table(req: CreateTableRequest, authorization: str = Header("")):
    """Create a new table in RMDB and register it in the workspace.

    Table names in RMDB include a folder prefix for namespace isolation:
      - Root-level tables:  tbl_{display_name}
      - Tables in folder N: f{N}_{display_name}

    This means two folders can each have a table with the same display_name
    without conflict — they are different RMDB tables.
    """
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}

    display_name = req.name.strip()
    if not display_name:
        return {"status": "error", "message": "表名不能为空"}

    conn = _get_conn()

    # Build the RMDB table name with namespace prefix + unique suffix
    # The unique suffix avoids RMDB buffer-pool bugs where dropped-table
    # index pages contaminate new AUTO_INCREMENT tables.
    safe_suffix = "".join(c if c.isalnum() or c == '_' else '_' for c in display_name)
    if req.folder_id:
        safe_name = f"f{req.folder_id}_{safe_suffix}"
    else:
        safe_name = f"tbl_{safe_suffix}"

    # Register in workspace FIRST to get a unique id
    max_sort = conn.execute(
        "SELECT COALESCE(MAX(sort_order), -1) FROM workspace_tables WHERE folder_id IS ?",
        (req.folder_id,)
    ).fetchone()[0]
    cur = conn.execute(
        "INSERT INTO workspace_tables (table_name, display_name, folder_id, sort_order) VALUES (?, ?, ?, ?)",
        (safe_name, display_name, req.folder_id, max_sort + 1),
    )
    table_id = cur.lastrowid

    # Append unique workspace id to guarantee fresh RMDB file name
    unique_name = f"{safe_name}_w{table_id}"

    # Create table in RMDB
    sql = f"CREATE TABLE {unique_name} (id INT PRIMARY KEY AUTO_INCREMENT)"
    result = execute_sql(sql)
    if result["status"] == "error":
        conn.execute("DELETE FROM workspace_tables WHERE id = ?", (table_id,))
        conn.commit()
        conn.close()
        return {"status": "error", "message": f"创建表失败: {result.get('message', 'Unknown error')}"}

    # Update the table_name to the unique name
    conn.execute("UPDATE workspace_tables SET table_name = ? WHERE id = ?", (unique_name, table_id))
    conn.commit()
    conn.close()
    return {
        "status": "success",
        "table": {"id": table_id, "table_name": unique_name, "display_name": display_name,
                   "folder_id": req.folder_id, "columns": [{"name": "id", "type": "INT", "pk": True, "auto_increment": True}]},
    }


class RenameTableRequest(BaseModel):
    name: str


@router.put("/workspace/tables/{table_id}/rename")
def api_rename_table(table_id: int, req: RenameTableRequest, authorization: str = Header("")):
    """Rename a table — update display name in workspace metadata."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    name = req.name.strip()
    if not name:
        return {"status": "error", "message": "名称不能为空"}
    conn = _get_conn()
    conn.execute("UPDATE workspace_tables SET display_name = ? WHERE id = ?", (name, table_id))
    conn.commit()
    conn.close()
    return {"status": "success"}


class MoveTableRequest(BaseModel):
    folder_id: int | None = None


@router.put("/workspace/tables/{table_id}/move")
def api_move_table(table_id: int, req: MoveTableRequest, authorization: str = Header("")):
    """Move a table to a different folder."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    conn.execute("UPDATE workspace_tables SET folder_id = ? WHERE id = ?", (req.folder_id, table_id))
    conn.commit()
    conn.close()
    return {"status": "success"}


@router.delete("/workspace/tables/{table_id}")
def api_delete_workspace_table(table_id: int, authorization: str = Header("")):
    """Delete a workspace table — drops from RMDB and removes metadata."""
    user = _get_current_user(authorization)
    if not user:
        return {"status": "error", "message": "未登录"}
    conn = _get_conn()
    table = conn.execute("SELECT * FROM workspace_tables WHERE id = ?", (table_id,)).fetchone()
    if not table:
        conn.close()
        return {"status": "error", "message": "表不存在"}
    table = dict(table)
    try:
        db_query_raw(f"DROP TABLE {table['table_name']}")
    except Exception:
        pass
    conn.execute("DELETE FROM workspace_tables WHERE id = ?", (table_id,))
    conn.commit()
    conn.close()
    return {"status": "success", "message": "表已删除"}
