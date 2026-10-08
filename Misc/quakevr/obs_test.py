"""The menus' OBS row (Quake/vr/vr_obs.cpp, vr_menuui.cpp) against a mock obs-websocket v5 server (obs_mock_server.py).

    python Misc/quakevr/obs_test.py [agent-name] [--shot]

The kit's run.sh runs the game in real time (the client runs on a thread of its own) with a menu open, on ports of
its own (447x: never a real OBS's 4455). Checked:
  1. nothing listening (vr_obs_process_check 0): the row hidden; vr_obs_process_check 2 (OBS's process "found"): the
     hint "OBS found: enable its WebSocket server"; vr_obs 0: hidden.
  2. no password: "OBS: Not recording"; the row pressed (the laser and the trigger) twice within 1.5 s: one
     ToggleRecord, "OBS: Recording 00:00:0x"; pressed again: "OBS: Not recording"; OBS quits: the row hidden.
  3. a password: none set: "OBS: password needed"; a wrong one: "OBS: wrong password" (closed with 4009); the right
     one: identified, "OBS: Not recording". The password is never printed.
  4. no frame stalls (run.sh --exclusive): vr_bench's frame p99 and the main thread's worst CPU work with vr_obs 0,
     while it tries a port nothing listens on (each refused connection ~2 s on its thread), and while connected and
     recording (a toggle in it).
--shot: the headset's eyes with a mock already recording for 12:34 (scratch/obs_row.png).
"""

import json
import os
import re
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from obs_mock_server import MockObs  # noqa: E402

KIT = "C:/OHWorkspace/qvr-kit"
BASH = "C:/Program Files/Git/bin/bash.exe" if os.path.isfile("C:/Program Files/Git/bin/bash.exe") else "bash"
PRESS = ["vr_mock_button main trigger 1", "wait3", "vr_mock_button main trigger 0", "wait3"]


def run(name, script, out=None, exclusive=False):
    args = [BASH, f"{KIT}/run.sh"] + (["--exclusive"] if exclusive else []) + [name, "-RealTime", "-Timeout", "180",
            "-Filter", r"^obs|^vr_obs|^vr_bench: |^vr_mock_laser|rror", "-Script", ";".join(script + ["toggleconsole", "quit"])]
    if out:
        args += ["-Clean", "-Out", out]
    r = subprocess.run(args, capture_output=True, text=True, timeout=600)
    if r.returncode or not r.stdout.strip():
        print(f"run.sh exit {r.returncode}: {r.stderr.strip()[-2000:]}")
    return r.stdout


def rows(out):
    return re.findall(r'obs: menu row (hidden|"[^"]*")', out)


