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

# 4. A session on the fake headset (FAKEXR_HEADSET): the log's formats, layers, sizes, the session's states and a timing
# line a second; the focus lost for frames 150-250 (FAKEXR_UNFOCUS, as SteamVR's dashboard does): the last frame shown
# again, no projection layer without a released image (the runtime's XR_ERROR_LAYER_INVALID); vr_xr_eye_scale 0.8.
LOG="$OUT/headset.log"; : > "$LOG"
export FAKEXR_LOG="$(cygpath -w "$LOG")" FAKEXR_HEADSET=fakexr_steam FAKEXR_EYE=600x640 FAKEXR_UNFOCUS=150-250
unset FAKEXR_FAIL_INSTANCE FAKEXR_D3D11
S="vr_xr_test 1;vr_xr_runtime 0;vr_xr_runtime_fallback 0;vr_xr_test_runtimes \"$ST\";vr_xr_test_active \"$ST\";vr_xr_test_processes vrserver.exe;vr_xr_eye_scale 0.8;vr_backend openxr;map start;wait400;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "OpenXR|VR:" > "$OUT/headset_game.log"
unset FAKEXR_HEADSET FAKEXR_EYE FAKEXR_UNFOCUS
cp -f "$XRLOG" "$OUT/headset_openxr.txt"
L="$OUT/headset_openxr.txt"
check $L "the fake headset's session logged" "extensions enabled: XR_KHR_opengl_enable" "system \"FakeXR headset\"" \
    "layers: a projection layer in stage space" "eyes: recommended 600x640" "formats offered .*: GL_RGBA8 GL_SRGB8_ALPHA8" \
    "format chosen: GL_SRGB8_ALPHA8" "xrCreateSwapchain \(left eye\): 480x512 GL_SRGB8_ALPHA8, 3 images" "eye images 480x512 .*vr_xr_eye_scale 0.8" \
    "session FOCUSED" "session VISIBLE \(no input focus" "session FOCUSED"
# (The first timing line may come after the focus is lost: the fake headset's frames aren't paced, frame 150 can come
# before a second has passed; the second's, with the frames shown again, before or after the focus is back.)
check $L "a timing line a second" "session FOCUSED" "xr 1\.[0-9]s: [0-9]+ frames \([0-9]+ rendered"
check $L "the last frame shown again while unfocused" "session VISIBLE \(no input focus" "[1-9][0-9]* last again"
check "$LOG" "unfocused: the last frame shown again, valid layers" "the focus lost" "the focus back" \
    "xrDestroySession: [0-9]+ frames: [0-9]+ projection layers \((9[0-9]|10[0-9]) without an image released"
if grep -q "LAYER_INVALID" "$LOG"; then echo "FAIL: a layer without a released image"; fails=$((fails + 1)); else echo "PASS: no invalid layer"; fi

# 5. The runtime's menu pausing a single player game (vr_xr_unfocused_pause 1): the focus lost for xrWaitFrame 200-300
# (FAKEXR_UNFOCUS): paused, the game's time still, resumed as it comes back (and running on: vr_debug_runtime_menu
# pauses it again later; with sound: faded to vr_xr_unfocused_volume 0); vr_xr_unfocused_pause 0: the game runs on.
LOG="$OUT/pause_fake.log"; : > "$LOG"
export FAKEXR_LOG="$(cygpath -w "$LOG")" FAKEXR_HEADSET=fakexr_steam FAKEXR_UNFOCUS=200-300
unset FAKEXR_FAIL_INSTANCE FAKEXR_D3D11 FAKEXR_EYE
P="vr_xr_test 1;vr_xr_runtime 0;vr_xr_runtime_fallback 0;vr_xr_test_runtimes \"$ST\";vr_xr_test_active \"$ST\";vr_xr_test_processes vrserver.exe;vr_backend openxr;map start"
S="$P;vr_xr_unfocused_pause 1;wait500;vr_debug_runtime_menu 1;wait5;vr_debug_runtime_menu 0;wait5;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Sound -Filter "VR: the|session lost" > "$OUT/pause_on.log"
S="$P;vr_xr_unfocused_pause 0;wait500;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "VR: the|session lost" > "$OUT/pause_off.log"
unset FAKEXR_HEADSET FAKEXR_UNFOCUS
L="$OUT/pause_on.log"
check $L "the runtime's menu pauses the game, resumes it" "VR: the runtime's menu has the focus$" "VR: the game paused at [0-9.]+ s" \
    "VR: the runtime's menu gave the focus back" "VR: the game resumed after [0-9.]+ s, [0-9]+ frames: game time [0-9.]+ -> [0-9.]+; the sounds down to 0\.0" \
    "VR: the game paused at" "VR: the game resumed"
