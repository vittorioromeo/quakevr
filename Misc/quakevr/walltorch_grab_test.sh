#!/bin/bash
# walltorch_grab_test.sh <agent> ["<extra commands>"] -- a hand reaching a torch on its wall (vrfiringrange's middle torch,
# -590 -760 75, on the prop wall whose face is x -599) from several directions, to several places on and about it, the
# grip pressed at several times (ROUND21.md, "Wall torches: a grip on the way takes it"). Each reach: the hand moves 12
# units in 1-unit steps (2 frames each) and stays, the grip pressed at step k (0: as the reach starts, about 0.33 s before
# the hand gets there; 6: halfway; 12: on arrival; 15: after), held 12 frames, let go, the hand back out. The torch's
# pull is off (vr_walltorch_pull 999) so the same torch stays on its wall. Prints how many reaches took hold of it
# ("walltorch: gripped on its wall"), by press time and by place, and the misses. The extra commands run before (e.g.
# "vr_walltorch_grab_time 0" for the old press window only). DIRS / TGTS / PRESSES / HAND (main|off) narrow it.
AGENT=$1; X="$2"; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
HAND=${HAND:-main}
if [ "$HAND" = off ]; then GRAB="+graboff"; UNGRAB="-graboff"; OTHER="main 0.35 1.1 -0.2 0 0 0"; else GRAB="+grabright"; UNGRAB="-grabright"; OTHER="off -0.35 1.1 -0.2 0 0 0"; fi
LOG=$(mktemp)
# dx dy dz: the approach (units per step, from the torch out); tgt: the end, from the torch's origin (the stick runs
# from 14 under it to 3 over it; -x is behind it, towards the wall).
for dir in ${DIRS:-1,0,0 1,1,0 1,-1,0 1,0,-1 1,0,1 1,1,-1}; do dir=${dir//,/ }
  P="map vrfiringrange;wait60;god;notarget;developer 1;vr_walltorch_pull 999;$X;setpos -566 -760 40 0 180 0;vr_mock_hand $OTHER;wait30;"
  for tgt in ${TGTS:-0,0,-6 0,0,0 1,3,-4 -3,0,-5 -5,0,-5}; do tgt=${tgt//,/ }
    for press in ${PRESSES:-0 6 12 15}; do
      set -- $dir; dx=$1; dy=$2; dz=$3
      set -- $tgt; tx=$(( -590 + $1 )); ty=$(( -760 + $2 )); tz=$(( 75 + $3 ))
      P="$P echo TRY dir $dx,$dy,$dz tgt ${tgt// /,} press $press;"
      for s in 12 11 10 9 8 7 6 5 4 3 2 1 0 -1 -2 -3; do
        st=$(( s < 0 ? 0 : s )); k=$(( 12 - s ))
        [ $k -eq $press ] && P="$P $GRAB;vr_mock_button $HAND grip 1;"
        P="$P vr_mock_hand_to $HAND $(( tx + dx*st )) $(( ty + dy*st )) $(( tz + dz*st ));wait2;"
      done
      P="$P wait4;$UNGRAB;vr_mock_button $HAND grip 0;vr_mock_hand_to $HAND -575 -760 55;wait12;"
    done
  done
  P="$P toggleconsole;quit"
  bash $KIT/run.sh $AGENT -Script "$P" -Filter "TRY|gripped on its wall" -Timeout 400 2>&1 | grep -v "^$\|^exit" >> $LOG
done
python - "$LOG" <<'PY'
import sys, collections
tries = []
for l in open(sys.argv[1]).read().splitlines():
    if l.startswith('TRY'):
        tries.append([l.split()[1:], False, ''])
    elif tries and 'gripped on its wall' in l:
        tries[-1][1] = True; tries[-1][2] = l.split('(')[-1].rstrip(')')
by = lambda i: collections.OrderedDict((k, [0, 0]) for k in dict.fromkeys(t[0][i] for t in tries))
press, place = by(5), by(3)
for t in tries:
    for d, k in ((press, t[0][5]), (place, t[0][3])):
        d[k][0] += t[1]; d[k][1] += 1
print(f"taken {sum(t[1] for t in tries)} of {len(tries)}")
print("by press step: " + ", ".join(f"{k}: {a}/{b}" for k, (a, b) in press.items()))
print("by place:      " + ", ".join(f"{k}: {a}/{b}" for k, (a, b) in place.items()))
for t in tries:
    if not t[1]:
        print("missed: " + " ".join(t[0]))
PY
