#!/usr/bin/env python3
"""ad_active_test.py <agent> [ids...] -- Arcane Dimensions' humanoids driven alive (ROUND22.md, "Active rigs for AD's
humanoids"; vr_box3d.cpp activeClasses): each AD humanoid (the debug spawner's choice, vr_ad_monsters.qc), one game on
vrfiringrange through AD's campaign (AD's data linked into the agent's base, qbase/ad), spawned 64 units ahead:

  shove    a sure two-handed shove (vr_knockdown_chance 100; vr_knockdown_test 22, as a real one): it staggers as a pinned
           ragdoll ("stagger: <n> <class> shoved") and falls ("staggered and fell"): knocked down;
  getup    it rises through its get-up with its body ("getup: <n> rises through frames", then "rises through its
           get-up"), handed back standing;
  neck     its neck snapped standing (vr_foegrab_necksnap_test): its head joint loose, its head flops (NECKANGLE every 12
           frames: the swing of its head from its chest at least 40 degrees and moving 20 or more).

The lift's flail: foegrab/judo_test.py adneck (its lift verdict needs "it flails in the hands" for these classes).
Prints a PASS/FAIL line per class and check, then FAILS <n>; each class's log in scratch/ad_active_<class>.txt.
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "testbatch"))
import testbatch  # noqa: E402

os.environ.setdefault("TB_NODRAW", "1")

AGENT = sys.argv[1] if len(sys.argv) > 1 else "dequant"
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
SCRATCH = os.path.join(TREE, "scratch")

# The AD humanoids driven alive (the debug spawner's choices; the centurion (220, on its hover board) and the skull
# wizard (206, a robe) are not).
ACTIVE = {227: "monster_army_grenade", 223: "monster_army_plasma", 211: "monster_army_rocket", 212: "monster_defender",
          217: "monster_eliminator", 232: "monster_pyro", 235: "monster_fumigator", 202: "monster_dcrossbow",
          216: "monster_dfury", 205: "monster_dguard", 200: "monster_dknight", 225: "monster_dsergeant",
          201: "monster_zombiek", 238: "monster_mummy"}
PICK = [int(a) for a in sys.argv[2:]] or list(ACTIVE)

PRE = ("sv_random_seed 7;skill 1;vr_campaign_native ad;wait30;map vrfiringrange;wait60;god;notarget;developer 1;"
       "vr_ad_everywhere 1;vr_knockdown_debug 1;vr_ragdoll_active_seed 0;vr_test_spawn {mon};vr_test_spawn_dist 64;"
       "impulse 241;wait60")
SAMPLES = "".join(";wait12;vr_foegrab_neck_angle" for _ in range(8))
# (Risen, it may walk off: walked up to again, so the neck snapped is its, not the range's grunt's.)
BODY = ("vr_knockdown_chance 100;vr_knockdown_test 22;wait90;vr_knockdown_test 1;wait420;vr_knockdown_chance 1;"
        "vr_mock_walk_to nearest {cls} 40;wait200;vr_mock_walk_to off;wait10;vr_foegrab_necksnap_test" + SAMPLES)
FILTER = "^knockdown|^stagger|^getup|NECKANGLE|necksnap|rror|CRASH"
FAILS = 0


def verdict(ok, what):
    global FAILS
    print(f"  {'PASS' if ok else 'FAIL'} {what}")
    if not ok:
        FAILS += 1


def main():
    global FAILS
    FAILS = 0  # (testbatch.two_pass runs this twice: the first pass's verdicts mean nothing)
    os.makedirs(SCRATCH, exist_ok=True)
    for mon in PICK:
        cls = ACTIVE[mon]
        short = cls.replace("monster_", "")
        out = testbatch.run_sh(AGENT, ["-Script", f"{PRE.format(mon=mon)};{BODY.format(cls=cls)};toggleconsole;quit", "-Filter", FILTER,
                                       "-Timeout", "300"]).replace("\r", "")
        with open(os.path.join(SCRATCH, f"ad_active_{short}.txt"), "w") as f:
            f.write(out)
        flat = " ".join(out.split())
        bad = "rror" in flat or "CRASH" in flat
        staggered = re.search(rf"stagger: \d+ {cls} shoved", flat) is not None
        fell = f"{cls} staggered and fell" in flat or re.search(rf"knockdown: \d+ {cls} down as a ragdoll", flat) is not None
        rose = re.search(r"getup: \d+ rises through frames (\d+) to (\d+)", flat)
        up = f"{cls} rises through its get-up" in flat
        loose = re.search(rf"necksnap: \d+ {cls}'s head joint loose", flat) is not None
        swings = [float(s) for s in re.findall(r"NECKANGLE \d+ swing ([-\d.]+)", flat)]
        flops = bool(swings) and max(swings) >= 40.0 and max(swings) - min(swings) >= 20.0
        verdict(staggered and fell and not bad, f"{short} shove: {'staggered' if staggered else 'no active stagger'}, "
                                                f"{'knocked down' if fell else 'not down'}")
        verdict(rose is not None and up and not bad,
                f"{short} getup: {'frames ' + rose.group(1) + '-' + rose.group(2) if rose else 'no body rise'}"
                f"{', handed back standing' if up else ', not back up'}")
        verdict(loose and flops and not bad,
                f"{short} neck: {'loose' if loose else 'not loose'}, swing "
                f"{min(swings) if swings else 0:.0f}-{max(swings) if swings else 0:.0f} deg")
    print(f"FAILS {FAILS}")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(testbatch.two_pass(AGENT, "ad_active_test", main))
