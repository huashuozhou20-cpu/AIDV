"""User authentication — SQLite storage, JWT tokens, password hashing."""
import hashlib
import os
import sqlite3
import time
from dataclasses import dataclass
from typing import Optional

import jwt

# JWT secret — in production this would be an env var
_JWT_SECRET = os.environ.get("JWT_SECRET", "aidv-dev-secret-change-in-prod")
_DB_PATH = os.path.join(os.path.dirname(__file__), "..", "users.db")
_ADMIN_USERNAME = os.environ.get("ADMIN_USERNAME", "").strip()
_ADMIN_PASSWORD = os.environ.get("ADMIN_PASSWORD", "").strip()


def _get_conn() -> sqlite3.Connection:
    conn = sqlite3.connect(_DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    """Create users table if it doesn't exist."""
    conn = _get_conn()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS users (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            username   TEXT UNIQUE NOT NULL,
            email      TEXT UNIQUE NOT NULL,
            password   TEXT NOT NULL,
            is_admin   INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        )
    """)
    # Migrate existing DBs that don't have the is_admin column yet
    try:
        conn.execute("ALTER TABLE users ADD COLUMN is_admin INTEGER NOT NULL DEFAULT 0")
    except sqlite3.OperationalError:
        pass  # column already exists
    # Practice scores table
    conn.execute("""
        CREATE TABLE IF NOT EXISTS practice_scores (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            question   TEXT NOT NULL,
            user_answer TEXT NOT NULL,
            correct    INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    """)
    # Application scenarios
    conn.execute("""
        CREATE TABLE IF NOT EXISTS scenarios (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id     INTEGER NOT NULL,
            name        TEXT NOT NULL,
            slug        TEXT NOT NULL UNIQUE,
            description TEXT DEFAULT '',
            icon        TEXT DEFAULT 'database',
            is_active   INTEGER DEFAULT 0,
            created_at  TEXT NOT NULL DEFAULT (datetime('now')),
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    """)
    # Tables belonging to each scenario
    conn.execute("""
        CREATE TABLE IF NOT EXISTS scenario_tables (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            scenario_id  INTEGER NOT NULL,
            table_name   TEXT NOT NULL,
            display_name TEXT NOT NULL,
            description  TEXT DEFAULT '',
            created_at   TEXT NOT NULL DEFAULT (datetime('now')),
            FOREIGN KEY (scenario_id) REFERENCES scenarios(id) ON DELETE CASCADE
        )
    """)
    # Workspace folders — hierarchical grouping of tables in Dashboard
    conn.execute("""
        CREATE TABLE IF NOT EXISTS workspace_folders (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT NOT NULL,
            parent_id  INTEGER DEFAULT NULL,
            sort_order INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            FOREIGN KEY (parent_id) REFERENCES workspace_folders(id) ON DELETE CASCADE
        )
    """)
    # Workspace tables — user-defined tables with metadata
    conn.execute("""
        CREATE TABLE IF NOT EXISTS workspace_tables (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            table_name   TEXT NOT NULL UNIQUE,
            display_name TEXT NOT NULL,
            folder_id    INTEGER DEFAULT NULL,
            columns_json TEXT DEFAULT '[{"name":"id","type":"INT","pk":true,"auto_increment":true}]',
            sort_order   INTEGER NOT NULL DEFAULT 0,
            created_at   TEXT NOT NULL DEFAULT (datetime('now')),
            FOREIGN KEY (folder_id) REFERENCES workspace_folders(id) ON DELETE SET NULL
        )
    """)
    # Auto-create/upgrade admin user from ADMIN_USERNAME env var
    if _ADMIN_USERNAME:
        import random, string
        if _ADMIN_PASSWORD:
            password = _hash_password(_ADMIN_PASSWORD)
        else:
            password = _hash_password("admin" + "".join(random.choices(string.ascii_lowercase, k=8)))
        conn.execute(
            "INSERT INTO users (username, email, password, is_admin) VALUES (?, ?, ?, 1) "
            "ON CONFLICT(username) DO UPDATE SET is_admin = 1, password = excluded.password",
            (_ADMIN_USERNAME, f"{_ADMIN_USERNAME}@admin.local", password),
        )
    conn.commit()
    conn.close()


def _hash_password(password: str) -> str:
    """Hash a password with SHA-256 + salt."""
    salt = os.urandom(16).hex()
    h = hashlib.sha256((salt + password).encode()).hexdigest()
    return f"{salt}${h}"


def _verify_password(password: str, stored: str) -> bool:
    """Verify a password against a stored hash."""
    salt, h = stored.split("$", 1)
    return hashlib.sha256((salt + password).encode()).hexdigest() == h


@dataclass
class User:
    id: int
    username: str
    email: str
    is_admin: bool = False


def register_user(username: str, email: str, password: str) -> Optional[User]:
    """Create a new user. Returns User on success, None if username/email taken."""
    if len(username) < 2 or len(password) < 4:
        return None
    conn = _get_conn()
    try:
        cursor = conn.execute(
            "INSERT INTO users (username, email, password) VALUES (?, ?, ?)",
            (username, email, _hash_password(password)),
        )
        conn.commit()
        return User(id=cursor.lastrowid, username=username, email=email, is_admin=False)
    except sqlite3.IntegrityError:
        return None  # username/email already exists
    finally:
        conn.close()


def authenticate(username: str, password: str) -> Optional[User]:
    """Verify credentials. Returns User on success, None on failure."""
    conn = _get_conn()
    row = conn.execute(
        "SELECT id, username, email, password, is_admin FROM users WHERE username = ? OR email = ?",
        (username, username),
    ).fetchone()
    conn.close()
    if row and _verify_password(password, row["password"]):
        return User(id=row["id"], username=row["username"], email=row["email"], is_admin=bool(row["is_admin"]))
    return None


def create_token(user: User) -> str:
    """Create a JWT token for the given user."""
    payload = {
        "user_id": user.id,
        "username": user.username,
        "is_admin": user.is_admin,
        "exp": int(time.time()) + 86400 * 7,  # 7 days
    }
    return jwt.encode(payload, _JWT_SECRET, algorithm="HS256")


def verify_token(token: str) -> Optional[dict]:
    """Verify and decode a JWT token. Returns payload or None."""
    try:
        return jwt.decode(token, _JWT_SECRET, algorithms=["HS256"])
    except jwt.InvalidTokenError:
        return None


def save_score(user_id: int, question: str, user_answer: str, correct: bool):
    """Save a practice attempt."""
    conn = _get_conn()
    conn.execute(
        "INSERT INTO practice_scores (user_id, question, user_answer, correct) VALUES (?, ?, ?, ?)",
        (user_id, question, user_answer, 1 if correct else 0),
    )
    conn.commit()
    conn.close()


def get_user_stats(user_id: int) -> dict:
    """Get practice statistics for a user."""
    conn = _get_conn()
    total = conn.execute("SELECT COUNT(*) FROM practice_scores WHERE user_id = ?", (user_id,)).fetchone()[0]
    correct = conn.execute("SELECT COUNT(*) FROM practice_scores WHERE user_id = ? AND correct = 1", (user_id,)).fetchone()[0]
    conn.close()
    return {"total": total, "correct": correct, "accuracy": round(correct / max(total, 1) * 100, 1)}


# ── Admin user management ──────────────────────────────────────────────────

def get_all_users() -> list[dict]:
    """List all registered users (admin only)."""
    conn = _get_conn()
    rows = conn.execute(
        "SELECT id, username, email, is_admin, created_at FROM users ORDER BY id"
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def delete_user(user_id: int) -> bool:
    """Delete a user by ID. Returns True if a row was deleted."""
    conn = _get_conn()
    cursor = conn.execute("DELETE FROM users WHERE id = ?", (user_id,))
    conn.commit()
    deleted = cursor.rowcount > 0
    conn.close()
    return deleted


def set_admin(user_id: int, is_admin: bool) -> bool:
    """Set a user's admin status. Returns True if a row was updated."""
    conn = _get_conn()
    cursor = conn.execute(
        "UPDATE users SET is_admin = ? WHERE id = ?",
        (1 if is_admin else 0, user_id),
    )
    conn.commit()
    updated = cursor.rowcount > 0
    conn.close()
    return updated
