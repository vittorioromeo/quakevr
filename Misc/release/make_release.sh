#!/bin/bash
# make_release.sh -- Git Bash front for make_release.ps1 (same arguments): docs/vr-port/RELEASING.md.
#   bash Misc/release/make_release.sh -Version 1.0.0 [-DryRun | -Publish] [...]
exec powershell -NoProfile -ExecutionPolicy Bypass -File "$(dirname "$0")/make_release.ps1" "$@"
