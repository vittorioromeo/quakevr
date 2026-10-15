/* qvr_doom_api.h -- the C interface between Quake VR (vr_doom.cpp) and qvr_doom.dll, Doom built from the vendored
   doomgeneric (Quake/vr/external/doomgeneric, GPL-2) with its own globals: loaded on first use, never linked into the
   engine, so that Doom's names (Z_Malloc, I_Error, M_Init, ...) never meet Quake's. Only the functions below are exported.

   Doom's clock is the engine's: qvr_doom_tick(ms) advances it and runs the tics that fall due (35 a second), so Doom
   pauses whenever the engine stops ticking it. Its screen is 320 x 200 RGBA (qvr_doom_frame). Input is Doom's key codes
   (QVR_DOOM_KEY_*, doomkeys.h's, or lower-case ASCII). Sound effects go back to the engine through the host's
   callbacks (each a DMX lump: the engine plays it through its own mixer, Steam Audio included, at the gadget); music
   is not played (no music module yet).

   A fatal error inside Doom (I_Error) or its own Quit returns to the engine: qvr_doom_tick returns QVR_DOOM_ERROR or
   QVR_DOOM_QUIT, qvr_doom_error() says why; the engine then calls qvr_doom_shutdown and unloads the DLL (all of Doom's
   memory is in a private heap, freed then; a new start loads it afresh). Single-threaded: call it from one thread. */

#ifndef QVR_DOOM_API_H
#define QVR_DOOM_API_H

