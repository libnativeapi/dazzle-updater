package dazzleupdater_test

import (
	"crypto/ed25519"
	"encoding/base64"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"runtime"
	"sync"
	"testing"

	dazzleupdater "github.com/fastforgedev/dazzle/bindings/go"
)

// These tests share native singletons; do not run them with t.Parallel.
func TestConfigurationAndActionErrors(t *testing.T) {
	u := dazzleupdater.UpdaterInstance()
	s := dazzleupdater.ReleaseIndexSourceInstance()
	if u != dazzleupdater.UpdaterInstance() || s != dazzleupdater.ReleaseIndexSourceInstance() {
		t.Fatal("instance accessors must return the same wrappers")
	}
	t.Cleanup(func() {
		u.SetSource("")
		u.SetCurrentVersion("")
		u.SetPublicKey("")
		s.SetURL("")
	})
	u.SetSource("")
	if got := u.Source(); got != "release-index" {
		t.Fatalf("default source = %q", got)
	}
	for _, value := range []string{"https://example.com/releases.json", "https://例子.example/更新.json", ""} {
		s.SetURL(value)
		if got := s.URL(); got != value {
			t.Fatalf("URL = %q, want %q", got, value)
		}
	}
	s.SetURL("https://example.com/index.json\x00ignored")
	if got := s.URL(); got != "https://example.com/index.json" {
		t.Fatalf("NUL-terminated URL = %q", got)
	}
	s.SetURL("")
	u.SetCurrentVersion("1.4.0")
	if got := u.CurrentVersion(); got != "1.4.0" {
		t.Fatalf("current version = %q", got)
	}
	for _, value := range []string{"11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=", ""} {
		u.SetPublicKey(value)
		if got := u.PublicKey(); got != value {
			t.Fatalf("public key = %q, want %q", got, value)
		}
	}
	if u.IsUpdateAvailable() || u.IsUpdateDownloaded() || u.UpdateVersion() != "" || u.UpdateSize() != 0 {
		t.Fatal("unexpected initial update state")
	}
	u.CancelDownload()
	if got := u.DownloadProgress(); got != 0 {
		t.Fatalf("initial progress = %v", got)
	}
	if u.DownloadUpdate() || u.ErrorCode() != "no-update" {
		t.Fatalf("download without a check: %s: %s", u.ErrorCode(), u.ErrorMessage())
	}
	if u.InstallUpdate() || u.ErrorCode() != "no-update" {
		t.Fatalf("install without a download: %s: %s", u.ErrorCode(), u.ErrorMessage())
	}
	u.SetSource("missing-source")
	if u.Source() != "missing-source" || u.CheckForUpdates() {
		t.Fatal("unknown source should be accepted as configuration but fail its check")
	}
	// Linux builds without libcurl intentionally have no HTTP implementation.
	if u.ErrorCode() == "unsupported-platform" {
		t.Skip("native core has no update-check implementation")
	}
	if u.ErrorCode() != "unknown-source" || u.ErrorMessage() == "" {
		t.Fatalf("unknown source: %s: %s", u.ErrorCode(), u.ErrorMessage())
	}
	u.SetSource("")
	if u.CheckForUpdates() || u.ErrorCode() != "invalid-index" {
		t.Fatalf("missing URL: %s: %s", u.ErrorCode(), u.ErrorMessage())
	}
}

