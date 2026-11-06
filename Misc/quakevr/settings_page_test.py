#!/usr/bin/env python3
"""Setup's settings page, end to end (docs/vr-port/INSTALLER.md, "Settings page").

    python Misc/quakevr/settings_page_test.py [agent-name]

1. The staleness check (Misc/installer/settings_page.py): one headless game dump of the page's settings plus two made
   to fail (a cvar that doesn't exist, one that isn't saved); the real list passes against the committed manifest; the
   bad list fails on both; a manifest with a changed label fails as stale.
2. The page's logic without its window (qvr-setup settings, the installer's CLI: built if missing) on a scratch
   install: a first install writes only the changed settings to installer_settings.cfg; the game reads it (values
   applied, the presets applied, "installer settings applied"); a change made in the game afterwards is kept after a
   second run of the page (which writes only what it changed, from the player's saved values), and the file is not run
   again once applied.
The game's folder gets installer_settings.cfg and two configs for the runs; they are moved to scratch afterwards and the
agent's ironwail.cfg is put back. Last line: "settings_page_test: N passed, M failed".
"""

import json
import os
import re
import shutil
import subprocess
import sys
import time

import qvrtest  # noqa: E402  (the tests' library: Misc/quakevr/qvrtest.py)

AGENT = sys.argv[1] if len(sys.argv) > 1 else "instset"
TREE = qvrtest.tree(AGENT)
SCRATCH = os.path.join(TREE, "scratch", "settings_page_%d" % int(time.time()))
TOOL = os.path.join(TREE, "Misc", "installer", "settings_page.py")
LIST = os.path.join(TREE, "Misc", "installer", "settings_page.txt")
MANIFEST = os.path.join(TREE, "quakevr", "settings_page.json")
CLI_DIR = os.path.join(TREE, "Installer", "src", "QuakeVR.Installer.Cli")
CLI = os.path.join(CLI_DIR, "bin", "Debug", "net9.0-windows", "qvr-setup.exe")
GAME = os.path.join(qvrtest.game_dir(AGENT), "quakevr")
LOG = os.path.join(qvrtest.game_dir(AGENT), "qconsole.log")

passed = failed = 0


def check(what, ok, detail=""):
    global passed, failed
    if ok:
        passed += 1
        print("ok: %s" % what)
    else:
        failed += 1
        print("FAIL: %s %s" % (what, detail))


def tool(*args):
    r = subprocess.run([sys.executable, TOOL] + list(args), capture_output=True, text=True)
    return r.returncode, r.stdout


