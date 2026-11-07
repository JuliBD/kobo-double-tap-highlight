# Implementation notes for contributors

This plugin runs inside Nickel, against private C++ interfaces rather than a
supported annotation API. These notes explain choices that can look unnecessary
or surprising during review. They describe the TinySegmenter release; the only
hardware-tested configuration is Libra 2 / firmware 4.38.23852.

## Native calls and compatibility

`src/probe.c` resolves mangled symbols through NickelHook. The `void *` function
slots and `reinterpret_cast` calls avoid requiring Kobo's proprietary headers;
they do not make the calls ABI-independent. The exact argument and return types
matter, including references, values and ARM structure-return conventions.
For example, the `QPoint` accessor takes an output pointer before `this`, and
the JavaScript evaluator returns `QVariant` and takes `QString` by value.

`TapGesture`'s `GestureMixin` base is addressed at `tap + 8` in `toggle.cc` and
`settings.cc`. That offset was checked against the inspected constructor and
recognizer calls. It is a firmware-dependent layout assumption, not padding
that can be removed or a portable Qt guarantee. The QWidget/QObject casts also
rely on the inspected inheritance layout. Keep these assumptions in view when
porting to another firmware.

The version/model comparison is advisory, not an activation whitelist. Optional
hook resolution and per-feature pointer checks let other configurations attempt
to run or leave a feature inactive when dependencies are missing. A matching
symbol name does **not** prove its signature, layout or semantics still match.
`tools/verify_native_bindings.py` checks the available local firmware evidence;
it cannot certify arbitrary firmware compatibility.

## Why there are two gesture paths

`src/double_tap.c` is a diagnostic tap-pair observer retained from development.
Its 400 ms / distance thresholds contribute to logging and replay tests; they
are **not** what decides whether to save or delete a highlight.

The active path is `src/toggle.cc`. It enables Nickel's own double-tap recognizer
only for an unobstructed KEPUB reader. Nickel owns the first-tap delay, movement
checks and ordinary single-tap delivery. This avoids letting the first tap open
reader controls and then trying to undo that action when the second tap arrives.
No gesture object is retained for later use or manufactured for replay.

The extra 1000 ms guard checks that a completed native double tap has a matching
start in the same live reader context. It is not an additional double-tap
recognition window. Page/book changes and window changes invalidate context.
An invalidated native double tap can be consumed without creating a highlight;
ordinary unconsumed taps are forwarded once.

## Native selection without showing its normal UI

A quick toggle still calls Nickel's selection/menu machinery to obtain the
selected word or existing bookmark. `processSelection` and menu setup are not
being used to display a menu to the user: their current-bookmark state supplies
the native hit test and the annotation target.

Resolve an existing highlight **before** expanding Japanese text. A tap in the
middle of an old passage should remove that passage, even when it is longer
than a permitted new word or crosses tokenizer boundaries. Removal therefore
uses the existing bookmark ID and full range; it does not tokenize the passage
or require that the plugin originally created it.

GUI-thread checks, `QPointer` guards and the busy flag protect synchronous native
callbacks that may re-enter hooks or destroy a reader/controller. They are not
a general threading or lifetime guarantee for arbitrary native pointers.

## Two different rendering workarounds

### A finite highlight starting at offset zero

On the inspected firmware, `HighlightingManager::addHighlightSpans` chooses a
whole-element overload when `start == 0`, even when `end > 0` describes a shorter
range. This explained a whole sentence appearing shaded while the annotation
list contained only a character or word.

`src/rendering.cc` routes this case through Nickel's existing bounded-range
overload, preserving the bookmark/part IDs and style. It changes the renderer
call, not the text, bookmark offsets or saved database record. It applies to
all qualifying saved highlights, including ones created outside this plugin.
The default-on `highlight_rendering_fix` setting controls this workaround.
Changing it requires a book redraw/reopen to affect already-rendered spans.

### Avoiding extra full-screen flashes during a quick toggle

