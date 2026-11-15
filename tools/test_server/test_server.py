#!/usr/bin/env python3
"""HTTP contract tests plus real core integration tests. Run: make test-server-test."""

import argparse
import base64
import contextlib
import http.client
import json
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import Request, urlopen

import server


@contextlib.contextmanager
def running(fixture, **kwargs):
    with server.TestServer(("127.0.0.1", 0), fixture, **kwargs) as httpd:
        thread = threading.Thread(target=httpd.serve_forever, kwargs={"poll_interval": 0.02})
        thread.start()
        try:
            yield httpd
        finally:
            httpd.stopping.set()
            httpd.shutdown()
            thread.join(timeout=5)


class HttpTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.payload = b"signed package bytes\n" * 8192
        self.package = self.root / "package with spaces.zip"
        self.package.write_bytes(self.payload)
        self.key = base64.b64encode(bytes(range(32))).decode()
        self.document = {"schema": 1, "releases": [{
            "version": "2.0.0", "releaseNotes": "# Notes", "assets": [{
                "platform": "macos", "arch": "universal", "format": "zip",
                "url": "http://old-host/package%20with%20spaces.zip",
                "signature": "signature", "size": len(self.payload),
            }],
        }]}
        self.feed = self.root / "releases.json"
        self.feed.write_text(json.dumps(self.document), encoding="utf-8")
        self.fixture = server.Fixture.from_feed(self.feed, self.key)
        self.logs = patch.object(server.Handler, "log_message")
        self.logs.start()
        self.addCleanup(self.logs.stop)
        context = running(self.fixture, index_delay=0.05, download_seconds=0.05)
        self.httpd = context.__enter__()
        self.addCleanup(context.__exit__, None, None, None)
        self.base = self.httpd.base_url

    def fetch(self, path):
        return urlopen(self.base + path, timeout=5)

    def index(self, scenario):
        with self.fetch(f"/scenarios/{scenario}/releases.json") as response:
            return json.load(response)

    def asset_url(self, scenario):
        return self.index(scenario)["releases"][0]["assets"][0]["url"]

    def test_console_and_configuration(self):
        with self.fetch("/") as response:
            self.assertIn("text/html", response.headers["Content-Type"])
            self.assertIn(b"/api/scenario", response.read())
        with self.fetch("/api/state") as response:
            self.assertEqual(response.headers["Cache-Control"], "no-store")
            state = json.load(response)
        self.assertEqual(state["publicKey"], self.key)
        self.assertEqual(len(state["scenarios"]), len(server.SCENARIOS))

    def test_switch_scenario_and_preserve_checked_download(self):
        with self.fetch("/releases.json") as response:
            checked = json.load(response)
        request = Request(self.base + "/api/scenario", data=b'{"scenario":"download-500"}',
                          headers={"Content-Type": "application/json"})
        with urlopen(request, timeout=5) as response:
            self.assertEqual(json.load(response)["scenario"], "download-500")
        with self.fetch("/releases.json") as response:
            new = json.load(response)
        old_url = checked["releases"][0]["assets"][0]["url"]
        self.assertIn("/scenarios/update/", old_url)
        with urlopen(old_url, timeout=5) as response:
            self.assertEqual(response.read(), self.payload)
        with self.assertRaises(HTTPError) as raised:
            urlopen(new["releases"][0]["assets"][0]["url"], timeout=5)
        self.assertEqual(raised.exception.code, 500)
        raised.exception.close()

    def test_bad_control_requests(self):
        for body in (b"{}", b"[]", b"null", b"x", b'{"scenario":[]}', b'{"scenario":"nope"}'):
            with self.subTest(body=body), self.assertRaises(HTTPError) as raised:
                urlopen(Request(self.base + "/api/scenario", data=body), timeout=5)
            self.assertEqual(raised.exception.code, 400)
            raised.exception.close()
        with self.assertRaises(HTTPError) as raised:
            urlopen(Request(self.base + "/api/scenario", data=b'{"scenario":"update"}',
                            headers={"Origin": "https://other.example"}), timeout=5)
        self.assertEqual(raised.exception.code, 403)
        raised.exception.close()

    def test_private_files_and_unknown_routes_are_not_served(self):
        (self.root / "secret.txt").write_text("private")
        for route in ("/secret.txt", "/../secret.txt", "/scenarios/unknown/releases.json",
                      "/scenarios/update/assets/999/file.zip"):
            with self.subTest(route=route), self.assertRaises(HTTPError) as raised:
                self.fetch(route)
            self.assertEqual(raised.exception.code, 404)
            raised.exception.close()

    def test_feed_cannot_escape_its_directory(self):
        asset = self.document["releases"][0]["assets"][0]
        for path in ("/../outside.zip", "/%2e%2e/outside.zip"):
            asset["url"] = path
            self.feed.write_text(json.dumps(self.document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "inside"):
                server.Fixture.from_feed(self.feed, self.key)

        with tempfile.TemporaryDirectory() as outside:
            target = Path(outside) / "private.zip"
            target.write_bytes(b"private")
            (self.root / "linked.zip").symlink_to(target)
            asset["url"] = "/linked.zip"
            self.feed.write_text(json.dumps(self.document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "inside"):
                server.Fixture.from_feed(self.feed, self.key)

    def test_malformed_base_feed_has_actionable_error(self):
        for document in ([], {}, {"schema": 1, "releases": [None]},
                         {"schema": 1, "releases": [{"assets": [None]}]}):
            self.feed.write_text(json.dumps(document), encoding="utf-8")
            with self.subTest(document=document), self.assertRaises(ValueError):
                server.Fixture.from_feed(self.feed, self.key)

    def test_metadata_scenarios(self):
        self.assertEqual(self.index("no-update")["releases"], [])
        self.assertEqual(self.index("older-version")["releases"][0]["version"], "0.0.1")
        self.assertEqual(self.index("unsupported-schema")["schema"], 999)
        release = self.index("no-notes")["releases"][0]
        self.assertNotIn("releaseNotes", release)
        self.assertGreater(len(self.index("long-notes")["releases"][0]["releaseNotes"]), 4000)
        for scenario, field, value in (
            ("no-compatible-asset", "platform", "unmatched-platform"),
            ("minimum-system-version", "minimumSystemVersion", "9999.0"),
            ("unsupported-format", "format", "unsupported-test-format"),
        ):
            self.assertEqual(self.index(scenario)["releases"][0]["assets"][0][field], value)

    def test_invalid_json(self):
        with self.fetch("/scenarios/invalid-json/releases.json") as response:
            with self.assertRaises(ValueError):
                json.load(response)

    def test_http_errors(self):
        for scenario in ("index-404", "index-500", "download-404", "download-500"):
            url = (self.base + f"/scenarios/{scenario}/releases.json" if scenario.startswith("index-")
                   else self.asset_url(scenario))
            with self.subTest(scenario=scenario), self.assertRaises(HTTPError) as raised:
                urlopen(url, timeout=5)
            self.assertEqual(raised.exception.code, int(scenario[-3:]))
            raised.exception.close()

    def test_real_bytes_and_tampering(self):
        for scenario in ("update", "bad-signature", "tampered-package", "slow-download", "unknown-length"):
            with self.subTest(scenario=scenario), urlopen(self.asset_url(scenario), timeout=5) as response:
                self.assertEqual(response.headers.get("Content-Length"),
                                 None if scenario == "unknown-length" else str(len(self.payload)))
                body = response.read()
                if scenario == "tampered-package":
                    self.assertNotEqual(body, self.payload)
                    self.assertEqual(body[1:], self.payload[1:])
                else:
                    self.assertEqual(body, self.payload)

    def test_interrupted_download(self):
        with urlopen(self.asset_url("interrupted-download"), timeout=5) as response:
            with self.assertRaises(http.client.IncompleteRead) as raised:
                response.read()
            self.assertEqual(len(raised.exception.partial), len(self.payload) // 2)

    def test_redirect_from_both_index_routes_and_asset(self):
        self.httpd.scenario = "redirect"
        for route in ("/releases.json", "/scenarios/redirect/releases.json"):
            with self.fetch(route) as response:
                self.assertTrue(response.url.endswith("/resolved.json"))
                index = json.load(response)
            with urlopen(index["releases"][0]["assets"][0]["url"], timeout=5) as response:
                self.assertIn("/resolved-assets/", response.url)
                self.assertEqual(response.read(), self.payload)

    def test_remote_base_url(self):
        with running(self.fixture, base_url="http://192.0.2.1:8765") as httpd:
            index = self.fixture.index(httpd.base_url, "update")
            self.assertTrue(index["releases"][0]["assets"][0]["url"].startswith(httpd.base_url))


class CoreIntegrationTests(unittest.TestCase):
    def test_every_scenario_through_real_core(self):
        signer = server.ROOT / "core/build/tests/fixture_signer"
        updater = server.ROOT / "core/build/examples/updater_example/updater_example"
        if not signer.is_file() or not updater.is_file():
            self.skipTest("build fixture_signer and updater_example with make test-server-test")
        # The generator uses the actual host's platform/architecture, as does Updater.
        args = argparse.Namespace(signer=signer, package=None, version="2.0.0", format="zip",
                                  platform={"Darwin": "macos", "Windows": "windows"}.get(
                                      server.platform.system(), "linux"),
                                  arch="universal", installer_arguments="")
        with tempfile.TemporaryDirectory() as directory, patch.object(server.Handler, "log_message"):
            fixture = server.Fixture.generate(args, Path(directory))
            with running(fixture, index_delay=0.02, download_seconds=0.05) as httpd:
                errors = {"invalid-json": "invalid-index", "unsupported-schema": "invalid-index",
                          "index-404": "network", "index-500": "network",
                          "download-404": "network", "download-500": "network",
                          "interrupted-download": "network", "bad-signature": "signature",
                          "tampered-package": "signature", "unsupported-format": "installer"}
                no_update = {"no-update", "older-version", "no-compatible-asset", "minimum-system-version"}
                for scenario in server.SCENARIOS:
                    with self.subTest(scenario=scenario):
                        command = [str(updater), f"{httpd.base_url}/scenarios/{scenario}/releases.json",
                                   fixture.public_key, "1.0.0"]
                        if scenario == "unsupported-format":
                            command.append("--install")
                        result = subprocess.run(command, capture_output=True, text=True, timeout=15)
                        output = result.stdout + result.stderr
                        if scenario in errors:
                            self.assertEqual(result.returncode, 1, output)
                            self.assertIn(f"failed: {errors[scenario]}:", output)
                        else:
                            self.assertEqual(result.returncode, 0, output)
                            self.assertIn("up to date" if scenario in no_update
                                          else "downloaded and signature verified", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
