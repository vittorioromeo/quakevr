// combat.qc

float IsExplosionDamage;

// VR: what kind of blow the next T_Damage is, for the firing range's training dummy (vr_dummy.qc),
// which reports every hit. Attacks that their inflictor alone does not tell apart set these just
// before their T_Damage (VR_SetHitKind) and clear them after (VR_ClearHitKind); the rest (missiles,
// thrown things, deflected projectiles) the dummy works out from the inflictor.
float QVR_HITKIND_NONE     = 0; // worked out from the inflictor
float QVR_HITKIND_MELEE    = 1; // a swing of a hand (vr_hithand): fist, weapon or a gun used as a club
float QVR_HITKIND_HEADBUTT = 2;
float QVR_HITKIND_BASH     = 3; // a weapon's guard driven forward (vr_bash)
float QVR_HITKIND_SHOVE    = 4; // both empty hands driven forward (vr_bash)
float QVR_HITKIND_SHOT     = 5; // a hitscan weapon (vr_hithand's, or vr_hitweapon): shotguns, lightning
float QVR_HITKIND_SPLASH   = 6; // an explosion's (or a discharge's) radius damage
float QVR_HITKIND_BLEED    = 7; // a thrown axe stuck in it bleeding it (vr_axestick.qc): no new wound

float vr_hitkind;     // QVR_HITKIND_*
float vr_hithand;     // the hand that struck or fired (-1: unknown)
string vr_hitweapon;  // what fired, when not the weapon in vr_hithand ("": that one)
vector vr_hitpos;     // where it struck ('0 0 0': unknown)
vector vr_hitdir;     // and going which way

// A melee blow's positional damage (weapons.qc, vr_melee_positional): its damage times the zone the blow landed on.
float(entity xTarget, vector xTo, vector xDir) VR_Melee_Positional;

// A shotgun blast's pellets on its target (ApplyMultiDamage), by where they struck.
float vr_hitpellets;
float vr_hitpellets_head;
float vr_hitpellets_limb;
float vr_hitpellets_leg;

// PositionalDamage's last answer (1 head, 2 extremity, 3 legs, 0 body), for which target, when.
float vr_hitpart;
entity vr_hitpart_target;
float vr_hitpart_time;
vector vr_hitpart_pos;

void(float xKind, float xHand, vector xPos, vector xDir) VR_SetHitKind =
{
    vr_hitkind = xKind;
    vr_hithand = xHand;
    vr_hitpos = xPos;
    vr_hitdir = xDir;
};

void() VR_ClearHitKind =
{
    vr_hitkind = QVR_HITKIND_NONE;
    vr_hithand = -1;
    vr_hitweapon = "";
    vr_hitpos = '0 0 0';
    vr_hitdir = '0 0 0';
};

