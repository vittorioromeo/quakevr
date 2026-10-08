"""Validate forward/reverse teleporter AI fixtures, including rotated aim."""
from pathlib import Path
import re
import sys


def cases(path):
    log = Path(path).read_text(errors="replace")
    assert "PORTAL_AI_REVIEW_DONE" in log
    assert not any(error in log for error in ("Host_Error", "Sys_Error", "shader failed"))
    return {name: body for name, body in re.findall(
        r"CASE_(\w+)\s+(.*?)(?=CASE_|PORTAL_AI_REVIEW_DONE)", log, re.S)}


def health(body):
    return float(re.search(r"health=([\d.]+)", body)[1])


for index, path in enumerate(sys.argv[1:]):
    c = cases(path)
    assert "found=1 enemy_player=1 physical=0 pvs=0" in c["acquire"]
    assert "range=1" in c["acquire"]
    assert "muzzle_clear=1 edge_clear=0" in c["muzzle"]
    assert health(c["bullet"]) < 1000
    assert health(c["laser"]) < health(c["bullet"])
    assert health(c["lightning"]) < health(c["laser"])
    assert health(c["wizard"]) < health(c["lightning"])
    assert health(c["delayed_closed"]) == health(c["wizard"])
    assert "projectiles=0 pending=0" in c["delayed_closed"]
    for name in ("closed_acquire", "near_blocked", "far_blocked", "behind", "ai_off", "outside"):
        assert "found=0 enemy_player=0" in c[name], (path, name)
    for name in ("near_blocked", "far_blocked"):
        assert "muzzle_clear=0" in c[name], (path, name)
    assert health(c["native_attack"]) < health(c["wizard"])
    assert "moved=0.00 enemy_player=1" in c["native_attack"]
    if index == 0:
        assert "found=1 enemy_player=1 physical=0" in c["remote_close"]
        assert "range=1" in c["remote_close"]  # Folded distance <120 still cannot trigger melee.
        assert "found=1 enemy_player=1 physical=1" in c["local"]
        assert "gate=0 range=0" in c["local"]
        assert "found=1 enemy_player=1 physical=0" in c["rotated_acquire"]
        assert health(c["rotated_bullet"]) < health(c["native_attack"])
        assert health(c["rotated_laser"]) < health(c["rotated_bullet"])

assert len(sys.argv) == 3, "Supply forward and reverse fixture logs"
print("PASS: bidirectional acquisition outside PVS, folded range/yaw, muzzle and room blockers, hitscan/projectile/lightning damage, delayed cancellation, native attacks without navigation, rotated aim")
