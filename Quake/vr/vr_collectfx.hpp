// vr_collectfx.hpp -- what a hand puts away is drawn going in (vr_collect_fx; ROUND21.md, "Put-away transition"): a box,
// a backpack, a key, a rune, a power-up let go of at a holster (into the pack), an ammo box or a round at the ammo pouch,
// an unarmed hand grenade at the grenade pouch. The server takes it at once, as before (the ammo, the key, the sounds):
// QVR_SVC_COLLECT tells that player's client, which draws a copy of it shrinking (to vr_collect_fx_size) into the
// holster or pouch over vr_collect_fx_time, following the body, then gone. Visual only; other players see it vanish.

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"

namespace qvr::client
{
struct EntityVr;
}

namespace qvr::collectfx
{

// The "hotspots" of the "into the gun" variant (QC vr_defs.qc): QVR_CFX_INTO_GUN, into the gun in the other hand (to the
// player alone); QVR_CFX_INTO_PROP, into the gun lying about that the message's entity is (to everyone who can see it:
// server::sendCollectSeen, from a hand or a loose round by contact alike).
inline constexpr int intoGunHotspot = 240;
inline constexpr int intoPropHotspot = 241;

// QVR_SVC_COLLECT: [byte hand][byte hotspot][short entity][short modelindex][float3 origin][float3 angles].
void parse();

// Each frame (the view's, once): the copies going in added to the scene, placed by the body as drawn (`s`).
void frame(const hands::State& s);

// A new map (the models are the old map's).
void clear();

// One of the copies: its networked scale and offset (the item's, vr_render.cpp), else null.
[[nodiscard]] const client::EntityVr* entityVr(const entity_t* e);

// One of the copies: how much it is shrunk, about its origin (vr_render.cpp); 1 otherwise.
[[nodiscard]] float shrink(const entity_t* e);

[[nodiscard]] int liveCount();

// The copies drawn this frame (copy `i` of maxCopies, null if none): the box ones cast shadows as the boxes do
// (vr_lighting.cpp collectBrushes; the alias ones are in cl_visedicts).
inline constexpr int maxCopies = 8;
[[nodiscard]] entity_t* liveCopy(int i);

} // namespace qvr::collectfx
