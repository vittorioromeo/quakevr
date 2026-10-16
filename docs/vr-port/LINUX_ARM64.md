# Linux and ARM64 (the Steam Frame)

Experimental. Quake VR builds and runs natively on ARM64 Linux with OpenXR, on Valve's Steam Frame (SteamVR's linuxarm64
OpenXR runtime). The port is Kasper Olesen's (kblood, PR #75), tested by him on a Frame in 2026-10; this branch takes
its build and portability part. Windows builds are unchanged by it: every Linux path is behind `_WIN32`/`__linux__` or
an ARM check, or in `vr.cmake`'s Linux branch, which MSBuild never reads.

## What the port changes

- **OpenXR on Linux** (`vr_backend_openxr.cpp`): the session binds the game's OpenGL context through
  `XrGraphicsBindingOpenGLXlibKHR` (SDL's GLX context and drawable, with its framebuffer configuration and visual).
  SDL must use X11 (`SDL_VIDEODRIVER=x11`). The Windows-only parts (the structured-exception guard around runtime calls,
  the WGL context checks, the VDXR interop log, `vr_xr_test_drop_context`) are compiled on Windows only; elsewhere a
  crash inside the runtime is not caught.
- **SSE/AVX kernels on ARM** (`vr_simd.hpp`): the AO bake's rays and the audio mixer keep their x86 intrinsics; on ARM,
  SIMDe 0.8.2 (`external/simde`, MIT) supplies them. `vr.cmake` turns off FMA contraction for those two files so the
  arithmetic stays in the SSE order.
- **Build fixes**: Zancle's `__decay` builtin avoided on clang 19+/gcc 15+; mimalloc's own new/delete left out
  (`vr_alloccount.cpp` has them); `vr_qcprofile` counts SDL's performance counter off Windows (no `__rdtsc`).
- **`-vr-hide-mirror`**: the desktop window is never shown (a standalone headset has no desktop to look at). The window
  then counts as focused: every present is made (no `vr_mirror_unfocused_hz` thinning) and the menus' "not focused"
  warning stays off.
- **The Frame's controllers**: `/interaction_profiles/valve/frame_controller_valve`, enabled when the runtime offers
  `XR_VALVE_frame_controller_interaction` (with the other vendor profiles, `vr_xr_vendor_profiles 1`).

## Building

Native on the device (the tested toolchain: clang 19 with C++23), with SDL2, X11, GL/GLX, the OpenXR headers and the
engine's audio libraries:

```sh
cmake -S . -B build-frame -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_USERDIRS=OFF \
  -DQVR_OPENXR_LIBRARY=/opt/steamvr/bin/linuxarm64/libopenxr_loader.so
cmake --build build-frame -j4
```

`QVR_OPENXR_LIBRARY` names SteamVR's loader: the Frame's ordinary library search found a dummy static library. Without
the OpenXR headers or a loader, CMake says so and builds the mock backend alone (as on macOS). `ldd build-frame/ironwail`
checks the libraries on the device.

## Running

```sh
SDL_VIDEODRIVER=x11 XR_RUNTIME_JSON=/opt/steamvr/steamxr_linuxarm64.json \
  ./ironwail -window -vr-hide-mirror -basedir . -game quakevr -nobindless +vr_mirror 0
```

The Frame's GPU (Adreno, Mesa's Turnip driver) needed more than this in kblood's tests; his launcher sets them, and they
are not part of this branch:

- `TU_DEBUG=sysmem`: the world shader's decal loop hung the GPU in GMEM rendering (a kill could hang the headset);
  sysmem rendering never did. The root cause is not known.
- `+vr_decals_world 0`: the world's decals as meshes, so that shader loop never runs.
- `MESA_VK_ABORT_ON_DEVICE_LOSS=1`: stop at once on a lost GPU.
- A first run with `+vr_graphics_preset 0 +vr_render_scale 0.45 +vid_fsaa 0`.

## Not in this branch

PR #75's other parts stay on the `staging/frame` reference branch, for later branches of their own: the user presence
pause and the calibration changes (`XR_EXT_user_presence`), frame synthesis and the GL/Vulkan bridge, the native Vulkan
BSP component, the profiler changes, the bench kill builtins, `vr_motion_play`'s `xr` and `kill` options, the Frame
launchers, installer and packaging (`Misc/frame/`), and the reference apps.
