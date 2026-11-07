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
    NumFields
};
inline constexpr const char* fieldNames[NumFields] = {"offset_x", "offset_y", "offset_z", "pitch", "yaw", "roll", "scale",
    "standoff", "start", "end", "rows", "taper_start", "taper_end", "leather", "folds", "fold_depth"};
// A field's value where nothing is set.
[[nodiscard]] constexpr float fieldDefault(int field)
{
    return field == Scale || field == TaperStart || field == TaperEnd || field == Leather ? 1.f : 0.f;
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
    NumParts
};
inline constexpr const char* partNames[NumParts] = {"cuirass", "helmet", "pauldron", "bracer", "thigh", "shin", "pauldron_sleeve",
    "bracer_sleeve", "thigh_sleeve", "shin_sleeve"};

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
    NumKinds
};
inline constexpr const char* kindNames[NumKinds] = {"grunt", "enforcer", "knight", "death_knight", "player"};

struct Override
{
    float v[NumFields]{};
    uint32_t has{0}; // (1 << Field)
};

// The kind a wearer's model is (-1 none).
[[nodiscard]] int kindOf(const char* model);
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
