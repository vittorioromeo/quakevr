//
//
//
// ----------------------------------------------------------------------------
// Entity (visual) scale fields
// ----------------------------------------------------------------------------

.vector model_scale;
.vector model_scale_origin;
.vector model_offset;

//
//
//
// ----------------------------------------------------------------------------
// New item management fields
// ----------------------------------------------------------------------------

.float vr_itemId;

//
//
//
// ----------------------------------------------------------------------------
// Hand and weapon touching callback fields
// ----------------------------------------------------------------------------

.void() handtouch;        // touched by VR hand
.void() vr_wpntouch;      // touched by VR weapon

//
//
//
// ----------------------------------------------------------------------------
// Off-hand think callback fields
// ----------------------------------------------------------------------------

.void() think2;
.float  nextthink2;

//
//
//
// ----------------------------------------------------------------------------
// Main hand weapon fields
// ----------------------------------------------------------------------------

.float weaponflags;

//
//
//
// ----------------------------------------------------------------------------
// Off-hand weapon fields
// ----------------------------------------------------------------------------

.float  weapon2;            // WID
.string weaponmodel2;
.float  weaponframe2;
.float  weaponflags2;

//
//
//
// ----------------------------------------------------------------------------
// Weapon reloading fields
// ----------------------------------------------------------------------------

// Weapon instances (vr_weaponinst.qc): each weapon is one record entity (classname "weapon_inst": its id and its
// magazine), which the hand holding it, the holster keeping it or the prop it lies as refers to. Moving a weapon moves
// the reference; the engine frees a record nothing refers to any more (and tells the clients its id is gone).
.entity weaponinst;  // Main hand; a weapon prop's (thrown, dropped, lying)
.entity weaponinst2; // Off-hand
.entity holsterweaponinst0;
.entity holsterweaponinst1;
.entity holsterweaponinst2;
.entity holsterweaponinst3;
.entity holsterweaponinst4;
.entity holsterweaponinst5;
.float wi_uid;  // A record's: its id (> 0, unique on the server; kept through saves, loads and map changes)
.float wi_clip; // A record's: its magazine (rounds; a chainsaw's fuel)
.float weaponclipsize;  // Main hand
.float weaponclipsize2; // Off-hand

//
//
//
// ----------------------------------------------------------------------------
// Holster weapon ID fields
// ----------------------------------------------------------------------------

.float holsterweapon0; // Hotspot: `QVR_HS_LEFT_SHOULDER_HOLSTER`.
.float holsterweapon1; // Hotspot: `QVR_HS_RIGHT_SHOULDER_HOLSTER`.
.float holsterweapon2; // Hotspot: `QVR_HS_LEFT_HIP_HOLSTER`.
.float holsterweapon3; // Hotspot: `QVR_HS_RIGHT_HIP_HOLSTER`.
.float holsterweapon4; // Hotspot: `QVR_HS_LEFT_UPPER_HOLSTER`.
.float holsterweapon5; // Hotspot: `QVR_HS_RIGHT_UPPER_HOLSTER`.

//
//
//
// ----------------------------------------------------------------------------
// Holster weapon model fields
// ----------------------------------------------------------------------------

// These are needed because they are read in C++ as 'STAT_XXX' variables to determine the weapon.
.string holsterweaponmodel0;
.string holsterweaponmodel1;
.string holsterweaponmodel2;
.string holsterweaponmodel3;
.string holsterweaponmodel4;
.string holsterweaponmodel5;

//
//
//
// ----------------------------------------------------------------------------
// Holster weapon flags fields
// ----------------------------------------------------------------------------

.float holsterweaponflags0; // Hotspot: `QVR_HS_LEFT_SHOULDER_HOLSTER`.
.float holsterweaponflags1; // Hotspot: `QVR_HS_RIGHT_SHOULDER_HOLSTER`.
.float holsterweaponflags2; // Hotspot: `QVR_HS_LEFT_HIP_HOLSTER`.
.float holsterweaponflags3; // Hotspot: `QVR_HS_RIGHT_HIP_HOLSTER`.
.float holsterweaponflags4; // Hotspot: `QVR_HS_LEFT_UPPER_HOLSTER`.
.float holsterweaponflags5; // Hotspot: `QVR_HS_RIGHT_UPPER_HOLSTER`.

//
//
//
// ----------------------------------------------------------------------------
// Hotspot fields
// ----------------------------------------------------------------------------

