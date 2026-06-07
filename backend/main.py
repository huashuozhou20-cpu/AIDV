"""AIDV Backend — FastAPI gateway to RMDB. Also serves the frontend SPA."""
import os
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse

from api.query import router as query_router
from api.chat import router as chat_router
from api.metrics import router as metrics_router
from api.auth import router as auth_router
from api.practice import router as practice_router
from api.scenarios import router as scenarios_router
from api.data import router as data_router
from api.import_api import router as import_router
from api.workspace import router as workspace_router

app = FastAPI(title="DB Gateway")

# Initialize exercise tables on startup
from core.schema import init_exercise_tables

@app.on_event("startup")
def startup():
    n = init_exercise_tables()
    if n > 0:
        print(f"Created {n} new exercise tables")

# API routes
app.include_router(query_router, prefix="/api")
app.include_router(chat_router, prefix="/api")
app.include_router(metrics_router, prefix="/api")
app.include_router(auth_router, prefix="/api")
app.include_router(practice_router, prefix="/api")
app.include_router(scenarios_router, prefix="/api")
app.include_router(data_router, prefix="/api")
app.include_router(import_router, prefix="/api")
app.include_router(workspace_router, prefix="/api")

# Serve frontend static files
FRONTEND_DIR = os.path.join(os.path.dirname(__file__), "..", "frontend", "dist")
if os.path.isdir(FRONTEND_DIR):
    app.mount("/assets", StaticFiles(directory=os.path.join(FRONTEND_DIR, "assets")), name="assets")

    @app.get("/{full_path:path}")
    async def serve_frontend(full_path: str = ""):
        """Serve frontend SPA — fallback to index.html for client-side routing."""
        # Try exact file match first
        file_path = os.path.join(FRONTEND_DIR, full_path) if full_path else ""
        if full_path and os.path.isfile(file_path):
            return FileResponse(file_path)
        # SPA fallback
        index_path = os.path.join(FRONTEND_DIR, "index.html")
        if os.path.isfile(index_path):
            return FileResponse(index_path)
        return {"message": "Frontend not built. Run: cd frontend && npm run build"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
