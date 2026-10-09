#!/bin/bash
# xr_runtime_test.sh <agent> -- headless tests of vr_xr_runtime's choice (Auto) and its fallback, without the real
# runtimes: fake installed runtimes, active runtime and processes (vr_xr_test_*), the fake runtime DLL
# (Misc/quakevr/fakexr, built first) for the loader's reload. Prints PASS/FAIL per check (docs/vr-port/TESTING.md,
# "OpenXR runtime choice").
NAME="${1:?usage: xr_runtime_test.sh <agent>}"
KIT=C:/OHWorkspace/qvr-kit
TREE="C:/OHWorkspace/qvr-agents/$NAME"
OUT="$TREE/scratch/xrtest"
mkdir -p "$OUT"
bash "$TREE/Misc/quakevr/fakexr/build.sh" "$TREE" | tail -1
F="$TREE/scratch/fakexr"
VD="$F/vd/virtualdesktop-openxr.json"; ST="$F/steam/steamxr_win64.json"; ME="$F/meta/oculus_openxr_64.json"
OT="$F/other/other_openxr.json"; NOPE="$F/nope/virtualdesktop-openxr.json"
ALL="$VD,$ST,$ME,$OT"
# Fake StreamerSettings.json files (Virtual Desktop's runtime setting; the real one's layout, other keys trimmed).
VDJ="$OUT/vdsettings"; mkdir -p "$VDJ"
printf '{\n  "SelectedTab": 2,\n  "OpenXRRuntime": 1,\n  "MonitorCount": 1\n}\n' > "$VDJ/steam.json"
printf '{\r\n  "OpenXRRuntime" : "VDXR",\r\n  "MonitorCount": 1\r\n}\r\n' > "$VDJ/named.json"
printf '{\n  "SelectedTab": 2\n}\n' > "$VDJ/nokey.json"
# Never the real runtimes' implicit layers in these runs.
export DISABLE_XR_APILAYER_VIRTUALDESKTOP_OCULUS_COMPATIBILITY=1
unset XR_RUNTIME_JSON
fails=0
check() { # check <log> <description> <regex>...: each regex in order, on lines after the previous match
    local log="$1" what="$2"; shift 2
    local from=1 ok=1
    for re in "$@"; do
        local n
        n=$(tail -n +$from "$log" | grep -n -E -m1 -- "$re" | cut -d: -f1)
        if [ -z "$n" ]; then ok=0; echo "FAIL: $what: no '$re'"; break; fi
        from=$((from + n))
    done
    [ $ok -eq 1 ] && echo "PASS: $what" || fails=$((fails + 1))
}
explain() { # explain <tag> <processes> <active> [runtimes]: one vr_xr_runtime_explain in Auto
    echo "echo CASE $1;vr_xr_test_processes \"$2\";vr_xr_test_active \"$3\";vr_xr_test_runtimes \"${4-$ALL}\";vr_xr_runtime_explain"
}