.float offhand_hotspot;
.float sawcord; // the chainsaws' starter cords (vr_chainsaw.qc VR_SAWCORD_*: which hand holds one, a pull)
.float vr_flatscreen; // this player plays flatscreen (-flatscreenmode, his own client: the engine sets it from his move): VR_FlatPlay
.float vr_prone; // the player lies down (vr_prone: his box vr_prone_height tall; the engine sets it from his move): no jumping (PlayerJump)
.float vr_movejump; // a remote client's walk runs move by move (the engine sets it each frame, vr_predict.cpp): his jump is the engine's, in the move whose key pressed it (PlayerPreThink leaves it; 0 again for AD's boots)
.float vr_movewaterjump; // ... and Quake's jump out of the water (PlayerPreThink sets it each frame for such a client
                        // on Quake's jump out, not the hands' climb: CheckWaterJump leaves it to the moves, MP_PLAN B8f)
.float vr_movewaterclimb; // ... or the hands' climb out (PlayerPreThink: bit 0 on, bits 1-2 the empty hands, as
                          // VR_WaterClimb_FreeHands; CheckWaterJump leaves it to the moves, MP_PLAN B8j part 3)
.float vr_botswing; // a bot's melee swing began at this time (W_Fire_Melee; bot_setvrfields swings its main hand: MP B11 part 3, part 3)
// A bot's hands as a flat-screen player's (MP bots phase 1; botflatpose, frikbot/bot_fight.qc bot_setvrfields):
.float vr_botswing_ph;    // its swing's phase this frame: 0 none, 1 the windup (strikes nothing), 2 the strike and after
.float vr_botparry;       // its guard's share (0 at rest .. 1 up), eased as vr_flat_parry_in_time / _out_time ease a player's
.float vr_botparry_at;    // when its guard goes up (a blow seen coming, after its reaction's delay)
.float vr_botparry_until; // and comes down
.float vr_botparry_next;  // no blow looked at again before this (one roll a blow)
.float vr_botparries;     // its guards raised (the tests)
.float vr_botwindup;      // its swing's extra hold at the top (s: its telegraph, bot_windup; set as the swing begins)
// A bot toned down by its skill (his voice notes 2026-11-21; frikbot/bot_fight.qc bot_aim_point, bot_fire_ready):
.vector vr_botaim_err;    // its aim's error now: _x right, _y up, each a share of the distance to its enemy
.float vr_botaim_next;    // ... drawn again then
.entity vr_botreact_of;   // the enemy it last reacted to
.float vr_botreact_at;    // ... it may fire at him from then on (its reaction time)
.float vr_botreact_seen;  // ... last ready to fire at him (bot_fire_ready)
.float vr_botburst_until; // its burst of fire goes on until then, then a pause (its fire discipline)
// A bot's off hand as a player's (MP B11 part 4; frikbot/bot_fight.qc bot_offhand_ai decides, bot_setvrfields moves it):
.float vr_botpunch;       // its punch began at this time (0: none)
.entity vr_botpunch_of;   // ... at whom
.float vr_botpunch_end;   // its last punch ended at this time
.entity vr_botreach;      // what its off hand reaches for (a thing lying near, or a player's held weapon; world: none)
.float vr_botreach_h;     // ... that player's hand holding the weapon
.float vr_botreach_s;     // ... 1 open, reaching; 2 closed on it; 3 pulling it away (a player's weapon)
.float vr_botreach_t;     // ... the state's start
.vector vr_botreach_at;   // ... the hand's place (world)
.float vr_botreach_next;  // it reaches for nothing before this time (a reach given up)
.float vr_botvr_t;        // the frame its hands' once-a-frame state was made for (time)
.vector vr_bothand_rel;   // ... its hands' places from its origin then (their velocity, .handvel)
.vector vr_botoffhand_rel;
.float vr_botoffgrip;     // ... its off hand's grip
.float vr_moveswim; // ... and so does the water's drag (WaterMove leaves it; the engine sets it each frame)
.float vr_movetakes; // ... and what else his moves may take (the engine sets it each frame: bit 0 the hands' climb out of
                     // the water, B8j part 3; bit 1 the grapple's rope, B8j part 4)
.entity vr_movegrapple; // ... the hook whose rope his moves hold and reel him by this frame (VR_Grapple_ServiceAnchor
                        // sets it, the engine clears it each frame; MP_PLAN B8j part 4) ...
.vector vr_movegrapple_at; // ... its rope's first corner from him
.vector vr_movegrapple_rope; // ... x its path beyond the corner, y the shortest, z the reeling hands (bit 0 the off hand,
                             // 1 the main) plus 4 times (the gun's hand plus one)
