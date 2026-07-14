"""Authentication and user management endpoints."""
from fastapi import APIRouter, Header
from pydantic import BaseModel
from core.auth import register_user, authenticate, create_token, verify_token, get_user_stats, init_db

router = APIRouter()

# Initialize DB on first import
init_db()


class RegisterRequest(BaseModel):
    username: str
    email: str
    password: str


class LoginRequest(BaseModel):
    username: str
    password: str
    as_admin: bool = False


def _get_current_user(authorization: str = Header("")) -> dict | None:
    """Extract and verify user from Authorization header."""
    if not authorization.startswith("Bearer "):
        return None
    return verify_token(authorization[7:])


@router.post("/auth/register")
def api_register(req: RegisterRequest):
    user = register_user(req.username.strip(), req.email.strip(), req.password)
    if not user:
        return {"status": "error", "message": "用户名或邮箱已被注册，或格式不符合要求"}
    token = create_token(user)
    return {"status": "success", "token": token, "user": {"id": user.id, "username": user.username, "email": user.email}}


@router.post("/auth/login")
def api_login(req: LoginRequest):
    user = authenticate(req.username.strip(), req.password)
    if not user:
        return {"status": "error", "message": "用户名或密码错误"}
    if req.as_admin and not user.is_admin:
        return {"status": "error", "message": "该用户不是管理员，请以普通用户身份登录"}
    token = create_token(user)
    return {"status": "success", "token": token, "user": {"id": user.id, "username": user.username, "email": user.email}}


@router.get("/auth/me")
def api_me(authorization: str = Header("")):
    payload = _get_current_user(authorization)
    if not payload:
        return {"status": "error", "message": "未登录"}
    return {"status": "success", "user": {"id": payload["user_id"], "username": payload["username"]}}


@router.get("/auth/stats")
def api_stats(authorization: str = Header("")):
    payload = _get_current_user(authorization)
    if not payload:
        return {"status": "error", "message": "未登录"}
    stats = get_user_stats(payload["user_id"])
    return {"status": "success", "stats": stats}