# The game's time still while paused, on after: the second pause (vr_debug_runtime_menu) later than the first.
T=( $(grep -oE "game time [0-9.]+ -> [0-9.]+" $L | grep -oE "[0-9.]+") )
if [ "${#T[@]}" -ge 4 ] && [ "${T[0]}" = "${T[1]}" ] && [ "${T[2]}" = "${T[3]}" ] && awk "BEGIN{exit !(${T[2]} > ${T[1]} + 0.05)}"; then
    echo "PASS: the game's time still while paused (${T[0]} -> ${T[1]}), on after (${T[2]})"
else echo "FAIL: the game's time while paused: ${T[*]}"; fails=$((fails + 1)); fi
L="$OUT/pause_off.log"
check $L "vr_xr_unfocused_pause 0: the game runs on" "VR: the runtime's menu has the focus \(vr_xr_unfocused_pause 0: the game runs on\)" \
    "VR: the runtime's menu gave the focus back"
if grep -q "VR: the game paused" $L; then echo "FAIL: paused with vr_xr_unfocused_pause 0"; fails=$((fails + 1)); else echo "PASS: not paused with vr_xr_unfocused_pause 0"; fi

# 6. The menu's status box's eye lines (vr_status prints them): a Quest 3 by its name (FAKEXR_SYSTEM) at 150% each side
# (2.25 times the panel's pixels): the panel, the runtime's share, the warning and its hint; Eye Image Size 0.66: none.
export FAKEXR_LOG="$(cygpath -w "$OUT/status_fake.log")" FAKEXR_HEADSET=fakexr_steam FAKEXR_SYSTEM="Meta Quest 3" FAKEXR_EYE=3096x3312
P="vr_xr_test 1;vr_xr_runtime 0;vr_xr_runtime_fallback 0;vr_xr_test_runtimes \"$ST\";vr_xr_test_active \"$ST\";vr_xr_test_processes vrserver.exe;vr_backend openxr;wait30;vr_status"
S="vr_xr_panel \"\";vr_render_scale 1;vr_xr_eye_scale 1;$P;vr_restart;vr_xr_eye_scale 0.66;wait5;vr_restart;wait30;echo SCALED;vr_status;vr_xr_eye_scale 1;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "status:|SCALED" > "$OUT/status.log"
unset FAKEXR_HEADSET FAKEXR_SYSTEM FAKEXR_EYE
L="$OUT/status.log"
check $L "the status box: a Quest 3's panel, the eyes too large" "status: Eyes 3096x3312 \(10\.3 Mpx\)" "status: = runtime's 3096x3312 x1\.00 x1\.00" \
    "status: Panel 2064x2208: runtime 225%, eyes 225%" "status: ! Eyes 2\.2x the panel's pixels: lower" "status: ! Eye Image Size" \
    "status: ! or the runtime's resolution"
check $L "Eye Image Size 0.66: the eyes at the panel's size, no warning" "SCALED" "status: = runtime's 3096x3312 x0\.66 x1\.00" \
    "status: Panel 2064x2208: runtime 225%, eyes 98%"
if sed -n '/SCALED/,$p' $L | grep -q "status: !"; then echo "FAIL: a warning at Eye Image Size 0.66"; fails=$((fails + 1)); else echo "PASS: no warning at 0.66"; fi
# 7. A crash inside the runtime (VDXR 1.1.0's null read in xrEnumerateSwapchainImages, 1.0.1), caught (vr_xr_guard 1).
# a: only VDXR's sRGB swapchains crash (FAKEXR_FAULT_SRGB): once more in GL_RGBA8, VR on with it; each request logged
# before its call; vr_xr_runtime_explain lists the API layers and the other programs' DLLs.
LOG="$OUT/fault_fake.log"; : > "$LOG"
export FAKEXR_LOG="$(cygpath -w "$LOG")" FAKEXR_HEADSET=fakexr_vd,fakexr_steam FAKEXR_FAULT_SRGB=fakexr_vd
unset FAKEXR_FAIL_INSTANCE FAKEXR_D3D11 FAKEXR_EYE FAKEXR_UNFOCUS FAKEXR_SYSTEM
P="vr_xr_test 1;vr_xr_runtime 4;vr_xr_test_runtimes \"$VD,$ST\";vr_xr_test_active \"$ST\";vr_xr_test_processes \"VirtualDesktop.Streamer.exe,vrserver.exe\";vr_backend openxr;map start;wait60"
bash "$KIT/run.sh" "$NAME" -Script "$P;echo FAULTSTATUS;vr_xr_runtime_explain;toggleconsole;quit" -Full \
    -Filter "OpenXR|VR:|FAULTSTATUS|last start|API layers|DLLs" > "$OUT/fault_srgb.log"
