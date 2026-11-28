// vr_reloadsys.hpp -- the procedural reloads' data (reloadsys, his words 2026-11-28: "a keyframe based system, and I can go
// through each keyframe and tweak the position of the hand and the timing ... make sure that the target position of the hand
// takes into account where the shells or the magazine ... should go"): one generic, data-driven reload per gun, a sequence of
// keyframes played by flat-screen play's procedural reload (vr_flatprocreload.cpp) and edited by the Reload Studio
// (vr_reloadstudio.cpp). Kept in quakevr/data/reloads.txt (vr_reload_file), built-in defaults where it has no entry.
//
// A reload: the item the free hand carries (a taped pair of shells; later a magazine, a rocket...), its destination on the gun
// (the loading port, the super shotgun's chambers; later a receiver, the muzzle, a cylinder's slots), the item's turn at its
// destination in the gun's frame (the backwards solve's target), and its keyframes. A keyframe: its time and ease, where the
// free hand goes (its pose, the ammo pouch, the destination's mouth, the destination, or held where it is) with its offset and
// turn there, the arc of its way, the gun hand's pose (the tilt that presents the destination, its move), a gun action at its
// start (take from the pouch, open the gun, close it), a condition (the gun closed / loaded), and whether it belongs to the
// reload's end (played when it is over or cancelled). The keys from `loopFrom` to `loopTo` repeat until the gun is full.
//
// The free hand's place at the destination is solved backwards each frame (vr_flatprocreload.cpp spotsOf): the item's pose
// there (the port measured on the gun's game pose, the item's turn on the gun's) -> the item's place in the hand (held::
// placeInHand, the server's own) -> the hand. A key's hand offset and turn at the destination go on top in the gun's frame.
// Mirrored for the other side: the free hand's values (offsets' right, turns' yaw and roll) by the free hand's side, the gun's
// by the gun hand's (as a right-handed player's: the free hand the left).

#pragma once

#include <glm/glm.hpp>

