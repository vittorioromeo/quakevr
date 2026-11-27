#!/bin/bash
# flatsink_test.sh <agent> -- flat-screen play: walking with the view pitched (0, 40, 80 degrees down) keeps the player on
# the floor, on two maps (vrtesthall's open floor, e1m1's start). (flatsink, ROUND26: a test walked a pitched view down
# through the floor, z 25 -> -24, because Quake's setpos turns noclip on; each setpos here is followed by `noclip 0`,
# as qvr_checks setpos_walk asks.) Each walk is a counted number of frames forward; the run ends on its own line.
# Prints "flatsink: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch/flatsink
mkdir -p "$S"

# walk <map> <x> <y> <z> <frames>: one game, a walk at each pitch, viewpos before and after each.
walk() {
    local map=$1 x=$2 y=$3 z=$4 frames=$5 script="map $1; god; notarget; wait15"
    for p in 0 40 80; do
        script="$script; setpos $x $y $z $p 90 0; noclip 0; wait10; echo flatsink: from $p; viewpos; +forward; wait$frames; -forward; wait10; echo flatsink: to $p; viewpos"
    done
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 150 \
        -Script "$script; echo flatsink: $map done; toggleconsole; quit" \
        -QuitOn "^flatsink: $map done" -Filter "^(flatsink:|Player pos)" > "$S/$map.log" 2>&1
}

# pos <map> <from|to> <pitch>: "y z" of the viewpos after that marker.
pos() { awk -v m="flatsink: $2 $3" 'index($0, m) == 1 {a = 1; next} a && /^Player pos:/ {gsub(/[()]/, ""); print $4, $5; exit}' "$S/$1.log"; }

# vrtesthall's floor is flat: z stays. e1m1's start walks down a step (y -292, z 88 -> 82, level or pitched): each pitch
# ends where the level walk did.
walk vrtesthall 0 -200 25 36
walk e1m1 480 -352 88 18
for map in vrtesthall e1m1; do
    check $(grep -aq "^flatsink: $map done" "$S/$map.log" && echo 1 || echo 0) "$map: run completed"
    set -- $(pos $map to 0)
    Y0=$1 Z0=$2
    for p in 0 40 80; do
        set -- $(pos $map from $p) $(pos $map to $p)
        check $(awk -v y0="$1" -v z0="$2" -v y1="$3" -v z1="$4" -v m=$map -v ly="$Y0" -v lz="$Z0" 'BEGIN{d = z1 - z0; exit !(y0 != "" && y1 != "" && y1 - y0 > 40 && (m != "vrtesthall" || (d > -1.5 && d < 1.5)) && y1 - ly > -2 && y1 - ly < 2 && z1 - lz > -1.5 && z1 - lz < 1.5)}' && echo 1 || echo 0) \
            "$map pitch $p: walked forward on the floor, as level (y ${1:-?} -> ${3:-?}, z ${2:-?} -> ${4:-?}; level: ${Y0:-?} ${Z0:-?})"
    done
done
verdict
exit $fail