# 1. Auto's choice, case by case (no runtime loaded).
S="vr_xr_test 1;vr_xr_runtime 4"
S="$S;$(explain none "" "$ST")"
S="$S;$(explain vd VirtualDesktop.Streamer.exe "$ST")"
S="$S;$(explain steam vrserver.exe "$VD")"
S="$S;$(explain steammon vrmonitor.exe "$VD")"
S="$S;$(explain meta OVRServer_x64.exe "$ST")"
S="$S;$(explain vdsteam "VirtualDesktop.Streamer.exe,vrserver.exe" "$ST")"
S="$S;$(explain tieactivemeta "vrserver.exe,OVRServer_x64.exe" "$ME")"
S="$S;$(explain tieactivesteam "vrserver.exe,OVRServer_x64.exe" "$ST")"
S="$S;$(explain missing VirtualDesktop.Streamer.exe "$ST" "$NOPE,$ST,$ME")"
S="$S;$(explain empty "" "" "")"
S="$S;vr_xr_test_fail \"virtualdesktop,steamxr\";$(explain simfail "VirtualDesktop.Streamer.exe,vrserver.exe" "$ST");vr_xr_test_fail \"\""
S="$S;vr_xr_runtime_fallback 0;$(explain nofallback "VirtualDesktop.Streamer.exe" "$ST");vr_xr_runtime_fallback 1"
S="$S;$(explain idlesteam VirtualDesktop.Streamer.exe "$VD");vr_xr_runtime_fallback 2;$(explain idlesteam2 VirtualDesktop.Streamer.exe "$VD");vr_xr_runtime_fallback 1"
# Virtual Desktop's own runtime setting (vr_xr_test_vd_runtime: VD's enum, or a StreamerSettings.json to read).
S="$S;vr_xr_test_vd_runtime 1;$(explain vdsteam1 VirtualDesktop.Streamer.exe "$VD")"
S="$S;$(explain vdsteam1run "VirtualDesktop.Streamer.exe,vrserver.exe" "$VD")"
S="$S;$(explain vdsteam1nostreamer "" "$ME")"
S="$S;$(explain vdsteam1missing VirtualDesktop.Streamer.exe "$VD" "$VD,$ME")"
S="$S;vr_xr_test_vd_runtime 2;$(explain vdvdxr2 "VirtualDesktop.Streamer.exe,vrserver.exe" "$ST")"
S="$S;vr_xr_test_vd_runtime 0;$(explain vdauto0 VirtualDesktop.Streamer.exe "$ST")"
S="$S;vr_xr_test_vd_runtime 7;$(explain vdodd7 VirtualDesktop.Streamer.exe "$ST")"
S="$S;vr_xr_test_vd_runtime \"$VDJ/steam.json\";$(explain vdfilesteam VirtualDesktop.Streamer.exe "$VD")"
S="$S;vr_xr_test_vd_runtime \"$VDJ/named.json\";$(explain vdfilenamed "VirtualDesktop.Streamer.exe,vrserver.exe" "$ST")"
S="$S;vr_xr_test_vd_runtime \"$VDJ/nokey.json\";$(explain vdfilenokey VirtualDesktop.Streamer.exe "$ST")"
S="$S;vr_xr_test_vd_runtime \"$VDJ/nope.json\";$(explain vdfilenone VirtualDesktop.Streamer.exe "$ST")"
S="$S;vr_xr_test_vd_runtime \"\";$(explain vdtestenv VirtualDesktop.Streamer.exe "$ST")"
# The manual choices: that runtime only.
S="$S;vr_xr_runtime 0;$(explain manual0 VirtualDesktop.Streamer.exe "$ME")"
S="$S;vr_xr_runtime 1;$(explain manual1 vrserver.exe "$ME")"
S="$S;vr_xr_runtime 2;$(explain manual2 VirtualDesktop.Streamer.exe "$ME")"
S="$S;vr_xr_runtime 3;vr_xr_runtime_json \"$OT\";$(explain manual3 "" "$ME");vr_xr_runtime_json \"\""
S="$S;vr_xr_runtime 1;$(explain manual1missing "" "$ME" "$ST,$ME")"
# The migration: a config of version 107 at the old default (0) takes Auto; one with VDXR (1) keeps it.
S="$S;vr_cfg_version 107;vr_xr_runtime 0;vr_migrate_config;echo MIGRATED0;vr_xr_runtime"
S="$S;vr_cfg_version 107;vr_xr_runtime 1;vr_migrate_config;echo MIGRATED1;vr_xr_runtime"
S="$S;vr_xr_runtime 4;wait5;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "CASE|choice:|^  [0-9]\.|outcome:|fallback:|NOT FOUND|left out|MIGRATED|\"vr_xr_runtime\" is|runtime setting|OpenXRRuntime|vr_xr_test_vd_runtime|not read" > "$OUT/explain.log"
L="$OUT/explain.log"
check $L "nothing running: the active runtime first" "CASE none" "choice: Auto: SteamVR - nothing running: the system's active" "1\. SteamVR" "2\. Virtual Desktop" "3\. Meta" "4\. other_openxr.json" "outcome: 1\. SteamVR"
check $L "Streamer running: VDXR" "CASE vd" "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running" "2\. SteamVR \(the system's active" "3\. Meta" "4\. other"
check $L "vrserver running: SteamVR" "CASE steam" "choice: Auto: SteamVR - running" "2\. Virtual Desktop \(VDXR\) \(the system's active"
check $L "vrmonitor running: SteamVR" "CASE steammon" "choice: Auto: SteamVR - running"
check $L "OVRServer running: Meta" "CASE meta" "choice: Auto: Meta \(Oculus\) - OVRServer running" "2\. SteamVR"
check $L "VD and SteamVR running: VDXR, then SteamVR" "CASE vdsteam" "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running" "2\. SteamVR \(running\)"
check $L "SteamVR and Meta running, Meta active: Meta first" "CASE tieactivemeta" "choice: Auto: Meta" "2\. SteamVR \(running\)"
check $L "SteamVR and Meta running, SteamVR active: SteamVR first" "CASE tieactivesteam" "choice: Auto: SteamVR" "2\. Meta \(Oculus\) \(OVRServer running\)"
check $L "a missing manifest is left out" "CASE missing" "NOT FOUND" "choice: Auto: SteamVR" "outcome"
check $L "nothing installed" "CASE empty" "choice: Auto: none - no runtime installed" "outcome: flat"
check $L "simulated failures: on to Meta" "CASE simfail" "1\. Virtual Desktop.*fails: simulated" "2\. SteamVR.*fails: simulated" "3\. Meta" "outcome: 3\. Meta"
check $L "fallback off" "CASE nofallback" "fallback: off" "outcome: 1\."
check $L "an idle SteamVR, not active, left out" "CASE idlesteam" "SteamVR, not running: left out" "1. Virtual Desktop" "2. Meta" "3. other_openxr" "outcome" "CASE idlesteam2"
if sed -n '/CASE idlesteam $/,/CASE idlesteam2/p' $L | grep -q "4\. "; then echo "FAIL: idle SteamVR tried"; fails=$((fails + 1)); else echo "PASS: idle SteamVR not in the order"; fi
check $L "an idle SteamVR tried last with fallback 2" "CASE idlesteam2" "1. Virtual Desktop" "2. Meta" "3. other_openxr" "4. SteamVR \(installed, not running\)"
check $L "VD set to SteamVR: SteamVR first, VDXR next" "CASE vdsteam1 $" "runtime setting: SteamVR: SteamVR first" "vr_xr_test_vd_runtime 1"     "choice: Auto: SteamVR - Virtual Desktop set to SteamVR$" "1\. SteamVR \(Virtual Desktop set to SteamVR\)"     "2\. Virtual Desktop \(VDXR\) \(Streamer running, set to SteamVR\)" "3\. Meta" "outcome: 1\. SteamVR"
check $L "VD set to SteamVR, SteamVR running" "CASE vdsteam1run" "choice: Auto: SteamVR - Virtual Desktop set to SteamVR, running" "2\. Virtual Desktop"
check $L "VD set to SteamVR, no Streamer: ignored" "CASE vdsteam1nostreamer" "runtime setting: SteamVR \(ignored: the Streamer isn't running\)"     "choice: Auto: Meta \(Oculus\) - nothing running"
check $L "VD set to SteamVR, SteamVR not installed: VDXR" "CASE vdsteam1missing"     "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running, set to SteamVR \(not installed\)" "2\. Meta" "outcome: 1\. Virtual"
check $L "VD set to VDXR: VDXR, SteamVR running next" "CASE vdvdxr2" "runtime setting: VDXR: VDXR first"     "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running, set to VDXR" "2\. SteamVR \(running\)"
check $L "VD Automatic: VDXR" "CASE vdauto0" "runtime setting: Automatic: VDXR first" "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running, set to Automatic"
check $L "VD's unknown value: as without it" "CASE vdodd7" "runtime setting: unknown: Auto as without it" "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running$"
check $L "StreamerSettings.json: OpenXRRuntime 1" "CASE vdfilesteam" "runtime setting: SteamVR: SteamVR first" "OpenXRRuntime 1 in .*steam\.json"     "choice: Auto: SteamVR - Virtual Desktop set to SteamVR$"
check $L "StreamerSettings.json: a name (CRLF)" "CASE vdfilenamed" "runtime setting: VDXR: VDXR first" "OpenXRRuntime \"VDXR\" in"     "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running, set to VDXR"
check $L "StreamerSettings.json without the key" "CASE vdfilenokey" "runtime setting: unknown" "nokey\.json: no OpenXRRuntime in it"     "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running$"
check $L "StreamerSettings.json missing" "CASE vdfilenone" "runtime setting: unknown" "nope\.json: not found" "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running$"
check $L "the test environment reads no StreamerSettings.json" "CASE vdtestenv" "runtime setting: unknown" "not read \(test environment"
check $L "manual 0: the system's active only" "CASE manual0" "choice: System default: Meta \(Oculus\) - the system's active runtime" "1\. Meta" "outcome: 1\. Meta"
check $L "manual 1: VDXR whatever runs" "CASE manual1" "choice: Virtual Desktop \(VDXR\): Virtual Desktop \(VDXR\) - chosen in the menu" "outcome: 1\. Virtual"
check $L "manual 2: SteamVR" "CASE manual2" "choice: SteamVR: SteamVR - chosen in the menu"
check $L "manual 3: the manifest" "CASE manual3" "choice: Manifest: other_openxr.json - chosen in the menu"
check $L "manual 1, VDXR not installed: the system's" "CASE manual1missing" "choice: Virtual Desktop \(VDXR\): Meta \(Oculus\) - Virtual Desktop \(VDXR\) not found"
check $L "migration: 0 -> Auto" "MIGRATED0" "\"vr_xr_runtime\" is \"4\""
check $L "migration: 1 kept" "MIGRATED1" "\"vr_xr_runtime\" is \"1\""

