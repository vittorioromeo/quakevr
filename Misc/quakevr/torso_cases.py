"""Torso direction acceptance poses (ROUND21.md, "Torso direction"): writes a vr_mock_play file of the cases the
author described, with a vr_torso_report at each checkpoint.

    python Misc/quakevr/torso_cases.py            # writes quakevr/motions/torso_cases.mock, prints its length
    bash <kit>/run.sh <name> -Filter "^torso" -Script "map start;wait30;vr_mock_play quakevr/motions/torso_cases.mock;wait<frames>;toggleconsole;quit"

Mock tracking space: x right, y up, forward -z, metres; the head's yaw left positive. Each report prints the head's
yaw, the old engine's estimate and the new one (play-space degrees, left positive).
"""

import math
import os

KEYS = []
T = 0.0


def turned(p, yaw):
    """A body-frame point turned left by `yaw` degrees about the vertical through the head."""
    x, y, z = p
    f, l = -z, -x
    a = math.radians(yaw)
    f2, l2 = f * math.cos(a) - l * math.sin(a), f * math.sin(a) + l * math.cos(a)
    return (-l2, y, -f2)


def pose(t, head_yaw, off, main, body_yaw=None):
    body = head_yaw if body_yaw is None else body_yaw
    KEYS.append(f"{t:.3f} head 0 1.65 0 0 {head_yaw:.1f} 0")
    for name, p in (("off", off), ("main", main)):
        x, y, z = turned(p, body)
        KEYS.append(f"{t:.3f} {name} {x:.3f} {y:.3f} {z:.3f}")


def report(t, label):
    KEYS.append(f"{t:.3f} cmd vr_torso_report {label}")


REST_OFF, REST_MAIN = (-0.2, 1.1, -0.35), (0.2, 1.1, -0.35)
BEHIND = (-0.05, 1.05, 0.22)            # the off hand behind the back
SIDE_OFF, SIDE_MAIN = (-0.25, 0.85, 0.1), (0.25, 0.85, 0.1)  # down by the sides

# 1. Rest, both hands forward.
pose(0.0, 0, REST_OFF, REST_MAIN)
pose(1.5, 0, REST_OFF, REST_MAIN)
report(1.5, "1-rest")

# 2. One hand behind the back, the other moving about in front.
pose(1.8, 0, BEHIND, (0.3, 1.2, -0.35))
pose(3.0, 0, BEHIND, (0.3, 1.2, -0.35))
report(3.0, "2a-behind,main-right")
pose(3.5, 0, BEHIND, (0.0, 1.3, -0.5))
report(3.5, "2b-behind,main-centre(moving)")
pose(4.5, 0, BEHIND, (0.0, 1.3, -0.5))
report(4.5, "2c-behind,main-centre")
pose(5.0, 0, BEHIND, (-0.25, 1.25, -0.4))
report(5.0, "2d-behind,main-left(moving)")
pose(6.0, 0, BEHIND, (-0.25, 1.25, -0.4))
report(6.0, "2e-behind,main-left")
pose(6.5, 0, BEHIND, (0.45, 1.2, -0.2))
pose(7.5, 0, BEHIND, (0.45, 1.2, -0.2))
report(7.5, "2f-behind,main-far-right")

# 3. One hand swept from right to left in front, the other down by the side.
pose(8.0, 0, SIDE_OFF, (0.5, 1.3, -0.25))
pose(9.0, 0, SIDE_OFF, (0.5, 1.3, -0.25))
report(9.0, "3a-sweep-start(right)")
pose(9.75, 0, SIDE_OFF, (0.0, 1.3, -0.55))
report(9.75, "3b-sweep-middle")
pose(10.5, 0, SIDE_OFF, (-0.45, 1.3, -0.3))
report(10.5, "3c-sweep-end(left)")
pose(11.5, 0, SIDE_OFF, (-0.45, 1.3, -0.3))
report(11.5, "3d-swept,held")

