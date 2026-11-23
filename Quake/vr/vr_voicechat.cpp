#include "vr_alloccount.h"
// vr_voicechat.cpp -- see vr_voicechat.hpp (the client: capture, encoding, sending, receiving, playing out, indicators;
// the server's relay: vr_voicerelay.cpp).

#include "vr_voicechat.hpp"
#include "vr_voicenotes.hpp"
#include "vr_cvars.hpp"
#include "vr_backend.hpp"
#include "vr_input.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_mpavatar.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"
#include "vr_api.h"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include "opus.h"
#include "rnnoise.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h> // (CoInitializeEx: the microphone opened on a worker)
#endif
#include <math.h>
#include <stdlib.h>

extern "C" void* VR_HeapMalloc(za::SizeT size);

namespace qvr::voicechat
{
namespace
{

constexpr int blockSamples = 480;              // RNNoise's 10 ms
constexpr double frameSeconds = 0.02;
constexpr int outQueue = 16;                   // frames waiting to go up (320 ms; older ones dropped)
constexpr int jitterSlots = 32;                // a speaker's frames by sequence number
constexpr double lead = 0.04;                  // s decoded ahead of the mix's (or the clock's) read position
constexpr double jitterMin = 0.04, jitterMax = 0.2; // s of a spurt's start delay: 40 ms + 3 x the measured jitter
constexpr double spurtTimeout = 0.12;          // s with nothing new: a spurt whose end marker was lost ends
constexpr int maxConcealed = 8;                // frames concealed in a row before the spurt is taken as ended
constexpr double idleStop = 2.0;               // s silent before a speaker's sound channel is let go
constexpr double ringSeconds = 2.0;            // the looping sound's length
constexpr double historySeconds = 1.0;         // the played samples kept for vr_voicechat_report
constexpr double historyKeep = 30.0;           // s the history is written after a vr_voicechat_report (the tests' reports)
constexpr za::SizeT catchUpMost = sampleRate / 10; // captured samples taken at most a frame (a hitch's older backlog dropped)
constexpr double talkingHold = 0.25;           // s the indicator stays after the last frame heard
constexpr int hangFrames = 20;                 // voice activation: frames sent on after the voice drops (0.4 s)
constexpr int voiceEntChannel = 1000;          // the voices' sound channel on the speaker's entity (none of the game's)
constexpr double meterLevelFloor = -50.0, meterLevelSpan = 30.0; // dBFS: denoise off, the level as a voice probability

[[nodiscard]] long long wallMs() // epoch milliseconds: compared across processes (za::Clock::now's epoch is unspecified)
{
    return za::Clock::nowUnixNanoseconds() / 1'000'000; // (system_clock's, truncated as duration_cast did)
}

// ---- Sending -------------------------------------------------------------------------------------------------------

struct Opened
{
    SDL_AudioDeviceID id = 0;
    za::String error; // (the worker's SDL_GetError when it failed: SDL's error is per thread)
};

struct Capture
{
    SDL_AudioDeviceID device = 0;
    jobs::Future<Opened> opening;
    jobs::Future<bool> closing;
    double retryAt = 0.0;
    bool audioInit = false;
    bool noMicSaid = false;
    bool pttEver = false;     // push-to-talk pressed in this game: its microphone kept open
    za::Vector<float> pending; // captured, not yet in a block (S16 scale)
    DenoiseState* denoise = nullptr;
    OpusEncoder* enc = nullptr;
    float frame[frameSamples]{};
    float preroll[frameSamples]{};
    bool havePreRoll = false;
    int frameFill = 0;
    float frameVad = 0.f;
    bool sending = false;
    int hang = 0;
    za::U16 seq = 0;
    Frame out[outQueue];
    Frame again[maxFramesPerMessage]; // the last datagram's new frames, sent once more
    int againCount = 0;
    int outHead = 0, outCount = 0; // (a ring: the oldest at outHead)
    double toneLast = -1.0;
    double tonePhase = 0.0;
    // counts (vr_voicechat_report)
    long long framesSent = 0, bytesSent = 0, framesDropped = 0, spurts = 0;
    bool firstSaid = false;
    float level = 0.f; // the last frame's RMS (0..1)
};
Capture cap;
bool pttHeld = false;
bool pttByHand = false;

// What this client last told the server it hears (vr_voicechat_listen, tellListen): the speakers he muted, his voice
// chat off. Told again at each signon (the server forgets it with his slot) and on each change.
struct ListenSent
{
    bool valid = false;
    unsigned muted = 0;
    bool off = false;
};
ListenSent listenSent;

} // namespace
} // namespace qvr::voicechat
extern "C" qboolean keydown[MAX_KEYS];             // keys.c
extern "C" int Key_StringToKeynum(const char* str); // keys.c
namespace qvr::voicechat
{
namespace
{

// Push-to-talk held: +vr_voicechat, the off hand at the mouth, or the Push-to-Talk key (vr_flat_key_voicechat: flat
// play owns it, VR reads it too) in the game.
[[nodiscard]] bool pttWanted()
{
    const int k = vr_flat_key_voicechat.string[0] ? Key_StringToKeynum(vr_flat_key_voicechat.string) : -1;
    return pttHeld || (key_dest == key_game && k > 0 && k < MAX_KEYS && keydown[k]);
}

// The Voice Chat page's texts (playerLine, ownLine).
struct MenuText
{
    char players[MAX_SCOREBOARD][96]{};
    char own[96]{};
};
MenuText menuText;

// ---- Receiving -----------------------------------------------------------------------------------------------------

struct JitterSlot
{
    bool has{false};
    Frame frame;
};

struct Speaker
{
    OpusDecoder* dec = nullptr;
    JitterSlot jb[jitterSlots];
    bool playing = false;
    za::U16 nextSeq = 0;
    int concealRun = 0;
    // the frames given out by sequence number: 1 from their own data, 2 recovered or concealed (a copy of the first: a
    // duplicate; of the second: late)
    za::U16 doneSeq[jitterSlots]{};
    za::U8 doneHow[jitterSlots]{};
    int stretch = 0, stretches = 0; // frames of concealment to put in before the next (a late frame: the delay up)
    double lastArrival = -1.0;
    za::U16 prevSeq = 0;      // the newest frame's sequence ...
    double prevArrival = 0.0; // ... and when it came (the jitter estimate)
    double jitter = 0.0;   // s, RFC 3550's estimate
    bool haveTransit = false;
    double startAt = 0.0;  // output samples (W): silence until here at a spurt's start (its jitter delay)
    // the looping sound (sound on) or the clock (off)
    sfx_t* sfx = nullptr;
    int channel = -1;
    double t0 = 0.0;       // realtime the clock's reader started at (no sound)
    long long wraps = 0;
    int lastPos = 0;
    double written = 0.0;  // W: output samples written (unwrapped)
    long long cleared = 0; // the ring zeroed up to here (unwrapped): what the mix has read (pump)
    double resamplePhase = 0.0;
    float lastSample = 0.f;
    double idleSince = -1.0;
    bool active = false;   // a channel or the clock running
    double lastHeard = -1.0;
    double lastVoiced = -1.0; // realtime his last frame with a voice arrived (muted or not: the Voice Chat page)
    bool firstSaid = false;
    // his settings (vr_voicechat_player_volume, vr_voicechat_mute): the slot's player's, kept through a rename, reset when
    // the slot empties (watchSlot) or you leave the game
    float volume = 1.f;
    bool muted = false;
    bool present = false;  // a player in the slot when last seen in the game (watchSlot)
    za::Vector<float> history; // the played 48 kHz samples, a ring (historySeconds)
    za::SizeT historyAt = 0;
    long long historyCount = 0;
    // counts
    long long received = 0, played = 0, concealed = 0, recovered = 0, late = 0, overflow = 0, skipped = 0, underruns = 0,
              restarts = 0, spurts = 0, dups = 0, stretched = 0;
};
Speaker speakers[MAX_SCOREBOARD];

// Sequence `seq` given out (`how`: 1 its own data, 2 recovered or concealed), the next one due.
void advance(Speaker& sp, za::U8 how)
{
    sp.doneSeq[sp.nextSeq % jitterSlots] = sp.nextSeq;
    sp.doneHow[sp.nextSeq % jitterSlots] = how;
    sp.nextSeq++;
}
bool clockOnly = false; // (the self-test: the play-out on the clock, no sound)

// The played samples' history (emit), written only while vr_voicechat_report is in use: realtime of its last report.
struct History
{
    double askedAt = -1.0;
};
History history;

[[nodiscard]] bool historyOn()
{
    return history.askedAt >= 0.0 && realtime - history.askedAt < historyKeep;
}

// (scratch: the frame's decoded samples, a captured block's)
struct Scratch
{
    float pcm[frameSamples * 2]{};
    float block[blockSamples]{};
    float denoised[blockSamples]{};
    za::I16 capture[4096]{};
    unsigned char packet[maxFrameBytes]{};
};
Scratch scratch;

// (the self-test's signals; the report's window)
struct SelftestScratch
{
    za::Vector<float> ref, out;
};
SelftestScratch selftestScratch;

[[nodiscard]] bool active()
{
    return cls.state == ca_connected && cls.signon == SIGNONS && cl.maxclients > 1 && !cls.demoplayback &&
           vr_voicechat.value != 0.f;
}

[[nodiscard]] bool testTone()
{
    return vr_voicechat_test_tone.value > 0.f;
}

// ---- The microphone ------------------------------------------------------------------------------------------------

[[nodiscard]] bool deviceJobsBusy()
{
    return (cap.opening.valid() && !cap.opening.ready()) || (cap.closing.valid() && !cap.closing.ready());
}

void takeOpened()
{
    if(!cap.opening.valid() || !cap.opening.ready())
    {
        return;
    }
    const Opened o = cap.opening.get();
    if(o.id)
    {
        cap.device = o.id;
        cap.retryAt = 0.0; // (the wait is a failed open's: one closed again, a level change, opens at once)
        Con_Printf("Voice chat: microphone open\n");
    }
    else if(!cap.noMicSaid)
    {
        cap.noMicSaid = true;
        Con_Printf("Voice chat: no microphone (%s)\n", o.error.cStr());
    }
}

void closeMic(bool wait)
{
    if(wait)
    {
        cap.opening.wait();
        takeOpened();
        cap.closing.wait();
    }
    if(!cap.device)
    {
        return;
    }
    const SDL_AudioDeviceID id = cap.device;
    cap.device = 0;
    if(wait)
    {
        SDL_CloseAudioDevice(id);
        return;
    }
    cap.closing = jobs::async(
        [id]()
        {
            SDL_CloseAudioDevice(id);
            return true;
        });
}

void openMic()
{
    takeOpened();
    if(cap.device || cap.opening.valid() || deviceJobsBusy() || voicenotes::deviceBusy() || realtime < cap.retryAt)
    {
        return;
    }
    cap.retryAt = realtime + 30.0;
    if(COM_CheckParm("-nomic"))
    {
        if(!cap.noMicSaid)
        {
            cap.noMicSaid = true;
            Con_Printf("Voice chat: no microphone (-nomic)\n");
        }
        return;
    }
    if(!cap.audioInit)
    {
        if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        {
            Con_Printf("Voice chat: no audio (%s)\n", SDL_GetError());
            return;
        }
        cap.audioInit = true;
    }
    const char* named = voicenotes::microphone(true);
    cap.opening = jobs::async(
        [name = za::String{named ? named : ""}]()
        {
#ifdef _WIN32
            (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
            SDL_AudioSpec want{}, have{};
            want.freq = sampleRate;
            want.format = AUDIO_S16SYS;
            want.channels = 1;
            want.samples = blockSamples;
            Opened o;
#ifdef __ANDROID__
            // Android asks the player for the microphone the first time (RECORD_AUDIO, AndroidManifest.xml); SDL's
            // call waits for the answer, so it is asked here on the worker, never at the start or on the main thread.
            if(!SDL_AndroidRequestPermission("android.permission.RECORD_AUDIO"))
            {
                o.error = za::String{"microphone permission not granted"};
                return o;
            }
#endif
            // (no changes allowed: SDL converts to 48 kHz mono 16-bit, RNNoise's and Opus's own)
            o.id = SDL_OpenAudioDevice(name.empty() ? nullptr : name.cStr(), 1, &want, &have, 0);
            if(o.id)
            {
                SDL_PauseAudioDevice(o.id, 0);
            }
            else
            {
                o.error = za::String{SDL_GetError()};
            }
            return o;
        });
}

// ---- Encoding ------------------------------------------------------------------------------------------------------

void queueFrame(const unsigned char* data, int len, za::U8 flags)
{
    if(cap.outCount == outQueue)
    {
        cap.outHead = (cap.outHead + 1) % outQueue;
        cap.outCount--;
        cap.framesDropped++;
    }
    Frame& f = cap.out[(cap.outHead + cap.outCount) % outQueue];
    f.seq = cap.seq++;
    f.flags = flags;
    f.len = static_cast<za::U8>(za::clamp(len, 0, maxFrameBytes));
    if(len > 0)
    {
        ZA_MEMCPY(f.data, data, static_cast<za::SizeT>(f.len));
    }
    cap.outCount++;
    if(len > 0)
    {
        cap.framesSent++;
        cap.bytesSent += len;
    }
}

[[nodiscard]] bool ensureEncoder()
{
    if(!cap.enc)
    {
        int err = 0;
        cap.enc = opus_encoder_create(sampleRate, 1, OPUS_APPLICATION_VOIP, &err);
        if(!cap.enc)
        {
            Con_Printf("Voice chat: no Opus encoder (%s)\n", opus_strerror(err));
            return false;
        }
    }
    return true;
}

// A talk spurt starts: the encoder fresh, at the bitrate asked for.
void startSpurt()
{
    opus_encoder_ctl(cap.enc, OPUS_RESET_STATE);
    opus_encoder_ctl(cap.enc, OPUS_SET_BITRATE(za::clamp(static_cast<int>(vr_voicechat_bitrate.value), 8000, 64000)));
    opus_encoder_ctl(cap.enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(cap.enc, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(cap.enc, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(cap.enc, OPUS_SET_PACKET_LOSS_PERC(10));
    cap.spurts++;
}

void encodeFrame(const float* pcm, za::U8 flags)
{
    float* in = scratch.pcm;
    const float gain = za::clamp(vr_voicechat_mic_gain.value, 0.f, 4.f) / 32768.f;
    for(int i = 0; i < frameSamples; i++)
    {
        in[i] = za::clamp(pcm[i] * gain, -1.f, 1.f);
    }
    const int len = opus_encode_float(cap.enc, in, frameSamples, scratch.packet, maxFrameBytes);
    if(len > 0)
    {
        queueFrame(scratch.packet, len, flags);
        if(!cap.firstSaid && (flags & flagSpurtStart))
        {
            cap.firstSaid = true;
            Con_Printf("mp_voice_tx: t=%lld seq=%d\n", wallMs(), static_cast<int>(cap.seq) - 1);
        }
    }
}

// A whole 20 ms frame captured (denoised): sent or not.
void frameDone()
{
    const bool vadMode = vr_voicechat_mode.value != 0.f;
    bool want = pttWanted();
    if(vadMode)
    {
        if(cap.frameVad >= vr_voicechat_vad.value)
        {
            cap.hang = hangFrames;
        }
        else if(cap.hang > 0)
        {
            cap.hang--;
        }
        want = want || cap.hang > 0;
    }
    if(want && ensureEncoder())
    {
        if(!cap.sending)
        {
            startSpurt();
            if(vadMode && cap.havePreRoll)
            {
                encodeFrame(cap.preroll, flagSpurtStart); // (the voice's onset, from the frame before)
                encodeFrame(cap.frame, 0);
            }
            else
            {
                encodeFrame(cap.frame, flagSpurtStart);
            }
            cap.sending = true;
        }
        else
        {
            encodeFrame(cap.frame, 0);
        }
    }
    else if(cap.sending)
    {
        queueFrame(nullptr, 0, flagSpurtEnd); // (the end marker: no data)
        cap.sending = false;
    }
    ZA_MEMCPY(cap.preroll, cap.frame, sizeof(cap.frame));
    cap.havePreRoll = true;
    cap.frameFill = 0;
    cap.frameVad = 0.f;
}

// A 10 ms block captured: denoised (its voice probability), into the frame.
void block(const float* in)
{
    const bool needed = pttWanted() || cap.sending || vr_voicechat_mode.value != 0.f;
    float vad = 0.f;
    const float* use = in;
    if(needed && vr_voicechat_denoise.value != 0.f)
    {
        if(!cap.denoise)
        {
            cap.denoise = rnnoise_create(nullptr);
        }
        if(cap.denoise)
        {
            vad = rnnoise_process_frame(cap.denoise, scratch.denoised, in);
            use = scratch.denoised;
        }
    }
    double sum = 0.0;
    for(int i = 0; i < blockSamples; i++)
    {
        sum += static_cast<double>(in[i]) * in[i];
    }
    const double rms = sqrt(sum / blockSamples) / 32768.0;
    cap.level = static_cast<float>(rms);
    if(use == in)
    {
        const double db = 20.0 * log10(za::max(rms, 1e-6));
        vad = static_cast<float>(za::clamp((db - meterLevelFloor) / meterLevelSpan, 0.0, 1.0));
    }
    ZA_MEMCPY(cap.frame + cap.frameFill, use, sizeof(float) * blockSamples);
    cap.frameFill += blockSamples;
    cap.frameVad = za::max(cap.frameVad, vad);
    if(cap.frameFill == frameSamples)
    {
        frameDone();
    }
}

// Where a frame's capture starts in `pending` samples waiting: the newest catchUpMost.
[[nodiscard]] za::SizeT catchUpFrom(za::SizeT pending)
{
    return pending > catchUpMost ? pending - catchUpMost : 0;
}

void captureFrame()
{
    const bool want = active();
    if(!want)
    {
        takeOpened(); // (an open still in flight when voice chat went off: taken, so it is closed here and not left recording)
        if(cap.device || cap.sending)
        {
            closeMic(false);
            cap.sending = false;
        }
        cap.pending.clear();
        cap.pttEver = false;
        cap.toneLast = -1.0;
        cap.outCount = 0;
        return;
    }
    const bool vadMode = vr_voicechat_mode.value != 0.f;
    if(testTone())
    {
        // (the test hook: a tone in place of the microphone, as many samples as the time since the last frame)
        if(cap.toneLast < 0.0)
        {
            cap.toneLast = realtime;
        }
        const int n = za::clamp(static_cast<int>((realtime - cap.toneLast) * sampleRate), 0, sampleRate / 10);
        cap.toneLast += static_cast<double>(n) / sampleRate;
        const double step = 2.0 * M_PI * vr_voicechat_test_tone.value / sampleRate;
        const float amp = za::clamp(vr_voicechat_test_level.value, 0.f, 1.f) * 32767.f;
        for(int i = 0; i < n; i++)
        {
            cap.pending.emplaceBack(amp * static_cast<float>(sin(cap.tonePhase)));
            cap.tonePhase = fmod(cap.tonePhase + step, 2.0 * M_PI);
        }
    }
    else
    {
        cap.toneLast = -1.0;
        if(vadMode || cap.pttEver || pttWanted())
        {
            cap.pttEver = cap.pttEver || pttWanted();
            openMic();
        }
        if(cap.device)
        {
            for(Uint32 got; (got = SDL_DequeueAudio(cap.device, scratch.capture, sizeof(scratch.capture))) > 0;)
            {
                const int n = static_cast<int>(got / sizeof(za::I16));
                for(int i = 0; i < n; i++)
                {
                    cap.pending.emplaceBack(static_cast<float>(scratch.capture[i]));
                }
            }
        }
        else if(cap.sending)
        {
            queueFrame(nullptr, 0, flagSpurtEnd); // (no microphone any more: nothing to send, the spurt ends)
            cap.sending = false;
            cap.pending.clear();
        }
    }
    // (100 ms at most taken a frame: a hitch's older backlog dropped, old speech is worthless and its RNNoise and Opus
    // work would land on a frame that is already late)
    za::SizeT at = catchUpFrom(cap.pending.size());
    for(; at + blockSamples <= cap.pending.size(); at += blockSamples)
    {
        ZA_MEMCPY(scratch.block, cap.pending.data() + at, sizeof(float) * blockSamples);
        block(scratch.block);
    }
    cap.pending.erase(cap.pending.data(), cap.pending.data() + at);
}

// ---- Playing out ---------------------------------------------------------------------------------------------------

[[nodiscard]] int outRate()
{
    return shm ? shm->speed : sampleRate;
}

[[nodiscard]] float attenuation()
{
    return vr_voicechat_radio.value != 0.f ? 0.f : za::max(0.f, vr_voicechat_attenuation.value);
}

// Player entity `ent`'s head (his avatar's, or his eyes' height).
[[nodiscard]] glm::vec3 headOf(int ent)
{
    const entity_t& e = cl_entities[ent];
    glm::vec3 at{e.origin[0], e.origin[1], e.origin[2]};
    if(const mpavatar::Avatar* a = mpavatar::of(ent); a && a->valid)
    {
        return at + a->head;
    }
    return at + glm::vec3{0.f, 0.f, 22.f};
}

// The listener's place (the head, or the view's origin).
[[nodiscard]] glm::vec3 listenerAt()
{
    const hands::State& s = hands::current();
    if(s.valid)
    {
        return s.head;
    }
    return glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
}

// The gain Quake's spatializer gives a sound `dist` units away at attenuation `attn` (SND_Spatialize's distance part).
[[nodiscard]] float distanceGain(float dist, float attn)
{
    return za::clamp(1.f - dist * attn / 1000.f, 0.f, 1.f);
}

[[nodiscard]] channel_t* channelOf(Speaker& sp, int ent)
{
    if(sp.channel < 0 || !sp.sfx)
    {
        return nullptr;
    }
    channel_t* ch = &snd_channels[sp.channel];
    return ch->sfx == sp.sfx && ch->entnum == ent && ch->entchannel == voiceEntChannel ? ch : nullptr;
}

[[nodiscard]] sfxcache_t* ringOf(const Speaker& sp)
{
    return sp.sfx ? static_cast<sfxcache_t*>(Cache_Check(&sp.sfx->cache)) : nullptr;
}

// The speaker's sound started (its looping ring, silent), or the clock without sound.
void startOutput(Speaker& sp, int slot)
{
    const int ent = slot + 1;
    sp.wraps = 0;
    sp.lastPos = 0;
    sp.written = 0.0;
    sp.cleared = 0;
    sp.resamplePhase = 0.0;
    sp.t0 = realtime;
    sp.channel = -1;
    sp.active = true;
    sp.idleSince = -1.0;
    if(!sp.sfx && !clockOnly)
    {
        char name[32];
        q_snprintf(name, sizeof(name), "qvr_voicechat/%d", slot);
        sp.sfx = S_PrecacheSound(name);
    }
    sfxcache_t* sc = ringOf(sp);
    if(!sc)
    {
        return; // (no sound: the clock alone)
    }
    sc->loopstart = 0;
    ZA_MEMSET(sc->data, 0, static_cast<za::SizeT>(sc->length) * 2);
    vec3_t org;
    const glm::vec3 head = headOf(ent);
    VectorSet(org, head.x, head.y, head.z);
    S_StartSound(ent, voiceEntChannel, sp.sfx, org, 1.f, attenuation());
    for(int c = 0; c < MAX_DYNAMIC_CHANNELS; c++)
    {
        if(snd_channels[c].sfx == sp.sfx && snd_channels[c].entnum == ent && snd_channels[c].entchannel == voiceEntChannel)
        {
            sp.channel = c;
            break;
        }
    }
}

void stopOutput(Speaker& sp, int slot)
{
    if(channelOf(sp, slot + 1))
    {
        S_StopSound(slot + 1, voiceEntChannel);
    }
    sp.channel = -1;
    sp.active = false;
}

// The read position (output samples, unwrapped): the mix's on the ring, else the clock's.
[[nodiscard]] double readPos(Speaker& sp, int slot)
{
    if(sp.sfx && ringOf(sp))
    {
        channel_t* ch = channelOf(sp, slot + 1);
        const sfxcache_t* sc = ringOf(sp);
        if(!ch)
        {
            return -1.0; // (its channel taken by another sound: started again)
        }
        if(ch->pos < sp.lastPos - sc->length / 2)
        {
            sp.wraps++;
        }
        sp.lastPos = ch->pos;
        return static_cast<double>(sp.wraps) * sc->length + ch->pos;
    }
    return (realtime - sp.t0) * outRate();
}

// 20 ms of 48 kHz samples into the output (resampled to the mix's rate, at his volume) and the history (while
// vr_voicechat_report is in use).
void emit(Speaker& sp, const float* pcm, bool silent)
{
    if(historyOn())
    {
        if(sp.history.empty())
        {
            sp.history.resize(static_cast<za::SizeT>(historySeconds * sampleRate), 0.f);
        }
        for(int i = 0; i < frameSamples; i++)
        {
            sp.history[sp.historyAt] = silent ? 0.f : pcm[i];
            sp.historyAt = sp.historyAt + 1 == sp.history.size() ? 0 : sp.historyAt + 1;
        }
        sp.historyCount += frameSamples;
    }
    sfxcache_t* sc = ringOf(sp);
    const double step = static_cast<double>(sampleRate) / outRate();
    const float gain = 32767.f * za::clamp(vr_voicechat_volume.value * sp.volume, 0.f, 4.f);
    // (linear interpolation between the frame's samples, the last one's carried over)
    while(sp.resamplePhase < frameSamples)
    {
        const int i = static_cast<int>(sp.resamplePhase);
        const float f = static_cast<float>(sp.resamplePhase - i);
        const float a = silent ? 0.f : (i == 0 ? sp.lastSample : pcm[i - 1]);
        const float b = silent ? 0.f : pcm[i];
        if(sc)
        {
            const long long w = static_cast<long long>(sp.written) % sc->length;
            reinterpret_cast<short*>(sc->data)[w] = static_cast<short>(za::clamp((a + (b - a) * f) * gain, -32767.f, 32767.f));
        }
        sp.written += 1.0;
        sp.resamplePhase += step;
    }
    sp.resamplePhase -= frameSamples;
    sp.lastSample = silent ? 0.f : pcm[frameSamples - 1];
}

// The next frame of a playing spurt: decoded, recovered by FEC from the next one, concealed, or the spurt's end.
void decodeNext(Speaker& sp, int slot)
{
    float* pcm = scratch.pcm;
    JitterSlot& s = sp.jb[sp.nextSeq % jitterSlots];
    if(s.has && s.frame.seq == sp.nextSeq)
    {
        s.has = false;
        if(s.frame.len == 0)
        {
            sp.playing = false; // (the end marker)
            emit(sp, pcm, true);
            advance(sp, 1);
            return;
        }
        const int n = opus_decode_float(sp.dec, s.frame.data, s.frame.len, pcm, frameSamples, 0);
        emit(sp, pcm, n != frameSamples);
        sp.played++;
        sp.concealRun = 0;
        sp.lastHeard = realtime;
        if(!sp.firstSaid)
        {
            sp.firstSaid = true;
            const double ahead = za::max(0.0, sp.written - za::max(0.0, readPos(sp, slot))) / outRate();
            Con_Printf("mp_voice_rx: t=%lld slot=%d seq=%d ahead_ms=%.0f\n", wallMs() + static_cast<long long>(ahead * 1000.0),
                slot, static_cast<int>(sp.nextSeq), ahead * 1000.0);
        }
        advance(sp, 1);
        return;
    }
    // (lost or late: a newer frame here?)
    bool newer = false;
    for(const JitterSlot& j : sp.jb)
    {
        newer = newer || (j.has && seqDiff(j.frame.seq, sp.nextSeq) > 0);
    }
    const JitterSlot& next = sp.jb[static_cast<za::U16>(sp.nextSeq + 1) % jitterSlots];
    if(next.has && next.frame.seq == static_cast<za::U16>(sp.nextSeq + 1) && next.frame.len > 0)
    {
        const int n = opus_decode_float(sp.dec, next.frame.data, next.frame.len, pcm, frameSamples, 1);
        emit(sp, pcm, n != frameSamples);
        sp.recovered++;
        sp.concealRun = 0;
        sp.lastHeard = realtime;
    }
    else if((newer || realtime - sp.lastArrival < spurtTimeout) && sp.concealRun < maxConcealed)
    {
        const int n = opus_decode_float(sp.dec, nullptr, 0, pcm, frameSamples, 0);
        emit(sp, pcm, n != frameSamples);
        sp.concealed++;
        sp.concealRun++;
    }
    else
    {
        sp.playing = false;
        emit(sp, pcm, true);
        return;
    }
    advance(sp, 2);
}

// A ring of `length` samples zeroed from `from` to `to` (unwrapped positions), across its end.
void zeroRing(short* ring, int length, long long from, long long to)
{
    if(to <= from || length <= 0)
    {
        return;
    }
    if(to - from >= length)
    {
        ZA_MEMSET(ring, 0, sizeof(short) * static_cast<za::SizeT>(length));
        return;
    }
    const long long at = from % length, n = to - from;
    const long long first = za::min(n, length - at);
    ZA_MEMSET(ring + at, 0, sizeof(short) * static_cast<za::SizeT>(first));
    ZA_MEMSET(ring, 0, sizeof(short) * static_cast<za::SizeT>(n - first));
}

// A speaker's output kept `lead` ahead of its reader.
void pump(Speaker& sp, int slot)
{
    if(!sp.active)
    {
        return;
    }
    S_MixJoin(); // (the pool's mix done: its channel's position read, the ring and the channel written)
    double r = readPos(sp, slot);
    if(r < 0.0)
    {
        sp.restarts++;
        startOutput(sp, slot);
        r = readPos(sp, slot);
        sp.startAt = sp.written;
    }
    // (what the mix has read since the last pump zeroed: it paints up to _snd_mixahead past its read position, beyond
    // what is written `lead` ahead, and the ring loops, so it read the speech of 2 s before there; now silence)
    if(sfxcache_t* sc = ringOf(sp); sc && static_cast<long long>(r) > sp.cleared)
    {
        zeroRing(reinterpret_cast<short*>(sc->data), sc->length, sp.cleared, static_cast<long long>(r));
        sp.cleared = static_cast<long long>(r);
    }
    const double rate = outRate();
    if(sp.written < r)
    {
        sp.underruns++;
        sp.written = r; // (behind its reader after a hitch: caught up, silent)
        sp.startAt = za::max(sp.startAt, r);
    }
    // (too much waiting: a frame skipped, keeping the delay the spurt started with)
    int buffered = 0;
    for(const JitterSlot& j : sp.jb)
    {
        buffered += j.has && seqDiff(j.frame.seq, sp.nextSeq) >= 0 ? 1 : 0;
    }
    const int most = static_cast<int>(jitterMax / frameSeconds) + 3;
    if(sp.playing && buffered > most)
    {
        JitterSlot& s = sp.jb[sp.nextSeq % jitterSlots];
        if(s.has && s.frame.seq == sp.nextSeq && s.frame.len > 0)
        {
            s.has = false;
            advance(sp, 2);
            sp.skipped++;
        }
    }
    const float* none = scratch.pcm;
    while(sp.written < r + lead * rate)
    {
        if(sp.playing && sp.written >= sp.startAt && sp.stretch > 0)
        {
            // (a frame came late: 20 ms of concealment put in, the delay up by as much)
            const int n = opus_decode_float(sp.dec, nullptr, 0, scratch.pcm, frameSamples, 0);
            emit(sp, scratch.pcm, n != frameSamples);
            sp.stretch--;
            sp.stretched++;
        }
        else if(sp.playing && sp.written >= sp.startAt)
        {
            decodeNext(sp, slot);
        }
        else
        {
            emit(sp, none, true);
        }
    }
    if(sp.playing || realtime - sp.lastHeard < talkingHold)
    {
        sp.idleSince = -1.0;
    }
    else if(sp.idleSince < 0.0)
    {
        sp.idleSince = realtime;
    }
    else if(realtime - sp.idleSince > idleStop)
    {
        stopOutput(sp, slot);
    }
    // (his head moves: the sound with it; radio or not)
    if(channel_t* ch = channelOf(sp, slot + 1))
    {
        const glm::vec3 head = headOf(slot + 1);
        VectorSet(ch->origin, head.x, head.y, head.z);
        ch->dist_mult = attenuation() / 1000.f;
        ch->master_vol = 255;
    }
}

void resetSpeaker(Speaker& sp, int slot, bool keepSettings)
{
    stopOutput(sp, slot);
    for(JitterSlot& j : sp.jb)
    {
        j.has = false;
    }
    sp.playing = false;
    sp.haveTransit = false;
    sp.jitter = 0.0;
    sp.lastArrival = -1.0;
    sp.lastVoiced = -1.0;
    sp.lastHeard = -1.0;
    sp.firstSaid = false;
    sp.received = sp.played = sp.concealed = sp.recovered = sp.late = sp.overflow = sp.skipped = sp.underruns = 0;
    sp.restarts = sp.spurts = sp.dups = sp.stretched = 0;
    sp.historyCount = 0;
    sp.stretch = sp.stretches = 0;
    for(za::U8& h : sp.doneHow)
    {
        h = 0;
    }
    if(sp.dec)
    {
        opus_decoder_ctl(sp.dec, OPUS_RESET_STATE);
    }
    if(!keepSettings)
    {
        sp.volume = 1.f;
        sp.muted = false;
    }
}

// A frame arrived from slot `slot`.
void receive(int slot, const Frame& f)
{
    const za::U16 seq = f.seq;
    const int len = f.len;
    Speaker& sp = speakers[slot];
    sp.received += len > 0 ? 1 : 0;
    const double now = realtime;
    // (RFC 3550's jitter: the arrival's transit against the sequence's clock, from the newest frame's. mpfix C2: only a
    // frame newer than the newest seen -- each frame comes twice, the next datagram's copy a datagram later, and the
    // copies read as 20 ms of jitter on a perfect link -- and the sequences' difference by seqDiff: the 16-bit wrap
    // after 22 minutes of talk is no jump)
    if(!sp.haveTransit || (f.flags & flagSpurtStart) || seqDiff(seq, sp.prevSeq) > 0)
    {
        if(sp.haveTransit && !(f.flags & flagSpurtStart))
        {
            const double d = fabs((now - sp.prevArrival) - seqDiff(seq, sp.prevSeq) * frameSeconds);
            sp.jitter += (za::min(d, 1.0) - sp.jitter) / 16.0;
        }
        sp.prevSeq = seq;
        sp.prevArrival = now;
        sp.haveTransit = true;
    }
    sp.lastArrival = now;
    if(len > 0)
    {
        sp.lastVoiced = now; // (muted too: the Voice Chat page shows who talks, to know whom to mute or hear again)
    }
    if(sp.muted || vr_voicechat.value == 0.f)
    {
        return;
    }
    if(!sp.dec)
    {
        int err = 0;
        sp.dec = opus_decoder_create(sampleRate, 1, &err);
        if(!sp.dec)
        {
            return;
        }
    }
    JitterSlot& s = sp.jb[seq % jitterSlots];
    const bool given = sp.doneHow[seq % jitterSlots] != 0 && sp.doneSeq[seq % jitterSlots] == seq;
    if((s.has && s.frame.seq == seq) || (given && sp.doneHow[seq % jitterSlots] == 1))
    {
        sp.dups++; // (each frame comes twice: its next datagram's copy)
        return;
    }
    if(sp.playing && seqDiff(seq, sp.nextSeq) < 0)
    {
        // (an earlier frame of the spurt before its decoding began: from it; else too late, the delay raised)
        if(sp.written >= sp.startAt || seqDiff(seq, sp.nextSeq) <= -jitterSlots / 2)
        {
            sp.late++;
            if(given && sp.stretches + sp.stretch < static_cast<int>((jitterMax - jitterMin) / frameSeconds))
            {
                sp.stretch++;
                sp.stretches++;
            }
            return;
        }
        sp.nextSeq = seq;
    }
    if(s.has && s.frame.seq != seq && (!sp.playing || seqDiff(s.frame.seq, sp.nextSeq) >= 0))
    {
        sp.overflow++;
    }
    s.has = true;
    s.frame = f;
    if(!sp.playing && len > 0)
    {
        // a spurt starts (its first frame, or the first that came): after its jitter delay
        S_MixJoin(); // (the pool's mix done: the channel started, its position read)
        if(!sp.active)
        {
            startOutput(sp, slot);
        }
        const double r = za::max(0.0, readPos(sp, slot));
        sp.written = za::max(sp.written, r);
        const double delay = za::clamp(jitterMin + 3.0 * sp.jitter, jitterMin, jitterMax);
        sp.startAt = za::max(sp.written, r + lead * outRate()) + delay * outRate();
        sp.playing = true;
        sp.nextSeq = seq;
        sp.concealRun = 0;
        sp.stretch = sp.stretches = 0;
        sp.spurts++;
        opus_decoder_ctl(sp.dec, OPUS_RESET_STATE);
        // (frames of this spurt before it, already here: from the oldest)
        for(JitterSlot& j : sp.jb)
        {
            if(j.has && seqDiff(j.frame.seq, seq) <= -jitterSlots / 2)
            {
                j.has = false; // (an older spurt's, never played: muted, or its end lost)
            }
            else if(j.has && seqDiff(j.frame.seq, sp.nextSeq) < 0 && j.frame.len > 0)
            {
                sp.nextSeq = j.frame.seq;
            }
        }
    }
}

[[nodiscard]] bool talking(int slot)
{
    const Speaker& sp = speakers[slot];
    return !sp.muted && sp.lastHeard >= 0.0 && realtime - sp.lastHeard < talkingHold;
}

// The indicators: over each player heard, and yours while you send.
void indicators()
{
    if(vr_voicechat_indicator.value == 0.f || !active())
    {
        return;
    }
    const glm::vec3 eye = listenerAt();
    const glm::vec3 tint{0.45f, 1.f, 0.55f};
    const float m2u = units::metresToUnits();
    for(int slot = 0; slot < cl.maxclients && slot < MAX_SCOREBOARD; slot++)
    {
        if(slot + 1 == cl.viewentity || !talking(slot))
        {
            continue;
        }
        const glm::vec3 at = headOf(slot + 1) + glm::vec3{0.f, 0.f, 0.32f * m2u};
        const glm::vec3 to = eye - at;
        const float yaw = glm::degrees(atan2f(to.y, to.x));
        text3d::queue("((o))", at, glm::vec3{0.f, yaw, 0.f}, text3d::Align::Centre, 0.12f, false, &tint);
    }
    const hands::State& s = hands::current();
    if(cap.sending && s.valid)
    {
        const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
        const glm::vec3 at = s.head + fwd * (0.7f * m2u) - glm::vec3{0.f, 0.f, 0.34f * m2u};
        text3d::queue("TALKING", at, glm::vec3{0.f, s.headAngles.y, 0.f}, text3d::Align::Centre, 0.06f, false, &tint);
    }
}

// ---- Commands ------------------------------------------------------------------------------------------------------

// A player by slot number (1-based as the scoreboard) or a part of his name; -1 none.
[[nodiscard]] int playerArg(const char* arg)
{
    if(arg[0] >= '0' && arg[0] <= '9')
    {
        const int n = atoi(arg) - 1;
        return n >= 0 && n < cl.maxclients && n < MAX_SCOREBOARD ? n : -1;
    }
    for(int i = 0; i < cl.maxclients && i < MAX_SCOREBOARD; i++)
    {
        if(cl.scores[i].name[0] && q_strcasestr(cl.scores[i].name, arg))
        {
            return i;
        }
    }
    return -1;
}

// A slot's settings against its player (`inGame`: you in the game, its names all known; `named`: a player in the slot):
// reset when the slot empties, so the next player in it starts heard at full volume. A rename keeps them (a mute is
// not escaped by a new name); a level change (names not known until you are in the game again) changes nothing.
void watchSlot(Speaker& sp, int slot, bool inGame, bool named)
{
    if(!inGame)
    {
        return;
    }
    if(sp.present && !named)
    {
        resetSpeaker(sp, slot, false);
    }
    sp.present = named;
}

void mute_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_voicechat_mute <player number or name part> [0|1]: mute a player's voice (no value: toggle)\n");
        return;
    }
    const int slot = playerArg(Cmd_Argv(1));
    if(slot < 0)
    {
        Con_Printf("Voice chat: no player \"%s\"\n", Cmd_Argv(1));
        return;
    }
    setMuted(slot, Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) != 0 : !speakers[slot].muted);
}

void volume_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_voicechat_player_volume <player number or name part> <0..4>: a player's voice volume\n");
        return;
    }
    const int slot = playerArg(Cmd_Argv(1));
    if(slot < 0)
    {
        Con_Printf("Voice chat: no player \"%s\"\n", Cmd_Argv(1));
        return;
    }
    setPlayerVolume(slot, static_cast<float>(Q_atof(Cmd_Argv(2))));
}

void players_f()
{
    for(int i = 0; i < cl.maxclients && i < MAX_SCOREBOARD; i++)
    {
        if(!cl.scores[i].name[0])
        {
            continue;
        }
        const Speaker& sp = speakers[i];
        Con_Printf("%2d %-16s %s volume %.2f%s\n", i + 1, cl.scores[i].name, sp.muted ? "muted" : "heard", sp.volume,
            talking(i) ? "  talking" : "");
    }
}

// Goertzel's share of the samples' power at `hz` (1: a pure tone of it), and their zero-crossing frequency.
void toneOf(const float* x, int n, double hz, double& share, double& freq, double& rms)
{
    const double w = 2.0 * M_PI * hz / sampleRate, c = 2.0 * cos(w);
    double s1 = 0.0, s2 = 0.0, power = 0.0;
    int crossings = 0;
    for(int i = 0; i < n; i++)
    {
        const double s0 = x[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
        power += static_cast<double>(x[i]) * x[i];
        crossings += i > 0 && ((x[i - 1] < 0.f) != (x[i] < 0.f)) ? 1 : 0;
    }
    const double mag2 = s1 * s1 + s2 * s2 - c * s1 * s2;
    share = power > 0.0 ? za::min(1.0, 2.0 * mag2 / (n * power)) : 0.0;
    freq = 0.5 * crossings * sampleRate / za::max(1, n);
    rms = sqrt(power / za::max(1, n));
}

void report_f()
{
    if(!historyOn())
    {
        // (the history starts now: its tone figures from a whole window of it)
        for(Speaker& sp : speakers)
        {
            sp.historyCount = 0;
        }
    }
    history.askedAt = realtime;
    const double hz = Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : 0.0;
    const int window = sampleRate / 2;
    Con_Printf("mp_voice_send: active=%d sending=%d mic=%d frames=%lld bytes=%lld dropped=%lld spurts=%lld level=%.3f queued=%d\n",
        active() ? 1 : 0, cap.sending ? 1 : 0, cap.device ? 1 : 0, cap.framesSent, cap.bytesSent, cap.framesDropped,
        cap.spurts, cap.level, cap.outCount);
    const glm::vec3 eye = listenerAt();
    for(int slot = 0; slot < MAX_SCOREBOARD; slot++)
    {
        const Speaker& sp = speakers[slot];
        if(sp.received == 0 && sp.historyCount == 0)
        {
            continue;
        }
        double share = 0.0, freq = 0.0, rms = 0.0;
        // (the last half second played: in time order out of the ring)
        if(!sp.history.empty() && sp.historyCount >= window)
        {
            za::Vector<float>& tmp = selftestScratch.out;
            tmp.clear();
            for(int i = 0; i < window; i++)
            {
                const za::SizeT at = (sp.historyAt + sp.history.size() - static_cast<za::SizeT>(window) + static_cast<za::SizeT>(i)) %
                                     sp.history.size();
                tmp.emplaceBack(sp.history[at]);
            }
            toneOf(tmp.data(), window, hz > 0.0 ? hz : 440.0, share, freq, rms);
        }
        const float dist = glm::distance(eye, headOf(slot + 1));
        Con_Printf("mp_voice: slot=%d ent=%d received=%lld played=%lld concealed=%lld recovered=%lld late=%lld overflow=%lld "
                   "skipped=%lld underruns=%lld restarts=%lld spurts=%lld dups=%lld stretched=%lld jitter_ms=%.1f talking=%d muted=%d dist=%.0f "
                   "gain=%.3f tone=%.3f freq=%.1f rms=%.4f sound=%d\n",
            slot, slot + 1, sp.received, sp.played, sp.concealed, sp.recovered, sp.late, sp.overflow, sp.skipped, sp.underruns,
            sp.restarts, sp.spurts, sp.dups, sp.stretched, sp.jitter * 1000.0, talking(slot) ? 1 : 0, sp.muted ? 1 : 0, dist,
            distanceGain(dist, attenuation()) * za::clamp(vr_voicechat_volume.value * sp.volume, 0.f, 4.f), share, freq, rms,
            channelOf(const_cast<Speaker&>(sp), slot + 1) ? 1 : 0);
    }
    for(int slot = 0; slot < MAX_SCOREBOARD; slot++)
    {
        if(listed(slot))
        {
            Con_Printf("mp_voice_row: slot=%d %s\n", slot, playerLine(slot)); // (the Voice Chat page's row)
        }
    }
    Con_Printf("mp_voice: done\n");
}

void resetCounts_f()
{
    cap.framesSent = cap.bytesSent = cap.framesDropped = cap.spurts = 0;
    cap.firstSaid = false;
    for(int slot = 0; slot < MAX_SCOREBOARD; slot++)
    {
        Speaker& sp = speakers[slot];
        sp.received = sp.played = sp.concealed = sp.recovered = sp.late = sp.overflow = sp.skipped = sp.underruns = 0;
        sp.restarts = sp.spurts = sp.dups = sp.stretched = 0;
        sp.firstSaid = false;
    }
    Con_Printf("mp_voice: reset\n");
}

void pttDown_f()
{
    pttHeld = true;
}

void pttUp_f()
{
    pttHeld = false;
    pttByHand = false;
}

// ---- Self-test -----------------------------------------------------------------------------------------------------

// The best lag (samples) of `out` behind `ref` and the SNR (dB) there, from `from` on.
void alignedSnr(const za::Vector<float>& ref, const za::Vector<float>& out, int from, int maxLag, int& bestLag, double& snr)
{
    double best = -1e30;
    bestLag = 0;
    const int n = static_cast<int>(za::min(ref.size(), out.size()));
    for(int lag = 0; lag <= maxLag; lag++)
    {
        double dot = 0.0;
        for(int i = from; i + lag < n; i++)
        {
            dot += static_cast<double>(ref[static_cast<za::SizeT>(i)]) * out[static_cast<za::SizeT>(i + lag)];
        }
        if(dot > best)
        {
            best = dot;
            bestLag = lag;
        }
    }
    double sig = 0.0, err = 0.0;
    for(int i = from; i + bestLag < n; i++)
    {
        const double r = ref[static_cast<za::SizeT>(i)], d = out[static_cast<za::SizeT>(i + bestLag)] - r;
        sig += r * r;
        err += d * d;
    }
    snr = 10.0 * log10(sig / za::max(err, 1e-12));
}

void selftest_f()
{
    SelftestScratch& t = selftestScratch;
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what) {
        tally.check(ok);
        Con_Printf("voicechat selftest: %s %s\n", ok ? "PASS" : "FAIL", what);
    };
    int err = 0;
    OpusEncoder* enc = opus_encoder_create(sampleRate, 1, OPUS_APPLICATION_VOIP, &err);
    OpusDecoder* dec = opus_decoder_create(sampleRate, 1, &err);
    check(enc && dec, va("an Opus 48 kHz mono VoIP encoder and decoder (%s)", opus_get_version_string()));
    if(!enc || !dec)
    {
        testkit::verdict("vr_voicechat_selftest", tally.passed, tally.failed);
        return;
    }
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(24000));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(10));

    // 1. Loopback: 2 s of a voiced-like signal (a 180 Hz fundamental and its harmonics, a slow vibrato, syllables: its
    //    level swelling 3.3 times a second, so SILK's voice detector keeps it active) through the codec.
    const int frames = 100, n = frames * frameSamples;
    t.ref.clear();
    t.ref.resize(static_cast<za::SizeT>(n), 0.f);
    t.out.clear();
    t.out.resize(static_cast<za::SizeT>(n), 0.f);
    double phase = 0.0;
    for(int i = 0; i < n; i++)
    {
        const double f0 = 180.0 * (1.0 + 0.02 * sin(2.0 * M_PI * 3.0 * i / sampleRate));
        phase += 2.0 * M_PI * f0 / sampleRate;
        const double env = 0.55 + 0.45 * sin(2.0 * M_PI * 3.3 * i / sampleRate);
        t.ref[static_cast<za::SizeT>(i)] =
            static_cast<float>(env * (0.2 * sin(phase) + 0.1 * sin(2.0 * phase) + 0.05 * sin(3.0 * phase)));
    }
    long long bytes = 0;
    int minBytes = 9999, maxBytes = 0;
    unsigned char packets[frames][maxFrameBytes];
    int lens[frames];
    const za::I64 t0 = za::Clock::nowNanoseconds();
    for(int f = 0; f < frames; f++)
    {
        lens[f] = opus_encode_float(enc, t.ref.data() + f * frameSamples, frameSamples, packets[f], maxFrameBytes);
        bytes += za::max(0, lens[f]);
        minBytes = za::min(minBytes, lens[f]);
        maxBytes = za::max(maxBytes, lens[f]);
    }
    const double encMs = za::nanosecondsToMilliseconds(za::Clock::nowNanoseconds() - t0) / frames;
    bool decodedAll = true;
    for(int f = 0; f < frames; f++)
    {
        decodedAll = decodedAll && opus_decode_float(dec, packets[f], lens[f], t.out.data() + f * frameSamples, frameSamples, 0) == frameSamples;
    }
    int lag = 0;
    double snr = 0.0;
    alignedSnr(t.ref, t.out, sampleRate / 5, 1200, lag, snr);
    const double kbps = bytes * 8.0 / (frames * frameSeconds) / 1000.0;
    check(minBytes > 0 && maxBytes <= maxFrameBytes && decodedAll,
        va("100 frames encoded (%d..%d bytes each) and each decoded whole (960 samples)", minBytes, maxBytes));
    check(kbps > 14.0 && kbps < 30.0, va("the bitrate near the 24 kbit/s asked for: %.1f kbit/s", kbps));
    check(snr > 6.0, va("loopback SNR %.1f dB > 6 dB (SILK codes the spectrum, not the waveform: 9.2 measured; aligned by its delay, %d samples = %.1f ms)", snr, lag, lag * 1000.0 / sampleRate));
    check(encMs < 2.0, va("an encode under 2 ms a 20 ms frame: %.3f ms", encMs));

    // 2. Loss: frame 50 lost; recovered from 51's in-band FEC, then concealed (PLC) for comparison.
    opus_decoder_ctl(dec, OPUS_RESET_STATE);
    for(int f = 0; f < 50; f++)
    {
        (void)opus_decode_float(dec, packets[f], lens[f], t.out.data() + f * frameSamples, frameSamples, 0);
    }
    const int fecN = opus_decode_float(dec, packets[51], lens[51], scratch.pcm, frameSamples, 1);
    double fecErr = 0.0, sig = 0.0;
    for(int i = 0; i < frameSamples; i++)
    {
        const double r = t.ref[static_cast<za::SizeT>(50 * frameSamples + i - lag)];
        fecErr += (scratch.pcm[i] - r) * (scratch.pcm[i] - r);
        sig += r * r;
    }
    check(fecN == frameSamples && fecErr < sig, va("a lost frame recovered from the next one's FEC (SNR %.1f dB; packet modes %d, %d)", 10.0 * log10(sig / za::max(fecErr, 1e-12)), packets[50][0] >> 3, packets[51][0] >> 3));
    opus_decoder_ctl(dec, OPUS_RESET_STATE);
    for(int f = 0; f < 50; f++)
    {
        (void)opus_decode_float(dec, packets[f], lens[f], t.out.data() + f * frameSamples, frameSamples, 0);
    }
    double plcPower = 0.0;
    bool plcOk = true;
    for(int k = 0; k < 5; k++)
    {
        plcOk = plcOk && opus_decode_float(dec, nullptr, 0, scratch.pcm, frameSamples, 0) == frameSamples;
        for(int i = 0; i < frameSamples; i++)
        {
            plcOk = plcOk && isfinite(scratch.pcm[i]) && fabsf(scratch.pcm[i]) <= 1.5f;
            plcPower += scratch.pcm[i] * scratch.pcm[i];
        }
    }
    check(plcOk && plcPower < 5.0 * sig, "five lost frames concealed (PLC): whole frames, bounded, no blow-up");

    // 3. RNNoise: white noise at -26 dBFS through it, attenuated; its voice probability low.
    DenoiseState* dn = rnnoise_create(nullptr);
    check(dn != nullptr && rnnoise_get_frame_size() == blockSamples, "an RNNoise state with its built-in model, 480-sample frames");
    if(dn)
    {
        unsigned rng = 12345u;
        double inP = 0.0, outP = 0.0, vadSum = 0.0;
        const int blocks = 200;
        const za::I64 d0 = za::Clock::nowNanoseconds();
        for(int b = 0; b < blocks; b++)
        {
            for(int i = 0; i < blockSamples; i++)
            {
                rng = rng * 1664525u + 1013904223u;
                scratch.block[i] = (static_cast<float>(rng >> 8) / 16777216.f - 0.5f) * 2.f * 0.05f * 32768.f * 1.732f;
            }
            const float vad = rnnoise_process_frame(dn, scratch.denoised, scratch.block);
            if(b >= 50) // (after it settles)
            {
                vadSum += vad;
                for(int i = 0; i < blockSamples; i++)
                {
                    inP += static_cast<double>(scratch.block[i]) * scratch.block[i];
                    outP += static_cast<double>(scratch.denoised[i]) * scratch.denoised[i];
                }
            }
        }
        const double dnMs = za::nanosecondsToMilliseconds(za::Clock::nowNanoseconds() - d0) / blocks;
        const double atten = 10.0 * log10(inP / za::max(outP, 1e-9));
        check(atten > 10.0, va("white noise attenuated by %.1f dB (> 10)", atten));
        check(vadSum / 150.0 < 0.3, va("its voice probability on noise %.2f (< 0.3)", vadSum / 150.0));
        check(dnMs < 1.0, va("a 10 ms block denoised in %.3f ms (< 1)", dnMs));
        rnnoise_destroy(dn);
    }

    // 4. The jitter buffer and play-out (the clock without sound): 100 frames in shuffled pairs, 10% lost: every frame
    //    played, recovered or concealed, in order, none late.
    {
        Speaker& sp = speakers[MAX_SCOREBOARD - 1];
        const int slot = MAX_SCOREBOARD - 1;
        resetSpeaker(sp, slot, true);
        const double savedRealtime = realtime;
        sfx_t* const savedSfx = sp.sfx;
        sp.sfx = nullptr; // (the clock: no sound in a self-test)
        clockOnly = true;
        opus_decoder_ctl(dec, OPUS_RESET_STATE);
        int lost = 0;
        for(int f = 0; f < frames; f++)
        {
            // frame f arrives at f*20 ms (pairs swapped: 1 before 0, 3 before 2...), every 10th lost; the clock runs
            const int g = f % 2 == 0 ? f + 1 : f - 1;
            realtime = savedRealtime + f * frameSeconds;
            if(g % 10 == 7)
            {
                lost++;
            }
            else
            {
                Frame fr;
                fr.seq = static_cast<za::U16>(g + 100);
                fr.flags = static_cast<za::U8>(g == 0 ? flagSpurtStart : 0);
                fr.len = static_cast<za::U8>(lens[g]);
                ZA_MEMCPY(fr.data, packets[g], static_cast<za::SizeT>(lens[g]));
                receive(slot, fr);
            }
            pump(sp, slot);
        }
        Frame end;
        end.seq = static_cast<za::U16>(frames + 100);
        end.flags = flagSpurtEnd;
        receive(slot, end);
        for(int k = 0; k < 20; k++)
        {
            realtime += frameSeconds;
            pump(sp, slot);
        }
        const long long out = sp.played + sp.recovered + sp.concealed;
        check(out == frames && sp.late == 0, va("the jitter buffer: %lld played, %lld recovered (FEC), %lld concealed of 100 sent "
                                                 "(%d lost, pairs swapped), %lld late", sp.played, sp.recovered, sp.concealed, lost, sp.late));
        check(!sp.playing, "the spurt ended at its end marker");
        realtime = savedRealtime;
        resetSpeaker(sp, slot, true);
        clockOnly = false;
        sp.sfx = savedSfx;
    }

    // 4b. (mpfix C2) The jitter estimate on a perfect link: each frame on time with the last one's copy beside it (as the
    //     relay sends them), the sequences across the 16-bit wrap: no jitter (the copies read as 20 ms before).
    {
        Speaker& sp = speakers[MAX_SCOREBOARD - 1];
        const int slot = MAX_SCOREBOARD - 1;
        resetSpeaker(sp, slot, true);
        const double savedRealtime = realtime;
        sfx_t* const savedSfx = sp.sfx;
        sp.sfx = nullptr;
        clockOnly = true;
        for(int f = 0; f < frames; f++)
        {
            realtime = savedRealtime + f * frameSeconds;
            for(int copy = f > 0 ? 1 : 0; copy >= 0; copy--)
            {
                const int g = f - copy;
                Frame fr;
                fr.seq = static_cast<za::U16>(0xffd0 + g);
                fr.flags = static_cast<za::U8>(g == 0 ? flagSpurtStart : 0);
                fr.len = static_cast<za::U8>(lens[g]);
                ZA_MEMCPY(fr.data, packets[g], static_cast<za::SizeT>(lens[g]));
                receive(slot, fr);
            }
            pump(sp, slot);
        }
        check(sp.jitter < 0.002, va("the jitter estimate with every frame's copy and the 16-bit wrap: %.1f ms (none)",
                                     sp.jitter * 1000.0));
        realtime = savedRealtime;
        resetSpeaker(sp, slot, true);
        clockOnly = false;
        sp.sfx = savedSfx;
    }

    // 5. The wire format (writeFrame / readFrame, every voice message's): a frame back as it was written; one longer
    //    than maxFrameBytes refused with its bytes read past, the next one still read in step.
    {
        za::U8 bytes[1024];
        sizebuf_t buf{};
        buf.data = bytes;
        buf.maxsize = sizeof(bytes);
        Frame a;
        a.seq = 0xfffe;
        a.flags = flagSpurtStart;
        a.len = 3;
        a.data[0] = 7;
        a.data[1] = 8;
        a.data[2] = 9;
        writeFrame(&buf, a);
        MSG_WriteShort(&buf, 5);
        MSG_WriteByte(&buf, 0);
        MSG_WriteByte(&buf, maxFrameBytes + 1);
        for(int k = 0; k <= maxFrameBytes; k++)
        {
            MSG_WriteByte(&buf, k);
        }
        Frame b;
        b.seq = 6;
        b.flags = flagSpurtEnd;
        writeFrame(&buf, b);
        WireRead read{buf};
        Frame ra, rlong, rb;
        const bool okA = readFrame(ra), okLong = readFrame(rlong), okB = readFrame(rb);
        const int atEnd = msg_readcount == buf.cursize;
        read.restore();
        check(okA && ra.seq == a.seq && ra.flags == a.flags && ra.len == 3 && ra.data[2] == 9 && !okLong && okB && rb.seq == 6 &&
                  rb.flags == flagSpurtEnd && rb.len == 0 && atEnd,
            "the wire: a frame read back as written; one over 160 bytes refused, the next still read in step");
        check(seqDiff(2, 0xfffe) == 4 && seqDiff(0xfffe, 2) == -4, "seqDiff across the 16-bit wrap");
    }

    // 6. The play-out's ring (a looping sound of 2 s): the mix reads up to _snd_mixahead past its position, beyond what is
    //    written `lead` ahead; what it has read is zeroed at each pump (zeroRing), so a read past the written end is
    //    silence, not the speech of a lap before. A ring of 1000 samples, silent at its start (startOutput), 2.5 laps of
    //    a reader 70 samples a pump with the writer 40 ahead of it, all speech: the reads past the written end, up to 100
    //    ahead, all 0 (before: the lap before's speech from the second lap on).
    {
        constexpr int length = 1000, step = 70, ahead = 40, overRead = 100;
        short ring[length]{};
        long long cleared = 0, written = 0, stale = 0, checked = 0;
        for(long long r = 0; r < length * 5 / 2; r += step)
        {
            zeroRing(ring, length, cleared, r);
            cleared = r;
            written = za::max(written, r); // (behind its reader: caught up, as pump's underrun)
            for(; written < r + ahead; written++)
            {
                ring[written % length] = 1;
            }
            for(long long p = written; p < r + overRead; p++, checked++)
            {
                stale += ring[p % length] != 0 ? 1 : 0;
            }
        }
        check(stale == 0 && checked > 0, va("the play-out's ring: a read past the written end silent (%lld of %lld samples stale)",
                                               stale, checked));
    }

    // 7. A player's settings belong to his slot's player: a rename keeps his mute and volume (no escaping a mute by a new
    //    name), a level change (names not known until in the game again) too; the slot emptied resets them.
    {
        Speaker& sp = speakers[MAX_SCOREBOARD - 1];
        const int slot = MAX_SCOREBOARD - 1;
        const bool savedMuted = sp.muted, savedPresent = sp.present;
        const float savedVolume = sp.volume;
        sp.present = true;
        sp.muted = true;
        sp.volume = 0.5f;
        watchSlot(sp, slot, true, true); // (renamed: a player still in the slot)
        const bool renamedKeeps = sp.muted && sp.volume == 0.5f;
        watchSlot(sp, slot, false, false); // (a level change: his name not yet known)
        const bool levelKeeps = sp.muted && sp.volume == 0.5f;
        watchSlot(sp, slot, true, false); // (he left)
        const bool leftResets = !sp.muted && sp.volume == 1.f;
        watchSlot(sp, slot, true, true); // (a new player in his slot)
        check(renamedKeeps && levelKeeps && leftResets && !sp.muted,
            va("a player's mute and volume kept through a rename (%d) and a level change (%d), reset when his slot empties (%d)",
                renamedKeeps ? 1 : 0, levelKeeps ? 1 : 0, leftResets ? 1 : 0));
        sp.muted = savedMuted;
        sp.present = savedPresent;
        sp.volume = savedVolume;
    }

    // 8. The capture's catch-up after a hitch: 100 ms at most taken (half a second waiting: the newest 4800 samples, 10
    //    RNNoise blocks, 5 Opus frames); the played history written only while vr_voicechat_report is in use.
    {
        const za::SizeT from = catchUpFrom(sampleRate / 2), none = catchUpFrom(sampleRate / 20);
        Speaker& sp = speakers[MAX_SCOREBOARD - 1];
        sfx_t* const savedSfx = sp.sfx;
        const History savedHistory = history;
        sp.sfx = nullptr;
        za::fill(scratch.pcm, 0.25f);
        history.askedAt = -1.0;
        const long long before = sp.historyCount;
        emit(sp, scratch.pcm, false);
        const long long off = sp.historyCount - before;
        history.askedAt = realtime;
        emit(sp, scratch.pcm, false);
        const long long on = sp.historyCount - before - off;
        history = savedHistory;
        sp.sfx = savedSfx;
        check(sampleRate / 2 - from == sampleRate / 10 && none == 0 && off == 0 && on == frameSamples,
            va("the capture's catch-up: %d of %d samples waiting taken (%d of %d); the history: %lld samples written without a "
               "report, %lld with", static_cast<int>(sampleRate / 2 - from), sampleRate / 2, static_cast<int>(sampleRate / 20 - none),
                sampleRate / 20, off, on));
    }

    // 9. The server's relay (vr_voicerelay.cpp): its fairness between speakers, the rotation, the mutes and voice off.
    serverSelftest(check);
    opus_encoder_destroy(enc);
    opus_decoder_destroy(dec);
    testkit::verdict("vr_voicechat_selftest", tally.passed, tally.failed);
}

