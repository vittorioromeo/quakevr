"""reload_test.py <agent> [modes] -- shoves and lifts after a reload (ROUND22.md, "Shoves and lifts after a reload").

A grunt shoved two-handed by the mock hands (a motion_synth palm_shove_2h take) and one lifted (both hands gripped at its
shoulders, pulled up), each must become a ragdoll (vr_knockdown_test 5: state 1, ragdoll 1; "lifted, held on as a
ragdoll"); then the reload, then the same again. Modes: restart, load (save, load), quickload (a save before phase A:
game time goes back), death (kill, restart: a fresh map), autoload (save first, kill, restart: sv_autoload loads it), map,
crowdload / crowdauto / crowddeath (ten corpses and one knocked down first: the pool full), midliftload / midliftauto
(saved with a grunt held up), midshoveload / midshoveauto (saved as one staggers). Default: all.
Knobs: RR_MAP (vrfiringrange), RR_PRE (before the map, e.g. "exec <cfg>"), RR_START (frames before the cheats: 400 lets
the level-start autosave happen), RR_LONG (frames before phase A, 3000), RR_EXTRA, RR_RUNARGS (e.g. -RealTime).
Logs: scratch/ragreload_<mode>.txt."""
import os, re, subprocess, sys

AGENT = sys.argv[1]
TREE = f"C:/OHWorkspace/qvr-agents/{AGENT}"
MOT = os.path.join(TREE, "quakevr", "motions")  # (git-ignored)
os.makedirs(MOT, exist_ok=True)
os.makedirs(os.path.join(TREE, "scratch"), exist_ok=True)
subprocess.run([sys.executable, os.path.join(TREE, "Misc", "quakevr", "motion_synth.py"), "palm_shove_2h", "--distance",
                "0.95", "--mock", "--out", MOT, "--name", "ragreload_shove"], stdout=subprocess.DEVNULL, check=True)
y, near, gap = 1.02, -1.15, 0.09
# (The hands at rest first, then brought slowly into the grunt: a quick move there is a shove.)
rest = [(0.0, f"main {gap} {y} -0.3"), (0.0, f"off {-gap} {y} -0.3"), (0.5, f"main {gap} {y} -0.3"), (0.5, f"off {-gap} {y} -0.3")]
keys = rest + [(2.0, f"main {gap} {y} {near-0.08}"), (2.0, f"off {-gap} {y} {near}"), (2.3, "cmd +grabmain"), (2.3, "cmd +graboff"),
        (2.8, f"main {gap} {y} {near-0.08}"), (2.8, f"off {-gap} {y} {near}"),
        (3.05, f"main {gap} {y+0.4} {near-0.08}"), (3.05, f"off {-gap} {y+0.4} {near}"),
        (3.6, "cmd -grabmain"), (3.6, "cmd -graboff"), (4.6, f"main {gap} {y} -0.3"), (4.6, f"off {-gap} {y} -0.3")]
for name, ks in (("ragreload_lift", keys), ("ragreload_rest", rest)):
    with open(os.path.join(MOT, name + ".mock"), "w") as f:
        for t, l in ks:
            f.write(f"{t:.3f} {l}\n")

PHASE = ("echo PHASE_{p}_SHOVE;vr_test_spawn 0;vr_weapon_grip_mode 1;impulse 150;wait5;vr_test_spawn_dist 34;impulse 241;wait40;vr_mock_play quakevr/motions/ragreload_shove.mock;wait100;"
         "vr_knockdown_test 5;wait5;vr_knockdown_test 20;wait5;"
         "echo PHASE_{p}_LIFT;vr_mock_play quakevr/motions/ragreload_rest.mock;wait30;vr_test_spawn_dist 36;impulse 241;wait40;vr_mock_play quakevr/motions/ragreload_lift.mock;"
         "wait300;vr_knockdown_test 5;wait80;vr_knockdown_test 20;wait5")
RELOAD = {"restart": "restart;wait60",
          "load": "save ragreload;wait5;load ragreload;wait60",
          "autoload": "kill;wait200;restart;wait90",  # (the save made before phase A: game time goes back)
          "quickload": "load ragreload;wait60",
          "death": "kill;wait200;restart;wait90",
          "map": "map vrfiringrange;wait60"}
