// vr_voicenotes.cpp -- see vr_voicenotes.hpp.

#include "vr_voicenotes.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_protocol.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace qvr::voicenotes
{
namespace
{

constexpr int wantedRate = 16000;      // Whisper's own rate
constexpr double maxSeconds = 180.0;   // a note longer than this is cut there
constexpr float mouthReach = 0.30f;    // metres from the mouth that count as "at the mouth"
constexpr double minSeconds = 0.8;     // a shorter note is taken for an accidental press, and dropped

SDL_AudioDeviceID device = 0;
int rate = wantedRate;
bool audioInit = false;
bool recording = false;
bool byButton = false;
double startTime = 0.0;
std::vector<std::int16_t> samples;
std::string baseName;                 // notes/<map>_<date>_<time>, without the extension

[[nodiscard]] bool ensureAudio()
{
    if(!audioInit)
    {
        if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        {
            Con_Printf("VR notes: no audio (%s)\n", SDL_GetError());
            return false;
        }
        audioInit = true;
    }
    return true;
}

// The capture device vr_note_device names (a part of its name), or null for the default.
[[nodiscard]] const char* deviceName()
{
    const char* wanted = vr_note_device.string;
    if(!wanted || !wanted[0])
    {
        return nullptr;
    }
    const int count = SDL_GetNumAudioDevices(1);
    for(int i = 0; i < count; i++)
    {
        const char* name = SDL_GetAudioDeviceName(i, 1);
        if(name && q_strcasestr(name, wanted))
        {
            return name;
        }
    }
    Con_Printf("VR notes: no microphone named like \"%s\"; using the default (vr_note_devices lists them)\n", wanted);
    return nullptr;
}

void haptic(float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value)
    {
        be->haptic(HAND_OFF, seconds, 120.f, amplitude);
    }
}

void writeLE(FILE* f, std::uint32_t v, int bytes)
{
    for(int i = 0; i < bytes; i++)
    {
        fputc(static_cast<int>((v >> (8 * i)) & 0xff), f);
    }
}

[[nodiscard]] bool writeWav(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "wb");
    if(!f)
    {
        return false;
    }
    const std::uint32_t bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
    fwrite("RIFF", 1, 4, f);
    writeLE(f, 36 + bytes, 4);
    fwrite("WAVEfmt ", 1, 8, f);
    writeLE(f, 16, 4);
    writeLE(f, 1, 2); // PCM
    writeLE(f, 1, 2); // mono
    writeLE(f, static_cast<std::uint32_t>(rate), 4);
    writeLE(f, static_cast<std::uint32_t>(rate * 2), 4);
    writeLE(f, 2, 2);
    writeLE(f, 16, 2);
    fwrite("data", 1, 4, f);
    writeLE(f, bytes, 4);
    fwrite(samples.data(), 1, bytes, f);
    fclose(f);
    return true;
}

[[nodiscard]] const char* modelName(int index)
{
    return index > 0 && index < MAX_MODELS && cl.model_precache[index] ? cl.model_precache[index]->name : "-";
}

// The moment's context, as the note starts.
void writeContext(const std::string& path, const std::string& when)
{
    FILE* f = fopen(path.c_str(), "w");
    if(!f)
    {
        return;
    }
    const entity_t& player = cl_entities[cl.viewentity];
    const hands::State& s = hands::current();
    fprintf(f, "time: %s\n", when.c_str());
    fprintf(f, "map: %s (%s)\n", cl.mapname, cl.levelname);
    fprintf(f, "game time: %.1f s\n", cl.time);
    fprintf(f, "position: %.0f %.0f %.0f\n", player.origin[0], player.origin[1], player.origin[2]);
    fprintf(f, "view: pitch %.0f yaw %.0f\n", s.headAngles.x, s.headAngles.y);
    fprintf(f, "health: %d  armour: %d  ammo: %d\n", cl.stats[STAT_HEALTH], cl.stats[STAT_ARMOR], cl.stats[STAT_AMMO]);
    fprintf(f, "main hand: weapon %d, %s\n", cl.stats[protocol::STAT_QVR_WEAPON], modelName(cl.stats[STAT_WEAPON]));
    fprintf(f, "off hand: weapon %d, %s\n", cl.stats[protocol::STAT_QVR_WEAPON2],
        modelName(cl.stats[protocol::STAT_QVR_WEAPONMODEL2]));
    fprintf(f, "skill: %.0f  graphics preset: %s  vr: %s\n", Cvar_VariableValue("skill"), vr_graphics_preset.string,
        vrActive() ? "on" : "off");
    fclose(f);
}

