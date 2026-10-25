/* qvr_doom_dll.c -- qvr_doom.dll's side of qvr_doom_api.h: doomgeneric's platform functions (DG_*), a sound module
   handing Doom's sound effects to the engine, a silent music module, and the exported functions. Built with
   qvr_doom_force.h included first in every file: Doom's malloc and friends use a private heap (freed whole at
   qvr_doom_shutdown) and its printf goes to the engine's console. Doom's exit() points (I_Error, I_Quit) longjmp back
   here (the QVR_DOOM patches in the vendored sources: Quake/vr/external/doomgeneric/README.QVR.md). */

#include "qvr_doom_api.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_player.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "w_wad.h"
#include "z_zone.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ---- State (file scope: one Doom per process) ------------------------------------------------------------------- */

char* qvr_doom_data_dir = NULL; /* m_config.c (GetDefaultConfigDir) */

static HANDLE heap = NULL;          /* Doom's memory (qvr_doom_force.h) */
static qvr_doom_host host;          /* the engine's callbacks */
static jmp_buf escape;              /* where I_Error and I_Quit return to */
static int escapeSet = 0;
static int stopped = QVR_DOOM_RUNNING; /* how Doom last left (QVR_DOOM_QUIT, QVR_DOOM_ERROR) */
static char errorText[512] = "";
static unsigned clockMs = 0;        /* Doom's clock (DG_GetTicksMs): the engine's frames added up */
static int lastTic = -1;            /* the tic (35 a second) Doom last ran at */
static unsigned char frameRgba[QVR_DOOM_WIDTH * QVR_DOOM_HEIGHT * 4];
static unsigned frameCount = 0;
static int soundsStarted = 0;
static char printLine[1024];
static int printLength = 0;

#define KEY_QUEUE 64
static unsigned short keyQueue[KEY_QUEUE]; /* (pressed << 8) | key */
static int keyRead = 0, keyWrite = 0;

static char* argStore[64];
static int argCount = 0;

extern boolean menuactive;
extern boolean messageNeedsInput;
extern int messageToPrint;
extern boolean automapactive;
extern int gametic;
int M_QvrMenuTap(int x, int y);  /* m_menu.c (QVR) */
int M_QvrSaveStringEnter(void);  /* m_menu.c (QVR) */

/* ---- Memory and text (qvr_doom_force.h) ---------------------------------------------------------------------------- */

static HANDLE doomHeap(void)
{
    if (!heap)
    {
        heap = HeapCreate(0, 0, 0);
    }
    return heap;
}

void* qvr_doom_malloc(size_t size)
{
    return HeapAlloc(doomHeap(), 0, size ? size : 1);
}

void* qvr_doom_calloc(size_t count, size_t size)
{
    if (size && count > (size_t)-1 / size)
    {
        return NULL; // count * size would wrap
    }
    return HeapAlloc(doomHeap(), HEAP_ZERO_MEMORY, count * size ? count * size : 1);
}

void* qvr_doom_realloc(void* p, size_t size)
{
    if (!p)
    {
        return qvr_doom_malloc(size);
    }
    return HeapReAlloc(doomHeap(), 0, p, size ? size : 1);
}

void qvr_doom_free(void* p)
{
    if (p)
    {
        HeapFree(doomHeap(), 0, p);
    }
}

char* qvr_doom_strdup(const char* s)
{
    const size_t n = strlen(s) + 1;
    char* d = (char*)qvr_doom_malloc(n);
    if (d)
    {
        memcpy(d, s, n);
    }
    return d;
}

/* Doom's text, a line at a time, to the engine. */
static void flushLine(void)
{
    printLine[printLength] = 0;
    if (host.print && printLength > 0)
    {
        host.print(host.user, printLine);
    }
    printLength = 0;
}

static void emitText(const char* text)
{
    for (; *text; text++)
    {
        if (*text == '\n')
        {
            flushLine();
        }
        else if (*text != '\r' && printLength < (int)sizeof(printLine) - 1)
        {
            printLine[printLength++] = *text;
        }
    }
}

int qvr_doom_printf(const char* format, ...)
{
    char text[1024];
    va_list args;
    int n;
    va_start(args, format);
    n = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    emitText(text);
    return n;
}

