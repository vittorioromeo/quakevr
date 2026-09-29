// vr_meleehud.hpp -- parry stamina and the counter's window as the client sees them (QC VR_Melee_Hud, vr_melee.qc,
// sends them as STAT_QVR_MELEE; docs/vr-port/ROUND21.md, "Stamina on the gadget; the glow"):
// - the wrist gadget's readout (vr_gadget_stamina; drawn by vr_gadget.cpp in its screen's top row);
// - the counter glow (vr_counter_glow, off as shipped): while the window is open, what the hands hold glows gold round
//   its edges (the alias shader's glow, as the force grab's but gold) and sheds golden embers off its surface, both
//   fading as the window closes. An empty hand's fist glows; a hand steadying the other's weapon doesn't.
//   It is drawn where the weapons are drawn, from the latest poses; the server only says how much of the window is left.

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"

namespace qvr::meleehud
{

struct State
{
    bool stamina{false};    // parry stamina is on (vr_parry_stamina)
    float left{1.f};        // the stamina left, 0..1 (0: none, exhausted: the next weapon parry knocks it away)
    bool low{false};        // one more one-handed parry would knock the weapon away
    bool recovering{false}; // it is coming back (no parry for vr_parry_stamina_delay)
    float counter{0.f};     // the counter window's share left, 1 as it opens .. 0 (0: none open)
    bool draining{false};   // hanging from a hold spends it (vr_climb_stamina)
};

// As the server last sent it (all off in a game without the VR progs).
[[nodiscard]] State state();

// Once a frame, before the eyes: the glow's strength, the embers.
void queue(const hands::State& s);

// The counter glow on `e`, 0..1 (0 but on what the hands hold while the window is open).
[[nodiscard]] float entityGlow(const entity_t* e);

} // namespace qvr::meleehud
