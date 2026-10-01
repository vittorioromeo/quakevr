# climb_jump.py: a jump at a ledge from against its wall (NOTES.md vrclimb_2026-10-01_11-49-25; ROUND21.md, "Climbing: a
# jump to a ledge from against its wall"). vrclimb's jump wall (a block 96 high, its face at x 0): walk into it with the
# stick, raise the main hand over the head, jump, and press the grip once, at one of `N` times after the jump (0 to
# 0.58 s, every 0.02 s), held 0.4 s; one trial a press time, each from `setpos -40 -310 24`.
#   python climb_jump.py script <dir> [cvars]   writes the plays into <dir> (absolute) and prints the console script;
#                                               cvars: extra commands first (e.g. "vr_climb_air_grab_time 0")
#   python climb_jump.py table <log>            one letter a trial (H the press took the ledge, L leniently, R later, in
#                                               the air, . none) and PASS if every press from 0 to 0.50 s took it
import re
import sys

N = 30
STEP = 0.02
MUST = 26  # presses 0 .. 0.50 s: the hand reaches the lip on the way up, or comes down to it, within the grab window

def script(d, extra):
    d = d.replace("\\", "/").rstrip("/")
    cmds = ["vr_fixed_frames 1", "map vrclimb", "wait30", "vr_climb 1", "vr_climb_debug 1"] + ([extra] if extra else [])
    for k in range(N):
        tp = STEP * k
        play = ["0.000 head 0 1.7 0", "0.000 off -0.20 1.00 -0.20 0 0 0", "0.000 main 0.20 1.00 -0.20 0 0 0",
                "0.000 cmd vr_mock_stick off 0 1",  # into the wall
                "1.200 main 0.20 1.00 -0.20 0 0 0", "1.600 main 0.15 1.90 -0.45 0 0 0",  # the hand up, over the head
                "1.700 cmd +jump", "1.800 cmd -jump", f"{1.7 + tp:.3f} cmd echo climbjump trial {k} press {tp:.2f}",
                f"{1.7 + tp:.3f} cmd +grabmain", f"{2.1 + tp:.3f} cmd -grabmain",
                "3.000 main 0.15 1.90 -0.45 0 0 0", "3.000 cmd vr_mock_stick off 0 0"]
        with open(f"{d}/climbjump_{k}.txt", "w", newline="\n") as f:
            f.write("\n".join(play) + "\n")
        cmds += ["setpos -40 -310 24 0 0 0", "noclip", "wait20", f"vr_mock_play {d}/climbjump_{k}.txt", "wait250"]
    cmds += ["toggleconsole", "quit"]
    print(";".join(cmds))

def table(path):
    lines = []
    with open(path, errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            # (the console wraps long lines)
            if lines and not re.match(r"\s*(climb|exit|\])", line):
                lines[-1] += line
            else:
                lines.append(line)
    res = {}
    cur = None
    for line in lines:
        m = re.search(r"climbjump trial (\d+) press", line)
        if m:
            cur = int(m.group(1))
            res[cur] = "."
            continue
        if cur is not None and res[cur] == "." and "hand holds at" in line:
            res[cur] = "R" if "in the air" in line else ("L" if "lenient" in line else "H")
    s = "".join(res.get(k, "?") for k in range(N))
    ok = all(c in "HLR" for c in s[:MUST])
    print(f"{s}  {sum(c in 'HLR' for c in s)}/{N} took the ledge; presses 0..{STEP * (MUST - 1):.2f} s: "
          f"{'PASS' if ok else 'FAIL'}")
    return ok

if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "script":
        script(sys.argv[2], " ".join(sys.argv[3:]))
    elif len(sys.argv) == 3 and sys.argv[1] == "table":
        sys.exit(0 if table(sys.argv[2]) else 1)
    else:
        print(__doc__ if __doc__ else "usage: climb_jump.py script <dir> [cvars] | table <log>")
        sys.exit(2)
