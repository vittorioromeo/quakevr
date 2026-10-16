#!/usr/bin/env python3
"""Randomized round-robin A/B matrix over run-dummy-kill-repro.py, run ON the Frame.

Each round runs every variant once in a random order; results append to matrix-<name>.jsonl next to this script. The
Frame's SteamVR has to be running for the OpenXR variants; between runs this waits for vrserver and vrcompositor and
cools down, since a GPU hang resets the stack.

The vr_gfx_orphan and vr_decals_test cvars (and the ironwail-test*/-head* binaries that carry them) were test-only hooks in
scratch builds on the Frame; they are not in the source tree. The findings are in docs/STEAM-FRAME-DEVELOPMENT.md.
"""
import json
import os
import random
import shlex
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
XR = ["--backend", "openxr", "--cvar", "vr_xr_unfocused=0", "--cvar", "vr_xr_unfocused_pause=0"]
VARIANTS = {
    "base": XR,
    "noreorder": XR + ["--env", "ZINK_DEBUG=noreorder"],
    "sync": XR + ["--env", "ZINK_DEBUG=sync"],
    "flushsync": XR + ["--env", "ZINK_DEBUG=flushsync"],
    "sysmem": XR + ["--env", "TU_DEBUG=sysmem"],
    "syncdraw": XR + ["--env", "TU_DEBUG=syncdraw"],
    "gore0": XR + ["--cvar", "vr_dummy_gore=0"],
    "nobufstor": XR + ["--arg=-nobufferstorage"],
    "synth0": XR + ["--cvar", "vr_xr_frame_synthesis=0"],
    "mockbase": ["--backend", "mock"],
    "nobgc": XR + ["--env", "ZINK_DEBUG=nobgc"],
    "noopt": XR + ["--env", "ZINK_DEBUG=noopt"],
    "lazy": XR + ["--env", "ZINK_DESCRIPTORS=lazy"],
    "gpl": XR + ["--env", "ZINK_DEBUG=gpl"],
}
# ZINK_DEBUG=flushsync (no flush-queue thread) made the crash certain, so these bisect from there: one change each.
FS = XR + ["--env", "ZINK_DEBUG=flushsync"]
VARIANTS.update({
    "fs_base": FS,
    "fs_idle": FS + ["--kills", "0", "--settle", "1800"],  # no kill at all: does flushsync crash by itself?
    "fs_gib0": FS + ["--cvar", "vr_dummy_gib=0"],  # the same hits, but the dummy does not die
    "fs_gore0": FS + ["--cvar", "vr_dummy_gore=0"],
    "fs_nobufstor": FS + ["--arg=-nobufferstorage"],
    "fs_sysmem": FS + ["--env", "TU_DEBUG=sysmem"],
    "fs_noreorder": XR + ["--env", "ZINK_DEBUG=flushsync,noreorder"],
    "fs_sync": XR + ["--env", "ZINK_DEBUG=flushsync,sync"],
    "fs_lazy": FS + ["--env", "ZINK_DESCRIPTORS=lazy"],
    "fs_syncdraw": FS + ["--env", "TU_DEBUG=syncdraw"],
    "fs_synth0": FS + ["--cvar", "vr_xr_frame_synthesis=0"],
    "fs_mock": ["--backend", "mock", "--env", "ZINK_DEBUG=flushsync"],
})
# What a hit/kill adds to the world: blood decals on the world (SSBOs 3/4 + atlas on unit 14 for the world shader),
# blood painted on models (render-to-layer into the wound texture arrays), gore (gibs, sprays, pools).
VARIANTS.update({
    "fs_decals0": FS + ["--cvar", "vr_decals=0"],
    "fs_wounds0": FS + ["--cvar", "vr_wounds=0"],
    "fs_goreall0": FS + ["--cvar", "vr_gore=0", "--cvar", "vr_dummy_gore=0"],
    "fs_dw0": FS + ["--cvar", "vr_decals=0", "--cvar", "vr_wounds=0"],
    "fs_all0": FS + ["--cvar", "vr_decals=0", "--cvar", "vr_wounds=0", "--cvar", "vr_gore=0", "--cvar", "vr_dummy_gore=0"],
    "fs_late0": FS + ["--cvar", "vr_xr_late_acquire=0"],
})
# Valve's FDM injection layer (XR_APILAYER_VALVE_fdm_injection / VK_LAYER_VALVE_fdm_injection) is loaded only with the
# OpenXR backend (the mock one never crashes): it adds fragment density maps (foveation) to the render passes Zink makes.
VARIANTS.update({
    "fdmdbg": FS + ["--env", "FDM_DEBUG=debug", "--kills", "1"],
    "nofdmlayer": XR + ["--env", "DISABLE_VULKAN_FDM_INJECTION_LAYER=1"],
    "fs_nofdmlayer": FS + ["--env", "DISABLE_VULKAN_FDM_INJECTION_LAYER=1"],
    "fs_tunofdm": FS + ["--env", "TU_DEBUG=nofdm"],
    "fs_fdm_nolayered": FS + ["--env", "FDM_DEBUG=disable_layered"],
    "fs_fdm_nooffsets": FS + ["--env", "FDM_DEBUG=disable_offsets"],
})