//============================================================================

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
float(entity targ, entity inflictor) CanDamage =
{
// bmodels need special checking because their origin is 0,0,0
    if(targ.movetype == MOVETYPE_PUSH)
    {
        traceline(inflictor.origin, 0.5 * (targ.absmin + targ.absmax), TRUE, self);
        if(trace_fraction == 1)
            return TRUE;
        if(trace_ent == targ)
            return TRUE;
        return FALSE;
    }

    traceline(inflictor.origin, targ.origin, TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '15 15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '-15 -15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '-15 15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;
    traceline(inflictor.origin, targ.origin + '15 -15 0', TRUE, self);
    if(trace_fraction == 1)
        return TRUE;

    return FALSE;
};


// The firing range's training dummy that died as the grunt it stands for (vr_dummy.qc, vr_dummy_gib): not counted, no loot.
.float vr_dummy_dead;
// QVR: Dawn of the Machine's infected (vr_mg3_infected.qc): what his first death turns him into (0: none, or turned).
.float vr_mg3_infected;
// QVR: Dawn of the Machine's own monsters drawn and fought as a stock one (vr_mg3_ogre.qc MG3_MON_*: the rocket ogre an
// ogre, the demo dog a rottweiler): which it is (0: the stock monster).
.float vr_mg3_mon;

/*
============
Killed
============
*/
void(entity targ, entity attacker) Killed =
{
    entity oself;

    oself = self;
    self = targ;
    // VR: the firing range's training dummy killed by what passed by its own report (VR_Dummy_Hit; a zombie it stands as
    // beheaded: vr_decap.qc): it dies as its enemy with Dummy Dies, else it stands, unhurt (vr_dummy.qc).
    if(VR_IsDummy(self) && !VR_Dummy_Killed(self))
    {
        self = oself;
        return;
    }
    targ.vr_ai_gate = 0; // (a dead monster's, its head's or its gibs': no gate route left for portal tosses)

    if(self.health < -99)
    {
        self.health = -99;        // don't let sbar look bad if a player
    }

//MED
    if(self.charmed)
    {
        self.effects = self.effects - (self.effects & EF_DIMLIGHT);
    }

    if(self.movetype == MOVETYPE_PUSH || self.movetype == MOVETYPE_NONE)
    {    // doors, triggers, etc
        // Native brush explosions fire authored target chains with the attacker as activator.
        if(MG_WorldCampaign() && self.classname == "func_explode") self.enemy = attacker;
        self.th_die();
        self = oself;
        return;
    }

    self.enemy = attacker;

    // QVR: Dawn of the Machine's infected (vr_mg3_infected.qc): his first death bursts him into a zombie or a fiend, not
    // counted (upstream's .infected), before any of Quake VR's dying (beheading, ragdoll, corpse, gore).
    if(self.vr_mg3_infected && (self.flags & FL_MONSTER))
    {
        MG3_Infected_Turn(attacker);
        self = oself;
        return;
    }

// bump the monster counter
    if((self.flags & FL_MONSTER) && !self.vr_dummy_dead && (!MGH_Active() || self.classname != "monster_zombie"))
    {
        killed_monsters = killed_monsters + 1;
        WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
    }

// PGM - guarantee gib if killed by buzzsaw.
    if(attacker.classname == "buzzsaw" ||
        attacker.classname == "Vengeance" ||
        attacker.classname == "pendulum")
        self.health = -99;

// PGM - gravity fix!
    self.gravity = 1.0;
    ClientObituary(self, attacker);

    self.takedamage = DAMAGE_NO;
    MGH_Kill(attacker);
    self.touch = SUB_Null;

    monster_death_use();
    VR_Corpse_DieWhole(self); // VR: never gibbed (vr_corpse_nogib 2: testing ragdolls with explosions)
    VR_Decap_Killed(self); // VR: beheaded by the blow (vr_decap.qc): its ragdoll headless at once, its head flying; dies whole
    VR_Shock_Killed(self); // VR: a living one's lasting shock (shorter) carried on as a body's (vr_shock.qc)
    float kdDown = self.vr_knockdown == 1; // VR: knocked down (vr_knockdown.qc): already a ragdoll
    float kdModel = self.modelindex;
    self.vr_knockdown = 0;
    self.th_die();
    if(kdDown && self.modelindex == kdModel && !self.vr_gib)
    {
        VR_Knockdown_FastForwardDeath(); // its death code at once: its scream, its drops; its ragdoll stays as it lies
    }

    // VR: a head thrown by the death code (the monster itself) stays grabbable, whatever the
    // death code did after (a soldier's drop makes it SOLID_NOT).
    if(self.vr_gib)
    {
        self.solid = SOLID_NOT_BUT_TOUCHABLE;
    }

    // VR: a monster that died whole becomes a corpse that can be gibbed (vr_corpse_gib).
    VR_Corpse_Watch(self);

    // VR: gore: a pool of blood spreads under the corpse (vr_gore, vr_carry.qc).
    VR_Gore_Killed(self);

    self = oself;
};

// ----------------------------------------------------------------------------
// VR: gibbing corpses (vr_corpse_gib). A monster that died whole, once its death animation is over
// and it lies still, is a corpse that takes damage: shots (shotguns, nails, lightning, missiles),
// explosions, and blows of a hand or of what it holds (VR_Corpse_StrikeFrame). Enough of it
// (its monster's vr_corpse_health_<monster>, times vr_corpse_health_mult; each blow's damage times its kind's
// vr_corpse_dmg_<kind>: VR_Corpse_DamageKind) gibs it as its own death code
// would have: its head, its gibs, the gib sound, and a mist of blood (VR_BloodMist, vr_carry.qc). Its damage never
// reaches Quake's death code (T_Damage hands it to VR_Corpse_Damage): nothing is counted, killed or
// triggered again. Only the monsters VR_Corpse_Parts knows (the ones whose death code can gib
// them) are gibbed: no bosses (Chthon, Shub-Niggurath, the dragon, Armagon), nothing that always
// gibs (zombies, mummies) or blows up.
//
// The corpse is not solid: shots and missiles stop at it as at gibs (the engine's MOVE_HITGIBS:
// touchable non-solids that take damage). Its box is made to lie on the floor (VR_Corpse_Arm): low,
// and wider than the standing monster's (a lying body is longer). The engine makes it a physics body
// (vr_corpse_collide, vr_box3d.cpp: props rest on it, held things push it) and, as set, an obstacle
// to players and monsters (vr_corpse_collide_player, _monsters).

.float vr_corpse;         // 1 dying (watched: VR_Corpse_Think2), 2 a corpse that can be gibbed
.float vr_corpse_hp;      // what damage it takes yet
.float vr_corpse_frame;   // its frame at the last look: the death animation is over when it stops changing
.float vr_corpse_since;   // when it died
.float vr_corpse_model;   // its modelindex when it died (the death code gibbing it changes it)
.float vr_corpse_struck;  // when a hand last struck it (a swing strikes once)
// VR: dying bodies (vr_corpse_dying): from the frame its death code makes it not solid (what it drops is dropped by
// then) to lying still, a dying monster takes a corpse's damage (VR_Corpse_Damage: gibbed by enough of it, never killed
// again), so a chainsaw or a blow cuts it as it falls. Touchable, not solid, as a corpse; its standing box kept (it is
// still falling: the precise hits test its model as drawn).
.float vr_corpse_dying;   // 1: dying, and taking damage as a corpse

// Decapitation (vr_decap.qc): its head cut off (its ragdoll headless; gibbed, no head thrown); the blows that cut it.
.float vr_headless;
.float vr_limbcut; // its ragdoll's bones cut off (bits; the engine sets it, vr_box3d.cpp cutLimb): a saved game's made so again (vr_limbs.qc)
void(entity e) VR_Decap_Killed;
// Limb gore (vr_limbs.qc).
float vr_decap_limb;
vector vr_limb_blast_at;
entity vr_limb_blast_by;
float vr_limb_blast_time;
float(entity e, entity by, vector xVel, float xHow, float xBone, float xLive) VR_Limb_Cut;
float(entity e, float xLive) VR_Limb_BlastPops;
void(float dm) VR_Limb_GibAll;
float() VR_Limb_GibsWanted;
float(entity targ, entity attacker, float xDamage) VR_Decap_Blow;
void(entity targ) VR_Decap_After;

// Knockdowns (vr_knockdown.qc): a shove knocked it down, a ragdoll while alive (the engine's vr_box3d.cpp, "Knockdowns").
.float vr_knockdown;         // 1 down (a ragdoll), 2 getting up; 0 neither (the engine reads it)
.float vr_kd_since;          // when it went down
.float vr_kd_until;          // when it next tries to get up
.vector vr_kd_mins;          // its standing box, for getting up
.vector vr_kd_maxs;
.float vr_kd_movetype;       // its movetype before
.float vr_kd_chance_h;       // its class's chance (a cvar handle; 0: never knocked down)
.float vr_kd_frame;          // the get-up's first frame: its own fall-and-rise's lowest, or a death's last (backwards)
.float vr_kd_frame2;         // another to choose from (a second death's last; -1 none)
.void() vr_kd_getup;         // its own get-up's frame function from vr_kd_frame on; none: the death backwards
.float vr_kd_rev_end;        // backwards: where the death vr_kd_frame ends starts (its first frame),
.float vr_kd_rev_end2;       // and vr_kd_frame2's
.float vr_kd_rev_to;         // backwards: the frame it stops at
.void() vr_kd_next;          // its own get-up: the frame function due next (VR_Knockdown_Step)
void(float xChanceH, float xFrame, void() xGetup) VR_Knockdown_SetupOwn;
void(float xChanceH, float xLastA, float xFirstA, float xLastB, float xFirstB) VR_Knockdown_SetupReverse;
float(entity e, vector xForward, float xKind, float xHand) VR_Knockdown_Try;
void() VR_Knockdown_FastForwardDeath;
float(entity c, float xHand, vector xAt) VR_Decap_CorpseBlow;
float(entity e, float xHand, vector xAt) VR_Decap_Saw;
// Head pops (vr_decap.qc, "Head pops"): the player's shot being fired (W_FireShotgun, W_FireSuperShotgun,
// W_FireLightning): QVR_DECAP_SHOTGUN, _SSG or _LIGHTNING (the blows' kinds: vr_decap.qc's QVR_DECAP_SWING ..); 0 none.
float QVR_DECAP_SHOTGUN = 4;   // a shotgun blast
float QVR_DECAP_SSG = 5;       // a super shotgun blast
float QVR_DECAP_LIGHTNING = 6; // a lightning gun bolt
float vr_decap_weapon;
float vr_decap_pellets; // the blast's pellets fired (FireBulletsImpl; Head pop chance: the share at the head)
vector vr_decap_src;    // and where from
void(entity e, float xDamage, vector xAt, vector xDir) VR_Decap_Pellet;
float(entity targ) VR_Decap_BlastArm;
float(entity targ, vector xAt, vector xDir, float xDamage) VR_Decap_BoltArm;
float(entity m, entity xAxe) VR_Decap_Thrown;
float(entity targ, entity xThing, entity by, float xDamage) VR_Decap_ThrownArm;
float(entity targ, entity inflictor, entity attacker, float xDamage) VR_Decap_QuadArm;
float(entity e) VR_Decap_Roll;
void() VR_Decap_TestFrame;
void() VR_Limb_TestFrame;
void() VR_Decap_ZombieDie;

// The monsters' death code (their th_die), to know what gibs they make.
void() army_die;
void() enf_die;
void() dog_die;
void() demon_die;
void() ogre_die;
void() knight_die;
void() hknight_die;
void() rknight_die; // (Dawn of the Machine's ranged knight: vr_mg3_rknight.qc)
void() shalrath_die;
void() sham_die;
void() wiz_die;
void() fish_die;
void() f_death1;
void() gremlin_die;
void() scourge_die;
void() eel_death;
void() eel_death16; // its last frame, looping (VR_Corpse_Arm)

// What gibbing `e` throws (VR_Corpse_Parts), as its death code's gib branch does.
string vr_corpse_head; // "": none, the body just goes (a fish)
string vr_corpse_gib1;
string vr_corpse_gib2;
string vr_corpse_gib3;
float vr_corpse_healthh; // its corpse health's cvar (vr_corpse_health_<monster>; times vr_corpse_health_mult: VR_Corpse_Health)

float(entity e) VR_Corpse_Parts =
{
    vr_corpse_healthh = cvarh_vr_corpse_health_grunt;
    vr_corpse_head = "";
    vr_corpse_gib1 = "progs/gib1.mdl";
    vr_corpse_gib2 = "progs/gib2.mdl";
    vr_corpse_gib3 = "progs/gib3.mdl";

    if(e.th_die == army_die)
    {
        vr_corpse_head = "progs/h_guard.mdl";
    }
    else if(e.th_die == enf_die)
    {
        vr_corpse_head = "progs/h_mega.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_enforcer;
    }
    else if(e.th_die == dog_die)
    {
        vr_corpse_head = "progs/h_dog.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_dog;
        vr_corpse_gib1 = "progs/gib3.mdl";
        vr_corpse_gib2 = "progs/gib3.mdl";
    }
    else if(e.th_die == demon_die)
    {
        vr_corpse_head = "progs/h_demon.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_fiend;
        vr_corpse_gib2 = "progs/gib1.mdl";
        vr_corpse_gib3 = "progs/gib1.mdl";
    }
    else if(e.th_die == ogre_die)
    {
        vr_corpse_head = "progs/h_ogre.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_ogre;
        vr_corpse_gib1 = "progs/gib3.mdl";
        vr_corpse_gib2 = "progs/gib3.mdl";
    }
    else if(e.th_die == rknight_die) // (Dawn of the Machine's ranged knight: the death knight's head and corpse health)
    {
        vr_corpse_head = "progs/h_hellkn.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_hellknight;
    }
    else if(e.th_die == knight_die || e.th_die == hknight_die)
    {
        vr_corpse_head = e.th_die == knight_die ? "progs/h_knight.mdl" : "progs/h_hellkn.mdl";
        vr_corpse_healthh = e.th_die == knight_die ? cvarh_vr_corpse_health_knight : cvarh_vr_corpse_health_hellknight;
        if(e.spawnflags & 2) // a statue
        {
            vr_corpse_gib1 = VR_StatueFile("progs/statgib1.mdl", "progs/vr_rock1.mdl");
            vr_corpse_gib2 = VR_StatueFile("progs/statgib2.mdl", "progs/vr_rock2.mdl");
            vr_corpse_gib3 = VR_StatueFile("progs/statgib3.mdl", "progs/vr_rock3.mdl");
        }
    }
    else if(e.th_die == shalrath_die)
    {
        vr_corpse_head = "progs/h_shal.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_vore;
    }
    else if(e.th_die == sham_die)
    {
        vr_corpse_head = "progs/h_shams.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_shambler;
    }
    else if(e.th_die == wiz_die)
    {
        vr_corpse_head = "progs/h_wizard.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_scrag;
        vr_corpse_gib1 = "progs/gib2.mdl";
        vr_corpse_gib3 = "progs/gib2.mdl";
    }
    else if(e.th_die == fish_die || e.th_die == f_death1)
    {
        vr_corpse_head = "";
        vr_corpse_healthh = cvarh_vr_corpse_health_fish;
    }
    else if(e.th_die == gremlin_die)
    {
        vr_corpse_head = "progs/h_grem.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_gremlin;
        vr_corpse_gib2 = "progs/gib1.mdl";
        vr_corpse_gib3 = "progs/gib1.mdl";
    }
    else if(e.th_die == scourge_die)
    {
        vr_corpse_head = "progs/h_scourg.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_scourge;
    }
    else if(e.th_die == supsham_die)
    {
        vr_corpse_head = "progs/h_shams.mdl"; // (Dawn of the Machine's super shambler: as the shambler)
        vr_corpse_healthh = cvarh_vr_corpse_health_shambler;
    }
    else if(e.th_die == VR_Decap_ZombieDie)
    {
        // A zombie or a mummy beheaded (vr_decap.qc): it died whole, its ragdoll gibbed as a corpse.
        vr_corpse_head = "progs/h_zombie.mdl";
        vr_corpse_healthh = e.classname == "monster_mummy" ? cvarh_vr_corpse_health_mummy : cvarh_vr_corpse_health_zombie;
    }
    else if(e.th_die == eel_death)
    {
        vr_corpse_head = "progs/eelgib.mdl";
        vr_corpse_healthh = cvarh_vr_corpse_health_eel;
        vr_corpse_gib2 = "progs/gib1.mdl";
        vr_corpse_gib3 = "progs/gib1.mdl";
    }
    else
    {
        return FALSE;
    }
    return TRUE;
};

// The damage that gibs the corpse VR_Corpse_Parts was last asked about: its monster's (vr_corpse_health_<monster>) times
// vr_corpse_health_mult.
float() VR_Corpse_Health =
{
    return max(1, cvar_hget(vr_corpse_healthh) * cvar_hget(cvarh_vr_corpse_health_mult));
};

// Never gibbed (vr_corpse_nogib, for testing ragdolls with explosions): 1 corpses, ragdolls and dying bodies; 2 also a
// monster's death (Killed: it dies whole, VR_Corpse_DieWhole).
float(float xLevel) VR_Corpse_NoGib =
{
    return cvar_hget(cvarh_vr_corpse_nogib) >= xLevel;
};

// Killed, before the death code (vr_corpse_nogib 2): a monster whose corpse can be gibbed (VR_Corpse_Parts) dies whole,
// however hard it was hit (its death code gibs it under some health: -40 for most).
void(entity e) VR_Corpse_DieWhole =
{
    if(VR_Corpse_NoGib(2) && (e.flags & FL_MONSTER) && e.health < -1 && VR_Corpse_Parts(e))
    {
        dprint(sprintf("corpse: %s dies whole (vr_corpse_nogib 2; health %.0f)\n", e.classname, e.health));
        e.health = -1;
    }
};

// From Killed, after the death code: a monster that died whole is watched until it lies still.
void(entity e) VR_Corpse_Watch =
{
    if(!cvar_hget(cvarh_vr_corpse_gib) || !(e.flags & FL_MONSTER) || e.vr_gib || e.movetype == MOVETYPE_BOUNCE ||
       e.model == string_null || !VR_Corpse_Parts(e) || e.model == vr_corpse_head)
    {
        return;
    }
    e.vr_corpse = 1;
    e.vr_corpse_since = time;
    e.vr_corpse_frame = -1;
    e.vr_corpse_model = e.modelindex;
    e.think2 = VR_Corpse_Think2;
    e.nextthink2 = time + 0.3;
};

void(entity e) VR_Corpse_Unwatch =
{
    if(e.vr_corpse_dying)
    {
        // Watched no more before it lay still (the option off, 15 s on, brought back): as Quake's dying (not a head:
        // VR_Corpse_Gib clears the flag first).
        e.vr_corpse_dying = 0;
        if(e.health <= 0 && !e.vr_gib)
        {
            e.takedamage = DAMAGE_NO;
            e.solid = SOLID_NOT;
            setorigin(e, e.origin);
        }
    }
    e.vr_corpse = 0;
    if(e.think2 == VR_Corpse_Think2)
    {
        e.think2 = SUB_Null;
        e.nextthink2 = 0;
    }
};

// Dying, not solid now (vr_corpse_dying): it takes damage as a corpse (VR_Corpse_Damage), as much as it will lying
// still (VR_Corpse_Health: its monster's, VR_Corpse_Parts set by VR_Corpse_Watch), touchable as a corpse.
void(entity e) VR_Corpse_ArmDying =
{
    if(!e.vr_corpse_dying)
    {
        VR_Corpse_Parts(e);
        e.vr_corpse_dying = 1;
        e.vr_corpse_hp = VR_Corpse_Health();
        if(e.health >= 0)
        {
            e.health = -1; // dead meat (as VR_Corpse_Arm)
        }
        e.takedamage = DAMAGE_YES; // not DAMAGE_AIM: no autoaim at it
        dprint("corpse: "); dprint(e.classname); dprint(" dying, "); dprint(ftos(e.vr_corpse_hp)); dprint(" to gib\n");
    }
    e.solid = SOLID_NOT_BUT_TOUCHABLE;
    setorigin(e, e.origin);
};

// Dying, a few times a second: once it is not solid (the death code has dropped what it drops),
// its animation over (the frame no longer changes), and at rest, it becomes a corpse.
void() VR_Corpse_Think2 =
{
    // Gibbed by the death code after all, removed, brought back, or the option turned off.
    if(self.vr_corpse != 1 || !cvar_hget(cvarh_vr_corpse_gib) || self.vr_gib || self.health > 0 ||
       self.movetype == MOVETYPE_BOUNCE || self.modelindex != self.vr_corpse_model || time > self.vr_corpse_since + 15)
    {
        VR_Corpse_Unwatch(self);
        return;
    }

    // Not solid (the death code dropped what it drops): it takes damage now, as a corpse (vr_corpse_dying). The death
    // code making it not solid again (an eel's last frame loops) leaves it touchable.
    if(self.solid == SOLID_NOT && cvar_hget(cvarh_vr_corpse_dying))
    {
        VR_Corpse_ArmDying(self);
    }

    float still = self.frame == self.vr_corpse_frame;
    self.vr_corpse_frame = self.frame;
    float resting = (self.flags & FL_ONGROUND) || self.velocity == '0 0 0' || time > self.vr_corpse_since + 3;
    if((self.solid == SOLID_NOT || self.vr_corpse_dying) && still && resting)
    {
        VR_Corpse_Arm(self);
        return;
    }
    self.nextthink2 = time + 0.1;
};

// How far from `xFrom` along `xDir` is clear of walls, up to `xMax`.
float(entity e, vector xFrom, vector xDir, float xMax) VR_Corpse_Clear =
{
    traceline(xFrom, xFrom + xDir * xMax, TRUE, e);
    return trace_fraction < 1 ? xMax * trace_fraction - 1 : xMax;
};

// It lies still: it takes damage now, in a box that lies on the floor.
void(entity e) VR_Corpse_Arm =
{
    float dyingHp = e.vr_corpse_dying ? e.vr_corpse_hp : -1; // the damage it took dying counts
    e.vr_corpse_dying = 0; // (before VR_Corpse_Unwatch: it is a corpse now, not undone)
    VR_Corpse_Unwatch(e);

    vector lo = e.mins;
    vector hi = e.maxs;
    float height = max(12, hi_z - lo_z);
    float r = max(12, min(max(hi_x, height * 0.4), hi_x + 16));
    lo_x = -r;
    lo_y = -r;
    hi_x = r;
    hi_y = r;
    if(e.flags & FL_SWIM)
    {
        // Floating in the water: round its middle.
        float half = min(16, max(8, height * 0.25));
        lo_z = -half;
        hi_z = half;
    }
    else
    {
        hi_z = lo_z + min(24, max(12, height * 0.3));
    }

    // Not into a wall: each side only as far as it is clear, and at least the standing monster's
    // (it stood there).
    vector mid = e.origin;
    mid_z = mid_z + (lo_z + hi_z) * 0.5;
    hi_x = max(e.maxs_x, VR_Corpse_Clear(e, mid, '1 0 0', r));
    lo_x = min(e.mins_x, -VR_Corpse_Clear(e, mid, '-1 0 0', r));
    hi_y = max(e.maxs_y, VR_Corpse_Clear(e, mid, '0 1 0', r));
    lo_y = min(e.mins_y, -VR_Corpse_Clear(e, mid, '0 -1 0', r));

    // Its monster's corpse health (VR_Corpse_Parts: vr_corpse_health_<monster>).
    VR_Corpse_Parts(e);

    e.vr_corpse = 2;
    e.vr_corpse_hp = VR_Corpse_Health();
    if(dyingHp >= 0)
    {
        e.vr_corpse_hp = min(e.vr_corpse_hp, dyingHp);
    }
    if(e.health >= 0)
    {
        e.health = -1; // dead meat: a rocket's direct hit looks for some health (T_MissileTouch)
    }
    e.solid = SOLID_NOT_BUT_TOUCHABLE;
    e.takedamage = DAMAGE_YES; // not DAMAGE_AIM: no autoaim at it
    setsize(e, lo, hi);
    if(e.think == eel_death16)
    {
        // A rogue eel's last death frame loops, making it SOLID_NOT every 0.1 s: shots and hands
        // went through its corpse. It lies still now: the loop is over.
        e.think = SUB_Null;
        e.nextthink = 0;
    }

    dprint("corpse: "); dprint(e.classname); dprint(" lies still, "); dprint(ftos(e.vr_corpse_hp));
    dprint(" to gib, box "); dprint(vtos(lo)); dprint(" "); dprint(vtos(hi)); dprint("\n");
};

// Positional damage (weapons.qc) takes a share off a shot on the legs or the limbs, and a corpse's
// low, wide box is all legs and limbs to it: the share is given back (a corpse has none).
float(entity targ) VR_Corpse_PositionalFactor =
{
    if(cvar_hget(cvarh_vr_positional_damage) == 0)
    {
        return 1;
    }

    float head = cvar_hget(cvarh_vr_headshot_mult);
    float limb = cvar_hget(cvarh_vr_limbshot_mult);
    float leg = cvar_hget(cvarh_vr_legshot_mult);
    float f = 1;

    // A shotgun blast's pellets (ApplyMultiDamage).
    if(vr_hitpellets > 0)
    {
        float body = vr_hitpellets - vr_hitpellets_head - vr_hitpellets_limb - vr_hitpellets_leg;
        f = (body + vr_hitpellets_head * head + vr_hitpellets_limb * limb + vr_hitpellets_leg * leg) / vr_hitpellets;
    }
    // One shot.
    else if(vr_hitpart_target == targ && vr_hitpart_time == time)
    {
        if(vr_hitpart == 1)
        {
            f = head;
        }
        else if(vr_hitpart == 2)
        {
            f = limb;
        }
        else if(vr_hitpart == 3)
        {
            f = leg;
        }
    }
    return max(0.1, f);
};

// Small gibs (vr_smallgibs.qc): torn out by a hit; with a gibbing.
void(entity targ, entity inflictor, entity attacker, float xDamage) VR_SmallGib_Hit;
void(float dm) VR_SmallGib_Gibbed;
float vr_sgib_corpsegib; // inside VR_Corpse_Gib: a gibbing's small gibs are a corpse's (vr_smallgibs.qc)

// The kinds of damage corpses and ragdolls take (VR_Corpse_DamageKind, vr_smallgibs.qc), each times its vr_corpse_dmg_*.
float QVR_CDMG_OTHER = 0;
float QVR_CDMG_SHOTS = 1;      // shotguns, the grunts' guns
float QVR_CDMG_NAILS = 2;      // nails, lasers, spikes
float QVR_CDMG_EXPLOSIONS = 3; // rockets, grenades, plasma, blasts
float QVR_CDMG_LIGHTNING = 4;  // the lightning gun, Mjolnir, a discharge
float QVR_CDMG_BLUNT = 5;      // a crowbar, a gun or Mjolnir swung, a pommel
float QVR_CDMG_FISTS = 6;      // an empty fist
float QVR_CDMG_BLADES = 7;     // the axe, a sword (swung, thrown, stuck in it)
float QVR_CDMG_CHAINSAW = 8;
float QVR_CDMG_PROPS = 9;      // thrown or flung things, deflected shots, a prop in the fist
float QVR_CDMG_FIRE = 10;      // burning (vr_burning.qc)
float QVR_CDMG_BASH = 11;      // bashes, shoves, headbutts
float(entity inflictor, entity attacker) VR_Corpse_DamageKind;
string(float xKind) VR_Corpse_DamageKindName;
float(float xKind) VR_Corpse_DamageKindMult;

// Its damage, from T_Damage (instead of Quake's): gibbed at no health.
float(entity targ, entity inflictor, entity attacker, float damage) VR_Corpse_Damage =
{
    if(!cvar_hget(cvarh_vr_corpse_gib))
    {
        targ.vr_corpse = 0;
        targ.vr_corpse_dying = 0;
        targ.takedamage = DAMAGE_NO;
        targ.solid = SOLID_NOT;
        setorigin(targ, targ.origin);
        return FALSE;
    }
    if(damage <= 0)
    {
        return FALSE;
    }

    if(attacker.super_damage_finished > time)
    {
        damage = damage * 4;
    }
    damage = damage / VR_Corpse_PositionalFactor(targ);

    // A headshot on a corpse pops its head, leaving the physical body in place.
    // Use the same weapon toggles and head-share test as a living killing blow.
    // A shot at a corpse's limb pops it the same way (Limb gore, vr_limbs.qc: vr_decap_limb its joint).
    if(vr_decap_ent == targ && vr_decap_time == time && (vr_decap_limb >= 0 || !targ.vr_headless))
    {
        vr_decap_ent = world;
        if(vr_decap_limb >= 0)
        {
            if(VR_Decap_Roll(targ) && VR_Limb_Cut(targ, vr_decap_by, vr_decap_vel, vr_decap_how, vr_decap_limb, FALSE))
            {
                return TRUE;
            }
        }
        else if(VR_Decap_Roll(targ) && VR_Decap_Cut(targ, vr_decap_by, vr_decap_vel, vr_decap_how))
        {
            return TRUE;
        }
    }
    // An explosion pops a corpse's limbs near it by chance (vr_limbs_blast; vr_limbs.qc).
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        VR_Limb_BlastPops(targ, FALSE);
    }

    VR_SmallGib_Hit(targ, inflictor, attacker, damage); // (the chainsaw always; Quad always)

    // Its kind's share (vr_corpse_dmg_<kind>): how much it takes to gib a corpse with each weapon. The small gibs above
    // keep the whole hit (their own kinds' multipliers).
    float kind = VR_Corpse_DamageKind(inflictor, attacker);
    float kindMult = VR_Corpse_DamageKindMult(kind);
    damage = damage * kindMult;
    string kindName = VR_Corpse_DamageKindName(kind);

    // Never gibbed (vr_corpse_nogib): it takes the hit (blood, small gibs, the engine's push) but loses no health.
    if(VR_Corpse_NoGib(1))
    {
        dprint(sprintf("corpse: %s hit by %s (%s x%.2f) for %.1f, never gibbed (vr_corpse_nogib)\n", targ.classname,
            inflictor.classname, kindName, kindMult, damage));
        return TRUE;
    }

    targ.vr_corpse_hp = targ.vr_corpse_hp - damage;
    dprint(sprintf("corpse: %s hit by %s (%s x%.2f) for %.1f, %.1f left\n", targ.classname,
        inflictor.classname != "" ? inflictor.classname : inflictor.model, kindName, kindMult, damage,
        max(0, targ.vr_corpse_hp)));
    if(targ.vr_corpse_hp <= 0)
    {
        VR_Corpse_Gib(targ, inflictor, attacker, damage);
    }
    return TRUE;
};

