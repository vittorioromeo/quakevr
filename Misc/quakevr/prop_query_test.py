"""Run live cached/uncached prop and force-grab comparisons in a disposable mock base."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from perf_suite import script, waits


def fixtures():
    common = ["vr_backend mock", "vr_enabled 1", "vr_mock_fast 1", "vr_fixed_frames 1",
              "vr_mock_eye_size 1024", "vid_vsync 0", "sv_autosave 0", "developer 1",
              "vr_tips 0", "vr_roomscale_move_mult 0", "vr_prop_query_verify 1"] + waits(80)
    mixed = common + ["map vrfiringrange"] + waits(150) + ["god 1", "notarget 1",
            "vr_physics_bigpile mixed 1000"] + waits(10) + ["vr_prop_query_test",
            "vr_physics_blast 130 -556 40 80"] + waits(180) + ["vr_prop_query_test",
            "vr_model_reload", "vr_retro_overrides_reload"] + waits(20) + ["vr_prop_query_test",
            "map e1m1"] + waits(150) + ["god 1", "notarget 1", "vr_prop_query_test"]
    portal = common + ["map start"] + waits(150) + ["god 1", "notarget 1",
             "vr_slipgates 1", "vr_portals 1", "vr_portals_walk 1", "vr_grab_gibs 2",
             "setpos 544 1320 24 0 90 0", "vr_mock_hand main 0 1.2 -0.35 63 0 0"] + waits(20)
    portal += ["vr_prop_query_test", "vr_portals_pulltest"] + waits(20) + ["+grabright",
              "vr_mock_button main grip 1"] + waits(60) + ["vr_mock_button main grip 0", "-grabright"] + waits(90)
    portal += ["vr_portals_pulltest"] + waits(5) + ["vr_slipgates 0"] + waits(45)
    portal += ["vr_prop_query_test", "vr_slipgates 1", "vr_portals_walk 0"] + waits(10)
    portal += ["vr_prop_query_test", "vr_portals_walk 1"] + waits(10) + ["vr_prop_query_test"]
    hands = list(common)
    for pick, mx, my in ((4, .06, 1.29), (1, .06, 1.29), (0, .06, 1.25), (9, .06, 1.25),
                         (10, .03, 1.25), (11, .06, 1.25), (12, .06, 1.25)):
        hands += ["map vrfiringrange"] + waits(60) + ["god 1", "notarget 1",
                  f"echo REGRIP_{pick}", "vr_mock_hand off -0.07 1.25 -0.25 70 0 0",
                  "vr_mock_hand main 0.3 1.25 -0.25 70 0 0"] + waits(10)
        hands += ["+graboff", "vr_mock_button off grip 1", f"vr_test_held_pick {pick}", "impulse 252"] + waits(20)
        hands += [f"vr_mock_hand main {mx} {my} -0.25 70 0 0"] + waits(3)
        for grab, button in (("+grabright", "main grip 1"), ("-graboff", "off grip 0"),
                             ("+graboff", "off grip 1"), ("-grabright", "main grip 0"),
                             ("+grabright", "main grip 1")):
            hands += [grab, "vr_mock_button " + button] + waits(15)
        hands += ["-graboff", "vr_mock_button off grip 0", "-grabright", "vr_mock_button main grip 0"] + waits(30)
        hands += ["vr_prop_query_test"]
    ragdolls = script("ragdolls_active_audit", 900, 1024, 0, False).splitlines()
    ragdolls.insert(0, "vr_prop_query_verify 1")
    ragdolls.insert(ragdolls.index("echo BENCH_DONE"), "vr_prop_query_test")
    vanilla = common + ["map e1m1"] + waits(150) + ["god 1", "notarget 1", "vr_prop_query_test"] + waits(180)
    for name, commands, vrgame in (("mixed", mixed, True), ("portal", portal, True),
                                   ("hands", hands, True), ("ragdolls", ragdolls, True),
                                   ("vanilla", vanilla, False)):
        # Replace the benchmark's final quit with the regression completion marker.
        if name == "ragdolls":
            commands = commands[:commands.index("echo BENCH_DONE")]
        yield name, commands + ["echo PROP_TEST_DONE", "disconnect"] + waits(5) + ["quit"], vrgame


def hand_outcomes(log):
    outcomes = []
    for block in re.split(r"REGRIP_", log)[1:]:
        outcomes.append(dict(pick=block.splitlines()[0].strip(),
            both=len(re.findall(r"^carry: both hands$", block, re.M)),
            kept=len(re.findall(r"^carry: kept by", block, re.M)),
            moved=[float(x) for x in re.findall(r"kept by.*moved ([0-9.]+)", block)]))
    assert len(outcomes) == 7
    return outcomes


def run(args):
    base, exe, output = (Path(p).resolve() for p in (args.base, args.exe, args.output))
    if "build-cmake" not in base.parts:
        raise ValueError("Use a disposable base under build-cmake")
    output.mkdir(parents=True, exist_ok=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    env = dict(os.environ, QVR_TEST_HIDDEN="1", QVR_TEST_BACKGROUND="1", QVR_NO_ERROR_DIALOG="1")
    results = []
    cases = [(name, commands, vrgame, exe) for name, commands, vrgame in fixtures()]
    if args.reference_exe:
        hands = next(commands for name, commands, _ in fixtures() if name == "hands")
        hands = [c for c in hands if not c.startswith("vr_prop_query_")]
        cases.append(("hands-reference", hands, True, Path(args.reference_exe).resolve()))
    for name, commands, vrgame, engine in cases:
        destination = output / name
        destination.mkdir(exist_ok=False)
        run_base = base if vrgame else base / "vanilla"
        folder = run_base / ("quakevr" if vrgame else "id1")
        if not vrgame:
            if folder.is_junction() or folder.is_symlink():
                raise ValueError("Vanilla testing requires a private id1 directory")
            folder.mkdir(parents=True, exist_ok=True)
            for pak in (base / "id1").glob("pak*.pak"):
                if not (folder / pak.name).exists():
                    os.link(pak, folder / pak.name)
            shutil.copy2(base / "quakevr/ironwail.cfg", folder / "ironwail.cfg")
        config = "\n".join(commands) + "\n"
        (folder / "autoexec.cfg").write_text(config)
        (destination / "autoexec.cfg").write_text(config)
        games = ["-game", "hipnotic", "-game", "rogue", "-game", "quakevr"] if vrgame else []
        process = subprocess.Popen([str(engine), "-basedir", str(run_base), *games, "-condebug", "-window",
                                    "-width", "960", "-height", "540", "-nosound", "-noconfigwrite", "-noaddons"],
                                   cwd=run_base, env=env, startupinfo=startup)
        try:
            code = process.wait(timeout=180)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait()
            raise RuntimeError(f"Timed out: {name}")
        shutil.copy2(run_base / "qconsole.log", destination / "qconsole.log")
        log = (destination / "qconsole.log").read_text(errors="replace")
        # vr_limits describes overflow paths using these names; only actual error lines are failures.
        assert code == 0 and "PROP_TEST_DONE" in log and not re.search(r"^(?:Sys_Error|Host_Error)(?:[: ]|$)", log, re.M), name
        if name != "hands-reference":
            assert "prop model queries: PASS" in log and "prop force-grab queries: PASS" in log, name
        if name == "portal":
            for required in ("portalpulltest: selected=1", "portalpulltest: gate=2",
                             "force grab: crossed slipgate 2", "force grab: caught",
                             "force grab: portal pull cancelled, object dropped in its room"):
                assert required in log, required
        if name == "ragdolls":
            assert "vr_ragdoll_list: 32 ragdolls" in log
        result = dict(fixture=name, exit_code=code, model_checks=re.findall(r"prop model queries: PASS (.*)", log),
                      chain_checks=re.findall(r"prop force-grab queries: PASS (.*)", log),
                      exe_sha256=hashlib.sha256(engine.read_bytes()).hexdigest())
        if name.startswith("hands"):
            result["hand_outcomes"] = hand_outcomes(log)
        if name == "hands-reference":
            current = next(v for v in results if v["fixture"] == "hands")["hand_outcomes"]
            for a, b in zip(current, result["hand_outcomes"]):
                assert (a["pick"], a["both"], a["kept"]) == (b["pick"], b["both"], b["kept"]), (a, b)
                assert len(a["moved"]) == len(b["moved"])
                assert all(abs(x - y) <= .03 for x, y in zip(a["moved"], b["moved"])), (a, b)
        results.append(result)
        print(f"PASS {name}: {len(result['model_checks'])} model sweeps, {len(result['chain_checks']) * 96} chains", flush=True)
    (output / "summary.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--reference-exe", help="Also compare the seven hand fixtures with the baseline executable")
    run(parser.parse_args())
