#!/usr/bin/env bash
# AIDV one-click launcher.  Ctrl+C to stop everything cleanly.

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
DB_DIR="$HOME/.aidv-data"
TCP_PORT=8765
HTTP_PORT=8080
BACKEND_PORT=8000
FRONTEND_PORT=3000

# ── Cleanup (only runs on SIGINT/SIGTERM, NOT on EXIT) ─────────────────
RMDB_PID=""
RMDB_WATCHDOG_PID=""
BACKEND_PID=""
FRONTEND_PID=""

cleanup() {
    echo ""
    echo "🛑 Shutting down..."
    # Kill watchdog first (so it doesn't restart RMDB)
    [ -n "$RMDB_WATCHDOG_PID" ] && kill $RMDB_WATCHDOG_PID 2>/dev/null
    [ -n "$FRONTEND_PID" ] && kill $FRONTEND_PID 2>/dev/null
    [ -n "$BACKEND_PID" ]  && kill $BACKEND_PID 2>/dev/null
    [ -n "$RMDB_PID" ]     && kill $RMDB_PID 2>/dev/null
    sleep 1
    # Force-clear ports
    for p in $TCP_PORT $HTTP_PORT $BACKEND_PORT; do
        fuser -k ${p}/tcp 2>/dev/null || true
    done
    echo "✅ Stopped. Run ./start.sh to restart."
    exit 0
}
trap cleanup SIGINT SIGTERM

# ── Kill stale processes ────────────────────────────────────────────────
echo "🔍 Freeing ports..."
for p in $TCP_PORT $HTTP_PORT $BACKEND_PORT; do
    fuser -k ${p}/tcp 2>/dev/null && echo "   Freed port $p" || true
done
sleep 1

# ── Init database directory ─────────────────────────────────────────────
# NEVER delete an existing database directory — only create if missing.
# RMDB will handle recovery of existing data on startup.
if [ ! -d "$DB_DIR" ]; then
    echo "📁 Creating new database: $DB_DIR"
fi

# ── Start RMDB (with auto-restart watchdog) ─────────────────────────────
echo "🚀 RMDB (port $HTTP_PORT)..."
rmdb_watchdog() {
    while true; do
        "$PROJECT_DIR/rmdb/build_new/bin/rmdb" "$DB_DIR" "$TCP_PORT" "$HTTP_PORT" &
        local pid=$!
        wait $pid 2>/dev/null
        echo "   ⚠ RMDB exited (code $?). Restarting in 1s..."
        sleep 1
    done
}
rmdb_watchdog &
RMDB_WATCHDOG_PID=$!

echo -n "   Waiting"
for i in $(seq 1 15); do
    sleep 1; echo -n "."
    curl -s -X POST "http://127.0.0.1:$HTTP_PORT/query" -d "SHOW TABLES;" > /dev/null 2>&1 && break
    if [ $i -eq 15 ]; then
        echo " FAILED"
        echo "❌ RMDB did not become ready. Check: curl http://127.0.0.1:$HTTP_PORT/query"
        kill $RMDB_WATCHDOG_PID 2>/dev/null
        exit 1
    fi
done
RMDB_PID=$(pgrep -f "rmdb.*$DB_DIR" | head -1)
echo " ready (pid $RMDB_PID, watchdog $RMDB_WATCHDOG_PID)"

# ── Start Backend ───────────────────────────────────────────────────────
echo "🚀 Backend (port $BACKEND_PORT)..."
cd "$PROJECT_DIR/backend"
DB_HOST=127.0.0.1 DB_PORT=$TCP_PORT HTTP_PORT=$HTTP_PORT python3 main.py &
BACKEND_PID=$!

echo -n "   Waiting"
for i in $(seq 1 10); do
    sleep 1; echo -n "."
    curl -s "http://127.0.0.1:$BACKEND_PORT/api/metrics" > /dev/null 2>&1 && break
    kill -0 $BACKEND_PID 2>/dev/null || { echo " CRASHED"; kill $RMDB_PID 2>/dev/null; exit 1; }
done
echo " ready (pid $BACKEND_PID)"

# ── Build Frontend ──────────────────────────────────────────────────────
echo "🔨 Building frontend..."
cd "$PROJECT_DIR/frontend"
npx vite build 2>&1 | grep -E "✓|error" || true
FRONTEND_PID=""  # frontend is served by backend, no separate process

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  ✅ AIDV 已启动"
echo "  🌐 http://localhost:$BACKEND_PORT"
echo "  ⏹  按 Ctrl+C 关闭"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

wait
