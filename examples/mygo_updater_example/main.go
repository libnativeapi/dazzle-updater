package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"runtime"
	"strings"
	"time"

	"github.com/egoist/mygo"
	"github.com/egoist/mygo/ui"
	"github.com/egoist/mygo/ui/markdown"
	dazzleupdater "github.com/fastforgedev/dazzle/bindings/go"
)

type action string

const (
	check    action = "Checking"
	download action = "Downloading"
	install  action = "Installing"
)

type configuration struct {
	feed, publicKey, version string
}

// Only the main thread reads or writes demo's fields. Workers call the thread-safe
// native API and deliver their result through MyGo's main-thread dispatcher.
type demo struct {
	config       configuration
	win          *mygo.Window
	updater      *dazzleupdater.Updater
	working      action
	available    bool
	downloaded   bool
	progress     float64
	status       string
	release      string
	notes        string
	size         int64
	quitting     bool
	closed       bool
	cancelling   bool
	smoke        bool
	afterAction  func(action, bool)
	markdownOpts markdown.Options
}

func (d *demo) invalidate() {
	if !d.closed && d.win != nil {
		d.win.Invalidate()
	}
}

func (d *demo) configurationChanged() {
	// An edited source/version/key needs a new check before a previous result can
	// be downloaded or installed. The running action always uses a fixed snapshot.
	d.available, d.downloaded = false, false
	d.release, d.notes, d.size = "", "", 0
	d.status = "Configuration changed. Check for updates again."
}

func (d *demo) start(next action) {
	if d.working != "" || d.closed || d.quitting {
		return
	}
	if next == download && (!d.available || d.downloaded) {
		return
	}
	if next == install && (!d.downloaded || d.smoke) {
		return
	}
	if next == check {
		dazzleupdater.ReleaseIndexSourceInstance().SetURL(strings.TrimSpace(d.config.feed))
		d.updater.SetSource("release-index")
		d.updater.SetCurrentVersion(strings.TrimSpace(d.config.version))
		d.updater.SetPublicKey(strings.TrimSpace(d.config.publicKey))
		d.available, d.downloaded = false, false
		d.release, d.notes, d.size = "", "", 0
	}
	d.working, d.status, d.progress = next, string(next)+"…", -1
	d.cancelling = false
	d.invalidate()

	finished := make(chan struct{})
	if next == download {
		go func() {
			ticker := time.NewTicker(100 * time.Millisecond)
			defer ticker.Stop()
			for {
				select {
				case <-finished:
					return
				case <-ticker.C:
					progress := d.updater.DownloadProgress()
					mygo.RunOnMain(func() {
						select {
						case <-finished:
							return // A later action may already be running.
						default:
						}
						if !d.closed && d.working == download {
							d.progress = progress
							if d.cancelling {
								d.updater.CancelDownload()
							}
							d.invalidate()
						}
					})
				}
			}
		}()
	}
	go func() {
		var ok bool
		switch next {
		case check:
			ok = d.updater.CheckForUpdates()
		case download:
			ok = d.updater.DownloadUpdate()
		case install:
			ok = d.updater.InstallUpdate()
		}
		close(finished)
		mygo.RunOnMain(func() { d.finish(next, ok) })
	}()
}

func (d *demo) finish(completed action, ok bool) {
	if d.closed {
		return
	}
	d.working = ""
	d.available, d.downloaded = d.updater.IsUpdateAvailable(), d.updater.IsUpdateDownloaded()
	d.release, d.notes, d.size = d.updater.UpdateDisplayVersion(), d.updater.UpdateReleaseNotes(), d.updater.UpdateSize()
	d.progress = d.updater.DownloadProgress()
	if !ok {
		d.status = fmt.Sprintf("%s failed · %s: %s", completed, d.updater.ErrorCode(), d.updater.ErrorMessage())
	} else {
		switch completed {
		case check:
			d.status = "You're up to date."
			if d.available {
				d.status = "Update " + d.release + " is available."
			}
		case download:
			d.status = "Downloaded and signature verified. Ready to install."
		case install:
			// Core waits for the host to exit before replacing the application.
			d.status = "Installer started. Closing the application…"
			mygo.App.Quit()
		}
	}
	d.invalidate()
	if d.quitting {
		mygo.App.Quit()
		return
	}
	if d.afterAction != nil {
		d.afterAction(completed, ok)
	}
}

func (d *demo) cancel() {
	if d.working == download && !d.cancelling {
		d.cancelling = true
		d.status = "Cancelling download…"
		d.updater.CancelDownload()
		d.invalidate()
	}
}