// Gibbed: its gibs and head thrown (the head is the monster itself, from now a gib: vr_carry.qc),
// or, with no head (a fish), the body gone.
void(entity c, entity inflictor, entity attacker, float damage) VR_Corpse_Gib =
{
    VR_Highlight_CorpseGib(c, inflictor, attacker);
    VR_Corpse_Parts(c);

    vector dir = '0 0 0';
    if(inflictor != world && inflictor != c)
    {
        dir = normalize((c.absmin + c.absmax) * 0.5 - (inflictor.absmin + inflictor.absmax) * 0.5);
    }
    dprint("corpse: "); dprint(c.classname); dprint(" gibbed (kills: "); dprint(ftos(killed_monsters));
    dprint(sprintf(", gib speed x%.2f)\n", vr_gib_speed > 0 ? vr_gib_speed : 1));

    entity oldSelf = self;
    self = c;
    vr_sgib_corpsegib = TRUE;

    self.vr_corpse = 0;
    self.vr_corpse_dying = 0; // (gibbed dying: its death code stops, ThrowHead's think instead)
    self.vr_corpse_hp = 0;
    self.takedamage = DAMAGE_NO;
    damage_inflictor = inflictor; // the gibs fly away from it (VelocityForDamage)
    damage_attacker = attacker;
    float dm = damage >= 60 ? -100 : -40; // a hard blow throws them further

    VR_BloodMist((self.absmin + self.absmax) * 0.5, dir, 1.5);
    sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
    ThrowGib(vr_corpse_gib1, dm);
    ThrowGib(vr_corpse_gib2, dm);
    ThrowGib(vr_corpse_gib3, dm);

    if(vr_corpse_head != "" && !self.vr_headless) // (beheaded: its head is gone already, vr_decap.qc)
    {
        ThrowHead(vr_corpse_head, dm);
        if(self.vr_gib)
        {
            self.solid = SOLID_NOT_BUT_TOUCHABLE; // grabbable (VR_MakeGibGrabbable)
        }
    }
    else
    {
        VR_SmallGib_Gibbed(dm); // (ThrowHead's, with a head)
        VR_Limb_GibAll(dm); // (and its own limbs: vr_gib_limbs, vr_limbs.qc)
        self.solid = SOLID_NOT;
        self.model = string_null;
        self.modelindex = 0;
        setorigin(self, self.origin);
        self.think = SUB_Remove;
        self.nextthink = time + 0.1;
    }

    vr_sgib_corpsegib = FALSE;
    self = oldSelf;
};

float(entity e, entity p, float xHand) VR_Ragdoll_HandSpares; // (vr_carry.qc)
float(entity e, entity p) VR_Throw_SparedBlows;
float(entity p, float xHand) VR_Throw_HandMayStrike;

// Every player frame, for each hand (self is the player; VR_Carry_HandFrame): a hand swung at a
// blow's speed (vr_melee_speed) through a corpse, or what it holds, strikes it.
void(entity xFrom, float xChan, entity xTarg) VR_Blunt_HitLayer; // (vr_crates.qc)

void(float xHand) VR_Corpse_StrikeFrame =
{
    if(!cvar_hget(cvarh_vr_corpse_gib) || !self.ishuman || self.health <= 0)
    {
        return;
    }

    // A blow's speed first: most frames the hand is slower.
    vector vel = VRGetHandVel(xHand);
    float speed = vlen(vel);
    if(speed < max(0.1, cvar_hget(cvarh_vr_melee_speed)))
    {
        return;
    }

    // As the hands' blows (vr_melee.qc): an empty hand steadying the other's weapon doesn't strike (the weapon's hand
    // does: the first hand to strike takes the swing). A gun carried by its pump strikes (round 21).
    if(VR_Melee_Thing(self, xHand) == VR_MTHING_NONE)
    {
        return;
    }

    vector dir = normalize(vel);
    vector hand = VRGetHandPos(xHand);
    entity c = world;
    vector at = hand;

    // The hand, along its swing.
    traceline(hand - dir * 6, hand + dir * 8, MOVE_HITGIBS | MOVE_HITMODEL_MELEE, self); // VR: the corpse's model (vr_hit_precise)
    if(trace_ent.vr_corpse == 2 || trace_ent.vr_corpse_dying)
    {
        c = trace_ent;
        at = trace_endpos;
    }
    // What it holds, from the hand to its end.
    else if(!VRIsHandWeaponless(self, xHand))
    {
        traceline(hand, VRGetMuzzlePos(xHand) + dir * 4, MOVE_HITGIBS | MOVE_HITMODEL_MELEE, self);
        if(trace_ent.vr_corpse == 2 || trace_ent.vr_corpse_dying)
        {
            c = trace_ent;
            at = trace_endpos;
        }
    }

    if(c == world || !c.takedamage || time < c.vr_corpse_struck + 0.35)
    {
        return;
    }
    // Not a ragdoll this hand holds or just let go of, nor one just thrown with both hands (a throw is not a blow:
    // ROUND21.md, "Two-handed ragdoll throws").
    if(VR_Ragdoll_HandSpares(c, self, xHand) || VR_Throw_SparedBlows(c, self) || !VR_Throw_HandMayStrike(self, xHand))
    {
        return;
    }
    c.vr_corpse_struck = time;

    // A blade's slash at its head beheads it instead (vr_decap.qc): no corpse damage.
    if(VR_Decap_CorpseBlow(c, xHand, at))
    {
        haptic(xHand, 0.0, 0.15, 60, 0.9);
        return;
    }

    float dmg = VR_Gib_StrikeDamage(speed);
    if(!VRIsHandWeaponless(self, xHand))
    {
        dmg = dmg * 1.5;
    }
    VR_SoundVaried(self, VRGetHandChannel(xHand), VRIsHandWeaponless(self, xHand) ? "fisthit.wav" : "player/axhit2.wav", 0.8, ATTN_NORM);
    VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), c);
    haptic(xHand, 0.0, 0.1, 60, 0.7);
    SpawnBlood(at, dir * 40, dmg);

    VR_SetHitKind(QVR_HITKIND_MELEE, xHand, at, dir);
    T_Damage(c, self, self, dmg);
    VR_ClearHitKind();

    // A lit torch sets it on fire (vr_burning.qc).
    entity t = VR_Carry_Held(self, xHand);
    if(t != world && VR_WallTorch_IsTorch(t) && t.wt_state == 1 && t.wt_fire > 0)
    {
        VR_Burn_Ignite(c, self, at, dir, VR_BURN_BLOW);
    }
};

