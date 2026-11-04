#!/bin/sh
set -eu
# Override only for isolated tests; NickelMenu uses the default device path.
settings=${DTP_SETTINGS_FILE:-/mnt/onboard/.kobo/doubletap.conf}
value=0
if [ -f "$settings" ] && [ "$(cat "$settings")" = 'japanese_long_press_words=0' ]; then value=1; fi
temporary=$settings.tmp.$$
umask 077
trap 'rm -f "$temporary"' EXIT HUP INT TERM
printf 'japanese_long_press_words=%s\n' "$value" > "$temporary"
mv -f "$temporary" "$settings"
if [ "$value" = 1 ]; then
    printf 'Japanese long-press words: ON\nApplies to your next long press.\n'
else
    printf 'Japanese long-press words: OFF\nKobo native selection restored for long presses.\n'
fi
