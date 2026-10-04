"""Check notes_feedback_review.ps1's disposable-game console log."""
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text(errors="replace")
assert "REVIEW_DONE" in text
assert "Host_Error" not in text and "Sys_Error" not in text
for tag, mode, on in (
    ("head_load_on", "onhead", 1), ("head_load_off", "onhead", 0),
    ("head_transition_off", "onhead", 0), ("gun_load_on", "ongun", 1),
    ("gun_transition_on", "ongun", 1),
):
    assert re.search(rf"torchprobe {tag} mode {mode} .* on {on} overhead", text), tag
assert "bodycheck: monster_enforcer health=-8 ragdoll=1" in text
held = [re.search(rf"torchprobe {tag} mode held holder 0 placed 1 on 0 overhead (\d \d)", text)
        for tag in ("held_before", "held_load")]
assert all(held) and held[0][1] == held[1][1], "held lamp's flipped grip persists"
assert "torchprobe held_released mode mounted" in text, "restored lamp can be released"
for distance in (96, 128, 160):
    section = text.split(f"CASE_cap_{distance}")[1].split("CASE_")[0]
    bodies = re.findall(r"bodycheck: monster_enforcer health=(-?[\d.]+) ragdoll=(\d)", section)
    assert len(bodies) == 2 and all(float(h) < 0 and r == "1" for h, r in bodies), (distance, bodies)
assert "monster_enforcer: never force grabbed" in text
assert re.search(r"decaptest: 21 .*headless 1, ragdoll 1, .*popped 1, gibbed 0", text)
assert "health=30 ragdoll=1 headless=0 knocked=1 water=3 type=-3" in text
assert "health=-10 ragdoll=1 headless=0 knocked=0 water=3 type=-3" in text
assert "drowned for 12, health -10" in text
samples = [(float(s), int(p), float(v)) for s, p, v in re.findall(
    r"wigglecheck: strength=([\d.]+) parts=(\d+) angular=([\d.]+)", text)]
assert any(s == 0 and p > 0 and v == 0 for s, p, v in samples)
assert max(v for s, p, v in samples if s > 0 and p > 0) > .01
assert "no spawn function" not in text
for weapon in ("shotgun", "supershotgun", "nailgun", "supernailgun", "grenadelauncher",
               "rocketlauncher", "lightning", "mjolnir", "laser_gun", "proximity_gun"):
    assert re.search(rf"vr_physics_spawn: \d+ weapon_{weapon} at", text), weapon
tilts = {int(a): (float(h), float(z)) for a, h, z in re.findall(
    r"tilttest: angle=(\d+) height=([\d.]+) axisZ=(-?[\d.]+)", text)}
assert all(tilts[a][1] == 1 for a in (0, 90, 180))
assert abs(tilts[180][0] / tilts[0][0] - .15) < .001
print("PASS: flashlight saves/transitions, real nail death, ragdoll replacement, head pop, force-grab exclusion, "
      "physical-head drowning, struggling controls, weapon pickups and upward shortened torch flame")
