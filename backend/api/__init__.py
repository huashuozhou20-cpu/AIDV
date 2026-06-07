"""API route modules — each module exports a FastAPI APIRouter as `router`."""
from .query import router as query_router
from .chat import router as chat_router
from .metrics import router as metrics_router