The native long-press selection path also changes display mode, closes the
footer UI and emits a signal named `repaint`. On the inspected firmware that
signal is connected to `ReadingView::forceFullUpdate`, so forwarding every
selection UI side effect produced unnecessary flashes for quick add/remove.

The A2, footer-close and repaint-signal hooks skip those side effects only for
the active automatic selection controller. Ordinary long presses retain their
native behavior. The hook on `forceFullUpdate` itself always forwards; it only
records/cancels pending partial treatment. Explicit full updates are respected.

The actual WebKit repaint may be scheduled **after** the annotation call
returns. `Context::eventFilter` therefore treats at most the next matching
window `UpdateRequest`, for up to 250 ms. This is an expiry bound, not a sleep
or an extra 250 ms delay. The same event is delivered synchronously with
Kobo-specific partial-update attributes and a re-entry guard; the outer filter
then consumes it so the native handler runs once. There is no `processEvents`
call or early paint flush.

Attributes 130 (partial) and 131 (full) are Kobo Qt extensions verified on the
inspected build, not standard portable Qt API values. Temporary partial flags
are restored, and an existing full-update hint prevents this treatment. A new
native gesture, page/book change or conflicting UI input cancels it. A raw
`TouchBegin` alone does not: Qt can deliver it after the native recognizer has
already completed the triggering double tap. Cancelling on that raw event was
too early for the scheduled repaint.

The rendering setting above controls the zero-offset correction, **not** these
quick-toggle UI/refresh hooks. Do not combine the two merely because both live
in `rendering.cc`, or interpret this as disabling all full-screen refreshes.

## Keeping the inline dictionary picker available

Nickel can count a short Japanese selection as several words. In the inspected
firmware, the inline results handler compares that count with the selected
dictionary's limit. English allows one word. A selection above the limit with
no result can lose its popup, which also removes the dictionary picker.

The optional count hook adjusts the eligibility count for short Japanese
word-shaped selections at that one call site. It leaves the query, dictionary,
and results intact. The hook applies to lookup from either a long press or a
saved highlight. It works independently of the long-press word setting.

The call-site check uses the Thumb return address relative to
`SelectionController::onInlineDefinitionResults` (offset `0x26e`). Only the
inspected Libra 2 / 4.38.23852 configuration enables this workaround.
The owner verified both lookup paths on that device with English selected and
Japanese long-press word selection enabled. Long-pressing a Japanese word and
single-tapping a saved Japanese word highlight both opened the native popup.

Recheck the call site and popup behavior before enabling it on other firmware.

## Annotation guards and the transient fingerprint

`src/annotations.cc` invokes the native `highlight` / `removeHighlight` paths
and observes their synchronous save/delete calls. It does not write SQLite
itself or maintain a second persistent annotation registry. The SHA-256
fingerprint is a temporary snapshot of content, text and range, used to reject
a changed deletion target or inconsistent save within the current operation.
It is not a persistent duplicate index or a security/authenticity check.
There is no 4096-entry lifetime cap on saved highlights.

The `__thread` scope pointers plus `ScopeGuard` associate nested native calls
with the operation that initiated them and restore an outer scope on return.
`Qt::DirectConnection` is intentional: queuing a call would outlive that stack
scope and bypass its intended validation. An asynchronous native save/delete
implementation would need a different design.

Nickel can change the page's DOM before attempting the database save. Rejecting
the save alone can leave a phantom visual highlight. The automatic save path
requires the native visual-removal helper and uses rejected bookmark IDs to
clean up the display. That helper does not delete annotations from the database.
This distinction matters when investigating visual versus persistent failures.

Current code requires a note-free highlight for guarded deletion. Nevertheless,
note-bearing removal was not conclusively verified on hardware, and a native UI
change can precede a blocked persistence operation. Do not infer guaranteed
note preservation from this predicate. Check the annotation list and reopen the
book when testing this case; the README deliberately states the uncertainty.

## Japanese runs in the book's WebKit frame

