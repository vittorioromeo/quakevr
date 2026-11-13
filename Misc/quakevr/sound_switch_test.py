"""sound_switch_test.py <agent> [cycles=30]: the sound list across game switches (ROUND25.md, "vkfixes").

id1 and Arcane Dimensions' start maps loaded by turns (vr_campaign_native, `cycles` times each), with sound (the SDL
dummy driver: nothing heard), `vr_limits`' "sounds known" read after each. PASS: the count the same in every id1 load
and in every AD load, id1's below AD's (the other game's sounds forgotten at each switch: S_ForgetGameSounds; the list
grew to their union before and never shrank), no load timed out. Ends on "sound_switch_test: N passed, M failed".
"""
import os
import re
import subprocess
import sys

agent = sys.argv[1]
cycles = int(sys.argv[2]) if len(sys.argv) > 2 else 30
step = "vr_campaign_native {g};vr_waitprint 6000 Client signed on: start;echo sndsw {g};vr_limits"
parts = ["developer 1"]
for _ in range(cycles):
    parts.append(step.format(g="ad"))
    parts.append(step.format(g="id1"))
parts.append("echo sndsw done;toggleconsole;quit")
bash = "C:/Program Files/Git/usr/bin/bash.exe" if os.name == "nt" else "bash"
cmd = [bash, "C:/OHWorkspace/qvr-kit/run.sh", agent, "-Sound", "-Script", ";".join(parts),
       "-Filter", "sndsw|sounds known|timed out|out of sfx|Host_Error:|Sys_Error:", "-QuitOn", "sndsw done",
       "-Timeout", str(120 + cycles * 50)]
res = subprocess.run(cmd, capture_output=True, text=True, env=dict(os.environ, SDL_AUDIODRIVER="dummy"))
log = res.stdout + res.stderr
rows = []
game = None
for line in log.splitlines():
    m = re.search(r"sndsw (\w+)", line)
    if m:
        game = m.group(1)
    m = re.search(r"sounds known\s+(\d+)", line)
    if m and game:
        rows.append((game, int(m.group(1))))
bad = [l for l in log.splitlines() if ("timed out" in l or "out of sfx" in l or "rror:" in l) and "full:" not in l]
ids = [n for g, n in rows if g == "id1"]
ads = [n for g, n in rows if g == "ad"]
print("sound_switch_test: %d loads; sounds known in id1 %s, in AD %s" % (len(rows), sorted(set(ids)), sorted(set(ads))))
for l in bad[:5]:
    print("sound_switch_test: FAIL", l)
ok = len(ids) == cycles and len(ads) == cycles and len(set(ids)) == 1 and len(set(ads)) == 1 and ids[0] < ads[0]
ok = ok and not bad
print("sound_switch_test: %d passed, %d failed" % (1 if ok else 0, 0 if ok else 1))
sys.exit(0 if ok else 1)