// The server told the speakers this client muted and whether his voice chat is off (the relay sends him none of theirs).
void tellListen()
{
    if(cls.state != ca_connected || cls.signon != SIGNONS || cl.maxclients <= 1 || cls.demoplayback)
    {
        listenSent.valid = false;
        return;
    }
    unsigned bits = 0;
    for(int slot = 0; slot < MAX_SCOREBOARD && slot < 32; slot++)
    {
        bits |= muted(slot) ? 1u << slot : 0u;
    }
    const bool off = vr_voicechat.value == 0.f;
    if(listenSent.valid && listenSent.muted == bits && listenSent.off == off)
    {
        return;
    }
    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, va("vr_voicechat_listen %u %d", bits, off ? 1 : 0));
    listenSent = ListenSent{true, bits, off};
}

} // namespace

// ---- The Voice Chat page -------------------------------------------------------------------------------------------

bool listed(int slot)
{
    return slot >= 0 && slot < cl.maxclients && slot < MAX_SCOREBOARD && cl.scores[slot].name[0] &&
           slot + 1 != cl.viewentity && cls.state == ca_connected;
}

unsigned long long players()
{
    unsigned long long bits = 0;
    for(int slot = 0; slot < MAX_SCOREBOARD && slot < 64; slot++)
    {
        bits |= listed(slot) ? 1ull << slot : 0ull;
    }
    return bits;
}