int qvr_doom_vfprintf(FILE* stream, const char* format, va_list args)
{
    char text[1024];
    int n;
    if (stream != stdout && stream != stderr)
    {
        return vfprintf(stream, format, args);
    }
    n = vsnprintf(text, sizeof(text), format, args);
    emitText(text);
    return n;
}

int qvr_doom_fprintf(FILE* stream, const char* format, ...)
{
    va_list args;
    int n;
    va_start(args, format);
    n = qvr_doom_vfprintf(stream, format, args);
    va_end(args);
    return n;
}

int qvr_doom_puts(const char* text)
{
    emitText(text);
    emitText("\n");
    return 0;
}

int qvr_doom_putchar(int c)
{
    const char text[2] = {(char)c, 0};
    emitText(text);
    return c;
}

/* ---- Leaving Doom (the QVR_DOOM patches in i_system.c) -------------------------------------------------------------- */

void QVR_DoomQuit(void)
{
    stopped = QVR_DOOM_QUIT;
    if (escapeSet)
    {
        longjmp(escape, 1);
    }
}

void QVR_DoomFatal(const char* message)
{
    if (stopped != QVR_DOOM_ERROR)
    {
        M_StringCopy(errorText, message, sizeof(errorText));
    }
    stopped = QVR_DOOM_ERROR;
    flushLine();
    if (escapeSet)
    {
        longjmp(escape, 1);
    }
    /* (no way back: should never happen, every entry point sets the escape) */
    TerminateProcess(GetCurrentProcess(), 3);
}

/* ---- doomgeneric's platform ------------------------------------------------------------------------------------- */

void DG_Init(void)
{
}

/* The screen: Doom's 32-bit pixels (cmap_to_fb: 0x00RRGGBB) as RGBA. */
void DG_DrawFrame(void)
{
    const uint32_t* in = (const uint32_t*)DG_ScreenBuffer;
    unsigned char* out = frameRgba;
    int i;
    for (i = 0; i < QVR_DOOM_WIDTH * QVR_DOOM_HEIGHT; i++, out += 4)
    {
        const uint32_t p = in[i];
        out[0] = (unsigned char)(p >> 16);
        out[1] = (unsigned char)(p >> 8);
        out[2] = (unsigned char)p;
        out[3] = 255;
    }
    frameCount++;
}

/* Doom never waits on its own: a sleep moves its clock on (its loops that wait for a tic end at once). */
void DG_SleepMs(uint32_t ms)
{
    clockMs += ms;
}

uint32_t DG_GetTicksMs(void)
{
    return clockMs;
}

int DG_GetKey(int* pressed, unsigned char* key)
{
    if (keyRead == keyWrite)
    {
        return 0;
    }
    *pressed = keyQueue[keyRead] >> 8;
    *key = (unsigned char)(keyQueue[keyRead] & 0xff);
    keyRead = (keyRead + 1) % KEY_QUEUE;
    return 1;
}

void DG_SetWindowTitle(const char* title)
{
    (void)title;
}

/* ---- Sound: Doom's effects to the engine ------------------------------------------------------------------------ */

/* i_sound.c binds these settings (the SDL module's): unused here. */
int use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;

static snddevice_t soundDevices[] = {SNDDEVICE_SB, SNDDEVICE_PAS, SNDDEVICE_GUS, SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS, SNDDEVICE_AWE32};
static boolean soundPrefix = true;

static boolean sndInit(boolean useSfxPrefix)
{
    soundPrefix = useSfxPrefix;
    return true;
}

static void sndShutdown(void)
{
    int c;
    for (c = 0; c < 16; c++)
    {
        if (host.sound_stop)
        {
            host.sound_stop(host.user, c);
        }
    }
}

static int sndLumpNum(sfxinfo_t* sfx)
{
    char name[16];
    if (sfx->link)
    {
        sfx = sfx->link;
    }
    M_snprintf(name, sizeof(name), soundPrefix ? "ds%s" : "%s", sfx->name);
    return W_CheckNumForName(name);
}

static void sndUpdate(void)
{
}

static void sndUpdateParams(int channel, int vol, int sep)
{
    if (host.sound_update)
    {
        host.sound_update(host.user, channel, vol, sep);
    }
}

