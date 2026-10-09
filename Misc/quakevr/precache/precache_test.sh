#!/bin/bash
# precache_test.sh <agent> [cases] -- saved games and the model precache list (ROUND21.md, "Saved games' models"):
# vr_model_check after each step (every entity's .modelindex against its .model; the client's list against the
# server's: "0 wrong; client: 0 wrong" is a pass). Each case in the firing range:
#   dummy    a save, the training dummy turned into a vore (its models precached late), another save; both loaded with
#            the vore wanted (the map's own precaches now in another order: the bug's buttons drawn as the vore's head)
#   death    a limb cut off (its model precached late) and the dummy a vore, an autosave; killed, the autosave
#            loaded, as dying does (Host_AutoLoad: its own `load` would run after the rest of a test script)
#   restart  the same late precaches, then `restart` without a save to load, and `map` of the same map
#   legacy   the dummy save with its `// qvr_` lines taken out (a save of an older build), and with its
#            vr_save_packmask/vr_save_campaign keys taken out: both refused ("not loaded"), the game running on
#   build    the save said to be another build's (a warning, the centre print), and of a newer format (refused)
#   hitch    a sword's killing slash at a grunt's limb, twice: no `late precache` of its limb model at the first
#            (vr_limbs_prebuild 1: made as the map loaded; with 0, the line and its time in ms), then with 0
AGENT=${1:?worktree name}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
CASES=${*:-dummy death restart legacy build}
FILTER="vr_model_check:|rror|CRASH|saved by|not loaded|Autoloading|^precachetest"
run() { bash $KIT/run.sh $AGENT -Script "$1" -Filter "${2:-$FILTER}" -Timeout 300 2>&1 | grep -v "^$" | grep -v "^exit=0" | sed -E 's/^.*-{8,}//; s/^-+//'; } # (a centre print's dashes before a line)
CHK="wait30;vr_model_check 1"
PRE="map vrfiringrange;wait30;god;notarget;developer 0;vr_dummy_type 0;wait30"
# (pass: every check after the first says 0 wrong for both)
verdict() { awk '/^vr_model_check:/ { l = $0; if (l !~ /client: -?[0-9]+ wrong/ && (getline x) > 0) l = l x; n++; if (l !~ / 0 wrong; client: 0 wrong/) bad++ } END { printf "precachetest: %d checks, %s\n", n, bad ? bad " FAILED" : "pass" }'; } # (a line the console wrapped: joined)
chk() { local out; out=$(run "$@"); echo "$out"; echo "$out" | verdict; }
for c in $CASES; do
    echo "== $c"
    case $c in
    dummy)
        chk "$PRE;vr_model_check;save pctest_a;wait10;vr_dummy_type 8;wait60;vr_model_check;save pctest_b;wait10;load pctest_a;$CHK;load pctest_b;$CHK;vr_dummy_type 0;toggleconsole;quit" ;;
    death)
        chk "$PRE;vr_test_spawn 0;vr_test_spawn_dist 90;impulse 241;wait30;vr_limb_test 4;wait30;vr_dummy_type 8;wait60;vr_model_check;save autosave/vrfiringrange 0;wait10;kill;wait30;load autosave/vrfiringrange;$CHK;vr_dummy_type 0;toggleconsole;quit" ;;
    restart)
        chk "$PRE;sv_autoload 0;vr_test_spawn 0;vr_test_spawn_dist 90;impulse 241;wait30;vr_limb_test 4;wait30;vr_dummy_type 8;wait60;vr_model_check;restart;$CHK;map vrfiringrange;$CHK;sv_autoload 2;vr_dummy_type 0;toggleconsole;quit" ;;
    legacy)
        run "$PRE;save pctest_a;wait10;vr_dummy_type 8;wait60;save pctest_b;wait10;toggleconsole;quit" "^xx" >/dev/null
        grep -v "^// qvr_" $TREE/quakevr/pctest_a.sav > $TREE/quakevr/pctest_legacy.sav
        grep -v '^"vr_save_\(packmask\|campaign\)"' $TREE/quakevr/pctest_a.sav > $TREE/quakevr/pctest_nokeys.sav
        run "map vrfiringrange;wait10;load pctest_legacy;wait10;load pctest_nokeys;wait10;toggleconsole;quit" "$FILTER|lacks|Saved game" ;;
    build)
        sed -E 's|^// qvr_save ([0-9]+) progs ([0-9a-f]+) build .*|// qvr_save \1 progs \2 build 2000-01-01 00000000|' $TREE/quakevr/pctest_b.sav > $TREE/quakevr/pctest_other.sav
        sed -E 's|^// qvr_save [0-9]+ |// qvr_save 99 |' $TREE/quakevr/pctest_b.sav > $TREE/quakevr/pctest_newer.sav
        run "map vrfiringrange;wait10;vr_dummy_type 8;load pctest_other;$CHK;load pctest_newer;wait30;vr_model_check;vr_dummy_type 0;toggleconsole;quit" "$FILTER|Saved game|another build|newer build" ;;
    hitch)
        # (the map loaded with each setting: the dummy, a grunt, has its limbs made with 1)
        for p in 1 0; do echo "-- vr_limbs_prebuild $p"; run "vr_limbs_prebuild $p;developer 1;map vrfiringrange;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 90;impulse 241;wait30;vr_limb_test 4;wait60;vr_mock_look 0 60;impulse 241;wait30;vr_limb_test 4;wait30;developer 0;vr_limbs_prebuild 1;toggleconsole;quit" "late precache: [^ ]*#limb|^limbtest: 4 at|limb models:|rror"; done ;;
    *) echo "unknown case $c" ;;
    esac
done