const char* playerLine(int slot)
{
    char* line = menuText.players[za::clamp(slot, 0, MAX_SCOREBOARD - 1)];
    if(!listed(slot))
    {
        q_strlcpy(line, "(left)", sizeof(menuText.players[0]));
        return line;
    }
    const Speaker& sp = speakers[slot];
    const bool isMuted = sp.muted;
    const bool voiced = sp.lastVoiced >= 0.0 && realtime - sp.lastVoiced < talkingHold;
    q_snprintf(line, sizeof(menuText.players[0]), "%s   %.0f%%%s", cl.scores[slot].name, sp.volume * 100.f, isMuted ? (voiced ? "   muted, talking" : "   muted") : talking(slot) ? "   ((o)) talking" : "");
    return line;
}

bool muted(int slot)
{
    return listed(slot) && speakers[slot].muted;
}

bool talkingNow(int slot)
{
    if(slot + 1 == cl.viewentity)
    {
        return cap.sending; // (you: while you send)
    }
    return listed(slot) && talking(slot);
}

float playerVolume(int slot)
{
    return listed(slot) ? speakers[slot].volume : 1.f;
}

void setMuted(int slot, bool on)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD || slot >= cl.maxclients)
    {
        return;
    }
    Speaker& sp = speakers[slot];
    sp.muted = on;
    if(sp.muted)
    {
        sp.playing = false;
    }
    Con_Printf("Voice chat: %s %s\n", cl.scores[slot].name, sp.muted ? "muted" : "heard");
}

