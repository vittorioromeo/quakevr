#!/usr/bin/env python3
"""hub_start_test.py <agent>: the hub's teleporter to a campaign with its own game folder (vrnotes1122c2, his note
e5start_2026-11-22_15-29-11: Start landed in Dimension of the Past, then he was back in the hub 3.3 s later).

The game is driven through its -cmdinbox (as mp_test.py does), not a start-up script: the teleporter's Run Setup
(QC localcmd) and its changelevel go to the end of the command buffer, behind whatever a script still holds, so a
script's later commands would hold them back; with the buffer empty they run as in real play.

  1. vrstart; Dimension of the Past chosen (vr_activestartpaknameidx 3, its lectern's command); the player set
     in the teleporter: the Run Setup screen opens (its cursor on Start);
  2. A pressed on Start: the campaign switch, e5start loads;
  3. on e5start: the trigger and A pressed as a player would, then 5 s (360 fixed frames): still e5start, no VR Hub
     ("VR Hub: going there"), the buttons the game's (no menu took them).
Ends "hub_start_test: N passed, M failed". Needs Dimension of the Past's data (the kit's qbase finds the rerelease).
"""
import os
import re
import subprocess
import sys
import time

import qvrtest


def main():
    agent = sys.argv[1] if len(sys.argv) > 1 else "round25"
    tree = qvrtest.tree(agent)
    os.makedirs(tree + "/scratch", exist_ok=True)
    inbox = tree + "/scratch/hub_start_inbox.txt"
    out = tree + "/scratch/hub_start_run.txt"
    log_path = qvrtest.game_dir(agent) + "/qconsole.log"
    open(inbox, "w").close()
    run = subprocess.Popen([qvrtest.BASH, qvrtest.KIT + "/run.sh", agent, "-Script", "vr_log_flush_frames 1;map vrstart;wait10;echo HS_READY",
                            "-ExtraArgs", "-cmdinbox " + inbox, "-Timeout", "300", "-Filter", "."],
                           stdout=open(out, "w"), stderr=subprocess.STDOUT)
    started = time.time()

    def log():
        try:
            return open(log_path, encoding="latin1").read()
        except OSError:
            return ""

    def wait_for(pattern, count=1, limit=180.0):
        # (the harness's own safety net: the game's log is read until the line shows, never a fixed sleep)
        while time.time() - started < 280:
            text = log()
            if text.count(pattern) >= count and os.path.getmtime(log_path) >= started:
                return text
            if run.poll() is not None:
                break
            time.sleep(0.1)
        return None

    def send(line):
        # (waitN as run.sh spells it: N wait commands, a frame each)
        parts = []
        for c in line.split(";"):
            m = re.fullmatch(r"wait(\d+)", c.strip())
            parts += qvrtest.waits(int(m.group(1))) if m else [c]
        with open(inbox, "a") as f:
            f.write(";".join(parts) + "\n")

    tally = qvrtest.Tally("hub_start_test")
    check = tally.check

    ready = wait_for("HS_READY")
    check(ready is not None, "vrstart loaded")
    if ready:
        send("vr_debug_buttons 1;vr_activestartpaknameidx 3;setpos -1150 46 154 0 90 0;noclip;wait30;echo HS_ASKED")
        wait_for("HS_ASKED")
        # (the teleporter's localcmd ran right after: the Run Setup screen, its cursor on Start)
        send("wait2;vr_mock_button main primary 1;wait5;vr_mock_button main primary 0;echo HS_PRESSED")
        landed = wait_for("Using protocol", 2, 120)
        text = log()
        check("Run setup: " in text.split("HS_ASKED", 1)[-1], "Start pressed (\"Run setup:\")")
        check(bool(landed), "Dimension of the Past's start map loaded")
        if landed:
            send("wait30;echo HS_LANDED;vr_mock_button main trigger 1;wait10;vr_mock_button main trigger 0;wait20;"
                 "vr_mock_button main primary 1;wait10;vr_mock_button main primary 0;wait360;status;echo HS_STAYED")
            stayed = wait_for("HS_STAYED", 1, 200)
            after = (stayed or log()).split("HS_LANDED", 1)[-1]
            check(stayed is not None, "5 s on e5start")
            check("VR Hub: going there" not in after and "Using protocol" not in after,
                  "never back to the hub (no map loaded since)")
            check(re.search(r"map:\s+e5start", after) is not None, "status: map e5start")
            presses = [l for l in after.splitlines() if l.startswith("VR buttons:")]
            check(len(presses) >= 4 and all("not in the game" not in l and "taken by" not in l for l in presses),
                  "the trigger and A went to the game (%d button lines, no menu)" % len(presses))
    send("disconnect;quit")
    try:
        run.wait(timeout=60)
    except subprocess.TimeoutExpired:
        check(False, "the game quit")
    return tally.verdict()


if __name__ == "__main__":
    sys.exit(main())