# The engine's own buffers (decal SSBOs, static meshes) are orphaned with glBufferData(NULL); vr_gfx_orphan 0 replaces them
# with a fresh buffer instead. Same test binary (ironwail-test, built with that cvar), so this is a clean A/B.
TEST = ["--exe", "ironwail-test"]
VARIANTS.update({
    "fs_orph1": FS + TEST + ["--cvar", "vr_gfx_orphan=1"],
    "fs_orph0": FS + TEST + ["--cvar", "vr_gfx_orphan=0"],
    "orph1": XR + TEST + ["--cvar", "vr_gfx_orphan=1"],
    "orph0": XR + TEST + ["--cvar", "vr_gfx_orphan=0"],
    "fs_orph0_lazy": FS + TEST + ["--cvar", "vr_gfx_orphan=0", "--env", "ZINK_DESCRIPTORS=lazy"],
    "fs_orph1_lazy": FS + TEST + ["--cvar", "vr_gfx_orphan=1", "--env", "ZINK_DESCRIPTORS=lazy"],
})

# Where in the decal path the crash starts: the world's shader reading the decals (vr_decals_test 1: it never does, the
# buffers are still made and bound), the buffers bound (2: made, never bound), or the SSBO path at all (vr_decals_world 0:
# the marks as meshes over the world instead). ironwail-test2 has vr_decals_test; the A/B baseline is the same binary.
TEST2 = ["--exe", "ironwail-test2"]
VARIANTS.update({
    "fs_dtest0": FS + TEST2 + ["--cvar", "vr_decals_test=0"],
    "fs_dnoread": FS + TEST2 + ["--cvar", "vr_decals_test=1"],
    "fs_dnobind": FS + TEST2 + ["--cvar", "vr_decals_test=2"],
    "fs_dworld0": FS + TEST2 + ["--cvar", "vr_decals_world=0"],
    "fs2_orph0": FS + TEST2 + ["--cvar", "vr_gfx_orphan=0"],
})

# The engine binaries the Frame holds, each in the same scratch game with the same flushsync + OpenXR setup:
# ironwail (= release-386e4ba, the installed one), the two spacewarp test builds, ironwail-test/-test2 (spacewarp tree + the
# orphan/decal test cvars), and builds of older or newer commits made by bisect-build.sh (ironwail-<name>).
VARIANTS.update({
    "fs_bin_rel": FS,
    "fs_bin_swsrgb": FS + ["--exe", "ironwail-spacewarp-srgb-test"],
    "fs_bin_sw": FS + ["--exe", "ironwail-spacewarp-test"],
    "fs_bin_swea": FS + ["--exe", "ironwail.spacewarp-ea1d62-20261015"],
    "fs_bin_t2": FS + TEST2,
    "fs_bin_head": FS + ["--exe", "ironwail-head"],  # a clean build of the git HEAD source (the release plus a few diagnostics)
})

# ironwail-head2: the git HEAD source with the vr_gfx_orphan / vr_decals_test cvars added and nothing else changed.
HEAD2 = ["--exe", "ironwail-head2"]
VARIANTS.update({
    "h2_base": FS + HEAD2,
    "h2_orph0": FS + HEAD2 + ["--cvar", "vr_gfx_orphan=0"],
    "h2_dnoread": FS + HEAD2 + ["--cvar", "vr_decals_test=1"],
    "h2_dnobind": FS + HEAD2 + ["--cvar", "vr_decals_test=2"],
    "h2_dworld0": FS + HEAD2 + ["--cvar", "vr_decals_world=0"],
    "h2_decals0": FS + HEAD2 + ["--cvar", "vr_decals=0"],
    "h2_lazy": FS + HEAD2 + ["--env", "ZINK_DESCRIPTORS=lazy"],
})