void setPlayerVolume(int slot, float volume)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD || slot >= cl.maxclients)
    {
        return;
    }
    speakers[slot].volume = za::clamp(volume, 0.f, 4.f);
    Con_Printf("Voice chat: %s at %.2f\n", cl.scores[slot].name, speakers[slot].volume);
}

const char* ownLine()
{
    char* line = menuText.own;
    const int n = sizeof(menuText.own);
    if(vr_voicechat.value == 0.f)
    {
        q_strlcpy(line, "You: voice chat off", n);
    }
    else if(!active())
    {
        q_strlcpy(line, "You: not in a multiplayer game", n);
    }
    else if(cap.sending)
    {
        q_snprintf(line, n, "You: TALKING (level %.0f%%)", za::min(cap.level * 400.f, 100.f));
    }
    else if(!cap.device && !testTone())
    {
        q_strlcpy(line, vr_voicechat_mode.value != 0.f ? "You: voice activation, the microphone opens"
                                                       : "You: push-to-talk (held: the microphone opens)", n);
    }
    else
    {
        q_snprintf(line, n, "You: %s (level %.0f%%)", vr_voicechat_mode.value != 0.f ? "voice activation, waiting" : "push-to-talk",
            za::min(cap.level * 400.f, 100.f));
    }
    return line;
}