static int sndStart(sfxinfo_t* sfx, int channel, int vol, int sep)
{
    const int lump = sfx->lumpnum;
    if (lump < 0 || lump >= (int)numlumps)
    {
        return -1;
    }
    soundsStarted++;
    if (host.sound_start)
    {
        const void* data = W_CacheLumpNum(lump, PU_STATIC);
        host.sound_start(host.user, channel, lumpinfo[lump].name, data, W_LumpLength(lump), vol, sep);
        W_ReleaseLumpNum(lump);
    }
    return channel;
}

static void sndStop(int channel)
{
    if (host.sound_stop)
    {
        host.sound_stop(host.user, channel);
    }
}

static boolean sndPlaying(int channel)
{
    return host.sound_playing ? host.sound_playing(host.user, channel) != 0 : false;
}

static void sndCache(sfxinfo_t* sounds, int count)
{
    (void)sounds;
    (void)count;
}

sound_module_t DG_sound_module = {soundDevices, sizeof(soundDevices) / sizeof(*soundDevices), sndInit, sndShutdown,
    sndLumpNum, sndUpdate, sndUpdateParams, sndStart, sndStop, sndPlaying, sndCache};

/* No music yet (Doom's MUS through an OPL emulator: a later step). */
static boolean musInit(void)
{
    return true;
}
static void musShutdown(void)
{
}
static void musVolume(int volume)
{
    (void)volume;
}
static void musNothing(void)
{
}
static void* musRegister(void* data, int len)
{
    (void)data;
    (void)len;
    return NULL;
}
static void musUnregister(void* handle)
{
    (void)handle;
}
static void musPlay(void* handle, boolean looping)
{
    (void)handle;
    (void)looping;
}
static boolean musPlaying(void)
{
    return false;
}

music_module_t DG_music_module = {soundDevices, sizeof(soundDevices) / sizeof(*soundDevices), musInit, musShutdown,
    musVolume, musNothing, musNothing, musRegister, musUnregister, musPlay, musNothing, musPlaying, musNothing};

/* ---- Exports ---------------------------------------------------------------------------------------------------- */

QVR_DOOM_EXPORT int qvr_doom_version(void)
{
    return QVR_DOOM_API_VERSION;
}

static void addArg(const char* a)
{
    if (argCount < (int)(sizeof(argStore) / sizeof(*argStore)) - 1)
    {
        argStore[argCount++] = qvr_doom_strdup(a);
    }
}

QVR_DOOM_EXPORT int qvr_doom_init(const qvr_doom_host* h, const char* iwad, const char* data_dir, int argc,
    const char* const* args)
{
    int i;
    memset(&host, 0, sizeof(host));
    if (h && h->version == QVR_DOOM_API_VERSION)
    {
        host = *h;
    }
    qvr_doom_data_dir = qvr_doom_strdup(data_dir);
    CreateDirectoryA(data_dir, NULL);

    argCount = 0;
    addArg("qvr_doom");
    addArg("-iwad");
    addArg(iwad);
    addArg("-mb");
    addArg("32");
    addArg("-nogui");
    for (i = 0; i < argc && args; i++)
    {
        addArg(args[i]);
    }
    argStore[argCount] = NULL;

    stopped = QVR_DOOM_RUNNING;
    clockMs = 0;
    lastTic = -1;
    if (setjmp(escape))
    {
        escapeSet = 0;
        return stopped == QVR_DOOM_RUNNING ? QVR_DOOM_ERROR : stopped;
    }
    escapeSet = 1;
    doomgeneric_Create(argCount, argStore);

    /* The engine's controls send these keys whatever default.cfg says (it can't keep doomgeneric's own codes over 127:
       they would load as none), and the next and previous weapon on keys of their own. */
    key_right = KEY_RIGHTARROW;
    key_left = KEY_LEFTARROW;
    key_up = KEY_UPARROW;
    key_down = KEY_DOWNARROW;
    key_strafeleft = KEY_STRAFE_L;
    key_straferight = KEY_STRAFE_R;
    key_fire = KEY_FIRE;
    key_use = KEY_USE;
    key_map_toggle = KEY_TAB;
    key_menu_activate = KEY_ESCAPE;
    key_menu_forward = KEY_ENTER;
    key_menu_confirm = 'y';
    key_menu_abort = 'n';
    key_menu_back = KEY_BACKSPACE;
    key_menu_up = KEY_UPARROW;
    key_menu_down = KEY_DOWNARROW;
    key_menu_left = KEY_LEFTARROW;
    key_menu_right = KEY_RIGHTARROW;
    key_nextweapon = QVR_DOOM_KEY_NEXTWEAPON;
    key_prevweapon = QVR_DOOM_KEY_PREVWEAPON;
    escapeSet = 0;
    flushLine();
    return 0;
}

