"""Database communication layer — HTTP primary, TCP fallback."""
import json
import socket
import time
import threading
import urllib.error
import urllib.request
from datetime import datetime, timezone
from . import config
from . import metrics as metrics_mod

# RMDB's HTTP server handles one request at a time. This lock prevents
# concurrent requests from the backend, avoiding "Connection reset" errors.
# RLock allows EXPLAIN to call execute_sql internally without deadlocking.
_rmdb_lock = threading.RLock()

# In-memory query log (ring buffer)
_query_log: list[dict] = []
_query_log_max = 200
_query_log_lock = threading.Lock()


def _append_query_log(sql: str, elapsed_ms: float, status: str, row_count: int = 0):
    global _query_log
    entry = {
        "time": datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S"),
        "sql": sql[:500],  # truncate very long queries
        "elapsed_ms": elapsed_ms,
        "status": status,
        "rows": row_count,
    }
    with _query_log_lock:
        _query_log.append(entry)
        if len(_query_log) > _query_log_max:
            _query_log = _query_log[-_query_log_max:]


def get_query_log(limit: int = 50) -> list[dict]:
    global _query_log
    with _query_log_lock:
        return list(reversed(_query_log[-limit:]))


def _ensure_semicolon(sql: str) -> str:
    """Append ';' if the SQL statement doesn't already end with one."""
    stripped = sql.rstrip()
    if not stripped.endswith(";"):
        return stripped + ";"
    return stripped


def http_query(sql: str) -> str:
    """Send SQL via RMDB's HTTP REST API. Retries on transient errors.

    IMPORTANT: RMDB returns HTTP 400 for SQL errors (e.g. table not found).
    We must read the response body even on error status — it contains the
    actual error message from RMDB.
    """
    url = f"http://{config.DB_HOST}:{config.HTTP_PORT}/query"
    data = _ensure_semicolon(sql).encode("utf-8")
    last_err = None
    for attempt in range(3):
        try:
            req = urllib.request.Request(url, data=data, method="POST")
            req.add_header("Content-Type", "text/plain")
            with urllib.request.urlopen(req, timeout=30) as resp:
                return resp.read().decode("utf-8", errors="replace").strip()
        except urllib.error.HTTPError as e:
            # RMDB uses HTTP 400 for SQL-level errors — read the body for the
            # actual error message and return it as-is (it's valid JSON).
            body = e.read().decode("utf-8", errors="replace").strip()
            if body:
                return body  # {"error":"..."} — let execute_sql handle it
            last_err = e
            if attempt < 2:
                time.sleep(0.3 * (attempt + 1))
        except Exception as e:
            last_err = e
            if attempt < 2:
                time.sleep(0.3 * (attempt + 1))
    raise ConnectionError(f"HTTP query failed after 3 retries: {last_err}")


