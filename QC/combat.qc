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

float vr_hitkind;     // QVR_HITKIND_*
float vr_hithand;     // the hand that struck or fired (-1: unknown)
string vr_hitweapon;  // what fired, when not the weapon in vr_hithand ("": that one)
vector vr_hitpos;     // where it struck ('0 0 0': unknown)
vector vr_hitdir;     // and going which way

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
        self.th_die();
        self = oself;
        return;
    }

    self.enemy = attacker;

// bump the monster counter
    if(self.flags & FL_MONSTER)
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
    self.touch = SUB_Null;

    monster_death_use();
    self.th_die();

    // VR: a head thrown by the death code (the monster itself) stays grabbable, whatever the
    // death code did after (a soldier's drop makes it SOLID_NOT).
    if(self.vr_gib)
    {
        self.solid = SOLID_NOT_BUT_TOUCHABLE;
    }

    // VR: a monster that died whole becomes a corpse that can be gibbed (vr_corpse_gib).
    VR_Corpse_Watch(self);

    self = oself;
};

// ----------------------------------------------------------------------------
// VR: gibbing corpses (vr_corpse_gib). A monster that died whole, once its death animation is over
// and it lies still, is a corpse that takes damage: shots (shotguns, nails, lightning, missiles),
// explosions, and blows of a hand or of what it holds (VR_Corpse_StrikeFrame). Enough of it
// (vr_corpse_health, times vr_corpse_toughness for a big monster) gibs it as its own death code
// would have: its head, its gibs, the gib sound, and a mist of blood (VR_BloodMist, vr_carry.qc). Its damage never
// reaches Quake's death code (T_Damage hands it to VR_Corpse_Damage): nothing is counted, killed or
// triggered again. Only the monsters VR_Corpse_Parts knows (the ones whose death code can gib
// them) are gibbed: no bosses (Chthon, Shub-Niggurath, the dragon, Armagon), nothing that always
// gibs (zombies, mummies) or blows up.
//
// The corpse is not solid, and never blocks anything: shots and missiles stop at it as at gibs
// (the engine's MOVE_HITGIBS: touchable non-solids that take damage). Its box is made to lie on the
// floor (VR_Corpse_Arm): low, and wider than the standing monster's (a lying body is longer).

.float vr_corpse;         // 1 dying (watched: VR_Corpse_Think2), 2 a corpse that can be gibbed
.float vr_corpse_hp;      // what damage it takes yet
.float vr_corpse_frame;   // its frame at the last look: the death animation is over when it stops changing
.float vr_corpse_since;   // when it died
.float vr_corpse_model;   // its modelindex when it died (the death code gibbing it changes it)
.float vr_corpse_struck;  // when a hand last struck it (a swing strikes once)

// The monsters' death code (their th_die), to know what gibs they make.
void() army_die;
void() enf_die;
void() dog_die;
void() demon_die;
void() ogre_die;
void() knight_die;
void() hknight_die;
void() shalrath_die;
void() sham_die;
void() wiz_die;
void() fish_die;
void() f_death1;
void() gremlin_die;
void() scourge_die;
void() eel_death;

// What gibbing `e` throws (VR_Corpse_Parts), as its death code's gib branch does.
string vr_corpse_head; // "": none, the body just goes (a fish)
string vr_corpse_gib1;
string vr_corpse_gib2;
string vr_corpse_gib3;
float vr_corpse_toughness; // times vr_corpse_health: about (its health / a knight's 75) to the 0.6, at least 1

