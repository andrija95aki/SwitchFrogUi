#!/bin/sh
# SwitchFrogUI guarded offline updater for H.OS 1.2 / R36SX.
# Package layout:
#   SwitchFrogUI-update/manifest.sha256
#   SwitchFrogUI-update/payload/cubegm/...
#   SwitchFrogUI-update/payload/frogui/...

CARD=/mnt/sdcard
PACKAGE="$CARD/SwitchFrogUI-update.tar.gz"
WORK="$CARD/.switchfrog-update"
STAGE="$WORK/SwitchFrogUI-update"
LOG="$CARD/SwitchFrogUI Update.log"
BACKUP_ROOT="$CARD/SwitchFrogUI Backups"

fail() {
    printf 'ERROR: %s\n' "$1" >> "$LOG"
    sync
    exit 1
}

printf 'SwitchFrogUI offline updater\n' > "$LOG"
printf 'Package: %s\n' "$PACKAGE" >> "$LOG"
[ -f "$PACKAGE" ] || fail 'Place SwitchFrogUI-update.tar.gz in the SD-card root.'
command -v tar >/dev/null 2>&1 || fail 'tar is unavailable on this firmware.'
command -v sha256sum >/dev/null 2>&1 || fail 'sha256sum is unavailable; refusing an unverified update.'

# Validate archive names before extraction. Absolute paths and parent traversal
# are never accepted, even when the package itself has a valid checksum list.
tar -tzf "$PACKAGE" > "$WORK.list" 2>>"$LOG" || fail 'The update archive is unreadable.'
grep -E '(^/|(^|/)\.\.(/|$))' "$WORK.list" >/dev/null 2>&1 && fail 'Unsafe path found in archive.'
grep -v '^SwitchFrogUI-update/' "$WORK.list" >/dev/null 2>&1 && fail 'Unexpected archive root.'

rm -rf "$WORK"
mkdir -p "$WORK" || fail 'Cannot create staging directory.'
tar -xzf "$PACKAGE" -C "$WORK" >>"$LOG" 2>&1 || fail 'Extraction failed.'
[ -f "$STAGE/manifest.sha256" ] || fail 'manifest.sha256 is missing.'
[ -d "$STAGE/payload" ] || fail 'payload directory is missing.'

(cd "$STAGE" && sha256sum -c manifest.sha256) >>"$LOG" 2>&1 || fail 'Checksum validation failed.'

update_id=`date +%Y%m%d-%H%M%S 2>/dev/null`
[ -n "$update_id" ] || update_id=manual
BACKUP="$BACKUP_ROOT/$update_id"
mkdir -p "$BACKUP" || fail 'Cannot create backup directory.'

while read expected relative; do
    [ -n "$relative" ] || continue
    relative=${relative#\*}
    case "$relative" in
        payload/cubegm/*|payload/frogui/*) ;;
        *) fail "Manifest contains disallowed path: $relative" ;;
    esac
    rel=${relative#payload/}
    case "$rel" in
        *../*|../*|*/..|/*) fail "Unsafe manifest path: $rel" ;;
        frogui/settings.txt|frogui/favorites.txt|frogui/playtime.txt|frogui/recent_games.txt|frogui/keyboard_gamepad.cfg|frogui/home_selection.cfg)
            fail "Package attempts to replace protected user data: $rel" ;;
    esac

    src="$STAGE/$relative"
    dst="$CARD/$rel"
    [ -f "$src" ] || fail "Manifest file is missing: $relative"
    parent=${dst%/*}
    backup_parent="$BACKUP/${rel%/*}"
    mkdir -p "$parent" "$backup_parent" || fail "Cannot create destination for $rel"
    if [ -f "$dst" ]; then
        cp -p "$dst" "$BACKUP/$rel" || fail "Backup failed: $rel"
    fi
    cp -p "$src" "$dst.switchfrog-new" || fail "Staging copy failed: $rel"
    mv -f "$dst.switchfrog-new" "$dst" || fail "Atomic install failed: $rel"
    printf 'Installed: %s\n' "$rel" >> "$LOG"
done < "$STAGE/manifest.sha256"

printf 'SUCCESS\nBackup: %s\n' "$BACKUP" >> "$LOG"
printf '%s\n' "$update_id" > "$CARD/frogui/last_update.txt"
sync
rm -rf "$WORK"
exit 0
