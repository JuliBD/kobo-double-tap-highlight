#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror tests/uninstall_test.c -o build/uninstall-test
build/uninstall-test