float(entity e) VR_Corpse_Parts =
{
    vr_corpse_toughness = 1;
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
    }
    else if(e.th_die == dog_die)
    {
        vr_corpse_head = "progs/h_dog.mdl";
        vr_corpse_gib1 = "progs/gib3.mdl";
        vr_corpse_gib2 = "progs/gib3.mdl";
    }
    else if(e.th_die == demon_die)
    {
        vr_corpse_head = "progs/h_demon.mdl";
        vr_corpse_toughness = 2.25;
        vr_corpse_gib2 = "progs/gib1.mdl";
        vr_corpse_gib3 = "progs/gib1.mdl";
    }
    else if(e.th_die == ogre_die)
    {
        vr_corpse_head = "progs/h_ogre.mdl";
        vr_corpse_toughness = 1.75;
        vr_corpse_gib1 = "progs/gib3.mdl";
        vr_corpse_gib2 = "progs/gib3.mdl";
    }
    else if(e.th_die == knight_die || e.th_die == hknight_die)
    {
        vr_corpse_head = e.th_die == knight_die ? "progs/h_knight.mdl" : "progs/h_hellkn.mdl";
        vr_corpse_toughness = e.th_die == knight_die ? 1 : 2;
        if(e.spawnflags & 2) // a statue
        {
            vr_corpse_gib1 = "progs/statgib1.mdl";
            vr_corpse_gib2 = "progs/statgib2.mdl";
            vr_corpse_gib3 = "progs/statgib3.mdl";
        }
    }
    else if(e.th_die == shalrath_die)
    {
        vr_corpse_head = "progs/h_shal.mdl";
        vr_corpse_toughness = 2.75;
    }
    else if(e.th_die == sham_die)
    {
        vr_corpse_head = "progs/h_shams.mdl";
        vr_corpse_toughness = 3.5;
    }
    else if(e.th_die == wiz_die)
    {
        vr_corpse_head = "progs/h_wizard.mdl";
        vr_corpse_gib1 = "progs/gib2.mdl";
        vr_corpse_gib3 = "progs/gib2.mdl";
    }
    else if(e.th_die == fish_die || e.th_die == f_death1)
    {
        vr_corpse_head = "";
    }
    else if(e.th_die == gremlin_die)
    {
        vr_corpse_head = "progs/h_grem.mdl";
        vr_corpse_toughness = 1.25;
        vr_corpse_gib2 = "progs/gib1.mdl";
        vr_corpse_gib3 = "progs/gib1.mdl";
    }
    else if(e.th_die == scourge_die)
    {
        vr_corpse_head = "progs/h_scourg.mdl";
        vr_corpse_toughness = 2.25;
    }
    else if(e.th_die == eel_death)
    {
        vr_corpse_head = "progs/eelgib.mdl";
        vr_corpse_gib2 = "progs/gib1.mdl";
        vr_corpse_gib3 = "progs/gib1.mdl";
    }
    else
    {
        return FALSE;
    }
    return TRUE;
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
    e.vr_corpse = 0;
    if(e.think2 == VR_Corpse_Think2)
    {
        e.think2 = SUB_Null;
        e.nextthink2 = 0;
    }
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

    float still = self.frame == self.vr_corpse_frame;
    self.vr_corpse_frame = self.frame;
    float resting = (self.flags & FL_ONGROUND) || self.velocity == '0 0 0' || time > self.vr_corpse_since + 3;
    if(self.solid == SOLID_NOT && still && resting)
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

    // Bigger monsters take more (VR_Corpse_Parts: an ogre 1.75 times, a shambler 3.5 times).
    VR_Corpse_Parts(e);

    e.vr_corpse = 2;
    e.vr_corpse_hp = max(1, cvar_hget(cvarh_vr_corpse_health) * vr_corpse_toughness);
    if(e.health >= 0)
    {
        e.health = -1; // dead meat: a rocket's direct hit looks for some health (T_MissileTouch)
    }
    e.solid = SOLID_NOT_BUT_TOUCHABLE;
    e.takedamage = DAMAGE_YES; // not DAMAGE_AIM: no autoaim at it
    setsize(e, lo, hi);

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