def cli(*args):
    r = subprocess.run([CLI, "settings"] + list(args), capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def game(script, done):
    qvrtest.run_sh(AGENT, ["-Filter", done, "-QuitOn", done, "-Timeout", "300", "-Script", script], timeout=600)
    with open(LOG, encoding="utf-8", errors="replace") as f:
        return f.read()


def cvar(text, name):
    m = re.findall(r'"%s" is "([^"]*)"' % re.escape(name), text)
    return m[-1] if m else None


def same(a, b):
    try:
        return a is not None and float(a) == float(b)
    except ValueError:
        return a == b


def stage(src, name):
    shutil.copyfile(src, os.path.join(GAME, name))


def aside(name):
    p = os.path.join(GAME, name)
    if os.path.exists(p):
        os.replace(p, os.path.join(SCRATCH, "old_" + name))


def main():
    os.makedirs(SCRATCH, exist_ok=True)

    # ---- 1. The staleness check
    bad = os.path.join(SCRATCH, "bad_list.txt")
    with open(LIST, encoding="utf-8") as f:
        text = f.read()
    with open(bad, "w", encoding="utf-8") as f:
        f.write(text + "\n[Broken > On Purpose]\nvr_no_such_setting\nvr_menu_turning\n")
    dump = os.path.join(SCRATCH, "dump.txt")
    code, out = tool("check", AGENT, "--list", bad, "--save-log", dump)
    check("bad list fails", code == 1, out[-300:])
    check("bad list: the missing cvar named", "vr_no_such_setting: no such cvar" in out)
    check("bad list: the unsaved cvar named", "vr_menu_turning: not saved with the config" in out)
    code, out = tool("check", AGENT, "--log", dump)
    check("real list passes against the committed manifest", code == 0 and "0 failed" in out, out[-400:])
    stale = os.path.join(SCRATCH, "stale.json")
    with open(MANIFEST, encoding="utf-8") as f:
        m = json.load(f)
    m["settings"][0]["label"] = "Renamed by the test"
    with open(stale, "w", encoding="utf-8") as f:
        json.dump(m, f)
    code, out = tool("check", AGENT, "--log", dump, "--manifest", stale)
    check("a changed label fails as stale", code == 1 and "stale manifest" in out, out[-300:])

    # ---- 2. The page's logic and the game
    if not os.path.exists(CLI):
        subprocess.run(["dotnet", "build", CLI_DIR], capture_output=True)
    check("qvr-setup built", os.path.exists(CLI))
    install = os.path.join(SCRATCH, "install")
    os.makedirs(os.path.join(install, "quakevr"))
    shutil.copyfile(MANIFEST, os.path.join(install, "quakevr", "settings_page.json"))
    code, out = cli("--target", install, "--set", "vr_snap_turn=45", "--set", "vr_gore=1", "--set", "vr_handedness=2",
                    "--set", "vr_graphics_preset=2", "--set", "vr_comfort_vignette=0")
    check("first install: 4 changed (the vignette left at its default)", code == 0 and "4 changed" in out, out[-500:])
    cfg = os.path.join(install, "quakevr", "installer_settings.cfg")
    with open(cfg, encoding="utf-8") as f:
        first = f.read()
    check("first file: only the changes", all(l in first for l in ('vr_snap_turn "45"', 'vr_gore "1"', 'vr_handedness "2"',
                                                                    'vr_graphics_preset "2"'))
          and "vr_comfort_vignette" not in first and "// stamp " in first, first)

    cfg_path = os.path.join(GAME, "ironwail.cfg")
    with open(cfg_path, "rb") as f:
        saved_cfg = f.read()
    try:
        stage(cfg, "installer_settings.cfg")
        show = "vr_snap_turn; vr_gore; vr_stick_swap; vr_gadget_arm; vr_turn_speed; vr_installer_applied; vr_graphics_preset_status"
        log = game("%s; vr_snap_turn 90; writeconfig sp_cfg1; echo SP done; toggleconsole; quit" % show, "SP done")
        check("game 1: applied once", "installer settings applied" in log)
        check("game 1: vr_snap_turn 45", same(cvar(log, "vr_snap_turn"), "45"), cvar(log, "vr_snap_turn"))
        check("game 1: vr_gore 1", same(cvar(log, "vr_gore"), "1"))
        check("game 1: left-handed (vr_stick_swap, vr_gadget_arm 1)",
              same(cvar(log, "vr_stick_swap"), "1") and same(cvar(log, "vr_gadget_arm"), "1"))
        state = re.findall(r"graphics preset: ([^(\n]*)", log)
        check("game 1: graphics Medium", bool(state) and state[-1].strip().startswith("Medium"), state)

        # The second run of the page, from the config the game saved (vr_snap_turn changed to 90 in the game).
        shutil.copyfile(os.path.join(GAME, "sp_cfg1.cfg"), os.path.join(install, "quakevr", "ironwail.cfg"))
        code, out = cli("--target", install, "--set", "vr_turn_speed=5")
        check("second install: starts from the saved values, nothing pending",
              '(vr_snap_turn, choice): "90"' in out and "0 pending" in out, out[:600])
        with open(cfg, encoding="utf-8") as f:
            second = f.read()
        check("second file: only vr_turn_speed", 'vr_turn_speed "5"' in second and "vr_snap_turn" not in second, second)
        code, out = cli("--target", install)
        check("no change: nothing written", "nothing written" in out)

        stage(cfg, "installer_settings.cfg")
        stage(os.path.join(GAME, "sp_cfg1.cfg"), "ironwail.cfg")
        log = game("%s; writeconfig sp_cfg2; echo SP done; toggleconsole; quit" % show, "SP done")
        check("game 2: applied", "installer settings applied" in log)
        check("game 2: the in-game change kept (vr_snap_turn 90)", same(cvar(log, "vr_snap_turn"), "90"), cvar(log, "vr_snap_turn"))
        check("game 2: vr_turn_speed 5", same(cvar(log, "vr_turn_speed"), "5"))

        stage(os.path.join(GAME, "sp_cfg2.cfg"), "ironwail.cfg")
        log = game("vr_turn_speed 2; vr_turn_speed; echo SP done; toggleconsole; quit", "SP done")
        check("game 3: not run again", "installer settings applied" not in log)
        check("game 3: the game's own change stands", same(cvar(log, "vr_turn_speed"), "2"))
    finally:
        with open(cfg_path, "wb") as f:
            f.write(saved_cfg)
        for name in ("installer_settings.cfg", "sp_cfg1.cfg", "sp_cfg2.cfg"):
            aside(name)
    print("settings_page_test: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