# 3'. The same sweep with the other hand forward too.
pose(12.0, 0, REST_OFF, (0.5, 1.3, -0.25))
pose(13.0, 0, REST_OFF, (0.5, 1.3, -0.25))
report(13.0, "3e-both,sweep-start")
pose(14.5, 0, REST_OFF, (-0.45, 1.3, -0.3))
report(14.5, "3f-both,sweep-end")
pose(15.5, 0, REST_OFF, (-0.45, 1.3, -0.3))
report(15.5, "3g-both,swept,held")

# 4. Both hands forward (a gun held out), the whole body turning 90 left in 1.5 s.
GUN_OFF, GUN_MAIN = (-0.05, 1.35, -0.45), (0.05, 1.35, -0.35)
pose(16.0, 0, GUN_OFF, GUN_MAIN)
pose(17.0, 0, GUN_OFF, GUN_MAIN)
report(17.0, "4a-gun,turn-start")
pose(17.75, 45, GUN_OFF, GUN_MAIN)
report(17.75, "4b-gun,turning(45)")
pose(18.5, 90, GUN_OFF, GUN_MAIN)
report(18.5, "4c-gun,turned(90)")
pose(19.0, 90, GUN_OFF, GUN_MAIN)
report(19.0, "4d-gun,turned+0.5s")
pose(20.0, 90, GUN_OFF, GUN_MAIN)
report(20.0, "4e-gun,turned+1.5s")

# 5. A glance: the head 60 further left for 0.4 s, the body and hands still.
pose(20.3, 150, GUN_OFF, GUN_MAIN, body_yaw=90)
pose(20.7, 150, GUN_OFF, GUN_MAIN, body_yaw=90)
report(20.7, "5a-glance(head150,body90)")
pose(21.0, 90, GUN_OFF, GUN_MAIN, body_yaw=90)
pose(22.0, 90, GUN_OFF, GUN_MAIN, body_yaw=90)
report(22.0, "5b-glance-back")

# 6. Normal turning, hands down by the sides: back to 0 in 1 s, then held.
pose(22.5, 90, SIDE_OFF, SIDE_MAIN)
pose(23.5, 90, SIDE_OFF, SIDE_MAIN)
report(23.5, "6a-sides,at90")
pose(24.0, 45, SIDE_OFF, SIDE_MAIN)
report(24.0, "6b-sides,turning(45)")
pose(24.5, 0, SIDE_OFF, SIDE_MAIN)
report(24.5, "6c-sides,turned(0)")
pose(25.5, 0, SIDE_OFF, SIDE_MAIN)
report(25.5, "6d-sides,turned+1s")

# 7. Aiming ahead with both hands, the head looking 45 right: the torso between.
pose(26.0, -45, GUN_OFF, GUN_MAIN, body_yaw=0)
pose(28.0, -45, GUN_OFF, GUN_MAIN, body_yaw=0)
report(28.0, "7-aim-ahead,head-45right")

# 8. Looking far right (100) for a while, hands down: the neck limit takes the torso along.
pose(28.5, -100, SIDE_OFF, SIDE_MAIN, body_yaw=0)
report(28.5, "8a-look-right100")
pose(30.0, -100, SIDE_OFF, SIDE_MAIN, body_yaw=0)
report(30.0, "8b-look-right100+1.5s")

# 9. A glance with the hands hanging down: the head 60 left for 0.6 s, the body still.
pose(30.5, -100, SIDE_OFF, SIDE_MAIN, body_yaw=-100)
pose(31.5, -100, SIDE_OFF, SIDE_MAIN, body_yaw=-100)
report(31.5, "9a-sides,body-100")
pose(31.8, -40, SIDE_OFF, SIDE_MAIN, body_yaw=-100)
pose(32.4, -40, SIDE_OFF, SIDE_MAIN, body_yaw=-100)
report(32.4, "9b-sides,glance-left60")
pose(32.7, -100, SIDE_OFF, SIDE_MAIN, body_yaw=-100)
pose(33.5, -100, SIDE_OFF, SIDE_MAIN, body_yaw=-100)
report(33.5, "9c-sides,glance-back")

END = 34.0
if __name__ == "__main__":
    out = os.path.join(os.path.dirname(__file__), "..", "..", "quakevr", "motions", "torso_cases.mock")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(KEYS) + "\n")
    print(f"wrote {os.path.normpath(out)}: {END} s, wait{int(END * 72) + 30}")
