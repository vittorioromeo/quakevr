# closein_test.sh <agent> [approach|wall|stepin]... -- the player's body against monsters' (ROUND22.md, "Closer to
# enemies": vr_body_monster_inset, vr_foegrab_step_in). Prints vr_body_contact_status lines ("dist": origins, level;
# "depth" > 0 overlapping) for the author's eye; see TESTING.md for the expected numbers.
#   approach  a grunt, an ogre, an enforcer (vr_test_spawn 0 1 8) walked into at inset 4 (A4), set to 0 touching (T0 at
#             once, T0b pushed apart), walked into at 0 (A0), backed off (B0), at 4 again (A4b, B4)
#   wall      backed into the firing range's wall at inset 0 and 12: the same origin
#   stepin    a held grunt walked into, step in on and off (G held, H walked in, R let go, R2 apart, B backed off)
K=C:/OHWorkspace/qvr-kit
A=${1:-closein}
shift
CASES=${*:-approach wall stepin}
PRE="sv_random_seed 7;map vrfiringrange;wait30;god;notarget"
END="toggleconsole;quit"
F="^(A4|A4b|T0|T0b|A0|B0|B4|W0|W12|G|H|R|R2|B|body contact|crouch:|player is stuck|Unstuck)"
for c in $CASES; do
  case $c in
  approach)
    for n in 0 1 8; do
      S="$PRE;vr_test_spawn $n;vr_test_spawn_dist 120;vr_body_monster_inset 4;impulse 241;wait40"
      S="$S;+forward;wait250;-forward;wait5;echo A4;vr_body_contact_status"
      S="$S;vr_body_monster_inset 0;wait2;echo T0;vr_body_contact_status;wait60;echo T0b;vr_body_contact_status"
      S="$S;+forward;wait150;-forward;wait5;echo A0;vr_body_contact_status;+back;wait20;-back;wait5;echo B0;vr_body_contact_status"
      S="$S;vr_body_monster_inset 4;+forward;wait150;-forward;wait5;echo A4b;vr_body_contact_status"
      S="$S;+back;wait20;-back;wait5;echo B4;vr_body_contact_status;$END"
      bash $K/run.sh $A -Script "$S" -Filter "$F"
    done ;;
  wall)
    S="$PRE;vr_body_monster_inset 0;+back;wait400;-back;wait5;echo W0;vr_crouch_status"
    S="$S;+forward;wait30;-forward;vr_body_monster_inset 12;+back;wait400;-back;wait5;echo W12;vr_crouch_status;$END"
    bash $K/run.sh $A -Script "$S" -Filter "$F" ;;
  stepin)
    for on in 1 0; do
      S="$PRE;developer 1;vr_foegrab_leniency 6;vr_foegrab_step_in $on;vr_foegrab_follow 0;vr_foegrab_drag 0"
      S="$S;vr_foegrab_break 200;vr_foegrab_slow 1;vr_test_spawn 0;vr_test_spawn_dist 40;impulse 241;wait40"
      S="$S;vr_mock_hand_to main nearest monster_army 6;wait2;vr_mock_hand_to main nearest monster_army 6;wait1"
      S="$S;+grabmain;vr_mock_button main grip 1;wait5;echo G;vr_body_contact_status;+forward;wait8;-forward;wait5"
      S="$S;echo H;vr_body_contact_status;-grabmain;vr_mock_button main grip 0;wait3;echo R;vr_body_contact_status"
      S="$S;wait60;echo R2;vr_body_contact_status;+back;wait20;-back;wait5;echo B;vr_body_contact_status;$END"
      echo "step in $on"
      bash $K/run.sh $A -Script "$S" -Filter "$F"
    done ;;
  esac
done