void start(bool fromButton)
{
    if(recording || !ensureAudio())
    {
        return;
    }

    SDL_AudioSpec want{}, have{};
    want.freq = wantedRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    const char* name = deviceName();
    device = SDL_OpenAudioDevice(name, 1, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if(!device)
    {
        Con_Printf("VR notes: can't open the microphone (%s)\n", SDL_GetError());
        return;
    }
    rate = have.freq;
    samples.clear();
    SDL_PauseAudioDevice(device, 0);

    // notes/<map>_<date>_<time>, as Ironwail names screenshots.
    const std::time_t now = std::time(nullptr);
    char stamp[64];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
    const std::string dir = std::string{com_gamedir} + "/notes";
    Sys_mkdir(dir.c_str());
    baseName = dir + "/" + (cl.mapname[0] ? cl.mapname : "menu") + "_" + stamp;
    char when[64];
    std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    writeContext(baseName + ".txt", when);

    recording = true;
    byButton = fromButton;
    startTime = realtime;
    Cbuf_AddText("screenshot\n");
    haptic(0.08f, 0.6f);
    Con_Printf("VR notes: recording from %s\n", name ? name : "the default microphone");
}

void take()
{
    if(!device)
    {
        return;
    }
    std::int16_t buffer[4096];
    for(Uint32 got; (got = SDL_DequeueAudio(device, buffer, sizeof(buffer))) > 0;)
    {
        samples.insert(samples.end(), buffer, buffer + got / sizeof(std::int16_t));
    }
}

void stop()
{
    if(!recording)
    {
        return;
    }
    take();
    SDL_CloseAudioDevice(device);
    device = 0;
    recording = false;

    const double seconds = static_cast<double>(samples.size()) / rate;
    if(seconds < minSeconds)
    {
        remove((baseName + ".txt").c_str());
        Con_Printf("VR notes: too short, dropped\n");
        haptic(0.03f, 0.3f);
        return;
    }
    if(writeWav(baseName + ".wav"))
    {
        Con_Printf("VR notes: saved %s.wav (%.1f s)\n", baseName.c_str(), seconds);
    }
    else
    {
        Con_Printf("VR notes: can't write %s.wav\n", baseName.c_str());
    }
    haptic(0.05f, 0.5f);
    samples.clear();
}

// Whether the off hand is at the mouth: within reach of a point a little below and in front of
// the eyes.
[[nodiscard]] bool offhandAtMouth()
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return false;
    }
    const float m2u = units::metresToUnits();
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
    const glm::vec3 mouth = s.head + fwd * (0.06f * m2u) - glm::vec3{0.f, 0.f, 0.09f * m2u};
    return glm::distance(s.pos[HAND_OFF], mouth) < mouthReach * m2u;
}

void noteDown_f()
{
    start(false);
}

void noteUp_f()
{
    if(!byButton)
    {
        stop();
    }
}

void devices_f()
{
    if(!ensureAudio())
    {
        return;
    }
    const int count = SDL_GetNumAudioDevices(1);
    Con_Printf("%d microphone%s:\n", count, count == 1 ? "" : "s");
    for(int i = 0; i < count; i++)
    {
        Con_Printf("  %s\n", SDL_GetAudioDeviceName(i, 1));
    }
    Con_Printf("vr_note_device \"<part of a name>\" picks one (empty: the default)\n");
}

} // namespace

void init()
{
    Cmd_AddCommand("+vr_note", noteDown_f);
    Cmd_AddCommand("-vr_note", noteUp_f);
    Cmd_AddCommand("vr_note_devices", devices_f);
}

void frame()
{
    if(!recording)
    {
        return;
    }
    take();
    const double seconds = realtime - startTime;
    if(seconds > maxSeconds)
    {
        stop();
        return;
    }

    // "REC 0:12", small and low in the view (the hand is at the mouth, right before the eyes).
    const hands::State& s = hands::current();
    if(s.valid)
    {
        char text[32];
        q_snprintf(text, sizeof(text), "REC %d:%02d", static_cast<int>(seconds) / 60, static_cast<int>(seconds) % 60);
        const float m2u = units::metresToUnits();
        const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
        const glm::vec3 at = s.head + fwd * (0.7f * m2u) - glm::vec3{0.f, 0.f, 0.28f * m2u};
        text3d::queue(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, text3d::Align::Centre, 0.07f);
    }
}

bool offhandButton(bool pressed)
{
    if(!vr_notes.value || key_dest != key_game)
    {
        return false;
    }
    if(pressed && !recording && offhandAtMouth())
    {
        start(true);
        return recording;
    }
    if(!pressed && recording && byButton)
    {
        stop();
        return true;
    }
    return false;
}

void shutdown()
{
    stop();
}

} // namespace qvr::voicenotes
