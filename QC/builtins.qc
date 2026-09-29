//
// builtin functions
//

void(vector ang)    makevectors        = #1;        // sets v_forward, etc globals
void(entity e, vector o) setorigin    = #2;
void(entity e, string m) setmodel    = #3;        // set movetype and solid first
void(entity e, vector min, vector max) setsize = #4;
// #5 was removed
void() break                        = #6;
float() random                        = #7;        // returns 0 - 1

// Frikbot overrides the following:
// void(entity e, float chan, string samp, float vol, float atten) sound = #8;

vector(vector v) normalize            = #9;
void(string e) error                = #10;
void(string e) objerror                = #11;
float(vector v) vlen                = #12;
float(vector v) vectoyaw            = #13;
entity() spawn                        = #14;
void(entity e) remove                = #15;

// sets trace_* globals
// nomonsters can be:
// An entity will also be ignored for testing if forent == test,
// forent->owner == test, or test->owner == forent
// a forent of world is ignored
void(vector v1, vector v2, float nomonsters, entity forent) traceline = #16;

entity() checkclient                = #17;    // returns a client to look for
entity(entity start, .string fld, string match) find = #18;
string(string s) precache_sound        = #19;
string(string s) precache_model        = #20;

// Frikbot overrides the following:
// void(entity client, string s)stuffcmd = #21;

entity(vector org, float rad) findradius = #22;
void(string s) bprint                = #23;

// Frikbot overrides the following:
// void(entity client, string s) sprint = #24;

void(string s) dprint                = #25;
string(float f) ftos                = #26;
string(vector v) vtos                = #27;
void() coredump                        = #28;        // prints all edicts
void() traceon                        = #29;        // turns statment trace on
void() traceoff                        = #30;
void(entity e) eprint                = #31;        // prints an entire edict
float(float yaw, float dist) walkmove    = #32;    // returns TRUE or FALSE
// #33 was removed
float() droptofloor= #34;    // TRUE if landed on floor
void(float style, string value) lightstyle = #35;
float(float v) rint                    = #36;        // round to nearest int
float(float v) floor                = #37;        // largest integer <= v
float(float v) ceil                    = #38;        // smallest integer >= v
// #39 was removed
float(entity e) checkbottom            = #40;        // true if self is on ground
float(vector v) pointcontents        = #41;        // returns a CONTENT_*
// #42 was removed
float(float f) fabs = #43;

// Frikbot overrides the following:
// vector(entity e, float speed) aim = #44;        // returns the shooting vector

float(string s) cvar = #45;                        // return cvar.value
void(string s) localcmd = #46;                    // put string into local que
entity(entity e) nextent = #47;                    // for looping through all ents
void(vector o, vector d, float color, float count) particle = #48;// start a particle effect
void() ChangeYaw = #49;                        // turn towards self.ideal_yaw
                                            // at self.yaw_speed
// #50 was removed
vector(vector v) vectoangles            = #51;

//
// direct client message generation
//

// Frikbot overrides the following:
// void(float to, float f) WriteByte    = #52;
// void(float to, float f) WriteChar    = #53;
// void(float to, float f) WriteShort   = #54;
// void(float to, float f) WriteLong    = #55;
// void(float to, float f) WriteCoord   = #56;
// void(float to, float f) WriteAngle   = #57;
// void(float to, string s) WriteString = #58;
// void(float to, entity s) WriteEntity = #59;

//
// broadcast client message generation
//

// void(float f) bWriteByte    = #59;
// void(float f) bWriteChar    = #60;
// void(float f) bWriteShort   = #61;
// void(float f) bWriteLong    = #62;
// void(float f) bWriteCoord   = #63;
// void(float f) bWriteAngle   = #64;
// void(string s) bWriteString = #65;
// void(entity e) bWriteEntity = #66;

void(float step) movetogoal                = #67;

string(string s) precache_file        = #68;    // no effect except for -copy
void(entity e) makestatic        = #69;
void(string s) changelevel = #70;

//#71 was removed

void(string var, string val) cvar_set = #72;    // sets cvar.value

// Frikbot overrides the following:
// void(entity client, string s) centerprint = #73;    // sprint, but in middle

void(vector pos, string samp, float vol, float atten) ambientsound = #74;

string(string s) precache_model2    = #75;        // registered version only
string(string s) precache_sound2    = #76;        // registered version only
string(string s) precache_file2        = #77;        // registered version only

// Frikbot overrides the following:
// void(entity e) setspawnparms        = #78;   // set parm1... to the
                                                // values at level start
                                                // for coop respawn

// ----------------------------------------------------------------------------
// Standard extension builtins (numbers as in DP/FTE/QSS/Ironwail)
// ----------------------------------------------------------------------------