.float vr_stand; // the head's height over the floor, a share of the standing (calibrated) eye height (1 standing; 0 not known): the engine sets it from his move (VR_KdTrig_Tackle)
.float handdrop;// a heavy weapon wrenched out of a hand (the engine's vr_weight_drop: VR_HANDDROP_*, weapons.qc)
.float handsynth; // the hands whose motion in this move is synthesized, not for gestures (the engine's vr_handsource.cpp: VR_HANDSYNTH_*, VR_HandMotionTracked)
.float vr_posetime; // the time of the poses in the player's last move (s, modulo VR_POSE_CLOCK_WRAP; -1 unknown): VR_Melee_Track
.float mainhand_hotspot;

//
//
//
// ----------------------------------------------------------------------------
// Off-hand ammunition fields
// ----------------------------------------------------------------------------

.float currentammo2;  // off-hand
.float ammocounter2;  // off-hand

//
//
//
// ----------------------------------------------------------------------------
// Off-hand firing button field
// ----------------------------------------------------------------------------

.float button3;        // off-hand fire

//
//
//
// ----------------------------------------------------------------------------
// VR Yaw Angle (Thumbstick-Controlled Turn Angle)
// ----------------------------------------------------------------------------

.float vryaw;

//
//
//
// ----------------------------------------------------------------------------
// Main hand physics fields
// ----------------------------------------------------------------------------

.vector handpos;
.vector handrot;
.vector handvel;
.vector handthrowvel;
.float  handvelmag;
.vector handavel;
.vector handthrowpos;  // where the thrown object left the hand (world)
.float  handthrowage;  // seconds since
.vector handthrowflick; // m/s: the part of handthrowvel the wrist's flick gives (a heavy thing keeps less: throwvelocity)

//
//
//
// ----------------------------------------------------------------------------
// Off-hand physics fields
// ----------------------------------------------------------------------------

.vector offhandpos;
.vector offhandrot;
.vector offhandvel;
.vector offhandthrowvel;
.float  offhandvelmag;
.vector offhandavel;
.vector offhandthrowpos;
.float  offhandthrowage;
.vector offhandthrowflick;

//
//
//
// ----------------------------------------------------------------------------
// Head physics fields
// ----------------------------------------------------------------------------

.vector headvel;
.vector headpos; // the head (between the eyes): with a lean it is not above the origin

//
//
//
// ----------------------------------------------------------------------------
// Weapon muzzle fields
// ----------------------------------------------------------------------------

.vector muzzlepos;
.vector offmuzzlepos;
// Each held gun's loading port as drawn (immersive reloading: vr_reload.qc; the hand's position when it has none)
.vector loadportpos;
.vector offloadportpos;
// And the way a round lies going in there (unit; zero: none): the shotgun's tube and the super shotgun's barrels
// forward (a shell's open end leads), a magazine gun's well up (the seated magazine's feed end)
.vector loadportaxis;
.vector offloadportaxis;
// And the way its opening faces (unit, outwards; zero: none): the shotgun's port down from the receiver, the open super
// shotgun's breech back from the barrels, a well down from the gun
.vector loadportface;
.vector offloadportface;
// Where a held gun's projectile leaves from, from its muzzle along the shot (forward, right, up: the shot's frame, so a
// portal's turn changes nothing; zero: the muzzle): the AD crossbow's bolt as drawn in its groove (vr_adcrossbow.cpp).
.vector launchofs;
.vector offlaunchofs;
// The magazine attached to each held (or carried) gun, as drawn (the engine; vr_reload.qc): a box, its middle and its three
// half-axes (the first from the middle up to its feed end, in the well); the axes all zero: none.
.vector magboxpos;
.vector magboxx;
.vector magboxy;
.vector magboxz;
.vector offmagboxpos;
.vector offmagboxx;
.vector offmagboxy;
.vector offmagboxz;
// Where the rounds stand in the ammo pouch on the front of the belt, as the client places it (the engine; vr_reload.qc
// VR_Reload_PouchFrame: pouch reloading): a box, its middle and its three half-axes (out of the body, to the left, up along
// the rounds standing in it); the axes all zero: no pouch.
.vector ammopouchpos;
.vector ammopouchx;
.vector ammopouchy;
.vector ammopouchz;
.vector shotrot;    // where the hand's shots go (angles): its aim turned by the weapon's Shot Pitch/Yaw (the engine)
.vector offshotrot;

