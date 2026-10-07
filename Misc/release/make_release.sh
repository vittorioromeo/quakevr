#!/bin/bash
# make_release.sh -- Git Bash front for make_release.ps1 (same arguments): docs/vr-port/RELEASING.md.
#   bash Misc/release/make_release.sh [-DryRun | -Publish] [-Version 1.0.0 -BumpVersion] [...]   (the version: VERSION)
exec powershell -NoProfile -ExecutionPolicy Bypass -File "$(dirname "$0")/make_release.ps1" "$@"
