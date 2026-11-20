# B11b: the single-player scenarios, vr_pmove_own 0 against 1, frame by frame (pmtrace_sp.sh each; usage: bash pmtrace_all.sh [walk|lift|swim|wjump|crawl]).
W=${W:-C:/OHWorkspace/qvr-agents/mp}
S=$W/Misc/quakevr/mp/pmtrace_sp.sh
P="sv_random_seed 1;vr_pmove_own V;vr_pmove_trace 0"
which=${1:-all}
if [ "$which" = all ] || [ "$which" = walk ]; then
bash $S walk "$P;map e1m1;wait20;god;notarget;vr_pmove_trace 1;vr_mock_stick off 0 1;wait60;+jump;wait2;-jump;wait60;+jump;wait2;-jump;wait40;vr_mock_stick off 1 0;wait60;vr_mock_stick off 0 -1;wait60;vr_mock_stick off 0 0;vr_mock_hand head 0 1.0 0;wait20;vr_mock_stick off 0 1;wait80;vr_mock_stick off -1 0;wait40;vr_mock_stick off 0 0;vr_mock_hand head;wait40;vr_pmove_trace 0;toggleconsole;quit" | tail -3
fi
if [ "$which" = all ] || [ "$which" = lift ]; then
bash $S lift "$P;map e1m1;wait20;god;notarget;setpos -584 2656 -94 0 90 0;noclip;wait5;vr_pmove_trace 1;wait200;vr_mock_stick off 0.3 0;wait40;vr_mock_stick off 0 1;wait60;vr_mock_stick off 0 0;wait30;vr_pmove_trace 0;toggleconsole;quit" | tail -3
fi
if [ "$which" = all ] || [ "$which" = swim ]; then
bash $S swim "$P;map e1m4;wait20;god;notarget;vr_pmove_trace 1;setpos 336 1300 388 0 90 0;noclip;wait5;wait40;vr_mock_stick off 0 0.6;wait60;vr_mock_stick off 0.6 0;wait60;+jump;wait60;-jump;vr_mock_stick off 0 -0.6;wait60;vr_mock_stick off 0 0;wait60;vr_pmove_trace 0;toggleconsole;quit" | tail -3
fi
if [ "$which" = all ] || [ "$which" = wjump ]; then
bash $S wjump "$P;vr_water_jump 1;map start;wait20;god;notarget;vr_pmove_trace 1;setpos 1313 1681 86 0 180 0;noclip;wait1;+jump;vr_mock_stick off 0 1;wait120;-jump;vr_mock_stick off 0 0;wait60;vr_pmove_trace 0;toggleconsole;quit" | tail -3
fi
if [ "$which" = all ] || [ "$which" = crawl ]; then
bash $S crawl "$P;map e1m1;wait20;god;notarget;setpos 480 -296 72 0 90 0;noclip;wait5;vr_pmove_trace 1;vr_mock_hand head 0 0.4 0;wait60;vr_mock_stick off 0 1;wait80;vr_mock_stick off -1 0;wait80;vr_mock_stick off 0 -1;wait60;vr_mock_stick off 0 0;wait40;vr_mock_hand head;wait60;vr_pmove_trace 0;toggleconsole;quit" | tail -3
fi
