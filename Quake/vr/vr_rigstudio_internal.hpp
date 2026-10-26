#pragma once

// vr_rigstudio_internal.hpp -- what the Rig Studio's files share (vr_rigstudio.cpp: the mode, the scene, the camera, the
// edits and the commands; vr_rigstudio_view.cpp: the viewport, its drawing, picking and handles; vr_rigstudio_ui.cpp:
// the windows). Internal: included by those three alone. All of it the main thread's.

#include "vr_rigstudio.hpp"
#include "vr_aggrip.hpp"
#include "vr_agpose.hpp"
#include "vr_engine.hpp"
#include "vr_posestudio.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigdata.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct ImDrawList;

namespace qvr::rigstudio::detail
{

inline constexpr int maxBones = ragdoll::maxBones;
inline constexpr int maxGroups = 64;
inline constexpr int maxUndo = 200;

enum class Source : int
{
    Rest = 0, // the rest pose, posed by hand (a named pose's place)
    Anim = 1, // one of the model's own animations, with its override tracks
    Seq = 2,  // a new sequence of keys
};

enum class Tool : int
{
    Select = 0,
    Rotate = 1,  // the bones turned (FK)
    Move = 2,    // ... moved
    IK = 3,      // a limb's end dragged, its two bones solved
    Rest = 4,    // the rest pose's joints and ends moved (the rig's)
    Weights = 5, // vertices selected (click, box), assigned, set
    Paint = 6,   // weights painted with a brush
    PoseKeys = 7, // the Pose Studio Data's key dragged by handles on the arms' targets, elbows, torso, the Pickup's legs
};
inline constexpr int toolCount = 8;

enum class MeshMode : int
{
    Game = 0,   // the game's own drawing of it (every procedural layer)
    Shaded = 1, // the studio's skinned mesh, lit flat (its weights: the edits shown live)
    Wire = 2,   // the game's drawing and the studio mesh's edges over it
    Xray = 3,   // the studio mesh see-through, every edge
};

enum class Colors : int
{
    None = 0,
    ByBone = 1, // each vertex its owning bone's colour
    Heat = 2,   // the selected bone's weights (blue 0 .. red 1)
};

enum class BoneLook : int
{
    Octahedral = 0,
    Stick = 1,
    Off = 2,
};

enum class Brush : int
{
    Add = 0,
    Subtract = 1,
    Smooth = 2,
};

// What an undo puts back: the model's whole working data and the pose under edit.
struct Snapshot
{
    rigdata::ModelData m;
    rigdata::Local live[maxBones];
    int seq{-1};
    za::String what;
    // The Pose Studio's data edited from here (its own store: vr_agrunt_poses, vr_agrunt_grips): its value before
    int poseCode{-1};
    agpose::Key poseKey;
    int gripKind{-1}, gripSlot{0}, gripHand{0};
    aggrip::Offset grip;
    // The aim target's place (studio-session state, never in a file): its place before
    bool aimEntry{false};
    glm::vec3 aimAt{0.f};
};

// Where the subject aims (an armed humanoid's guns, its melee guard): a point in the room, as the game aims at an enemy
// standing there (QC VR_AGK_Hold: its Aim stance, its target's point). Studio-session state: kept across subject
// changes, not saved.
struct AimTarget
{
    bool on{true};        // the subject aims at it (off: it carries its weapon, as before)
    bool show{true};      // drawn, with its move handle
    bool atCamera{false}; // it follows the studio camera
    bool placed{false};   // its place set (else put ahead of the subject when it stands)
    bool selected{false}; // picked (its handle drawn)
    glm::vec3 at{0.f};    // world
};
inline constexpr float aimAhead = 200.f; // units: the default target ahead of the subject
inline constexpr float aimHeight = 12.f; // ... above its origin (its chest: the guns' height, so the aim is level)

struct KeyRef
{
    int bone{0};
    int frame{0}; // (its key by its frame: whole frames)
};

// The subject's choice (what vr_rigstudio spawns).
struct Choice
{
    char spawn[64]{"monster_vr_grunt"};
    int weapon{4};  // the armed list's code (vr_toolgun_armed_list; -1 fists, 0 random): the shotgun (2 is the axe)
    int weapon2{0}; // its left hand's (0 none)
};

struct Camera
{
    glm::vec3 focus{0.f};
    float yaw{180.f};   // degrees: the camera looks along this (0: +x)
    float pitch{12.f};  // degrees down
    float dist{90.f};
    bool look{false};   // right mouse: looking about from where it is
};

// The view of the last frame drawn (its projection, for the overlays and the picks).
struct View
{
    glm::vec3 org{0.f}, fwd{1.f, 0.f, 0.f}, right{0.f, -1.f, 0.f}, up{0.f, 0.f, 1.f};
    float tanX{1.f}, tanY{1.f};
    float x{0.f}, y{0.f}, w{1.f}, h{1.f}; // the 3D view's rectangle in the window (pixels)
};

struct Studio
{
    bool open{false};
    bool wantMap{false};
    bool staged{false};
    bool flatChecked{false};
    // The scene
    glm::vec3 stage{0.f};
    float stageYaw{0.f};
    glm::vec3 home{0.f};
    int subject{0};
    double spawnAt{-1.0};
    Choice choice;
    Choice made;
    // Settings put back on closing
    float keepFreeze{0.f}, keepViewsize{100.f}, keepCrosshair{0.f}, keepViewmodel{1.f};
    bool keptSettings{false};
    // The model edited
    const qmodel_t* model{nullptr};
    const ragdoll::Rig* rig{nullptr};
    int numBones{0};
    rigdata::ModelData doc; // the working copy (published to the game on each edit)
    bool dirtyRig{false};
    za::Vector<uint8_t> dirtySeq;  // per sequence
    za::Vector<Snapshot> undo, redo;
    Snapshot preEdit;              // the data as the frame began, no widget held: a widget's edit's undo (its first
                                   // frame's change already made when it reports itself activated)
    bool dragUndone{false};        // a drag's snapshot taken (one an edit, not one a frame)
    // The pose under edit: Rest: each bone's local; Anim, Seq: a delta after the keys (not keyed yet)
    rigdata::Local live[maxBones];
    // Selection
    uint32_t selBones{0};
    int active{-1};
    uint32_t hidden{0};
    int solo{-1};
    za::Vector<uint8_t> selVerts;
    za::Vector<KeyRef> selKeys;
    // Tools
    Tool tool{Tool::Rotate};
    bool autoKey{true};
    bool snap{false};
    Brush brush{Brush::Add};
    float brushRadius{40.f};   // pixels
    float brushStrength{0.5f};
    bool mirror{false};
    float setWeight{1.f};
    // Timeline
    Source source{Source::Rest};
    int group{0};              // Anim: the frame group
    int seq{-1};               // Seq: its index
    float frame{0.f};
    bool playing{false};
    bool loop{true};
    float speed{1.f};
    bool onion{false};
    int onionSpan{1};
    // Display
    MeshMode mesh{MeshMode::Game};
    Colors colors{Colors::None};
    BoneLook bones{BoneLook::Octahedral};
    bool joints{true}, names{true}, proxies{false}, ikTargets{true}, grips{true}, limits{false};
    bool showOutliner{true}, showProps{true}, showTimeline{true}, showTools{true}, showPoseData{false}, showHelp{false},
        showPrefs{false}, showSequence{true};
    // Layers toggled (their cvars, put back on closing)
    bool layerArms{true}, layerLegs{true}, layerUpper{true}, layerSmooth{true}, layerKicks{true};
    float keepIk{1.f}, keepLegs{1.f}, keepUpper{1.f}, keepLerp{1.f}, keepKicks{1.f};
    bool keptLayers{false};
    Camera cam;
    View view;
    // Clipboards
    rigdata::Pose poseClip;
    bool havePoseClip{false};
    za::Vector<rigdata::Track> keyClip; // (frames from the first copied)
    // The arms as solved (relinkSkinned's note)
    posestudio::Note note;
    int noteFrame{-1};
    // The search box
    char search[64]{};
    char newSeqName[64]{"taunt"};
    char poseName[64]{"pose"};
    char eventName[64]{"fire"};
    char status[256]{};
    // The Pose Studio data window's choice
    int pdKind{0}, pdClass{1}, pdPose{0}, pdPhase{0}, pdHand{0};
    bool pdGlobal{true};
    bool pdFollow{true};   // the key follows the subject: its kind, its weapon's class (the Pose Handles' default)
    int pdPart{-1};        // the Pose Handles' part picked (posestudio's numbers: 0 the right weapon .. 11; -1 none)
    int subjKind{-1}, subjClass{-1}; // the subject's, as QC tells them (rigstudio::qcHold)
    Tool lastTool{Tool::Rotate};
    double statusAt{0.0};
    // This frame's geometry (vr_rigstudio_view.cpp's)
    za::Vector<glm::vec3> restVerts;   // pose 0 (model units)
    za::Vector<glm::vec3> baseVerts;   // the frame under the pose
    za::Vector<glm::vec3> posedVerts;  // drawn (model units)
    za::Vector<glm::vec3> worldVerts;
    za::Vector<glm::vec3> normals;     // posed, per vertex
    za::Vector<int> tris;              // .mdl vertices, 3 a triangle
    za::Vector<int> edges;             // pairs
    za::Vector<za::Vector<int>> nbrs;  // per vertex
    za::Vector<rigdata::VertWeights> weights; // effective, per vertex
    za::Vector<glm::vec2> screen;      // per vertex (pixels)
    za::Vector<float> depth;           // per vertex (along the view; <= 0 behind)
    za::Vector<uint8_t> facing;        // per vertex: faces the camera
    bool geomFor{false};               // the geometry made for `model`
    int weightsGen{-1}, docGen{0};
    glm::vec3 pivots[maxBones]{}, ends[maxBones]{};
    rigpose::Rigid P[maxBones];        // the frame under the pose (rest -> it)
    rigpose::Rigid F[maxBones];        // the bones as posed
    bool armsLayer{false};             // the pose's arms drive the game's arms
    int basePose1{0}, basePose2{0};
    float baseBlend{0.f};
    // Interaction
    bool dragging{false};
    int dragAxis{-1};      // the handle's part being dragged
    int dragKind{0};       // 1 rotate, 2 move, 3 IK, 4 rest pivot, 5 rest end, 6 box select, 7 paint, 8 scrub, 9 keys
    glm::vec2 dragStart{0.f}, dragLast{0.f};
    float dragAngle{0.f};
    int ikChain{-1};       // the IK's lower bone
    glm::vec3 ikTarget{0.f};
    int hoverBone{-1};
    int hoverVert{-1};
    AimTarget aim;
};
extern Studio st;

// vr_rigstudio.cpp
[[nodiscard]] const aliashdr_t* hdrOf();
void say(const char* text);
void pushUndo(const char* what);
void keepPreEdit();          // the frame's start (no widget held): the data kept for pushPreEditUndo
void pushPreEditUndo(const char* what);
void pushPoseUndo(int code);                       // a Pose Studio key about to change
void pushGripUndo(int kind, int slot, int hand);   // a grip about to change
void pushAimUndo();                                // the aim target about to move
void resetAim();                                   // the aim target ahead of the subject, level (undoable)
[[nodiscard]] glm::vec3 aimDefault();              // ... where that is
void undo();
void redo();
void touchedRig();          // the rig part changed (dirty, published)
void touchedSeq(int seq);   // a sequence changed
void publish();
void save();
void revert();
void printDefaults();
void respawn();
void close();
[[nodiscard]] int groupCount();
[[nodiscard]] const rigdata::AnimGroup& groupAt(int i);
[[nodiscard]] float timelineLength(); // frames
[[nodiscard]] rigdata::Sequence* curSeq();
[[nodiscard]] rigdata::AnimOverride* curOverride(bool make);
[[nodiscard]] za::Vector<rigdata::Track>* curTracks(bool make);
[[nodiscard]] rigdata::Track* trackOf(za::Vector<rigdata::Track>& tracks, int bone, bool make);
void keySelected();         // a key at the frame for the selected bones (the live delta folded in)
void keyBone(int bone);
void clearLive();
void setFrame(float f);
void newSequence(const char* name, int frames, float fps, bool loop);
void selectBone(int b, bool add);
[[nodiscard]] int boneByName(const char* name);
[[nodiscard]] int mirrorBone(int b);
// A world (model space) motion applied to bone `b` (about whatever it says), as an edit of the pose under edit.
void moveBone(int b, const rigpose::Rigid& world);
void applyPose(const rigdata::Pose& p, uint32_t bones);
void mirrorPose();
// The pose under edit evaluated at `frame` (the bones' F, the frame's P and drawn vertices' base).
void evaluate(float frame, rigpose::Rigid* F, rigpose::Rigid* P, bool withLive);
[[nodiscard]] rigdata::Pose currentPose(); // the bones' locals now (absolute)
// Weights
void ensureWeights();       // `weights` up to date with the doc
[[nodiscard]] rigdata::VertWeights& editWeights(int v); // the vertex's override (made from its effective weights)
void normalizeVert(rigdata::VertWeights& w);
void setVertWeight(int v, int bone, float value); // others rescaled to make 1
void assignSelected(int bone);
void normalizeSelected();
void selectByBone(int bone, bool add);
void growSelection(bool grow);
[[nodiscard]] int mirrorVert(int v);

// vr_rigstudio_view.cpp
void buildGeometry();       // the model's triangles, edges, rest vertices (once a model)
void poseGeometry();        // this frame's posed vertices, normals
void viewport();            // the 3D view's overlays, picks, handles, camera input
[[nodiscard]] glm::vec3 toWorld(const glm::vec3& p);
[[nodiscard]] glm::vec3 toModelDir(const glm::vec3& d);
bool project(const glm::vec3& world, glm::vec2& out, float* depth = nullptr);
void frameSelection();
void snapView(int which, bool flip);
void cameraView(float* origin, float* angles);
// The Pose Keys tool's part `part` (posestudio's numbers) as the subject's note has it now (world; the chest's at its
// waist): false when not a pose key's part now.
[[nodiscard]] bool posePart(int part, rigpose::Rigid& out);
[[nodiscard]] glm::quat bodyTurn();   // the subject's facing (its key space's axes in the world)
// The studio's mock mouse (the tests): a drag of a handle queued as the hand would make it, a frame a step (fed to
// ImGui before its frame: mockFeed). `owner` 1 the aim target, 2 the Pose Keys' part `part`; `axis` 0..2 an arrow
// (`amount` units along it), 20..22 a ring (`amount` degrees round it). False: no such handle shown now.
bool mockDrag(int owner, int part, int axis, float amount);
void mockFeed();
[[nodiscard]] bool mockBusy();

// vr_rigstudio_ui.cpp
void windows();
[[nodiscard]] glm::vec4 boneColor(int b);

} // namespace qvr::rigstudio::detail