# 2. The fallback for real, through the loader: the fake runtimes, VD's and Meta's without a headset (xrGetSystem fails),
# SteamVR's xrCreateInstance failing, the other one simulated. Each loaded and unloaded in turn in one process.
LOG="$OUT/fakexr.log"; : > "$LOG"
export FAKEXR_LOG="$(cygpath -w "$LOG")" FAKEXR_FAIL_INSTANCE=fakexr_steam FAKEXR_D3D11=fakexr_vd
XRLOG="$TREE/quakevr/qvr_openxr.txt"
S="vr_xr_test 1;vr_xr_runtime 4;vr_xr_test_runtimes \"$ALL\";vr_xr_test_active \"$ST\";vr_xr_test_processes \"VirtualDesktop.Streamer.exe,vrserver.exe\";vr_xr_test_fail other;vr_backend openxr;wait20;echo MENULINE;vr_xr_runtime_explain;wait5;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "OpenXR|VR:|last start|MENULINE|graphics DLLs" > "$OUT/fallback.log"
L="$OUT/fallback.log"
check $L "fallback order through the loader" "OpenXR runtime choice: Auto: Virtual Desktop \(VDXR\) - Streamer running" \
    "trying Virtual Desktop" "OpenXR runtime: FakeXR fakexr_vd" "xrGetSystem" "Virtual Desktop \(VDXR\) failed to start" \
    "trying SteamVR" "xrCreateInstance failed" "SteamVR failed to start" \
    "trying Meta" "OpenXR runtime: FakeXR fakexr_meta" "xrGetSystem" "Meta \(Oculus\) failed to start" \
    "trying other_openxr.json" "other_openxr.json failed \(simulated" "no runtime started \(4 tried\)" "failed to start openxr backend" \
    "last start: Auto: none started \(flat\)"
