#!/usr/bin/env bash
# Install Tank plugins and standalone app.
# Usage:
#   ./install.sh           — install to user directories (no root needed)
#   ./install.sh --system  — install system-wide to /usr/lib (requires sudo)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SYSTEM=0

for arg in "$@"; do
    case "$arg" in
        --system) SYSTEM=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

if [[ $SYSTEM -eq 1 ]]; then
    VST3_DIR="/usr/lib/vst3"
    CLAP_DIR="/usr/lib/clap"
    BIN_DIR="/usr/local/bin"
else
    VST3_DIR="${HOME}/.vst3"
    CLAP_DIR="${HOME}/.clap"
    BIN_DIR="${HOME}/.local/bin"
fi

echo "Installing Tank..."

mkdir -p "${VST3_DIR}"
rm -rf   "${VST3_DIR}/Tank.vst3"
cp -r    "${SCRIPT_DIR}/VST3/Tank.vst3" "${VST3_DIR}/"
echo "  VST3  → ${VST3_DIR}/Tank.vst3"

mkdir -p "${CLAP_DIR}"
cp       "${SCRIPT_DIR}/CLAP/Tank.clap" "${CLAP_DIR}/"
chmod    755 "${CLAP_DIR}/Tank.clap"
echo "  CLAP  → ${CLAP_DIR}/Tank.clap"

mkdir -p "${BIN_DIR}"
cp       "${SCRIPT_DIR}/bin/Tank" "${BIN_DIR}/"
chmod    755 "${BIN_DIR}/Tank"
echo "  App   → ${BIN_DIR}/Tank"

echo "Done."