// Do not tear down the native library while an operation is in flight. Checks
// and installation cannot be cancelled; a download can, and then shutdown resumes.
func (d *demo) deferQuit() bool {
	if d.working == "" {
		return false
	}
	d.quitting = true
	d.cancel()
	d.status = "Waiting for the current operation to finish before closing…"
	d.invalidate()
	return true
}

func (d *demo) view(c *ui.Context) {
	busy := d.working != ""
	ui.Column(c).Fill().Padding(24).Gap(14).Children(func() {
		ui.Text(c, "Software updates").FontSize(28).Bold()
		ui.Text(c, "dazzle updater · MyGo example").TextColor(c.Theme().TextMuted)
		ui.Column(c).Gap(8).Children(func() {
			ui.Text(c, "Release index URL")
			if ui.TextInput(c, &d.config.feed).Label("Feed address").Placeholder("https://example.com/myapp/releases.json").Disabled(busy || d.smoke).Changed() {
				d.configurationChanged()
			}
			ui.Text(c, "Current version")
			if ui.TextInput(c, &d.config.version).Label("Installed version").Placeholder("1.0.0").Disabled(busy || d.smoke).Changed() {
				d.configurationChanged()
			}
			ui.Text(c, "Ed25519 public key (Base64)")
			if ui.TextInput(c, &d.config.publicKey).Label("Signing public key").Placeholder("The public key shipped with your application").Disabled(busy || d.smoke).Changed() {
				d.configurationChanged()
			}
		})
		ui.Row(c).Gap(10).Children(func() {
			if ui.PrimaryButton(c, "Check for updates").Disabled(busy || d.smoke).Clicked() {
				d.start(check)
			}
			if ui.Button(c, "Download update").Disabled(busy || !d.available || d.downloaded || d.smoke).Clicked() {
				d.start(download)
			}
			if ui.Button(c, "Install and quit").Disabled(busy || !d.downloaded || d.smoke).Clicked() {
				d.start(install)
			}
			if d.working == download && ui.Button(c, "Cancel").Disabled(d.cancelling || d.smoke).Clicked() {
				d.cancel()
			}
		})
		if busy {
			ui.Progress(c, d.progress)
		}
		ui.Text(c, d.status).Wrap()
		if d.available {
			ui.Text(c, fmt.Sprintf("Version %s · %s", d.release, formatSize(d.size))).Bold()
		}
		ui.Scroll(c).Grow(1).Children(func() {
			if d.notes != "" {
				markdown.View(c, d.notes, &d.markdownOpts)
			} else {
				ui.Text(c, "Release notes will appear here after an update is found.").TextColor(c.Theme().TextMuted).Wrap()
			}
		})
	})
}

func formatSize(size int64) string {
	if size <= 0 {
		return "download size unknown"
	}
	return fmt.Sprintf("%.1f MiB", float64(size)/(1024*1024))
}

func run(config configuration, smoke bool) error {
	d := &demo{config: config, updater: dazzleupdater.UpdaterInstance(), status: "Ready to check for updates.", smoke: smoke}
	var result error
	var stopSmoke func()
	mygo.App.WhenReady(func() {
		d.win = mygo.NewWindow(mygo.WindowOptions{
			Title: "dazzle updater — MyGo", Width: 800, Height: 680,
			MinWidth: 760, MinHeight: 600, Content: ui.View(d.view),
		})
		d.win.OnClose(func(e *mygo.CloseEvent) {
			if d.deferQuit() {
				e.PreventDefault()
			}
		})
		d.win.OnClosed(func() {
			d.closed = true
			mygo.App.Quit()
		})
		mygo.App.OnBeforeQuit(func(e *mygo.QuitEvent) {
			if d.deferQuit() {
				e.PreventDefault()
			}
		})
		if smoke {
			var err error
			stopSmoke, err = startSmoke(d, &result)
			if err != nil {
				result = err
				mygo.App.Quit()
			}
		}
	})
	err := mygo.App.Run()
	d.closed = true
	if stopSmoke != nil {
		stopSmoke()
	}
	if err != nil {
		return err
	}
	return result
}

func main() {
	runtime.LockOSThread()
	feed := flag.String("feed", os.Getenv("DAZZLE_DEMO_FEED"), "release index HTTP(S) URL")
	key := flag.String("public-key", os.Getenv("DAZZLE_DEMO_PUBLIC_KEY"), "Ed25519 public key, Base64 encoded")
	version := flag.String("version", "1.0.0", "current application version")
	smoke := flag.Bool("smoke", false, "exercise the live window with a local signed feed, then quit without installing")
	flag.Parse()
	if err := run(configuration{feed: *feed, publicKey: *key, version: *version}, *smoke); err != nil {
		log.Fatal(err)
	}
	if *smoke {
		log.Println("PASS: MyGo window, update check, signed download, signature rejection and up-to-date state")
	}
}
