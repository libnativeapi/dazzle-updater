#!/bin/sh
set -eu

binding_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$binding_dir" -B "$binding_dir/build" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$binding_dir/build" --config Release --target dazzle_updater --parallel