`tools/embed-japanese.py` bundles pinned TinySegmenter and the ES5 adapter into a
generated C header. Parsing happens on the Kobo, on demand after a Japanese
selection, not on the PC or by preprocessing the entire book. English selection
bypasses expansion. The long-press hook forwards native selection once, then
uses the same expansion/validation without saving a highlight automatically.

The adapter collects a bounded block of text and maps UTF-16 offsets back to
DOM nodes. This is needed because inline markup can split a word across text
nodes. Ruby `RT`/`RP` readings and hidden content are excluded; punctuation and
block boundaries prevent accidental joins. Surrogate pairs and combining kana
need special handling because JavaScript/Qt offsets are UTF-16 code units.
TinySegmenter is statistical word segmentation, not a phrase recognizer.

The node/text budgets (4096 visits/characters, 512 text entries), 24-code-unit
Japanese token cap and matching DOM/native-text checks deliberately reject
ambiguous or oversized selections. These are operation bounds, unrelated to
how many annotations a reader can store. Do not remove them to select longer
phrases without reviewing the range-mapping and save-validation assumptions.

Every script path returns a nonempty JSON string. The inspected native evaluator
can retry an evaluation whose result is empty; a retry of a DOM mutation is not
safe. Keep the explicit reject responses. C++ also checks that Nickel's own
selected text equals the proposed token before invoking the save path.

Sudachi-related compile-time branches remain from a disabled experiment.
`SUDACHI=0` / `DTP_USE_SUDACHI=0` is the default release configuration; its Rust
library and dictionary are not required. Their presence is not a runtime
backend selector. `docs/build.md` describes the supported default build.

## Settings, names and integration details

- Settings are read on demand from the small bounded configuration file rather
  than cached at startup, so NickelMenu changes take effect without restarting
  the plugin. Missing/invalid settings default on. Rendering already on screen
  still needs a redraw.
- `libkobodoubletapprobe.so`, `KoboDoubleTapProbe`, `prototype-*` versions and some
  log filenames are historical identifiers retained by the installed artifact
  and diagnostic tooling. Renaming an installed library requires an upgrade
  migration so two differently named plugins cannot remain loaded together.
- The hidden `QPushButton` signal adapter and `Prototype:` labels belong to the
  old manual-menu test path. `DTP_TEST_MANUAL_MENU` enables that path only in
  fixtures; it is off in ordinary device builds. It bridges a dynamically
  described native signal to the typed Qt connection without a new moc class.
- The Makefile includes NickelHook's make fragment twice following its two-pass
  structure. Local compiler helper overrides filter prerequisites to source
  files: headers must trigger rebuilds but must not become extra compiler
  inputs. The explicit exported `NickelHook` aggregate avoids a GCC 4.9 issue
  with the framework's file-scope C compound-literal macro.
- Uninstall cleanup runs before Qt initialization and uses `unlink` only for
  this plugin's two NickelMenu integration files. Preferences/logs are retained;
  it does not traverse directories or touch annotation records.
- Diagnostic logging is bounded. Ordinary reports stop at their file budget;
  the refresh trace rotates to one `.previous` file. The `069` trace filename
  is historical, not the current version. The raw Kobo version line is never
  logged because it contains the device serial number.

## Evidence and limits of the tests

See [build instructions](build.md) for commands. `tests/toggle_qt_cases.inc`,
`tests/rendering_qt_cases.inc` and `tests/annotation_qt_test.cc` exercise the
production adapters with ARM Qt native-reader doubles. DOM cases live in
`tests/japanese_selection_test.cjs`; gesture observation/replay and initialization
checks are separate portable tests.

These checks cover routing, refusal, lifecycle and event-delivery behavior.
They do not emulate Nickel's real WebKit renderer, e-ink waveforms or firmware
ABI. Changes to native signatures, offsets, refresh hints or annotation actions
still require firmware inspection and device testing. Do not describe a passing
mock-based test as proof that another Kobo/firmware combination is supported.
