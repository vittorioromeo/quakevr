#!/bin/sh
# repo_cleanup.sh -- removes the tracked files docs/vr-port/REPO_CLEANUP.md rates "certain": unused by every build
# (MSBuild, CMake, the Makefiles, QC/progs.src), by the engine, the QuakeC, the maps, the menus and the test suite,
# and named by nothing that is kept. Run once, from anywhere in the repository; it only stages the removals
# (`git rm`), so `git status` shows them before you commit, and `git reset -q HEAD -- <path> && git checkout -- <path>`
# brings any one back. The "likely" and "check with Vittorio" rows are not touched: they are in the doc.
#
# Verified (REPO_CLEANUP.md, "Verification"): with these files moved out of the tree, build.sh (QC, Release x64,
# statics, QC precedence, FGD) built with 0 warnings and e1m1, vrstart and vrfiringrange loaded with the body on.
set -e
cd "$(git rev-parse --show-toplevel)"

git rm -q --ignore-unmatch -- \
    Misc/quakevr/slipgate_dark_cmp.sh \
    Misc/quakevr/slipgate_dark_diag.sh \
    Misc/quakevr/slipgate_dark_here.sh \
    Misc/quakevr/slipgate_dark_pair.sh \
    Misc/quakevr/slipgate_dark_sweep.sh \
    Misc/quakevr/slipgate_see_diag.sh \
    Misc/quakevr/slipgate_see_diag2.sh \
    Misc/quakevr/slipgate_see_shot.sh \
    Misc/quakevr/slipgate_pvs_probe.sh \
    Misc/quakevr/slipgate_pvs_sweep.sh \
    Misc/quakevr/pvs/added.txt \
    Misc/quakevr/pvs/profile_run.txt \
    Misc/quakevr/pvs/head_map.sh \
    Misc/quakevr/pvs/probe_head.sh \
    Misc/quakevr/install_notes_oct5_build.ps1 \
    Misc/quakevr/scratch/hz_reach.txt \
    Misc/quakevr/scratch/hz_zones.txt \
    quakevr/progs/vrtorso.mdl \
    QC/hip_model.qc \
    QC/hip_spr.qc

git status --short | grep '^D ' || true
echo "repo_cleanup.sh: removals staged; review with git status, then commit."
