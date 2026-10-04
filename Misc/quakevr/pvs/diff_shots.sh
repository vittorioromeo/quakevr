#!/bin/bash
# Compare explicitly chosen screenshots from a matched-position run; do not compare different times/yaws as PVS proof.
# Usage: diff_shots.sh reference.png comparison.png
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
python "$D/pngdiff.py" "$1" "$2"
