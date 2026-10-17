K=C:/OHWorkspace/qvr-kit
MPRE="map e1m1;wait60;developer 1;vr_weapon_grip_mode 1;impulse 9;wait2;impulse 156;wait3;vr_test_weaponinst 7;impulse 120;wait3;give n 100;vr_mock_hand off -0.15 1.25 -0.40 50 0 0;vr_mock_hand main 0.25 1.1 -0.3 0 0 0;wait10"
BTN="vr_mock_hand_to main wbutton front 6;wait10;vr_mock_hand_to main wbutton front 6;wait10;vr_mock_hand_to main wbutton front 1;wait10;vr_mock_hand_to main wbutton front 1;wait15"
F="ammo switch|^weapon button 0"
echo "== A empty hand, default"
bash $K/run.sh tghost -Script "$MPRE;$BTN;toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "ammo switch|button 0" | head -5
echo "== B main hand holds a gun"
bash $K/run.sh tghost -Script "$MPRE;impulse 155;wait10;$BTN;toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "ammo switch|button 0" | head -5
echo "== C same, option off"
bash $K/run.sh tghost -Script "$MPRE;vr_ammo_switch_empty_hand 0;impulse 155;wait10;$BTN;toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "ammo switch|button 0" | head -5
echo "== D main hand holds a prop (gib)"
bash $K/run.sh tghost -Script "$MPRE;vr_test_held_hand 1;vr_test_held_pick 10;give r 10;impulse 252;wait5;$BTN;toggleconsole;quit" -Filter "$F" 2>&1 | grep -E "ammo switch|button 0" | head -5