void init()
{
    Cmd_AddCommand("+vr_voicechat", pttDown_f);
    Cmd_AddCommand("-vr_voicechat", pttUp_f);
    Cmd_AddCommand("vr_voicechat_mute", mute_f);
    Cmd_AddCommand("vr_voicechat_player_volume", volume_f);
    Cmd_AddCommand("vr_voicechat_players", players_f);
    Cmd_AddCommand("vr_voicechat_report", report_f);
    Cmd_AddCommand("vr_voicechat_reset", resetCounts_f);
    Cmd_AddCommand("vr_voicechat_selftest", selftest_f);
    serverInit();
}

void shutdown()
{
    closeMic(true);
    for(int slot = 0; slot < MAX_SCOREBOARD; slot++)
    {
        if(speakers[slot].dec)
        {
            opus_decoder_destroy(speakers[slot].dec);
            speakers[slot].dec = nullptr;
        }
    }
    if(cap.enc)
    {
        opus_encoder_destroy(cap.enc);
        cap.enc = nullptr;
    }
    if(cap.denoise)
    {
        rnnoise_destroy(cap.denoise);
        cap.denoise = nullptr;
    }
}

void frame()
{
    // (mpfix C5, privacy) Push-to-talk ends whatever ate its release: by hand when the off hand's button is up (Holster
    // Setup, the toolgun's menu: they take every button, its release too), and any when a menu or the console comes up
    // or voice chat is not active (out of the game: a stuck flag sent at once in the next one). (The desktop window's
    // focus is not the headset's: a hidden or unfocused window plays on.)
    if(pttHeld && (!active() || key_dest != key_game || (pttByHand && !input::secondaryPressed(HAND_OFF))))
    {
        pttHeld = false;
        pttByHand = false;
    }
    captureFrame();
    const bool on = cls.state == ca_connected && cl.maxclients > 1;
    const bool inGame = on && cls.signon == SIGNONS;
    for(int slot = 0; slot < MAX_SCOREBOARD; slot++)
    {
        Speaker& sp = speakers[slot];
        if(!on)
        {
            const bool left = cls.state != ca_connected; // (out of the game: every slot's settings gone)
            if(sp.active || sp.playing || (left && (sp.present || sp.muted || sp.volume != 1.f)))
            {
                resetSpeaker(sp, slot, !left);
                sp.present = sp.present && !left;
            }
            continue;
        }
        watchSlot(sp, slot, inGame, slot < cl.maxclients && cl.scores[slot].name[0] != 0);
        pump(sp, slot);
    }
    tellListen();
    indicators();
}

