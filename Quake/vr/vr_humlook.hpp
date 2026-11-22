#pragma once

// vr_humlook.hpp -- the armed humanoids' recolours by the weapon they spawned with (the author's ask, 2026-11-21: "run-time
// conditional hue/tint recolors on their textures, and also hue/tint recolors on the backpacks"; ARMED_MONSTERS.md,
// "Recolours by weapon"). QC gives each armed humanoid its look as it is armed (vr_humrepl.qc VR_HUM_Look: .vr_hum_look,
// its row times 16 plus its weapon's category; its worn pack the same plus partPack); this sends each entity's look to
// the clients (QVR_SVC_HUMLOOK, reliable, on a change) and fills the alias instance's Recolor rows from the table
// (quakevr/humlook.txt: a colour, strength, lightness and tint per row, category and part).
//
// Which texels (phase 2, his verdict on phase 1: "The recolors look a bit unnatural and also affect the skin/faces of the
// enemies"): a mask per skin (2 bits a Quake texel: clothes, pack or left), made from the skin's 8-bit palette rows as it
// is first drawn (quakevr/humlook_masks.txt: each key's rows; a skin's own rows and texels painted by hand where rows are
// shared), in one storage buffer the shader reads (HumMasks, SSBO 17). The dye keeps each texel's shading (its luminance
// in the row's colour, or a tint of its own colour). Faces and flesh are on rows no mask counts. The Texture Studio
// (vr_humstudio.cpp; Debug > Tools) edits the table and the masks live. Off (vr_humlook 0) nothing is recoloured.

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

struct entity_s;

namespace qvr::humlook
{

inline constexpr int maxKinds = 8;       // armed monster rows (QC VR_AM_*: grunt, enforcer, knight, death knight)
inline constexpr int maxCategories = 16; // weapon categories (QC VR_HUM_CAT_*; 0 none)
inline constexpr int partPack = 4096;    // a look's part bit: its worn backpack (else its body)
inline constexpr int numKeys = 5;        // the masks' keys: the four kinds' bodies, then a worn pack
inline constexpr int keyPack = 4;

// A texel's class: painted (ClassNone: its palette row decides), or a row's (none, clothes, pack).
inline constexpr unsigned char ClassNone = 0, ClassClothes = 1, ClassPack = 2, ClassNever = 3;

// A row of the table (a type, category and part).
struct Spec
{
    bool set{false};
    float rgb[3]{1.f, 1.f, 1.f}; // the colour dyed
    float strength{0.f};         // how much (0..1)
    float light{1.f};            // the lightness's factor
    float tint{0.f};             // 0 the dye, 1 the texel's colour times the colour
};

// A skin's mask: its model's skin and key (a kind's body or a pack), its 8-bit texels, our painted texels.
struct Skin
{
    za::String model;
    int skin{0};
    int key{0};
    int w{0}, h{0};
    za::Vector<unsigned char> texels; // the skin's palette indices (read from the model's file)
    za::Vector<unsigned char> paint;  // per texel: ClassNone (its row decides) or the class painted
    za::Vector<unsigned char> nearFlesh; // per texel: 1 amid flesh (a dyed row's texel left as it is: an arm's highlight)
    bool ownRows{false};              // its own rows' classes instead of its key's
    unsigned char rows[16]{};
    bool read{false}; // its file read (texels empty: none to be had)
    int first{-1};    // its words' first in the buffer (-1: not there)
    bool dirty{true}; // to be written again
    za::Vector<int> cursor; // the Texture Studio's brush: the texels it would change now (shown, never saved)
};

// VR_ServerFrameEnd: each entity's look sent to the clients where it changed.
void serverFrame();
// VR_WriteClientSpawnState: every look (after a clear), for a client coming in.
void serverWriteAll(struct sizebuf_s* msg);
// QVR_SVC_HUMLOOK: an entity's look set or cleared (-1: all cleared).
void parseEvent();
// The console commands (vr_humlook_reload, vr_humlook_info; the studio's).
void registerCommands();
// VR_AliasInstance: `e`'s four Recolor rows (all zero: none).
void instanceRows(const struct entity_s* e, float rows[4][4]);

// The table's and the masks' files (vr_humlook_file, vr_humlook_masks_file: in quakevr/).
[[nodiscard]] const char* tableName();
[[nodiscard]] const char* masksName();
// A row of the table (set or not; nullptr: none such). Changes show on the next frame.
[[nodiscard]] Spec* spec(int kind, int cat, int part);
// A key's palette rows' classes (16; nullptr: none such).
[[nodiscard]] unsigned char* keyRows(int key);
// A texel's class in the mask (0 left, 1 clothes, 2 pack).
[[nodiscard]] unsigned classAt(const Skin& s, int t);
// The mask of `model`'s skin for that key (made and read at first use; nullptr: no 8-bit skin). Set `dirty` after a change.
[[nodiscard]] Skin* skinFor(const qmodel_t* model, int skinnum, int key);
// A look's mask key (-1 none), and the client's look of an entity.
[[nodiscard]] int keyOf(int look);
[[nodiscard]] int lookOf(int num);
[[nodiscard]] za::Vector<Skin>& allSkins();
void reload();
[[nodiscard]] bool saveTable();
[[nodiscard]] bool saveMasks();

// The Texture Studio (vr_humstudio.cpp).
void studioInit();                                 // its commands
void studioFrame();                                // the client frame: its sliders against the table, its strokes
[[nodiscard]] bool studioOverlay();                // the masks shown on the recoloured (vr_humstudio_overlay)
[[nodiscard]] bool studioCursorOn();               // a brush cursor shown on a skin now (its instances drawn with their mask)
void studioDraw2D();                               // the 2D pass: the edited skin's flat view while painting
[[nodiscard]] const char* studioStatus();          // its page's status line
[[nodiscard]] const char* studioRowText(int row);  // a palette row's line on its page (its class on the skin edited)
void studioRowPick(int row);                       // ... cycled: none, clothes, pack
// Its strokes' buttons: VR (a hand's trigger paints, its grip erases, B/Y undoes; true: eaten) and flat (+attack).
[[nodiscard]] bool studioButton(int hand, int button, bool down);
[[nodiscard]] bool studioAttack(bool down);

} // namespace qvr::humlook
