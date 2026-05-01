from pathlib import Path
import json

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="DTU Meter API", version="0.1.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

DATA_PATH = Path(__file__).resolve().parent.parent / "data" / "latest.json"
CONFIG_PATH = Path(__file__).resolve().parent.parent.parent / "dtu-core" / "config" / "config.json"


def load_json(path: Path):
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


@app.get("/api/health")
def health():
    return {"status": "ok"}


@app.get("/api/latest")
def latest():
    data = load_json(DATA_PATH)
    if data is None:
        return {
            "meter_id": "",
            "status": "empty",
            "timestamp": "",
            "data": {}
        }
    return data


@app.get("/api/config")
def config():
    data = load_json(CONFIG_PATH)
    if data is None:
        return {"status": "missing_config"}
    return data
