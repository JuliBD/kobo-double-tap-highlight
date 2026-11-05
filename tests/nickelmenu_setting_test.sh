#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
export DTP_SETTINGS_FILE=$test_dir/doubletap.conf
script=$root/integration/nickelmenu/toggle-japanese-longpress.sh
first=$(/bin/sh "$script")
[ "$(cat "$DTP_SETTINGS_FILE")" = 'japanese_long_press_words=0' ]
printf '%s' "$first" | grep -q OFF
second=$(/bin/sh "$script")
[ "$(cat "$DTP_SETTINGS_FILE")" = 'japanese_long_press_words=1' ]
printf '%s' "$second" | grep -q ON
[ "$(find "$test_dir" -type f | wc -l)" -eq 1 ]
render=$(/bin/sh "$script" rendering)
printf '%s' "$render" | grep -q 'Highlight rendering fix: OFF'
grep -q '^japanese_long_press_words=1$' "$DTP_SETTINGS_FILE"
grep -q '^highlight_rendering_fix=0$' "$DTP_SETTINGS_FILE"
/bin/sh "$script" >/dev/null
grep -q '^japanese_long_press_words=0$' "$DTP_SETTINGS_FILE"
grep -q '^highlight_rendering_fix=0$' "$DTP_SETTINGS_FILE"
/bin/sh "$script" rendering >/dev/null
grep -q '^japanese_long_press_words=0$' "$DTP_SETTINGS_FILE"
grep -q '^highlight_rendering_fix=1$' "$DTP_SETTINGS_FILE"
printf '# keep comment\nfuture_setting=keep\n' >> "$DTP_SETTINGS_FILE"
/bin/sh "$script" >/dev/null
grep -q '^future_setting=keep$' "$DTP_SETTINGS_FILE"
grep -q '^# keep comment$' "$DTP_SETTINGS_FILE"
[ "$(find "$test_dir" -type f | wc -l)" -eq 1 ]
[ ! -d "$DTP_SETTINGS_FILE.lock" ]
printf 'PASS: independent long-press/rendering toggles, default On, persisted Off/On, atomic updates and unrelated settings preserved\n' 