void writeClient(sizebuf_t* buf)
{
    if(!active() || cls.signon != SIGNONS)
    {
        cap.againCount = 0;
        return;
    }
    // [QVR_CLC_VOICE][byte count] x ([short seq][byte flags][byte len][len bytes]): the last datagram's frames again
    // (NetQuake drops a datagram that comes after a newer one: a frame in two survives most reordering and a loss),
    // then the new ones
    int bytes = 2, again = 0, fresh = 0;
    for(; again < cap.againCount && buf->cursize + bytes + frameHeaderBytes + cap.again[again].len <= buf->maxsize; again++)
    {
        bytes += frameHeaderBytes + cap.again[again].len;
    }
    for(; fresh < cap.outCount && again + fresh < maxFramesPerMessage; fresh++)
    {
        const Frame& f = cap.out[(cap.outHead + fresh) % outQueue];
        if(buf->cursize + bytes + frameHeaderBytes + f.len > buf->maxsize)
        {
            break;
        }
        bytes += frameHeaderBytes + f.len;
    }
    if(again + fresh == 0)
    {
        cap.againCount = 0;
        return;
    }
    MSG_WriteByte(buf, QVR_CLC_VOICE);
    MSG_WriteByte(buf, again + fresh);
    for(int i = 0; i < again; i++)
    {
        writeFrame(buf, cap.again[i]);
    }
    cap.againCount = 0;
    for(int i = 0; i < fresh; i++)
    {
        writeFrame(buf, cap.out[cap.outHead]);
        cap.again[cap.againCount++] = cap.out[cap.outHead];
        cap.outHead = (cap.outHead + 1) % outQueue;
        cap.outCount--;
    }
}