// Its damage, from T_Damage (instead of Quake's): gibbed at no health.
float(entity targ, entity inflictor, entity attacker, float damage) VR_Corpse_Damage =
{
    if(!cvar_hget(cvarh_vr_corpse_gib))
    {
        targ.vr_corpse = 0;
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

    targ.vr_corpse_hp = targ.vr_corpse_hp - damage;
    dprint("corpse: "); dprint(targ.classname); dprint(" hit by "); dprint(inflictor.classname); dprint(" for ");
    dprint(ftos(damage)); dprint(", "); dprint(ftos(max(0, targ.vr_corpse_hp))); dprint(" left\n");
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
    VR_Corpse_Parts(c);

    vector dir = '0 0 0';
    if(inflictor != world && inflictor != c)
    {
        dir = normalize((c.absmin + c.absmax) * 0.5 - (inflictor.absmin + inflictor.absmax) * 0.5);
    }
    dprint("corpse: "); dprint(c.classname); dprint(" gibbed (kills: "); dprint(ftos(killed_monsters)); dprint(")\n");

    entity oldSelf = self;
    self = c;

    self.vr_corpse = 0;
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

    if(vr_corpse_head != "")
    {
        ThrowHead(vr_corpse_head, dm);
        if(self.vr_gib)
        {
            self.solid = SOLID_NOT_BUT_TOUCHABLE; // grabbable (VR_MakeGibGrabbable)
        }
    }
    else
    {
        self.solid = SOLID_NOT;
        self.model = string_null;
        self.modelindex = 0;
        setorigin(self, self.origin);
        self.think = SUB_Remove;
        self.nextthink = time + 0.1;
    }

    self = oldSelf;
};

// Every player frame, for each hand (self is the player; VR_Carry_HandFrame): a hand swung at a
// blow's speed (vr_melee_speed) through a corpse, or what it holds, strikes it.
void(float xHand) VR_Corpse_StrikeFrame =
{
    if(!cvar_hget(cvarh_vr_corpse_gib) || !self.ishuman || self.health <= 0)
    {
        return;
    }

    vector vel = VRGetHandVel(xHand);
    float speed = vlen(vel);
    if(speed < max(0.1, cvar_hget(cvarh_vr_melee_speed)))
    {
        return;
    }

    vector dir = normalize(vel);
    vector hand = VRGetHandPos(xHand);
    entity c = world;
    vector at = hand;

    // The hand, along its swing.
    traceline(hand - dir * 6, hand + dir * 8, MOVE_HITGIBS, self);
    if(trace_ent.vr_corpse == 2)
    {
        c = trace_ent;
        at = trace_endpos;
    }
    // What it holds, from the hand to its end.
    else if(!VRIsHandEmpty(self, xHand))
    {
        traceline(hand, VRGetMuzzlePos(xHand) + dir * 4, MOVE_HITGIBS, self);
        if(trace_ent.vr_corpse == 2)
        {
            c = trace_ent;
            at = trace_endpos;
        }
    }

    if(c == world || !c.takedamage || time < c.vr_corpse_struck + 0.35)
    {
        return;
    }
    c.vr_corpse_struck = time;

    float dmg = VR_Gib_StrikeDamage(speed);
    if(!VRIsHandEmpty(self, xHand))
    {
        dmg = dmg * 1.5;
    }
    sound(self, CHAN_AUTO, VRIsHandEmpty(self, xHand) ? "fisthit.wav" : "player/axhit2.wav", 0.8, ATTN_NORM);
    haptic(xHand, 0.0, 0.1, 60, 0.7);
    SpawnBlood(at, dir * 40, dmg);

    VR_SetHitKind(QVR_HITKIND_MELEE, xHand, at, dir);
    T_Damage(c, self, self, dmg);
    VR_ClearHitKind();
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
float VR_SHOVE_SAFEFALL = 150; // units of drop without damage; 0.4 a unit beyond

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
    float contents = pointcontents(m.origin);
    float dmg = 0;
    if(contents == CONTENT_LAVA)
    {
        dmg = 100;
    }
    else if(contents == CONTENT_SLIME)
    {
        dmg = 30;
    }
    else if(contents == CONTENT_EMPTY && xDrop > VR_SHOVE_SAFEFALL)
    {
        dmg = (xDrop - VR_SHOVE_SAFEFALL) * 0.4;
    }
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
        if(e.health > 0 && speed >= VR_SHOVE_MINSPEED && e.movetype == MOVETYPE_STEP)
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
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) T_DamageImpl =
{
    vector    dir;
    entity    oldself;
    float    save;
    float    take;

    if(!targ.takedamage)
    {
        return FALSE;
    }

    if(targ.classname == "trigger_explosion")
    {
        if(IsExplosionDamage != 1)
        {
            return FALSE;
        }
    }

    IsExplosionDamage = 0;

    // VR: a gib or head only ever bursts (vr_gib_destroy, vr_carry.qc), never dies again.
    if(targ.th_die == VR_Gib_Die)
    {
        return VR_Gib_Damage(targ, inflictor, attacker, damage);
    }

    // VR: a corpse is only ever gibbed (vr_corpse_gib), never killed again.
    if(targ.vr_corpse == 2)
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

        if(targ.ishuman)
        {
            // VR: felt more in the hand on the side the blow came from, and more the harder it is.
            float amp = min(1, 0.35 + take / 40);
            float dur = min(0.5, 0.12 + take / 60);
            float side = 0;
            if(inflictor != world && inflictor != targ)
            {
                vector oldForward = v_forward;
                vector oldRight = v_right;
                vector oldUp = v_up;
                makevectors(targ.v_angle);
                side = normalize((inflictor.absmin + inflictor.absmax) * 0.5 - targ.origin) * v_right; // 1: right
                v_forward = oldForward;
                v_right = oldRight;
                v_up = oldUp;
            }
            entity oldHapticSelf = self;
            self = targ;
            haptic(cVR_MainHand, 0.0, dur, 30, amp * (0.4 + 0.3 * (1 + side)));
            haptic(cVR_OffHand, 0.0, dur, 30, amp * (0.4 + 0.3 * (1 - side)));
            self = oldHapticSelf;
        }
    }

    // rune power up (Earth Magic)
    if(deathmatch)
    {
        take = RuneApplyEarth(take, targ);
    }

    // VR: the firing range's training dummy reports the hit and takes no harm (vr_dummy.qc).
    if(targ.classname == "vr_dummy")
    {
        VR_Dummy_Hit(targ, inflictor, attacker, damage, isVRMelee);
        return TRUE;
    }

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

        Killed (targ, attacker);
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

    self = oldself;
    return TRUE;
};

