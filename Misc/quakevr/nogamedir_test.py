"""nogamedir_test.py -- Quake VR's game folder mounted on every start, also without "-game quakevr" (common.c
COM_AddVRGameArg; ROUND26.md, "nogamedir"), and the missing-data notice of a broken install (vr_gamedir.cpp).

    python Misc/quakevr/nogamedir_test.py <agent> [--id1 <folder with pak0.pak, pak1.pak>] [case...]

The engine is started directly, as a player's own shortcut does (no run.sh: its launch always passes -game quakevr), in
the player's command line shape (-flatscreenmode -heapsize -conwidth), hidden, in fast mode, at idle priority, with an
install-like layout: <tree>/scratch/nogamedir_base (id1's pak0 and pak1 copied from --id1, the kit's qbase by default:
Quake's folder without Quake VR) and the tree itself (its quakevr: Quake VR's folder). Cases:
- missing: Quake's folder alone: no quakevr added, the notice given (console, the centre print at the map's spawn),
  every data/*.txt "No ..." line (the player's report);
- nogame: "-basedir <Quake> -basedir <Quake VR>" and no -game: quakevr added, its game code and data read, e1m1 played;
- testmod: ... "-game testmod" (a mod folder in Quake's folder): "-game quakevr" put under it (game dirs quakevr;testmod).
- testmod_explicit: "-game quakevr -game testmod" given: unchanged (the same game dirs).
Each map run ends by itself (its script's quit), a 120 s timeout only a safety net. Prints "<case>: ok|FAIL ..." and
"nogamedir_test: N passed, M failed". The campaigns' launches (-game ad, -game hipnotic) go through run.sh (ROUND26).
"""
import argparse
import glob
import os
import re
import shutil
import subprocess

KIT = "C:/OHWorkspace/qvr-kit"

SCRIPT = [
    "vr_backend mock", "vr_mock_fast 1", "vr_fixed_frames 1", "sv_autosave 0", "con_logcenterprint 1",
    "vr_gamedata_notice", "vr_gamecode", "map e1m1", *["wait"] * 30,  # (the player spawned: the notice's centre print)
    "viewpos", "edict 1", "vr_gamedata_notice", "disconnect", "wait", "wait", "quit",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("agent")
    ap.add_argument("--id1", default=KIT + "/qbase/id1")
    ap.add_argument("cases", nargs="*")
    a = ap.parse_args()
    tree = f"C:/OHWorkspace/qvr-agents/{a.agent}"
    exe = tree + "/Windows/VisualStudio/Build-ironwail/bin/x64/Release/ironwail.exe"
    base = tree + "/scratch/nogamedir_base"
    os.makedirs(base + "/id1", exist_ok=True)
    os.makedirs(base + "/testmod/maps", exist_ok=True)
    for pak in ("pak0.pak", "pak1.pak"):
        if not os.path.exists(f"{base}/id1/{pak}"):
            shutil.copyfile(f"{a.id1}/{pak}", f"{base}/id1/{pak}")
    cases = {
        # (arguments, quakevr mounted, the missing notice, the map played)
        "missing": (["-basedir", base], False, True, True),
        "nogame": (["-basedir", base, "-basedir", tree], True, False, True),
        # (a mod over quakevr: e1m1 doesn't load there, "vrgrunt.mdl#plate0 not found", on next too: not this test's)
        "testmod": (["-basedir", base, "-basedir", tree, "-game", "testmod"], True, False, False),
        "testmod_explicit": (["-basedir", base, "-basedir", tree, "-game", "quakevr", "-game", "testmod"], True, False, False),
    }
    names = a.cases or list(cases)
    passed = 0
    for name in names:
        extra, wantVr, wantNotice, wantMap = cases[name]
        with open(base + "/id1/autoexec.cfg", "w") as f:
            f.write("\n".join(SCRIPT) + "\n")
        log = base + "/qconsole.log"
        if os.path.exists(log):
            os.replace(log, log + ".old")
        env = dict(os.environ, QVR_TEST_HIDDEN="1", QVR_NO_ERROR_DIALOG="1", QVR_TEST_BACKGROUND="1", SDL_ASSERT="abort")
        args = [exe, *extra, "-condebug", "-foreignconfigs", "-noconfigwrite", "-nosound", "-window", "-width", "640",
                "-height", "360", "-flatscreenmode", "-heapsize", "480000", "-conwidth", "800"]
        try:
            subprocess.run(args, cwd=base, env=env, timeout=120, creationflags=0x40)  # (IDLE_PRIORITY_CLASS)
        except subprocess.TimeoutExpired:
            print(f"{name}: FAIL timed out")
            continue
        finally:
            # (what a run makes in the tree's root, the last base dir: moved into scratch)
            for stray in ("cache", "testmod"):
                if os.path.isdir(f"{tree}/{stray}"):
                    os.replace(f"{tree}/{stray}", f"{tree}/scratch/nogamedir_old_{stray}_{os.getpid()}_{name}")
        text = open(log, encoding="latin-1").read() if os.path.exists(log) else ""
        for line in text.splitlines():
            if re.search(r"game folder added|game data wasn't|No data/|game code |Player pos|looked for quakevr", line):
                print(f"  {name}| {line.strip()[:180]}")
        dirs = re.findall(r"game dirs \(?([^)\s]*)", text)
        dirs = dirs[0].split(";") if dirs and dirs[0] else []
        mounted = "quakevr" in dirs
        notice = "Quake VR's game data wasn't found" in text and "(looked for quakevr in" in text
        noData = "No data/weapons.txt" in text
        ok = mounted == wantVr and notice == wantNotice and noData != wantVr and ("Player pos" in text or not wantMap)
        if name.startswith("testmod"):
            ok = ok and dirs[-2:] == ["quakevr", "testmod"]
        passed += ok
        print(f"{name}: {'ok' if ok else 'FAIL'} (game dirs {';'.join(dirs) or 'id1'}, notice {notice}, "
              f"data missing {noData})")
    print(f"nogamedir_test: {passed} passed, {len(names) - passed} failed")


if __name__ == "__main__":
    main()
