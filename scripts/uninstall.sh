#!/usr/bin/env bash
# Remove Tank from all known install locations.
set -euo pipefail

removed=0

remove() {
    local path="$1"
    if [[ -e "$path" ]]; then
        rm -rf "$path"
        echo "  Removed: $path"
        removed=1
    fi
}

echo "Uninstalling Tank..."

remove "${HOME}/.vst3/Tank.vst3"
remove "${HOME}/.clap/Tank.clap"
remove "${HOME}/.local/bin/Tank"

if [[ $EUID -eq 0 ]]; then
    remove "/usr/lib/vst3/Tank.vst3"
    remove "/usr/lib/clap/Tank.clap"
    remove "/usr/local/bin/Tank"
else
    for path in "/usr/lib/vst3/Tank.vst3" \
                "/usr/lib/clap/Tank.clap" \
                "/usr/local/bin/Tank"; do
        if [[ -e "$path" ]]; then
            echo "  Skipping $path (re-run with sudo to remove)"
        fi
    done
fi

if [[ $removed -eq 0 ]]; then
    echo "  Nothing to remove."
else
    echo "Done."
fi