# ironwail-head3: HEAD2 + vr_stereo_test 1 (each eye image detached from its framebuffer BEFORE xrReleaseSwapchainImage, as the
# spacewarp work tree does). ironwail-head4: also the world shader's decal path in steps, picked by vr_decals_test (see
# QVR_DECAL_FUNCTIONS's tmode): 3 the grid read but no decals, 4 no reads, 5 one read, 6 decals read but no atlas reads,
# 7 decal read and nothing more (1 the shader never reads, 2 the buffers never bound: those work on head2 too).
HEAD3 = ["--exe", "ironwail-head3"]
HEAD4 = ["--exe", "ironwail-head4"]
VARIANTS.update({
    "h3_base": FS + HEAD3,
    "h3_detach": FS + HEAD3 + ["--cvar", "vr_stereo_test=1"],
    "h4_base": FS + HEAD4,
})
for _mode in (3, 4, 5, 6, 7):
    VARIANTS["h4_t%d" % _mode] = FS + HEAD4 + ["--cvar", "vr_decals_test=%d" % _mode]

# ironwail-head5: HEAD2 + the spacewarp tree's vr_profile.cpp/.hpp (the profiler's GPU timestamp queries changed there).
HEAD5 = ["--exe", "ironwail-head5"]
VARIANTS.update({
    "h5_base": FS + HEAD5,
    "h5_pg0": FS + HEAD5 + ["--cvar", "vr_profile_gpu=0"],
})



# Turnip/ir3 switches on HEAD2 (the same flushsync + OpenXR set-up): which compiler or driver path the crash lives in.
VARIANTS.update({
    "h2_nodescpf": FS + HEAD2 + ["--env", "IR3_SHADER_DEBUG=nodescprefetch"],
    "h2_nopre": FS + HEAD2 + ["--env", "IR3_SHADER_DEBUG=nopreamble"],
    "h2_noearlypre": FS + HEAD2 + ["--env", "IR3_SHADER_DEBUG=noearlypreamble"],
    "h2_nouboopt": FS + HEAD2 + ["--env", "IR3_SHADER_DEBUG=nouboopt"],
    "h2_nofp16": FS + HEAD2 + ["--env", "IR3_SHADER_DEBUG=nofp16"],
    "h2_flushall": FS + HEAD2 + ["--env", "TU_DEBUG=flushall"],
    "h2_nolrz": FS + HEAD2 + ["--env", "TU_DEBUG=nolrz"],
    "h2_noubwc": FS + HEAD2 + ["--env", "TU_DEBUG=noubwc"],
    "h2_gmem": FS + HEAD2 + ["--env", "TU_DEBUG=gmem"],
    "h2_forcebin": FS + HEAD2 + ["--env", "TU_DEBUG=forcebin"],
    "h2_nobin": FS + HEAD2 + ["--env", "TU_DEBUG=nobin"],
    "h2_sysmem": FS + HEAD2 + ["--env", "TU_DEBUG=sysmem"],
})


# Turnip's autotune (picks GMEM or sysmem for each render pass; the same flushsync + OpenXR set-up on HEAD2).
VARIANTS.update({
    "h2_atbw": FS + HEAD2 + ["--env", "TU_AUTOTUNE_ALGO=bandwidth"],
    "h2_atprof": FS + HEAD2 + ["--env", "TU_AUTOTUNE_ALGO=profiled"],
    "h2_atprofimm": FS + HEAD2 + ["--env", "TU_AUTOTUNE_ALGO=profiled_imm"],
    "h2_atsys": FS + HEAD2 + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem"],
    "h2_atflags0": FS + HEAD2 + ["--env", "TU_AUTOTUNE_FLAGS=0"],
    "h2_atlog": FS + HEAD2 + ["--env", "TU_DEBUG=startup", "--env", "TU_AUTOTUNE_ALGO=bandwidth", "--env", "MESA_LOG_LEVEL=debug"],
})

VARIANTS.update({
    "h2_disasm": FS + HEAD2 + ["--env", "IR3_SHADER_DEBUG=disasm"],
    "h2_spillall": FS + HEAD2 + ["--env", "IR3_SHADER_DEBUG=spillall"],
})

# E2: the world decals read through buffer textures (ironwail-head6), against the SSBO build (HEAD2).
HEAD6 = ["--exe", "ironwail-head6"]
VARIANTS.update({
    "h6_base": FS + HEAD6,
    "h6_async": XR + HEAD6,
    "h2_async": XR + HEAD2,
    "h6_noaf": FS + HEAD6 + ["--cvar", "vr_decals_world=1"],
})

