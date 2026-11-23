#!/bin/bash
# melee_space_test.sh <agent> [space]: how close melee monsters come to the player's head while they attack him
# (ROUND22.md, "Melee space"). vrtesthall, god, the player standing still; each monster spawned 128 units ahead in turn
# (a fresh map each), left to attack for 900 frames; vr_enemy_melee_space_status's closest approach: its box to the
# head (level, < 0: the head inside it) and its origin to the head, overall and while it attacked in melee.
# Run once with Melee Space off (0) and once at [space] (default 16), a batched case each. PASS: with it on, no monster's box came nearer
# the head than space - 2 while attacking (but the fiend: its leap is a throw, pushed back after it lands), and the
# knights still stood within their blows' reach (ai_melee: 60 units origin to origin; here under 52 level, head to
# origin). AD's monsters (ad_ai_charge and kin) need AD installed: listed only when they spawned.
AGENT=${1:?worktree name}
SPACE=${2:-16}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$TREE/scratch"
# ogre, zombie, shambler, knight, hell knight, rottweiler, fiend, mummy, phantom swordsman, MG3's infected knight and
# death knight; AD's death knight, zombie knight, hunter ogre, minotaur, fury
MONSTERS=${MONSTERS:-"1 2 3 5 6 7 9 14 15 31 33 200 201 203 215 216"}
source "$(dirname "$0")/qvrtest.sh"
# (its runs played as batches: a case for each monster and setting, copies in parallel; TESTING.md "Batched test
# cases". testbatch3: was one game a setting, the monsters one after another on fresh maps)
source "$(dirname "$0")/testbatch/testbatch.sh"
# (undrawn, vr_mock_fast 2: the distances are the server's; the table the same digit for digit as the old one-game runs
# drawn, testbatch3: 97 s -> 38 s. TB_NODRAW=0: drawn)
export TB_RATE=${TB_RATE-72} TB_NODRAW=${TB_NODRAW-1}
run() {
    : > "$TREE/scratch/melee_space_$1.txt"
    for M in $MONSTERS; do
        tb_run_sh -Script "developer 1;sv_random_seed 7;vr_enemy_melee_space $1;map vrtesthall;wait30;god;vr_enemy_melee_space_status reset;vr_test_spawn $M;vr_test_spawn_dist 128;impulse 241;wait900;echo melee space: monster $M;vr_enemy_melee_space_status;toggleconsole;quit" -Filter "melee space:|CRASH|Host_Error|SYS_ERROR" -Timeout 300 >> "$TREE/scratch/melee_space_$1.txt" 2>&1
    done
}
tb_body() {
run 0
run "$SPACE"
[ "$TB_PASS" = collect ] && return
python - "$TREE/scratch/melee_space_0.txt" "$TREE/scratch/melee_space_$SPACE.txt" "$SPACE" <<'EOF'
import sys, re
def rows(p):
    r, num = {}, None
    for line in open(p, encoding='utf-8', errors='replace'):
        m = re.search(r'melee space: monster (\d+)', line)
        if m:
            num = m.group(1)
            continue
        m = re.search(r'melee space: (\S+) gap ([-\d.]+) dist ([-\d.]+) melee gap ([-\d.]+) dist ([-\d.]+) pushed ([-\d.]+) in (\d+)', line)
        if m and num:
            r[num + ' ' + m.group(1)] = [float(v) for v in m.groups()[1:]]
    return r
off, on, space = rows(sys.argv[1]), rows(sys.argv[2]), float(sys.argv[3])
fails = []
print(f'{"monster":30s} {"off: gap dist (melee: gap dist)":34s} {"on: gap dist (melee: gap dist) pushed/frames":44s}')
for k in sorted(set(off) | set(on), key=lambda k: int(k.split()[0])):
    a, b = off.get(k), on.get(k)
    fa = f'{a[0]:6.1f} {a[1]:6.1f} ({a[2]:6.1f} {a[3]:6.1f})' if a else '-'
    fb = f'{b[0]:6.1f} {b[1]:6.1f} ({b[2]:6.1f} {b[3]:6.1f}) {b[4]:6.1f}/{int(b[5])}' if b else '-'
    print(f'{k:30s} {fa:34s} {fb:44s}')
    if b and b[2] > -900 and b[2] < space - 2 and 'demon' not in k:
        fails.append(f'{k} melee gap {b[2]:.1f}')
    if b and k.split()[1] in ('monster_knight', 'monster_hell_knight') and not (-900 < b[3] < 52):
        fails.append(f'{k} out of its blows reach ({b[3]:.1f})')
if not on:
    fails.append('no rows')
print('FAIL: ' + '; '.join(fails) if fails else 'PASS')
print(f'melee_space: {0 if fails else 1} passed, {1 if fails else 0} failed')
sys.exit(1 if fails else 0)
EOF
[ $? = 0 ] || fail=1
}
tb_main "$AGENT" tb_body
exit $fail
