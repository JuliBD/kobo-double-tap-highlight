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
printf 'PASS: NickelMenu toggle defaults On, switches Off/On, reports state, and leaves no temporary file\n'