float(float angle) sin = #60;           // DP_QC_SINCOSSQRTPOW, radians
float(float angle) cos = #61;           // DP_QC_SINCOSSQRTPOW, radians
float(float v) sqrt = #62;              // DP_QC_SINCOSSQRTPOW
float(float a, float b) min = #94;      // DP_QC_MINMAXBOUND
float(float a, float b) max = #95;      // DP_QC_MINMAXBOUND
float(float base, float exp) pow = #97; // DP_QC_SINCOSSQRTPOW
float(string s) strlen = #114;          // FRIK_FILE
string(string s, float start, float length) substring = #116; // FRIK_FILE
float(string s, float index) str2chr = #222;                  // FTE_STRINGS
float(float y, float x) atan2 = #474;   // DP_QC_ASINACOSATANATAN2TAN, radians
string(string fmt, ...) sprintf = #627; // DP_QC_SPRINTF

// ----------------------------------------------------------------------------
// Quake VR builtins (#0: bound by name by the engine's VR module)
// ----------------------------------------------------------------------------

void(vector o, vector d, float preset, float count) particle2 = #0; // start a particle2 effect
void(entity e, vector org, vector dir, float kind, float amount, float extra) woundevent = #0; // a wound painted on e's model by every client (vr_wounds.qc)
void(float hand, float delay, float duration, float frequency, float amplitude) haptic = #0; // VR haptics function
void(float hand, float strength, vector dir) handimpact = #0; // knock the `self` player's drawn hand (a parried blow)
// Spent casings out of the weapon in the `self` player's `hand`, `delay` seconds from now: drawn by
// that player's client from its weapon model's port (vr_shells.cpp; vr_shells). `kind`
// QVR_CASING_*; `flags` QVR_EJECT_*.
void(float hand, float kind, float count, float delay, float flags) ejectcasings = #0;
float QVR_CASING_SHOTGUNSHELL = 0;
float QVR_EJECT_FLICK = 1; // a flick reload: flung by the spinning weapon
vector(entity e, vector handangles, float grab) carryangles = #0; // a held object's angles: at the grip (grab 1) its turn in the hand is kept; after, it turns with the hand
// Round 21: hotspot `index` (0..3) of the weapon in `hand`: where (a grip's point, a blade grip's middle; '0 0 0' if
// none), and its type (what 0: 0 none, 1 grip, 2 blade), bias (1, units) or a blade's share of the way from the hand
// to the tip (2). The local player's only (as its view draws them).
vector(entity player, float hand, float index) weaponhotspot = #0;
float(entity player, float hand, float index, float what) weaponhotspotinfo = #0;
vector(entity e, vector handpos, vector palm) carryfit = #0; // at the grip: how far to move it (the way the palm faces) to sit against the palm, not in the fist (vr_held_surface_fit)
vector(entity e, entity player, float grab) carry2h = #0; // held in both hands: at the second grip (grab 1) the grips are kept; after, where it goes (its angles are set)
void(vector at, float damage) physicsblast = #0; // an explosion throws the rigid bodies round it (Box3D's)
// Held Object Offsets (vr_props.inc, by the prop's model): a setting ("mass": kg, the setting else Box3D's; "throw": the
// share of the hand's throw it keeps; "grip", "two_hands", "forcegrab", ...: see vr_props.inc; its default for a model
// with no settings).
float(entity e, string key) propvalue = #0;
// A prop held the same way every time (its "grip" 1): its turn in the hand set (and its angles now); returns its origin's
// place in the hand (forward, right, up: .carry_offset). lefthand: the left hand's (mirrored).
vector(entity e, vector handangles, float lefthand) propgrip = #0;
// Weapon Offsets and Weapon Weights (vr_weapons.inc) of the weapon drawn with `model` (WeaponIdToModel), by the key its
// cvars have: "w_mass" (kg), "w_meleedmg", "w_throwdmg" (its damage multipliers)... (0: no such weapon or key).
float(string model, string key) weaponvalue = #0;
// Weight and damage (vr_weight_damage_*): a melee blow's or a throw's damage multiplier for a thing of `mass` kg (1 for
// no mass). Heavy leniency (vr_weight_lenient*): the factor on its strikes' and throws' speed thresholds (1: not heavy).
float(float mass) weightdamage = #0;
float(float mass) weightleniency = #0;
// Rocks and bricks lying about (engine vr_debris.cpp; vr_debris.qc): placing them (after the map's entities: how
// many), piece i's model, and putting piece i on an entity with its model set (its skin, size, turn, place on the floor
// and box; its kind: 1 a rock, 2 a brick, 0 none).
float() debrisplan = #0;
string(float i) debrismodel = #0;
float(entity e, float i) debrisput = #0;
vector(entity e, vector angles, vector point) modelpoint = #0; // where a point of e's model (its axes: x forward, y left, z up) is from its origin, turned by angles as drawn
// A hand's push on a rigid body: the point `at` gets at least `vel`'s speed along it (an impulse there: a tall box pushed
// high tips over). FALSE if Box3D doesn't move it.
float(entity e, vector at, vector vel) physicspush = #0;
float(entity e, entity player) carry2hoff = #0; // held in both hands: which have been pulled off it (1 the off hand, 2 the main; vr_carry_two_hands_detach)
float(entity e, vector point, float hand) carryreach = #0; // whether self's hand can take hold of it: its fist touches its drawn surface (vr_carry_grab_bias; point: the hand's, unused)
void(vector ang) makeforward = #0; // sets v_forward
vector(entity e, float max) modelbounds = #0; // the model's mins (max 0) or maxs, in model space
vector(entity e) modelcentre = #0; // the middle of the drawn model (turned with it, scaled and offset as drawn), in the world
void(vector v1, vector mins, vector maxs, vector v2, float nomonsters, entity forent) tracebox = #0; // traceline with a box
// Precise hit detection (engine vr_hitmodel.cpp; the option vr_hit_precise): shots, missiles, the grappling hook, melee
// blows and thrown things meet a monster's (or a corpse's) model as drawn, grown by their class's tolerance
// (vr_hit_tolerance_*), not Quake's box round it. Added to a traceline's "nomonsters" (FALSE, MOVE_HITGIBS): monsters are
// met at their models, the class in the two bits above (a missile's move has it from .vr_hitclass). With the option off,
// ignored: the boxes, as before.
float MOVE_HITMODEL = 512;          // a gun's shot (vr_hit_tolerance_guns)
float MOVE_HITMODEL_GRAPPLE = 1536; // the grappling hook's (MOVE_HITMODEL + 1 * 1024)
float MOVE_HITMODEL_MELEE = 2560;   // a melee blow's (+ 2 * 1024)
float MOVE_HITMODEL_THROWN = 3584;  // a thrown thing's (+ 3 * 1024)
float HITCLASS_GUNS = 0;
float HITCLASS_GRAPPLE = 1;
float HITCLASS_MELEE = 2;
float HITCLASS_THROWN = 3;
float VR_HITMODEL_REACH = 64; // units a model is found out of its box (an ogre's chainsaw reaches 33 out; a walking monster drawn behind it): the engine's MOVE_HITMODEL_REACH
float(entity e) hitmodel_target = #0; // whether e's model is what is hit (the option on; a monster or corpse with a Quake model)
// The segment a..b (a sphere of `radius` plus the class's tolerance) against e's model as drawn: where along it (0..1)
// it first meets it (0: it starts in it), -1 missed; -2: e's model is not tested (the option off, not a target).
float(entity e, vector a, vector b, float radius, float hitclass) hitmodel_segment = #0;
// The point p on e's drawn model as it is on the model standing (its stand frame), at e's origin, turned with its yaw
// (where positional damage's head sphere and regions are); p itself if e is no target.
vector(entity e, vector p) hitmodel_rest = #0;

