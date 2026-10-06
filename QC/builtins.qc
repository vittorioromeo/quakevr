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
// The `self` player's `hand` has just caught `e` from a force grab, which flew at `org` turned `ang`: its client eases the
// weapon now in the hand from there (vr_forcegrab_catch_blend; vr_drawblend.cpp).
void(float hand, entity e, vector org, vector ang) catchblend = #0;
// Where the force grab takes `e`: a weapon by its handle (its origin: its Weapon Offsets hold it there), anything else by
// its drawn middle (modelcentre).
vector(entity e) forcegrabpoint = #0;
// Spent casings out of the weapon in the `self` player's `hand`, `delay` seconds from now: drawn by
// that player's client from its weapon model's port (vr_shells.cpp; vr_shells). `kind`
// QVR_CASING_*; `flags` QVR_EJECT_*.
void(float hand, float kind, float count, float delay, float flags) ejectcasings = #0;
float QVR_CASING_SHOTGUNSHELL = 0;
float QVR_EJECT_FLICK = 1; // a flick reload: flung by the spinning weapon
// Weapon effects (engine vr_weaponfx.cpp): `shooter` fired the weapon in its `hand` (-1: a monster's gun): its drawn
// weapon kicks and its muzzle flashes, as that weapon's settings say (Weapon Offsets > Effects); a grunt's gun flashes
// at its muzzle. Sent to the clients, nothing in the game changes.
void(entity shooter, float hand) weaponfired = #0;
// A hitscan pellet (or round) of `shooter`'s, from `from` (the muzzle) to `to` (what it hit, or its range's end): the
// clients may draw a bullet tracer along it (vr_tracers, the weapon's own settings). `hand` -1: a monster's.
void(entity shooter, float hand, vector from, vector to) tracer = #0;
// The lightning gun in water's effects (engine vr_shock.cpp): QVR_SHOCK_SELF the `self` player shocked (its client: a flash
// over the view, arcs over the hands, arms and body, for `duration` s), QVR_SHOCK_SURFACE arcs on a liquid's surface round
// `org` (every client), QVR_SHOCK_BURST arcs out from `org` in a liquid. `radius`: how far the shock reaches.
void(float kind, vector org, float radius, float duration) watershock = #0;
float QVR_SHOCK_SELF = 0;
float QVR_SHOCK_SURFACE = 1;
float QVR_SHOCK_BURST = 2;
// `player` struck by lightning (a shambler's bolt, another's lightning gun, a trap's, the water's shock spreading) for
// `damage` at `at`: its client draws Quad's arcs over its arms, hands and body a while (vr_shock_self_time, by the damage;
// engine vr_shock.cpp KindSelfHit). Nothing else.
void(entity player, float damage, vector at) playershock = #0;
vector(entity e, vector handangles, float grab) carryangles = #0; // a held object's angles: at the grip (grab 1) its turn in the hand is kept; after, it turns with the hand
// Round 21: hotspot `index` (0..3) of the weapon in `hand`: where (a grip's point, a blade grip's middle; '0 0 0' if
// none), and its type (what 0: 0 none, 1 grip, 2 blade), bias (1, units) or a blade's share of the way from the hand
// to the tip (2). The local player's only (as its view draws them).
vector(entity player, float hand, float index) weaponhotspot = #0;
float(entity player, float hand, float index, float what) weaponhotspotinfo = #0;
// A weapon lying about taken by a hotspot (vr_weapon_grab_hotspots): the hotspot (index + 1; 0: its handle) the local
// player's empty `hand` is nearer than the handle of `weapon` (as its view finds it); others' are 0.
float(entity player, float hand, entity weapon) weapongrabspot = #0;
// Weapons held anywhere (vr_weapon_grab_anywhere): how the local player's empty `hand` holds the weapon in its other hand
// (0 not, 1 by a hotspot, 2 anywhere on it, or a moment ago); others' are 0.
float(entity player, float hand) weaponanygrip = #0;
// The weapon the local player's `hand` carries off its handle, as drawn: what 0 its handle's place, 1 its angles as
// CreateThrownWeapon takes them (the pitch negated), 2 '1 0 0' if known ('0 0 0' else), 3 its tip (its muzzle's anchor); 4 its handle and 5 its tip in the frame of
// the hand carrying it as drawn (forward, right, up: makevectors of the hand's angles).
vector(entity player, float hand, float what) weapondrawnpose = #0;
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
// Held props' grips (vr_grip.hpp; Held Object Offsets' Grip: where taken, fixed, in the palm, along the handle, and the
// offset on top). carrygrip: as `player`'s `hand` (lefthand: the left one) at `handangles` takes `e`, its origin at
// `offset` in the hand (forward, right, up: where it was taken): its turn in the hand set (its angles now) and its origin's
// place returned (.carry_offset). carryplace: each frame it is held in one hand, its angles set (turning with the hand)
// and its place in the hand returned (`offset` is .carry_offset; placed again at once when its settings change).
vector(entity e, entity player, float hand, float lefthand, vector handangles, vector offset) carrygrip = #0;
vector(entity e, vector handangles, vector offset) carryplace = #0;
// A hand grenade just taken from the grenade pouch (after carrygrip; `offset` its .carry_offset): turned in the hand by
// vr_grenade_pouch_hold_* (its angles now), its origin's place in the hand returned (.carry_offset).
vector(entity e, vector handangles, vector offset) carrypouch = #0;
// Weapon Offsets and Weapon Weights (vr_weapons.inc) of the weapon drawn with `model` (WeaponIdToModel), by the key its
// cvars have: "w_mass" (kg), "w_meleedmg", "w_throwdmg" (its damage multipliers)... (0: no such weapon or key).
float(string model, string key) weaponvalue = #0;
// Weight and damage (vr_weight_damage_*): a melee blow's or a throw's damage multiplier for a thing of `mass` kg (1 for
// no mass). Heavy leniency (vr_weight_lenient*): the factor on its strikes' and throws' speed thresholds (1: not heavy).
float(float mass) weightdamage = #0;
float(float mass) weightleniency = #0;
// Throws by weight (vr_throw_mass_model): the velocity (m/s) a thing of `mass` kg leaves `hands` hands (1 or 2) at, from
// the hand's throw estimate `vel` (m/s) and the part of it the wrist's flick gives (VRGetHandThrowFlick): a heavy thing
// keeps less of the flick, the gain of a real throw (vr_throw_gain_*), and its speed limited by its mass.
vector(vector vel, vector flick, float mass, float hands) throwvelocity = #0;
// Throws' hits and spin by weight (vr_throw_hit_*, vr_throw_spin_mass_exp), by `what`: 0 (mass) the factor on a thrown
// thing's speed thresholds (its heavy leniency, lower for a thing too heavy to be thrown vr_throw_hit_top m/s); 1 (mass,
// minmult) the least speed (units/s) it hurts at (times the weapon's own: Weapon Weights' Throw Speed Needed); 2 (mass,
// minmult, curvemult, speed, fullspeed) its damage factor hitting at `speed` units/s, 0 under it (speed over `fullspeed`,
// the base damage's, over that factor, times `curvemult`, eased in); 3 (mass, hands, rate) a throw's spin rate (rad/s)
// limited by its mass. VR_Thrown_*, VR_Throw_Spin.
float(float what, float mass, ...) throwweight = #0;
// Rocks and bricks lying about (engine vr_debris.cpp; vr_debris.qc): placing them (after the map's entities: how
// many), piece i's model, and putting piece i on an entity with its model set (its skin, size, turn, place on the floor
// and box; its kind: 1 a rock, 2 a brick, 0 none).
float() debrisplan = #0;
string(float i) debrismodel = #0;
float(entity e, float i) debrisput = #0;
float(entity e) debrisplace = #0; // a piece the map placed (vr_debris_piece, its model set): sized, turned by its yaw, resting on the floor at its origin; its kind (0: not a rock or brick model)
// Wooden crates (engine vr_crates.cpp; vr_crates.qc), as the rocks and bricks: placing them (after the map's entities:
// how many), crate i's model, and putting crate i on an entity with its model set (its skin, turn, place and box; its
// kind: 1 small, 2 large, 0 none); a crate the map or a test placed (level, turned by its yaw, on the floor under it).
float() crateplan = #0;
string(float i) cratemodel = #0;
float(entity e, float i) crateput = #0;
float(entity e) crateplace = #0;
float(float i) cratecrowbar = #0; // whether the plan puts a crowbar on crate i (vr_crate_crowbar)
float(entity e, float i) cratecrowbarput = #0; // lays the crowbar e (lying about, its model set) flat on crate i's top, as the plan says
// The solid prop that blocks monsters' sight (.vr_blocksight: a crate, an explosive box; not one in a hand) between
// `start` and `end`, met by its shape as it lies (Box3D's), not `ignore` nor `ignore2`; world if none.
entity(vector start, vector end, entity ignore, entity ignore2) sightblocked = #0;
float(entity prop, vector start, vector end) heldshape = #0; // where the line meets the shape of prop held in a hand (0..1; 1: not)
float(entity prop, vector mins, vector maxs, float reach) heldbox = #0; // whether the shape of prop held in a hand meets the box grown by reach (1; 0 not; -1 no held body)
vector(entity e, vector angles, vector point) modelpoint = #0; // where a point of e's model (its axes: x forward, y left, z up) is from its origin, turned by angles as drawn
// A hand's push on a rigid body: the point `at` gets at least `vel`'s speed along it (an impulse there: a tall box pushed
// high tips over). FALSE if Box3D doesn't move it. `mass` (kg, 0 or none: no limit): what pushes weighs that much, so the
// point gets only the share of the two masses meeting (a hand's, a gun's: vr_box3d_hand_mass; its Mass and vr_box3d_weapon_arm_mass).
float(entity e, vector at, vector vel, optional float mass) physicspush = #0;
// Ragdolls (vr_ragdoll; the engine's vr_box3d.cpp, "Ragdolls"): whether e is one; a hand takes the limb of e it is on
// (or catches the one it pulls); a force grab's pull of e's limb nearest the hand's aim (due in `flight` s); let go (a held
// limb keeps `vel`, units/s); 0 nothing held, 1 a limb held, 2 one pulled; how far the held or pulled limb's point is from
// the hand (-1 none); the limb nearest p (-1: not a ragdoll); p into limb `bone`'s space (units) and back (toWorld).
float(entity e) isragdoll = #0;
float(entity e, entity player, float hand) ragdollgrab = #0;
// Knockdowns (vr_knockdown.qc; the engine's vr_box3d.cpp, "Knockdowns"): whether e can be knocked down as a ragdoll (a
// monster with a rig, ragdolls on); e's ragdoll made now, alive (.vr_knockdown 1, touchable, not solid); e getting up
// from it: the get-up's first frame (frameA, or frameB if that fits how it lies better; -1 none), room for its box
// mins..maxs looked for within range: 0 none (it stays down), else 1 or 2 (the frame chosen), e moved there and turned.
float(entity e) canragdoll = #0;
float(entity e) ragdollknockdown = #0;
float(entity e, float frameA, float frameB, vector mins, vector maxs, float range) ragdollgetup = #0;
float(entity e, entity player, float hand, float flight) ragdollpull = #0;
void(entity player, float hand, vector vel) ragdollrelease = #0;
float(entity player, float hand) ragdollheld = #0;
float(entity player, float hand) ragdollreach = #0;
float(entity e, vector p) ragdollbone = #0;
vector(entity e, float bone, vector p, float toWorld) ragdollpoint = #0;
// Decapitation (vr_decap.qc; the engine's vr_box3d.cpp, "Decapitation"): e's ragdoll (made now for a dead monster with a
// rig) loses its head, launched by `blade` (u/s): 1 if it did; after it, ragdollcut's 0 the head's middle, 1 its angles,
// 2 its velocity, 3 its spin (rad/s: .vr_spin), 4 the neck now, 5 the way out of the stump (6 the head's middle before it); 1 if p
// is on e's ragdoll's head (0 not or
// headless, -1 not a ragdoll).
float(entity e, vector blade, ...) ragdolldecap = #0; // [float settle]: its parts' motion times this at its next step (a head pop's)
vector(entity e, float what, ...) ragdollcut = #0; // [float bone]: 4 and 5 the stump of that limb (-1 the head)
// Limb gore (vr_limbs.qc; the engine's vr_box3d.cpp "Limb gore"): e's limb at joint `bone` (-1 the head) cut off as
// ragdolldecap's head ([float settle]); ragdolllimb the joint a hit at p would cut (the head's bone for the head; -1 the
// torso, -2 no rig); ragdolllimbs: 0 its limb joints not cut (bits, not the head's), 1 its head's bones, 2 its bones cut
// off, 3 its head bone (-1 none), 4 what a cut at [bone] takes now, 5 [bone]'s parent; limbmodel the limb's model ("":
// none; just cut: the piece cut); limbplace where a limb not cut is now: 0 its model's origin, 1 its angles, 2 its
// velocity, 3 a point on its own bone's surface (the tests' hit).
float(entity e, float bone, vector blade, ...) ragdollcutlimb = #0;
float(entity e, vector p) ragdolllimb = #0;
float(entity e, float what, ...) ragdolllimbs = #0;
string(entity e, float bone) limbmodel = #0;
vector(entity e, float bone, float what) limbplace = #0;
// The piece a cut at `bone` (-1 the head) takes (just cut: the piece cut; a body gibbed whole: the limb as it is): what 0
// {kg it weighs (its share of e's ragdoll's mass by the kind of limb: a hand a little, a thigh a lot), the ragdoll's
// mass, that share}; 1 its
// cut end (the joint) in its model's space (units, unscaled: modelpoint turns it). '0 0 0': none.
vector(entity e, float bone, float what) limbpiece = #0;
float(entity e, vector p, ...) ragdollhead = #0; // [float neck]: units from the neck that count (at least 4)
// A shot's push: the first loose prop between `start` and `end` (a health box, a gib, a weapon lying about: traces pass
// through them) pushed as physicspush where the shot goes in. That prop, or world.
entity(vector start, vector end, vector vel, float mass) physicsshot = #0;
// The Box3D prop's velocity relative to `rel` kept by `keep` (0 .. 1), no faster than `maxSpeed` (0: any), `add` added; its
// spin kept by `keepSpin`: on its body, so that a physicspush after it this frame adds to it (a .velocity written would
// override both). FALSE if it is not Box3D's.
float(entity e, vector rel, float keep, float keepSpin, float maxSpeed, vector add) physicsdamp = #0;
// The grappling hook's physical rope (engine vr_ropesim.cpp; vr_grapple.qc VR_Grapple_RopeShape): ropestep steps it (once
// a frame; its chain from `gun` to `end`, falling, lying, never through the world or props but `skipA`, `skipB`) and
// gives the taut path's length from `game` (the body, or the gun) to `end`; ropepivot its first corner from `game` (0),
// from `end` (1), or '(beyond the first, beyond the second, the whole path)' (2); ropesend its points to the clients for
// the beam of `owner` and `beamId` (drawn through them).
float(entity hook, vector gun, vector game, vector end, float length, entity skipA, entity skipB) ropestep = #0;
vector(entity hook, float which) ropepivot = #0;
void(entity hook, entity owner, float beamId) ropesend = #0;
// The quick reel-in along the rope's path: the hook's new place, `remaining` units of the path left from `gun` round its
// corners (the hook now at `end`); the corners passed are gone, and ropepivot reads the path after it (from the gun).
vector(entity hook, vector gun, vector end, float remaining) roperetract = #0;
// The rope of `owner`'s beam `beamId` ended: its hook's rope is another beam's now (the client takes the old one away).
void(entity owner, float beamId) ropeend = #0;
float(entity e, entity player) carry2hoff = #0; // held in both hands: which have been pulled off it (1 the off hand, 2 the main; vr_carry_two_hands_detach)
void(entity e, entity player, float hand) carry2hkeep = #0; // held in both hands, now by `hand` alone: moved onto its grip there
float(entity e, vector point, float hand) carryreach = #0; // whether self's hand can take hold of it: its fist touches its drawn surface (vr_carry_grab_bias; point: the hand's, unused)
void(vector ang) makeforward = #0; // sets v_forward
vector(entity e, float max) modelbounds = #0; // the model's mins (max 0) or maxs, in model space
// The drawn box of e's model (as drawn: a weapon model's own scale and offset, its model_scale and model_offset), not
// turned, about its origin: its mins (max 0) or maxs. modelbounds is the model's raw box, not where a weapon model is drawn.
vector(entity e, float max) drawnbounds = #0;
// The model_offset putting the middle of e's drawn box (its model_scale as now, its model_offset ignored) at `at`.
vector(entity e, vector at) modeloffsetto = #0;
vector(entity e) modelcentre = #0; // the middle of the drawn model (turned with it, scaled and offset as drawn), in the world
// findradius's chain (the same order) less what has no model or whose modelcentre is out of the cone from org along dir
// (a unit vector) whose cosine is mincos (a little wider: test each again), or within a unit of org.
entity(vector org, float rad, vector dir, float mincos) findcone = #0;
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
float MOVE_PORTALS = 4096;          // QVR, traceline: on through the slipgates it crosses (vr_portals.cpp, vr_portals_walk)
float HITCLASS_GUNS = 0;
float HITCLASS_GRAPPLE = 1;
float HITCLASS_MELEE = 2;
float HITCLASS_THROWN = 3;
float VR_HITMODEL_REACH = 96; // units a model is found out of its box (an ogre's chainsaw reaches 33 out; a walking monster drawn behind it, a dog's step 64): the engine's MOVE_HITMODEL_REACH
float(entity e) hitmodel_target = #0; // whether e's model is what is hit (the option on; a monster or corpse with a Quake model)
// The segment a..b (a sphere of `radius` plus the class's tolerance) against e's model as drawn: where along it (0..1)
// it first meets it (0: it starts in it), -1 missed; -2: e's model is not tested (the option off, not a target).
float(entity e, vector a, vector b, float radius, float hitclass) hitmodel_segment = #0;
// The segment a..b, a sphere of `radius`, against any entity's Quake .mdl as drawn (a prop's, an item's: not only a
// monster's): where along it it first meets it (0: it starts in it), -1 missed, -2 no such model (a brush model: its box).
float(entity e, vector a, vector b, float radius) hitmodel_any = #0;
// The point p on e's drawn model as it is on the model standing (its stand frame), at e's origin, turned with its yaw
// (where positional damage's head sphere and regions are); p itself if e is no target.
vector(entity e, vector p) hitmodel_rest = #0;
// Debug > Show Hit Zones (vr_debug_hitzones): a zone of e drawn this server frame, in e's own frame (x forward along its
// yaw, y left, z up, from its origin): a box lo..hi (radius 0) or a capsule of `radius` from lo to hi. zone: 0 body, 1
// head, 2 extremities, 3 legs, 4 melee beheading (its colour).
void(entity e, float zone, vector lo, vector hi, float radius) debughitzone = #0;
// What of you the flame of torch t, held by you, touches (single player: the client's last frame; vr_burn_self,
// vr_burn_drop on): 1 the holding hand, 2 the other hand, 4 the holding arm, 8 the other arm, 16 the torso or legs, 32
// the head (bits), plus 256 times the deepest contact's bit (but the holding hand's); 0 nothing. trace_endpos: where (that
// contact), on the body; trace_plane_normal: the way out of it.
float(entity t) torchflametouch = #0;
// A map flame capsule against player p: same bits, 1/4 off hand/arm, 2/8 main; deepest includes both hands.
// parts selects eligible contacts (15 hands/arms, 48 torso/legs/head, 63 all).
// A negative radius only requests tracked body shapes for the next frame (returns 0).
float(entity p, vector a, vector b, float radius, float parts) mapflametouch = #0;