def tcp_query(payload: str, timeout: int = 30) -> str:
    """Send raw text via TCP socket (fallback / diagnostic)."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect((config.DB_HOST, config.DB_PORT))
        sock.sendall(_ensure_semicolon(payload).encode("utf-8"))
        sock.shutdown(socket.SHUT_WR)
        chunks = []
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        return b"".join(chunks).decode("utf-8", errors="replace").strip()
    finally:
        sock.close()


def db_query_raw(sql: str) -> str:
    """Send SQL to RMDB. Serialised via lock to avoid overwhelming RMDB."""
    with _rmdb_lock:
        # Try HTTP first (with retries built into http_query)
        if config.HTTP_PORT:
            for attempt in range(2):
                try:
                    raw = http_query(sql)
                    if raw:
                        return raw
                except Exception:
                    if attempt == 0:
                        time.sleep(0.5)  # brief pause before retry
            # HTTP failed twice — fall through to TCP
        # TCP fallback
        try:
            return tcp_query(sql)
        except Exception:
            return ""


def _parse_rmdb_ascii_table(text: str) -> tuple[list[str], list[list[str]]]:
    """Parse RMDB's ASCII-art table output into columns and rows.

    RMDB returns formatted tables like::

        +----+------+
        | id | name |
        +----+------+
        | 1  | Alice|
        | 2  | Bob  |
        +----+------+
        Total record(s): 2

    Returns (columns, data).  If parsing fails, falls back to returning the
    raw text as a single ``result`` column.
    """
    lines = text.split("\n")
    while lines and not lines[-1].strip():
        lines.pop()

    columns: list[str] = []
    rows: list[list[str]] = []
    header_found = False
    header_sep_seen = False

    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("+") and stripped.endswith("+"):
            if header_found:
                header_sep_seen = True
            continue
        if stripped.lower().startswith("total record"):
            break
        if stripped.startswith("|") and stripped.endswith("|"):
            cells = [c.strip() for c in stripped[1:-1].split("|")]
            if not header_found:
                columns = cells
                header_found = True
            elif header_sep_seen:
                rows.append(cells)

    if columns:
        return columns, rows
    return ["result"], [[line] for line in lines if line.strip()]


def execute_sql(sql: str) -> dict:
    """Execute a SQL statement and return a structured result dict."""
    metrics_mod.record_query()
    try:
        t0 = time.time()
        raw = db_query_raw(sql)
        elapsed_ms = round((time.time() - t0) * 1000, 2)

        # RMDB is unreachable — empty response means something is wrong
        if not raw:
            _append_query_log(sql, elapsed_ms, "error")
            return {
                "status": "error",
                "message": "数据库无响应 — 请确认 RMDB 是否在运行 (./start.sh)",
                "columns": [], "data": [], "execution_time_ms": elapsed_ms,
            }

        if raw.startswith("{"):
            try:
                j = json.loads(raw)
                if "columns" in j and "data" in j:
                    result = {
                        "status": "success",
                        "columns": j.get("columns", []),
                        "data": j.get("data", j.get("rows", [])),
                        "execution_time_ms": elapsed_ms,
                    }
                    _append_query_log(sql, elapsed_ms, "success", len(result["data"]))
                    return result
                if "error" in j:
                    _append_query_log(sql, elapsed_ms, "error")
                    return {
                        "status": "error",
                        "message": str(j["error"]),
                        "columns": [],
                        "data": [],
                        "execution_time_ms": elapsed_ms,
                    }
                if "result" in j:
                    cols, data = _parse_rmdb_ascii_table(str(j["result"]))
                    result = {
                        "status": "success",
                        "columns": cols,
                        "data": data,
                        "execution_time_ms": elapsed_ms,
                    }
                    _append_query_log(sql, elapsed_ms, "success", len(data))
                    return result
                _append_query_log(sql, elapsed_ms, "success")
                return {
                    "status": "success",
                    "columns": list(j.keys()),
                    "data": [list(j.values())],
                    "execution_time_ms": elapsed_ms,
                }
            except json.JSONDecodeError:
                pass

        result = _parse_raw_response(raw, elapsed_ms)
        _append_query_log(sql, elapsed_ms, result.get("status", "error"), len(result.get("data", [])))
        return result
    except socket.timeout:
        _append_query_log(sql, 0, "timeout")
        return {"status": "error", "message": "数据库连接超时 — 请确认 RMDB 是否在运行", "columns": [], "data": [], "execution_time_ms": 0}
    except ConnectionRefusedError:
        _append_query_log(sql, 0, "connection_refused")
        return {"status": "error", "message": f"RMDB 未运行 — 请先执行 ./start.sh", "columns": [], "data": [], "execution_time_ms": 0}
    except Exception as e:
        _append_query_log(sql, 0, "exception")
        return {"status": "error", "message": str(e), "columns": [], "data": [], "execution_time_ms": 0}


def _parse_raw_response(raw: str, elapsed_ms: float) -> dict:
    """Parse pipe/tab-delimited TCP response into columns + rows."""
    if not raw:
        return {"status": "success", "columns": [], "data": [], "execution_time_ms": elapsed_ms}
    if "ERROR" in raw.upper():
        return {"status": "error", "message": raw, "columns": [], "data": [], "execution_time_ms": elapsed_ms}

    lines = [l.strip() for l in raw.split("\n") if l.strip()]
    if not lines:
        return {"status": "success", "columns": [], "data": [], "execution_time_ms": elapsed_ms}

    separator = "\t" if "\t" in raw else ("|" if "|" in raw else None)
    if separator:
        columns = [c.strip() for c in lines[0].split(separator)]
        data = []
        for line in lines[1:]:
            vals = [v.strip() for v in line.split(separator)]
            if len(vals) == len(columns):
                data.append(vals)
            elif len(vals) <= len(columns):
                data.append(vals)
        return {"status": "success", "columns": columns, "data": data, "execution_time_ms": elapsed_ms}

    return {"status": "success", "columns": ["result"], "data": [[line] for line in lines], "execution_time_ms": elapsed_ms}