float(string s) cvar_hmake = #0;
float(float h) cvar_hget = #0;
void() cvar_hclear = #0;

float() worldtext_hmake = #0;
void(float h, string s) worldtext_hsettext = #0;
void(float h, vector v) worldtext_hsetpos = #0;
void(float h, vector v) worldtext_hsetangles = #0;
void(float h, float v) worldtext_hsethalign = #0;
void(float h, float v) worldtext_hsetscale = #0;

void(vector org, string text, vector colour, float scale) floattext = #0; // a text rising from `org` and fading (1.5 s), facing the viewer, for every client

void(float to, vector v) WriteVec3 = #0; // three WriteCoords

float(string path) fileexists = #0; // whether a file is in the search path (data that may not be installed)

// Water splashes (vr_physics.cpp): where a segment first goes into water, slime or lava from the
// open (or out of it), on the surface -- `start` itself if it crosses none (the trace globals are
// left alone); and a splash there, something going `dir` in `strength` hard (4 a shot, 6-15 a hand
// or a thrown thing, 20-50 a body), with its sound (QVR_SPLASH_*; vr_water_splash, vr_water_sounds).
vector(vector start, vector end) liquidentry = #0;
void(vector org, vector dir, float strength, float snd) watersplash = #0;
float QVR_SPLASH_SILENT = 0; // the caller plays its own sound
float QVR_SPLASH_SHOT = 1;   // a small plip
float QVR_SPLASH_THING = 2;  // a splash, small or big by strength
