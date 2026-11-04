# Kobo Double Tap

Double-tap a word in Kobo's built-in reader to highlight it. Double-tap inside
an existing highlight to remove the entire highlight.

**v0.8.0-beta.3 is a limited beta candidate validated on the target device.**
The user accepted beta.1 word add/remove and older-passage removal,
ordinary input/refresh and reopen persistence on the development Kobo. The user also accepted beta.2 Japanese long-press word selection, native
highlight saving, the NickelMenu On/Off toggle and English pass-through. Disable/re-enable worked with notes retained. Beta.2 uninstall restored native
selection and the annotation comparison found no changes; beta.3 automatic
menu cleanup and reinstallation also passed. Before/after uninstall snapshots
show no changes to existing annotations.

## Compatibility

| Item | Supported target |
| --- | --- |
| Device | Kobo Libra 2 |
| Firmware | 4.38.23852 |
| Book format | KEPUB in the built-in Nickel reader |
| Japanese | Local TinySegmenter word expansion |
| Other text | Kobo's native word selection |

The plugin deliberately stays inactive on other device/firmware combinations.
EPUB, PDF, other Kobo models and other firmware are not supported by this beta.
It is a Nickel modification, not a KOReader or calibre plugin. NickelMenu is
optional: it provides the long-press setting menu. Compatibility with other modifications needs more testing.

## Install

1. Back up the Kobo's `.kobo/KoboReader.sqlite` database and any important
   annotations/settings before testing. Keep that backup on your computer.
2. Download `KoboRoot.tgz` from the project's GitHub Releases page and check its
   SHA-256 against `SHA256SUMS.txt`. Use a published release, not an arbitrary
   binary or GitHub's automatically generated source archive.
3. Connect the Kobo by USB. Copy `KoboRoot.tgz` into its hidden `.kobo` folder.
   If another update is already there, finish that update before continuing.
4. Safely eject and let the Kobo install/restart. Open a KEPUB and test on a
   disposable highlight first.

To update, use the same procedure with the newer verified installer.
The current prototype installs one shared library and writes bounded diagnostic
logs plus a highlight fingerprint registry under `.kobo`. No account, network
service, extra dictionary or PC parser is required during reading.

## Use

- Double-tap unhighlighted text to save a native word highlight.
- Double-tap anywhere inside an existing highlight to remove
  **that entire highlight**, even if it is an older or multiword highlight.
- Note-bearing highlights: visual highlight removal was observed on the device,
  but persistence and the fate of the attached note were not checked. Note
  protection is not verified; neither is deletion of the attached note.
- Japanese long presses initially select a TinySegmenter word (default On).
  Handles, dragging and dictionary/highlight actions remain native.
- In NickelMenu on Home or in the reader, tap **Japanese long-press words** to
  switch On/Off. A message confirms the new state. It applies to the next long
  press, persists across restarts, and leaves double taps tokenized.
- Single taps, page buttons and English long presses retain their native roles.
  Normal native highlighting/removal remains available.

Japanese is segmented on demand on the Kobo. TinySegmenter may split compounds
or expressions into shorter pieces; this is word selection, not phrase
recognition. Sudachi is disabled and is not linked into the default build.

## Optional NickelMenu setting

The plugin works without NickelMenu, with Japanese long-press words enabled.
To add the toggle when NickelMenu is installed, extract `NickelMenu-settings.zip`
to the Kobo drive root, keeping its `.adds` paths. Alternatively copy
`integration/nickelmenu/doubletap` to `.adds/nm/doubletap` and
`integration/nickelmenu/toggle-japanese-longpress.sh` to
`.adds/doubletap/toggle-japanese-longpress.sh`. This adds a separate configuration
file; existing NickelMenu entries are preserved. Safely eject after copying.

The setting lives in `.kobo/doubletap.conf`. `japanese_long_press_words=0`
disables expansion for long presses; absent configuration defaults to On.
Remove the two added `.adds` files to remove this optional menu integration.

## Limits and recovery

This beta runs inside Nickel and changes real annotations. Removal has no
plugin undo command: restore from a backup or highlight the passage again.
Creating highlights is bounded by a 4096-entry local fingerprint registry;
older-highlight removal does not require an entry in that registry.

To disable: create an empty file named `doubletap_disable` in the exposed
Kobo drive's root (alongside `.kobo`), then restart. Remove the file and restart
to enable again. The flag must not end in `.txt`.

To uninstall: create an empty `doubletap_uninstall` file in that same drive
root, then restart. The loader removes the plugin and its two optional NickelMenu integration
files. Preferences and diagnostics remain for reinstallation. Automatic menu
cleanup in beta.3 passed the device uninstall test. Highlight records and notes
are not intentionally removed by uninstalling. Logs and the registry can
remain on the exposed drive. See [the device acceptance steps](docs/release-checklist.md).

Keep the last working installer for rollback. Do not factory-reset to solve a
plugin problem before preserving your books, settings and annotations. The
loader includes NickelHook's crash failsafe, but this does not replace a backup.

## Feedback and development

Report reproducible problems using the GitHub issue template. Include model,
firmware, version and book format, and inspect any logs before sharing them.
Do not upload your book, full annotation database or device backup publicly.

[Build instructions](docs/build.md) · [Release checklist](docs/release-checklist.md)
· [Third-party notices](THIRD_PARTY.md) · [MIT license](LICENSE)

This is an independent community project, unaffiliated with Kobo/Rakuten.