check "$LOG" "the loader loads and unloads each runtime in turn" "fakexr_vd loaded" "fakexr_vd xrCreateInstance" \
    "fakexr_vd xrGetSystem" "fakexr_vd xrDestroyInstance" "fakexr_vd unloaded" "fakexr_steam loaded" "fakexr_steam xrCreateInstance failed" \
    "fakexr_steam unloaded" "fakexr_meta loaded" "fakexr_meta xrDestroyInstance" "fakexr_meta unloaded"
unset FAKEXR_FAIL_INSTANCE FAKEXR_D3D11
check "$LOG" "d3d11.dll kept loaded after VDXR's unload (the NVIDIA crash)" "fakexr_vd d3d11.dll loaded"     "fakexr_vd xrDestroyInstance" "fakexr_vd d3d11.dll freed: still loaded" "fakexr_vd unloaded"
check "$OUT/fallback.log" "the game says it keeps d3d11.dll" "keeping d3d11.dll loaded for good" "MENULINE" "graphics DLLs: d3d11\.dll kept"
check "$XRLOG" "qvr_openxr.txt: the start's log" "=== OpenXR start" "command line: .*ironwail" "XR_RUNTIME_JSON when the game started: not set" \
    "choice: Auto: Virtual Desktop" "trying Virtual Desktop" "the loader loaded FakeXR fakexr_vd" \
    "Virtual Desktop \(VDXR\)'s library is the one loaded: .*fakexr_vd\.dll" "WARNING: OpenXR: xrGetSystem.*failed" "keeping d3d11.dll" \
    "stopping FakeXR fakexr_vd" "trying SteamVR" "WARNING: OpenXR: xrCreateInstance" "trying Meta" "WARNING: OpenXR: no runtime started"
