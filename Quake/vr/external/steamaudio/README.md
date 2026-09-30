# Steam Audio (vendored, Windows x64)

Valve's Steam Audio SDK: HRTF binaural rendering, occlusion and transmission, reflections and reverb simulated from a
scene. Apache License 2.0 (`LICENSE`, the licence's text); the library bundles third-party code under their own
licences (`THIRDPARTY.md`, from the SDK: Intel IPP, FFTS, PFFFT, MySOFA, Embree, the CIPIC HRTF database, Google's
spherical harmonics library and others).

- Source: https://github.com/ValveSoftware/steam-audio/releases/tag/v4.8.1, `steamaudio_4.8.1.zip` (published
  2026-02-11; SHA-256 of the zip `4a0aa5ec1176f38f0b0993a37c2259d9e86f27e22d5e24f83ec4c3cb9a1d5449`).
- Version: 4.8.1 (`phonon_version.h`).

Kept from the SDK, unchanged (git stores the text files with LF line endings):

| File | Size | What |
|---|---:|---|
| `include/phonon.h`, `phonon_version.h`, `phonon_interfaces.h` | 287 KB | the C API |
| `lib/windows-x64/phonon.dll` | 52.9 MB | the library (SHA-256 `ca3dbc01dbc24492717011e80f6a51404ca143ae344ca660971d2c983f1e058d`) |
| `THIRDPARTY.md` | 36 KB | the bundled code's licences |

Left out: the import library (`phonon.lib`: the game loads the DLL at run time, it links nothing), the Linux
(`libphonon.so`, 40 MB), macOS, Android and iOS libraries, AMD's `TrueAudioNext.dll` and `GPUUtilities.dll` (GPU
convolution, unused), the debug symbols, the docs and the Unity/Unreal/FMOD/Wwise plugins. A Linux build finds
`libphonon.so` next to the executable if one is put there (from the same release), and runs without it otherwise.

Quake VR uses it in `Quake/vr/vr_steamaudio.cpp` (the loader: `SDL_LoadObject`, every function looked up by name;
without the DLL, or with a version that refuses the context, the game keeps Quake's own panning),
`vr_audio.cpp` (the voices: binaural and direct effects, the reverb) and `vr_audiosim.cpp` (the scene from the map,
the simulations on the game's thread pool). See `docs/vr-port/ROUND21.md`, "Spatial audio (Steam Audio)". The
Windows build copies `phonon.dll` next to `ironwail.exe` (`Windows/VisualStudio/quakevr.props`); a release ships it
there with `LICENSE` and `THIRDPARTY.md`.
