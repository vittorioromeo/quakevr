# Quake VR: Unleashed — experimental Steam Frame native port

Based on upstream Unleashed 1.0.0. Community ARM64/Linux OpenGL/GLX/OpenXR port;
the original mod is by Vittorio Romeo. This is not an official upstream release.

**Known issue: performance is uneven and some locations need optimization.**
Use classic graphics preset 0, render scale 0.45 and MSAA off as the initial
baseline. Calibration ran near 72 fps on the tested Frame, but this does not
establish sustained campaign/combat performance.

The wearer confirmed entry into actual VR. Hardware evidence includes the
native process registered as SteamVR's active scene, head and both hand tracking,
Frame grip/stick action delivery, completed calibration, and gameplay captured
from the headset compositor. Shotgun/nailgun hits and physical axe attacks were
recorded in the training hub. Automated isolated VM/AO/physics smoke tests passed.

Manual reload, throwing, climbing, campaign combat benchmarks and calibrated
restart still need additional validation. Steam Audio ARM64/HRTF is not included;
the engine uses ordinary Quake audio. Hardware foveation is unavailable in this
OpenGL path. The documentation distinguishes physical tests from mock tests.

## Installation

Extract the Frame kit on the headset. Supply the official upstream **v1.0.0
QuakeVR.zip** and your own Quake id1 folder containing PAK0.PAK and PAK1.PAK:

```sh
python3 install-frame.py --mod-archive /path/to/QuakeVR.zip \
  --quake-data /path/to/Quake/id1 --register
```

Use a fresh installation directory. Original QuakeQuest, saves and PAK files
are not overwritten. Steam registration names the new shortcut
**Quake VR: Unleashed (Steam Frame)** and explicitly marks it as VR. If automatic
registration is unavailable, add run-native.sh to Steam and mark it for the VR
library. Close the Steam dashboard before evaluating the immersive scene.

This package does **not** include commercial Quake PAKs, saves, personal settings,
the Windows installer, private headset photos, or the OpenXR runtime.

See `docs/STEAM-FRAME-DEVELOPMENT.md` in the source tree for the native build,
runtime diagnostics, evidence, installation details and performance methodology.
The engine follows the upstream GPLv2 license and retains third-party notices.

Upstream maintainers are welcome to integrate, cherry-pick or take over this
port in their own repository. This experimental release is intended to make
the working implementation available while performance work continues.
