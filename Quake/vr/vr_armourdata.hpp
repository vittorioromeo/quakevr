#pragma once

// vr_armourdata.hpp -- the armour's hand tweaks (quakevr/data/armour.txt; docs/vr-port/ARMOUR_STUDIO.md): per wearer kind and
// per piece, overrides laid over what the generator makes (vr_agarmor.cpp buildShell): the piece moved, turned, scaled, stood
// off the skin, started and ended further along its bone, its rows. An empty file is the generator's armour exactly (the same
// shells, byte for byte: nothing is applied where nothing is set). Read at start, again when the file changes on disk
// (vr_armour_watch, once a second) or on vr_armour_reload; the Armour Studio (vr_armourstudio.hpp) edits it live and saves it.
//
// Kinds: grunt (progs/vrgrunt.mdl, progs/soldier.mdl), enforcer (vrenforcer, enforcer), knight (vrknight), death_knight
// (vrhknight), player (every progs/vrbody*.mdl build); or a model by its path ("progs/vrbody_bare.mdl"), over its kind's. Parts:
// cuirass, helmet, pauldron, bracer, thigh, shin (the limbs both sides, mirrored: the left's values, the right's the mirror
// image), pauldron_l / _r ... (one side's own, over the both-sides values), and each limb part's sleeve over its joint
// (pauldron_sleeve, bracer_sleeve, thigh_sleeve, shin_sleeve, and their _l / _r). Each value set alone is laid over: the
// most specific wins value by value (the model's side, the model's both sides, the kind's side, the kind's both sides).

#include <stdint.h>

