"""Targeted room + record/replay routing check: python melee_room_test.py <agent>."""
import csv
import io
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

TREE = Path(__file__).resolve().parents[2]
KIT = Path(os.environ.get("KIT", "C:/OHWorkspace/qvr-kit"))
BASH = "C:/Program Files/Git/bin/bash.exe"
NAME = "vrmelee-recording"
OUT = TREE / "scratch" / "meleeroom_check"
OUT.mkdir(parents=True, exist_ok=True)


def check(ok, message):
    print(("PASS " if ok else "FAIL ") + message)
    if not ok:
        raise SystemExit(1)


def run(label, script, filter, finish=True):
    # Keep the exact reproducible script and console output in the worker's own scratch folder.
    cfg = TREE / "quakevr" / "meleeroom_check.cfg"
    commands = []
    for command in script.split(";"):
        match = re.fullmatch(r"wait(\d+)", command)
        commands.extend(["wait"] * int(match[1]) if match else [command])
    cfg.write_text("\n".join(commands) + ("\ntoggleconsole\ndisconnect\nwait\nwait\nquit\n" if finish else "\n"))
    result = subprocess.run([BASH, str(KIT / "run.sh"), sys.argv[1], "-Script", "exec meleeroom_check.cfg",
                             "-Timeout", "240", "-Filter", filter], capture_output=True, text=True)
    text = result.stdout + result.stderr
    (OUT / (label + ".log")).write_text(text)
    check(result.returncode == 0 and "ENGINE CRASH" not in text, label + " hidden run")
    return text


data = (TREE / "quakevr" / "maps" / (NAME + ".bsp")).read_bytes()
offset, length = struct.unpack_from("<ii", data, 4)
entities = data[offset:offset + length].decode("ascii").rstrip("\0")
classes = re.findall(r'"classname" "([^"]+)"', entities)
check(classes.count("vr_dummy") == 1 and classes.count("func_weapon_grabbable") == 7,
      "compiled BSP: one dummy, seven melee pickups")
check(set(classes) == {"worldspawn", "info_player_start", "vr_dummy", "func_weapon_grabbable", "light"},
      "compiled BSP: no sandbox objects or extra enemies")
source = (TREE / "quakevr" / "maps" / (NAME + ".map")).read_text()
check(set(re.findall(r'"weapon" "(\d+)"', source)) == {"2", "3", "13", "14", "17", "18"}
      and '"weaponflags" "1"' in source, "all melee types, including both swords")

log = run("load_record", "map e1m1;wait60;echo ROOM_SMOKE;vr_dummy_type 0;vr_dummy_health 30;map " + NAME +
          ";wait120;edicts;vr_brush_trace 128 -600 48 128 600 48 32;"
          "vr_brush_trace -600 128 48 600 128 48 32;vr_brush_trace 128 128 32 128 128 -32;"
          "setpos 0 -48 24 0 90 0;noclip;wait10;vr_fixed_frames 1;vr_fixed_frames_rate 90;"
          "vr_motion_armed 1;vr_motion_preroll 0.5;vr_motion_tail 0;wait60;"
          "vr_motion_record no_hit_room_check;wait90;vr_motion_stop;wait10;vr_motion_armed 0",
          "ROOM_SMOKE|EDICT |classname|weapon |weaponflags|model |origin |brush trace|Motion recorder|ENGINE|Host_Error|ERROR")
traces = re.findall(r"brush trace: fraction ([0-9.]+)", log)
check(len(traces) == 3 and traces[:2] == ["1.000", "1.000"] and float(traces[2]) < 1,
      "two 1200-unit player-width clear sweeps; solid floor")
check(re.search(r"classname\s+monster_army", log) is not None and log.count("func_weapon_grabbable") >= 7
      and log.count("classname      thrown_weapon") == 7,
      "runtime: dummy and all seven weapon spawners loaded")
takes = sorted((TREE / "quakevr" / "motions").glob("no_hit_room_check_*.csv"))
check(bool(takes), "recorder saved a take in the worker's motions folder")
take = takes[-1]
original = take.read_bytes()
check(("# map: " + NAME).encode() in original, "new recording names the dedicated map")
rows = list(csv.DictReader(io.StringIO("\n".join(l for l in original.decode().splitlines() if not l.startswith("#")))))
check(len(rows) >= 80, "recorded at least 80 frames (actual " + str(len(rows)) + ")")

log = run("direct_replay", "map e1m1;wait60;developer 1;vr_motion_play " + take.stem + ";wait300",
          "vr_motion_play:|Playing|Motion playback|Melee Recording|hands vs|recorded:|replayed:|ENGINE|Host_Error|ERROR")
check("hands vs the take" in log and "no target in this map" not in log,
      "direct replay routes from e1m1 to the room and finds its target")
errors = re.search(r"main max ([\d.]+).*off max ([\d.]+)", log)
check(errors is not None and max(float(errors[1]), float(errors[2])) < 0.01,
      "recorded hand poses replay within 0.01 units")
# An old map header never forces playback back to the sandbox. The target-relative placement
# and all recorded hand settings are used verbatim, as with old author recordings.
legacy = TREE / "quakevr" / "motions" / "room_legacy_header.csv"
legacy_bytes = original.replace(("# map: " + NAME).encode(), b"# map: vrfiringrange")
legacy.write_bytes(legacy_bytes)
(TREE / "quakevr" / "motions" / "room_check.txt").write_text(take.name + "\n" + legacy.name + "\n")
log = run("evaluation", "toggleconsole;vr_motion_eval list room_check.txt out room_eval.csv quit",
          "eval:|PASS|FAIL|ERROR|ENGINE|Host_Error", finish=False)
table = TREE / "quakevr" / "motions" / "room_eval.csv"
check(table.exists(), "evaluation wrote its result table")
results = list(csv.DictReader(line for line in table.open() if not line.startswith("#")))
check(len(results) == 2 and all(row["verdict"] == "PASS" for row in results),
      "default evaluation: new recording + legacy map header both PASS")
check(all(row["same_hits_as_recorded"] == "yes" and float(row["hand_error_u"]) < 0.01 for row in results),
      "legacy and new takes preserve recorded events and poses within 0.01 units")
check(take.read_bytes() == original and legacy.read_bytes() == legacy_bytes,
      "replay and evaluation preserve the recording bytes")
print("PASS melee recording room targeted checks complete")