# The same without keeping them (vr_xr_keep_graphics_dlls 0, the old way): d3d11.dll gone once the runtimes unloaded.
export FAKEXR_D3D11=fakexr_vd
S="vr_xr_test 1;vr_xr_runtime 4;vr_xr_keep_graphics_dlls 0;vr_xr_test_runtimes \"$VD\";vr_xr_test_active \"$VD\";vr_xr_test_processes VirtualDesktop.Streamer.exe;vr_backend openxr;wait20;echo MENULINE;vr_xr_runtime_explain;wait5;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "OpenXR|MENULINE|graphics DLLs" > "$OUT/nokeep.log"
unset FAKEXR_D3D11
check "$OUT/nokeep.log" "without vr_xr_keep_graphics_dlls, d3d11.dll unloads with the runtime" "trying Virtual Desktop" "MENULINE" "graphics DLLs: d3d11\.dll not loaded"
if grep -q "keeping d3d11" "$OUT/nokeep.log"; then echo "FAIL: kept with vr_xr_keep_graphics_dlls 0"; fails=$((fails + 1)); else echo "PASS: nothing kept with vr_xr_keep_graphics_dlls 0"; fi

# 2b. Virtual Desktop set to SteamVR, through the loader: SteamVR's fake first (no headset), then VDXR's.
: > "$LOG"
S="vr_xr_test 1;vr_xr_runtime 4;vr_xr_test_runtimes \"$ALL\";vr_xr_test_active \"$VD\";vr_xr_test_processes VirtualDesktop.Streamer.exe;vr_xr_test_vd_runtime 1;vr_xr_test_fail other;vr_xr_steamvr_wait 1;vr_backend openxr;wait20;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "OpenXR|VR:" > "$OUT/vdsteam.log"
check "$OUT/vdsteam.log" "VD set to SteamVR through the loader" "OpenXR runtime choice: Auto: SteamVR - Virtual Desktop set to SteamVR"     "trying SteamVR \(Virtual Desktop set to SteamVR\)" "OpenXR runtime: FakeXR fakexr_steam" "SteamVR failed to start"     "trying Virtual Desktop \(VDXR\) \(Streamer running, set to SteamVR\)" "OpenXR runtime: FakeXR fakexr_vd"
check "$LOG" "SteamVR's runtime loaded before VDXR's" "fakexr_steam loaded" "fakexr_steam unloaded" "fakexr_vd loaded"
check "$OUT/vdsteam.log" "SteamVR asked again for the headset (vr_xr_steamvr_wait 1)" "SteamVR has no headset yet: asking again for up to 1 s"     "SteamVR still has no headset after 1\.[0-9] s \([4-6] tries\)" "SteamVR failed to start"
n=$(grep -c "fakexr_steam xrGetSystem" "$LOG"); if [ "$n" -ge 4 ]; then echo "PASS: SteamVR's xrGetSystem asked $n times"; else echo "FAIL: SteamVR's xrGetSystem asked $n times"; fails=$((fails + 1)); fi
n=$(grep -c "fakexr_vd xrGetSystem" "$LOG"); if [ "$n" -eq 1 ]; then echo "PASS: VDXR's asked once (no wait)"; else echo "FAIL: VDXR's xrGetSystem asked $n times"; fails=$((fails + 1)); fi

