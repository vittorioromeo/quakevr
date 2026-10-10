# doomgeneric in Quake VR

[ozkl/doomgeneric](https://github.com/ozkl/doomgeneric) at commit `dcb7a8db` (2026-04-12), GPL-2 (`LICENSE`): Doom
(Chocolate Doom's code) with its platform layer cut down to a few functions. Built into `qvr_doom.dll`
(`Windows/VisualStudio/qvr_doom.vcxproj`), the wrist gadget's Doom (docs/vr-port/DOOM.md), never linked into the
engine. Its platform functions, sound module and the exported interface are Quake VR's:
`Quake/vr/doom/qvr_doom_dll.c`, `qvr_doom_api.h`, `qvr_doom_force.h` (included first in every file: Doom's heap a
private one, its text to the engine's console, `exit()` an error back to the engine).

Upstream's files are here unchanged but for the platform backends left out (`doomgeneric_*.c` but `doomgeneric.c`,
`i_sdlsound.c`, `i_sdlmusic.c`, `i_allegro*.c`) and these patches, each under `#ifdef QVR_DOOM` and marked `QVR`:

- `i_system.c`: `I_Error` and `I_Quit` return to the engine (`QVR_DoomFatal`, `QVR_DoomQuit`: a longjmp to the
  exported call that ran Doom) instead of a message box and `exit()`.
- `d_main.c`: the screen melt runs a step a frame (the engine draws its own frames meanwhile) instead of a loop sleeping
  until it ends, the game's tics waiting for it as vanilla's loop made them; no ENDOOM screen (it `exit()`s).
- `m_config.c`: the settings (`default.cfg`) in the folder the engine gives (`quakevr/doom/`), the saves under it in
  `savegame/<iwad>/`.
- `m_menu.c`: an empty save slot's name is the level's (no keyboard in the headset: Enter saves at once);
  `M_QvrMenuTap` (a tap on a menu item selects it: the touchscreen) and `M_QvrSaveStringEnter`.
- `i_sound.c`: no `SDL_mixer.h` (the engine plays the sounds).