# Ten grunts killed (their corpses ragdolls: vr_ragdoll_max's pool full) and one knocked down, before the reload.
CROWD = "".join(f"vr_test_spawn_dist {100 + 20 * i};impulse 241;wait20;vr_test_kill 1;wait40;" for i in range(10))
CROWD += "vr_knockdown_time_min 20;vr_knockdown_time_max 20;vr_test_spawn_dist 80;impulse 241;wait30;vr_knockdown_test 0;wait30;"
RELOAD["crowdload"] = CROWD + "save ragreload;wait5;load ragreload;wait60"
RELOAD["crowdauto"] = CROWD + "save ragreload;wait5;kill;wait200;restart;wait90"
RELOAD["crowddeath"] = CROWD + "kill;wait200;restart;wait90"
# Saved mid-action (an autosave can be: not hurt, not firing, slow): a grunt held up in a lift, one staggering.
MIDLIFT = ("vr_mock_play quakevr/motions/ragreload_rest.mock;wait30;vr_test_spawn_dist 36;impulse 241;wait40;"
           "vr_mock_play quakevr/motions/ragreload_lift.mock;wait" + os.environ.get("RR_MIDWAIT", "300") + ";")
MIDSHOVE = ("vr_mock_play quakevr/motions/ragreload_rest.mock;wait30;vr_test_spawn_dist 34;impulse 241;wait40;"
            "vr_mock_play quakevr/motions/ragreload_shove.mock;wait" + os.environ.get("RR_MIDWAIT", "50") + ";")
for k, v in (("midlift", MIDLIFT), ("midshove", MIDSHOVE)):
    RELOAD[k + "load"] = v + "save ragreload;wait5;load ragreload;wait60;echo AFTERLOAD;vr_knockdown_test 5;wait600;vr_knockdown_test 5;wait5;vr_knockdown_test 20;wait5"
    RELOAD[k + "auto"] = v + "save ragreload;wait5;kill;wait200;restart;wait90;echo AFTERLOAD;vr_knockdown_test 5;wait600;vr_knockdown_test 5;wait5;vr_knockdown_test 20;wait5"
MAP = os.environ.get("RR_MAP", "vrfiringrange")
RELOAD["map"] = f"map {MAP};wait60"
# RR_PRE: commands before the map (exec a config); the cheats after the level-start autosave (t 2.5 s: cheats stop it).
PRE = (os.environ.get("RR_PRE", "") + f";sv_random_seed 7;map {MAP};wait{os.environ.get('RR_START', '30')};god 1;notarget 1;developer 1;"
       "echo ATSTART;vr_test_spawn 0;vr_knockdown_test 5;vr_foegrab_debug 1;vr_foegrab_leniency 6;"
       "vr_knockdown_debug 1;vr_knockdown_chance 100;vr_foegrab_lift_time 2;wait5")
FILT = r"AFTERLOAD|foegrab: [a-z_]* \(|PHASE_|knockdown: |lifted, held|no lift|rror|CRASH|ragdoll"
MODES = sys.argv[2:] or list(RELOAD)
fails = 0
for m in MODES:
    extra = os.environ.get("RR_EXTRA", "")
    early = "save ragreload;wait5;" if m in ("autoload", "quickload") else ""
    script = (f"{PRE};{extra};{early}wait{os.environ.get('RR_LONG', '3000')};{PHASE.format(p='A')};echo RELOAD_{m};{RELOAD[m]};god 1;notarget 1;{PHASE.format(p='B')};"
              "toggleconsole;quit")
    bash = os.environ.get("BASH_EXE", "C:/Program Files/Git/bin/bash.exe")
    out = subprocess.run([bash, "C:/OHWorkspace/qvr-kit/run.sh", AGENT, "-Script", script, "-Filter", FILT + "|RELOAD_",
                          "-Timeout", "300", *os.environ.get("RR_RUNARGS", "").split()], stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
    out = out.decode("utf-8", "replace").replace("\r", "")
    with open(os.path.join(TREE, "scratch", f"ragreload_{m}.txt"), "w") as f:
        f.write(out)
    res = {}
    for p in "AB":
        s = out.split(f"PHASE_{p}_SHOVE")[-1] if f"PHASE_{p}_SHOVE" in out else ""
        shove = s.split(f"PHASE_{p}_LIFT")[0]
        lift = s.split(f"PHASE_{p}_LIFT")[-1].split("RELOAD_")[0] if f"PHASE_{p}_LIFT" in s else ""
        shoved = bool(re.search(r"knockdown: monster_army: state 1, health [\d.]+, ragdoll 1", shove))
        lifted = "lifted, held on as a ragdoll" in lift
        res[p] = (shoved, lifted)
    ok = all(res["A"]) and all(res["B"]) and "CRASH" not in out
    fails += not ok
    print(f"{m}: before shove {res['A'][0]} lift {res['A'][1]}; after shove {res['B'][0]} lift {res['B'][1]}: "
          f"{'PASS' if ok else 'FAIL'}")
print("ragreload:", "PASS" if fails == 0 else f"{fails} FAIL")
