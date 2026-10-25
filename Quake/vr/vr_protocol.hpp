// vr_protocol.hpp -- Quake VR extensions to Ironwail's PROTOCOL_RMQ.
//
// A server running Quake VR progs sets PRFL_QUAKEVR (vr_api.h) in the RMQ protocol flags.
// Both ends then add:
//   - clc_move: a VR block after the vanilla fields (head, hands, muzzles, VR bits, ...);
//   - entity updates: model scale / scale origin / offset / no-rotate in update bits 24..27;
//   - svc_quakevr (39) + sub-command for particles, late precaches, world text, floating texts
//     (damage numbers), haptics, knocks on the drawn hands and spent casings;
//   - an extra "beam id" byte in TE_LIGHTNING1-3 / TE_BEAM, so one entity can own two beams;
//   - entity updates: a weapon prop's weapon id in update bit 28 (QC vr_weaponinst.qc);
//   - VR stats (weapons in both hands, holsters, clips, the hands' and holsters' weapon ids) in stat slots 64+, sent through
//     Ironwail's generic stat channel.

#pragma once

namespace qvr::protocol
{

// Entity update bits (above U_EXTEND2, so only present when bits >= 1 << 24).
inline constexpr int U_QVR_SCALE = 1 << 24;        // 3 floats
inline constexpr int U_QVR_SCALEORIGIN = 1 << 25;  // 3 coords
inline constexpr int U_QVR_OFFSET = 1 << 26;       // 3 coords
inline constexpr int U_QVR_NOROTATE = 1 << 27;     // no data: a rigid body, whose EF_ROTATE model keeps its angles
inline constexpr int U_QVR_WEAPONUID = 1 << 28;    // long, byte: a weapon prop's weapon id (QC vr_weaponinst.qc: its record's), its clip (0-255);
                                                   // id 0: not a weapon prop, the byte its loot beam's (vr_server.cpp lootByte)
inline constexpr int U_QVR_SPIN = 1 << 29;         // no data: drawn spinning as an EF_ROTATE pickup (a weapon pickup drawn as its prop)
inline constexpr int U_QVR_NOMAG = 1 << 30;        // no data: a weapon prop with no magazine in (QC QVR_WPNFLAG_NOMAG: immersive reloading)
// The bits above are full (1 << 31 is the sign: sv_main.c's "bits >= 1 << 24" test); this one is the protocol's spare
// U_UNUSED21, set only by the VR server (VR_EntityUpdateBits) and read only by the VR client (VR_ParseEntityUpdate).
inline constexpr int U_QVR_SSGOPEN = 1 << 21;      // byte: a super shotgun prop broken open (QC QVR_WPNFLAG_SSG_OPEN), its chambers loaded (0-2)

// Server -> client.
inline constexpr int svc_quakevr = 39;

enum SvcQuakeVr : int
{
    QVR_SVC_PARTICLE2 = 1,      // [coord3 org][char3 dir*16][byte preset][short count]
    QVR_SVC_PRECACHE_MODEL = 2, // [short index][string name]
    QVR_SVC_WORLDTEXT_MAKE = 3, // [short handle]
    QVR_SVC_WORLDTEXT_TEXT = 4, // [short handle][string]
    QVR_SVC_WORLDTEXT_POS = 5,  // [short handle][coord3]
    QVR_SVC_WORLDTEXT_ANGLES = 6, // [short handle][float3]
    QVR_SVC_WORLDTEXT_HALIGN = 7, // [short handle][byte 0 left, 1 centre, 2 right]
    QVR_SVC_WORLDTEXT_SCALE = 8,  // [short handle][float]
    QVR_SVC_PRECACHE_SOUND = 9,   // [short index][string name]
    QVR_SVC_HAPTIC = 10,          // [byte hand][float delay][float duration][float frequency][float amplitude]
    QVR_SVC_HANDIMPACT = 11,      // [byte hand][float strength][float3 direction]: the drawn hand is knocked (a parry)
    QVR_SVC_FLOATTEXT = 12,       // [coord3 org][byte3 colour][byte scale * 32][string]: a text rising from a point and fading
    QVR_SVC_EJECT = 13,           // [byte hand][byte kind][byte count][byte flags][byte delay * 100]: spent casings out of a weapon (vr_shells.cpp)
    QVR_SVC_WOUND = 14,           // [short entity][coord3 org][char3 dir*127][byte kind][byte amount][byte extra]: a wound on a model (vr_wounds.cpp)
    QVR_SVC_WOUNDCLEAR = 15,      // [short entity]: an entity sent wounds was removed: its slot's next entity starts clean (reliable)
    QVR_SVC_CATCHBLEND = 16,      // [byte hand][short entity][float3 origin][float3 angles]: a force grab's weapon caught by the hand, from there (vr_drawblend.cpp)
    QVR_SVC_ROPE = 17,            // [short entity][byte beam id][byte count][coord3 first][short3 x (count - 1): eighths of a unit from the one before]: a grappling hook's rope's corners (none: straight), for the beam of that entity and id; sent when they change (vr_ropesim.cpp, vr_rope.cpp); count ropeEnded: that beam's rope ended (its hook's rope is another beam's now)
    QVR_SVC_SHOCK = 18,           // [byte kind][coord3 org][short radius][byte duration * 50]: the lightning gun in water (vr_shock.cpp): 0 the receiving player shocked (to that client only), 1 arcs on a liquid's surface, 2 a burst of arcs in it
    QVR_SVC_FIRED = 19,           // [short entity][byte hand, 255 a monster's gun]: a weapon fired (vr_weaponfx.cpp: its drawn kick, its muzzle flash)
    QVR_SVC_TRACER = 20,          // [short entity][byte hand, 255 a monster's][coord3 from][coord3 to]: a hitscan pellet's line, for a bullet tracer (vr_weaponfx.cpp)
    QVR_SVC_WEAPONGONE = 21,      // [long weapon id]: that weapon is gone (its record freed: nothing has it any more): its blood forgotten (vr_wounds.cpp; reliable)
    QVR_SVC_TIP_MAKE = 22,        // [short handle]: a map tip (func_vr_tip) begins (vr_tips.cpp; QC/vr_tips.qc)
    QVR_SVC_TIP_NAME = 23,        // [short handle][string]: its name (its key in tips_seen.txt, with the map's)
    QVR_SVC_TIP_TEXT = 24,        // [short handle][string]: its text (\n new lines)
    QVR_SVC_TIP_POS = 25,         // [short handle][coord3]: its point in the map
    QVR_SVC_TIP_ENT = 26,         // [short handle][short entity index, -1 a fixed point, -2 gone]: what it follows
    QVR_SVC_TIP_DISTANCE = 27,    // [short handle][float]: how near (units) it must be to show (0: vr_tips_distance)
    QVR_SVC_TIP_SIZE = 28,        // [short handle][float]: its screen's text size (0: vr_tips_size)
    QVR_SVC_TIP_DELAY = 29,       // [short handle][float]: seconds near and seen before it shows (< 0: vr_tips_delay)
    QVR_SVC_TIP_FLAGS = 30,       // [short handle][byte]: its flags (qvr::tips::Flags)
    QVR_SVC_RULES = 31,           // [byte count]([string cvar][float value]) x count: server rules' values (vr_serverrules.cpp; reliable, with the spawn state and on a change)
    QVR_SVC_COLLECT = 32,         // [byte hand][byte hotspot][short entity][short modelindex][float3 origin][float3 angles]: a thing that hand put away at that holster or pouch, drawn going in (vr_collectfx.cpp; to that player only; into a gun lying about, to everyone who can see it)
    QVR_SVC_VIEWMESSAGE = 33,     // [short seconds * 10][string]: a message floating in the middle of the view that long (QC viewmessage: target_vr_message; vr_worldtext.cpp); to that player only, reliable
    QVR_SVC_HITREACT = 34,        // [short entity][coord3 at][char3 dir*127][byte part + 1 (0 unknown)][byte damage][byte share of its health * 255]: a living monster struck, its hit reaction drawn (QC hitreact; vr_hitreact.cpp)
    QVR_SVC_GORESOUND = 35,       // [byte kind][short entity][coord3 at][byte strength * 255]: a gore sound (QC goresound; vr_goresound.cpp)
    QVR_SVC_ARMPOSE = 36,         // [short grunt][short weapon][byte flags][byte wr * 255][coord3 v][short3 fw, up: body's, rifle line's * 32000][byte wo * 255][byte wl * 255][coord3 vl]: an armed grunt's hands and held weapon this server frame (QC VR_AG_SendPose; vr_hitreact.cpp), unreliable
};
inline constexpr int ropeEnded = 255; // QVR_SVC_ROPE's count: the beam's rope ended (no corners follow)

// Client -> server: clc_move VR block button bits.
inline constexpr int QVR_BUTTON_OFFHANDATTACK = 1 << 0; // -> .button3
inline constexpr int QVR_BUTTON_HANDSTRACKED = 1 << 1;  // hands come from real tracking
inline constexpr int QVR_BUTTON_OFFHANDBUSY = 1 << 2;   // the hand holds the flashlight (client-side): no force grab
inline constexpr int QVR_BUTTON_MAINHANDBUSY = 1 << 3;  // (-> QC QVR_VRBITS0_*HAND_BUSY)
inline constexpr int QVR_BUTTON_OFFHANDSECONDARY = 1 << 4;  // the hand's upper face button (Y; B on the main hand) is held,
inline constexpr int QVR_BUTTON_MAINHANDSECONDARY = 1 << 5; // in the game (-> QC QVR_VRBITS0_*HAND_SECONDARY: the grapple's reel)
inline constexpr int QVR_BUTTON_OFFHANDPRIMARY = 1 << 6;  // the hand's lower face button (X; A on the main hand) is held,
inline constexpr int QVR_BUTTON_MAINHANDPRIMARY = 1 << 7; // in the game (-> QC QVR_VRBITS0_*HAND_PRIMARY: the grapple's unreel)
// The VR block's vrBits0 (QC QVR_VRBITS0_* in bits 0-13): bit 15 says this client plays flatscreen (-flatscreenmode:
// vr_flatplay.cpp); the server takes it out (QC's bit 15 is its own) and sets the player's .vr_flatscreen.
inline constexpr int moveFlatplayBit = 1 << 15;

// VR stats (cl.stats / cl.statsf indices).
enum Stat : int
{
    STAT_QVR_FIRST = 64,

