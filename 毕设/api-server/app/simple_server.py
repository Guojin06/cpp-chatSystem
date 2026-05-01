from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import json

BASE_DIR = Path(__file__).resolve().parent.parent
DATA_PATH = BASE_DIR / "data" / "latest.json"
CONFIG_PATH = BASE_DIR.parent / "dtu-core" / "config" / "config.json"


def load_json(path: Path):
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


class RequestHandler(BaseHTTPRequestHandler):
    def _send_json(self, payload, status=200):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        if self.path == "/api/health":
            self._send_json({"status": "ok"})
            return

        if self.path == "/api/latest":
            data = load_json(DATA_PATH)
            if data is None:
                self._send_json({
                    "meter_id": "",
                    "status": "empty",
                    "timestamp": "",
                    "data": {}
                })
                return
            self._send_json(data)
            return

        if self.path == "/api/config":
            data = load_json(CONFIG_PATH)
            if data is None:
                self._send_json({"status": "missing_config"}, 404)
                return
            self._send_json(data)
            return

        self._send_json({"status": "not_found"}, 404)


def main():
    server = HTTPServer(("0.0.0.0", 8000), RequestHandler)
    print("simple api server running at http://0.0.0.0:8000")
    server.serve_forever()


if __name__ == "__main__":
    main()