float(string s) cvar_hmake = #0;
float(float h) cvar_hget = #0;
void() cvar_hclear = #0;

float() worldtext_hmake = #0;
void(float h, string s) worldtext_hsettext = #0;
void(float h, vector v) worldtext_hsetpos = #0;
void(float h, vector v) worldtext_hsetangles = #0;
float(entity trig, entity who) portal_handles = #0; // QVR: the engine carries this player through the slipgate (vr_portals_walk)
float() portal_crossings = #0;       // QVR: how many slipgates the last MOVE_PORTALS traceline went through (0 none)
vector(float i) portal_entry = #0;   // QVR: ... where it went into the i-th (from 0)
vector(float i) portal_exit = #0;    // QVR: ... where it came out of it
vector(vector v) portal_turn = #0;   // QVR: v turned as that traceline was by its slipgates
entity(vector from, float range, vector aim, float mincos) findportalcone = #0;
vector(vector from, vector point) portal_pullimage = #0;
vector(entity e, vector hand, float begin) portal_pulltarget = #0;
void(float h, float v) worldtext_hsethalign = #0;
void(float h, float v) worldtext_hsetscale = #0;

// A tip for new players placed in a map (func_vr_tip, QC/vr_tips.qc; Quake/vr/vr_tips.cpp). The server owns the list
// and sends it to every client (and the whole list to each client as it spawns); a handle is one of its tips.
float() vr_tip_make = #0;
void(float h, string s) vr_tip_setname = #0;   // its name: its key in vr_tips_seen, with the map's
void(float h, string s) vr_tip_settext = #0;   // its text (\n new lines)
void(float h, vector v) vr_tip_setpos = #0;    // its point in the map
void(float h, entity e) vr_tip_setentity = #0; // what it follows, live (world: a fixed point)
void(float h, float v) vr_tip_setdistance = #0; // how near it must be (units; 0: vr_tips_distance)
void(float h, float v) vr_tip_setsize = #0;    // its screen's text size (0: vr_tips_size)
void(float h, float v) vr_tip_setdelay = #0;   // seconds near before it shows (< 0: vr_tips_delay)
void(float h, float v) vr_tip_setflags = #0;   // qvr::tips::Flags (QVR_TIP_* below)

