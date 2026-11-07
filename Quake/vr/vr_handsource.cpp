// vr_handsource.cpp -- see vr_handsource.hpp.

#include "vr_handsource.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"


namespace qvr::handsource
{
namespace
{

struct HandMark
{
    Source source{Source::Tracked};
    const char* why{"controller"};
};

// This frame's marks, and the last frame's (vr_handsource_debug prints a change).
HandMark marks[HAND_COUNT];
HandMark printed[HAND_COUNT];

[[nodiscard]] bool validHand(int h)
{
    return h >= 0 && h < HAND_COUNT;
}

} // namespace

void beginFrame(bool tracked)
{
    for(int h = 0; h < HAND_COUNT; h++)
    {
        if(vr_handsource_debug.value != 0.f &&
            (marks[h].source != printed[h].source || marks[h].why != printed[h].why))
        {
            Con_Printf("handsource: %s hand %s (%s)%s\n", h == HAND_MAIN ? "main" : "off", name(marks[h].source),
                marks[h].why, gestures(h) ? "" : ": no gestures");
        }
        printed[h] = marks[h];
        marks[h] = tracked ? HandMark{Source::Tracked, "controller"} : HandMark{Source::Synthesized, "flat-screen pose"};
        if(vr_handsource_force.value != 0.f)
        {
            marks[h] = HandMark{Source::Synthesized, "forced (vr_handsource_force)"};
        }
    }
}

void mark(int h, Source s, const char* why)
{
    if(validHand(h) && static_cast<int>(s) >= static_cast<int>(marks[h].source))
    {
        marks[h] = HandMark{s, why};
    }
}

Source source(int h)
{
    return validHand(h) ? marks[h].source : Source::Tracked;
}

const char* why(int h)
{
    return validHand(h) ? marks[h].why : "";
}

bool gestures(int h)
{
    return source(h) != Source::Synthesized || vr_handsource_gate.value == 0.f;
}

za::U8 moveBits()
{
    za::U8 bits = 0;
    if(!gestures(HAND_OFF))
    {
        bits |= 1; // QC VR_HANDSYNTH_OFF
    }
    if(!gestures(HAND_MAIN))
    {
        bits |= 2; // VR_HANDSYNTH_MAIN
    }
    return bits;
}

const char* name(Source s)
{
    switch(s)
    {
    case Source::Tracked: return "tracked";
    case Source::Synthesized: return "synthesized";
    case Source::Action: return "action";
    }
    return "?";
}

} // namespace qvr::handsource
