// vr_arenacards.hpp -- the Survival Arena's perk cards (QC/vr_arena_perks.qc): a boss wave's clear offers the player
// his choice of perks, shown as a row of cards on a panel floating in front of him. The panel is placed in front of
// the head when the offer comes (and again when he comes back from a menu, or after looking away from it for a
// second), then stays where it is in the world. Either hand's laser lights a card; its trigger picks it (as do the
// number keys). The arena waits meanwhile: the QC holds its director until the pick (VR_ARK_HoldThink).
//
// The QC owns the offer; the panel asks it once a frame (VR_ARK_CardSerial, _CardCount, _CardBit, ...: the server's
// VM, a single player game) and answers with VR_ARK_CardsSeen (it shows them) and VR_ARK_CardPick. Tests:
// vr_arena_cards_mock_laser (the main hand's laser on a card whatever its pose), vr_arena_cards_dump.

#pragma once

#include "vr_hands.hpp"
#include "vr_posing.hpp"

namespace qvr::arenacards
{

// The cards are up and drawn in the eyes (in the game: not in a menu or at the intermission).
[[nodiscard]] bool shown();

// The host frame, with this frame's hands: the offer asked of the QC, the panel placed, where the lasers meet it, a
// pick passed on.
void frame(const hands::State& s);

// A controller's button (vr_input.cpp): a trigger pressed with its laser on the panel is the panel's (a card picked
// when on one), and so is its release. True when taken.
[[nodiscard]] bool button(int hand, posing::Button b, bool down);

// A key in the game (keys.c, VR_ArenaCardsKey): 1 .. the cards' count picks that card while they are shown.
[[nodiscard]] bool key(int key, bool down);

// The 2D pass's end (VR_End2D): the panel's image. And its quad in the eye being drawn, with the lasers at it.
void render();
void drawInEye(const hands::State& s);

void registerCommands();

} // namespace qvr::arenacards