    STAT_QVR_WEAPON = STAT_QVR_FIRST, // main hand weapon id (.weapon)
    STAT_QVR_WEAPON2,                 // off-hand weapon id
    STAT_QVR_WEAPONMODEL2,            // model index
    STAT_QVR_WEAPONFRAME2,
    STAT_QVR_WEAPONFLAGS,
    STAT_QVR_WEAPONFLAGS2,
    STAT_QVR_AMMO2,                   // .currentammo2
    STAT_QVR_AMMOCOUNTER,
    STAT_QVR_AMMOCOUNTER2,
    STAT_QVR_WEAPONCLIP,
    STAT_QVR_WEAPONCLIP2,
    STAT_QVR_WEAPONCLIPSIZE,
    STAT_QVR_WEAPONCLIPSIZE2,
    STAT_QVR_HOLSTERWEAPON0,          // 6 each: weapon id, model index, flags, clip
    STAT_QVR_HOLSTERWEAPONMODEL0 = STAT_QVR_HOLSTERWEAPON0 + 6,
    STAT_QVR_HOLSTERWEAPONFLAGS0 = STAT_QVR_HOLSTERWEAPONMODEL0 + 6,
    STAT_QVR_HOLSTERWEAPONCLIP0 = STAT_QVR_HOLSTERWEAPONFLAGS0 + 6,

