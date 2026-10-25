#pragma once

// vr_wounds_internal.hpp -- what vr_wounds.cpp (see vr_wounds.hpp) shares with its diagnostics, vr_wounds_diag.cpp.
// Internal: included by those two alone. What was the file's anonymous namespace and both use is namespace
// wounds::detail: its types and variables here (each variable one object: inline), its functions declared here
// and defined in the file that owns them.

#include "vr_modelmetadata.hpp"
#include "vr_wounds.hpp"
#include "vr_dents.hpp"
#include "vr_bloodstream.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_modelcollide.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"
#include "vr_avatar.hpp"
#include "vr_client.hpp"
#include "vr_held.hpp"

#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Remove.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Vocabulary/Span.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <string.h>


namespace qvr::wounds::detail
{

inline constexpr int splashThings = 24; // things one splash bloodies at most
// Five vec4s, as the paint shader reads them (gl_shaders.h's wound_paint_fragment_shader).
struct Splat
{
    glm::vec4 v[5]{};
};

inline constexpr glm::vec4 paintBlood{1.f, 0.f, 0.f, 0.f};

// The wound skins (vr_wounds 0): per part (0 the off hand, 1 the main hand, 2 the body).
struct SkinPart
{
    double seen{-1e9};   // cl.time of the last look
    double washed{-1e9}; // cl.time it was last under water (-1e9: not washed)
    int washedLevel{0};  // the damage skin it showed then
    int gib{0};          // a gib's blood on it: the skin it shows at least
    int gibEnt{0};       // the gib it held at the last look
};

inline SkinPart skinParts[3];

struct Event
{
    int num{0};
    glm::vec3 org{0.f};
    glm::vec3 dir{0.f, 0.f, -1.f};
    int kind{0};
    int amount{0};
    int extra{0};
    bool clean{false}; // a clean training dummy's (QC QVR_WOUND_CLEAN): nothing painted, its dent alone (vr_dummy_dents)
};

struct Mask
{
    const entity_t* ent{nullptr}; // null: free
    const qmodel_t* model{nullptr};
    int w{0}, h{0};               // its region of its layer (from the corner)
    bool view{false};             // the player's own body or hand: never taken for another
    double lastDrawn{-1e9};       // vr_gametime
    double painted{-1e9};         // vr_gametime of its last paint
    float wetLeft{0.f};           // seconds it may still be drying
    float hotLeft{0.f};           // seconds its embers may still glow
    float heal{0.f};              // blood and char still to take off (a heal), 0..1
    float waterline{0.f};         // its last liquid's surface (drips under it)
    int liquid{0};                // 0 water, 1 slime (the drips' colour)
    double dripNext{0.0};
    double bled{-1e9};            // cl.time blood was last painted on it (its blood in the water it stands in: vr_water_blood)
    double bloodCloud{-1e9};      // cl.time of its last cloud of it there
    float sides[2]{0.f, 0.f};     // your body's: the bones of its right side (bits 0..23, 24..47), painted into the twin
                                  // layer (its arms and legs share their skin's texels, mirrored): 0 none
    bool gear{false};             // a weapon's or a prop's you held: washed, kept dropped and held again (vr_gore_gear)
    int weapon{0};                // a weapon's: its id (QC vr_weaponinst.qc), whatever shows it (0: an entity's own mask)
    int otherSlot{-1};            // chunky (your body's or hand's in the pool): its layer of bloodArray, the blood on it not
                                  // yours (-1 none; a fine mask's is its own layer there)
};

inline GLuint array = 0;

struct LoggedWound
{
    Event ev;
    glm::vec3 rel{0.f}; // its point from the player's origin
    za::U32 seed{0};    // the draws' state it was painted with (re-opened: the same draws, the same wounds where it was)
    float bled{0.f};    // re-opening: the share of its blood back (painted again in steps: vr_gore_reopen_fade)
};

inline za::Vector<LoggedWound> playerWounds; // the player's wounds not healed yet, oldest first

inline double lastWash = -1e9;               // cl.time the player was last being washed

inline bool reopenPending = false;           // washed while hurt: the wounds re-open

inline double reopenStart = -1.0;            // cl.time they began to (-1: not yet)
inline int reopenSmears = 0;                 // the times their blood ran onto the hands since (reopen: two)
void markWashed();                           // being washed now: the wounds re-open after (vr_wounds.cpp wash)

inline za::Vector<Mask> masks; // one a layer

inline constexpr int layerSize = 256;         // a mask's largest side, in texels

inline int fineSize = 0;     // their side in texels (0: none; yours in the pool)

inline GLuint fbo = 0;

// The server's kinds (QC/vr_wounds.qc QVR_WOUND_*).
enum Kind : int
{
    KindClear = -1, // QVR_SVC_WOUNDCLEAR: the entity was removed
    KindShot = 1,
    KindNail = 2,
    KindMelee = 3,
    KindBlast = 4,
    KindBurn = 5,
    KindZap = 6,
    KindLava = 7,
    KindSlime = 8,
    KindLiquid = 9,
    KindWeaponGone = -2, // QVR_SVC_WEAPONGONE: the weapon `num` (its id) is gone: its blood forgotten
};

struct Splash
{
    glm::vec3 at{0.f};
    float reach{0.f};  // units past a thing's size
    float share{0.f};  // the drops' share of a thing's surface at its side facing the blood (falling off with the square of the distance)
    float soak{0.f};   // what lies within `core` of it (a gibbed monster's drops) soaked all over, this much (0 none)
    float core{0.f};
    double until{0.0}; // cl.time
    int done[splashThings]{};
    int doneCount{0};
};

inline za::Vector<Splash> splashes;

inline za::Vector<glm::vec3> spatteredNow;                  // this frame's spatters' centres (a blast's pellets: one)

inline za::Vector<Event> events;

inline int layers = 0;       // the pool's (array): masks 0 .. layers - 1 (chunky mode: the last your body's right side)

inline int paintsTotal = 0;

inline float lastPaintMs = 0.f;

void maskBlood(int layer, int& count, double& sum, int region = -1);
void end();
int acquire(const entity_t* e, bool view, bool create, bool box = false);
int layersOf(int layer, int out[2], int side[2]);
bool ensureTexture();
void washUnder(int layer, entity_t* e, float surface, float amount);
void smearHand(int hand, int blots, float amount, bool theirs = true);
void paint(int layer, entity_t* e, const za::Vector<Splat>& splats);
void bridgeOwnWound(entity_t* source, const za::Vector<Splat>& splats);
[[nodiscard]] bool isGib(int num);
[[nodiscard]] int maskCount();
[[nodiscard]] Splat woundSplat(const glm::vec3& c, float r, const glm::vec3& axis, const glm::vec3& along, float stretch,
    float depth, float facing, float run, const glm::vec4& what);
[[nodiscard]] bool hasOther(int layer);
void attachBlood(GLenum target, int in);
void attach(GLenum target, int layer);
void holstered(entity_t* out[protocol::numHolsters], int weapon[protocol::numHolsters]);
[[nodiscard]] int parkIndex(const entity_t* e);
[[nodiscard]] entity_t* gearOf(int hand);
[[nodiscard]] bool inWorld(const entity_t* e);
void armMarks(int hand, int count, float size, float legs, bool lower);
[[nodiscard]] glm::vec3 originOf(const entity_t& e);
[[nodiscard]] Splat spatterSplat(const glm::vec3& c, float radius, float core, float ref, float share, float drop);
void paintOnYou(const za::Vector<Splat>& splats, const glm::vec3& at, float reach, bool gear, const glm::vec3* blow = nullptr);
[[nodiscard]] float modelRadius(const qmodel_t* m);
void gibStrike(const glm::vec3& at, float r, float amount);
void spatterFrom(const Event& ev, int saw);
[[nodiscard]] glm::vec3 thingCentre(const entity_t& e);
[[nodiscard]] int ownRes();
[[nodiscard]] int bodyRes();
[[nodiscard]] bool isFine(int layer);
[[nodiscard]] int bloodLayerOf(int in);
void wetInfo(); // vr_wounds_info: the wet coat on your body, hands and what they hold (vr_wounds_wet_coat)
void queueInfo(); // vr_wounds_info: the events waiting for a frame's budget (vr_wounds_budget)

} // namespace qvr::wounds::detail
