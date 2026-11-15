"""Loopback-only fixture; RFC 8032 test 2 signs the one-byte payload b'r'."""

import http.server
import json
import sys
import uuid


asset_path = "/swift-binding-" + uuid.uuid4().hex + ".zip"


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == asset_path:
            body = b"r"
        elif self.path == "/releases.json":
            base = "http://127.0.0.1:" + str(self.server.server_port)
            body = json.dumps({
                "schema": 1,
                "releases": [{
                    "version": "2.0.0", "displayVersion": "第二版",
                    "releaseNotes": "## 更新\n\nSwift 支持",
                    "releaseNotesUrl": base + "/notes",
                    "assets": [{
                        "platform": sys.argv[1], "arch": "universal", "format": "zip",
                        "url": base + asset_path, "size": (1 << 33) + 123,
                        "signature": "kqAJqfDUyrhyDoILX2QlQKKye1QWUD+Ps3YiI+vbadoIWsHkPhWZbkWPNhPQ8R2MOHsurrQwKu6wDSkWErsMAA==",
                    }],
                }],
            }).encode("utf-8")
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


with http.server.HTTPServer(("127.0.0.1", 0), Handler) as server:
    print(server.server_port, flush=True)
    server.serve_forever()