# E3: HEAD7 = E2 + vr_gfx_orphan 2 (the engine's storage buffers updated in place, never replaced while they fit).
HEAD7 = ["--exe", "ironwail-head7"]
VARIANTS.update({
    "h7_inplace": FS + HEAD7 + ["--cvar", "vr_gfx_orphan=2"],
    "h7_ctl": FS + HEAD7,
})

# The same switches on the real (non-flushsync) game path: OpenXR, async frames, HEAD2 and the release binary (the default --exe).
VARIANTS.update({
    "a2_atsys": XR + HEAD2 + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem"],
    "a2_gmem": XR + HEAD2 + ["--env", "TU_DEBUG=gmem"],
    "a2_noubwc": XR + HEAD2 + ["--env", "TU_DEBUG=noubwc"],
    "a2_sysmem": XR + HEAD2 + ["--env", "TU_DEBUG=sysmem"],
    "a2_lazy": XR + HEAD2 + ["--env", "ZINK_DESCRIPTORS=lazy"],
    "rel_base": XR,
    "rel_atsys": XR + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem"],
    "rel_gmem": XR + ["--env", "TU_DEBUG=gmem"],
    "rel_noubwc": XR + ["--env", "TU_DEBUG=noubwc"],
    "rel_lazy": XR + ["--env", "ZINK_DESCRIPTORS=lazy"],
    "relfs_base": FS,
    "relfs_atsys": FS + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem"],
    "relfs_gmem": FS + ["--env", "TU_DEBUG=gmem"],
    # the marks as meshes over the world (vr_decals_world 0) instead of read by the world shader, alone and with the autotune switch
    "rel_dw0": XR + ["--cvar", "vr_decals_world=0"],
    "relfs_dw0": FS + ["--cvar", "vr_decals_world=0"],
    "rel_atsys_dw0": XR + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem", "--cvar", "vr_decals_world=0"],
})

# ironwail-bind: the HEAD source plus a test-only check that the world draw's SSBO bindings 3/4 (the decals) are what the
# engine expects (QVR_DECAL_BINDTEST=1 logs a mismatch, =2 unbinds and rebinds them before each draw call).
BIND = ["--exe", "ironwail-bind"]
VARIANTS.update({
    "bc_fs_base": FS + BIND,
    "bc_fs_check": FS + BIND + ["--env", "QVR_DECAL_BINDTEST=1"],
    "bc_fs_rebind": FS + BIND + ["--env", "QVR_DECAL_BINDTEST=2"],
    "bc_base": XR + BIND,
    "bc_check": XR + BIND + ["--env", "QVR_DECAL_BINDTEST=1"],
    "bc_rebind": XR + BIND + ["--env", "QVR_DECAL_BINDTEST=2"],
})

# Mesa's own threads on the release binary's real path: the gallium threaded context and the GL thread.
VARIANTS.update({
    "rel_nothread": XR + ["--env", "GALLIUM_THREAD=0"],
    "rel_noglthread": XR + ["--env", "mesa_glthread=false"],
    "rel_nothread_both": XR + ["--env", "GALLIUM_THREAD=0", "--env", "mesa_glthread=false"],
})

# Turnip's autotune says per render pass why it picked GMEM or sysmem (e.g. "selecting=GMEM reason=fdm"); read it with prefer_sysmem on.
VARIANTS.update({
    "rel_atsys_log": XR + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem", "--env", "MESA_LOG_LEVEL=debug"],
})

# The Steam client's environment (what a shortcut-launched game inherits) has MESA_GL_VERSION_OVERRIDE=4.3 and
# MESA_GLSL_VERSION_OVERRIDE=430; a plain ssh shell does not, so every run above saw GL 4.6 where the headset's saw 4.3.
OV = ["--env", "MESA_GL_VERSION_OVERRIDE=4.3", "--env", "MESA_GLSL_VERSION_OVERRIDE=430"]
VARIANTS.update({
    "ov_base": XR + OV,
    "ov_atsys": XR + OV + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem"],
    "ov_dw0": XR + OV + ["--cvar", "vr_decals_world=0"],
    "ov_fs_base": FS + OV,
})

# The headset's faulting submit had the world pass in GMEM despite prefer_sysmem (autotune picks GMEM for reasons such as
# fdm or preempt_latency_risk, which a headless run may never hit). TU_DEBUG=gmem forces what the wearer's run had:
# does a fix hold when GMEM is certain?
GM = XR + OV + ["--env", "TU_DEBUG=gmem"]
VARIANTS.update({
    "g_base": GM,
    "g_atsys": GM + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem"],
    "g_dw0": GM + ["--cvar", "vr_decals_world=0"],
    "g_sys": XR + OV + ["--env", "TU_DEBUG=sysmem"],
    "g_fs_dw0": FS + OV + ["--env", "TU_DEBUG=gmem", "--cvar", "vr_decals_world=0"],
})

