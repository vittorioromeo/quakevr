# Launch flags Quake VR adds

Ironwail's own flags (`-window`, `-width`, `-basedir`, `-game`, `-nosound`, `-condebug`, ...) are upstream's and not
listed here. Naming follows upstream: lowercase, no separators (`-nosound`, `-noclipcontrol`).

**Prefer a cvar.** A setting that can wait until the `+` commands run (after the configs) is a `+cvar value` on the
command line, not a new flag. A flag is for what must be known earlier (before the window, the VR backend, the thread
pool or the configs) or for a one-run version of an archived cvar. Where something must be read before the `+`
commands apply, the engine reads the `+cvar` from the command line early (`+vr_backend`, `+vr_enabled`,
`+vr_xr_runtime`, `+vr_jobs_threads`) instead of adding a flag.

## Flags

| Flag | What it does | Who uses it |
|---|---|---|
| `-flatscreenmode` | Flat-screen mode: the VR gameplay on the desktop, no headset (vr_flatplay.cpp) | the installer's flat shortcut, README, ~180 tests |
| `-diagnostics` | Diagnostics mode for this run only (`vr_diagnostics 1` is the saved setting) | the diagnostics shortcut, INSTALL.md, xr_runtime_test.sh |
| `-bugreport` | The bug report bundle (vr_runlogs.cpp) | the bug-report shortcut, docs |
| `-hidemirror` | No desktop window, the game always "focused": a standalone headset's (the Steam Frame) | Steam Frame players, the Linux/WSL kit runs |
| `-unattended` | Nobody wears the headset: its presence sensor (XR_EXT_user_presence) is ignored | the Quest's unmanned perf scripts, xr_runtime_test.sh |
| `-glsafe` | Graphics safe mode for this run (`vr_glsafe 1` is the saved setting) | players after a start-up crash (INSTALL.md troubleshooting) |
| `-noglsafe` | No automatic safe mode | Quest APK READMEs |
| `-ragtool` | The ragdoll tool (vr_ragtool.cpp, RAGTOOL.md) | the author |
| `-prepare <file>`, `-preparerelight` | The installer's preparation run (vr_prepare.cpp) | the installer |
| `-noconfigwrite`, `-noautoexec` | No config written at quit; autoexec.cfg skipped | the installer, the kit, tests |
| `-foreignconfigs` | Other engines' start-up configs allowed (they run before the `+` commands: no cvar can say it) | kit run.ps1, qvrprof.sh, mp_harness |
| `-evalcopy` | The motion review's background copy (never starts copies of its own) | the engine itself |
| `-nomapindex` | No map index or Slipseer fetch | the installer, ~50 tests |
| `-nostartmap` | No tutorial or hub map at start | mp_harness, mp_test |
| `-loopbackonly` | Networking on 127/8 only (before the drivers start) | the MP tests, scenario_check |
| `-natgateway <port>` | The NAT test shim (net_dgrm.c) | mp_test, mp_nat |
| `-cmdinbox <path>` | A command inbox file the game polls (vr_mptest.cpp) | the MP harness, hub_start_test |
| `-condebugfile <path>` | `-condebug`'s log somewhere else | mp_harness |
| `-windowpos x y` | The window's position | mp_local.ps1 |
| `-nomic` | No microphone capture | the MP harness |
| `-fakeandroid` (`QVR_FAKE_ANDROID`) | Android's paths and limits on the PC (no child processes among them) | questogg_test.sh, quakevr.props |
| `-memguard` (`QVR_MEMGUARD`) | Page-guarded allocations (a use-after-free hunt) | debugging only |
| `-glescompat` (`QVR_GLES_COMPAT`) | The OpenGL ES 3.2 feature set on the PC | gles_render_test.sh, bench cfgs |
| `-glesclipcontrol` (`QVR_GLES_CLIP_CONTROL`) | Under `-glescompat`: EXT_clip_control kept, as the Quest's driver | gles_render_test.sh |
| `-glesnoext` (`QVR_GLES_NOEXT`) | Under `-glescompat`: the optional ES extensions treated as absent (their fallbacks tested) | gles_render_test.sh |
| `-glesdebug a,b,...` (`QVR_GLES_DEBUG`) | The ES profile's bring-up switches, below | Quest bring-up, by hand |
| `-glfakevendor <v>` | Another GPU vendor's workarounds | manual AMD checks |
| `-shadersasync [0/1]` (`QVR_SHADERS_ASYNC`) | Asynchronous shader compiles on or off | Quest README, gles_render_test.sh |
| `-noparallelshaders` | No KHR_parallel_shader_compile (upstream's `-no<glcap>` family) | Quest README |
| `-noviewportlayer` | No viewport-layer shadow capability (players have `vr_shadow_layered 0`) | debugging only |

### `-glesdebug` words

Comma-separated, e.g. `-glesdebug verbose,keep:glsl`. An unknown word is said in the console with the list.

| Word | What it does |
|---|---|
| `noclipcontrol` | Android: the driver's EXT_clip_control not used (shadows, reflections and portals off) |
| `shaderset` | The ES shader-set step (the lite world's first set) in a test run too |
| `srgbencode` | The driver's sRGB write encoding kept (EXT_sRGB_write_control not used) |
| `structcopy` | The shaders' struct copies left whole, as the PC's (a driver workaround's A/B) |
| `centroid` | The shaders' `centroid` qualifiers kept, as the PC's (a driver workaround's A/B) |
| `verbose` | Each ES compile error printed with its source line |
| `keep:<name>` | Under `-glescompat`, this switch keeps its PC form, to measure its ES form's cost: `bindless`, `multibind`, `layer`, `clip`, `glsl`, `foveation`, `views` |

## Deprecated (until 1.3)

Each still works and prints a note at start-up naming its replacement.

| Old | Now |
|---|---|
| `-vrmock` | `+vr_backend mock` (read from the command line before the configs) |
| `-vr-hide-mirror` | `-hidemirror` |
| `-vr-unattended` | `-unattended` |

## Removed in 1.2

| Flag | Instead |
|---|---|
| `-jobs <n>` | `+vr_jobs_threads <n>` (read from the command line when the pool is made) |
| `-mpnetversion <n>` | `+vr_net_test_version <n>` (test cvar) |
| `-netlookupdelay <s>`, `-netlookupfail` | `+vr_net_test_lookup_delay <s>`, `+vr_net_test_lookup_fail 1` (test cvars) |
| `-campaignmissing <folder>` | `+vr_mp_test_campaign_missing <folder>` (test cvar) |
| `-glesdefine A,B` | `+vr_gles_define A,B` |
| `-glesfull` | `+vr_gl_world_lite 0` |
| `-glesnoclipcontrol`, `-gleskeep`, `-glesshaderset`, `-glessrgbencode`, `-glesstructcopy`, `-glescentroid`, `-glesverbose` (`QVR_GLES_KEEP`) | `-glesdebug` (`QVR_GLES_DEBUG`) |
| `-glsafeauto`, `-nochildprocesses` (use `-fakeandroid`), `-rangerhead_nodetail`, `-shadersasynctest`, `-shaderloadshot`, `-epicmanifests` | nothing: nobody passed them |
| `-zone` (upstream's) | nothing: the heap has no maximum since 1.1 (ignored) |

Every flag added to the engine gets a row here (name, what it does, who passes it).
