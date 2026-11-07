# Kobo Double Tap v0.8.0-beta.6

Beta.6 keeps the inline dictionary picker available for short Japanese words
when the selected dictionary has no match. With English selected on Libra 2
firmware 4.38.23852, both a long press and a tap on a saved Japanese highlight
now open the popup. See [issue #1](https://github.com/JuliBD/kobo-double-tap-highlight/issues/1).
The workaround is enabled on that model and firmware.

Limited beta candidate: core functions and recovery tests passed on the target
Libra 2. Broader device/firmware compatibility remains untested.

Double-tap to save a word highlight in Kobo's built-in KEPUB reader. Double-tap
inside an existing highlight to remove the whole highlight, including older
highlights and multiword passages. For note-bearing highlights, visual removal
was observed but persistence and attached-note handling remain unverified.

**Only tested on Kobo Libra 2, firmware 4.38.23852, KEPUB.** Other models and
firmware are untested; back up annotations and test at your own risk. Version
beta.4 removes the device/firmware whitelist and retains required native-symbol
checks. EPUB/PDF handling is not implemented. No NickelMenu dependency.

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

Beta.4 allows untested models/firmware to initialize. Missing or unfamiliar
version metadata emits an advisory warning rather than disabling the plugin.
Required-symbol checks and the user disable flag still apply.

Beta.5 removes the duplicate ID/checksum registry and its 4096-entry limit.
All highlight persistence uses Kobo's native annotation system. In-operation
snapshots still detect unexpected save/delete changes; nothing is stored in a
second highlight registry. Existing legacy metadata is ignored.

The rendering correction is now an independent NickelMenu option, **Highlight
rendering fix**, default On. It applies to all native highlight ranges, including
old/multiword/note-bearing highlights, without changing stored records. Off
restores native rendering. Reopen the book after changing it to redraw existing
highlights. The long-press option is independent and also defaults to On.

Beta.5 device checks passed on Libra 2 / 4.38.23852 / KEPUB: existing-highlight
rendering, rendering Off/On after reopening, and double-tap save/remove. The
legacy registry remains absent after use. Other device/firmware combinations
and the fate of notes attached to removed highlights remain unverified.