# The installed launcher since 2026-10-15 ~16:30 (the headset survived kills with it): prefer_sysmem, TU_DEBUG=sysmem,
# +vr_decals_world 0, the Steam environment.
VARIANTS.update({
    "inst": XR + OV + ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem", "--env", "TU_DEBUG=sysmem", "--cvar", "vr_decals_world=0"],
})

# What the kill draws that still hangs the GPU with the world's decal loop off (rp1, the shotgun take replayed with GMEM forced:
# g_dw0 hung 3/3, the scripted kills 0/4): g_dw0 minus one thing at a time. Every hang came right after the kill, with the gibs,
# the head pop and the notification's hologram and text board made then.
BI = GM + ["--cvar", "vr_decals_world=0"]
VARIANTS.update({
    "b_rec": BI + ["--cvar", "vr_recording_clean=1"],        # no hologram, wrist log or head-locked text
    "b_gore": BI + ["--cvar", "vr_dummy_gore=0"],            # no gore sprays, wounds or small gibs from the dummy
    "b_nodecal": BI + ["--cvar", "vr_decals=0"],             # no blood marks at all, in the shader or as meshes
    "b_noholo": BI + ["--cvar", "vr_messages_hologram=0", "--cvar", "vr_notify_wrist=0"],
})


# The merge of frame-native + the spacewarp WIP + origin/next (build-next2, build-next2-sw): smoke tests with the installed
# launcher's workarounds. sw1: the sw build's GL/Vulkan interop transport (latched at VR start); sw2: and frame synthesis.
SW = ["--env", "TU_AUTOTUNE_ALGO=prefer_sysmem", "--env", "TU_DEBUG=sysmem", "--cvar", "vr_decals_world=0"]
VARIANTS.update({
    "sw1": XR + OV + SW + ["--cvar", "vr_xr_vulkan_interop=1"],
    "sw2": XR + OV + SW + ["--cvar", "vr_xr_vulkan_interop=1", "--cvar", "vr_xr_frame_synthesis=1"],
})


def alive(name):
    return subprocess.run(["pgrep", "-x", name], capture_output=True).returncode == 0


def main():
    name = sys.argv[1]
    rounds = int(sys.argv[2])
    names = sys.argv[3].split(",") if len(sys.argv) > 3 else list(VARIANTS)
    kills = sys.argv[4] if len(sys.argv) > 4 else "6"
    common = []  # KEY=VALUE environment for every variant, e.g. MESA_SHADER_CACHE_DISABLE=true (a cold cache each run)
    for kv in (sys.argv[5].split(",") if len(sys.argv) > 5 and sys.argv[5] else []):
        common += ["--env", kv]
    # KILLREPRO_SCEN: the scenario's arguments for every run, e.g. "--scenario replay --take slash_2026-10-15_17-41-41.csv --repeat 4
    # --play-arg kill --exe ironwail-replay" (default: the scripted kills, vr_dummy_test 3)
    scen = shlex.split(os.environ.get("KILLREPRO_SCEN", "--scenario test3"))
    log = HERE / f"matrix-{name}.jsonl"
    rng = random.Random(name)
    for r in range(1, rounds + 1):
        order = names[:]
        rng.shuffle(order)
        for v in order:
            for _ in range(24):
                if alive("vrserver") and alive("vrcompositor"):
                    break
                time.sleep(5)
            tag = f"{name}-{v}-r{r}"
            cmd = ["python3", str(HERE / "run-dummy-kill-repro.py"), "--tag", tag, "--kills", kills,
                   "--timeout", "300"] + scen + common + VARIANTS[v]
            p = subprocess.run(cmd, capture_output=True, text=True)
            line = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else ""
            try:
                row = json.loads(line)
            except ValueError:
                row = {"tag": tag, "error": (p.stdout + p.stderr)[-400:]}
            row["variant"] = v
            row["round"] = r
            with log.open("a") as f:
                f.write(json.dumps(row) + "\n")
            print(tag, "exit", row.get("exit"), "kills", row.get("kills_begun"), "hang", row.get("hangcheck_lines"),
                  "faults", row.get("gpu_fault_lines"), "sp", row.get("sp_fault_lines"), "killed", row.get("dummy_killed_lines"), "dumps", len(row.get("new_dumps", [])), flush=True)
            time.sleep(15)
    print("MATRIX_DONE", flush=True)


if __name__ == "__main__":
    main()
