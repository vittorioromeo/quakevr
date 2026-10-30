#!/bin/bash
# config_sparse_test.sh <agent> [full config] -- the sparse config's round trip (configVersion 218; vr_cvars.cpp
# VR_ConfigSaves, vr_config_diag.cpp; CODE_STYLE.md, "Settings and the config"), headless, no map:
#   the full config (default: the run's own, ironwail.cfg.baseline; or the given file, copied into the game folder as
#   cfgtest_src.cfg and executed with its migration) -> the game folder's config written (sparse), edited as another
#   copy of the game would (a changed line removed, a line added) and written again: both taken (VR_ConfigMergeOthers)
#   -> snap -> writeconfig (sparse) -> only changed vr_ settings in it, each as snapped -> every plain vr_ setting
#   scrambled -> the sparse config executed -> every setting as snapped -> three defaults moved (one at its default, a
#   slot one, a changed one) -> executed again -> the two at their defaults take the new ones, the changed one its line.
# Writes quakevr/cfgtest_*.cfg (git-ignored); cfgtest_full_<tag>.cfg: every setting after the load, to compare builds.
# Ends on the verdict line "config_test: N passed, M failed"; exits 1 on a failure.
AGENT=${1:?worktree name}
SRC=$2
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
LOAD=""
TAG=base
if [ -n "$SRC" ]; then
    cp "$SRC" "$TREE/quakevr/cfgtest_src.cfg" || exit 1
    LOAD="exec cfgtest_src.cfg;vr_migrate_config;"
    TAG=src
fi
SPARSE=cfgtest_sparse_$TAG
OUT=$(bash $KIT/run.sh $AGENT -Script "${LOAD}vr_config_test dumpfull cfgtest_full_$TAG.cfg;writeconfig;vr_config_test othercopy;writeconfig;vr_config_test checkmerge;vr_config_test snap;writeconfig $SPARSE;vr_config_test checkfile $SPARSE.cfg;vr_config_test scramble;exec $SPARSE.cfg;vr_config_test compare;vr_config_test newdefault;exec $SPARSE.cfg;vr_config_test checkdefault;vr_config_test done;toggleconsole;quit" \
    -Filter "^config_test:|from another copy|rror|CRASH" -QuitOn "^config_test: [0-9]+ passed" -Timeout 300 2>&1 | grep -v "^$")
echo "$OUT"
echo "$OUT" | grep -q "^config_test: [1-9][0-9]* passed, 0 failed" || exit 1