//
//
//
// ----------------------------------------------------------------------------
// VR-related bits
// ----------------------------------------------------------------------------

.float vrbits0;

//
//
//
// ----------------------------------------------------------------------------
// Handtouch-related fields
// ----------------------------------------------------------------------------

.float  touchinghand;
.float  handtouch_hand;
.entity handtouch_ent;

//
//
//
// ----------------------------------------------------------------------------
// Bot-related fields
// ----------------------------------------------------------------------------

.float ishuman;

//
//
//
// ----------------------------------------------------------------------------
// Rigid-body flight (engine: Box3D, vr_box3d.cpp)
// ----------------------------------------------------------------------------

.float  vr_rigid; // MOVETYPE_TOSS/BOUNCE entities that spin, bounce and settle as rigid bodies
.float  vr_projectile; // a projectile (weapons.qc VR_Proj_Mark): a small MOVETYPE_FLY one's or a grenade's move meets corpses, gibs, knocked-down bodies (vr_hitmodel.cpp)
.float  vr_hitclass; // precise hits (vr_hitmodel.cpp): a missile's class for its tolerance (HITCLASS_*: 0 a gun's, 1 the grappling hook's); -1: Quake's boxes
.vector vr_spin;  // their angular velocity, radians per second, world axes
                  // (.vr_rigid 2: a solid one, SOLID_BBOX, its box kept round its drawn shape as it turns: an explosive box)
.float  vr_pickup_spin; // a weapon pickup drawn as its prop (vr_pickup_prop_models): drawn spinning as an EF_ROTATE pickup
                        // about its middle while not a rigid body (the engine's U_QVR_SPIN)
// A thrown axe stuck in something by its blade (engine: vr_axestick.cpp, which sticks it and moves it with what it is in;
// QC/vr_axestick.qc).
.float  vr_stick_kind;   // 0 not stuck; 1 the level or a brush entity; 2 a prop; 3 a monster's model; 4 a monster's box;
                         // -1 it came out (it never sticks again)
.entity vr_stick_ent;    // what it is in (world: the level)
.vector vr_stick_ofs;    // its origin in that thing's frame (the engine's)
.vector vr_stick_ang;    // its turn in that frame (the engine's)
.vector vr_stick_anchor; // kind 3: the model's triangle and where on it (the engine's)
.float  vr_stick_edge;   // which blade (the engine's)
.vector vr_stick_at;     // the middle of the blade's edge in the world (the engine's, each frame)
.vector vr_stick_dir;    // the way the blade faces, into what it is in (the engine's, each frame)
.float  vr_stick_wiggle; // degrees the axe is rocked about its edge (a force grab's tug)
.vector vr_stick_vel;    // in flight: its velocity and spin at the last step (the engine's: one that changes has hit
.vector vr_stick_spin;   // something, and it never sticks after: a handle-first throw bounces)
.float  vr_stick_embed;  // how it stuck (the engine's; weapons.txt embed): AXESTICK_POINT, AXESTICK_EDGE
.float  vr_stick_depth;  // by its point: units from the tip back to where it went in (the engine's)
.float  vr_stick_slide;  // units it has slid along its axis since it stuck (+ deeper, - out: vr_embedhold.qc)
.float  vr_stick_goal;   // a held stab's: the depth it goes on to as it enters (the engine's; 0: there)
.float  vr_stick_bladelen; // a held stab's: the blade's length (the engine's)
.float  vr_stick_loose;  // in an arm or a leg: its turn follows the limb's eased (the engine reads it)
.entity vr_embed_off;    // on a player: the blade its off hand holds in a body (VR_Embed_Derive; the engine reads it)
.entity vr_embed_main;   // the main hand's
.float  vr_embed_handle; // the hands on theirs by the handle (bits)
.vector vr_embed_armgap; // on a player: cm each hand drawn on a blade is past its stretched arm (x off, y main; the engine's)
.vector vr_embed_armpast; // on a player: cm each hand's drawn wrist is past the most its arm reaches, negative within it (x off, y main; the engine's)
.vector vr_stick_base;   // its angles before the hand's leeway and the tug's shake (the engine's)
.vector vr_stick_lean;   // the leeway's turn now (the engine's)
.float  vr_stick_shake;  // how near the tug is to tugging it out, 0..1 (vr_embedhold.qc; the engine shakes it)
.float  vr_stick_freecheck; // when its handle was last checked for being in the body (the engine's)
.float  vr_stick_freed;  // degrees its handle was turned out of the body, all told (the engine's)
.float  vr_stick_lost;   // since when what it is in can't say where it is (the engine's; 0 it can): it falls (VR_AxeStick_Think)
.float  vr_stick_sag;    // by its point, in the level: degrees it sags under its weight now (the engine's)
.float  vr_stick_sagv;   // how fast (degrees a second; the engine's)
.void(float xSpeed) vr_impact; // a rigid body hitting something (the world, a prop, a monster) at xSpeed m/s (at least
                               // 2): self the body, other what it hit (world for the level). Box3D's hits, after its step.