#ifdef __cplusplus
extern "C" {
#endif

#define QVR_DOOM_API_VERSION 1

#define QVR_DOOM_WIDTH 320
#define QVR_DOOM_HEIGHT 200

/* qvr_doom_tick's results. */
#define QVR_DOOM_RUNNING 0
#define QVR_DOOM_QUIT 1  /* Doom's own Quit (its menu): its settings saved */
#define QVR_DOOM_ERROR 2 /* I_Error: qvr_doom_error() */

/* Doom's key codes (doomkeys.h). Letters and digits are their lower-case ASCII. */
#define QVR_DOOM_KEY_RIGHTARROW 0xae
#define QVR_DOOM_KEY_LEFTARROW 0xac
#define QVR_DOOM_KEY_UPARROW 0xad
#define QVR_DOOM_KEY_DOWNARROW 0xaf
#define QVR_DOOM_KEY_STRAFE_L 0xa0
#define QVR_DOOM_KEY_STRAFE_R 0xa1
#define QVR_DOOM_KEY_USE 0xa2
#define QVR_DOOM_KEY_FIRE 0xa3
#define QVR_DOOM_KEY_ESCAPE 27
#define QVR_DOOM_KEY_ENTER 13
#define QVR_DOOM_KEY_TAB 9
#define QVR_DOOM_KEY_BACKSPACE 0x7f
#define QVR_DOOM_KEY_RSHIFT (0x80 + 0x36)
/* Bound by the DLL to the next and previous weapon (key_nextweapon, key_prevweapon). */
#define QVR_DOOM_KEY_NEXTWEAPON ']'
#define QVR_DOOM_KEY_PREVWEAPON '['

/* The engine's side. Every callback may be null (then nothing happens). */
typedef struct qvr_doom_host
{
    int version; /* QVR_DOOM_API_VERSION */
    void* user;
    /* A line of Doom's text output (its startup banner, warnings), without the newline. */
    void (*print)(void* user, const char* line);
    /* A sound effect on one of Doom's channels (0 .. 7: a new one on a channel cuts the one before), `lump` its name
       (DSPISTOL), `dmx` the lump (format 3: rate, sample count, 16 bytes of padding, 8-bit unsigned samples, padding),
       `volume` 0..127, `separation` 0 (left) .. 128 (centre) .. 255 (right). */
    void (*sound_start)(void* user, int channel, const char* lump, const void* dmx, int dmx_length, int volume,
        int separation);
    void (*sound_stop)(void* user, int channel);
    int (*sound_playing)(void* user, int channel);
    void (*sound_update)(void* user, int channel, int volume, int separation);
} qvr_doom_host;

/* What Doom is doing, for the engine's controls and the tests. */
typedef struct qvr_doom_status
{
    int menu_active;     /* Doom's menu is up */
    int message_input;   /* and asks a yes/no question ("y" answers) */
    int save_name;       /* and waits for a save's name (Enter saves it; one is filled in) */
    int automap;         /* the automap is up */
    int game_state;      /* 0 level, 1 intermission, 2 finale, 3 demo screen (Doom's gamestate_t) */
    int in_level;        /* a level is being played (not a demo) */
    int episode, map;    /* Doom's gameepisode, gamemap */
    int health, armor;   /* the player's */
    int ready_weapon;    /* 0 fist .. 8 super shotgun (weapontype_t) */
    int ammo[4];         /* clip, shells, cells, rockets */
    int x, y, angle;     /* the player's position (map units) and facing (degrees) */
    int kills, items, secrets;
    int tics;            /* game tics run so far (gametic) */
    int frames;          /* frames drawn so far */
    int sounds;          /* sound effects started so far */
} qvr_doom_status;

#if defined(_WIN32) && defined(QVR_DOOM_BUILD)
#define QVR_DOOM_EXPORT __declspec(dllexport)
#else
#define QVR_DOOM_EXPORT
#endif

/* The interface's version (QVR_DOOM_API_VERSION). */
QVR_DOOM_EXPORT int qvr_doom_version(void);

/* Starts Doom: `iwad` the IWAD's path, `data_dir` where its settings (default.cfg) and saves (savegames/<iwad>/) go
   (made if missing; ending in a slash), `args` more of Doom's command line (-file pwad.wad, -warp 1 1, -skill 3; may
   be null). 0 when it started; else Doom failed (qvr_doom_error()) and only qvr_doom_shutdown may follow. */
QVR_DOOM_EXPORT int qvr_doom_init(const qvr_doom_host* host, const char* iwad, const char* data_dir, int argc,
    const char* const* args);

/* Doom's clock moved on by `ms` (the engine's frame; at most 250 counted): the tics that fell due run and the screen
   is drawn. QVR_DOOM_RUNNING, QVR_DOOM_QUIT or QVR_DOOM_ERROR. */
QVR_DOOM_EXPORT int qvr_doom_tick(unsigned ms);

/* A key pressed (1) or released (0), taken at the next tic. */
QVR_DOOM_EXPORT void qvr_doom_key(int pressed, int key);

/* The screen as last drawn: QVR_DOOM_WIDTH x QVR_DOOM_HEIGHT RGBA (rows top first, alpha 255); `frame` (may be null):
   how many frames were drawn, to tell a new one. */
QVR_DOOM_EXPORT const unsigned char* qvr_doom_frame(unsigned* frame);

QVR_DOOM_EXPORT void qvr_doom_get_status(qvr_doom_status* out);

/* Sets one of Doom's settings by its default.cfg name (sfx_volume, screenblocks, joyb_speed, usegamma, ...): 1 when it
   is one. */
QVR_DOOM_EXPORT int qvr_doom_set(const char* name, const char* value);

/* A cheat or command typed into the game, as on the keyboard (iddqd, idclev11): each character a key press. */
QVR_DOOM_EXPORT void qvr_doom_type(const char* text);

/* A tap on the screen (x 0..319, y 0..199) while Doom's menu is up: the skull moved to the item tapped; returns the
   key that does what the tap means (Enter, or an arrow on a slider's row; Escape ends a message), 0 for none. */
QVR_DOOM_EXPORT int qvr_doom_menu_tap(int x, int y);

/* Why Doom stopped (I_Error's message), or "". */
QVR_DOOM_EXPORT const char* qvr_doom_error(void);

/* Saves Doom's settings (unless it failed) and frees everything it holds (its memory, its open files): the DLL may
   then be unloaded. */
QVR_DOOM_EXPORT void qvr_doom_shutdown(void);

typedef int (*qvr_doom_version_fn)(void);
typedef int (*qvr_doom_init_fn)(const qvr_doom_host*, const char*, const char*, int, const char* const*);
typedef int (*qvr_doom_tick_fn)(unsigned);
typedef void (*qvr_doom_key_fn)(int, int);
typedef const unsigned char* (*qvr_doom_frame_fn)(unsigned*);
typedef void (*qvr_doom_get_status_fn)(qvr_doom_status*);
typedef int (*qvr_doom_set_fn)(const char*, const char*);
typedef void (*qvr_doom_type_fn)(const char*);
typedef int (*qvr_doom_menu_tap_fn)(int, int);
typedef const char* (*qvr_doom_error_fn)(void);
typedef void (*qvr_doom_shutdown_fn)(void);

#ifdef __cplusplus
}
#endif

#endif
