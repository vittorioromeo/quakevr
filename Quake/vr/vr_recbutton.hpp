// vr_recbutton.hpp -- what the recorders share (the motion recorder, vr_motion.cpp; the animation recorder,
// vr_animrec.cpp): the controller button that starts and ends a recording (vr_motion_button, vr_ar_button: the same
// choices) and the countdown before a recording starts (spoken in beeps and buzzes, shown in view).

#pragma once

#include "vr_backend.hpp"

#include "Zancle/Base/GetArraySize.hpp"


namespace qvr::recbutton
{

// A record button's choice (VR Settings > Advanced > Motion Recorder, "Record Button"; Debug > Animation Recorder): a
// button of one hand, pressed to start a recording and again to end it; with `withTrigger`, held while that hand's
// trigger is pulled (the button alone does nothing then). Whichever it is, it is the recorder's while armed: the game
// never gets it (only the trigger pulled without the button, for a combination).
struct Binding
{
    int hand;
    bool HandInput::*button;
    bool withTrigger;
    const char* text; // the HUD's instruction
};

inline constexpr Binding bindings[] = {
    {HAND_OFF, &HandInput::stickClick, false, "click the off stick"},          // 0
    {HAND_MAIN, &HandInput::stickClick, false, "click the main stick"},        // 1
    {HAND_MAIN, &HandInput::primary, false, "press A"},                        // 2
    {HAND_MAIN, &HandInput::secondary, false, "press B"},                      // 3
    {HAND_OFF, &HandInput::primary, false, "press X"},                         // 4
    {HAND_OFF, &HandInput::secondary, false, "press Y"},                       // 5
    {HAND_OFF, &HandInput::grip, false, "squeeze the off grip"},               // 6
    {HAND_MAIN, &HandInput::grip, false, "squeeze the main grip"},             // 7
    {HAND_MAIN, &HandInput::secondary, true, "hold B, pull the main trigger"}, // 8
    {HAND_OFF, &HandInput::secondary, true, "hold Y, pull the off trigger"},   // 9
};
inline constexpr int bindingCount = static_cast<int>(za::getArraySize(bindings));

// A cvar's value as a choice (out of range: the first).
[[nodiscard]] inline const Binding& binding(float value)
{
    const int i = static_cast<int>(value);
    return bindings[i >= 0 && i < bindingCount ? i : 0];
}

// The buttons a recorder can take, an index for its "taken" table (a press it took has its release taken too).
inline constexpr bool HandInput::*buttons[] = {
    &HandInput::trigger, &HandInput::grip, &HandInput::primary, &HandInput::secondary, &HandInput::stickClick};
inline constexpr int buttonCount = static_cast<int>(za::getArraySize(buttons));

[[nodiscard]] inline int buttonIndex(bool HandInput::*button)
{
    for(int i = 0; i < buttonCount; i++)
    {
        if(buttons[i] == button)
        {
            return i;
        }
    }
    return -1;
}

// The countdown before a recording starts: `seconds` long from start(); each whole second left is announced once
// (tick() returns it: 3, 2, 1), and tick() returns 0 once, when it runs out (the recording starts then). Measured on
// the game's clock (realtime: the same frames at any frame rate in a test run).
struct Countdown
{
    double endsAt{-1.0};
    int lastAnnounced{0};

    void start(double now, double seconds)
    {
        endsAt = now + (seconds > 0.0 ? seconds : 0.0);
        lastAnnounced = 0;
    }
    void cancel() { endsAt = -1.0; }
    [[nodiscard]] bool running() const { return endsAt >= 0.0; }
    [[nodiscard]] double left(double now) const { return running() ? (endsAt - now > 0.0 ? endsAt - now : 0.0) : 0.0; }

    // -1: nothing to say this frame; n > 0: n seconds left (said once each); 0: it ran out (once; it stops running).
    [[nodiscard]] int tick(double now)
    {
        if(!running())
        {
            return -1;
        }
        const double l = endsAt - now;
        if(l <= 0.0)
        {
            endsAt = -1.0;
            return 0;
        }
        const int whole = static_cast<int>(l + 0.999); // 2.4 s left: "3"; 3 s: "3"
        if(whole != lastAnnounced)
        {
            lastAnnounced = whole;
            return whole;
        }
        return -1;
    }
};

} // namespace qvr::recbutton