namespace qvr::armourdata
{

// A part's values (Field order; a set's bits in Override::has).
enum Field : int
{
    OffsetX,  // units along the model's forward, left and up (its rest pose), the whole piece
    OffsetY,
    OffsetZ,
    Pitch,    // degrees about the model's left axis, up axis and forward axis, round the piece's middle
    Yaw,
    Roll,
    Scale,    // the piece's size round its middle (1 as made)
    Standoff, // units further off the skin (a grid piece's radii; a sleeve or the helmet pushed out from its middle)
    Start,    // units the plates' first row starts further along the piece (towards its far end: the waist, the hand, the foot)
    End,      // units the plates' last row ends further along it
    Rows,     // rows of plates (0: the generator's; the cuirass counts its collar row)
    TaperStart, // the plate rows' size round the piece's line at its first row (1 as made), lerped row by row to ...
    TaperEnd,   // ... this at its last row (a thigh wider at the hip, narrower at the knee: 1.15 and 0.9)
    Leather,    // a sleeve's leather drawn (1, as made) or not at all (0): the sleeve parts' alone
    Folds,      // a joint's leather's bellows: its folds (0-4; not set: vr_agarmor_folds), the sleeve parts' alone (the upper's first)
    FoldDepth,  // ... how deep they stand on the inside of a bend (units; not set: vr_agarmor_fold_depth)
    Hero,       // its hero plates (a row's plates merged into one: the breastplate, the pauldrons' top row, the elbows' and knees'
                // cops; vr_agarmor_hero): 1 (as made) or 0, every plate its own; the cuirass's and the limbs' plates
    Sag,        // a beast's leather junction's middle dropped this far (units, below 0 raised; vr_beastarmour.cpp): its sleeve parts'
    ScaleX,     // its size along the model's forward, left and up (its rest pose), round its middle, times Scale (1 as made):
    ScaleY,     // a piece made wider than it is deep, or longer (studioui, 2026-11-27)
    ScaleZ,
    NumFields
};
inline constexpr const char* fieldNames[NumFields] = {"offset_x", "offset_y", "offset_z", "pitch", "yaw", "roll", "scale",
    "standoff", "start", "end", "rows", "taper_start", "taper_end", "leather", "folds", "fold_depth", "hero", "sag",
    "scale_x", "scale_y", "scale_z"};
// A field's value where nothing is set.
[[nodiscard]] constexpr float fieldDefault(int field)
{
    return field == Scale || field == ScaleX || field == ScaleY || field == ScaleZ || field == TaperStart || field == TaperEnd || field == Leather || field == Hero ? 1.f : 0.f; // (Sag 0)
}

enum Part : int
{
    PartCuirass,
    PartHelmet,
    PartPauldron,
    PartBracer,
    PartThigh,
    PartShin,
    PartPauldronSleeve,
    PartBracerSleeve,
    PartThighSleeve,
    PartShinSleeve,
    // The beasts' alone (vr_beastarmour.hpp: a beast's pieces are the slots 0-9, its cuirass, helmet, pauldrons, bracers and
    // thighs those parts; slots 8 and 9 body plates of its own: the rottweiler's croup and collar, the ogre's back plate and
    // faulds...). Each beast piece's sleeve is its leather junction to the piece it joins (vr_beastarmour.cpp).
    PartCuirassSleeve,
    PartHelmetSleeve,
    PartPlate8,
    PartPlate9,
    PartPlate8Sleeve,
    PartPlate9Sleeve,
    // A beast's part of a slot on another bone with its own tweaks (beastarmour.txt's last word; ROUND26.md, "beastfit"): the
    // scrag's tail (parts of his belt's slot 8) and its junctions. The ogre's shins (with their knee cops) are PartShin, by side.
    PartTail,
    PartTailSleeve,
    // The scrag's lower body's back half (ROUND26.md, "scragsplit"): his slot 8's plate in two, its front the slot's own plate8
    // ("Plate 8 Front" on him), its back this one, each with its junction.
    PartPlate8Back,
    PartPlate8BackSleeve,
    NumParts
};
inline constexpr const char* partNames[NumParts] = {"cuirass", "helmet", "pauldron", "bracer", "thigh", "shin", "pauldron_sleeve",
    "bracer_sleeve", "thigh_sleeve", "shin_sleeve", "cuirass_sleeve", "helmet_sleeve", "plate8", "plate9", "plate8_sleeve", "plate9_sleeve",
    "tail", "tail_sleeve", "plate8_back", "plate8_back_sleeve"};
inline constexpr int humanParts = PartShinSleeve + 1; // (the humanoids' parts: the first ten)
// Whether a part is a limb's (both sides, mirrored, or one side's own), and whether a sleeve (leather).
[[nodiscard]] constexpr bool limbPart(int part)
{
    return part >= PartPauldron && part <= PartShinSleeve;
}
[[nodiscard]] constexpr bool sleevePart(int part)
{
    return (part >= PartPauldronSleeve && part <= PartHelmetSleeve) || part == PartPlate8Sleeve || part == PartPlate9Sleeve ||
        part == PartTailSleeve || part == PartPlate8BackSleeve;
}

enum Side : int
{
    SideBoth,
    SideLeft,
    SideRight,
    NumSides
};

enum Kind : int
{
    KindGrunt,
    KindEnforcer,
    KindKnight,
    KindDeathKnight,
    KindPlayer,
    KindAdKnight, // AD's death guard and crossbow knight (progs/vrdguard.mdl, vrdcrossbow.mdl): one set of tweaks for both while
                  // their authored rigs have the same bones (adShared); else each its model's own (by its path)
    NumKinds
};
inline constexpr const char* kindNames[NumKinds] = {"grunt", "enforcer", "knight", "death_knight", "player", "ad_knight"};

struct Override
{
    float v[NumFields]{};
    uint32_t has{0}; // (1 << Field)
};

// A piece's size along the model's forward, left and up (its rest pose): scale times scale_x / _y / _z, each set one
// clamped to 0.2 .. 3 (unset: 1). Whether any is set and not 1 (the piece's shape changed by its size).
inline bool scaleAxes(const Override& o, float out[3])
{
    const auto value = [&](int f) {
        const float x = ((o.has >> static_cast<unsigned>(f)) & 1u) != 0u ? o.v[f] : 1.f;
        return x < 0.2f ? 0.2f : x > 3.f ? 3.f : x;
    };
    const float s = value(Scale);
    bool changed = false;
    for(int i = 0; i < 3; i++)
    {
        out[i] = s * value(ScaleX + i);
        changed = changed || out[i] != 1.f;
    }
    return changed;
}

// The kind a wearer's model is (-1 none).
[[nodiscard]] int kindOf(const char* model);
// Whether AD's two knights share their tweaks (KindAdKnight): both authored rigs read and their bones the same (names,
// parents, joints; pivots and limits may differ, the tweaks being relative to each body's pieces).
[[nodiscard]] bool adShared();

// A beast's piece (vr_beastarmour.hpp: slot 0-9) as a part and side: slot 0 the cuirass, 1 the helmet, 2-7 the limb parts
// (left, right), 8 and 9 plate8 and plate9; `sleeve` its leather junction.
void beastPart(int slot, bool sleeve, int& part, int& side);
// What applies to a beast's piece `slot` of `model` (its own path's entry; the limbs' both-sides values mirrored), as resolve.
bool resolveBeast(const char* model, int slot, bool sleeve, Override& out);
// What applies to a beast's part of a slot with a part of its own (PartTail, PartShin: beastarmour.txt's last word) on `side`
// (a limb part's left or right, SideBoth else; the both-sides values mirrored on the right); `sleeve` its junction.
bool resolveBeastPart(const char* model, int part, int side, bool sleeve, Override& out);
// A beast part's sleeve (its junction's part): PartShin's PartShinSleeve, PartTail's PartTailSleeve, PartPlate8Back's
// PartPlate8BackSleeve; itself if one already.
[[nodiscard]] int beastSleeve(int part);
// A hash of everything that applies to beast `model`'s pieces (0: nothing).
[[nodiscard]] uint32_t beastSignature(const char* model);
// What applies to agarmor piece `piece` (vr_agarmor.hpp Piece) of `model`: its plates, or its sleeve. Values not set are 0
// (fieldDefault: Scale, the tapers and Leather 1); `has` which are set. False: nothing set.
bool resolve(const char* model, int piece, bool sleeve, Override& out);
// A hash of everything that applies to `model` (0: nothing).
[[nodiscard]] uint32_t signature(const char* model);
// Bumped whenever the table changes (read, edited, undone).
[[nodiscard]] int generation();

// The table's entry for a kind (by name, made if `make`): its own values, by part and side. Null: none.
[[nodiscard]] Override* entry(const char* name, int part, int side, bool make);
// A kind's entry cleared (part -1: every part; side -1 every side).
void clear(const char* name, int part, int side);
// The table changed by hand (the studio): generation bumped, the armour made again (agarmor::overridesChanged).
void changed();
// The studio's undo: the whole table kept (at most 64 steps), and put back (false: nothing kept).
void pushUndo();
bool popUndo();
[[nodiscard]] int undoCount();

// The file: its path on disk; read (false: none there, the table empty), written (false: failed).
[[nodiscard]] const char* filePath();
bool load();
bool save();
// How many parts have a value set (the studio's status line).
[[nodiscard]] int countSet();

void init();  // the commands (vr_armour_reload, vr_armour_print), the first read
void frame(); // vr_armour_watch: the file read again when it changed on disk

} // namespace qvr::armourdata