float() getVRMeleeBloodlust =
{
    return cvar_hget(cvarh_vr_melee_bloodlust);
}

/*
============
T_Damage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
============
*/
// VR: shoved off ledges. A walking monster (MOVETYPE_STEP) only moves by its velocity while it is
// off the ground: Quake's step physics stops it dead as it lands (FL_ONGROUND), and its AI's walks
// (walkmove, movetogoal) never step off an edge. So a push was the little hop it gives, a few units
// at vr_push's strength, and a monster pushed to an edge landed on its lip and stood there, half
// over the drop. Now a hard push (VR_SHOVE_MINSPEED and more) also slides it along the floor after
// the hop, slowing down (VR_SHOVE_FRICTION), its feet just clear of the floor, so its AI does not
// walk it back meanwhile (a monster off the ground does not walk). Over an edge -- nothing under the
// middle of its feet -- it keeps going and falls, and on landing takes damage for the drop
// (VR_SHOVE_SAFEFALL units and more; lava and slime burn). Then its AI goes on as before.

float VR_SHOVE_MINSPEED = 100; // units/s: a softer push is only the hop
float VR_SHOVE_FRICTION = 500; // units/s/s
float VR_SHOVE_SAFEFALL = 150; // units of drop without damage; 0.4 a unit beyond (any monster's fall: VR_Monster_Fall)

// VR: a live monster's fall damage, whatever threw it (a shove over a ledge, the grappling hook, a blast, a knockdown's
// throw): from the speed it came down at, the drop that speed takes (v^2 / 2g) beyond VR_SHOVE_SAFEFALL, 0.4 a unit (as
// a shove's fall was measured), so the harder it lands the more it takes. Called by the engine: standing, as it lands
// (SV_Physics_Step: its speed down); knocked down (vr_knockdown.qc), its ragdoll's hardest hit on the level in a frame
// (vr_box3d.cpp: that hit's speed along the vertical). A burst of hits (the ragdoll's limbs, a bounce) counts once.
.float vr_fall_time; // until when a landing does not hurt it again
void(entity m, float xSpeed) VR_Monster_Fall =
{
    if(m.health <= 0 || !(m.flags & FL_MONSTER) || !m.takedamage || time < m.vr_fall_time)
    {
        return;
    }
    float drop = xSpeed * xSpeed / (2 * max(1, cvar("sv_gravity")));
    if(drop <= VR_SHOVE_SAFEFALL)
    {
        return;
    }
    m.vr_fall_time = time + 0.5;
    float dmg = (drop - VR_SHOVE_SAFEFALL) * 0.4;
    dprint(sprintf("fall: %s landed at %g u/s (a %g unit drop): %g damage%s\n", m.classname, xSpeed, drop, dmg,
        m.vr_knockdown ? " (knocked down)" : ""));
    T_Damage(m, world, world, dmg);
};

.entity vr_shove; // on a monster: its slide (a "vr_shove" entity; world when none)

void() VR_Shove_Think;

// A push of `e` along `xDir` at `xSpeed` (units/s, horizontal) starts a slide (VR_Push).
void(entity e, vector xDir, float xSpeed) VR_Shove_Start =
{
    entity s = e.vr_shove;
    if(s == world || s.classname != "vr_shove" || s.owner != e)
    {
        s = spawn();
        s.classname = "vr_shove";
        s.owner = e;
        s.pos1 = e.origin; // the highest it has been since it was last on the ground
        e.vr_shove = s;
    }
    s.movedir = xDir;
    s.speed = xSpeed;
    s.ltime = time + 3;
    s.think = VR_Shove_Think;
    s.nextthink = time;
};

// Landed after a drop of `xDrop` units (self is the slide).
void(entity m, float xDrop) VR_Shove_Landed =
{
    // At its feet (its origin, the middle of its box, is above shallow slime: e1m1's burned nothing).
    float contents = pointcontents(m.origin + '0 0 1' * (m.mins_z + 1));
    float dmg = 0;
    if(contents == CONTENT_LAVA)
    {
        dmg = 100;
    }
    else if(contents == CONTENT_SLIME)
    {
        dmg = 30;
    }
    // (The drop's own damage is any landing's now: the engine's VR_Monster_Fall, from its speed.)
    if(xDrop > 24 || dmg > 0)
    {
        dprint("shove: "); dprint(m.classname); dprint(" landed from "); dprint(ftos(xDrop)); dprint(" units, ");
        dprint(ftos(dmg)); dprint(" damage\n");
    }
    if(dmg > 0)
    {
        T_Damage(m, world, world, dmg);
    }
};

void() VR_Shove_End =
{
    if(self.owner.vr_shove == self)
    {
        self.owner.vr_shove = world;
    }
    remove(self);
};

// Once a frame while the monster slides or falls.
void() VR_Shove_Think =
{
    entity m = self.owner;
    if(m.vr_shove != self || m.health <= 0 || m.movetype != MOVETYPE_STEP || (m.flags & (FL_FLY | FL_SWIM)) ||
       !(m.flags & FL_MONSTER) || time > self.ltime + 3)
    {
        VR_Shove_End();
        return;
    }
    self.nextthink = time;

    // What is right under the middle of its feet.
    vector foot = m.origin;
    foot_z = foot_z + m.mins_z;
    traceline(foot + '0 0 1', foot - '0 0 4', TRUE, m);
    float supported = trace_fraction < 1 && trace_plane_normal_z > 0.7;
    float gap = foot_z - trace_endpos_z;

    // Off the ground (the hop, or falling): Quake's physics carries it. Its highest, for the drop.
    if(!(m.flags & FL_ONGROUND) && (!supported || gap > 0.5 || m.velocity_z > 1))
    {
        self.pos1_z = max(self.pos1_z, m.origin_z);
        self.cnt = 1;
        return;
    }

    // Down again.
    if(self.cnt && (m.flags & FL_ONGROUND))
    {
        self.cnt = 0;
        VR_Shove_Landed(m, self.pos1_z - m.origin_z);
        if(m.health <= 0)
        {
            VR_Shove_End();
            return;
        }
    }

    // On the ground: a drop is measured from here (not from where it was shoved: a ramp down is none).
    self.pos1 = m.origin;

    // As fast as it goes (a wall stops it).
    self.speed = min(self.speed, max(0, m.velocity_x * self.movedir_x + m.velocity_y * self.movedir_y));

    // Standing on an edge's lip, the middle of its feet over the drop: it goes on over.
    if(!supported)
    {
        m.flags = m.flags - (m.flags & FL_ONGROUND);
        self.speed = max(self.speed, 80);
        m.velocity = self.movedir * self.speed;
        return;
    }

    if(self.speed < 20 || time > self.ltime)
    {
        m.velocity_x = 0;
        m.velocity_y = 0; // it settles on the floor (its velocity is no longer the slide)
        VR_Shove_End();
        return;
    }

    // Sliding, slowing down: its feet just clear of the floor (gravity undone for this frame).
    self.speed = max(0, self.speed - VR_SHOVE_FRICTION * frametime);
    m.flags = m.flags - (m.flags & FL_ONGROUND);
    m.velocity = self.movedir * self.speed + '0 0 1' * (cvar_hget(cvarh_sv_gravity) * (m.gravity ? m.gravity : 1) *
                                                        frametime);
};

// VR: pushes `e` along `dir` (made horizontal) at `speed` units/s, lifted by `up`: players and
// walking monsters are thrown (their velocity), flying and swimming monsters stepped aside. Monsters
// bigger than a grunt move less (by their size), smaller ones more.
void(entity e, vector dir, float speed, float up) VR_Push =
{
    speed = speed * cvar_hget(cvarh_vr_push);
    up = up * cvar_hget(cvarh_vr_push);
    dir_z = 0;
    dir = normalize(dir);
    if(speed <= 0 || dir == '0 0 0' || e.solid == SOLID_BSP)
    {
        return;
    }

    if(e.flags & FL_MONSTER)
    {
        float mass = max(0.6, vlen(e.size) / vlen('32 32 56'));
        speed = speed / mass;
        up = up / mass;
        if(e.flags & (FL_FLY | FL_SWIM))
        {
            entity oldSelf = self;
            self = e;
            walkmove(vectoyaw(dir), speed * 0.08);
            self = oldSelf;
            return;
        }
        e.velocity = dir * speed + '0 0 1' * up;
        e.flags = e.flags - (e.flags & FL_ONGROUND);
        if(e.health > 0 && speed >= VR_SHOVE_MINSPEED && e.movetype == MOVETYPE_STEP && !e.vr_knockdown)
        {
            VR_Shove_Start(e, dir, speed);
        }
        return;
    }

    if(e.movetype == MOVETYPE_WALK)
    {
        e.velocity = e.velocity + dir * speed + '0 0 1' * up;
        e.flags = e.flags - (e.flags & FL_ONGROUND);
    }
};

//MED 01/10/97 added empathyused variable
float empathyused;

// VR: gib speed by what gibbed it (vrfiringrange_2026-10-01_11-35-41; ROUND21.md, "Gib speed by what gibbed it"): a melee
// blow (vr_gib_speed_melee), a thrown thing or a light weapon (vr_gib_speed_light: the shotguns, nails, an enforcer's
// laser, a stuck axe bleeding it), or the rest (vr_gib_speed_heavy: explosions, rockets, grenades, lightning, plasma,
// monsters' blows: Quake's). Set for the length of each T_DamageImpl, so every gibbing it causes (a monster's death, a
// corpse's: VR_Corpse_Gib) throws at that speed (VelocityForDamage); 0 between blows (Quake's).
float vr_gib_speed;

float QVR_GIBKIND_HEAVY = 0;
float QVR_GIBKIND_MELEE = 1;
float QVR_GIBKIND_LIGHT = 2;

float(entity inflictor, entity attacker, float isVRMelee) VR_GibKind =
{
    float k = vr_hitkind;
    if(k == QVR_HITKIND_SPLASH)
    {
        return QVR_GIBKIND_HEAVY;
    }
    if(isVRMelee || k == QVR_HITKIND_MELEE || k == QVR_HITKIND_HEADBUTT || k == QVR_HITKIND_BASH ||
       k == QVR_HITKIND_SHOVE)
    {
        return QVR_GIBKIND_MELEE;
    }
    if(k == QVR_HITKIND_BLEED)
    {
        return QVR_GIBKIND_LIGHT;
    }
    if(k == QVR_HITKIND_SHOT)
    {
        // Lightning (the gun's, Mjolnir's: vr_hitweapon) is heavy; the shotguns, the grunts' guns light.
        if(vr_hitweapon != "" ||
           (vr_hithand >= 0 && (attacker.flags & FL_CLIENT) && VRGetEntWeapon(attacker, vr_hithand) == WID_LIGHTNING))
        {
            return QVR_GIBKIND_HEAVY;
        }
        return QVR_GIBKIND_LIGHT;
    }
    if(inflictor == world || inflictor == attacker)
    {
        // A player's blow with nothing between (a kick, a landing): light. A monster's own: Quake's.
        return (attacker.flags & FL_CLIENT) ? QVR_GIBKIND_LIGHT : QVR_GIBKIND_HEAVY;
    }
    string c = inflictor.classname;
    if(c == "missile" || c == "grenade" || c == "MultiRocket" || c == "MiniGrenade" || c == "MultiGrenade" ||
       c == "proximity_grenade" || c == "plasma" || c == "hiplaser")
    {
        return QVR_GIBKIND_HEAVY;
    }
    if(c == "spike" || c == "lava_spike" || c == "laser" || c == "thrown_weapon" || inflictor.vr_gib ||
       (inflictor.carry_use && !inflictor.vr_grenade) || inflictor.vr_deflect_time > 0)
    {
        return QVR_GIBKIND_LIGHT; // nails, an enforcer's bolt; thrown, flung or deflected things
    }
    return QVR_GIBKIND_HEAVY;
};

