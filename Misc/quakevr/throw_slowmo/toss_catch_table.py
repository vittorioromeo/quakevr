# toss_catch_table.py <log> -- toss_catch_test.sh's lines: a trial each, its throw's speed and whether it was caught.
import sys, re
trial = None; speed = None; caught = None; out = []
def flush():
    if trial: out.append(f"  toss {trial[0]} cm, grip {float(trial[1]):.2f} s after: throw {speed or '-':>5s} m/s  {caught or 'MISSED'}")
for l in open(sys.argv[1], errors="replace"):
    if l.startswith("TRIAL"):
        trial = l.split()[1:3]; speed = None; caught = None
    elif l.startswith("throw main") and trial and speed is None:
        speed = re.match(r"throw main: ([\d.]+)", l).group(1)
    elif "taken into the main" in l and trial and caught is None:
        m = re.search(r"([\d.]+) units from its handle", l); caught = f"caught {m.group(1) if m else '?'} u from the handle"
    elif l.startswith("CHECK"):
        flush(); trial = None
print("\n".join(out) if out else "  (no trials)")
