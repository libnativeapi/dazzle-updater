#!/usr/bin/env python3
"""A local release-index server with repeatable updater failure scenarios (stdlib only)."""

import argparse
import base64
import copy
import json
import math
import platform
import subprocess
import tempfile
import threading
import time
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit


SCENARIOS = {
    "update": "有更新：下载真实文件并验证 Ed25519 签名；使用 demo 或自己的安装包可继续安装。",
    "no-update": "已是最新：返回空的 releases。",
    "older-version": "仅有旧版本：0.0.1 不应更新当前 1.0.0。",
    "no-compatible-asset": "没有适用安装包：所有资源的平台均不匹配。",
    "minimum-system-version": "系统版本过低：安装包要求系统 9999.0。",
    "invalid-json": "索引 JSON 损坏：检查应报告 invalid-index。",
    "unsupported-schema": "不支持的 schema：检查应报告 invalid-index。",
    "index-404": "索引 HTTP 404：检查应报告 network。",
    "index-500": "索引 HTTP 500：检查应报告 network。",
    "slow-index": "延迟返回索引：观察 Checking 窗口。",
    "download-404": "安装包 HTTP 404：下载应报告 network。",
    "download-500": "安装包 HTTP 500：下载应报告 network。",
    "slow-download": "慢速下载：观察进度，也可取消后重新检查、下载。",
    "unknown-length": "慢速下载但无 Content-Length：观察不确定进度。",
    "interrupted-download": "传输中断：仅发送一半文件，下载应报告 network。",
    "bad-signature": "错误的签名：完整下载后应报告 signature。",
    "tampered-package": "安装包被篡改：修改一个字节，下载后应报告 signature。",
    "redirect": "索引与安装包均经 HTTP 302 跳转，下载校验应成功。",
    "unsupported-format": "签名有效但格式不支持：点击安装应报告 installer。",
    "no-notes": "无更新说明：观察精简的 Available 窗口。",
    "long-notes": "很长的 Markdown 更新说明：观察滚动、链接与排版。",
}

ROOT = Path(__file__).resolve().parents[2]


class Fixture:
    def __init__(self, document, assets, public_key="", installable=False):
        self.document = document
        self.assets = assets
        self.public_key = public_key
        self.installable = installable

    @classmethod
    def from_feed(cls, feed, public_key):
        """Map only files named by this feed, never expose the directory itself."""
        document = json.loads(feed.read_text(encoding="utf-8"))
        if (not isinstance(document, dict) or document.get("schema") != 1
                or not isinstance(document.get("releases"), list) or not document["releases"]):
            raise ValueError("--feed must contain schema 1 and at least one release")
        assets = {}
        for release in document["releases"]:
            if not isinstance(release, dict) or not isinstance(release.get("assets"), list):
                raise ValueError("each --feed release must contain an assets array")
            for asset in release["assets"]:
                if not isinstance(asset, dict) or not isinstance(asset.get("url"), str):
                    raise ValueError("each --feed asset must contain a URL string")
                relative = unquote(urlsplit(asset["url"]).path).lstrip("/")
                path = (feed.parent / relative).resolve()
                if not path.is_relative_to(feed.parent.resolve()) or not path.is_file():
                    raise ValueError(f"feed asset must be a file inside {feed.parent}: {relative}")
                asset_id = str(len(assets))
                assets[asset_id] = path
                asset["url"] = asset_id
        if not assets:
            raise ValueError("--feed must contain at least one asset")
        return cls(document, assets, public_key, installable=True)

    @classmethod
    def generate(cls, args, directory):
        signer = args.signer.resolve()
        if not signer.is_file():
            raise ValueError(f"no fixture_signer at {signer}; run make test-server first, or use --feed")
        keys = dict(line.split(" ", 1) for line in subprocess.check_output(
            [str(signer), "gen-key"], text=True).splitlines())
        package = args.package
        if package is None:
            package = directory / "download-fixture.zip"
            # Stored rather than compressed: a small fixture with observable progress.
            with zipfile.ZipFile(package, "w", compression=zipfile.ZIP_STORED) as archive:
                archive.writestr("README.txt", "Download/signature fixture only; not an application.\n")
                archive.writestr("payload.bin", b"dazzle updater test fixture\n" * 160000)
        package = package.resolve(strict=True)
        if not package.is_file():
            raise ValueError(f"--package must be a file: {package}")
        signature = subprocess.check_output(
            [str(signer), "sign", keys["secret"], str(package)], text=True).strip()
        document = {"schema": 1, "releases": [{
            "version": args.version,
            "displayVersion": args.version,
            "releaseNotes": "## What's new\n\n- **Signed updates** from a real HTTP server.\n"
                            "- Check download progress, cancellation and failures.\n"
                            "\n### 测试说明\n\n在测试控制台切换场景后，重新检查更新。\n",
            "assets": [{
                "platform": args.platform, "arch": args.arch,
                "url": "0", "size": package.stat().st_size,
                "format": args.format, "signature": signature,
                "installerArguments": args.installer_arguments,
            }],
        }]}
        return cls(document, {"0": package}, keys["public"], args.package is not None)

    def index(self, base_url, scenario):
        document = copy.deepcopy(self.document)
        if scenario == "no-update":
            document["releases"] = []
        if scenario == "unsupported-schema":
            document["schema"] = 999
        for release in document["releases"]:
            if scenario == "older-version":
                release["version"] = release["displayVersion"] = "0.0.1"
            if scenario == "no-notes":
                release.pop("releaseNotes", None)
                release.pop("releaseNotesUrl", None)
            if scenario == "long-notes":
                release["releaseNotes"] = "# Release notes\n\n" + "\n\n".join(
                    f"## Change {i}\n\n- **改进**下载与校验。\n- A [link](https://example.com).\n"
                    "\n```text\ndazzle updater --version\n```" for i in range(1, 41))
            for asset in release["assets"]:
                asset_id = asset["url"]
                name = quote(self.assets[asset_id].name, safe="")
                asset["url"] = f"{base_url}/scenarios/{scenario}/assets/{asset_id}/{name}"
                if scenario == "no-compatible-asset":
                    asset["platform"] = "unmatched-platform"
                if scenario == "minimum-system-version":
                    asset["minimumSystemVersion"] = "9999.0"
                if scenario == "bad-signature":
                    asset["signature"] = base64.b64encode(bytes(64)).decode("ascii")
                if scenario == "unsupported-format":
                    asset["format"] = "unsupported-test-format"
        return document


class TestServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, fixture, scenario="update", base_url=None,
                 index_delay=3.0, download_seconds=8.0):
        super().__init__(address, Handler)
        self.fixture = fixture
        self.scenario = scenario
        host, port = self.server_address
        self.base_url = base_url or f"http://{'127.0.0.1' if host == '0.0.0.0' else host}:{port}"
        self.index_delay = index_delay
        self.download_seconds = download_seconds
        self.stopping = threading.Event()

    def state(self):
        scenario = self.scenario
        return {"scenario": scenario, "feed": f"{self.base_url}/releases.json",
                "publicKey": self.fixture.public_key,
                "packageProvided": self.fixture.installable,
                "scenarios": [{"name": name, "description": description,
                               "feed": f"{self.base_url}/scenarios/{name}/releases.json"}
                              for name, description in SCENARIOS.items()]}

    def server_close(self):
        self.stopping.set()
        super().server_close()


class Handler(BaseHTTPRequestHandler):
    def reply(self, status, body, content_type="application/json; charset=utf-8", headers=None):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        for name, value in (headers or {}).items():
            self.send_header(name, value)
        self.end_headers()
        self.wfile.write(body)

    def json_reply(self, status, value):
        self.reply(status, json.dumps(value, ensure_ascii=False).encode("utf-8"))

    def do_GET(self):
        try:
            self.get()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            # A client cancelling a download is an expected part of this server.
            pass

    def get(self):
        path = urlsplit(self.path).path
        if path == "/":
            self.reply(200, Path(__file__).with_name("index.html").read_bytes(),
                       "text/html; charset=utf-8")
            return
        if path == "/api/state":
            self.json_reply(200, self.server.state())
            return
        parts = path.strip("/").split("/")
        if path == "/releases.json":
            scenario, resource = self.server.scenario, ["releases.json"]
        elif len(parts) >= 3 and parts[0] == "scenarios" and parts[1] in SCENARIOS:
            scenario, resource = parts[1], parts[2:]
        else:
            self.json_reply(404, {"error": "unknown route"})
            return
        if resource == ["releases.json"]:
            if scenario == "slow-index" and self.server.stopping.wait(self.server.index_delay):
                return
            if scenario in ("index-404", "index-500"):
                self.json_reply(int(scenario[-3:]), {"error": scenario})
            elif scenario == "invalid-json":
                self.reply(200, b'{"schema": 1, "releases": [')
            elif scenario == "redirect":
                self.reply(302, b"", headers={"Location": "/scenarios/redirect/resolved.json"})
            else:
                self.json_reply(200, self.server.fixture.index(self.server.base_url, scenario))
        elif scenario == "redirect" and resource == ["resolved.json"]:
            self.json_reply(200, self.server.fixture.index(self.server.base_url, scenario))
        elif (len(resource) == 3 and resource[0] in ("assets", "resolved-assets")
              and resource[1] in self.server.fixture.assets):
            if scenario == "redirect" and resource[0] == "assets":
                self.reply(302, b"", headers={"Location": path.replace("/assets/", "/resolved-assets/", 1)})
            else:
                self.send_asset(scenario, self.server.fixture.assets[resource[1]])
        else:
            self.json_reply(404, {"error": "unknown resource"})

    def send_asset(self, scenario, path):
        if scenario in ("download-404", "download-500"):
            self.json_reply(int(scenario[-3:]), {"error": scenario})
            return
        size = path.stat().st_size
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Cache-Control", "no-store")
        if scenario != "unknown-length":
            self.send_header("Content-Length", str(size))
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        remaining = size // 2 if scenario == "interrupted-download" else size
        chunk_size = max(1, min(65536, math.ceil(size / 64)))
        first = True
        start = time.monotonic()
        sent = 0
        with path.open("rb") as file:
            while remaining and not self.server.stopping.is_set():
                chunk = file.read(min(chunk_size, remaining))
                if not chunk:
                    break
                if first and scenario == "tampered-package":
                    chunk = bytes([chunk[0] ^ 1]) + chunk[1:]
                first = False
                self.wfile.write(chunk)
                self.wfile.flush()
                sent += len(chunk)
                remaining -= len(chunk)
                if scenario in ("slow-download", "unknown-length"):
                    due = start + self.server.download_seconds * sent / max(1, size)
                    self.server.stopping.wait(max(0, due - time.monotonic()))

    def do_POST(self):
        if self.path != "/api/scenario":
            self.json_reply(404, {"error": "unknown route"})
            return
        # The browser console is same-origin; reject requests from other websites.
        if self.headers.get("Origin") not in (None, f"http://{self.headers.get('Host')}",
                                              self.server.base_url):
            self.json_reply(403, {"error": "cross-origin request"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 4096:
                raise ValueError("expected a small JSON body")
            value = json.loads(self.rfile.read(length))
            scenario = value["scenario"]
            if not isinstance(scenario, str) or scenario not in SCENARIOS:
                raise ValueError("unknown scenario")
        except (ValueError, KeyError, TypeError):
            self.json_reply(400, {"error": "expected {\"scenario\": \"update\"} with a known scenario"})
            return
        self.server.scenario = scenario
        self.json_reply(200, self.server.state())


def nonnegative(value):
    number = float(value)
    if not math.isfinite(number) or number < 0:
        raise argparse.ArgumentTypeError("must be finite and nonnegative")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--feed", type=Path, help="existing signed index; asset URL paths must exist beside it")
    source.add_argument("--package", type=Path, help="sign and serve your real zip/exe/msi instead of a download fixture")
    parser.add_argument("--signer", type=Path, default=ROOT / "core/build/tests/fixture_signer")
    parser.add_argument("--public-key", default="", help="public key for --feed, displayed in the console")
    parser.add_argument("--platform", choices=["macos", "windows", "linux", "ios", "android", "ohos"],
                        default={"Darwin": "macos", "Windows": "windows"}.get(platform.system(), "linux"))
    parser.add_argument("--arch", choices=["x64", "arm64", "x86", "universal"],
                        default={"x86_64": "x64", "amd64": "x64", "aarch64": "arm64"}.get(
                            platform.machine().lower(), platform.machine().lower()))
    parser.add_argument("--format", choices=["zip", "exe", "msi"], default="zip")
    parser.add_argument("--installer-arguments", default="")
    parser.add_argument("--version", default="2.0.0", help="offered version; set example's current version to 1.0.0")
    parser.add_argument("--scenario", choices=SCENARIOS, default="update")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765, help="0 selects a free port")
    parser.add_argument("--base-url", help="public origin for remote clients, e.g. http://192.168.1.2:8765")
    parser.add_argument("--index-delay", type=nonnegative, default=3.0)
    parser.add_argument("--download-seconds", type=nonnegative, default=8.0)
    parser.add_argument("--ready-file", type=Path, help="write console configuration JSON once listening")
    args = parser.parse_args()
    if args.base_url:
        url = urlsplit(args.base_url)
        if url.scheme not in ("http", "https") or not url.netloc or url.path not in ("", "/") or url.query or url.fragment:
            parser.error("--base-url must be an HTTP(S) origin without a path, query or fragment")
        args.base_url = args.base_url.rstrip("/")
    with tempfile.TemporaryDirectory(prefix="dazzle-test-server-") as directory:
        try:
            fixture = (Fixture.from_feed(args.feed.resolve(), args.public_key) if args.feed
                       else Fixture.generate(args, Path(directory)))
            server = TestServer((args.host, args.port), fixture, args.scenario, args.base_url,
                                args.index_delay, args.download_seconds)
        except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
            parser.exit(1, f"test server: {error}\n")
        with server:
            state = server.state()
            if args.ready_file:
                args.ready_file.write_text(json.dumps(state, ensure_ascii=False), encoding="utf-8")
            print(f"Console:    {server.base_url}/\nRelease URL: {state['feed']}\n"
                  f"Public key:  {fixture.public_key or '(use the key for your signed feed)'}\n"
                  f"Scenario:    {args.scenario}", flush=True)
            if not fixture.installable:
                print("Download fixture only. Use make demo / make demo-linux or --package for installation.", flush=True)
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                pass


if __name__ == "__main__":
    main()