float(entity inflictor, entity attacker, float isVRMelee) VR_GibSpeedScale =
{
    float k = VR_GibKind(inflictor, attacker, isVRMelee);
    float h = cvarh_vr_gib_speed_heavy;
    if(k == QVR_GIBKIND_MELEE)
    {
        h = cvarh_vr_gib_speed_melee;
    }
    else if(k == QVR_GIBKIND_LIGHT)
    {
        h = cvarh_vr_gib_speed_light;
    }
    return min(3, max(0.05, cvar_hget(h)));
};

// Knocked off a hold (ROUND21.md, "Enemy shoves: parry, enforcers, knocked off a hold"): set on a player to make the
// hands let go of a ledge or a rung (or stop pulling up onto one) at the start of the player's next frame, the
// velocity a blast or a shove gave it kept (the engine's vr_climb.cpp reads it and clears it every frame; hanging, the
// climb owns the body's move and a blast's push was lost).
.float vr_climb_knockoff;

// `p` knocked off its holds by `xWhy`: a shove that lands (vr_enemyshove.qc), a blast of at least vr_climb_blast_letgo
// (T_DamageDeal). Nothing if it holds none.
void(entity p, string xWhy) VR_Climb_KnockOff =
{
    if(p.classname != "player")
    {
        return;
    }
    p.vr_climb_knockoff = 1; // (the engine lets go only of holds it has: none, nothing happens)
    if(VRHasBit(p.vrbits0, QVR_VRBITS0_MAINHAND_CLIMBING) || VRHasBit(p.vrbits0, QVR_VRBITS0_OFFHAND_CLIMBING))
    {
        dprint(sprintf("climb: knocked off the hold by %s\n", xWhy));
    }
};

// The damage itself (T_DamageImpl's).
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) T_DamageDeal =
{
    vector    dir;
    entity    oldself;
    float    save;
    float    take;

    if(!targ.takedamage)
    {
        return FALSE;
    }

    // Dawn of the Machine's Bloody Nightmare: the player's blows 80%, the rest on the player 120% (vr_mg3_defs.qc).
    if(MG3_Campaign() && (serverflags & MG3_SF_BN_ACTIVE))
    {
        damage = MG3_BloodyNightmareDamage(targ, attacker, damage);
    }
    // Dawn of the Machine's lava man: 0.8 of anything but the lightning gun and the laser cannon, nothing from Chthon.
    if(targ.mg3_lavaman)
    {
        damage = MG3_LavaManDamage(targ, inflictor, attacker, damage);
        if(damage <= 0)
        {
            return FALSE;
        }
    }

    if(targ.classname == "trigger_explosion")
    {
        if(IsExplosionDamage != 1)
        {
            return FALSE;
        }
    }

    IsExplosionDamage = 0;

    // VR: Debug: Show Damage Numbers (vr_debug_damage_numbers, vr_dummy.qc VR_DamageNumber): a gib's, a crate piece's or
    // a corpse's hit as dealt (their own damage below); everything else's as taken (after armour).
    if(targ.th_die == VR_Gib_Die || targ.vr_cratekind == 3 || targ.vr_corpse == 2 || targ.vr_corpse_dying)
    {
        VR_DamageNumber(targ, inflictor, attacker, damage, 0, isVRMelee);
    }

    // VR: a gib or head only ever bursts (vr_gib_destroy, vr_carry.qc), never dies again.
    if(targ.th_die == VR_Gib_Die)
    {
        return VR_Gib_Damage(targ, inflictor, attacker, damage);
    }

    // VR: a crate's piece only ever bursts into dust (vr_crates.qc).
    if(targ.vr_cratekind == 3)
    {
        return VR_CratePiece_Damage(targ, inflictor, attacker, damage);
    }

    // VR: a corpse is only ever gibbed (vr_corpse_gib), never killed again; a body still dying too (vr_corpse_dying).
    if(targ.vr_corpse == 2 || targ.vr_corpse_dying)
    {
        return VR_Corpse_Damage(targ, inflictor, attacker, damage);
    }

    if(VR_ItemUtil_EntHasItem(targ, IID_SHIELD))
    {
        // Allow some damage to pass through.
        damage = shield_hit(targ, inflictor, attacker, damage);

        if(damage == 0)
        {
            return FALSE;
        }
    }

    if(discharged && targ.wetsuit_finished)
    {
        return FALSE;
    }

    //MED
    // moved damage_attacker down a bit

    // HONEY
    // cg: check if this needs a specific damage type
    if((targ.flags & FL_SPECIFICDAMAGE) && !(attacker.damage_weapon & targ.items)) {
        return FALSE;
    }

    // check for quad damage powerup on the attacker
    if(attacker.super_damage_finished > time)
    {
        damage = damage * 4;
    }

    // VR: the Gameplay menu's damage multipliers.
    if(targ == attacker)
    {
        if(targ.flags & FL_CLIENT)
        {
            damage = damage * cvar_hget(cvarh_vr_damage_self);
        }
    }
    else if((targ.flags & FL_CLIENT) && !(attacker.flags & FL_CLIENT))
    {
        damage = damage * cvar_hget(cvarh_vr_damage_to_player);
    }
    else if((attacker.flags & FL_CLIENT) && (targ.flags & FL_MONSTER))
    {
        damage = damage * cvar_hget(cvarh_vr_damage_to_enemies);
    }

    // rune power up (Black Magic)
    if(deathmatch)
    {
        damage = RuneApplyBlack(damage, attacker);
    }

    //MED
    //check for empathy shields
    if(VR_ItemUtil_EntHasItem(targ, IID_EMPATHY_SHIELDS) &&
       !VR_ItemUtil_EntHasItem(inflictor, IID_EMPATHY_SHIELDS) &&
       (targ != attacker))
    {
        empathyused = 1;
        damage = damage/2;
        T_DamageImpl (attacker,targ,targ,damage,isVRMelee);
        empathyused = 0;
    }

    //MED
    // used by buttons and triggers to set activator for target firing
    damage_attacker = attacker;

    //MED
    // used to keep track of what hit us
    damage_inflictor = inflictor;

    // VR: the motion recorder's events (vr_motion.qc): a hand's or the head's hit.
    VR_Motion_Hit(targ, attacker, damage, isVRMelee);

// save damage based on the target's armor level

    //ZOID--
    if(TeamArmorDam(targ, inflictor, attacker, damage))
    {
        save = ceil(targ.armortype * damage);
    }
    else
    {
        save = 0;
    }
    //--ZOID

    if(save >= targ.armorvalue)
    {
        save = targ.armorvalue;
        targ.armortype = 0;    // lost all armor

        VR_ItemUtil_EntDelItem(targ, IID_ARMOR1);
        VR_ItemUtil_EntDelItem(targ, IID_ARMOR2);
        VR_ItemUtil_EntDelItem(targ, IID_ARMOR3);
    }

    targ.armorvalue = targ.armorvalue - save;
    take = ceil(damage-save);

    // add to the damage total for clients, which will be sent as a single
    // message at the end of the frame
    // FIXME: remove after combining shotgun blasts?
    if(targ.flags & FL_CLIENT)
    {
        targ.dmg_take = targ.dmg_take + take;
        targ.dmg_save = targ.dmg_save + save;
        targ.dmg_inflictor = inflictor;
    }

    // figure momentum add
    if((inflictor != world) && (targ.movetype == MOVETYPE_WALK))
    {
        dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
        dir = normalize(dir);
        targ.velocity = targ.velocity + dir*damage*8;
        // VR: a blast strong enough knocks a player off a ledge or a rung, which it throws (vr_climb_blast_letgo).
        float blastLetGo = cvar_hget(cvarh_vr_climb_blast_letgo);
        if(vr_hitkind == QVR_HITKIND_SPLASH && blastLetGo > 0 && damage >= blastLetGo)
        {
            VR_Climb_KnockOff(targ, sprintf("a blast of %g (%s)", damage, inflictor.classname));
        }
    }

    // check for godmode or invincibility
    if(targ.flags & FL_GODMODE)
    {
        return FALSE;
    }

    if(targ.invincible_finished >= time)
    {
        if(targ.invincible_sound < time)
        {
            sound(targ, CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM);
            targ.invincible_sound = time + 2;
        }

        return TRUE;
    }

    //MED
    if(VR_ItemUtil_EntHasItem(targ, IID_EMPATHY_SHIELDS))
    {
        if(self.empathy_sound < time)
        {
            sound(targ, CHAN_ITEM, "hipitems/empathy2.wav", 1, ATTN_NORM);
            self.empathy_sound = time + 0.5;
        }
    }

    // team play damage avoidance
    //ZOID--
    if(!TeamHealthDam(targ, inflictor, attacker, damage))
    {
        return TRUE; // same team
    }
    //--ZOID

    if(targ.classname == "player")
    {
        SpawnBloodSplash(targ.origin, take / 2.0);
        SpawnBloodSplash(targ.handpos, take / 2.0);
        SpawnBloodSplash(targ.offhandpos, take / 2.0);

        // VR: the buzz a hit gives the controllers is the client's now, with the knock on the drawn hands
        // (vr_painknock.cpp: svc_damage, vr_pain_haptics).
    }

    // rune power up (Earth Magic)
    if(deathmatch)
    {
        take = RuneApplyEarth(take, targ);
    }

    // VR: gore: blood sprayed onto the walls, floor and ceiling behind the hit (vr_gore, vr_carry.qc). The training
    // dummy's too, as a grunt's (vr_dummy_gore: VR_Dummy_Clean).
    VR_DamageNumber(targ, inflictor, attacker, take, save, isVRMelee); // VR: Debug: Show Damage Numbers
    VR_Gore_Hit(targ, inflictor, take);
    VR_SmallGib_Hit(targ, inflictor, attacker, damage); // VR: small gibs torn out (vr_smallgibs.qc)
    VR_Wound_Hit(targ, inflictor, attacker, take); // VR: painted on its model (vr_wounds.qc)

    // VR: the firing range's training dummy reports the hit and takes no harm (vr_dummy.qc); with vr_dummy_gib, a blow
    // that would kill a grunt makes it the grunt it stands for, and the blow is dealt to it as to one: it dies, is
    // beheaded, has its head popped or is gibbed as a grunt is.
    if(VR_IsDummy(targ) && !VR_Dummy_Hit(targ, inflictor, attacker, damage, isVRMelee))
    {
        return TRUE;
    }

    // QVR: Dawn of the Machine's health thresholds, read before the blow as upstream (vr_mg3_triggers.qc)
    if(targ.health_target != string_null)
        MG3_HealthTarget(targ, attacker);

    // do the damage
    targ.health = targ.health - take;

    if(targ.health <= 0)
    {
        if(isVRMelee)
        {
            // VR melee takedown
            if(getVRMeleeBloodlust() == QVR_MELEEBLOODLUST_ENABLED)
            {
                float bloodlustAmount = ceil(1 + take / 12) *
                    cvar_hget(cvarh_vr_melee_bloodlust_mult) * 0.75;

                T_Heal(attacker, bloodlustAmount, 1);
            }
        }

        float hlScore = VR_Highlight_Kill(targ, inflictor, attacker, isVRMelee); // VR: the highlight log (vr_highlights.qc)
        string hlWhat = vr_hl_what;
        Killed (targ, attacker);
        VR_Highlight_Killed(targ, hlScore, hlWhat);
        return TRUE;
    }
    else
    {
        if(isVRMelee)
        {
            // VR melee damage
            if(getVRMeleeBloodlust() == QVR_MELEEBLOODLUST_ENABLED)
            {
                T_Heal(attacker, ceil(take / 12), 1);
            }
        }
    }

    // react to the damage
    oldself = self;
    self = targ;
    // VR: knocked down or getting up (vr_knockdown.qc): its pain's sound, not its pain's frames (a ragdoll; or its
    // get-up playing on); down, each hit keeps it down a little longer.
    float kdState = self.vr_knockdown;
    float kdFrame = self.frame;
    void() kdThink = self.think;
    float kdNext = self.nextthink;

    //MED 10/17/96 added charmed stuff
    if((self.flags & FL_MONSTER) && attacker != world)
    {
        // get mad unless of the same class (except for soldiers)
        if(self != attacker && attacker != self.enemy && (self.charmer != attacker))
        {
            if((self.classname != attacker.classname)
            || (self.classname == "monster_army") || (self.classname == "monster_armagon"))
            {
                if(self.classname != "monster_dragon")
                {
                    if(self.enemy.classname == "player")
                    {
                        self.oldenemy = self.enemy;
                    }

                    self.enemy = attacker;
                    FoundTarget();
                }
            }
        }
    }

    if(self.th_pain)
    {
        self.th_pain (attacker, take);
    // nightmare mode monsters don't go into pain frames often
        if(skill == 3)
            self.pain_finished = time + 5;
    }

    if(kdState && self.health > 0)
    {
        self.frame = kdFrame;
        self.think = kdThink;
        self.nextthink = kdNext;
        if(kdState == 1)
        {
            self.vr_kd_until = min(max(self.vr_kd_until, time) + cvar_hget(cvarh_vr_knockdown_hit_time),
                time + cvar_hget(cvarh_vr_knockdown_time_max));
        }
    }

    self = oldself;
    return TRUE;
};