def main():
    name = next((a for a in sys.argv[1:] if not a.startswith("--")), "obsrec")
    checks = []

    def check(what, ok):
        checks.append((what, bool(ok)))

    start = ["developer 1", "menu_main", "wait20"]

    if "--shot" in sys.argv:
        mock = MockObs(4474, recording_ms=754000, quiet=True).start()
        out = run(name, start + ["vr_obs_process_check 0", "vr_obs_port 4474", "vr_obs_connect", "wait120",
                                 "vr_mock_laser obs", "wait30", "vr_obs_status", "vr_eyeshot 3", "wait10"], out="obs_row.png")
        mock.stop()
        print(out.strip()[-800:])
        return 0

    # 1. Nothing listening.
    out = run(name, start + ["vr_obs_process_check 0", "vr_obs_port 4479", "vr_obs_connect", "wait270", "vr_obs_status",
                             "vr_obs_process_check 2", "vr_obs_connect", "wait270", "vr_obs_status",
                             "vr_obs 0", "wait5", "vr_obs_status"])
    r = rows(out)
    print("--- nothing:", r)
    check("nothing listening: hidden", len(r) == 3 and r[0] == "hidden" and "obs: not found" in out)
    check("OBS 'running', no server: the hint", len(r) == 3 and r[1] == '"OBS found: enable its WebSocket server"')
    check("vr_obs 0: hidden", len(r) == 3 and r[2] == "hidden")

    # 2. No password: the row's press.
    mock = MockObs(4472, quiet=True).start()

    def quit_when_stopped():  # OBS quits a second after the second toggle
        t0 = time.time()
        while time.time() - t0 < 150 and mock.counts.get("stopped", 0) < 1:
            time.sleep(0.05)
        time.sleep(1.0)
        mock.stop()

    threading.Thread(target=quit_when_stopped, daemon=True).start()
    out = run(name, start + ["vr_obs_process_check 0", "vr_obs_port 4472", "vr_obs_connect", "wait90", "vr_obs_status",
                             "vr_mock_laser obs", "wait10"] + PRESS + PRESS + ["wait300", "vr_obs_status"] + PRESS +
              ["wait60", "vr_obs_status", "wait360", "vr_obs_status"])
    r = rows(out)
    print("--- toggle:", r, mock.counts)
    check("connected: Not recording", len(r) == 4 and r[0] == '"OBS: Not recording"')
    check("two presses within 1.5 s: one ToggleRecord, recording",
          len(r) == 4 and re.fullmatch(r'"OBS: Recording 00:00:0[2-6]"', r[1]) and mock.counts.get("recording") == 1)
    check("pressed again: Not recording", len(r) == 4 and r[2] == '"OBS: Not recording"' and mock.counts.get("stopped") == 1)
    check("OBS quit: hidden", len(r) == 4 and r[3] == "hidden")
    check("events subscribed (Outputs: 64)", mock.counts.get("identified (eventSubscriptions 64)") == 1)

    # 3. A password.
    secret, wrong = "hunter2-mock", "not-it-mock"
    mock = MockObs(4473, password=secret, quiet=True).start()
    out = run(name, start + ["vr_obs_process_check 0", "vr_obs_port 4473", "vr_obs_password \"\"", "vr_obs_connect", "wait90",
                             "vr_obs_status", f"vr_obs_password {wrong}", "wait1600", "vr_obs_status",
                             f"vr_obs_password {secret}", "wait90", "vr_obs_status", "vr_obs_password \"\""])
    mock.stop()
    r = rows(out)
    print("--- password:", r, mock.counts)
    check("no password: 'password needed'", len(r) == 3 and r[0] == '"OBS: password needed"')
    check("wrong password: 'wrong password' (4009), not tried again on its own",
          len(r) == 3 and r[1] == '"OBS: wrong password"' and mock.counts.get("auth failed") == 1)
    check("right password: identified, Not recording", len(r) == 3 and r[2] == '"OBS: Not recording"')
    check("the password never printed", secret not in out and wrong not in out)

    # 4. Frame times: vr_obs 0, trying a port nothing listens on, connected (a toggle in the middle).
    mock = MockObs(4475, quiet=True).start()
    bench = lambda tag: [f"vr_bench_begin obs_{tag} 450", "wait460"]
    out = run(name, start + ["vr_obs 0", "wait30"] + bench("off") +
              ["vr_obs 1", "vr_obs_process_check 0", "vr_obs_port 4479", "vr_obs_connect"] + bench("trying") +
              ["vr_obs_port 4475", "vr_obs_connect", "wait30", "vr_bench_begin obs_connected 450", "wait200", "vr_obs_toggle",
               "wait260"], exclusive=True)
    mock.stop()
    stats = {}
    for tag in ("off", "trying", "connected"):
        try:
            with open(f"{KIT}/bases/{name}/qbase/quakevr/profile/bench/obs_{tag}.json") as fh:
                f = json.load(fh)["frame"]
            stats[tag] = (f["frame_ms"]["p99"], f["cpu_busy_ms"]["max"])
        except (OSError, KeyError, ValueError):
            pass
    print("--- frames (period p99, cpu work max; ms):", stats, mock.counts)
    base = stats.get("off")
    for tag in ("trying", "connected"):
        s = stats.get(tag)
        check(f"no stall while {tag} ({s} vs off {base})", s and base and s[0] < base[0] + 1.0 and s[1] < 11.0)
    check("connected run toggled", mock.counts.get("recording") == 1)

    failed = [w for w, ok in checks if not ok]
    for w, ok in checks:
        print(("PASS " if ok else "FAIL ") + w)
    print(f"{len(checks) - len(failed)}/{len(checks)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