cp -f "$XRLOG" "$OUT/fault_srgb_openxr.txt"
L="$OUT/fault_srgb.log"
check $L "a crash in the runtime's sRGB swapchain: caught, once more in GL_RGBA8" "trying Virtual Desktop" \
    "FakeXR fakexr_vd [0-9.]+ crashed inside xrEnumerateSwapchainImages: an access violation reading 0x0 in" \
    "^fakexr_vd\.dll\+0x[0-9a-f]+; caught \(vr_xr_guard 1\)" \
    "once more in GL_RGBA8" "the eye swapchains are GL_RGBA8 \(GL_SRGB8_ALPHA8 failed: the runtime crashed in it, caught\)" \
    "FAULTSTATUS" "implicit OpenXR API layers: [0-9]+ installed" "other programs' DLLs in the game" "last start: .*Virtual Desktop"
check "$OUT/fault_srgb_openxr.txt" "the requests logged before each call, the retry's too" "API layers: [0-9]+ on" "GL interop: glCreateMemoryObjectsEXT" \
    "xrCreateSession \(OpenGL" "xrCreateSwapchain \(left eye\): requesting 400x440 GL_SRGB8_ALPHA8 \(0x8c43\), usage 0x1, 1 sample" \
    "xrCreateSwapchain \(left eye\): xrEnumerateSwapchainImages \(3 OpenGL images\)" "crashed inside xrEnumerateSwapchainImages" \
    "xrCreateSwapchain \(left eye\): requesting 400x440 GL_RGBA8" "xrCreateSwapchain \(right eye\): 400x440 GL_RGBA8, 3 images" "OpenXR: started"
check "$LOG" "the fake runtime crashed once, then made the GL_RGBA8 swapchains" "fakexr_vd xrEnumerateSwapchainImages: crashing" \
    "fakexr_vd xrCreateSwapchain 400x440 format 0x8058" "fakexr_vd xrCreateSwapchain 400x440 format 0x8058"
# b: every VDXR swapchain crashes (FAKEXR_FAULT_IMAGES): VDXR given up (destroyed, guarded), Auto on to SteamVR's fake; a
# crash later: the report's VR line names the runtime, the headset and the layers.
: > "$LOG"
unset FAKEXR_FAULT_SRGB
export FAKEXR_FAULT_IMAGES=fakexr_vd
bash "$KIT/run.sh" "$NAME" -Script "$P;vr_crash_test av" -Full -Filter "OpenXR|VR:|ENGINE CRASH|crash" > "$OUT/fault_all.log"
unset FAKEXR_FAULT_IMAGES FAKEXR_HEADSET
L="$OUT/fault_all.log"
check $L "every swapchain crashing: that runtime given up, the next one started" "trying Virtual Desktop" \
    "crashed inside xrEnumerateSwapchainImages" "once more in GL_RGBA8" "crashed inside xrEnumerateSwapchainImages" \
    "Virtual Desktop \(VDXR\) failed to start" "VR: FakeXR fakexr_vd [0-9.]+ crashed \(xrEnumerateSwapchainImages: an access violation" \
    "the game caught it and tries the next runtime" \
    "trying SteamVR" "OpenXR runtime: FakeXR fakexr_steam"
check $L "the crash report's VR line" "ENGINE CRASH" "VR: OpenXR FakeXR fakexr_steam [0-9.]+ \(SteamVR\); API layers: [0-9]+ on" "overlays/hooks: " \
    "headset \"FakeXR headset\""
check "$LOG" "VDXR's fake destroyed after its crashes, SteamVR's made" "fakexr_vd xrEnumerateSwapchainImages: crashing" \
    "fakexr_vd xrEnumerateSwapchainImages: crashing" "fakexr_vd xrDestroySession" "fakexr_vd xrDestroyInstance" "fakexr_steam xrCreateInstance"