// A grenade shots set off (vr_grenade.qc, "Shooting grenades"): any damage sets it off.
float(entity targ, entity inflictor, entity attacker, float damage, float xMelee) VR_GrenShot_Damage;
float(entity targ, entity inflictor, entity attacker, float damage, float xMelee) VR_WallTorch_Shot; // (vr_walltorch.qc: a wall torch a shot meets comes off its wall)
float(entity t) VR_WallTorch_OnWall; // (vr_walltorch.qc: it is on its wall, not taken yet)

// Deals the damage, with the gibs it may throw at the speed for what dealt it (vr_gib_speed).
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) T_DamageImpl =
{
    if(targ.classname == "func_axe_button" && !MG3_AxeButton_Struck(targ, inflictor, attacker, isVRMelee))
    {
        return FALSE; // only a melee blow opens it (vr_mg3_weapons.qc)
    }
    if(targ.vr_shootnade)
    {
        return VR_GrenShot_Damage(targ, inflictor, attacker, damage, isVRMelee);
    }
    if(VR_WallTorch_OnWall(targ))
    {
        return VR_WallTorch_Shot(targ, inflictor, attacker, damage, isVRMelee); // (its coming off is what the damage does)
    }
    float oldGibSpeed = vr_gib_speed;
    vr_gib_speed = VR_GibSpeedScale(inflictor, attacker, isVRMelee);
    float r = T_DamageDeal(targ, inflictor, attacker, damage, isVRMelee);
    vr_gib_speed = oldGibSpeed;
    return r;
};

// ----------------------------------------------------------------------------
// Parrying (vr_parry): a monster's melee blow on a VR player who holds a weapon (a sword, the axe,
// any gun; one hand or two) about level across in front of them, between them and the monster
// (VR_Parry_Blocks), is partly blocked (vr_parry_reduction): a clang, sparks,
// the hand knocked back. Held in one hand, the weapon may be knocked out of it
// (vr_parry_drop_chance); held in two, never. With parry stamina on (vr_parry_stamina) the chance is
// not used: each parry tires you, and the one that leaves you none knocks the weapon away, one hand or
// two. Any parry opens a counter's window (vr_counter): the next melee attack hits harder.

// Parry stamina and counter-attacks (vr_melee.qc).
float(entity p, float hand, float twoHanded) VR_Stamina_Parry;
void(entity p, float hand) VR_Counter_Open;

// The firing range's training dummy while its own blow is dealt (vr_dummy.qc, vr_dummy_attacks): only then.
.float vr_dummy_striking;

// A grunt's or an enforcer's shove while its blow is dealt (vr_enemyshove.qc): parried as a melee blow; its push is its
// own (VR_EnemyShove_Push), not the parry's nor a melee blow's.
.float vr_eshove_striking;

// Monsters that strike in melee (a blow comes from the monster itself, up close: T_Damage(enemy, self, self)).
// Combat 6 (NOTES.md vrfiringrange_2026-10-02_01-01-02, "not able to parry the Overlord's melee") added the ones
// the list had missed: the Overlord's smash (Dissolution of Eternity's monster_super_wrath), the phantom swordsman's
// sword (monster_sword), the Guardian's stab and smash (monster_morph), the dragon's bite and tail (monster_dragon,
// up close: its tail reaches 250 units, the parry 150), Scourge of Armagon's scorpion's sting (monster_scourge), the
// marksman ogre's chainsaw (monster_ogre_marksman: id1's maps, Honey) and the spawn's leap (monster_tarbaby, as the
// fiend's and the rottweiler's). Not blows: the eel's zap, the zombie's and the mummy's thrown flesh, the lava man's
// and the wraths' missiles, Armagon (no melee). The mummy (no melee of its own) stays listed, harmless.
float(entity e) VR_Parry_MeleeMonster =
{
    string c = e.classname;
    return c == "monster_knight" || c == "monster_hell_knight" || c == "monster_ogre" || c == "monster_demon1" ||
           c == "monster_dog" || c == "monster_shambler" || c == "monster_super_shambler" || c == "monster_fish" || c == "monster_gremlin" ||
           c == "monster_mummy" || c == "monster_super_wrath" || c == "monster_sword" || c == "monster_morph" ||
           c == "monster_dragon" || c == "monster_scourge" || c == "monster_ogre_marksman" || c == "monster_tarbaby" ||
           e.vr_dummy_striking || e.vr_eshove_striking;
};

// Whether `p`'s `hand` holds a weapon with the hand's forward across `toAttacker` (horizontal, unit):
// square to it within 50 degrees, the hand ahead of the body, between the hips and above the head.
// The bash guard's test until round 18 and the parry's until round 16, now only VR_Parry_Probe's "old
// test": a sword's blade or the axe's handle is about 70 degrees off the hand's forward (the aim), so
// a sword or axe held level across had its hand's forward pointing at the attacker, and never parried.
float(entity p, float hand, vector toAttacker) VR_Parry_HandAcross =
{
    if(VRIsHandWeaponless(p, hand))
    {
        return FALSE;
    }

    makeforward(VRGetEntHandRot(p, hand));
    if(fabs(v_forward * toAttacker) > 0.766) // sin(50 degrees)
    {
        return FALSE;
    }

    vector rel = VRGetEntHandPos(p, hand) - p.origin;
    float ahead = rel * toAttacker;
    return ahead > 2 && ahead < 44 && rel_z > -8 && rel_z < 50;
};

// The weapon's line (VR_Parry_Blocks): from its butt (a sword's pommel, a gun's grip) to its far end
// (its muzzle point: a sword's tip, the axe's head, a gun's muzzle); where it was blocked.
vector vr_parry_butt;
vector vr_parry_end;
vector vr_parry_at;
// Where the blow comes from (world; '0 0 0': unknown, the blow's way is used): set around VR_Parry_Blocks by callers
// that know the attacker, for the muzzle's test.
vector vr_parry_from;

// The weapon in `p`'s `hand`, into vr_parry_butt .. vr_parry_end (FALSE: an empty hand): its line from the near end
// to the far end, as the melee lays it out (VR_Melee_Line, vr_melee.qc; a gun carried by its pump too).
float(entity p, float hand) VR_Parry_WeaponLine =
{
    if(VRIsHandWeaponless(p, hand))
    {
        return FALSE;
    }
    float thing = VR_Melee_Thing(p, hand);
    if(thing == VR_MTHING_NONE || thing == VR_MTHING_FIST || !VR_Melee_Line(p, hand, thing))
    {
        return FALSE;
    }
    vr_parry_butt = vr_mline_near;
    vr_parry_end = vr_mline_far;
    return TRUE;
};

// Whether the line `butt` .. `end` held by `p` guards against a blow coming from `toAttacker`
// (horizontal, unit), between the player and the attacker:
// - about level: within vr_parry_angle of horizontal;
// - across: not pointing at the attacker (at least 30 degrees off its line, seen from above);
// - in front: some point of it ahead of the body (of the head or of the player's middle, whichever
//   is further back), up to vr_parry_reach ahead, no further to the side than 90 cm, and from a
//   metre below the eyes (the hips) to 45 cm above them (raised over the head). Into vr_parry_at.
float(entity p, vector butt, vector end, vector toAttacker) VR_Parry_LineBlocks =
{
    vector a = end - butt;
    float len = vlen(a);
    if(len < 1 || fabs(a_z) / len > sin(min(89, max(5, cvar_hget(cvarh_vr_parry_angle))) * 0.0174533))
    {
        return FALSE;
    }
    // Across: at least 30 degrees off the attacker's line. (Round 21: 55 degrees took 4 of the author's 49 poses that are
    // no parry out of the stance, but as many of his parry poses and bashes: his held-out takes did no better.)
    vector flat = a;
    flat_z = 0;
    if(fabs(normalize(flat) * toAttacker) > 0.866) // cos(30 degrees)
    {
        return FALSE;
    }

    float mu = VRMetersToUnits();
    float reach = max(0.3, cvar_hget(cvarh_vr_parry_reach)) * mu;
    vector head = VRGetHeadPos(p);
    vector side;
    side_x = toAttacker_y;
    side_y = -toAttacker_x;
    side_z = 0;
    // In front of the body: the weapon's middle within half a metre of the body's midline (not held out to the side).
    if(fabs((butt + a * 0.5 - (p.origin + head) * 0.5) * side) > 0.5 * mu)
    {
        return FALSE;
    }
    for(float i = 0; i <= 4; i++)
    {
        vector pt = butt + a * (i * 0.25);
        float aheadBody = (pt - p.origin) * toAttacker;
        float aheadHead = (pt - head) * toAttacker;
        float lateral = fabs((pt - (p.origin + head) * 0.5) * side);
        float dz = (pt_z - head_z) / mu;
        if(max(aheadBody, aheadHead) > 0 && min(aheadBody, aheadHead) < reach && lateral < 0.9 * mu && dz > -1 &&
           dz < 0.45)
        {
            vr_parry_at = pt;
            return TRUE;
        }
    }
    return FALSE;
};

// Whether `p`'s `hand` holds a weapon (a sword, the axe, any gun; in one hand or two) across a blow
// coming from `toAttacker` (horizontal, unit): the weapon's own line (VR_Parry_WeaponLine), or, held
// with both hands, the line through the hands (as it was held, whatever way the weapon is drawn).
float(entity p, float hand, vector toAttacker) VR_Parry_Blocks =
{
    if(!VR_Parry_WeaponLine(p, hand))
    {
        return FALSE;
    }
    // The far end (a gun's muzzle, a sword's tip) pointing at the attacker is no parry, whatever line blocks: it must
    // point off to the side, within vr_parry_muzzle_angle of the attacker at most, seen from the weapon (the author's
    // guns held at the dummy 30 degrees off it, his parry poses at least 60; round 21).
    vector toward = toAttacker;
    if(vr_parry_from != '0 0 0')
    {
        toward = vr_parry_from - (vr_parry_butt + vr_parry_end) * 0.5;
        toward_z = 0;
        toward = vlen(toward) > 1 ? normalize(toward) : toAttacker;
    }
    if(normalize(vr_parry_end - vr_parry_butt) * toward >
       cos(min(90, max(0, cvar_hget(cvarh_vr_parry_muzzle_angle))) * 0.0174533))
    {
        return FALSE;
    }
    if(VR_Parry_LineBlocks(p, vr_parry_butt, vr_parry_end, toAttacker))
    {
        return TRUE;
    }
    // (Either hand's weapon: the off hand's may be steadied by the empty main hand. The line is the
    // same either way round.)
    if(VRGet2HAiming(p) && VRIsHandEmpty(p, VRGetOtherHand(hand)))
    {
        vector h = VRGetEntHandPos(p, hand);
        vector o = VR_Melee_HelpGrip(p, VRGetOtherHand(hand)); // where the helping hand holds it
        return VR_Parry_LineBlocks(p, o - (h - o) * 0.5, h + (h - o) * 0.5, toAttacker);
    }
    return FALSE;
};

// QVR: impulse 249 (weapons.qc): prints whether each hand's pose parries a blow from straight ahead
// (and the guard's test), for tuning in the mock.
void() VR_Parry_Probe =
{
    vector to = '0 0 0';
    to_x = cos(self.v_viewangle_y * 0.0174533); // the head's yaw (v_angle is the main hand's aim)
    to_y = sin(self.v_viewangle_y * 0.0174533);
    for(float hand = 0; hand < 2; hand++)
    {
        if(!VR_Parry_WeaponLine(self, hand))
        {
            continue;
        }
        vector a = vr_parry_end - vr_parry_butt;
        vector flat = a;
        flat_z = 0;
        vector side = '0 0 0';
        side_x = to_y;
        side_y = -to_x;
        float lvl = atan2(fabs(a_z), vlen(flat)) * 57.29578;
        float off = atan2(fabs(flat * side), fabs(flat * to)) * 57.29578;
        float blocks = VR_Parry_Blocks(self, hand, to);
        dprint("parry probe: hand "); dprint(ftos(hand)); dprint(" weapon "); dprint(ftos(VRGetEntWeapon(self, hand)));
        dprint(": off level "); dprint(ftos(lvl)); dprint(", off the attacker's line "); dprint(ftos(off));
        dprint(", ahead "); dprint(ftos((vr_parry_end - self.origin) * to));
        dprint(blocks ? ": PARRIES" : ": no parry");
        dprint(VR_Parry_HandAcross(self, hand, to) ? " (old test: yes)" : " (old test: no)");
        dprint(VRGet2HAiming(self) ? ", two hands" : ""); dprint(", health "); dprint(ftos(self.health));
        vector u = normalize(a);
        dprint(", along (ahead left up) "); dprint(ftos(u * to)); dprint(" "); dprint(ftos(u * side * -1)); dprint(" ");
        dprint(ftos(u_z));
        dprint("\n");
    }
};

