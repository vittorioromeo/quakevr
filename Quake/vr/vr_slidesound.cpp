// vr_slidesound.cpp -- flat-screen play's slide sound (vr_slidesound.hpp; ROUND24.md, "Lying body: slide and prone").

#include "vr_slidesound.hpp"

#include "vr_client.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_flatmove.hpp"
#include "vr_geom.hpp"
#include "vr_mpavatar.hpp"
#include "vr_slide.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/glm.hpp>

namespace qvr::slidesound
{

namespace
{

constexpr const char* startName = "vr/slide_start.wav";
constexpr const char* loopName = "vr/slide_loop.wav";
constexpr float fullSpeed = 480.f;   // units/s: the scrape at its loudest and highest
constexpr float fadeIn = 0.08f;      // seconds the scrape takes to come up
constexpr float fadeOut = 0.18f;     // ... and to die away once the slide ends
constexpr float pitchLow = 0.8f;     // the scrape's rate slow .. fast
constexpr float pitchHigh = 1.12f;
constexpr int loopChannel = 7;       // the player's own channel for it (one sound at a time on it: it replaces)

// One player's scrape: yours (on your view entity), or another player's (syncfix5: on his entity, from his copy's slide,
// the server's word in his avatar; heard where he is).
struct Voice
{
    channel_t* ch{nullptr};
    bool wasSliding{false};
    float level{0.f};       // the scrape's volume now (0..1, before vr_flat_slide_sound)
};

struct State
{
    sfx_t* start{nullptr};
    sfx_t* loop{nullptr};
    Voice own;
    double lastTime{-1.0};
    int starts{0};          // slides started (the tests)
    float peakLevel{0.f};   // the loudest this slide (the tests)
};

State st;

// The other players' (syncfix5): their voices, where each was last frame and his speed along the floor from it.
struct Other
{
    Voice v;
    glm::vec3 lastAt{0.f};
    double lastTime{-1.0};
    float speed{0.f};
};
Other others[MAX_SCOREBOARD + 1];

[[nodiscard]] channel_t* playing(const Voice& v, int ent)
{
    channel_t* ch = v.ch;
    return ch && st.loop && ch->sfx == st.loop && ch->entnum == ent && ch->entchannel == loopChannel ? ch : nullptr;
}

void stopLoop(Voice& v, int ent)
{
    if(v.ch)
    {
        S_MixJoin(); // (a read of the channels on the main thread: the pool's mix done first, S_MixKick)
        if(channel_t* ch = playing(v, ent))
        {
            ch->sfx = nullptr;
        }
    }
    v.ch = nullptr;
}

// Voice `v` of entity `ent` (at `org`, going `speed` along the floor) this frame: the scuff as a slide starts, the scrape
// following the speed while it lasts, dying away after. `on` false: none (the scrape stops). True: a slide started.
bool drive(Voice& v, int ent, bool on, bool sliding, float speed, const glm::vec3& org, float volume, float dt)
{
    const bool started = sliding && !v.wasSliding;
    if(started && st.start && volume > 0.f)
    {
        vec3_t o{org.x, org.y, org.z};
        S_StartSound(ent, 0, st.start, o, volume, 1.f);
    }
    v.wasSliding = sliding;
    // The scrape follows the speed along the floor: louder and higher the faster, fading as it slows; gone once it ends.
    const float want = sliding ? za::clamp(speed / fullSpeed, 0.f, 1.f) : 0.f;
    const float rate = want > v.level ? 1.f / fadeIn : 1.f / fadeOut;
    v.level += za::clamp(want - v.level, -rate * dt, rate * dt);
    if(!on || !st.loop || volume <= 0.f || v.level < 0.01f || ent <= 0)
    {
        if(!sliding || !on || volume <= 0.f)
        {
            stopLoop(v, ent);
        }
        if(!on)
        {
            v.level = 0.f;
            v.wasSliding = false;
        }
        return started;
    }
    const float pitch = pitchLow + (pitchHigh - pitchLow) * v.level;
    S_MixJoin(); // (a read of the channels on the main thread: the pool's mix done first, S_MixKick)
    channel_t* ch = playing(v, ent);
    if(!ch)
    {
        vec3_t o{org.x, org.y, org.z};
        S_StartSoundPitch(ent, loopChannel, st.loop, o, volume * v.level, 1.f, pitch);
        v.ch = nullptr;
        for(int i = 0; i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i++)
        {
            channel_t& c = snd_channels[i];
            if(c.sfx == st.loop && c.entnum == ent && c.entchannel == loopChannel)
            {
                v.ch = &c;
                break;
            }
        }
        ch = v.ch;
        if(!ch)
        {
            return started; // (no sound system, or not heard: tried again next frame)
        }
    }
    ch->master_vol = static_cast<int>(volume * v.level * 255.f);
    ch->pitch = pitch;
    ch->resampled = 1; // (its end reckoned again at the new rate: snd_mix.c)
    geom::store(org, ch->origin); // (another player's: it goes with him)
    return started;
}

// The other players' slides (syncfix5): his copy's (the server's slide in his avatar: mpslide), heard from him as he hears
// his own; not your own entity (yours above), not a player whose copy is not drawn.
void othersFrame(float volume)
{
    for(int p = 1; p <= MAX_SCOREBOARD; p++)
    {
        Other& o = others[p];
        const mpavatar::Avatar* a = p != cl.viewentity && p <= cl.maxclients && cls.signon == SIGNONS ? mpavatar::of(p) : nullptr;
        const bool drawn = a && mpavatar::drawnNow(p);
        if(!drawn && !o.v.ch && !o.v.wasSliding && o.v.level <= 0.f)
        {
            o.lastTime = -1.0;
            continue;
        }
        const glm::vec3 at = geom::vec(cl_entities[p].origin);
        const float dt = o.lastTime >= 0.0 ? za::clamp(static_cast<float>(cl.time - o.lastTime), 0.f, 0.1f) : 0.f;
        if(dt > 0.f)
        {
            const float now = glm::length(glm::vec2{at.x - o.lastAt.x, at.y - o.lastAt.y}) / dt;
            o.speed += (za::min(now, 2.f * fullSpeed) - o.speed) * za::min(1.f, dt * 12.f); // (his place steps at the snapshots: eased)
        }
        o.lastAt = at;
        o.lastTime = cl.time;
        (void)drive(o.v, p, drawn, drawn && a->sliding, o.speed, at, volume, dt);
    }
}

} // namespace

void prepare()
{
    st.start = S_PrecacheSound(startName);
    st.loop = S_PrecacheSound(loopName);
}

void frame(bool on)
{
    const float dt = st.lastTime >= 0.0 ? za::clamp(static_cast<float>(cl.time - st.lastTime), 0.f, 0.1f) : 0.f;
    st.lastTime = cl.time;
    const float volume = za::clamp(vr_flat_slide_sound.value, 0.f, 1.f);
    const bool sliding = on && (flatmove::sliding() || slide::headsetSliding()) && cl.viewentity > 0 && cls.signon == SIGNONS;
    const float speed = glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]});
    const glm::vec3 org = cl.viewentity > 0 ? geom::vec(cl_entities[cl.viewentity].origin) : glm::vec3{0.f};
    if(sliding && !st.own.wasSliding)
    {
        st.starts++;
        st.peakLevel = 0.f;
    }
    (void)drive(st.own, cl.viewentity, on, sliding, speed, org, volume, dt);
    st.peakLevel = za::max(st.peakLevel, st.own.level);
    othersFrame(volume);
}

float level()
{
    return st.own.level;
}

int starts()
{
    return st.starts;
}

float peakLevel()
{
    return st.peakLevel;
}

} // namespace qvr::slidesound
