# Mid-game model loads on the firing range (ROUND22.md, "Mid-game model loads"): an ogre and a knight first spawned in
# play (the debug spawner: dispenser kinds), then the toolgun's candle and lantern; prints the models loaded in play,
# the hitch log's frames over half a budget and the map's load time.
# Usage: bash Misc/quakevr/perfload_test.sh <agent> [vr_limbs_prebuild: 2 the default, 1 the old one]
A=${1:?agent}
P=${2:-2}
bash C:/OHWorkspace/qvr-kit/run.sh "$A" -Script "developer 1;vr_limbs_prebuild $P;map vrfiringrange;wait60;god;vr_startup_times;vr_profile_csv 1;vr_profile_hitch 0.5;wait90;echo PL ogre;vr_test_spawn 1;vr_test_spawn_dist 150;impulse 241;wait150;echo PL knight;vr_test_spawn 5;impulse 241;wait150;echo PL toolgun candle;vr_toolgun_spawn_test 50 51;wait60;echo PL end;vr_cache_info;toggleconsole;quit" -Filter "^PL|model loaded in play|vr_profile: hitch|^  total  .*ms$|models loaded in play|limb models: [0-9]" -Timeout 300
