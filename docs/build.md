# Build the TinySegmenter release

The default build uses NickelTC GCC 4.9.4 / Qt 5.2.1 for ARM hard-float.
Rust, Sudachi and its dictionary are not needed.

## Linux

Use Python 3, make, pkg-config and the pinned NickelTC toolchain. Run
`python3 tools/fetch_nickeltc.py` to obtain the verified image layer locally.
Extract `.tools/nickeltc-image/toolchain.tar.gz` into a local toolchain directory
and add its `bin` directory to PATH. The fetch tool documents the actual output
filename; check it before extraction. No Kobo proprietary libraries are needed
to compile or package.

```
python3 tools/embed-japanese.py
make clean
make all
make koboroot
python3 -m pip install -r tools/requirements.txt
python3 tools/verify_package.py KoboRoot.tgz src/libkobodoubletapprobe.so
```

`make koboroot` must follow `make all`. The archive installs one shared library
at `/usr/local/Kobo/imageformats/libkobodoubletapprobe.so`. Build from a clean
tree when changing configuration. Preserve the exact source with the artifact.

Pinned NickelTC image manifest:
`sha256:3296eb400d270cfd26f5773ab09625d4469f85680b3a191951dfb78d8ade7840`.
Pinned layer:
`sha256:862464e9e2622637c11fb49c036a98e99998c1328b130422a1dea6d4b45c9e21`.
Vendored framework/tokenizer revisions are recorded in `vendor/*/UPSTREAM.md`.

## Windows / WSL development environment

The development helper `tools/build-kobo.ps1` expects Ubuntu-24.04 and NickelTC
at `/opt/kobo-double-tap/nickeltc`. It builds and verifies without modifying a
connected Kobo. `tools/stage-device.ps1` is a separate, device-specific helper
which requires deployment history and backups; it is not the public installer.

## Verification

Run `tools/test-uninstall.sh` on Linux to check removal of the two plugin menu
files, preservation of preferences/other entries, idempotence and error handling.


- `tools/test-host.ps1 -Zig PATH_TO_ZIG`: portable Zig 0.13 host tests.
- `tools/test-annotations.ps1`: ARM Qt integration tests with native-reader
  doubles; defaults to TinySegmenter. Requires QEMU and an ARM loader/Qt setup.
- `node tests/japanese_selection_test.cjs`: real DOM tests, using Playwright
  and its Chromium installation; regenerate the bundle first.
- `tools/verify_native_bindings.py`: local firmware ABI-availability checks;
  requires privately supplied matching firmware libraries under
  `firmware/4.38.23852`. These libraries are never included in public source.

The integration tests exercise production adapters, not a complete Nickel
emulator. Native signatures were inspected separately against the target
firmware; symbol presence alone does not verify signatures or hardware behavior.
See [physical release gates](release-checklist.md).
