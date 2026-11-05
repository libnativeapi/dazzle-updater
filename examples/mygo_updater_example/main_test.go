package main

import (
	"testing"

	"github.com/egoist/mygo/ui"
)

func TestEditingConfigurationRequiresAnotherCheck(t *testing.T) {
	for _, label := range []string{"Feed address", "Installed version", "Signing public key"} {
		t.Run(label, func(t *testing.T) {
			d := &demo{
				config:    configuration{feed: "https://example.com/releases.json", version: "1.0.0", publicKey: "key"},
				available: true, downloaded: true, release: "2.0", notes: "## Ready", size: 1024,
			}
			tt := ui.NewTester(d.view, 800, 680)
			if err := tt.Click(label); err != nil {
				t.Fatal(err)
			}
			tt.Key(ui.Cmd, ui.KeyA)
			tt.Type("changed")
			if d.available || d.downloaded || d.release != "" || d.notes != "" || d.size != 0 {
				t.Fatal("editing configuration kept an old release available for installation")
			}
			if !tt.HasText("Check for updates again") {
				t.Fatal("configuration change was not shown")
			}
			// The updater is deliberately nil: stale action buttons must not call it.
			if err := tt.Click("Download update"); err != nil {
				t.Fatal(err)
			}
			if err := tt.Click("Install and quit"); err != nil {
				t.Fatal(err)
			}
			if d.working != "" {
				t.Fatal("a stale update action started")
			}
		})
	}
}

func TestBusyFormPreservesConfiguration(t *testing.T) {
	config := configuration{feed: "https://example.com/releases.json", version: "1.0.0", publicKey: "key"}
	d := &demo{config: config, working: check, status: "Checking…", progress: -1}
	tt := ui.NewTester(d.view, 800, 680)
	for _, label := range []string{"Feed address", "Installed version", "Signing public key"} {
		if err := tt.Click(label); err != nil {
			t.Fatal(err)
		}
		tt.Key(ui.Cmd, ui.KeyA)
		tt.Type("changed")
	}
	if d.config != config {
		t.Fatal("configuration changed while a native action was running")
	}
}
