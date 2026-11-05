package main

import (
	"bytes"
	"crypto/ed25519"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"runtime"
	"strconv"
	"time"

	"github.com/egoist/mygo"
)

// Exercise the same actions as the buttons in a real MyGo window. This fixture
// is only used with --smoke; it never calls InstallUpdate or changes preferences.
func startSmoke(d *demo, result *error) (func(), error) {
	platform := map[string]string{"darwin": "macos", "linux": "linux", "windows": "windows"}[runtime.GOOS]
	if platform == "" {
		return nil, fmt.Errorf("smoke check requires a desktop platform")
	}
	publicKey, secretKey, err := ed25519.GenerateKey(nil)
	if err != nil {
		return nil, err
	}
	payload := bytes.Repeat([]byte("dazzle updater / MyGo smoke fixture\n"), 8192)
	assetPath := "/mygo-smoke-" + base64.RawURLEncoding.EncodeToString(publicKey) + ".zip"
	signature := base64.StdEncoding.EncodeToString(ed25519.Sign(secretKey, payload))
	mux := http.NewServeMux()
	server := httptest.NewServer(mux)
	mux.HandleFunc(assetPath, func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Length", strconv.Itoa(len(payload)))
		// Keep the transfer visible long enough to exercise progress rendering.
		for offset := 0; offset < len(payload); offset += 16384 {
			end := min(offset+16384, len(payload))
			if _, err := w.Write(payload[offset:end]); err != nil {
				return
			}
			w.(http.Flusher).Flush()
			select {
			case <-r.Context().Done():
				return
			case <-time.After(20 * time.Millisecond):
			}
		}
	})
	mux.HandleFunc("/releases.json", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(map[string]any{
			"schema": 1,
			"releases": []any{map[string]any{
				"version": "2.0.0", "displayVersion": "2.0",
				"releaseNotes": "## What's new\n\n- **Go support** with a MyGo interface.\n- Signed downloads and live progress.\n- Unicode: 更新就绪。",
				"assets": []any{map[string]any{
					"platform": platform, "arch": "universal", "format": "zip",
					"url": server.URL + assetPath, "size": len(payload), "signature": signature,
				}},
			}},
		})
	})
	d.config = configuration{
		feed: server.URL + "/releases.json", version: "1.0.0",
		publicKey: base64.StdEncoding.EncodeToString(publicKey),
	}
	d.status = "Running the local smoke check…"
	d.invalidate()
	*result = fmt.Errorf("smoke check was closed before completion")
	finished, stage := false, 0
	fail := func(message string) {
		finished = true
		*result = fmt.Errorf("smoke check: %s (%s)", message, d.status)
		mygo.App.Quit()
	}
	d.afterAction = func(completed action, ok bool) {
		if finished {
			return
		}
		if !d.win.IsVisible() {
			fail("window is not visible")
			return
		}
		switch stage {
		case 0:
			if completed != check || !ok || !d.available || d.release != "2.0" || d.notes == "" || d.size != int64(len(payload)) {
				fail("release metadata was not displayed")
				return
			}
			stage++
			d.start(download)
		case 1:
			if completed != download || !ok || !d.downloaded || d.progress != 1 {
				fail("signed download failed")
				return
			}
			stage++
			d.config.publicKey = base64.StdEncoding.EncodeToString(make([]byte, ed25519.PublicKeySize))
			d.start(check)
		case 2:
			if completed != check || !ok || !d.available || d.downloaded {
				fail("recheck did not clear the downloaded state")
				return
			}
			stage++
			d.start(download)
		case 3:
			if completed != download || ok || d.downloaded || d.updater.ErrorCode() != "signature" {
				fail("invalid signature was accepted")
				return
			}
			stage++
			d.config.version = "2.0.0"
			d.start(check)
		case 4:
			if completed != check || !ok || d.available || d.downloaded || d.notes != "" {
				fail("up-to-date check did not clear the previous release")
				return
			}
			finished = true
			*result = nil
			mygo.App.Quit()
		}
	}
	start := time.AfterFunc(time.Second, func() {
		mygo.RunOnMain(func() {
			if !finished && !d.closed {
				d.start(check)
			}
		})
	})
	timeout := time.AfterFunc(20*time.Second, func() {
		mygo.RunOnMain(func() {
			if !finished && !d.closed {
				fail("timed out")
			}
		})
	})
	return func() {
		start.Stop()
		timeout.Stop()
		server.Close()
	}, nil
}
