# Kobo Double Tap v0.8.0-beta.3

Limited beta candidate: core functions and recovery tests passed on the target
Libra 2. Broader device/firmware compatibility remains untested.

Double-tap to save a word highlight in Kobo's built-in KEPUB reader. Double-tap
inside an existing highlight to remove the whole highlight, including older
highlights and multiword passages. For note-bearing highlights, visual removal
was observed but persistence and attached-note handling remain unverified.

Target: **Kobo Libra 2, firmware 4.38.23852, KEPUB only**. Other models,
firmware and formats are unsupported. No NickelMenu dependency.

Compared with 0.6.10:

- Existing highlights no longer need to appear in the plugin's registry to
  be removable. A double tap removes the entire native hit-tested highlight.
- Existing-highlight resolution happens before Japanese tokenization.
- Deletion checks the current ID, text, book and range again at the manager
  boundary. Repeated deletion is blocked in offline tests. Note checks in those
  tests do not establish real-device note protection.
- TinySegmenter remains the default; Sudachi and its dictionary are excluded.
- MIT license, user guide, issue template and release acceptance checklist.

Install: back up annotations/settings, verify `SHA256SUMS.txt`, copy
`KoboRoot.tgz` into `.kobo`, safely eject and allow the restart. See README for
disable/uninstall instructions and limitations. No plugin undo is provided for
removal. Japanese segmentation can produce short tokens. Native ordinary
highlighting and note management remain available.

Beta.1 hardware results: user reports English/Japanese add/remove, older-passage
removal, ordinary input/refresh and reopen persistence pass. The earlier note
protection claim is withdrawn: only visual highlight removal was observed.
Beta.2 long-press word selection, native highlight saving, NickelMenu Off/On
and English pass-through: **user-reported pass**. Disable, re-enable, uninstall with automatic menu cleanup and reinstall passed
on the development device. Offline checks and artifact hashes
are recorded alongside the candidate; these do not prove device behavior.

Beta.2 also expands initial Japanese long-press selections with TinySegmenter,
enabled by default. The optional Home/reader NickelMenu toggle persists without
a restart. English long presses, native handles and annotation actions remain
native. Double-tap tokenization is independent of this setting.

Beta.3 adds an uninstall callback to remove only `.adds/nm/doubletap` and
`.adds/doubletap/toggle-japanese-longpress.sh`, preventing an inactive toggle
from remaining in NickelMenu. Existing NM entries, preferences, logs and
annotations remain. Cleanup has isolated filesystem tests and passed the physical uninstall test.
Reinstallation restored double taps and Japanese long presses. The final
uninstall comparison found no changes to existing annotations.
