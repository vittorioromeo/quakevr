"""Validate the teleporter reach and torch-contact integration fixtures."""
from pathlib import Path
import re
import sys
reach, torch = (Path(p).read_text(errors="replace") for p in sys.argv[1:3])
for log in (reach, torch):
    assert "Host_Error" not in log and "Sys_Error" not in log
    assert "shader failed" not in log.lower()
assert "REACH_REVIEW_DONE" in reach and "TORCH_REVIEW_DONE" in torch
across = reach.split("CASE_reach_across", 1)[1].split("CASE_reach_withdraw", 1)[0]
assert "gate=2" in across and "splittrace=1" in across and "error=0.0000" in across
assert "clipped_copies=2 bone_poses=33" in across
assert "copies=1" in reach.split("CASE_prop_straddles", 1)[1].split("CASE_prop_across", 1)[0]
assert "moved to gate 2" in reach and "moved to gate 0" in reach
assert "carry: let go" in reach
for generation in (2, 3, 4):
    assert f"generation={generation} current={generation} copies=0" in reach
for kind in (21, 4, 2, 10, 1, 23, 24):
    assert re.search(rf"walltorch result: kind {kind} detached=1 held=0 blood=0", torch)
assert "walltorch result: kind 2 detached=1 held=0 blood=0 missile=1" in torch
assert "walltorch grip result: detached=1 held=1" in torch
if len(sys.argv) > 3:
    shock = Path(sys.argv[3]).read_text(errors="replace")
    assert "SHOCK_REVIEW_DONE" in shock
    assert "health=29 corpse=0" in shock
    for kind in ("live", "corpse"):
        active = shock.split(f"CASE_{kind}_shock", 1)[1].split(f"CASE_{kind}_expired", 1)[0]
        assert "triangles=810" in active and "active=1" in active
        assert "active=0" in shock.split(f"CASE_{kind}_expired", 1)[1].split("CASE_", 1)[0]
print("PASS: folded reach, shared animated pose, split prop physics, crossing/withdrawal/release, map resets, bloodless torch knockdowns and live grenade contact")