// ----------------------------------------------------------------------------
// Parrying (vr_parry): a monster's melee blow on a VR player who holds a weapon (a sword, the axe,
// any gun; one hand or two) about level across in front of them, between them and the monster
// (VR_Parry_Blocks), is partly blocked (vr_parry_reduction): a clang, sparks,
// the hand knocked back. Held in one hand, the weapon may be knocked out of it
// (vr_parry_drop_chance); held in two, never.

// Monsters that strike in melee (a blow comes from the monster itself, up close).
float(entity e) VR_Parry_MeleeMonster =
{
    string c = e.classname;
    return c == "monster_knight" || c == "monster_hell_knight" || c == "monster_ogre" || c == "monster_demon1" ||
           c == "monster_dog" || c == "monster_shambler" || c == "monster_fish" || c == "monster_gremlin" ||
           c == "monster_mummy";
};

// Whether `p`'s `hand` holds a weapon with the hand's forward across `toAttacker` (horizontal, unit):
// square to it within 50 degrees, the hand ahead of the body, between the hips and above the head.
// The guard's test (VR_Parry_Guard, for the bash). It was the parry's too, until round 16: a sword's
// blade or the axe's handle is about 70 degrees off the hand's forward (the aim), so a sword or axe
// held level across had its hand's forward pointing at the attacker, and never parried.
float(entity p, float hand, vector toAttacker) VR_Parry_HandAcross =
{
    if(VRIsHandEmpty(p, hand))
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

// The weapon in `p`'s `hand`, into vr_parry_butt .. vr_parry_end (FALSE: an empty hand).
float(entity p, float hand) VR_Parry_WeaponLine =
{
    if(VRIsHandEmpty(p, hand))
    {
        return FALSE;
    }
    vector h = VRGetEntHandPos(p, hand);
    vector m = hand == cVR_OffHand ? p.offmuzzlepos : p.muzzlepos;
    if(m == '0 0 0' || vlen(m - h) > 80 || vlen(m - h) < 4)
    {
        m = h + VRForwardOf(VRGetEntHandRot(p, hand)) * 16;
    }
    vr_parry_butt = h - (m - h) * 0.3;
    vr_parry_end = m;
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
    if(VR_Parry_LineBlocks(p, vr_parry_butt, vr_parry_end, toAttacker))
    {
        return TRUE;
    }
    if(hand == cVR_MainHand && VRGet2HAiming(p))
    {
        vector h = VRGetEntHandPos(p, cVR_MainHand);
        vector o = VRGetEntHandPos(p, cVR_OffHand);
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
    if(!cvar_hget(cvarh_vr_parry_unarmed) || !VRIsHandEmpty(p, cVR_MainHand) || !VRIsHandEmpty(p, cVR_OffHand))
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
    // Crossed: the main hand on the off hand's side (the left for the right-handed).
    vector right;
    right_x = toAttacker_y;
    right_y = -toAttacker_x;
    right_z = 0;
    float mainRight = cvar("vr_lefthanded") ? -1 : 1;
    return (m * right - o * right) * mainRight < -1;
};

// Guards (a bash is a guard pushed forward: VR_Bash in vr_juice.qc): the parry's poses, held at chest
// or face height.

// Whether `xAt` is at a guard's height for `p`: from 65 cm below the eyes (the chest) to 15 cm above.
float(entity p, vector xAt) VR_Parry_GuardHeight =
{
    vector head = VRGetHeadPos(p);
    float dz = (xAt_z - head_z) / VRMetersToUnits();
    return dz > -0.65 && dz < 0.15;
};

// Whether `p`'s `hand` holds a weapon in a guard, facing `xForward` (horizontal, unit): the hand's
// forward across in front (VR_Parry_HandAcross), about level (within 30 degrees: a weapon raised or
// pointing down, as in a swing, is no guard), at a guard's height.
float(entity p, float hand, vector xForward) VR_Parry_Guard =
{
    if(!VR_Parry_HandAcross(p, hand, xForward))
    {
        return FALSE;
    }
    makeforward(VRGetEntHandRot(p, hand));
    return fabs(v_forward_z) < 0.5 && VR_Parry_GuardHeight(p, VRGetEntHandPos(p, hand));
};

// Whether `p` holds both empty hands together in front, facing `xForward`: crossed or side by side,
// less than 45 cm apart, ahead of the body, at a guard's height.
float(entity p, vector xForward) VR_Parry_HandsTogether =
{
    if(!VRIsHandEmpty(p, cVR_MainHand) || !VRIsHandEmpty(p, cVR_OffHand))
    {
        return FALSE;
    }
    vector m = VRGetEntHandPos(p, cVR_MainHand);
    vector o = VRGetEntHandPos(p, cVR_OffHand);
    return vlen(m - o) < 0.45 * VRMetersToUnits() && (m - p.origin) * xForward > 2 && (o - p.origin) * xForward > 2 &&
           VR_Parry_GuardHeight(p, m) && VR_Parry_GuardHeight(p, o);
};

float(entity targ, entity inflictor, entity attacker, float damage) VR_Parry =
{
    if(inflictor != attacker || !(attacker.flags & FL_MONSTER) || damage <= 0 || !cvar_hget(cvarh_vr_parry) ||
       targ.classname != "player" || !VRHandsTracked(targ) || !VR_Parry_MeleeMonster(attacker))
    {
        return damage;
    }

    vector to = attacker.origin - targ.origin;
    to_z = 0;
    // Up close only (a shambler's lightning also comes from the shambler itself).
    if(vlen(to) > 150 || (attacker.classname == "monster_shambler" && vlen(to) > 110))
    {
        return damage;
    }
    to = normalize(to);

    // The globals the monster's attack code may still use after the damage.
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;

    float hand = -1;
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

    // Unarmed: arms crossed in front take the blow on the forearms (both hands knocked, less taken
    // off than with a weapon).
    if(hand < 0 && VR_Parry_ArmsCrossed(targ, to))
    {
        entity oldSelfX = self;
        self = targ;
        sound(self, CHAN_AUTO, "fisthit.wav", 1, ATTN_NORM);
        sound(self, CHAN_AUTO, "demon/dland2.wav", 0.5, ATTN_NORM);
        haptic(cVR_MainHand, 0.0, 0.3, 60, 1.0);
        haptic(cVR_OffHand, 0.0, 0.3, 60, 1.0);
        handimpact(cVR_MainHand, 4, to * -1);
        handimpact(cVR_OffHand, 4, to * -1);
        particle2(VRGetHandPos(cVR_MainHand) + to * 4, (to * -1 + '0 0 0.5') * 60, QVR_PARTICLE_PRESET_BULLETPUFF, 12);
        self = oldSelfX;

        VR_Push(attacker, to, 260 * cvar_hget(cvarh_vr_parry_push_enemy), 90 * cvar_hget(cvarh_vr_parry_push_enemy));
        VR_Push(targ, to * -1, 200 * cvar_hget(cvarh_vr_parry_push_player), 40 * cvar_hget(cvarh_vr_parry_push_player));
        dprint("parry: "); dprint(attacker.classname); dprint(" with crossed arms\n");
        damage = damage * (1 - min(1, max(0, cvar_hget(cvarh_vr_parry_unarmed_reduction))));
    }

    if(hand >= 0)
    {
        entity oldSelf = self;
        self = targ;

        vector at = vr_parry_at + to * 4;
        sound(self, CHAN_AUTO, "player/axhit2.wav", 1, ATTN_NORM);
        sound(self, CHAN_AUTO, "weapons/tink1.wav", 0.6, ATTN_NORM);
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
        else if(random() < cvar_hget(cvarh_vr_parry_drop_chance))
        {
            DropWeaponInHand(self, hand, VRGetEntWeapon(self, hand), VRGetEntWeaponFlags(self, hand),
                VRGetEntWeaponClip(self, hand));
            sprint(self, "The blow knocks the weapon out of your hand!\n");
        }

        self = oldSelf;

        // Both are pushed apart (vr_parry_push_enemy, vr_parry_push_player).
        VR_Push(attacker, to, 320 * cvar_hget(cvarh_vr_parry_push_enemy), 120 * cvar_hget(cvarh_vr_parry_push_enemy));
        VR_Push(targ, to * -1, 180 * cvar_hget(cvarh_vr_parry_push_player), 40 * cvar_hget(cvarh_vr_parry_push_player));

        dprint("parry: "); dprint(attacker.classname); dprint(" with hand "); dprint(ftos(hand));
        dprint(twoHanded ? " (two hands)\n" : "\n");
        damage = damage * (1 - min(1, max(0, cvar_hget(cvarh_vr_parry_reduction))));
    }

    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return damage;
};

float(entity targ, entity inflictor, entity attacker, float damage) T_Damage =
{
    float parried = VR_Parry(targ, inflictor, attacker, damage);
    float corpse = targ.vr_corpse == 2;
    float r = T_DamageImpl(targ, inflictor, attacker, parried, 0);

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
       targ.classname == "player" && VR_Parry_MeleeMonster(attacker) && vlen(attacker.origin - targ.origin) < 150)
    {
        VR_Push(targ, targ.origin - attacker.origin, damage * 12 * cvar_hget(cvarh_vr_melee_push_player),
            damage * 3 * cvar_hget(cvarh_vr_melee_push_player));
    }
    return r;
};

float(entity targ, entity inflictor, entity attacker, float damage) T_Damage_VRMelee =
{
    float r = T_DamageImpl(targ, inflictor, attacker, damage, 1);

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

    float     points;
    entity    head;
    vector    org;

    IsExplosionDamage = 1;

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
                        if(head.classname == "monster_shambler")
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
                    if(head.classname == "monster_shambler")
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
