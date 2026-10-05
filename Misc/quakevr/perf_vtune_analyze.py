"""Summarize VTune's late-window main-thread samples (not frame timings).

Top-down inclusive totals are percentages; hotspots' self times are seconds.
Normalize the former to SDL_main, and exclude repeated recursive ancestors.
"""
import csv
import json
from pathlib import Path
import sys


def analyze(folder):
    with (folder / "late-hotspots.csv").open() as file:
        hot = list(csv.DictReader(file))
    main = [r for r in hot if r["Thread"].startswith("WinMainCRTStartup")]
    if not main:
        raise ValueError(f"No main-thread samples in {folder}")
    effective = sum(float(r["CPU Time:Effective Time"]) for r in main)
    spin = sum(float(r["CPU Time:Spin Time"]) for r in main)
    main = sorted(main, key=lambda r: -float(r["CPU Time:Effective Time"]))
    with (folder / "late-top-down.csv").open() as file:
        tree = list(csv.DictReader(file))
    stack, totals, denominator = [], {}, 0
    targets = ("R_DrawAliasModels", "VR_RetroAlias", "VR_AliasInstance", "VR_PhysicsFrameEnd",
               "writeProp", "syncEntities", "beforeStep", "keepInWorld", "SV_Physics_Client",
               "SV_Physics_Toss", "VR_TouchLinks", "PF_findradius", "PF_findportalcone",
               "buildWorld", "worldBuckets", "clipToWorld", "lightAlong", "b3World_Step")
    for row in tree:
        text = row["Function Stack"]
        depth, name = len(text) - len(text.lstrip()), text.strip()
        stack = stack[:depth]
        if name == "SDL_main":
            denominator += float(row["CPU Time:Effective Time:Total"])
        if "SDL_main" in stack:
            for target in targets:
                matches = lambda n: n == target or n.endswith("::" + target)
                if matches(name) and not any(matches(n) for n in stack):
                    totals[target] = totals.get(target, 0) + float(row["CPU Time:Effective Time:Total"])
        stack.append(name)
    if denominator <= 0:
        raise ValueError(f"No resolved main-thread call tree in {folder}")
    return dict(capture=folder.name, window=(folder / "late-window.txt").read_text().strip(),
                main_effective_seconds=effective, main_spin_seconds=spin,
                inclusive_main_percent={k: round(100 * v / denominator, 3) for k, v in totals.items()},
                self_main_percent=[dict(function=r["Function"], module=r["Module"],
                                       percent=round(100 * float(r["CPU Time:Effective Time"]) / effective, 3))
                                   for r in main[:20]])


if __name__ == "__main__":
    root = Path(sys.argv[1])
    results = [analyze(folder) for folder in sorted(root.glob("vtune-*"))
               if (folder / "late-window.txt").exists()]
    (root / "vtune-summary.json").write_text(json.dumps(results, indent=2))
    for r in results:
        print(r["capture"], r["window"])
        print("  main inclusive %:", r["inclusive_main_percent"])
        print("  main self %:", r["self_main_percent"][:8])
