"""Metrics and lock-graph endpoints."""
import json
from fastapi import APIRouter
from core import metrics as metrics_mod
from core.database import db_query_raw

router = APIRouter()


@router.get("/metrics")
def api_metrics():
    return metrics_mod.get_latest_metrics()


@router.get("/metrics/history")
def api_metrics_history():
    return metrics_mod.get_history()


@router.get("/lock-graph")
def api_lock_graph():
    # Try RMDB's native SHOW LOCK GRAPH first
    try:
        raw = db_query_raw("SHOW LOCK GRAPH")
        if raw:
            if raw.startswith("{"):
                j = json.loads(raw)
                if "result" in j:
                    return _parse_rmdb_lock_graph(str(j["result"]))
            else:
                return _parse_rmdb_lock_graph(raw)
    except Exception:
        pass

    # No locks — return empty graph
    return {"has_deadlock": False, "nodes": [], "edges": [], "cycle_node_ids": []}


def _parse_rmdb_lock_graph(text: str) -> dict:
    """Parse RMDB's SHOW LOCK GRAPH ASCII output into structured lock graph."""
    import re
    nodes: list[dict] = []
    edges: list[dict] = []
    seen_txns: set[str] = set()

    # Parse lock table rows: "| txn_id | lock_data | mode | granted |"
    for line in text.split("\n"):
        line = line.strip()
        if not line.startswith("|") or "txn_id" in line or "no locks" in line:
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) >= 5 and parts[1] and parts[1] != "txn_id":
            txn_id = parts[1]
            lock_data = parts[2]
            granted = "yes" in parts[4].lower()
            if txn_id not in seen_txns:
                seen_txns.add(txn_id)
                nodes.append({
                    "id": f"Tx-{txn_id}",
                    "label": f"Tx-{txn_id}\n{lock_data}",
                    "status": "active" if granted else "blocked",
                })

    # Parse wait-for edges
    in_wait_for = False
    for line in text.split("\n"):
        line = line.strip()
        if "Wait-for edges:" in line:
            in_wait_for = True
            continue
        if in_wait_for and line.startswith("|") and "waiter" not in line:
            parts = [p.strip() for p in line.split("|")]
            if len(parts) >= 3 and parts[1] and parts[2]:
                waiter = parts[1]
                holder = parts[2]
                waiter_id = f"Tx-{waiter}"
                holder_id = f"Tx-{holder}"
                if waiter_id not in seen_txns:
                    seen_txns.add(waiter_id)
                    nodes.append({"id": waiter_id, "label": f"Tx-{waiter}", "status": "blocked"})
                if holder_id not in seen_txns:
                    seen_txns.add(holder_id)
                    nodes.append({"id": holder_id, "label": f"Tx-{holder}", "status": "active"})
                edges.append({
                    "from": waiter_id,
                    "to": holder_id,
                    "label": f"waiting on Tx-{holder}",
                })

    # Check for deadlocks (cycles in wait-for graph)
    has_deadlock = "Deadlock victims" in text
    cycle_node_ids = [n["id"] for n in nodes if n["status"] == "blocked"] if has_deadlock else []

    return {
        "has_deadlock": has_deadlock,
        "nodes": nodes,
        "edges": edges,
        "cycle_node_ids": cycle_node_ids,
    }


@router.get("/anomalies")
def api_anomalies():
    """Detect slow queries and other anomalies from the query log."""
    from core.database import get_query_log
    log = get_query_log(200)
    anomalies = []
    for entry in log:
        if entry["elapsed_ms"] > 100:
            anomalies.append({
                "type": "slow_query",
                "severity": "high" if entry["elapsed_ms"] > 500 else "medium" if entry["elapsed_ms"] > 200 else "low",
                "time": entry["time"],
                "detail": f"Query took {entry['elapsed_ms']}ms: {entry['sql'][:100]}",
                "sql": entry["sql"][:200],
            })
        if entry["status"] == "error":
            anomalies.append({
                "type": "query_error",
                "severity": "medium",
                "time": entry["time"],
                "detail": f"Query failed: {entry['sql'][:100]}",
                "sql": entry["sql"][:200],
            })
    return {"anomalies": anomalies[:20], "count": len(anomalies)}
