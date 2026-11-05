#!/bin/sh
set -eu
settings=${DTP_SETTINGS_FILE:-/mnt/onboard/.kobo/doubletap.conf}
case ${1:-longpress} in
    longpress) key=japanese_long_press_words; label='Japanese long-press words' ;;
    rendering) key=highlight_rendering_fix; label='Highlight rendering fix' ;;
    *) printf 'Unknown setting\n' >&2; exit 1 ;;
esac
umask 077
lock=$settings.lock
if ! mkdir "$lock" 2>/dev/null; then printf 'Settings are busy; try again.\n' >&2; exit 1; fi
temporary=$settings.tmp.$$
trap 'rm -f "$temporary"; rmdir "$lock"' EXIT HUP INT TERM
input=/dev/null
if [ -f "$settings" ]; then input=$settings; fi
# Preserve other settings/comments and replace only the selected key.
awk -v key="$key" '
    function trim(s) { sub(/^[ \t\r]+/, "", s); sub(/[ \t\r]+$/, "", s); return s }
    BEGIN { current=1 }
    {
        equal=index($0,"=")
        if (equal && trim(substr($0,1,equal-1))==key) {
            value=trim(substr($0,equal+1))
            if (value=="0" || value=="1") current=value
        } else print
    }
    END { print key "=" (current==0 ? 1 : 0) }
' "$input" > "$temporary"
mv -f "$temporary" "$settings"
if [ "$(tail -n 1 "$settings")" = "$key=1" ]; then state=ON; else state=OFF; fi
printf '%s: %s\n' "$label" "$state"
if [ "$key" = highlight_rendering_fix ]; then
    printf 'Reopen your book to redraw existing highlights.\n'
else
    printf 'Applies to your next long press.\n'
fi