// Whether `p` holds both empty hands crossed in front (an X, wrists together), facing a blow from
// `toAttacker`: each hand on the other's side of the body, close to the other, ahead of the body,
// between the hips and above the head (vr_parry_unarmed).
float(entity p, vector toAttacker) VR_Parry_ArmsCrossed =
{
    if(!cvar_hget(cvarh_vr_parry_unarmed) || !VRIsHandWeaponless(p, cVR_MainHand) || !VRIsHandWeaponless(p, cVR_OffHand))
    {
        return FALSE;
    }
    vector m = VRGetEntHandPos(p, cVR_MainHand) - p.origin;
    vector o = VRGetEntHandPos(p, cVR_OffHand) - p.origin;
    if(vlen(m - o) > 16)
    {
        return FALSE;
    }
    for(float i = 0; i < 2; i++)
    {
        vector rel = i == 0 ? m : o;
        float ahead = rel * toAttacker;
        if(ahead < 2 || ahead > 40 || rel_z < -4 || rel_z > 50)
        {
            return FALSE;
        }
    }
    // Crossed: the main hand (the right controller, left-handed too) on the off hand's side (the left).
    vector right;
    right_x = toAttacker_y;
    right_y = -toAttacker_x;
    right_z = 0;
    return m * right - o * right < -1;
};

// The bash's guard (a bash is a guard pushed forward: VR_Bash in vr_juice.qc): the parry's pose.

// Whether `p`'s `hand` holds a weapon in a guard, facing `xForward` (horizontal, unit): the parry's
// pose (VR_Parry_Blocks, as for a blow from straight ahead): the weapon's line (or, held with both
// hands, the line through them) about level, across and in front. One hand or two, a sword (by its
// grip or its blade), the axe or a gun held across. Until round 18 the guard was the hand's forward
// across (VR_Parry_HandAcross, the parry's old test), which a sword or the axe held level across
// never passed (their blades are some 70 degrees off the hand's forward). A gun carried by its
// foregrip (the hand-off) is a guard too (round 21).
float(entity p, float hand, vector xForward) VR_Parry_Guard =
{
    return VR_Parry_Blocks(p, hand, xForward);
};

// How the last blow through VR_Parry was met: 0 not parried, 1 with a weapon, 2 with crossed arms (read by the
// training dummy after its own blow: vr_dummy.qc).
float vr_parried;

// A successful parry cuts off the attack, holding its first pain pose briefly before normal AI resumes.
// Replacing think (rather than only attack_finished, which some melee checks ignore) cancels all its later hits.
void() VR_Parry_Recover =
{
    if(self.health <= 0 || self.vr_knockdown)
    {
        return;
    }
    if(time < self.vr_parry_stagger_until)
    {
        self.think = VR_Parry_Recover;
        self.nextthink = self.vr_parry_stagger_until;
        return;
    }
    self.vr_parry_stagger_until = 0;
    self.vr_melee_begun = 0;
    self.attack_state = AS_STRAIGHT;
    dprint(sprintf("parry interrupt: %s recovered\n", self.classname));
    self.th_run();
};

// The dragon must close its attack scheduler without losing its flight route (rogue_dragon.qc).
void() VR_Dragon_ParryInterrupt;

void(entity m, entity p) VR_Parry_Interrupt =
{
    // The dummy manages its own practice timing. Knockdown/death already own the monster's think.
    if(!cvar_hget(cvarh_vr_parry_interrupt) || m.health <= 0 || m.vr_knockdown || m.vr_dummy_striking || !m.th_run)
    {
        return;
    }
    float stagger = min(3, max(0.1, cvar_hget(cvarh_vr_parry_stagger)));
    m.vr_parry_stagger_until = time + stagger;
    m.attack_finished = max(m.attack_finished, m.vr_parry_stagger_until);
    m.attack_state = AS_STRAIGHT;
    m.vr_melee_begun = 0;
    entity oldSelf = self;
    self = m;
    // Leap touches can keep biting after their animation is stopped. The landing callback must also
    // return after T_Damage when interrupted, so it cannot overwrite this recovery think.
    if(m.classname == "monster_demon1" || m.classname == "monster_dog" || m.classname == "monster_tarbaby")
    {
        m.touch = SUB_Null;
        m.movetype = MOVETYPE_STEP;
    }
    // A high pain amount bypasses random flinch thresholds without dealing damage. The spawn's pain can
    // split it (mitosis), and the Guardian's can teleport it: leave their poses alone. An attack still stops
    // when a monster refuses a pain pose.
    if(m.classname == "monster_dragon")
    {
        VR_Dragon_ParryInterrupt();
    }
    else if(m.th_pain && m.classname != "monster_tarbaby" && m.classname != "monster_morph")
    {
        m.pain_finished = 0;
        m.th_pain(p, 1000);
    }
    m.think = VR_Parry_Recover;
    m.nextthink = m.vr_parry_stagger_until;
    self = oldSelf;
    dprint(sprintf("parry interrupt: %s staggered %.2f s\n", m.classname, stagger));
};

float(entity targ, entity inflictor, entity attacker, float damage) VR_Parry =
{
    vr_parried = 0;
    // Some attacks deliver several hits in the same QC callback. None after the successful parry may land,
    // even if that first blow knocked the weapon away or the player lowered their guard.
    if(inflictor == attacker && (attacker.flags & FL_MONSTER) && attacker.vr_parry_stagger_until > time)
    {
        dprint(sprintf("parry interrupt: %s remaining melee hit cancelled\n", attacker.classname));
        return 0;
    }
    if(inflictor != attacker || !(attacker.flags & FL_MONSTER) || damage <= 0 || !cvar_hget(cvarh_vr_parry) ||
       targ.classname != "player" || !VRHandsTracked(targ) || !VR_Parry_MeleeMonster(attacker))
    {
        return damage;
    }

    vector to = attacker.origin - targ.origin;
    to_z = 0;
    // Up close only (a shambler's lightning also comes from the shambler itself).
    if(vlen(to) > 150 || (VR_IsShambler(attacker) && vlen(to) > 110))
    {
        return damage;
    }
    to = normalize(to);

    // The globals the monster's attack code may still use after the damage.
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;

    float hand = -1;
    vr_parry_from = attacker.origin;
    if(VR_Parry_Blocks(targ, cVR_MainHand, to))
    {
        hand = cVR_MainHand;
    }
    else if(VR_Parry_Blocks(targ, cVR_OffHand, to))
    {
        hand = cVR_OffHand;
    }
    else
    {
        // Why not (developer 1): where the blow came from (0 ahead, 90 on the left) and how far.
        float fromYaw = anglemod(vectoyaw(to) - targ.v_viewangle_y + 180) - 180;
        dprint("parry: none: "); dprint(attacker.classname); dprint(" at "); dprint(ftos(rint(fromYaw)));
        dprint(" degrees, "); dprint(ftos(rint(vlen(attacker.origin - targ.origin)))); dprint(" units\n");
    }
    vr_parry_from = '0 0 0';

    // Unarmed: arms crossed in front take the blow on the forearms (both hands knocked, less taken
    // off than with a weapon).
    float armsCrossed = FALSE;
    if(hand < 0)
    {
        armsCrossed = VR_Parry_ArmsCrossed(targ, to);
    }
    // The parry's cooldown, per monster (vr_parry_cooldown; NOTES.md vrfiringrange_2026-10-01_16-44-46, _22-58-06,
    // _22-59-02: the ogre's chainsaw, a knight's swing, one attack several quick blows): a blow parried within that long
    // of its last parried blow is still parried (its damage cut, the knock felt and both pushed apart as ever) but is no
    // new parry: no sound, no sparks, no stamina spent (no weapon knocked away), and above all no new counter window
    // (VR_Counter_Open's shing and glow, which combat 4's quiet parry still replayed for every blow, and its window
    // restarted). Its next attack is a full parry again.
    float quiet = FALSE;
    if((hand >= 0) || armsCrossed)
    {
        // Combat 6 (vr_parry_cooldown_attack): a blow of the same melee attack as its last parried blow is a cooldown
        // blow too, however long between them (the fiend's two claws 0.6 s apart, the Overlord's double smash 0.5 s,
        // the shambler's chained swings): one attack, one parry, one stamina cost. The attack: from its th_melee call
        // (.vr_melee_begun: ai.qc, fight.qc, hip_mon_grem.qc) until it runs again (ai_run clears it), 2 s at most (a
        // monster with its own AI that never calls ai_run).
        float cooldown = max(0, cvar_hget(cvarh_vr_parry_cooldown));
        float sameAttack = (cooldown > 0 && cvar_hget(cvarh_vr_parry_cooldown_attack) && attacker.vr_melee_begun > 0 &&
                            time - attacker.vr_melee_begun < 2 && attacker.vr_parry_last >= attacker.vr_melee_begun);
        quiet = ((cooldown > 0 && attacker.vr_parry_last > 0 && time - attacker.vr_parry_last < cooldown) || sameAttack);
        if(quiet)
        {
            dprint(sprintf("parry: %s again %.2f s after its last parried blow%s: cooldown (damage cut, no sound, no "
                "counter)\n", attacker.classname, time - attacker.vr_parry_last, sameAttack ? ", same attack" : ""));
        }
        attacker.vr_parry_last = time;
    }
    if(quiet)
    {
        // (The same attack: the first parry's sound, sparks, stamina and counter window stand.)
        vr_parried = armsCrossed ? 2 : 1;
        entity oldSelfQ = self;
        self = targ;
        if(armsCrossed)
        {
            haptic(cVR_MainHand, 0.0, 0.3, 60, 1.0);
            haptic(cVR_OffHand, 0.0, 0.3, 60, 1.0);
            handimpact(cVR_MainHand, 4, to * -1);
            handimpact(cVR_OffHand, 4, to * -1);
        }
        else
        {
            haptic(hand, 0.0, 0.3, 90, 1.0);
            handimpact(hand, 5, to * -1);
            if(VRGet2HAiming(self))
            {
                haptic(VRGetOtherHand(hand), 0.0, 0.25, 90, 0.8);
                handimpact(VRGetOtherHand(hand), 3, to * -1);
            }
        }
        self = oldSelfQ;
        float pushE = armsCrossed ? 260 : 320;
        float liftE = armsCrossed ? 90 : 120;
        VR_Push(attacker, to, pushE * cvar_hget(cvarh_vr_parry_push_enemy), liftE * cvar_hget(cvarh_vr_parry_push_enemy));
        if(!attacker.vr_eshove_striking) // (a shove pushes you its own way: VR_EnemyShove_Hit)
        {
            float pushP = armsCrossed ? 200 : 180;
            VR_Push(targ, to * -1, pushP * cvar_hget(cvarh_vr_parry_push_player), 40 * cvar_hget(cvarh_vr_parry_push_player));
        }
        VR_Motion_Event("parry", armsCrossed ? "arms" : VR_Motion_HandSub(hand), armsCrossed ? -1 : hand, damage,
            armsCrossed ? VRGetEntHandPos(targ, cVR_MainHand) : vr_parry_at, attacker, "cooldown");
        VR_Parry_Interrupt(attacker, targ);
        v_forward = oldForward;
        v_right = oldRight;
        v_up = oldUp;
        float cut = armsCrossed ? cvar_hget(cvarh_vr_parry_unarmed_reduction) : cvar_hget(cvarh_vr_parry_reduction);
        return damage * (1 - min(1, max(0, cut)));
    }
    if(armsCrossed)
    {
        entity oldSelfX = self;
        self = targ;
        VR_SoundVaried(self, CHAN_AUTO, "fisthit.wav", 1, ATTN_NORM);
        sound(self, CHAN_AUTO, "demon/dland2.wav", 0.5, ATTN_NORM);
        haptic(cVR_MainHand, 0.0, 0.3, 60, 1.0);
        haptic(cVR_OffHand, 0.0, 0.3, 60, 1.0);
        handimpact(cVR_MainHand, 4, to * -1);
        handimpact(cVR_OffHand, 4, to * -1);
        particle2(VRGetHandPos(cVR_MainHand) + to * 4, (to * -1 + '0 0 0.5') * 60, QVR_PARTICLE_PRESET_BULLETPUFF, 12);
        self = oldSelfX;

        VR_Push(attacker, to, 260 * cvar_hget(cvarh_vr_parry_push_enemy), 90 * cvar_hget(cvarh_vr_parry_push_enemy));
        if(!attacker.vr_eshove_striking) // (a shove pushes you its own way: VR_EnemyShove_Hit)
        {
            VR_Push(targ, to * -1, 200 * cvar_hget(cvarh_vr_parry_push_player), 40 * cvar_hget(cvarh_vr_parry_push_player));
        }
        dprint("parry: "); dprint(attacker.classname); dprint(" with crossed arms\n");
        VR_Motion_Event("parry", "arms", -1, damage, VRGetHandPos(cVR_MainHand), attacker, "");
        vr_parried = 2;
        VR_Highlight_Parry(targ, attacker, -1);
        VR_Counter_Open(targ, -1);
        damage = damage * (1 - min(1, max(0, cvar_hget(cvarh_vr_parry_unarmed_reduction))));
    }

    if(hand >= 0)
    {
        entity oldSelf = self;
        self = targ;

        vector at = vr_parry_at + to * 4;
        // Its own ring (vr_bash_sound; round 18), apart from a blow landing; 0: the old clang.
        float parrySound = cvar_hget(cvarh_vr_bash_sound);
        if(parrySound > 0)
        {
            sound(self, VRGetHandChannel(hand), "vr/parry.wav", min(1, parrySound), ATTN_NORM);
            VR_SoundVaried(self, VRGetHandChannel(hand), "player/axhit2.wav", 0.35 * min(1, parrySound), ATTN_NORM);
        }
        else
        {
            VR_SoundVaried(self, VRGetHandChannel(hand), "player/axhit2.wav", 1, ATTN_NORM);
            VR_SoundVaried(self, VRGetHandChannel(hand), "weapons/tink1.wav", 0.6, ATTN_NORM);
        }
        particle2(at, (to * -1 + '0 0 0.5') * 80, QVR_PARTICLE_PRESET_SPARKS, 24);
        haptic(hand, 0.0, 0.3, 90, 1.0);
        handimpact(hand, 5, to * -1);

        float twoHanded = VRGet2HAiming(self);
        if(twoHanded)
        {
            float other = VRGetOtherHand(hand);
            haptic(other, 0.0, 0.25, 90, 0.8);
            handimpact(other, 3, to * -1);
        }

        self = oldSelf;

        // Both are pushed apart (vr_parry_push_enemy, vr_parry_push_player).
        VR_Push(attacker, to, 320 * cvar_hget(cvarh_vr_parry_push_enemy), 120 * cvar_hget(cvarh_vr_parry_push_enemy));
        if(!attacker.vr_eshove_striking) // (a shove pushes you its own way: VR_EnemyShove_Hit)
        {
            VR_Push(targ, to * -1, 180 * cvar_hget(cvarh_vr_parry_push_player), 40 * cvar_hget(cvarh_vr_parry_push_player));
        }

        dprint("parry: "); dprint(attacker.classname); dprint(" with hand "); dprint(ftos(hand));
        dprint(twoHanded ? " (two hands)\n" : "\n");
        VR_Motion_Event("parry", VR_Motion_HandSub(hand), hand, damage, vr_parry_at, attacker, twoHanded ? "two hands" : "");
        vr_parried = 1;

        // What the parry costs: with parry stamina on, the parry that empties it knocks the weapon away (one hand or
        // two); else, held in one hand, the drop chance may.
        float knocked = FALSE;
        if(cvar_hget(cvarh_vr_parry_stamina))
        {
            knocked = VR_Stamina_Parry(targ, hand, twoHanded);
        }
        else if(!twoHanded)
        {
            knocked = random() < cvar_hget(cvarh_vr_parry_drop_chance);
        }
        if(knocked)
        {
            self = targ;
            DropWeaponInHand(self, hand, VRGetEntWeapon(self, hand), VRGetEntWeaponFlags(self, hand));
            sprint(self, cvar_hget(cvarh_vr_parry_stamina) ? "Exhausted: the blow knocks the weapon out of your hand!\n" :
                "The blow knocks the weapon out of your hand!\n");
            self = oldSelf;
        }
        VR_Highlight_Parry(targ, attacker, hand);
        VR_Counter_Open(targ, hand);
        damage = damage * (1 - min(1, max(0, cvar_hget(cvarh_vr_parry_reduction))));
    }

    if(vr_parried)
    {
        VR_Parry_Interrupt(attacker, targ);
    }
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return damage;
};