void parse()
{
    // [byte slot][byte count] x ([short seq][byte flags][byte len][len bytes])
    const int slot = MSG_ReadByte();
    const int count = MSG_ReadByte();
    if(count == 0 && !msg_badread && slot >= 0 && slot < MAX_SCOREBOARD)
    {
        speakers[slot].lastVoiced = realtime; // (a speaker you muted talks: the server sends his frames no more)
    }
    for(int i = 0; i < count && !msg_badread; i++)
    {
        Frame f;
        if(readFrame(f) && slot >= 0 && slot < MAX_SCOREBOARD && slot + 1 != cl.viewentity)
        {
            receive(slot, f);
        }
    }
}

bool offhandButton(bool pressed)
{
    if(pressed)
    {
        glm::vec3 at, facing;
        if(!active() || vr_voicechat_mode.value != 0.f || vr_notes.value != 0.f || key_dest != key_game ||
            !voicenotes::mouth(at, facing) ||
            glm::distance(hands::current().pos[HAND_OFF], at) > 0.30f * units::metresToUnits())
        {
            return false;
        }
        pttHeld = true;
        pttByHand = true;
        return true;
    }
    if(pttByHand)
    {
        pttHeld = false;
        pttByHand = false;
        return true;
    }
    return false;
}

bool deviceBusy()
{
    return deviceJobsBusy();
}

