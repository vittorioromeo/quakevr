// vr_slidesound.cpp -- flat-screen play's slide sound (vr_slidesound.hpp; ROUND24.md, "Lying body: slide and prone").

#include "vr_slidesound.hpp"

#include "vr_client.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_flatmove.hpp"

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

struct State
{
    sfx_t* start{nullptr};
    sfx_t* loop{nullptr};
    channel_t* ch{nullptr};
    bool wasSliding{false};
    float level{0.f};       // the scrape's volume now (0..1, before vr_flat_slide_sound)
    double lastTime{-1.0};
    int starts{0};          // slides started (the tests)
    float peakLevel{0.f};   // the loudest this slide (the tests)
};

State st;

[[nodiscard]] channel_t* playing()
{
    channel_t* ch = st.ch;
    return ch && st.loop && ch->sfx == st.loop && ch->entnum == cl.viewentity && ch->entchannel == loopChannel ? ch : nullptr;
}

void stopLoop()
{
    if(st.ch)
    {
        S_MixJoin();
        if(channel_t* ch = playing())
        {
            ch->sfx = nullptr;
        }
    }
    st.ch = nullptr;
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
    const bool sliding = on && flatmove::sliding() && cl.viewentity > 0 && cls.signon == SIGNONS;
    if(sliding && !st.wasSliding)
    {
        st.starts++;
        st.peakLevel = 0.f;
        if(st.start && volume > 0.f)
        {
            vec3_t org;
            VectorCopy(cl_entities[cl.viewentity].origin, org);
            S_StartSound(cl.viewentity, 0, st.start, org, volume, 1.f);
        }
    }
    st.wasSliding = sliding;
    // The scrape follows the speed along the floor: louder and higher the faster, fading as it slows; gone once it ends.
    const float speed = glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]});
    const float want = sliding ? za::clamp(speed / fullSpeed, 0.f, 1.f) : 0.f;
    const float rate = want > st.level ? 1.f / fadeIn : 1.f / fadeOut;
    st.level += za::clamp(want - st.level, -rate * dt, rate * dt);
    st.peakLevel = za::max(st.peakLevel, st.level);
    if(!on || !st.loop || volume <= 0.f || st.level < 0.01f || cl.viewentity <= 0)
    {
        if(!sliding || !on || volume <= 0.f)
        {
            stopLoop();
        }
        if(!on)
        {
            st.level = 0.f;
            st.wasSliding = false;
        }
        return;
    }
    const float pitch = pitchLow + (pitchHigh - pitchLow) * st.level;
    S_MixJoin();
    channel_t* ch = playing();
    if(!ch)
    {
        vec3_t org;
        VectorCopy(cl_entities[cl.viewentity].origin, org);
        S_StartSoundPitch(cl.viewentity, loopChannel, st.loop, org, volume * st.level, 1.f, pitch);
        st.ch = nullptr;
        for(int i = 0; i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i++)
        {
            channel_t& c = snd_channels[i];
            if(c.sfx == st.loop && c.entnum == cl.viewentity && c.entchannel == loopChannel)
            {
                st.ch = &c;
                break;
            }
        }
        ch = st.ch;
        if(!ch)
        {
            return; // (no sound system, or not heard: tried again next frame)
        }
    }
    ch->master_vol = static_cast<int>(volume * st.level * 255.f);
    ch->pitch = pitch;
    ch->resampled = 1; // (its end reckoned again at the new rate: snd_mix.c)
}

float level()
{
    return st.level;
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
