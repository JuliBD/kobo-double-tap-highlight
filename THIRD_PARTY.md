# Third-party sources

NickelHook is vendored at the revision documented in
`vendor/NickelHook/UPSTREAM.md`, with its MIT license preserved there.

The selection-menu adapter uses the signal-to-QPushButton technique demonstrated
by Patrick Gaskin's NickelMenu (`src/nickelmenu.cc`), inspected at commit
`bc799ebe95a413a4857aa97a88e2b16557e6427f`:
https://github.com/pgaskin/NickelMenu . Its MIT license is preserved in
`licenses/NickelMenu.txt`. This implementation uses the native item factory and
different hook points, rather than copying NickelMenu's private object allocation.

Qt is dynamically linked using the NickelTC toolchain. No Qt runtime or Kobo
firmware libraries are bundled in the installation archive. Proprietary firmware
used for local inspection stays in ignored workspace directories.

TinySegmenter 0.2 by Taku Kudo is vendored unchanged under BSD 3-Clause, using
the pinned leungwensen mirror/package with its additional MIT notice. Source,
both licenses and provenance are in `vendor/TinySegmenter/`. Both full license
notices are also embedded in the JavaScript payload inside the plugin binary.
The original URL is http://chasen.org/~taku/software/TinySegmenter/ .