void writeFrame(sizebuf_t* msg, const Frame& f)
{
    MSG_WriteShort(msg, f.seq);
    MSG_WriteByte(msg, f.flags);
    MSG_WriteByte(msg, f.len);
    SZ_Write(msg, f.data, f.len);
}

bool readFrame(Frame& f)
{
    f.seq = static_cast<za::U16>(MSG_ReadShort());
    f.flags = static_cast<za::U8>(MSG_ReadByte());
    const int len = MSG_ReadByte();
    f.len = static_cast<za::U8>(za::clamp(len, 0, maxFrameBytes));
    for(int k = 0; k < len; k++)
    {
        const int b = MSG_ReadByte();
        if(k < maxFrameBytes)
        {
            f.data[k] = static_cast<za::U8>(b);
        }
    }
    return !msg_badread && len >= 0 && len <= maxFrameBytes;
}

} // namespace qvr::voicechat

// S_LoadSound's in-memory sounds (VR_SndMemory): a speaker's looping ring, silent, 16-bit at the mix's rate.
extern "C" unsigned char* VR_VoicechatSound(const char* name, int* size)
{
    using namespace qvr::voicechat;
    if(ZA_STRNCMP(name, "qvr_voicechat/", 14) != 0 || !shm)
    {
        return nullptr;
    }
    const int rate = shm->speed;
    const int samples = static_cast<int>(ringSeconds * rate);
    const int data = samples * 2;
    const int total = 44 + data;
    unsigned char* w = static_cast<unsigned char*>(VR_HeapMalloc(static_cast<za::SizeT>(total)));
    ZA_MEMSET(w, 0, static_cast<za::SizeT>(total));
    const auto u32 = [&](int at, unsigned v) {
        w[at] = static_cast<unsigned char>(v);
        w[at + 1] = static_cast<unsigned char>(v >> 8);
        w[at + 2] = static_cast<unsigned char>(v >> 16);
        w[at + 3] = static_cast<unsigned char>(v >> 24);
    };
    const auto u16 = [&](int at, unsigned v) {
        w[at] = static_cast<unsigned char>(v);
        w[at + 1] = static_cast<unsigned char>(v >> 8);
    };
    ZA_MEMCPY(w, "RIFF", 4);
    u32(4, static_cast<unsigned>(total - 8));
    ZA_MEMCPY(w + 8, "WAVEfmt ", 8);
    u32(16, 16);
    u16(20, 1);
    u16(22, 1);
    u32(24, static_cast<unsigned>(rate));
    u32(28, static_cast<unsigned>(rate * 2));
    u16(32, 2);
    u16(34, 16);
    ZA_MEMCPY(w + 36, "data", 4);
    u32(40, static_cast<unsigned>(data));
    *size = total;
    return w;
}

extern "C" void VR_WriteVoice(sizebuf_t* buf)
{
    qvr::voicechat::writeClient(buf);
}

extern "C" int VR_ReadVoice(struct client_s* client)
{
    return qvr::voicechat::serverRead(client) ? 1 : 0;
}

extern "C" void VR_WriteVoiceTo(sizebuf_t* msg, struct client_s* client)
{
    qvr::voicechat::serverWrite(msg, client);
}