void(vector org, string text, vector colour, float scale) floattext = #0; // a text rising from `org` and fading (1.5 s), facing the viewer, for every client

void(float to, vector v) WriteVec3 = #0; // three WriteCoords

float() highlighting = #0; // a highlight log is running (vr_highlights; vr_highlights.qc)
// A moment for the highlight log: its kind, score (0: the kind's own), what it was done to, with what, and the game time
// it began (0: now).
void(string kind, float score, entity subject, string detail, float since) highlight = #0;
float(float v) anglemod = #0; // v wrapped into [0, 360) (vr_builtins.cpp)

// The melee history of a hand's blow `b` (vr_melee.qc VR_Blow: the ring of its last poses, VR_Melee_Track), looked back
// through from the newest pose (vr_builtins.cpp; the look-back times are in Quake/vr/vr_melee_shared.h):
// - meleerun: how far the far end (or, `xGrip`, the grip) has come towards where it goes now (m): the most of its
//   displacement along its way now, looking back VR_MELEE_RUN_TIME at most (a wave's back and forth cancels).
// - meleewristspeed: the wrist's own speed (m/s): its net travel over the last VR_MELEE_WRIST_TIME, over that time. The
//   hand turned in place (a wiggle: the controller and the tip flick fast) barely moves it; a swing, a stab, a punch
//   carry it along.
// - meleewiggled: whether the grip went back the other way nearly as fast (VR_MELEE_WIGGLE_BACK of its speed now)
//   within VR_MELEE_WIGGLE_TIME: wiggled, not swung (whatever the hand holds: VR_Melee_Wiggled spares fists).
// - meleegripago: where the grip was `xBack` s ago (from the head, in the play space, metres; as far back as the
//   history goes).
float(entity b, float xGrip) meleerun = #0;
float(entity b) meleewristspeed = #0;
float(entity b) meleewiggled = #0;
vector(entity b, float xBack) meleegripago = #0;
float(string path) fileexists = #0; // whether a file is in the search path (data that may not be installed)

