"""attach_holster_check.py <label> <log|-> -- holsterattach: the holstered guns' bayonets as drawn (vr_embed_test 88 sets
them: the shotgun in the right hip holster, the super shotgun broken open in the left hip one; attach_test.sh).

Reads the client's vr_attach_report blocks (after `attachholster: set`; each block ends `attach_report: N drawn`): every
block must draw a bayonet on each holstered gun (where=holster kind=1, gun=progs/v_shot.mdl and progs/v_shot2.mdl) with
its mount on the gun's mount as the gun is drawn (gap=, on the super shotgun's open barrels) within MAX_GAP units.
Prints one `ok|FAIL <what>` line per check and `attachholster_<label>: N passed, M failed`."""
import re
import sys

MAX_GAP = 0.05  # world units between the bayonet's drawn mount and the gun's (the same transform: float noise only)
GUNS = ("progs/v_shot.mdl", "progs/v_shot2.mdl")
LINE = re.compile(r"attach_drawn: where=(\w+) ent=-?\d+ kind=(\d+) gun=(\S+) at=\S+ \S+ \S+ gap=([-\d.]+)")


def main():
    label = sys.argv[1]
    text = sys.stdin.read() if sys.argv[2] == "-" else open(sys.argv[2], encoding="utf-8", errors="replace").read()
    lines = text.splitlines()
    start = next((i for i, l in enumerate(lines) if "attachholster: set" in l), None)
    results = []
    if start is None:
        results.append((False, "the scene was set (attachholster: set)"))
    else:
        blocks, cur = [], []
        for l in lines[start + 1:]:
            m = LINE.search(l)
            if m:
                cur.append((m.group(1), int(m.group(2)), m.group(3), float(m.group(4))))
            elif "attach_report:" in l:
                blocks.append(cur)
                cur = []
        results.append((len(blocks) >= 1, "a vr_attach_report after the scene (%d)" % len(blocks)))
        for b, drawn in enumerate(blocks):
            for gun in GUNS:
                got = [d for d in drawn if d[0] == "holster" and d[1] == 1 and d[2] == gun]
                ok = len(got) == 1 and 0.0 <= got[0][3] <= MAX_GAP
                results.append((ok, "report %d: the bayonet on the holstered %s drawn at its mount (gap %s)" %
                                (b + 1, gun, got[0][3] if got else "none")))
    for ok, what in results:
        print("%s %s" % ("ok" if ok else "FAIL", what))
    passed = sum(1 for ok, _ in results if ok)
    print("attachholster_%s: %d passed, %d failed" % (label, passed, len(results) - passed))


if __name__ == "__main__":
    main()
