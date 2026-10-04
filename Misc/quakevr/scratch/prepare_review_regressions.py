"""Buildable, isolated QC fixtures for the 2026-10-04 regression checks.

Run: python Misc/quakevr/scratch/prepare_review_regressions.py build-cmake/review-probe
Compile the resulting QC/progs.src with the shipping flags, then copy its
quakevr/progs.dat into an isolated test game (never the author's live game).
See docs/vr-port/TESTING.md for fixture commands and expected results.
"""
import argparse
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    target = parser.parse_args().output.resolve()
    root = Path(__file__).resolve().parents[3]
    if target == root or target in root.parents or (root in target.parents and root / "build-cmake" not in target.parents):
        parser.error("output must be a separate scratch directory")
    qc = target / "QC"
    shutil.copytree(root / "QC", qc, dirs_exist_ok=True)
    (target / "quakevr").mkdir(parents=True, exist_ok=True)
    shared = target / "Quake/vr"
    shared.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / "Quake/vr/vr_melee_shared.h", shared)
    shutil.copy2(Path(__file__).with_name("review_regressions.qc"), qc)
    hook = qc / "vr_walltorch.qc"
    text = hook.read_text()
    marker = "void() VR_WallTorch_TestShot =\n{"
    if text.count(marker) != 1:
        parser.error("wall-torch fixture hook changed; review the insertion point")
    cases = "\n".join(
        f'    if(cvar("vr_test_walltorch_shot") == {number}) '
        f'{{ cvar_set("vr_test_walltorch_shot", "-1"); {name}(); return; }}'
        for number, name in [(99, "ReviewProbe"), (98, "ReviewTorch"), (97, "ReviewExitBlock")]
    )
    hook.write_text(text.replace(marker, marker + "\n" + cases))
    with (qc / "progs.src").open("a") as source:
        source.write("\nreview_regressions.qc\n")
    print(qc)


if __name__ == "__main__":
    main()