// Water splashes (vr_physics.cpp): where a segment first goes into water, slime or lava from the
// open (or out of it), on the surface -- `start` itself if it crosses none (the trace globals are
// left alone); and a splash there, something going `dir` in `strength` hard (4 a shot, 6-15 a hand
// or a thrown thing, 20-50 a body), with its sound (QVR_SPLASH_*; vr_water_splash, vr_water_sounds).
// The next entity after `start` (in edict order) with any of `flags` set in the float field `fld`; world when none: a
// loop over the monsters (findflags(world, flags, FL_MONSTER)...) steps through them alone, in the engine (vr_builtins.cpp).
entity(entity start, .float fld, float flags) findflags = #0;
vector(vector start, vector end) liquidentry = #0;
void(vector org, vector dir, float strength, float snd) watersplash = #0;
float QVR_SPLASH_SILENT = 0; // the caller plays its own sound
float QVR_SPLASH_SHOT = 1;   // a small plip
float QVR_SPLASH_THING = 2;  // a splash, small or big by strength

void(entity target, float duration) bodyshock = #0;
void(entity target, float left) bodysmoulder = #0; // a burning body's fire goes out in `left` s (0: just out; below 0: doused): it smokes while it burns and a while after (vr_burning.qc)
void(entity target, float duration) bodyshockdeath = #0; // lightning's lasting shock on a body it killed or struck: arcs crawling over it, fading (vr_shock.qc)
void(entity box, entity player, float hand, float begin) portal_carry = #0;

float(entity observer, entity target, vector from, vector point, float gate) portal_ai_sight = #0;
vector(float gate, vector value, float direction) portal_ai_map = #0;
entity(entity observer) portal_ai_client = #0;
