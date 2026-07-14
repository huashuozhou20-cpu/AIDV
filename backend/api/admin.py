"""Admin user management — list, delete, promote/demote users (admin-only)."""
from fastapi import APIRouter, Header
from pydantic import BaseModel
from core.auth import verify_token, get_all_users, delete_user, set_admin

router = APIRouter()


def _get_current_user(authorization: str = Header("")) -> dict | None:
    if not authorization.startswith("Bearer "):
        return None
    return verify_token(authorization[7:])


def _require_admin(authorization: str = Header("")) -> dict | None:
    user = _get_current_user(authorization)
    if user is None or not user.get("is_admin"):
        return None
    return user


@router.get("/admin/users")
def api_list_users(authorization: str = Header("")):
    """List all registered users (admin only)."""
    admin = _require_admin(authorization)
    if not admin:
        return {"status": "error", "message": "无权访问，需要管理员权限"}
    return {"status": "success", "users": get_all_users()}


@router.delete("/admin/users/{user_id}")
def api_delete_user(user_id: int, authorization: str = Header("")):
    """Delete a user (admin only, cannot self-delete)."""
    admin = _require_admin(authorization)
    if not admin:
        return {"status": "error", "message": "无权访问，需要管理员权限"}
    if user_id == admin["user_id"]:
        return {"status": "error", "message": "不能删除自己的账户"}
    if delete_user(user_id):
        return {"status": "success", "message": "用户已删除"}
    return {"status": "error", "message": "用户不存在"}


class SetAdminRequest(BaseModel):
    is_admin: bool


@router.put("/admin/users/{user_id}/admin")
def api_toggle_admin(user_id: int, req: SetAdminRequest, authorization: str = Header("")):
    """Promote or demote a user's admin status (admin only, cannot self-demote)."""
    admin = _require_admin(authorization)
    if not admin:
        return {"status": "error", "message": "无权访问，需要管理员权限"}
    if user_id == admin["user_id"] and not req.is_admin:
        return {"status": "error", "message": "不能移除自己的管理员权限"}
    if set_admin(user_id, req.is_admin):
        action = "已提升为管理员" if req.is_admin else "已降为普通用户"
        return {"status": "success", "message": action}
    return {"status": "error", "message": "用户不存在"}
