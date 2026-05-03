from pathlib import Path
import json
import sqlite3
from datetime import datetime, timedelta
from typing import List, Dict

from fastapi import FastAPI, Query
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="DTU Meter API", version="0.2.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

DATA_PATH = Path(__file__).resolve().parent.parent / "data" / "latest.json"
CONFIG_PATH = Path(__file__).resolve().parent.parent.parent / "dtu-core" / "config" / "config.json"
DB_PATH = Path(__file__).resolve().parent.parent.parent / "dtu-core" / "data" / "meter_readings.db"


def load_json(path: Path):
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def query_db(sql: str, params=()) -> List[Dict]:
    """通用数据库查询，返回字典列表"""
    if not DB_PATH.exists():
        return []
    conn = sqlite3.connect(str(DB_PATH))
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    cur.execute(sql, params)
    rows = cur.fetchall()
    conn.close()
    return [dict(row) for row in rows]


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


@app.get("/api/history")
def history(
    meter_id: str = Query(default=None, description="电表编号，可选"),
    limit: int = Query(default=100, ge=1, le=1000, description="返回条数"),
    start_time: str = Query(default=None, description="起始时间，格式 YYYY-MM-DD HH:MM:SS"),
    end_time: str = Query(default=None, description="结束时间，格式 YYYY-MM-DD HH:MM:SS"),
):
    """
    历史数据查询接口

    - 不带参数：返回最近 limit 条记录
    - 带 meter_id：仅查询指定电表
    - 带时间范围：查询 start_time 到 end_time 之间的记录

    示例：
    - GET /api/history?limit=10
    - GET /api/history?meter_id=000000000001&limit=50
    - GET /api/history?start_time=2026-05-01 00:00:00&end_time=2026-05-03 23:59:59
    """
    if start_time and end_time:
        # 时间范围查询
        sql = """
            SELECT id, meter_id, timestamp,
                   voltage, current, active_energy, power_factor,
                   uploaded, upload_retry
            FROM meter_records
            WHERE timestamp BETWEEN ? AND ?
            ORDER BY id DESC
            LIMIT ?
        """
        rows = query_db(sql, (start_time, end_time, limit))
    elif meter_id:
        # 按电表ID查询
        sql = """
            SELECT id, meter_id, timestamp,
                   voltage, current, active_energy, power_factor,
                   uploaded, upload_retry
            FROM meter_records
            WHERE meter_id = ?
            ORDER BY id DESC
            LIMIT ?
        """
        rows = query_db(sql, (meter_id, limit))
    else:
        # 默认：最近 N 条
        sql = """
            SELECT id, meter_id, timestamp,
                   voltage, current, active_energy, power_factor,
                   uploaded, upload_retry
            FROM meter_records
            ORDER BY id DESC
            LIMIT ?
        """
        rows = query_db(sql, (limit,))

    return {
        "count": len(rows),
        "data": rows
    }


@app.get("/api/stats")
def stats():
    """统计数据：总记录数、待上传数、上传成功率"""
    sql_total = "SELECT COUNT(*) as cnt FROM meter_records"
    sql_pending = "SELECT COUNT(*) as cnt FROM meter_records WHERE uploaded = 0"
    sql_uploaded = "SELECT COUNT(*) as cnt FROM meter_records WHERE uploaded = 1"

    total = query_db(sql_total)
    pending = query_db(sql_pending)
    uploaded = query_db(sql_uploaded)

    t = total[0]["cnt"] if total else 0
    p = pending[0]["cnt"] if pending else 0
    u = uploaded[0]["cnt"] if uploaded else 0
    rate = (u / t * 100) if t > 0 else 0

    return {
        "total": t,
        "pending": p,
        "uploaded": u,
        "upload_rate": round(rate, 2)
    }