float(entity targ, entity inflictor, entity attacker, float damage) T_Damage =
{
    float parried = VR_Parry(targ, inflictor, attacker, damage);
    float corpse = targ.vr_corpse == 2 || targ.vr_corpse_dying;
    float qpop = VR_Decap_QuadArm(targ, inflictor, attacker, parried); // VR: with Quad, any headshot kill pops (vr_decap.qc)
    float r = T_DamageImpl(targ, inflictor, attacker, parried, 0);
    if(qpop)
    {
        VR_Decap_After(targ);
    }
    if(cvar_hget(cvarh_vr_debug_shots) && (attacker.flags & FL_CLIENT) && targ != attacker)
    {
        dprint(sprintf("damage: %s %g by %s (part %g at %v), health %g\n", targ.classname, parried, inflictor.classname,
            vr_hitpart_time == time ? vr_hitpart : -1, vr_hitpart_pos, targ.health));
    }

    // VR: hit reactions: a heavy hit shoves a monster (vr_hit_push), a killing blow throws the body
    // (vr_kill_push; an explosion's away from it). Not a corpse's (vr_corpse_gib): it lies still.
    if(r && !corpse && (targ.flags & FL_MONSTER) && (attacker.flags & FL_CLIENT))
    {
        vector from = inflictor == attacker ? attacker.origin : (inflictor.absmin + inflictor.absmax) * 0.5;
        float hitPush = cvar_hget(cvarh_vr_hit_push);
        if(targ.health <= 0)
        {
            float killPush = cvar_hget(cvarh_vr_kill_push);
            VR_Push(targ, targ.origin - from, min(700, damage * 14) * killPush, min(260, damage * 5) * killPush);
        }
        else if(damage >= 15)
        {
            VR_Push(targ, targ.origin - from, min(350, damage * 6) * hitPush, min(80, damage * 1.5) * hitPush);
        }
    }

    // VR: a monster's melee blow that was not parried throws the player back (vr_melee_push_player;
    // Quake's own push is weak).
    if(r && parried == damage && inflictor == attacker && (attacker.flags & FL_MONSTER) &&
       targ.classname == "player" && VR_Parry_MeleeMonster(attacker) && !attacker.vr_eshove_striking && vlen(attacker.origin - targ.origin) < 150)
    {
        VR_Push(targ, targ.origin - attacker.origin, damage * 12 * cvar_hget(cvarh_vr_melee_push_player),
            damage * 3 * cvar_hget(cvarh_vr_melee_push_player));
    }
    return r;
};

// Quad Damage's sound (items/damage3.wav, at most once a second) plays when you deal damage, not when you press a
// trigger (ROUND21.md, "Debug menu; quad sound; grenade catch default; no empty-hand deflection"): a gun's shot
// (SuperDamageSound in the guns' firing, as in id's game), a blow landing on something that takes it
// (T_Damage_VRMelee: a weapon's, a fist's, a club's, the headbutt), a bash or shove that hurts, the grappling hook
// biting. The trigger alone (a melee weapon, the fist, something held, the hook fired) is silent. vr_debug_shots
// prints each, with what played it.
void(entity xPlayer, string xWhy) VR_QuadSound =
{
    if(xPlayer.super_damage_finished <= time || xPlayer.super_sound >= time)
    {
        return;
    }

    xPlayer.super_sound = time + 1;
    sound(xPlayer, CHAN_BODY, "items/damage3.wav", 1, ATTN_NORM);
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("quad sound: %s\n", xWhy));
    }
};

float(entity targ, entity inflictor, entity attacker, float damage) T_Damage_VRMelee =
{
    // Positional damage for a blow (vr_melee_positional): the zone of the point on the model it met, the same the guns
    // use. Off: 1, and nothing is recorded - exactly the flat melee damage of before.
    float mult = VR_Melee_Positional(targ, vr_hitpos, vr_hitdir);
    damage = damage * mult;
    float decap = VR_Decap_Blow(targ, attacker, damage); // VR: a beheading cut (vr_decap.qc): Killed cuts the head off
    float r = T_DamageImpl(targ, inflictor, attacker, damage, 1);
    if(decap)
    {
        VR_Decap_After(targ);
    }
    if(r && damage > 0 && targ != attacker && (attacker.flags & FL_CLIENT))
    {
        VR_QuadSound(attacker, "a blow landed");
    }
    if(cvar_hget(cvarh_vr_debug_shots) && (attacker.flags & FL_CLIENT) && targ != attacker)
    {
        dprint(sprintf("damage: %s %g by a melee blow (part %g x%.2f at %v), health %g\n", targ.classname, damage,
            vr_hitpart_time == time ? vr_hitpart : -1, mult, vr_hitpos, targ.health));
    }

    // VR: melee blows push what they hit, dead or alive (vr_melee_push).
    if(targ != attacker && targ.takedamage)
    {
        VR_Push(targ, targ.origin - attacker.origin, min(600, damage * 10) * cvar_hget(cvarh_vr_melee_push),
            min(200, damage * 3) * cvar_hget(cvarh_vr_melee_push));
    }
    return r;
};

/*
============
T_RadiusDamage
============
*/
// VR: an explosion rumbles in the hands of the players near it (vr_explosion_rumble), fading with
// distance.
void(vector xOrigin, float xPower) VR_ExplosionRumble =
{
    if(!cvar_hget(cvarh_vr_explosion_rumble))
    {
        return;
    }
    float reach = 300 + xPower * 3;
    entity oldSelf = self;
    entity p = find(world, classname, "player");
    while(p)
    {
        float d = vlen(p.origin - xOrigin);
        if(p.ishuman && d < reach)
        {
            float a = 1 - d / reach;
            self = p;
            haptic(cVR_MainHand, 0.0, 0.2 + 0.3 * a, 45, a);
            haptic(cVR_OffHand, 0.0, 0.2 + 0.3 * a, 45, a);
        }
        p = find(p, classname, "player");
    }
    self = oldSelf;
};

void(entity inflictor, entity attacker, float damage, entity ignore) T_RadiusDamage =
{
    VR_ExplosionRumble(inflictor.origin, damage);
    physicsblast(inflictor.origin, damage);

    float     points;
    entity    head;
    vector    org;

    IsExplosionDamage = 1;
    vr_limb_blast_at = inflictor.origin; // VR: the limbs near it may pop (vr_limbs.qc, vr_limbs_blast)
    vr_limb_blast_by = attacker;
    vr_limb_blast_time = time;
    float hlKills = vr_hl_kills; // VR: the highlight log: the kills it makes (vr_highlights.qc)
    vr_hl_blast_depth = vr_hl_blast_depth + 1;

    head = findradius(inflictor.origin, damage+40);

    while(head)
    {
        if(head != ignore)
        {
            if(head.takedamage)
            {
                org = head.origin + (head.mins + head.maxs)*0.5;
                points = 0.5*vlen(inflictor.origin - org);
                if(points < 0)
                    points = 0;
                points = damage - points;
                if(head == attacker)
                    points = points * 0.5;
                if(points > 0)
                {
                    if(CanDamage(head, inflictor))
                    {    // shambler takes half damage from all explosions
                        float oldHitKind = vr_hitkind;
                        vr_hitkind = QVR_HITKIND_SPLASH;
                        if(VR_IsShambler(head))
                            T_Damage(head, inflictor, attacker, points*0.5);
                        else
                            T_Damage(head, inflictor, attacker, points);
                        vr_hitkind = oldHitKind;
                    }
                }
            }
        }
        head = head.chain;
    }
    vr_hl_blast_depth = vr_hl_blast_depth - 1;
    VR_Highlight_Blast(inflictor, attacker, vr_hl_kills - hlKills);
};

/*
============
T_BeamDamage
============
*/
void(entity attacker, float damage) T_BeamDamage =
{
    float     points;
    entity    head;

    head = findradius(attacker.origin, damage+40);

    while(head)
    {
        if(head.takedamage)
        {
            points = 0.5*vlen(attacker.origin - head.origin);
            if(points < 0)
                points = 0;
            points = damage - points;
            if(head == attacker)
                points = points * 0.5;
            if(points > 0)
            {
                if(CanDamage(head, attacker))
                {
                    float oldHitKind = vr_hitkind;
                    vr_hitkind = QVR_HITKIND_SPLASH;
                    if(VR_IsShambler(head))
                        T_Damage(head, attacker, attacker, points*0.5);
                    else
                        T_Damage(head, attacker, attacker, points);
                    vr_hitkind = oldHitKind;
                }
            }
        }
        head = head.chain;
    }
};