    STAT_QVR_FGMAIN = STAT_QVR_HOLSTERWEAPONCLIP0 + 6, // force grab: entity * 4 + state (1 aimed, 2 locked, 3 pulled)
    STAT_QVR_FGOFF,
    STAT_QVR_CARRYMAIN, // the entity each hand carries (vr_carry.qc), 0 none: drawn in the hand (vr_held.cpp)
    STAT_QVR_CARRYOFF,
    STAT_QVR_CLIMB,     // the holding hands (vr_climb.cpp): bit 0 off, 1 main; bits 2-7, 8-13 their holds' serials;
                        // bits 14-21, 22-29 their ways out (the yaw towards the drop, in 256ths of a turn)
    STAT_QVR_CLIMBOFFX, // each hand's hold, in eighths of a unit: the drawn hand is put on it
    STAT_QVR_CLIMBMAINX = STAT_QVR_CLIMBOFFX + 3,
    STAT_QVR_MELEE = STAT_QVR_CLIMBMAINX + 3, // parry stamina and the counter's window (QC .vr_melee_hud; vr_meleehud.cpp)
    STAT_QVR_WEAPONUID,  // the main hand's weapon's id (QC vr_weaponinst.qc: its record's; 0 none): its blood (vr_wounds.cpp)
    STAT_QVR_WEAPONUID2, // the off hand's
    STAT_QVR_HOLSTERWEAPONUID0, // 6: each holster's weapon's id (0 none): its blood goes with it, drawn holstered too
    STAT_QVR_AMMOTYPE = STAT_QVR_HOLSTERWEAPONUID0 + 6, // the main hand's ammo type (.currentammo: QC's AID_*; the off hand's: STAT_QVR_AMMO2): the wrist gadget's MAIN tile
    // The server's reloading mode as it applies (vr_reload_mode; 0 without Weapon Mode Immersive): the client draws the
    // ammo pouch, the guns' magazines and the clip on the ammo screens by it, not by its own setting (multiplayer).
    STAT_QVR_RELOADMODE,
    STAT_QVR_POUCHKIND,  // what the ammo pouch gives and shows (QC .vr_pouch_kind: 1 shells, 2 nailgun magazines, 3 super nailgun's, 4 cells)
    STAT_QVR_POUCHCOUNT, // and how many of its ammo are left (.vr_pouch_count): its contents as drawn, its counter
    // The stealth AI's gem on the wrist gadget (QC .stl_hud, vr_stealth.qc): how visible you are (0..100) plus 1000 times
    // how loud your last noise was (0..100); -1 with the stealth AI off.
    STAT_QVR_STEALTH,
    STAT_QVR_FOEGRAB,     // the hands holding an enemy (vr_foegrab.cpp): bit 0 the off hand, 1 the main
    STAT_QVR_FOEGRABOFFX, // each holding hand's palm's place on the enemy, in eighths of a unit: the drawn hand is put there
    STAT_QVR_FOEGRABMAINX = STAT_QVR_FOEGRABOFFX + 3,
    // AD support: Arcane Dimensions' row on the wrist gadget (QC vr_ad_items.qc AD_HudUpdate): what is held (bits 0-5
    // the artifacts Lava Shield, Air Tank, Blast Belt, Jump Boots, Sharp Shooter, Nail Piercer; 6-8 the debuffs
    // burning, poisoned, bleeding; 9-12 the custom keys), the custom keys' HUD skins (4 bits each), the soonest
    // artifact's seconds left (0: none ends).
    STAT_QVR_ADHUD = STAT_QVR_FOEGRABMAINX + 3,
    STAT_QVR_ADKEYS,
    STAT_QVR_ADTIME,
    // AD support: Arcane Dimensions' screen tint (QC vr_ad_player.qc VR_AD_Tint, AD's v_cshift): its colour (65536 red
    // + 256 green + blue), its density (0-255) plus 256 for a cinematic camera's fade (vr_ad.cpp VR_AD_TintBlend).
    STAT_QVR_ADTINT,
    STAT_QVR_ADTINTDEN,
    // AD support: the wrist gadget's Arcane Dimensions page (QC vr_ad_items.qc AD_HudUpdate; vr_gadget.cpp adPage): each
    // artifact's seconds left (8 bits each, 255 no end: the Lava Shield, Air Tank, Blast Belt; the Jump Boots, Sharp
    // Shooter, Nail Piercer), the debuffs (the poison and the bleeding still to come, the burning's seconds left: 8
    // bits each), the carried weapons' upgrades (3 bits a weapon, 1 the Shadow Axe, 2 the Widowmaker, 4 the Plasma Gun:
    // the main hand, the off hand, then the holsters).
    STAT_QVR_ADART1,
    STAT_QVR_ADART2,
    STAT_QVR_ADDEBUFF,
    STAT_QVR_ADUPGRADES,
    // AD support: the sky fog AD set (QC vr_ad_bridge.qc VR_AD_SkyFog, AD's r_skyfog): its density (0-1) + 1; 0 the map's
    // own (gl_fog.c: VR_AD_SkyFog).
    STAT_QVR_ADSKYFOG,
    // Incandescent blades (QC vr_hotblade.qc VR_HotBlade_Hud; vr_meleehud.cpp): how the hands' weapons glow (0-255 each:
    // the off hand's bits 0-7, the main hand's 8-15) and the holsters' (0-15 each: holster i bits 4i..4i+3).
    STAT_QVR_HOTBLADE,
    STAT_QVR_HOTBLADEHOLSTER,
    // Magnetic holsters (QC vr_magholster.qc; vr_magholster.cpp): 6, each holster's bound weapon's id (0: none bound).
    STAT_QVR_MAGBIND0,
    // The hands carrying a thing to put away (QC .vr_carry_items, VR_Carry_IsCollectable: an ammo box, a health box, a
    // backpack): bit 0 the off hand, 1 the main. Their zones are larger (vr_holster_zone_item_*; vr_holsterzones.cpp).
    STAT_QVR_CARRYITEM = STAT_QVR_MAGBIND0 + 6,
    STAT_QVR_END

};

inline constexpr int numHolsters = 6;

} // namespace qvr::protocol
