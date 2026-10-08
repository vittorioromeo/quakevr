// vr_stealth_defs.qc -- the stealth AI's fields and entry points (vr_stealth.qc; docs/vr-port/STEALTH_PLAN.md), ahead
// of ai.qc and combat.qc, which call them.

// On a monster:
.float stl_state;       // STL_IDLE or STL_ALERT (Hostile is Quake's: an enemy)
.float stl_phase;       // Alert: STL_TURN, STL_WALK, STL_SEARCH, STL_RETURN
.float stl_meter;       // the suspicion meter, 0..1
.float stl_seen;        // when the meter last filled (the player seen)
.float stl_decayed;     // when it last drained
.vector stl_point;      // what it investigates
.float stl_phase_time;  // when the phase began
.float stl_alert_time;  // when it was last disturbed
.float stl_alert_power; // and how strongly (a noise's reach over its distance; a sighting 1)
.float stl_post_set;    // its post recorded:
.vector stl_post;       // where it stood,
.float stl_post_yaw;    // facing,
.entity stl_post_goal;  // and its movetarget (a path_corner: the path it walked)
.float stl_post_walk;   // it was walking that path
.float stl_settle;      // back at its post: turning to its yaw
.float stl_wary;        // until when it is sensitive (an investigation that found nothing)
.entity stl_marker;     // the point it walks to (its goalentity while Alert)
.float stl_class;       // 0 not yet known; 1 the stealth rules apply; -1 excluded (a boss, a flyer, a swimmer)
.float stl_hear;        // its kind's hearing (times)
.float stl_sight;       // and eyes (the meter's rate, times)
.float stl_look_time;   // searching: when it looks elsewhere next; walking: until when it walks
.float stl_look_base;   // and the yaw it looks about
.vector stl_walk_from;  // walking: where it was a second ago
.float stl_walk_check;  // and when that was
.float stl_stuck;       // seconds it made no headway
.float stl_scan_time;   // when it next looks for bodies, torches and beams
.entity stl_body;       // the last body it investigated
.float stl_lit;         // in a flashlight's beam (the lens's glare: its meter sees him lit)
.float stl_lost_time;   // Hostile: when it last saw its enemy
.vector stl_lost_at;    // and where
.entity stl_next;       // a gathered list's next (VR_Stealth_Gather)
.entity stl_suspect;    // the player its meter is on (coop: the most suspicious one in sight)
.float stl_rate;        // and the meter's rate at that sighting
.float stl_heard;       // the last noise it heard (time; its kind, what made it): tests, the debug log
.float stl_heard_kind;
.entity stl_heard_src;
// On a body or a gib: what may be noticed (STL_NOTICE_BODY), and when a monster first noticed it.
.float stl_notice;
.float stl_found;
// On a player:
.float stl_light;       // the light on him (128: full), and when it was measured
.float stl_light_time;
.float stl_stand_eye;   // his eyes' height over his feet standing (the highest lately)
.float stl_step_time;   // the next running step's noise
.float stl_shot_time;   // the last shot's noise (a held trigger's stream: one a fifth of a second)
.float stl_loud;        // the last noise he made (its reach) and when (the gem's ring)
.float stl_loud_time;
.float stl_climb_time;  // when a hand last held a ledge (climbing is silent)
.float stl_vis;         // how visible he is, 0..1 (the gem)
.entity stl_pnext;      // the next player (stl_players, listed each frame)
.float stl_hud;         // the gem's readings (STAT_QVR_STEALTH): visible 0..100 plus 1000 times loud 0..100; -1 off

float STL_IDLE = 0;
float STL_ALERT = 1;

float STL_TURN = 1;
float STL_WALK = 2;
float STL_SEARCH = 3;
float STL_RETURN = 4;

// Noise kinds
float STL_N_STEP = 1;   // the player's running steps, a hard landing
float STL_N_PROP = 2;   // a prop's knock
float STL_N_MELEE = 3;  // a blow landing
float STL_N_GUN = 4;    // a shot
float STL_N_BLAST = 5;  // an explosion
// Other disturbances (VR_Stealth_Alert's kind)
float STL_A_GLIMPSE = 10; // the meter past vr_stealth_glimpse
float STL_A_GRAZE = 11;
float STL_A_BEAM = 12;    // lit by a beam
float STL_A_BEAMSEEN = 13;
float STL_A_TORCH = 14;
float STL_A_BODY = 15;
float STL_A_SHARED = 16;  // another monster's alarm
float STL_A_LOST = 17;    // a Hostile one that lost its enemy

float STL_NOTICE_BODY = 1;

void() VR_Stealth_Tick;
float() VR_Stealth_MeterOn;
float(entity m) VR_Stealth_Applies;
void(entity m, vector at, float xKind, float xPower, float xShare) VR_Stealth_Alert;
void() VR_Stealth_TestFrame;
void(entity m) VR_Stealth_Died;
void(entity mis) VR_Stealth_GrazeMissile;
void() VR_Stealth_PlayerFrame;
void() VR_Stealth_Frame;
float(float xVis) VR_Stealth_Lose;
float(entity client) VR_Stealth_See;
float() VR_Stealth_Stand;
float(float dist) VR_Stealth_Walk;
void() VR_Stealth_Hostile;
float(entity targ, entity attacker) VR_Stealth_SneakMult;
void(vector org, float range, entity src, float kind) VR_Stealth_Noise;
void(vector org, float damage) VR_Stealth_Blast;
void(vector from, vector to, entity shooter, entity hit) VR_Stealth_Graze;
void(entity shooter, float weapon) VR_Stealth_Shot;
