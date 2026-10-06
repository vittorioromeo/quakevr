# flick_test.py: writes quakevr/flick_test.cfg (the flashlight wrist flick, vr_flashlight_flick; ROUND21.md "Flashlight:
# turned over by a flick of the wrist"). Run: python Misc/quakevr/flashflick/flick_test.py, then
# bash <kit>/run.sh <name> -Script "map e1m1;wait60;exec flick_test.cfg;toggleconsole;quit" -Filter "^==|flick:"
# Writes quakevr/flick_test.cfg: synthetic wrist motions of the main hand holding the flashlight.
X, Y, Z = -0.35, 1.1, -0.35
out = []
def pose(p=0, yaw=0, roll=0, dx=0.0):
    out.append(f"vr_mock_hand off {X+dx:.4f} {Y} {Z} {p:.2f} {yaw:.2f} {roll:.2f};wait")
def W(n):
    return ";".join(["wait"] * n)
def settle(n=90):
    out.append(";".join(["wait"] * n))
def ramp(key, a, b, frames, **kw):
    for i in range(1, frames + 1):
        v = a + (b - a) * i / frames
        args = dict(kw); args[key] = v
        if 'move' in args:
            args['dx'] = args.pop('move') * i
        pose(**args)
def case(name):
    out.append(f'echo "== {name}"')
out += ["vr_fixed_frames 0", "vr_flashlight 1", "god", "notarget", "vr_mock_hand main 0.35 1.1 -0.35 0 0 0", "vr_mock_hand off -0.35 1.1 -0.35 0 0 0", W(150), "vr_flashlight_give left", W(30), "vr_flashlight_probe", "vr_fixed_frames 1", "vr_flashlight_flick_debug 1"]
pose(0); settle()
case("slow wrist up 60 deg over 60 frames, back"); ramp('p', 0, -60, 60); ramp('p', -60, 0, 60); settle()
case("aim around slowly: pitch 0..40, yaw 0..40 over 40 frames"); [pose(-i, i) for i in range(1, 41)]; [pose(-i, i) for i in range(40, -1, -1)]; settle()
case("flick up 60 deg in 6 frames, back slowly"); ramp('p', 0, -60, 6); settle(30); ramp('p', -60, 0, 60); settle()
case("flick down 60 deg in 6 frames, back slowly"); ramp('p', 0, 60, 6); settle(30); ramp('p', 60, 0, 60); settle()
case("flick up and straight back (6+6 frames): one flip"); ramp('p', 0, -60, 6); ramp('p', -60, 0, 6); settle()
case("flick up, back, again at 0.5 s: two flips"); ramp('p', 0, -60, 6); ramp('p', -60, 0, 6); settle(40); ramp('p', 0, -60, 6); ramp('p', -60, 0, 6); settle()
case("twist (roll 70 deg in 6 frames)"); ramp('roll', 0, 70, 6); ramp('roll', 70, 0, 6); settle()
case("wave sideways (yaw 60 deg in 6 frames)"); ramp('yaw', 0, 60, 6); ramp('yaw', 60, 0, 6); settle()
case("flick during a swing (hand moving fast)"); ramp('p', 0, -60, 6, move=0.1); ramp('p', -60, 0, 6, move=-0.1); pose(0); settle()
out.append("vr_flashlight_flick_speed 1200")
case("strength 1200: the 60-in-6 flick"); ramp('p', 0, -60, 6); ramp('p', -60, 0, 6); settle()
case("strength 1200: 90 in 4 frames"); ramp('p', 0, -90, 4); settle(30); ramp('p', -90, 0, 60); settle()
out.append("vr_flashlight_flick_speed 600;vr_flashlight_flick 0")
case("off: the flick"); ramp('p', 0, -60, 6); ramp('p', -60, 0, 6); settle()
out.append("vr_flashlight_flick 1")
case("end")
open("quakevr/flick_test.cfg", "w").write("\n".join(out) + "\n")
