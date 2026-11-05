package dazzleupdater

/*
#cgo CFLAGS: -I${SRCDIR}/../../core/include
#cgo LDFLAGS: -L${SRCDIR}/build/lib -ldazzle_updater
#cgo darwin LDFLAGS: -Wl,-rpath,${SRCDIR}/build/lib
#cgo linux LDFLAGS: -Wl,-rpath,${SRCDIR}/build/lib -Wl,-rpath,$ORIGIN
*/
import "C"