QVR_DOOM_EXPORT int qvr_doom_tick(unsigned ms)
{
    int tic;
    if (stopped != QVR_DOOM_RUNNING)
    {
        return stopped;
    }
    clockMs += ms > 250 ? 250 : ms;
    tic = (int)(((unsigned long long)clockMs * 35) / 1000);
    if (tic == lastTic)
    {
        return QVR_DOOM_RUNNING;
    }
    lastTic = tic;
    if (setjmp(escape))
    {
        escapeSet = 0;
        flushLine();
        return stopped;
    }
    escapeSet = 1;
    doomgeneric_Tick();
    escapeSet = 0;
    return stopped;
}

QVR_DOOM_EXPORT void qvr_doom_key(int pressed, int key)
{
    const int next = (keyWrite + 1) % KEY_QUEUE;
    if (next == keyRead || key <= 0 || key > 255)
    {
        return; /* (full: dropped) */
    }
    keyQueue[keyWrite] = (unsigned short)(((pressed ? 1 : 0) << 8) | key);
    keyWrite = next;
}

QVR_DOOM_EXPORT const unsigned char* qvr_doom_frame(unsigned* frame)
{
    if (frame)
    {
        *frame = frameCount;
    }
    return frameRgba;
}

QVR_DOOM_EXPORT void qvr_doom_get_status(qvr_doom_status* out)
{
    const player_t* p = &players[consoleplayer];
    int i;
    memset(out, 0, sizeof(*out));
    out->menu_active = menuactive ? 1 : 0;
    out->message_input = messageToPrint && messageNeedsInput ? 1 : 0;
    out->save_name = M_QvrSaveStringEnter();
    out->automap = automapactive ? 1 : 0;
    out->game_state = (int)gamestate;
    out->in_level = gamestate == GS_LEVEL && usergame && !demoplayback;
    out->episode = gameepisode;
    out->map = gamemap;
    out->health = p->health;
    out->armor = p->armorpoints;
    out->ready_weapon = (int)p->readyweapon;
    for (i = 0; i < 4; i++)
    {
        out->ammo[i] = p->ammo[i];
    }
    if (p->mo)
    {
        out->x = p->mo->x >> FRACBITS;
        out->y = p->mo->y >> FRACBITS;
        out->angle = (int)(((unsigned long long)p->mo->angle * 360) >> 32);
    }
    out->kills = p->killcount;
    out->items = p->itemcount;
    out->secrets = p->secretcount;
    out->tics = gametic;
    out->frames = (int)frameCount;
    out->sounds = soundsStarted;
}

QVR_DOOM_EXPORT int qvr_doom_set(const char* name, const char* value)
{
    return M_SetVariable((char*)name, (char*)value) ? 1 : 0;
}

QVR_DOOM_EXPORT void qvr_doom_type(const char* text)
{
    for (; *text; text++)
    {
        qvr_doom_key(1, (unsigned char)*text);
        qvr_doom_key(0, (unsigned char)*text);
    }
}

QVR_DOOM_EXPORT int qvr_doom_menu_tap(int x, int y)
{
    return M_QvrMenuTap(x, y);
}

QVR_DOOM_EXPORT const char* qvr_doom_error(void)
{
    return errorText;
}

QVR_DOOM_EXPORT void qvr_doom_shutdown(void)
{
    /* Its settings saved and its own exit functions run (I_Quit: they end in QVR_DoomQuit), unless it failed. */
    if (stopped == QVR_DOOM_RUNNING)
    {
        if (!setjmp(escape))
        {
            escapeSet = 1;
            I_Quit();
        }
        escapeSet = 0;
    }
    flushLine();
    _fcloseall(); /* (its own C runtime's: the WADs, a demo, its config) */
    if (heap)
    {
        HeapDestroy(heap);
        heap = NULL;
    }
    qvr_doom_data_dir = NULL;
    argCount = 0;
    stopped = QVR_DOOM_QUIT;
}
