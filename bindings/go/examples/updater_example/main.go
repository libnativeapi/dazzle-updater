package main

import (
	"fmt"

	dazzleupdater "github.com/fastforgedev/dazzle/bindings/go"
)

func main() {
	source := dazzleupdater.ReleaseIndexSourceInstance()
	source.SetURL("https://example.com/myapp/releases.json")

	updater := dazzleupdater.UpdaterInstance()
	updater.SetCurrentVersion("1.4.0")
	updater.SetPublicKey("11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=")

	fmt.Println("Source:", updater.Source())
	fmt.Println("Release index:", source.URL())
	fmt.Println("Current version:", updater.CurrentVersion())
	fmt.Println("Public key:", updater.PublicKey())
}