# c: the GL context not current when the swapchain is asked for (vr_xr_test_drop_context): made current again, logged;
# the runtime finds it current.
: > "$LOG"
export FAKEXR_HEADSET=fakexr_steam
S="vr_xr_test 1;vr_xr_runtime 2;vr_xr_test_runtimes \"$ST\";vr_xr_test_active \"$ST\";vr_xr_test_processes vrserver.exe;vr_xr_test_drop_context 1;vr_backend openxr;wait30;toggleconsole;quit"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -Filter "OpenXR|VR:" > "$OUT/dropctx.log"
unset FAKEXR_HEADSET
cp -f "$XRLOG" "$OUT/dropctx_openxr.txt"
check "$OUT/dropctx_openxr.txt" "the GL context made current again before the swapchain" "xrCreateSession: GL context [0-9A-Fa-fx]+, DC" \
    "xrCreateSwapchain \(left eye\): GL context 0+, DC 0+ current" "the game's GL context wasn't current on this thread .*made current again: done" \
    "xrCreateSwapchain \(left eye\): 400x440 GL_SRGB8_ALPHA8, 3 images" "OpenXR: started"
if grep -q "NO GL CONTEXT" "$LOG"; then echo "FAIL: the fake runtime saw no GL context"; fails=$((fails + 1)); else echo "PASS: the fake runtime always had the GL context"; fi
# 8. Diagnostics mode (-diagnostics): <game folder>/diagnostics/<date>_<time> gets the console, the GL debug output (a
# message of the game's own first), the OpenXR debug messenger's messages (the fake runtime's with FAKEXR_DEBUG_UTILS,
# and the loader's), what is said to a debugger, the crash report, the logs copied; off by default: no folder.
export FAKEXR_LOG="$(cygpath -w "$OUT/diag_fake.log")" FAKEXR_HEADSET=fakexr_steam FAKEXR_DEBUG_UTILS=fakexr_steam
DIAGS="C:/OHWorkspace/qvr-kit/bases/$NAME/qbase/quakevr/diagnostics"
before=$(ls "$DIAGS" 2>/dev/null | wc -l)
S="vr_xr_test 1;vr_xr_runtime 2;vr_xr_test_runtimes \"$ST\";vr_xr_test_active \"$ST\";vr_xr_test_processes vrserver.exe;vr_backend openxr;map start;wait60;vr_diagnostics_status;vr_crash_test av"
bash "$KIT/run.sh" "$NAME" -Script "$S" -Full -ExtraArgs "-diagnostics" -Filter "iagnostics|ENGINE CRASH" > "$OUT/diag.log"
unset FAKEXR_HEADSET FAKEXR_DEBUG_UTILS
DIAG="$DIAGS/$(ls "$DIAGS" | tail -1)"
check "$DIAG/console.log" "diagnostics: the console" "Diagnostics mode \(-diagnostics\): logs go to .*diagnostics" "Diagnostics mode on"
check "$DIAG/gl_debug.log" "diagnostics: GL debug output" "Quake VR diagnostics: GL debug output on"
check "$DIAG/openxr_debug.log" "diagnostics: the OpenXR messenger" "debug messenger: on \(XR_EXT_debug_utils\)" \
    "\[info\] FAKEXR-hello xrCreateDebugUtilsMessengerEXT \(1 so far\)" "OpenXR-Loader" "step: xrCreateSession"
check "$DIAG/diagnostics.txt" "diagnostics: the summary, the logs copied after the crash" "diagnostics \(-diagnostics\)" \
    "debug_output.log: OutputDebugString .*: caught" "Ended: a crash" "Logs copied:" "gl_startup.log -> gl_startup.log"
if ls "$DIAG"/crash/*.txt > /dev/null 2>&1 && [ -f "$DIAG/debug_output.log" ]; then echo "PASS: diagnostics: the crash report and debug_output.log in the folder"
else echo "FAIL: diagnostics: no crash report or debug_output.log in $DIAG"; fails=$((fails + 1)); fi
mid=$(ls "$DIAGS" | wc -l)
bash "$KIT/run.sh" "$NAME" -Script "wait5;toggleconsole;quit" -Filter "iagnostics" > /dev/null
after=$(ls "$DIAGS" | wc -l)
if [ "$mid" -eq $((before + 1)) ] && [ "$after" -eq "$mid" ]; then echo "PASS: diagnostics: one folder for -diagnostics, none without"
else echo "FAIL: diagnostics folders: $before -> $mid -> $after"; fails=$((fails + 1)); fi
echo "xr_runtime_test: $fails failed"