namespace qvr::reloadsys
{

// What the free hand carries (later phases: magazines, rockets and grenades, a revolver's cylinder rounds).
enum class Item : int
{
    Shell,
    Pair,
    Magazine,
    Rocket,
    Cylinder,
    Count
};
// Where it goes on the gun.
enum class Dest : int
{
    Port,
    Chamber,
    Receiver,
    Muzzle,
    CylinderSlot,
    Count
};
// Where a key takes the free hand.
enum class Target : int
{
    Rest,  // its own pose (off it by the key's offset, in the body's frame)
    Pouch, // the ammo pouch (offset in the body's frame, turned in it)
    Mouth, // the item `approach` units out of its destination's opening (solved backwards)
    Dest,  // the item at its destination (solved backwards)
    Hold,  // where it is (a gun-only key)
    Count
};
// A gun action at the key's start (Take: through the key, once the hand is at the pouch).
enum class Action : int
{
    None,
    Take,   // impulse 99: the pouch's take (VR_Reload_Take); the key waits for the item in the hand
    Insert, // the key waits at its end for the item to go in (VR's port test), its failsafe after
    Open,   // impulse 107: the gun opened (the super shotgun broken open, its spent shells out)
    Close,  // impulse 108: the gun closed (the super shotgun snapped shut)
    Eject,  // (later: a magazine let out; nothing yet)
    Count
};
// When a key plays (skipped otherwise).
enum class Cond : int
{
    Always,
    Closed, // the gun shut (the super shotgun not broken open)
    Opened,
    Loaded, // rounds in the gun
    Count
};
enum class Ease : int
{
    Linear,
    Smooth,
    In,
    Out,
    Count
};

inline constexpr int maxKeys = 24;
inline constexpr int nameSize = 24;

struct Key
{
    char name[nameSize]{"key"};
    float time{0.2f};    // s: the free hand's move
    float gunTime{0.f};  // s: the gun hand's (<= 0: the key's time)
    Ease ease{Ease::Smooth};
    Target target{Target::Hold};
    glm::vec3 handOfs{0.f}; // units: forward, right, up (Rest / Pouch: the body's frame; Mouth / Dest: the gun's)
    glm::vec3 handRot{0.f}; // degrees: pitch, yaw, roll (Pouch: the hand's on the body's; Mouth / Dest: the item's on its destination's, the gun's frame)
    float arc{0.f};      // units the way bows up and out
    float approach{0.f}; // units out of the destination's opening (Mouth)
    glm::vec3 gunRot{0.f}; // degrees: the gun's tilt (pitch + muzzle down, yaw in, roll)
    glm::vec3 gunOfs{0.f}; // units: forward, in (towards the body's middle), up (the body's frame)
    Action action{Action::None};
    Cond cond{Cond::Always};
    bool finish{false};  // the reload's end (played when it is over or cancelled)
};

struct Reload
{
    char gun[nameSize]{""}; // "shotgun"
    int weapon{0};          // its weapon ID (WID_*)
    Item item{Item::Pair};
    Dest dest{Dest::Port};
    glm::vec3 itemRot{0.f}; // degrees: the item's turn at its destination in the gun's frame
    int loopFrom{-1}, loopTo{-1};
    int keyCount{0};
    Key keys[maxKeys];
};

// The commands (vr_reloadsys_reload, _save, _list, _set) and the file read.
void init();
// The reload of weapon `weapon` (WID_*), nullptr none. Of the table: edits (the studio's) show at once.
[[nodiscard]] Reload* forWeapon(int weapon);
[[nodiscard]] int count();
[[nodiscard]] Reload& at(int i);
// The built-in reload of gun `i` (the studio's Reset).
[[nodiscard]] Reload builtIn(int weapon);
// The file (vr_reload_file, under quakevr/): read (true: found; the built-ins where it has no entry) and written.
bool load();
bool save();
[[nodiscard]] const char* filePath();
// Bumped by every edit and load (the studio's "unsaved" mark compares it with the saved one).
[[nodiscard]] int revision();
void touched();
[[nodiscard]] int savedRevision();

// Names (the file's words and the studio's labels).
[[nodiscard]] const char* itemName(Item v);
[[nodiscard]] const char* destName(Dest v);
[[nodiscard]] const char* targetName(Target v);
[[nodiscard]] const char* actionName(Action v);
[[nodiscard]] const char* condName(Cond v);
[[nodiscard]] const char* easeName(Ease v);
// The ease's curve at share t (0..1).
[[nodiscard]] float ease(Ease e, float t);
// A key's gun time (its own, else the key's), at least 0.02 s; its hand time at least 0.02 s.
[[nodiscard]] float handTime(const Key& k);
[[nodiscard]] float gunTime(const Key& k);
// The key after `i` (the loop back while `looping`, the first `finish` key once not; skipping keys whose condition fails by
// `cond(c)`); -1: the end.
template<class F>
[[nodiscard]] int nextKey(const Reload& r, int i, bool looping, F&& cond)
{
    int n = (looping && i == r.loopTo && r.loopFrom >= 0) ? r.loopFrom : i + 1;
    for(int guard = 0; guard < maxKeys * 2 && n >= 0 && n < r.keyCount; guard++)
    {
        if(!looping && !r.keys[n].finish)
        {
            n++;
            continue;
        }
        if(!cond(r.keys[n].cond))
        {
            n++;
            continue;
        }
        return n;
    }
    return -1;
}
// The first key (its condition met) of the reload's run (`finish` false) or of its end.
template<class F>
[[nodiscard]] int firstKey(const Reload& r, bool finish, F&& cond)
{
    for(int n = 0; n < r.keyCount; n++)
    {
        if(r.keys[n].finish == finish && cond(r.keys[n].cond))
        {
            return n;
        }
    }
    return -1;
}
// The keys holding the item: from the take's to the insert's (-1 none).
[[nodiscard]] int takeKey(const Reload& r);
[[nodiscard]] int insertKey(const Reload& r);

} // namespace qvr::reloadsys
