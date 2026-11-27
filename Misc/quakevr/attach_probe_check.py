"""attach_probe_check.py <mode: vr|flat> <log file, - stdin> -- bayonetfix's client-side checks for attach_test.sh.

Reads a run's filtered console lines and prints one `<1|0> <what>` line per check (the shell's `check` takes them):
  - the HUD while the hand drives a gun stuck by its bayonet: after each `hud probe: weapon W clip C` marker (QC
    vr_attach_test.qc VR_AT_HudCheck), the next `flathud: right hand:` line (vr_flathud_report) shows weapon W, shown; in
    flat play its big number the clip C (the status bars' clip);
  - the bayonet taken off into the off hand: after the `held probe: the off hand carries vr_bayonet` marker
    (VR_AT_HeldProbe), the next `held_report: off` line (vr_held_report) has the entity the server says that hand carries
    drawn there, its model the
    bayonet's, its blade out of the fist on the index finger's side (its +x up in the hand), and in flat play on the screen.
"""
import re
import sys

mode, path = sys.argv[1], sys.argv[2]
text = sys.stdin.read() if path == "-" else open(path, encoding="utf-8", errors="replace").read()
lines = text.splitlines()


def out(ok, what):
    print(f"{1 if ok else 0} {what}")


def after(i, pattern):
    for line in lines[i + 1:i + 40]:
        m = re.search(pattern, line)
        if m:
            return m
    return None


huds = 0
helds = 0
for i, line in enumerate(lines):
    m = re.search(r"hud probe: weapon (\d+) clip (\d+)", line)
    if m:
        huds += 1
        wid, clip = int(m.group(1)), int(m.group(2))
        r = after(i, r"flathud: right hand: weapon (-?\d+) shown (\d) count (-?\d+) pic (-?\d+) big (-?\d+)")
        ok = r is not None and int(r.group(1)) == wid and r.group(2) == "1" and int(r.group(3)) > 0
        if mode == "flat":
            ok = ok and int(r.group(5)) == clip
        out(ok, f"{mode}: the HUD shows the stuck gun's ammo (weapon {wid}, clip {clip}): " +
            (r.group(0) if r else "no flathud report"))
    m = re.search(r"held probe: the off hand carries (\w+)", line)
    if m:
        helds += 1
        r = after(i, r"held_report: off carried=(\d+) ent=(\d+) drawn=(\d) model=(\S+) x_in_hand=(\S+) (\S+) (\S+) inview=(\d)/8")
        ok = (r is not None and m.group(1) == "vr_bayonet" and int(r.group(1)) > 0 and r.group(1) == r.group(2) and
              r.group(3) == "1" and r.group(4) == "progs/bayonet.mdl" and float(r.group(7)) > 0.3)
        if mode == "flat":
            ok = ok and int(r.group(8)) >= 4
        out(ok, f"{mode}: the bayonet taken off is drawn in the off hand, blade up out of the fist" +
            (", on the screen" if mode == "flat" else "") + ": " + (r.group(0) if r else "no held report"))
out(huds == 2, f"{mode}: two HUD probes (the shotgun's, the super shotgun's): {huds}")
out(helds == 1, f"{mode}: one held probe: {helds}")