.float vr_blocksight; // a solid prop monsters can't see through while it lies about (a crate, an explosive box: sightblocked)
.float vr_shot_radius;  // a grenade shots set off (vr_grenade.qc, "Shooting grenades"; engine: vr_hitmodel.cpp VR_ShotTargetClip): shots
                        // and missiles meet a cube this half size round its middle (0: Quake's rules); with takedamage
.float vr_shot_ownfrom; // its owner's own shots meet it from then (not as it leaves his launcher or hand)
.float vr_melee_time; // a monster's melee attack (ai_charge, ai_melee and kin; AD's ad_ai_*): when it last stepped or
                      // struck in one; the engine keeps it out of your head meanwhile (vr_enemy_melee_space)
.float vr_melee_arc;      // Show Melee Arcs (vr_enemy_melee_arc_show; fight.qc VR_MeleeArc_ShowFrame, set each frame it is
.float vr_melee_arc_wide; // shown, at vr_melee_shown): its blows' arc (degrees each side of its facing), its sweeping
.float vr_melee_reach;    // blows' (0: none seen yet), its reach (units, origin to origin); the engine draws them
.float vr_melee_shown;    // (vr_bodycontact.cpp meleeArcDraw)
.float vr_melee_rule;     // ... and its blows' rule (VR_MELEE_RULE_*: an arc, a leap's touch, all round, a shove's cone)
.float vr_melee_blow_reach; // QC's: its last blow's reach (units, origin to origin) and when (time): the parry's range
.float vr_melee_blow_time;  // (fight.qc VR_MeleeBlow, combat.qc VR_Parry_Range)
.float vr_melee_reach_base; // QC's: its blows' own reach (Quake's or AD's, before the growth: fight.qc VR_MeleeReach)
.float vr_melee_wide_seen;  // QC's: it struck a sweeping blow (its wide arc shown from then)
.float vr_melee_turn_time;  // QC's: the frame (time) of its last swing turn (fight.qc VR_MeleeArc_TurnBegin), and its
.float vr_melee_turn_yaw0;  // facing before that frame's first turn: a frame's turns held together, however many calls
.float vr_aim_cone;    // Show Aim Cones (vr_enemy_aim_show; vr_aimcone.qc VR_AimCone_ShowFrame, set each frame it is shown, at
.float vr_aim_fire;    // vr_aim_shown): a ranged monster's aim cone and fire cone (degrees each side of its facing); while
.float vr_aim_shown;   // a shot waits for it to turn, vr_aim_holding is when the wait began (0: none); the engine draws
.float vr_aim_holding; // them (vr_bodycontact.cpp aimConeDraw)
.void() vr_aim_fn;     // QC's (vr_aimcone.qc VR_AimCone_Wait): a waiting shot's function (called when the wait ends), the
.void() vr_aim_next;   // think it had (its next frame) and how long until it (seconds), the wait's kind of shot, when
.float vr_aim_step;    // the last wait ended (none again for a second: one wait an attack) and the frame (time) a wait
.float vr_aim_kind;    // ended in (its shot fires then, unheld)
.float vr_aim_ended;
.float vr_aim_go;

// Climbable ropes and chains (vr_ropes.qc; the engine's vr_climbrope.cpp reads them): their map keys, 0 the default.
.float rope_length;    // units (256)
.float rope_thickness; // units across (1.5)
.float rope_segments;  // pieces (one every 16 units)
.float rope_mass;      // kg, the whole rope (5; a chain 20)
.float rope_stiffness; // 0.05-1 (1)
.float rope_damping;   // per second (0.5)
.float rope_style;     // 1: drawn as a chain (vr_rope)
.float rope_pinned;    // its bottom held where it first hangs (PINNED; a target: at it)
.float rope_slack;     // ropes7: with a target or rope_end, its length over the straight line between its ends (0.1: 10% longer)
.vector rope_end;      // ropes7: its bottom pinned at this point (as a target's origin; '0 0 0': none)