func TestSignedDownload(t *testing.T) {
	platform := map[string]string{"darwin": "macos", "linux": "linux", "windows": "windows"}[runtime.GOOS]
	if platform == "" {
		t.Skip("no desktop HTTP implementation on this platform")
	}
	payload := []byte("a signed Go binding test payload; never installed")
	publicKey, privateKey, err := ed25519.GenerateKey(nil)
	if err != nil {
		t.Fatal(err)
	}
	signature := base64.StdEncoding.EncodeToString(ed25519.Sign(privateKey, payload))
	// Every process gets a distinct cache filename, including concurrent test runs.
	assetPath := "/go-binding-" + base64.RawURLEncoding.EncodeToString(publicKey) + ".zip"
	mux := http.NewServeMux()
	server := httptest.NewServer(mux)
	defer server.Close()
	mux.HandleFunc(assetPath, func(w http.ResponseWriter, r *http.Request) {
		_, _ = w.Write(payload)
	})
	const announcedSize int64 = 1<<33 + 123 // Exercise a non-zero value beyond 32 bits.
	mux.HandleFunc("/releases.json", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(map[string]any{
			"schema": 1,
			"releases": []any{map[string]any{
				"version": "2.0.0", "displayVersion": "第二版", "releaseNotes": "## 更新\n\nGo 支持",
				"releaseNotesUrl": server.URL + "/notes",
				"assets": []any{map[string]any{
					"platform": platform, "arch": "universal", "format": "zip",
					"url": server.URL + assetPath, "size": announcedSize, "signature": signature,
				}},
			}},
		})
	})
	u := dazzleupdater.UpdaterInstance()
	s := dazzleupdater.ReleaseIndexSourceInstance()
	u.SetSource("")
	u.SetCurrentVersion("1.0.0")
	u.SetPublicKey(base64.StdEncoding.EncodeToString(publicKey))
	s.SetURL(server.URL + "/releases.json")
	t.Cleanup(func() {
		// Starting a new check clears the last result and download state.
		s.SetURL("")
		u.CheckForUpdates()
		u.SetPublicKey("")
		u.SetCurrentVersion("")
	})
	if !u.CheckForUpdates() {
		if u.ErrorCode() == "unsupported-platform" {
			t.Skip("native core has no update-check implementation")
		}
		t.Fatalf("check: %s: %s", u.ErrorCode(), u.ErrorMessage())
	}
	if !u.IsUpdateAvailable() || u.UpdateVersion() != "2.0.0" || u.UpdateDisplayVersion() != "第二版" ||
		u.UpdateReleaseNotes() != "## 更新\n\nGo 支持" || u.UpdateReleaseNotesURL() != server.URL+"/notes" ||
		u.UpdateSize() != announcedSize || u.ErrorCode() != "" || u.ErrorMessage() != "" {
		t.Fatal("release metadata did not cross the C ABI correctly")
	}
	if !u.DownloadUpdate() {
		t.Fatalf("download: %s: %s", u.ErrorCode(), u.ErrorMessage())
	}
	if !u.IsUpdateDownloaded() || u.DownloadProgress() != 1 {
		t.Fatal("signed download did not finish")
	}
	// A new check clears the download; a different key must reject the same payload.
	if !u.CheckForUpdates() || u.IsUpdateDownloaded() {
		t.Fatal("check did not clear the downloaded state")
	}
	u.SetPublicKey(base64.StdEncoding.EncodeToString(make([]byte, ed25519.PublicKeySize)))
	if u.DownloadUpdate() || u.ErrorCode() != "signature" || u.IsUpdateDownloaded() {
		t.Fatalf("wrong key: %s: %s", u.ErrorCode(), u.ErrorMessage())
	}
	u.SetCurrentVersion("2.0.0")
	if !u.CheckForUpdates() || u.IsUpdateAvailable() || u.UpdateVersion() != "" || u.UpdateSize() != 0 {
		t.Fatal("up-to-date check did not clear release metadata")
	}
}

func TestUpdaterUIConfiguration(t *testing.T) {
	ui := dazzleupdater.UpdaterUIInstance()
	if ui != dazzleupdater.UpdaterUIInstance() {
		t.Fatal("UI instance accessor returned a different wrapper")
	}
	// These settings are in-memory; do not change the user's persisted preferences
	// or open native windows from a Go test process without a UI event loop.
	name, allowed := ui.AppName(), ui.IsAutomaticDownloadsAllowed()
	t.Cleanup(func() {
		ui.SetAppName(name)
		ui.SetAutomaticDownloadsAllowed(allowed)
	})
	ui.SetAppName("示例 Go App")
	if ui.AppName() != "示例 Go App" || ui.IsVisible() {
		t.Fatal("UI name or visibility did not round-trip")
	}
	ui.SetAutomaticDownloadsAllowed(false)
	if ui.IsAutomaticDownloadsAllowed() || ui.IsAutomaticDownloadsEnabled() {
		t.Fatal("automatic downloads should be disallowed")
	}
	ui.SetAutomaticDownloadsAllowed(true)
	if !ui.IsAutomaticDownloadsAllowed() {
		t.Fatal("automatic downloads should be allowed")
	}
}

func TestConcurrentConfiguration(t *testing.T) {
	u := dazzleupdater.UpdaterInstance()
	s := dazzleupdater.ReleaseIndexSourceInstance()
	t.Cleanup(func() { s.SetURL("") })
	var workers sync.WaitGroup
	for i := 0; i < 8; i++ {
		workers.Add(1)
		go func() {
			defer workers.Done()
			for j := 0; j < 100; j++ {
				s.SetURL("https://例子.example/更新.json")
				if got := s.URL(); got != "https://例子.example/更新.json" {
					t.Errorf("concurrent URL = %q", got)
				}
				_ = u.DownloadProgress()
			}
		}()
	}
	workers.Wait()
}
