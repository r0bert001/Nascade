#!/usr/bin/env python3
"""本地 ScreenScraper 模拟服务，用于 M0 验证 HTTP/JSON 解析。

仅返回固定的平台/游戏数据，不会访问网络，也不包含任何真实凭据。
用法: python tools/mock_screenscraper.py [端口]
"""

import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PORT = 8765
COVER_PATH = Path(__file__).resolve().parent.parent / "packaging/stockos/Nascade.pak/icon.png"


def systems_body():
    return {
        "response": {
            "systemes": [
                {"id": 3, "nom": "Nintendo Entertainment System", "shortname": "NES"},
                {"id": 4, "nom": "Super Nintendo", "shortname": "SNES"},
                {"id": 12, "nom": "Game Boy Advance", "shortname": "GBA"},
            ],
            "ssuser": {"id": "mock", "numjour": "0"},
        }
    }


def game_body():
    return {
        "response": {
            "jeu": {
                "id": "12345",
                "noms": [
                    {"langue": "en", "text": "The Legend of Zelda: The Minish Cap"},
                    {"langue": "zh", "text": "塞尔达传说：缩小帽"},
                ],
                "synopsis": [
                    {"langue": "en", "text": "A Game Boy Advance adventure."},
                    {"langue": "zh", "text": "Game Boy Advance 上的塞尔达冒险。"},
                ],
                "developpeur": {"id": "1", "text": "Capcom"},
                "editeur": {"id": "2", "text": "Nintendo"},
                "dates": [
                    {"region": "jp", "text": "2004-11-04"},
                    {"region": "us", "text": "2005-01-10"},
                ],
                "genres": [
                    {
                        "id": "1",
                        "noms": [
                            {"langue": "zh", "text": "动作冒险"},
                            {"langue": "en", "text": "Action Adventure"},
                        ],
                    }
                ],
                "joueurs": {"text": "1"},
                "note": {"text": "9.1"},
                "medias": [
                    {
                        "type": "box-2D",
                        "region": "jp",
                        "url": f"http://127.0.0.1:{PORT}/media/cover.png",
                    },
                    {
                        "type": "ss",
                        "region": "jp",
                        "url": f"http://127.0.0.1:{PORT}/media/screen.png",
                    },
                ],
            }
        }
    }


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/media/"):
            if not COVER_PATH.is_file():
                self.send_error(404)
                return
            payload = COVER_PATH.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "image/png")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return

        if "jeuInfos.php" in self.path:
            body = game_body()
        elif "systemeListe.php" in self.path:
            body = systems_body()
        else:
            self.send_error(404)
            return

        payload = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt, *args):
        path = self.path.split("?", 1)[0]
        sys.stderr.write(f"mock-screenscraper: {self.command} {path}\n")


def main() -> int:
    global PORT
    if len(sys.argv) > 1:
        PORT = int(sys.argv[1])
    server = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    print(f"mock ScreenScraper listening on http://127.0.0.1:{PORT}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
