# usage (ROUND21.md, "Climbing: mantling onto sloping tops"): slopes_test.sh "<cvars>"
# the mantle play: python Misc/quakevr/climb/climb_plays.py mantle (hy 1.49: the hands on a 48-high lip with the default body)
P=${P:-C:/OHWorkspace/qvr-agents/movetweaks/scratch/climb/mantle_1.49.txt}
S="vr_climb 1;map vrslopes;wait30;god;$1;vr_climb_debug 1"
i=0
for name in level up10 up20 up30 up45 down20 across15 up20slab; do
  x=$((i*160+48)); i=$((i+1))
  S="$S;echo == $name;setpos $x -18 24 0 90 0;noclip;wait40;vr_mock_play $P;wait300;vr_mock_play"
done
bash C:/OHWorkspace/qvr-kit/run.sh movetweaks -Script "$S;toggleconsole;quit" -Timeout 300 -Filter "ENGINE|rror|^==|mantle|no room|holds at" 2>&1 | grep -v grunt | cut -c1-150