# 2c. vr_xr_runtime on the command line (the author's debugger arguments had +vr_xr_runtime 1): said so; a manifest
# with a relative library_path (the real ones' way) resolved.
mkdir -p "$F/rel"
cat > "$F/rel/other_openxr.json" <<'JSON'
{
  "file_format_version": "1.0.0",
  "runtime": {
    "library_path": "./../fakexr_other.dll",
    "name": "FakeXR rel"
  }
}
JSON
S="vr_xr_test 1;vr_xr_test_runtimes \"$ALL\";vr_xr_test_active \"$ST\";vr_xr_test_processes VirtualDesktop.Streamer.exe;echo CMDLINE1;vr_xr_runtime_explain;vr_xr_runtime 4;echo CMDLINE4;vr_xr_runtime_explain"
S="$S;vr_xr_runtime 3;vr_xr_runtime_json \"$F/rel/other_openxr.json\";vr_backend openxr;wait20;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -ExtraArgs "+vr_xr_runtime 1" -Filter "CMDLINE|choice:|command line|OpenXR|^  [0-9]\." > "$OUT/cmdline.log"
L="$OUT/cmdline.log"
check $L "+vr_xr_runtime 1 on the command line: said so" "CMDLINE1" "set on the command line \(\+vr_xr_runtime 1\): set again at every start"     "choice: Virtual Desktop \(VDXR\): Virtual Desktop \(VDXR\) - chosen on the command line$"     "CMDLINE4" "the command line's \+vr_xr_runtime 1 is set again at the next start" "choice: Auto: Virtual Desktop \(VDXR\) - Streamer running$"
check $L "a relative library_path resolved" "trying other_openxr.json" "the loader loaded FakeXR fakexr_other"     "other_openxr.json's library is the one loaded: .*scratch.fakexr.fakexr_other\.dll"

# 3. An XR_RUNTIME_JSON the game was started with wins (and is left as it is).
export XR_RUNTIME_JSON="$(cygpath -w "$ME")"
S="vr_xr_test 1;vr_xr_runtime 4;vr_xr_test_runtimes \"$ALL\";vr_xr_test_processes VirtualDesktop.Streamer.exe;vr_xr_runtime_explain;vr_backend openxr;wait20;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "OpenXR|choice:|^  [0-9]\.|outcome:" > "$OUT/outside.log"
unset XR_RUNTIME_JSON
L="$OUT/outside.log"
check $L "outside XR_RUNTIME_JSON wins" "choice: Meta \(Oculus\) - XR_RUNTIME_JSON set outside the game" "1\. Meta \(Oculus\) \(XR_RUNTIME_JSON set outside" \
    "is set outside the game" "OpenXR runtime: FakeXR fakexr_meta"
echo "xr_runtime_test: $fails failed"
