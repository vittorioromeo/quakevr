// vr_decap.qc -- decapitation (docs/vr-port/ROUND21.md, "Decapitation"; the engine's vr_box3d.cpp, "Decapitation").
//
// A killing blow at a live monster's head (positional damage's head zone: PositionalHead) by a bladed slash cuts its
// head off: a sword's or an axe's blade (not its hilt, handle or pommel) swung across its line (vr_decap_slash_angle: a blade
// driven along its line is a stab) at vr_decap_speed or more, an axe thrown into it edge first (vr_axestick.cpp decides
// the edge), or the chainsaw (its running bar at the neck, or a swing of its bar). The monster skips its death animation:
// its ragdoll is made at once from the frame it is in (the engine: ragdolldecap), keeping its motion and the blow's
// knock, without its head; the head flies off as its own gib (VR_Decap_ThrowHead: from where the head was, turned as it
// was, launched by the blade and spun by it); the stump bleeds (a wound round the neck, a squish, small gibs, a burst
// and a fountain of blood: VR_Decap_FountainThink). A headless corpse is a ragdoll as any other; gibbed, it throws no
// head (.vr_headless: VR_Corpse_Gib; kept in a saved game, the engine makes its ragdoll headless again).
// A slash at a ragdoll's head (or a dying body's) beheads it the same way instead of taking its corpse health
// (vr_decap_corpses; VR_Decap_CorpseBlow, from VR_Corpse_StrikeFrame). A zombie's head cut off kills it for good,
// whatever the damage (vr_decap_zombies; its ragdoll: vr_ragdoll.cpp zombieSeeds).
// Only monsters with a ragdoll rig (vr_ragdoll.cpp seedTables: their bone named "head") and a head gib here
// (VR_Decap_HeadModel) are beheaded; the rest die as before. Ragdolls off: nothing.

float VR_DECAP_BLADE_SWORD = 0.4;  // the blade proper: from this share of the way from the grip to the tip (VR_MELEE_BLADE_FROM)
float VR_DECAP_BLADE_AXE = 0.6;    // an axe's head (25..37 units up its 40)
float VR_DECAP_DIED_LATE = 0.3;    // s: a thrown axe's blade going in this soon after its hit box killed the monster still beheads it
float VR_DECAP_SPURT_TICK = 0.05;  // s between the fountain's spurts
float VR_DECAP_BEAT = 2.2;         // Hz: its pulses

float QVR_DECAP_SWING = 1;  // a sword or an axe swung
float QVR_DECAP_THROWN = 2; // an axe thrown
float QVR_DECAP_SAW = 3;    // the chainsaw
float QVR_DECAP_BLUNT = 7;  // a fist, gun or other blunt melee head hit
float QVR_DECAP_THROWN_BLUNT = 8; // a thrown (or flung) blunt weapon or prop (Head pop chance: heavy enough, always)
float QVR_DECAP_BLAST = 10; // an explosion popping a limb near it (vr_limbs.qc: vr_limbs_blast)
float QVR_DECAP_QUAD = 9;   // any other headshot kill with Quad Damage (vr_decap_pop_quad: a nail, a rocket, a grenade, a stab)
float QVR_DECAP_LASER = 11; // an enforcer's laser bolt (an enemy's, or your enforcer's rifle): vr_decap_pop_laser (vr_limbs.qc: VR_Decap_LaserArm)
// Head pops (below: "Head pops"; combat.qc: QVR_DECAP_SHOTGUN 4, _SSG 5, _LIGHTNING 6): a shot kills with a headshot.

.float vr_decap_died; // when it was killed (Killed: VR_Decap_Killed)

// The blow that would behead (VR_Decap_Arm), while its damage is dealt: Killed (VR_Decap_Killed) cuts the head off if
// it kills; VR_Decap_After ends it (a zombie: beheaded whatever).
entity vr_decap_ent;
entity vr_decap_by;
float vr_decap_time;
float vr_decap_chance; // the armed blow's chance to behead or pop (Head pop chance; 1: sure): rolled when it kills (VR_Decap_Roll)
vector vr_decap_vel; // the blade's velocity at the cut (world, units/s)
vector vr_decap_at;
float vr_decap_how;
// vr_decap_limb (combat.qc): the armed blow's limb (vr_limbs.qc): -1 the head, else the rig's joint cut
vector vr_decap_slashvel; // VR_Decap_Slash's answer: the struck point's velocity (world, units/s)
float vr_decap_count;     // heads cut off (the tests)
entity vr_decap_lasthead; // the last head thrown (the tests)
float vr_decap_pops;      // heads popped (the tests)
float VR_DECAP_POP_SIZE = 1.5; // a popped head's burst: a head gib's (VR_Decap_ThrowHead: VR_Gib_MakeDestroyable 1.5)

void() VR_Decap_ZombieDie;
float(entity e, entity by, vector xVel, float xHow) VR_Decap_Cut;
// Limb gore (vr_limbs.qc, after this file).
.float vr_limb_bone;
float(entity e, entity by, vector xVel, float xHow, float xBone, float xLive) VR_Limb_Cut;
void(entity e) VR_Limb_Killed;
void(entity h, entity c, float xBone) VR_Limb_SetMass;
void(entity h, entity c, float xBone, float xHead) VR_Limb_EndFountain;
void(entity e) VR_Limb_BlastKilled;
float(entity e) VR_Limb_IsZombie;

string(float xHow) VR_Decap_HowName =
{
    if(xHow == QVR_DECAP_THROWN) { return "a thrown axe"; }
    if(xHow == QVR_DECAP_SAW) { return "the chainsaw"; }
    if(xHow == QVR_DECAP_SHOTGUN) { return "a shotgun blast"; }
    if(xHow == QVR_DECAP_SSG) { return "a super shotgun blast"; }
    if(xHow == QVR_DECAP_LIGHTNING) { return "a lightning bolt"; }
    if(xHow == QVR_DECAP_BLUNT) { return "a blunt head blow"; }
    if(xHow == QVR_DECAP_THROWN_BLUNT) { return "a thrown blunt thing"; }
    if(xHow == QVR_DECAP_QUAD) { return "a Quad Damage headshot"; }
    if(xHow == QVR_DECAP_BLAST) { return "an explosion"; }
    if(xHow == QVR_DECAP_LASER) { return "a laser bolt"; }
    return "a slash";
};

// The head gib `e` throws ("": none, it is never beheaded). A new monster with a ragdoll rig: its head here.
string(entity e) VR_Decap_HeadModel =
{
    string c = e.classname;
    if((c == "monster_knight" || c == "monster_hell_knight") && (e.spawnflags & 2))
    {
        return ""; // (a statue: stone, VR_Corpse_Parts)
    }
    if(c == "monster_army")          { return "progs/h_guard.mdl"; }
    if(c == "monster_knight")        { return "progs/h_knight.mdl"; }
    if(c == "monster_ogre" || c == "monster_ogre_marksman") { return "progs/h_ogre.mdl"; }
    if(c == "monster_enforcer")      { return "progs/h_mega.mdl"; }
    if(c == "monster_hell_knight")   { return "progs/h_hellkn.mdl"; }
    if(c == "monster_ranged_knight") { return "progs/h_hellkn.mdl"; } // (Dawn of the Machine's: its gib head, upstream's)
    if(c == "monster_dog")           { return "progs/h_dog.mdl"; }
    if(c == "monster_wizard")        { return "progs/h_wizard.mdl"; }
    if(c == "monster_zombie")        { return "progs/h_zombie.mdl"; }
    if(c == "monster_demon1")        { return "progs/h_demon.mdl"; }
    if(c == "monster_shambler")      { return "progs/h_shams.mdl"; }
    if(c == "monster_super_shambler") { return "progs/h_shams.mdl"; } // (Dawn of the Machine's: the shambler's head gib)
    if(c == "monster_gremlin")       { return "progs/h_grem.mdl"; }
    if(c == "monster_shalrath")      { return "progs/h_shal.mdl"; }
    if(c == "monster_scourge")       { return "progs/h_scourg.mdl"; } // (Hipnotic's centroid)
    if(c == "monster_mummy")         { return "progs/h_zombie.mdl"; } // (Rogue's: mummy_die throws a zombie's head)
    return "";
};

// Whether `e` may lose its head (the option, a monster with a head gib, not headless already; a zombie: vr_decap_zombies).
float(entity e) VR_Decap_Allowed =
{
    if(!cvar_hget(cvarh_vr_decap) || e.vr_headless || e.vr_gib || !(e.flags & FL_MONSTER) || VR_Decap_HeadModel(e) == "")
    {
        return FALSE;
    }
    if(e.classname == "monster_zombie" && !cvar_hget(cvarh_vr_decap_zombies))
    {
        return FALSE;
    }
    return TRUE;
};

// Whether a blow of `xHow` may behead (vr_decap_thrown, vr_decap_chainsaw; the head pops' vr_decap_shotgun,
// vr_decap_super_shotgun, vr_decap_lightning).
float(float xHow) VR_Decap_HowOn =
{
    if(xHow == QVR_DECAP_THROWN) { return cvar_hget(cvarh_vr_decap_thrown); }
    if(xHow == QVR_DECAP_SAW) { return cvar_hget(cvarh_vr_decap_chainsaw); }
    if(xHow == QVR_DECAP_SHOTGUN) { return cvar_hget(cvarh_vr_decap_shotgun); }
    if(xHow == QVR_DECAP_SSG) { return cvar_hget(cvarh_vr_decap_super_shotgun); }
    if(xHow == QVR_DECAP_LIGHTNING) { return cvar_hget(cvarh_vr_decap_lightning); }
    if(xHow == QVR_DECAP_THROWN_BLUNT) { return cvar_hget(cvarh_vr_decap_pop_thrown); }
    if(xHow == QVR_DECAP_LASER) { return cvar_hget(cvarh_vr_decap_pop_laser) > 0; }
    return TRUE;
};

// Whether `xHow` pops the head (a shot's) rather than cutting it off.
float(float xHow) VR_Decap_IsPop =
{
    return xHow == QVR_DECAP_SHOTGUN || xHow == QVR_DECAP_SSG || xHow == QVR_DECAP_LIGHTNING || xHow == QVR_DECAP_BLUNT ||
           xHow == QVR_DECAP_THROWN_BLUNT || xHow == QVR_DECAP_QUAD || xHow == QVR_DECAP_BLAST || xHow == QVR_DECAP_LASER;
};

// `e`'s head zone (xHeadForward, xHeadUp, xHeadRadius): positional damage's (PositionalHead, weapons.qc, the one
// source for headshots, melee head hits, head pops and beheading). FALSE: none.
float(entity e) VR_Decap_HeadZone =
{
    return PositionalHead(e);
};

// Whether a hit at `xAt` going `xDir` on `e` is at its head: its head zone (VR_Decap_HeadZone: on its model standing,
// vr_hit_precise; else the ray past its head sphere), whether positional damage is on or not.
float(entity e, vector xAt, vector xDir) VR_Decap_OnHead =
{
    if(!VR_Decap_HeadZone(e))
    {
        return FALSE;
    }
    float yaw = e.angles_y * (3.14159265 / 180);
    vector facing;
    facing_x = cos(yaw);
    facing_y = sin(yaw);
    facing_z = 0;
    vector head = e.origin + facing * xHeadForward + '0 0 1' * xHeadUp;
    if(hitmodel_target(e))
    {
        return vlen(hitmodel_rest(e, xAt) - head) < xHeadRadius + VR_HEAD_MARGIN;
    }
    vector d = normalize(xDir);
    if(d == '0 0 0')
    {
        return vlen(xAt - head) < xHeadRadius;
    }
    float t = max(0, (head - xAt) * d);
    return vlen(head - (xAt + d * t)) < xHeadRadius;
};

// The melee's decapitation zone of `e` (VR_Decap_MeleeZone): the head zone grown by vr_decap_head_size (its radius,
// VR_HEAD_MARGIN in with precise hits, times it) and drawn down by vr_decap_neck units (a capsule: the sphere's middle
// from the head's down to that much below it, the neck). `xHeadRadius` its radius after; `vr_decap_zone_top` and
// `vr_decap_zone_bottom` its ends (world, on the model standing). FALSE: no head zone.
vector vr_decap_zone_top;
vector vr_decap_zone_bottom;
float(entity e) VR_Decap_MeleeZone =
{
    if(!VR_Decap_HeadZone(e))
    {
        return FALSE;
    }
    float yaw = e.angles_y * (3.14159265 / 180);
    vector facing;
    facing_x = cos(yaw);
    facing_y = sin(yaw);
    facing_z = 0;
    vr_decap_zone_top = e.origin + facing * xHeadForward + '0 0 1' * xHeadUp;
    vr_decap_zone_bottom = vr_decap_zone_top - '0 0 1' * max(0, cvar_hget(cvarh_vr_decap_neck));
    xHeadRadius = (xHeadRadius + (hitmodel_target(e) ? VR_HEAD_MARGIN : 0)) * max(0.1, cvar_hget(cvarh_vr_decap_head_size));
    return TRUE;
};

// Whether a melee blow at `xAt` going `xDir` on `e` is at its head for beheading (a slash, the chainsaw's bar, a
// corpse's head on its model standing): VR_Decap_OnHead's test against the melee's zone (VR_Decap_MeleeZone: grown, and
// down the neck). Head pops and thrown axes keep the head zone itself (VR_Decap_OnHead).
float(entity e, vector xAt, vector xDir) VR_Decap_OnMeleeHead =
{
    if(!VR_Decap_MeleeZone(e))
    {
        return FALSE;
    }
    vector top = vr_decap_zone_top;
    vector bottom = vr_decap_zone_bottom;
    vector p;
    if(hitmodel_target(e))
    {
        p = hitmodel_rest(e, xAt);
        vector q = top;
        q_z = min(top_z, max(bottom_z, p_z));
        return vlen(p - q) < xHeadRadius;
    }
    vector d = normalize(xDir);
    // (The ray's closest approach to spheres along the neck, at most half a radius apart: the capsule near enough.)
    float n = max(1, ceil((top_z - bottom_z) / max(0.5, xHeadRadius * 0.5)));
    for(float i = 0; i <= n; i++)
    {
        vector c = top + (bottom - top) * (i / n);
        float t = 0;
        if(d != '0 0 0')
        {
            t = max(0, (c - xAt) * d);
        }
        if(vlen(c - (xAt + d * t)) < xHeadRadius)
        {
            return TRUE;
        }
    }
    return FALSE;
};

// Whether what hand `xHand` of `self` holds strikes `xAt` as a beheading cut: a sword's or an axe's blade swung across
// its line at vr_decap_speed (its struck point's speed), or the chainsaw's bar. QVR_DECAP_SWING or _SAW (the struck
// point's velocity in vr_decap_slashvel), 0 if not. `xLive`: the hand's blow (VR_Melee_Decide's kind: a slash, not a
// stab or a pommel strike); else a corpse's (VR_Corpse_StrikeFrame: the blade's place and motion only).
string vr_decap_no;    // VR_Decap_Slash's refusal ("": none)
float vr_decap_quiet;  // VR_Decap_Slash says nothing (VR_Decap_Judge's)
float(float xHand, vector xAt, float xLive) VR_Decap_Slash =
{
    entity b = VR_Blow(xHand);
    float thing = b.mh_thing;
    vr_decap_no = "not a blade held by its handle";
    if(b.mh_free || (thing != VR_MTHING_SWORD && thing != VR_MTHING_AXE && thing != VR_MTHING_SAW) ||
       (thing == VR_MTHING_AXE && b.mh_wid != WID_AXE && b.mh_wid != WID_SUPERAXE))
    {
        return 0; // (held off its handle: its pommel, hilt or handle struck with; Mjolnir: a hammer, not a blade)
    }
    vector line = b.mh_far - b.mh_grip;
    float len = vlen(line);
    if(len < 1)
    {
        return 0;
    }
    float f = ((xAt - b.mh_grip) * line) / (len * len); // (the grip 0, the tip 1, the pommel below 0)
    float how = QVR_DECAP_SWING;
    string no = "";
    if(thing == VR_MTHING_SAW)
    {
        how = QVR_DECAP_SAW;
        if(f < VR_SAW_BAR_START || (xLive && b.mh_kind == VR_MKIND_POMMEL))
        {
            no = "the chainsaw's body or handle";
        }
    }
    else if(xLive && b.mh_kind != VR_MKIND_SLASH)
    {
        no = (b.mh_kind == VR_MKIND_STAB) ? "a stab" : "a pommel or hilt";
    }
    else if(f < ((thing == VR_MTHING_AXE) ? VR_DECAP_BLADE_AXE : VR_DECAP_BLADE_SWORD))
    {
        no = (f < 0) ? "the pommel" : "the hilt or handle";
    }
    vector vp = VR_Melee_PointVel(b, min(1, f));
    float speed = vlen(vp);
    if(no == "" && how != QVR_DECAP_SAW)
    {
        vector axis = normalize(b.mh_rfar - b.mh_rgrip);
        if(speed < cvar_hget(cvarh_vr_decap_speed))
        {
            no = sprintf("too slow (%.1f m/s)", speed);
        }
        else if(fabs(vp * axis) > speed * cos(min(90, max(0, cvar_hget(cvarh_vr_decap_slash_angle))) * (3.14159265 / 180)))
        {
            no = sprintf("along the blade, a stab (%.2f)", fabs(vp * axis) / speed);
        }
    }
    vr_decap_no = no;
    if(no != "")
    {
        if(!vr_decap_quiet)
        {
            dprint(sprintf("decap: no: %s at the head (%.2f along the blade)\n", no, f));
        }
        return 0;
    }
    vr_decap_slashvel = VR_Blow_ToWorld(vp) * VRMetersToUnits();
    return how;
};

// ----------------------------------------------------------------------------
float(float xSpeed) VR_Gib_StrikeDamage;

// Blunt melee head pops (ROUND21.md, "Blunt melee head pops"; the author: a punch, the crowbar or a gun's butt should
// very rarely pop a head, Mjolnir very often). A blunt blow's headshot kill (a fist, a gun's butt or barrel, the
// crowbar, a club, a pommel or handle end, Mjolnir's head) pops the head by chance: its weapon's scale
// (vr_decap_pop_fist_scale, _gun_, _crowbar_, _pommel_, _club_, _mjolnir_) times the blow's hardness to the power
// vr_decap_pop_melee_curve, at most 1. The hardness (0..1): the striking part's speed (mh_speed) on its way from
// vr_decap_pop_melee_soft_speed to _hard_speed, and the blow's damage (the headshot multiplier in, before Quad's x4) on
// its way from vr_decap_pop_melee_soft_damage to _hard_damage, mixed by vr_decap_pop_melee_damage_weight. A corpse's
// head struck the same (its speed the hand's, its damage the corpse strike's). Blades' slashes and the chainsaw still
// cut heads off, sure. vr_decap_pop_chance 0: every blunt headshot kill pops, as before. With Quad Damage
// (vr_decap_pop_quad): always (VR_Decap_QuadPops).

float VR_DECAP_BCLASS_FIST = 0;
float VR_DECAP_BCLASS_GUN = 1;
float VR_DECAP_BCLASS_CROWBAR = 2;
float VR_DECAP_BCLASS_POMMEL = 3;
float VR_DECAP_BCLASS_CLUB = 4;
float VR_DECAP_BCLASS_MJOLNIR = 5;
string vr_decap_why;    // the armed blow's chance's reason (the roll's print)

string(float xClass) VR_Decap_BluntName =
{
    if(xClass == VR_DECAP_BCLASS_FIST) { return "fist"; }
    if(xClass == VR_DECAP_BCLASS_GUN) { return "gun"; }
    if(xClass == VR_DECAP_BCLASS_CROWBAR) { return "crowbar"; }
    if(xClass == VR_DECAP_BCLASS_CLUB) { return "club"; }
    if(xClass == VR_DECAP_BCLASS_MJOLNIR) { return "mjolnir"; }
    return "pommel";
};

// Blow `b`'s blunt class, `xThing` what the hand holds (VR_MTHING_*).
float(entity b, float xThing) VR_Decap_BluntClass =
{
    if(xThing == VR_MTHING_FIST) { return VR_DECAP_BCLASS_FIST; }
    if(xThing == VR_MTHING_GUN) { return VR_DECAP_BCLASS_GUN; }
    if(xThing == VR_MTHING_CROWBAR) { return VR_DECAP_BCLASS_CROWBAR; }
    if(xThing == VR_MTHING_CLUB) { return VR_DECAP_BCLASS_CLUB; }
    if(b.mh_wid == WID_MJOLNIR && b.mh_kind != VR_MKIND_POMMEL) { return VR_DECAP_BCLASS_MJOLNIR; }
    return VR_DECAP_BCLASS_POMMEL;
};

// A blunt blow striking at `xSpeed` m/s for `xDamage`: its hardness (0..1).
float(float xSpeed, float xDamage) VR_Decap_BluntHardness =
{
    float s0 = cvar_hget(cvarh_vr_decap_pop_melee_soft_speed);
    float s1 = max(s0 + 0.01, cvar_hget(cvarh_vr_decap_pop_melee_hard_speed));
    float d0 = cvar_hget(cvarh_vr_decap_pop_melee_soft_damage);
    float d1 = max(d0 + 0.01, cvar_hget(cvarh_vr_decap_pop_melee_hard_damage));
    float w = min(1, max(0, cvar_hget(cvarh_vr_decap_pop_melee_damage_weight)));
    float ts = min(1, max(0, (xSpeed - s0) / (s1 - s0)));
    float td = min(1, max(0, (xDamage - d0) / (d1 - d0)));
    return (1 - w) * ts + w * td;
};

// The chance a blunt blow of class `xClass` at `xSpeed` m/s for `xDamage` that kills with a headshot pops the head.
float(float xClass, float xSpeed, float xDamage) VR_Decap_BluntChance =
{
    if(!cvar_hget(cvarh_vr_decap_pop_chance))
    {
        return 1;
    }
    float h = VR_Decap_BluntHardness(xSpeed, xDamage);
    float scale = cvar_hget(cvarh_vr_decap_pop_pommel_scale);
    if(xClass == VR_DECAP_BCLASS_FIST) { scale = cvar_hget(cvarh_vr_decap_pop_fist_scale); }
    else if(xClass == VR_DECAP_BCLASS_GUN) { scale = cvar_hget(cvarh_vr_decap_pop_gun_scale); }
    else if(xClass == VR_DECAP_BCLASS_CROWBAR) { scale = cvar_hget(cvarh_vr_decap_pop_crowbar_scale); }
    else if(xClass == VR_DECAP_BCLASS_CLUB) { scale = cvar_hget(cvarh_vr_decap_pop_club_scale); }
    else if(xClass == VR_DECAP_BCLASS_MJOLNIR) { scale = cvar_hget(cvarh_vr_decap_pop_mjolnir_scale); }
    return min(1, max(0, scale) * pow(h, max(0, cvar_hget(cvarh_vr_decap_pop_melee_curve))));
};

// Whether `by` (the blow's or the shot's) has Quad Damage and so pops every head it kills with a headshot
// (vr_decap_pop_quad): no chance, no range, no blast's head share (ROUND21.md, "Quad Damage always pops").
float(entity by) VR_Decap_QuadPops =
{
    return cvar_hget(cvarh_vr_decap_pop_quad) && (by.flags & FL_CLIENT) && by.super_damage_finished > time;
};

// A blow of `xHow` about to strike `targ` (alive) at its head with the blade at `xVel`: armed, Killed beheads it.
float(entity targ, entity attacker, vector xAt, vector xVel, float xHow) VR_Decap_Arm =
{
    if(!VR_Decap_Allowed(targ) || !VR_Decap_HowOn(xHow) ||
       (targ.health <= 0 && (!VR_Decap_IsPop(xHow) || !cvar_hget(cvarh_vr_decap_corpses))))
    {
        return FALSE;
    }
    vr_decap_ent = targ;
    vr_decap_by = attacker;
    vr_decap_time = time;
    vr_decap_vel = xVel;
    vr_decap_at = xAt;
    vr_decap_how = xHow;
    vr_decap_chance = 1;
    vr_decap_limb = -1;
    return TRUE;
};

// A hand's blow about to deal its damage to `targ` (T_Damage_VRMelee; self the player, vr_hithand the hand): armed if
// it is a beheading cut at a live monster's head.
float(entity targ, entity attacker, float xDamage) VR_Decap_HeadBlow =
{
    if(vr_hitkind != QVR_HITKIND_MELEE || vr_hithand < 0 || attacker != self || !(self.flags & FL_CLIENT) ||
       targ.health <= 0 || !VR_Decap_Allowed(targ))
    {
        return FALSE;
    }
    entity b = VR_Blow(vr_hithand);
    if(b.mh_kind == VR_MKIND_POMMEL || b.mh_wid == WID_MJOLNIR || b.mh_thing == VR_MTHING_FIST || b.mh_thing == VR_MTHING_GUN ||
       b.mh_thing == VR_MTHING_CROWBAR || b.mh_thing == VR_MTHING_CLUB)
    {
        if(!VR_Decap_OnHead(targ, b.mh_c_pos, '0 0 0'))
        {
            dprint(sprintf("decap: no: a blunt blow off %s's head (frame %g)\n", targ.classname, targ.frame));
            return FALSE;
        }
        // A zombie's head pops only to a blow that would knock it down, as a shot's (VR_DECAP_ZOMBIE_DOWN, 25: a light
        // tap it survives keeps its head; VR_Decap_After kills an armed zombie).
        if(targ.classname == "monster_zombie" && xDamage < 25) { return FALSE; }
        if(!VR_Decap_Arm(targ, attacker, b.mh_c_pos, VRGetHandVel(vr_hithand) * VRMetersToUnits(), QVR_DECAP_BLUNT))
        {
            return FALSE;
        }
        // (Its chance: Blunt melee head pops above.)
        float bc = VR_Decap_BluntClass(b, b.mh_thing);
        vr_decap_chance = VR_Decap_BluntChance(bc, b.mh_speed, xDamage);
        vr_decap_why = sprintf("%s %.1f m/s %.0f damage, hardness %.2f", VR_Decap_BluntName(bc), b.mh_speed, xDamage,
            VR_Decap_BluntHardness(b.mh_speed, xDamage));
        return TRUE;
    }
    if(b.mh_thing != VR_MTHING_SWORD && b.mh_thing != VR_MTHING_AXE && b.mh_thing != VR_MTHING_SAW)
    {
        return FALSE;
    }
    if(VR_Decap_OnMeleeHead(targ, b.mh_c_pos, b.mh_c_dir))
    {
        float how = VR_Decap_Slash(vr_hithand, b.mh_c_pos, TRUE);
        if(how && VR_Decap_Arm(targ, attacker, b.mh_c_pos, vr_decap_slashvel, how))
        {
            return TRUE;
        }
    }
    // With Quad Damage, a blade's blow at the head that doesn't cut it off (a stab, a slow one) pops it.
    if(VR_Decap_QuadPops(attacker) && VR_Decap_OnHead(targ, b.mh_c_pos, '0 0 0') &&
       (targ.classname != "monster_zombie" || xDamage >= 25))
    {
        return VR_Decap_Arm(targ, attacker, b.mh_c_pos, VRGetHandVel(vr_hithand) * VRMetersToUnits(), QVR_DECAP_QUAD);
    }
    return FALSE;
};

// Whether `e` is judged for beheading: one that may lose its head (VR_Decap_Allowed; the training dummy as the enemy it
// stands as, its classname: it says whether it would, VR_Decap_Judge, and loses it once killed: vr_dummy_gib).
float(entity e) VR_Decap_Judged =
{
    return VR_Decap_Allowed(e);
};

// A hand's blade blow landing on `targ` (self the player; hand `xHand`'s blow, VR_Blow: its contact mh_c_pos), judged by
// VR_Decap_Blow's rules as if it killed: 1 it beheads, 0 not, -1 not judged (not a sword's, an axe's or the chainsaw's
// blow; `targ` not one that may lose its head, VR_Decap_Judged). For the motion recorder's "decap" event and the training
// dummy's report (docs/vr-port/MOTIONS.md, "Decapitation takes"): vr_decap_judge_why says why, with the numbers to tune
// the rules on: "zone in|out d/r off h below the head's middle (units on the model standing, from the zone's line)
// part f (the striking part: f the share of the way from the grip to the tip) slash a deg (the struck point's motion off
// the blade's line: 0 a stab, 90 straight across) flat b deg (off the edge: 0 the edge first, 90 the flat; the blade's
// flat taken as the hand's sides) tilt c deg (the motion above level: -90 a chop straight down) speed s m/s kind k", then
// the refusal (" - refused (...)") if any; no ':' nor ';' (the take's events column). vr_decap_judge_zone: 1 in the zone.
string vr_decap_judge_why;
float vr_decap_judge_zone;
float VR_DEG = 57.29578; // degrees a radian
float(entity targ, float xHand) VR_Decap_Judge =
{
    vr_decap_judge_why = "";
    vr_decap_judge_zone = 0;
    if(xHand < 0 || !(self.flags & FL_CLIENT) || !VR_Decap_Judged(targ))
    {
        return -1;
    }
    entity b = VR_Blow(xHand);
    if(b.mh_thing != VR_MTHING_SWORD && b.mh_thing != VR_MTHING_AXE && b.mh_thing != VR_MTHING_SAW)
    {
        return -1;
    }
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    vector at = b.mh_c_pos;

    // The zone: where the point struck lies on the model standing, from the zone's line (the head's middle down the neck).
    float onZone = VR_Decap_OnMeleeHead(targ, at, b.mh_c_dir);
    vr_decap_judge_zone = onZone;
    string zone = "zone none";
    if(VR_Decap_MeleeZone(targ))
    {
        vector p = hitmodel_target(targ) ? hitmodel_rest(targ, at) : at;
        vector q = vr_decap_zone_top;
        q_z = min(vr_decap_zone_top_z, max(vr_decap_zone_bottom_z, p_z));
        zone = sprintf("zone %s %.1f/%.1f off %.1f below the head's middle", onZone ? "in" : "out", vlen(p - q),
                       xHeadRadius, vr_decap_zone_top_z - p_z);
    }

    // The blade: the share of the way from the grip to the tip (below 0 the pommel), its motion there.
    vector line = b.mh_far - b.mh_grip;
    float len = max(1, vlen(line));
    float f = ((at - b.mh_grip) * line) / (len * len);
    vector vp = VR_Melee_PointVel(b, min(1, f));
    float speed = vlen(vp);
    vector axis = normalize(b.mh_rfar - b.mh_rgrip);
    vector across = vp - axis * (vp * axis);
    float slash = atan2(vlen(across), fabs(vp * axis)) * VR_DEG;
    makevectors(VRGetEntHandRot(self, xHand));
    vector side = VR_Blow_ToPlaySpace(v_right);
    side = normalize(side - axis * (side * axis));
    float flatPart = fabs(across * side);
    float flat = atan2(flatPart, vlen(across - side * (across * side))) * VR_DEG;
    float tilt = atan2(vp_z, sqrt(vp_x * vp_x + vp_y * vp_y)) * VR_DEG;
    string kind = VR_Melee_Category(b.mh_kind, b.mh_thing);

    vr_decap_quiet = TRUE;
    float how = VR_Decap_Slash(xHand, at, TRUE);
    vr_decap_quiet = FALSE;
    float yes = onZone && how && VR_Decap_HowOn(how);
    string no = "";
    if(!onZone)
    {
        no = " - refused (not at the head or neck)";
    }
    else if(!how)
    {
        no = sprintf(" - refused (%s)", vr_decap_no);
    }
    else if(!yes)
    {
        no = " - refused (Chainsaw off)";
    }
    string angles = sprintf("slash %.0f deg flat %.0f deg tilt %.0f deg", slash, flat, tilt);
    vr_decap_judge_why = sprintf("%s part %.2f (%s) %s speed %.1f m/s kind %s%s", zone, f, b.mh_part, angles, speed, kind, no);
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return yes;
};

// The armed blow's damage dealt: a zombie it didn't kill is beheaded all the same (it dies for good); the blow is done.
void(entity targ) VR_Decap_After =
{
    if(vr_decap_ent != targ)
    {
        return;
    }
    float zombie = targ.classname == "monster_zombie" || (vr_decap_limb >= 0 && VR_Limb_IsZombie(targ)); // (a limb: a mummy too)
    if(vr_decap_time == time && zombie && targ.health > 0 && !targ.vr_gib &&
       (targ.flags & FL_MONSTER) && (!VR_IsDummy(targ) || cvar_hget(cvarh_vr_dummy_gib))) // (the dummy: only if it dies)
    {
        // (Cut first: a zombie that can't be beheaded, ragdolls off, isn't killed by it; Killed then cuts no more.)
        entity by = vr_decap_by;
        float oldHealth = targ.health;
        vr_decap_ent = world;
        if(!VR_Decap_Roll(targ))
        {
            return;
        }
        targ.health = -1;
        float cut = (vr_decap_limb >= 0) ? VR_Limb_Cut(targ, by, vr_decap_vel, vr_decap_how, vr_decap_limb, TRUE) :
                                           VR_Decap_Cut(targ, by, vr_decap_vel, vr_decap_how);
        if(!cut)
        {
            targ.health = oldHealth;
            return;
        }
        targ.th_die = VR_Decap_ZombieDie;
        Killed(targ, by);
    }
    vr_decap_ent = world;
};

// The head gib: `m` at `xAt` turned `xAng`, flying at `xVel` and spinning at `xSpin` (rad/s), as a thrown head
// (ThrowHead, player.qc) of its own; a rigid body from the start (as a thrown gib's flight: Box3D turns it as it spins,
// at any angle; it passes through its body until clear of it).
entity(entity c, string m, vector xAt, vector xAng, vector xVel, vector xSpin) VR_Decap_ThrowHead =
{
    entity h = spawn();
    h.classname = c.classname; // (as the head a gibbing throws: the monster itself)
    setmodel(h, m);
    h.frame = 0;
    h.model_scale = c.model_scale;
    setorigin(h, xAt);
    h.angles = xAng;
    h.movetype = MOVETYPE_BOUNCE;
    h.takedamage = DAMAGE_NO;
    h.solid = SOLID_NOT;
    h.view_ofs = '0 0 8';
    setsize(h, '-10 -10 0', '10 10 10');
    h.velocity = xVel;
    h.avelocity = '0 0 0';
    h.flags = 0;
    h.gorging = TRUE;
    h.think = HeadThink;
    h.nextthink = time + 1.5;
    VR_MakeGibGrabbable(h);
    VR_Gib_MakeDestroyable(h, 1.5);
    VR_SmallGib_CarryMults(c, h); // (vr_smallgibs.qc) it carries its enemy's gib and brain counts: bursting it later
    VR_Gore_WatchFling(h); // (it may stick to a ceiling or a wall)
    h.vr_rigid = 1;
    h.vr_spin = xSpin;
    VR_Limb_SetMass(h, c, -1); // (its share of the ragdoll's mass; vr_limbs.qc)
    VR_Limb_EndFountain(h, c, -1, TRUE); // (its neck spurts as it flies)
    particle2(xAt, xVel * 0.3, QVR_PARTICLE_PRESET_BLOOD, 30);
    vr_decap_lasthead = h;
    return h;
};

// The stump's fountain (self; owner its body): spurts of blood out of the neck, pulsing and dying away over
// vr_decap_fountain s; the drops fall and mark the floor (the particles' decals).
void() VR_Decap_FountainThink =
{
    entity c = self.owner;
    float age = time - self.ltime;
    float bone = self.vr_limb_bone; // (-1 the neck; a limb's stump: its joint, vr_limbs.qc)
    float stump = (bone < 0) ? c.vr_headless : (ragdolllimbs(c, 2) & (1 << bone));
    if(age > self.wait || !stump || c.modelindex != self.cnt || !isragdoll(c))
    {
        remove(self);
        return;
    }
    self.nextthink = time + VR_DECAP_SPURT_TICK;
    vector neck = ragdollcut(c, 4, bone);
    vector out = ragdollcut(c, 5, bone);
    float fade = 1 - age / max(0.1, self.wait);
    float beat = 0.55 + 0.45 * cos(age * VR_DECAP_BEAT * 2 * 3.14159265);
    float s = fade * beat;
    vector jitter;
    jitter_x = crandom() * 10;
    jitter_y = crandom() * 10;
    jitter_z = crandom() * 6;
    // The spurt: drops thrown out of the neck (a blood trail's: thrown along `dir`, falling; not the Blood preset, whose
    // mist cloud each time hung over the body as a red haze); and now and then a hit's spray of blood onto the floor
    // and walls round it (the gore's lines: splats and drips).
    particle2(neck + out * 1.5, out * (3 + 7 * s) + jitter * 0.04, QVR_PARTICLE_PRESET_BLOODTRAIL, 2 + ceil(4 * s));
    if(time >= self.pausetime && s > 0.3)
    {
        self.pausetime = time + 0.3;
        vector down = out;
        down_z = 0;
        down = normalize(down + jitter * 0.1 - '0 0 0.7');
        particle2(neck, down * 7, QVR_GORE_HIT, 6 + 3000); // (a blade's hit: kind 3)
    }
};

// A head popped (VR_Decap_IsPop) at `xAt` by a shot going `xVel`: as its head gib destroyed (VR_Gib_Burst: a mist of
// blood, the gore's burst, small gibs, brain chunks, the gib sound) and a squish.
void(entity e, vector xAt, vector xVel) VR_Decap_PopEffects =
{
    VR_BloodMist(xAt, xVel, VR_DECAP_POP_SIZE);
    VR_Gore_Burst(xAt, xVel, VR_DECAP_POP_SIZE);
    VR_SmallGib_Burst(xAt, xVel, VR_DECAP_POP_SIZE, e); // its small gibs (vr_smallgibs.qc)
    VR_SmallGib_Brains(xAt, xVel, TRUE, e); // and its brain (vr_smallgibs.qc)
    VR_SoundVaried(e, CHAN_AUTO, VR_Gib_SquishSound(FALSE), 1, ATTN_NORM);
    sound(e, CHAN_BODY, "player/udeath.wav", 0.8, ATTN_NORM);
    vr_decap_pops++;
};

// The cut's look and sound on `e` (just beheaded: ragdollcut knows the head and the stump), the blade (or the shot) at
// `xVel`, `xHow` the blow's: a head pop bursts the head where it was, and no head flies.
void(entity e, vector xVel, float xHow) VR_Decap_Effects =
{
    float pop = VR_Decap_IsPop(xHow);
    string m = VR_Decap_HeadModel(e);
    if(pop)
    {
        VR_Decap_PopEffects(e, ragdollcut(e, 0), xVel);
    }
    else if(m != "")
    {
        VR_Decap_ThrowHead(e, m, ragdollcut(e, 0), ragdollcut(e, 1), ragdollcut(e, 2), ragdollcut(e, 3));
    }

    // The stump: blood painted round the neck (the head's vertices drawn there), the cut's squish, a burst of blood and
    // a few small gibs; then its fountain.
    vector neck = ragdollcut(e, 4);
    vector out = ragdollcut(e, 5);
    vector side;
    side_x = out_y;
    side_y = -out_x;
    side_z = 0;
    side = (vlen(side) > 0.1) ? normalize(side) : '1 0 0';
    vector side2;
    side2_x = out_y * side_z - out_z * side_y;
    side2_y = out_z * side_x - out_x * side_z;
    side2_z = out_x * side_y - out_y * side_x;
    woundevent(e, neck + out * 4, out * -1, QVR_WOUND_MELEE, 60, 0);
    woundevent(e, neck + side * 4, side * -1, QVR_WOUND_MELEE, 50, 0);
    woundevent(e, neck - side * 4, side, QVR_WOUND_MELEE, 50, 0);
    woundevent(e, neck + side2 * 4, side2 * -1, QVR_WOUND_MELEE, 50, 0);
    woundevent(e, neck - side2 * 4, side2, QVR_WOUND_MELEE, 50, 0);
    if(!pop) // (a head pop's burst is there already)
    {
        VR_SoundVaried(e, CHAN_AUTO, VR_Gib_SquishSound(FALSE), 1, ATTN_NORM);
        sound(e, CHAN_BODY, "player/udeath.wav", 0.5, ATTN_NORM);
        VR_BloodMist(neck, xVel, 0.6);
        VR_Gore_Burst(neck, out, 0.5);
    }
    if(!pop && cvar_hget(cvarh_vr_smallgibs))
    {
        for(float i = 0; i < 3; i++)
        {
            vector v = out * (60 + random() * 60) + normalize(xVel) * 40 + side * (crandom() * 40) + '0 0 40';
            VR_SmallGib_Tag(VR_SmallGib_Spawn(neck + out * 2, v, e), QVR_SGIB_SIT_MELEE);
        }
    }
    float len = cvar_hget(cvarh_vr_decap_fountain);
    if(len > 0)
    {
        entity f = spawn();
        f.classname = "vr_decap_fountain";
        f.owner = e;
        f.ltime = time;
        f.wait = len;
        f.cnt = e.modelindex;
        f.vr_limb_bone = -1; // (the neck)
        f.think = VR_Decap_FountainThink;
        f.nextthink = time;
    }
};

// Cuts `e`'s head off (a dead monster: just killed, dying, a corpse or a ragdoll): its ragdoll headless (made now if it
// has none), its head thrown by the blade at `xVel`. FALSE if it can't (ragdolls off, no rig).
float(entity e, entity by, vector xVel, float xHow) VR_Decap_Cut =
{
    float oldSolid = e.solid;
    if(e.solid != SOLID_NOT && e.solid != SOLID_NOT_BUT_TOUCHABLE)
    {
        e.solid = SOLID_NOT; // (a dead monster to the engine: its ragdoll)
    }
    // (A head pop: the shot's force went into the head; the body keeps vr_decap_pop_body_speed of its knock.)
    if(!ragdolldecap(e, xVel, VR_Decap_IsPop(xHow) ? max(0, cvar_hget(cvarh_vr_decap_pop_body_speed)) : 1))
    {
        e.solid = oldSolid;
        dprint(sprintf("decap: no: %s has no ragdoll to behead (Ragdolls off?)\n", e.classname));
        return FALSE;
    }
    e.vr_headless = 1; // (the engine's too)
    VR_Decap_Effects(e, xVel, xHow);
    vr_decap_count++;
    if(VR_Decap_IsPop(xHow))
    {
        dprint(sprintf("decap: %s's head popped by %s (%s)\n", e.classname, by.classname, VR_Decap_HowName(xHow)));
        return TRUE;
    }
    dprint(sprintf("decap: %s beheaded by %s (%s at %.1f m/s): its head at %.0f u/s\n", e.classname, by.classname,
        VR_Decap_HowName(xHow), vlen(xVel) / VRMetersToUnits(), vlen(ragdollcut(e, 2))));
    return TRUE;
};

// Killed, before the death code (self is `e`): the armed blow beheads it. It dies whole (its death code would gib it);
// a zombie dies for good (VR_Decap_ZombieDie).
void(entity e) VR_Decap_Killed =
{
    e.vr_decap_died = time;
    if(vr_decap_ent != e || vr_decap_time != time)
    {
        VR_Limb_BlastKilled(e); // (an explosion's kill: its limbs near the blast may pop, vr_limbs.qc)
        return;
    }
    vr_decap_ent = world;
    if(vr_decap_limb >= 0)
    {
        VR_Limb_Killed(e); // (the blow at a limb: vr_limbs.qc)
        return;
    }
    if(!VR_Decap_Roll(e) || !VR_Decap_Cut(e, vr_decap_by, vr_decap_vel, vr_decap_how))
    {
        return;
    }
    if(e.health < -1)
    {
        e.health = -1;
    }
    if(e.classname == "monster_zombie" || e.classname == "monster_mummy")
    {
        e.th_die = VR_Decap_ZombieDie; // (a mummy's death always gibs it: mummy_die)
    }
};

// A beheaded zombie's death: it lies as its ragdoll does, never to get up (zombie_die gibs it). A beheaded mummy's too
// (Rogue's mummy_die always gibs it; a mummy is killed as other monsters are, not beheaded whatever as a zombie is).
void() VR_Decap_ZombieDie =
{
    self.think = SUB_Null;
    self.nextthink = 0;
    self.inpain = 0;
    self.solid = SOLID_NOT;
    self.deadflag = DEAD_DEAD;
};

// A hand's blow at corpse `c` (VR_Corpse_StrikeFrame; self the player) landing at `xAt`: a beheading cut at its head
// (its ragdoll's head part, or a dying body's head) cuts it off instead of taking its corpse health. TRUE if it did.
float(entity c, float xHand, vector xAt) VR_Decap_HeadCorpseBlow =
{
    if(!cvar_hget(cvarh_vr_decap) || !cvar_hget(cvarh_vr_decap_corpses) || !VR_Decap_Allowed(c))
    {
        return FALSE;
    }
    vr_decap_limb = -1;
    entity b = VR_Blow(xHand);
    float thing = VR_Melee_Thing(self, xHand);
    if((b.mh_thing == thing && (b.mh_kind == VR_MKIND_POMMEL || b.mh_wid == WID_MJOLNIR)) || thing == VR_MTHING_FIST || thing == VR_MTHING_GUN ||
       thing == VR_MTHING_CROWBAR || thing == VR_MTHING_CLUB)
    {
        float head = ragdollhead(c, xAt, 0); // a head hit, without the blade's neck extension
        if(head < 0) { head = VR_Decap_OnHead(c, xAt, '0 0 0'); }
        if(!head)
        {
            return FALSE;
        }
        // By chance, as a live one's (Blunt melee head pops; its damage the corpse strike's, VR_Corpse_StrikeFrame).
        float speed = vlen(VRGetHandVel(xHand));
        float dmg = VR_Gib_StrikeDamage(speed) * (VRIsHandWeaponless(self, xHand) ? 1 : 1.5);
        float bc = VR_Decap_BluntClass(b, thing);
        if(!VR_Decap_QuadPops(self))
        {
            vr_decap_by = self;
            vr_decap_chance = VR_Decap_BluntChance(bc, speed, dmg);
            vr_decap_how = QVR_DECAP_BLUNT;
            vr_decap_why = sprintf("%s at a corpse %.1f m/s %.0f damage", VR_Decap_BluntName(bc), speed, dmg);
            if(!VR_Decap_Roll(c))
            {
                return FALSE;
            }
        }
        return VR_Decap_Cut(c, self, VRGetHandVel(xHand) * VRMetersToUnits(), QVR_DECAP_BLUNT);
    }
    if(b.mh_thing != VR_MTHING_SWORD && b.mh_thing != VR_MTHING_AXE && b.mh_thing != VR_MTHING_SAW)
    {
        return FALSE;
    }
    float on = ragdollhead(c, xAt, cvar_hget(cvarh_vr_decap_neck));
    if(on < 0)
    {
        on = (hitmodel_target(c) && VR_Decap_OnMeleeHead(c, xAt, VR_Blow_ToWorld(b.mh_vfar)));
    }
    if(!on)
    {
        return FALSE;
    }
    float how = VR_Decap_Slash(xHand, xAt, FALSE);
    return how && VR_Decap_HowOn(how) && VR_Decap_Cut(c, self, vr_decap_slashvel, how);
};

// The running chainsaw's bar in `e` at `xAt` (VR_Chainsaw_Cut; self the player), moving at `xVel` (units/s): 2 a
// corpse (or a dying body) beheaded (no tick's damage), 1 a live one armed (its tick's damage beheads if it kills; a
// zombie whatever: VR_Decap_After), 0 neither.
float(entity e, float xHand, vector xAt) VR_Decap_HeadSaw =
{
    if(!cvar_hget(cvarh_vr_decap) || !cvar_hget(cvarh_vr_decap_chainsaw) || !VR_Decap_Allowed(e))
    {
        return 0;
    }
    entity b = VR_Blow(xHand);
    vector vel = VR_Blow_ToWorld(VR_Melee_PointVel(b, 1)) * VRMetersToUnits();
    vector dir = normalize(VRGetMuzzlePos(xHand) - VRGetHandPos(xHand));
    if(e.health <= 0 || e.vr_corpse == 2 || e.vr_corpse_dying)
    {
        if(!cvar_hget(cvarh_vr_decap_corpses))
        {
            return 0;
        }
        float on = ragdollhead(e, xAt, cvar_hget(cvarh_vr_decap_neck));
        if(on < 0)
        {
            on = (hitmodel_target(e) && VR_Decap_OnMeleeHead(e, xAt, dir));
        }
        return (on && VR_Decap_Cut(e, self, vel, QVR_DECAP_SAW)) ? 2 : 0;
    }
    if(!VR_Decap_OnMeleeHead(e, xAt, dir))
    {
        return 0;
    }
    return VR_Decap_Arm(e, self, xAt, vel, QVR_DECAP_SAW) ? 1 : 0;
};

// A thrown axe `xAxe` gone into `m` edge first (VR_AxeStick_Stuck), before its blow: armed if at its head, fast enough
// (vr_decap_speed); a monster its hit box killed a moment ago (its blade going in after) beheaded at once.
float(entity m, entity xAxe) VR_Decap_HeadThrown =
{
    if(!cvar_hget(cvarh_vr_decap) || !cvar_hget(cvarh_vr_decap_thrown) || !VR_Decap_Allowed(m))
    {
        return FALSE;
    }
    vector vel = xAxe.velocity;
    vector at = xAxe.vr_stick_at;
    if(!VR_Decap_OnHead(m, at, vel))
    {
        return FALSE;
    }
    float speed = vlen(vel) / VRMetersToUnits();
    if(speed < cvar_hget(cvarh_vr_decap_speed))
    {
        dprint(sprintf("decap: no: a thrown axe too slow (%.1f m/s) at the head\n", speed));
        return FALSE;
    }
    entity by = VR_Throw_Thrower(xAxe);
    if(m.health > 0)
    {
        return VR_Decap_Arm(m, by, at, vel, QVR_DECAP_THROWN);
    }
    if(time - m.vr_decap_died < VR_DECAP_DIED_LATE)
    {
        VR_Decap_Cut(m, by, vel, QVR_DECAP_THROWN);
    }
    return FALSE;
};

// ----------------------------------------------------------------------------
// Head pops (ROUND21.md, "Head pops"): the player's shotgun or super shotgun blast, or lightning gun bolt, that kills a
// monster that can be beheaded with a headshot pops its head: no death animation and no gibbing (an overkill too), its
// ragdoll headless at once as a beheading's (VR_Decap_Cut, the stump's wounds and fountain), but no head flies: the head
// bursts where it was as its head gib destroyed would (VR_Decap_PopEffects). vr_decap_shotgun, vr_decap_super_shotgun,
// vr_decap_lightning.
// A blast counts as a whole: its pellets on him (FireBulletsImpl traces them one by one and deals a run of them on one
// target at once: ApplyMultiDamage), as far as they had struck when it killed him, their damage with the headshot
// multiplier; when at least vr_decap_head_share of it is from pellets at his head (VR_Decap_OnHead: positional damage's
// head zone, whether positional damage is on or not), it is a headshot. A bolt: the one that kills him strikes his head.
// A zombie (vr_decap_zombies): a headshot that would knock it down (VR_DECAP_ZOMBIE_DOWN, zombie_pain's) pops its head
// and it dies for good, as a slash's beheading does (VR_Decap_After).

float VR_DECAP_ZOMBIE_DOWN = 25; // a hit this hard knocks a zombie down (zombie_pain)
float VR_DECAP_SHOT_SPEED = 300; // units/s: a shot's "blade" velocity (the stump's blood, along the shot)

.float vr_decap_bserial; // the blast its pellets below are of (vr_sgib_blastserial)
.float vr_decap_bdmg;    // the blast's damage on it so far (the headshot multiplier in)
.float vr_decap_bhead;   // of which from pellets at its head
.vector vr_decap_bat;    // those pellets' points summed
.float vr_decap_bn;      // and how many
.vector vr_decap_bdir;   // every pellet's direction summed

// A pellet of the player's blast (vr_decap_weapon) of `xDamage` striking `e` at `xAt` going `xDir` (TraceAttack).
void(entity e, float xDamage, vector xAt, vector xDir) VR_Decap_HeadPellet =
{
    if(!vr_decap_weapon || !vr_sgib_blast || !VR_Decap_Allowed(e))
    {
        return;
    }
    if(e.vr_decap_bserial != vr_sgib_blastserial)
    {
        e.vr_decap_bserial = vr_sgib_blastserial;
        e.vr_decap_bdmg = 0;
        e.vr_decap_bhead = 0;
        e.vr_decap_bat = '0 0 0';
        e.vr_decap_bn = 0;
        e.vr_decap_bdir = '0 0 0';
    }
    e.vr_decap_bdmg = e.vr_decap_bdmg + xDamage;
    e.vr_decap_bdir = e.vr_decap_bdir + normalize(xDir);
    if(VR_Decap_OnHead(e, xAt, xDir))
    {
        e.vr_decap_bhead = e.vr_decap_bhead + xDamage;
        e.vr_decap_bat = e.vr_decap_bat + xAt;
        e.vr_decap_bn = e.vr_decap_bn + 1;
    }
};

// A blast's damage about to be dealt to `targ` (ApplyMultiDamage; self the shooter): armed (Killed pops its head) if
// the blast on it so far is a headshot.
float(entity targ) VR_Decap_HeadBlastArm =
{
    if(!vr_decap_weapon || !vr_sgib_blast || !(self.flags & FL_CLIENT) || targ.vr_decap_bserial != vr_sgib_blastserial ||
       targ.vr_decap_bn <= 0)
    {
        return FALSE;
    }
    float share = targ.vr_decap_bhead / max(0.001, targ.vr_decap_bdmg);
    float least = min(1, max(0, cvar_hget(cvarh_vr_decap_head_share)));
    if(share < least && !VR_Decap_QuadPops(self))
    {
        dprint(sprintf("decap: no: %s, %.2f of the blast at the head (Head Share %.2f)\n", targ.classname, share,
            least));
        return FALSE;
    }
    if(targ.classname == "monster_zombie" && targ.vr_decap_bdmg < VR_DECAP_ZOMBIE_DOWN)
    {
        return FALSE;
    }
    vector at = targ.vr_decap_bat * (1 / targ.vr_decap_bn);
    if(!VR_Decap_Arm(targ, self, at, normalize(targ.vr_decap_bdir) * VR_DECAP_SHOT_SPEED, vr_decap_weapon))
    {
        return FALSE;
    }
    vr_decap_chance = VR_Decap_PopChance(vr_decap_weapon, vlen(at - vr_decap_src),
        targ.vr_decap_bn / max(1, vr_decap_pellets));
    vr_decap_why = sprintf("%g of %g pellets at the head", targ.vr_decap_bn, vr_decap_pellets);
    return TRUE;
};

// The player's lightning bolt (vr_decap_weapon) about to deal `xDamage` to `targ`, striking it at `xAt` going `xDir`
// (LightningDamage; self the shooter): armed if it strikes its head.
float(entity targ, vector xAt, vector xDir, float xDamage) VR_Decap_HeadBoltArm =
{
    if(vr_decap_weapon != QVR_DECAP_LIGHTNING || !(self.flags & FL_CLIENT) || !targ.takedamage ||
       !VR_Decap_Allowed(targ) || !VR_Decap_OnHead(targ, xAt, xDir))
    {
        return FALSE;
    }
    if(targ.classname == "monster_zombie" && xDamage < VR_DECAP_ZOMBIE_DOWN)
    {
        return FALSE;
    }
    if(!VR_Decap_Arm(targ, self, xAt, normalize(xDir) * VR_DECAP_SHOT_SPEED, QVR_DECAP_LIGHTNING))
    {
        return FALSE;
    }
    vr_decap_chance = VR_Decap_PopChance(QVR_DECAP_LIGHTNING, vlen(xAt - (self.origin + self.view_ofs)), 1);
    vr_decap_why = "a bolt";
    return TRUE;
};

// ----------------------------------------------------------------------------
// Head pop chance (ROUND21.md, "Head pop chance"; the author: they happened far too often). A shotgun's or super
// shotgun's headshot kill pops the head always within vr_decap_pop_always_range player lengths (VR_DECAP_PLAYER_LENGTH)
// of the shooter, never at vr_decap_pop_never_range or farther, and between them by chance: the weapon's scale
// (vr_decap_pop_ssg_scale, _sg_scale) times (1 - t)^its falloff (vr_decap_pop_ssg_falloff, _sg_falloff: the shotgun's
// dies away sooner), t the way from the one range to the other, times (1 - w) + w x the share of the blast's pellets at
// the head (w: vr_decap_pop_pellet_weight). A lightning bolt's always pops (vr_decap_pop_lightning_always; off: the
// ranges with the super shotgun's falloff). A thrown or flung blunt weapon or prop (VR_Decap_ThrownArm) pops it when
// vr_decap_pop_thrown_mass kg or heavier, a lighter one at vr_decap_pop_thrown_light_chance. Melee blows keep theirs
// (sure). Only the killing blow rolls (VR_Decap_Roll: Killed, a zombie knocked down, a corpse's head shot);
// vr_decap_pop_chance 0: every headshot kill pops, as before.

float VR_DECAP_PLAYER_LENGTH = 56; // units: the player's height (the ranges' unit)
float vr_decap_rolls;   // rolls made (the tests)
float vr_decap_rollpops; // and won

// The chance a headshot kill of `xHow` from `xDist` units with `xShare` of its pellets at the head pops the head.
float(float xHow, float xDist, float xShare) VR_Decap_PopChance =
{
    if(!cvar_hget(cvarh_vr_decap_pop_chance) || (xHow != QVR_DECAP_SHOTGUN && xHow != QVR_DECAP_SSG &&
       xHow != QVR_DECAP_LIGHTNING))
    {
        return 1;
    }
    if(xHow == QVR_DECAP_LIGHTNING && cvar_hget(cvarh_vr_decap_pop_lightning_always))
    {
        return 1;
    }
    float near = max(0, cvar_hget(cvarh_vr_decap_pop_always_range)) * VR_DECAP_PLAYER_LENGTH;
    float far = max(near, cvar_hget(cvarh_vr_decap_pop_never_range) * VR_DECAP_PLAYER_LENGTH);
    if(xDist <= near)
    {
        return 1;
    }
    if(xDist >= far)
    {
        return 0;
    }
    float t = (xDist - near) / (far - near);
    float ssg = xHow != QVR_DECAP_SHOTGUN;
    float scale = max(0, cvar_hget(ssg ? cvarh_vr_decap_pop_ssg_scale : cvarh_vr_decap_pop_sg_scale));
    float falloff = max(0, cvar_hget(ssg ? cvarh_vr_decap_pop_ssg_falloff : cvarh_vr_decap_pop_sg_falloff));
    float w = min(1, max(0, cvar_hget(cvarh_vr_decap_pop_pellet_weight)));
    float pellets = (1 - w) + w * min(1, max(0, xShare));
    return min(1, scale * pow(1 - t, falloff) * pellets);
};

// The armed blow kills `e`: whether it beheads or pops (its chance, vr_decap_chance: 1 sure, else a roll; vr_decap_pop_roll
// >= 0 the roll's value, the tests').
float(entity e) VR_Decap_Roll =
{
    // (Times the head's or the limbs' global multiplier: vr_decap_chance_scale, vr_limbs_chance_scale.)
    float c = vr_decap_chance * max(0, cvar_hget(vr_decap_limb >= 0 ? cvarh_vr_limbs_chance_scale : cvarh_vr_decap_chance_scale));
    vr_decap_chance = 1;
    if(c >= 1)
    {
        return TRUE;
    }
    if(VR_Decap_QuadPops(vr_decap_by))
    {
        dprint(sprintf("decap: pop chance %.3f on %s (%s, %s): Quad Damage, popped\n", c, e.classname,
            VR_Decap_HowName(vr_decap_how), vr_decap_why));
        return TRUE;
    }
    float r = cvar_hget(cvarh_vr_decap_pop_roll);
    if(r < 0)
    {
        r = random();
    }
    float won = r < c;
    vr_decap_rolls++;
    if(won)
    {
        vr_decap_rollpops++;
    }
    dprint(sprintf("decap: pop chance %.3f on %s (%s, %s): rolled %.3f, %s\n", c, e.classname, VR_Decap_HowName(vr_decap_how),
        vr_decap_why, r, won ? "popped" : "no pop"));
    return won;
};

// `xThing` (a thrown weapon or prop, thrown or flung by `by`) about to deal its blow to `targ`: armed (Killed pops its
// head) if it is blunt (not a sword, an axe or the chainsaw: a thrown axe's edge beheads, VR_Decap_Thrown) and strikes
// its head (a zombie: a blow that knocks it down); sure when vr_decap_pop_thrown_mass kg or heavier, else vr_decap_pop_thrown_light_chance (0: not armed).
float(entity targ, entity xThing, entity by, float xDamage) VR_Decap_HeadThrownArm =
{
    if(!(by.flags & FL_CLIENT) || targ.health <= 0 || !(targ.flags & FL_MONSTER) || !VR_Decap_Allowed(targ) ||
       (vr_decap_ent == targ && vr_decap_time == time))
    {
        return FALSE;
    }
    float quad = VR_Decap_QuadPops(by);
    if(xThing.classname == "thrown_weapon" && !quad &&
       (xThing.weapon == WID_SWORD || xThing.weapon == WID_AXE || xThing.weapon == WID_CHAINSAW || xThing.weapon == WID_SUPERAXE))
    {
        return FALSE;
    }
    vector vel = xThing.velocity;
    if(!VR_Decap_OnHead(targ, xThing.origin, vel) || (targ.classname == "monster_zombie" && xDamage < VR_DECAP_ZOMBIE_DOWN))
    {
        return FALSE;
    }
    float mass = VR_Thrown_Mass(xThing);
    float least = cvar_hget(cvarh_vr_decap_pop_thrown_mass);
    float chance = (mass >= least || quad) ? 1 : min(1, max(0, cvar_hget(cvarh_vr_decap_pop_thrown_light_chance)));
    if(chance <= 0)
    {
        dprint(sprintf("decap: no pop: %s (%.1f kg, under %.1f) at %s's head\n", xThing.classname, mass, least,
            targ.classname));
        return FALSE;
    }
    if(!VR_Decap_Arm(targ, by, xThing.origin, vel, QVR_DECAP_THROWN_BLUNT))
    {
        return FALSE;
    }
    vr_decap_chance = chance;
    vr_decap_why = sprintf("%.1f kg", mass);
    dprint(sprintf("decap: armed: %s (%s, %.1f kg) at %s's head, chance %.2f\n", xThing.classname, xThing.netname, mass,
        targ.classname, chance));
    return TRUE;
};

// Any other blow of `attacker`'s with Quad Damage (vr_decap_pop_quad) about to deal `xDamage` (before Quad's x4) to
// `targ` (T_Damage; `inflictor` a nail, a rocket, a grenade, a thrown thing, a shot): armed (Killed pops its head) if it
// strikes its head: where a shot or a nail struck it (PositionalDamage's point, this frame), else where the projectile
// is, on it (not a blast's way off). Not while a blast or a bolt is dealt (vr_decap_weapon: their own arming, their
// toggles), nor what is armed already.
float(entity targ, entity inflictor, entity attacker, float xDamage) VR_Decap_HeadQuadArm =
{
    if(!VR_Decap_QuadPops(attacker) || vr_decap_weapon || targ.health <= 0 || !targ.takedamage ||
       !VR_Decap_Allowed(targ) || (vr_decap_ent == targ && vr_decap_time == time))
    {
        return FALSE;
    }
    vector at;
    vector dir = inflictor.velocity;
    if(vr_hitpart_target == targ && vr_hitpart_time == time)
    {
        at = vr_hitpart_pos;
        if(inflictor == attacker || vlen(dir) < 1)
        {
            dir = at - (attacker.origin + attacker.view_ofs);
        }
    }
    else if(inflictor != attacker && inflictor != world)
    {
        at = inflictor.origin;
        float m = 16;
        if(at_x < targ.absmin_x - m || at_y < targ.absmin_y - m || at_z < targ.absmin_z - m ||
           at_x > targ.absmax_x + m || at_y > targ.absmax_y + m || at_z > targ.absmax_z + m)
        {
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }
    if(!VR_Decap_OnHead(targ, at, dir) || (targ.classname == "monster_zombie" && xDamage * 4 < VR_DECAP_ZOMBIE_DOWN))
    {
        return FALSE;
    }
    if(!VR_Decap_Arm(targ, attacker, at, normalize(dir) * VR_DECAP_SHOT_SPEED, QVR_DECAP_QUAD))
    {
        return FALSE;
    }
    vr_decap_why = sprintf("%s at the head", inflictor.classname);
    dprint(sprintf("decap: armed: Quad Damage, %s at %s's head\n", inflictor.classname, targ.classname));
    return TRUE;
};

// ----------------------------------------------------------------------------
// Tests: `vr_decap_test <n>` (Debug > Gore Tests), on the monster (or corpse) nearest you, a blow made up as the hand's
// would be (its weapon's line across the head, its motion) and dealt as the melee's (T_Damage_VRMelee) or the corpses'
// (VR_Corpse_StrikeFrame's): 1 a sword's slash (kills: its health 1 first), 2 the same along the blade (a stab), 3 its
// pommel, 4 too slow, 5 at a ragdoll's or corpse's head (a corpse's cut), 6 the chainsaw's running bar at the neck (its
// health 1), 7 a slash at full health (a zombie: beheaded whatever), 8 gibs the nearest corpse (a headless one first;
// the heads thrown counted), 9 an axe's slash, 10 the chainsaw at a corpse's head, 11 its health 1 (no blow: a thrown
// axe's test). Head pops (a shot from your eye, VR_Decap_TestShot): 12 a shotgun blast at the head that just kills, 13
// the super shotgun's, 14 the super shotgun's at health 1 (overkill), 15 a lightning bolt at the head (health 1), 16 a
// shotgun blast at the body (health 1), 17 a shotgun blast at the head at health 500 (not killed), 18 a bolt at the body
// (health 1); a zombie at its 60. Prints `decaptest:` lines: beheaded or not, the heads thrown, popped, gibbed.

// The gore tests on a crowd (vr_gore_test_crowd: the benchmarks' gore scenarios, Debug > Gore Tests): each monster
// within its radius tested in turn as if it were the nearest (vr_gore_test_only), the tests' lines left out but one.
entity vr_gore_test_only;
float vr_gore_test_quiet;
void(float xTest, float xDecap) VR_Gore_TestCrowd;

void(string xText) VR_Gore_Print =
{
    if(!vr_gore_test_quiet)
    {
        bprint(xText);
    }
};

// The nearest monster (live: xLive 1; a dead one: 0) to `self` within 400 units (a crowd's test: its monster in turn).
entity(float xLive) VR_Decap_TestTarget =
{
    entity best = world;
    float bestDist = vr_gore_test_only != world ? 999999 : 400;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.vr_gib || e.vr_headless || (xLive ? e.health <= 0 : e.health > 0) ||
           (vr_gore_test_only != world && e != vr_gore_test_only))
        {
            continue;
        }
        float d = vlen(e.origin - self.origin);
        if(d < bestDist)
        {
            best = e;
            bestDist = d;
        }
    }
    return best;
};

// Hand `cVR_MainHand`'s blow made up: `xThing` held across the line from the player to `xAt` (its grip `xF` of its
// length short of it), moving `xAlong` (1: along its line) at `xSpeed` m/s, as kind `xKind`.
void(vector xAt, float xThing, float xKind, float xF, float xAlong, float xSpeed) VR_Decap_TestBlow =
{
    entity b = VR_Blow(cVR_MainHand);
    vector toward = xAt - self.origin;
    toward_z = 0;
    toward = normalize(toward);
    vector axis;
    axis_x = -toward_y;
    axis_y = toward_x;
    axis_z = 0;
    float len = 30;
    b.mh_thing = xThing;
    b.mh_wid = WID_SWORD;
    if(xThing == VR_MTHING_AXE)
    {
        b.mh_wid = WID_AXE;
    }
    else if(xThing == VR_MTHING_SAW)
    {
        b.mh_wid = WID_CHAINSAW;
    }
    b.mh_kind = xKind;
    b.mh_speed = xSpeed;
    b.mh_free = 0;
    b.mh_grip = xAt - axis * (len * xF);
    b.mh_far = b.mh_grip + axis * len;
    b.mh_near = b.mh_grip - axis * 6;
    vector eye = self.origin + self.view_ofs;
    b.mh_rgrip = VR_Blow_ToPlaySpace(b.mh_grip - eye) * (1 / VRMetersToUnits());
    b.mh_rfar = VR_Blow_ToPlaySpace(b.mh_far - eye) * (1 / VRMetersToUnits());
    b.mh_rnear = VR_Blow_ToPlaySpace(b.mh_near - eye) * (1 / VRMetersToUnits());
    vector way = normalize(toward * (1 - xAlong) + axis * xAlong);
    vector v = VR_Blow_ToPlaySpace(way) * xSpeed;
    b.mh_vgrip = v;
    b.mh_vfar = v;
    b.mh_vnear = v;
    b.mh_c_pos = xAt;
    b.mh_c_dir = way;
    b.mh_c_ent = world;
};

float vr_poptest_spread; // (VR_Decap_PopTest: the test shot with its weapon's spread)

// A head pop's test shot from your eye at `xAt`: a blast (`xWeapon` QVR_DECAP_SHOTGUN or _SSG: its pellets, no spread)
// or a bolt (_LIGHTNING), as the weapon's (vr_decap_weapon, its Weapon Damage).
void(float xWeapon, vector xAt) VR_Decap_TestShot =
{
    vector src = self.origin + self.view_ofs;
    if(vr_gore_test_only != world)
    {
        src = xAt + normalize(src - xAt) * 24; // (a crowd's: from just before it, not through the others)
    }
    vector dir = normalize(xAt - src);
    VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
    vr_decap_weapon = xWeapon;
    if(xWeapon == QVR_DECAP_LIGHTNING)
    {
        self.damage_weapon = IT_LIGHTNING;
        LightningDamage(src, xAt + dir * 40, self, cvar_hget(cvarh_vr_dmg_lightning));
    }
    else
    {
        self.damage_weapon = IT_SHOTGUN;
        vr_pellet_damage = cvar_hget(xWeapon == QVR_DECAP_SSG ? cvarh_vr_dmg_super_shotgun : cvarh_vr_dmg_shotgun);
        vr_tracer_hand = cVR_MainHand;
        vr_tracer_from = src;
        vector spread = '0 0 0';
        if(vr_poptest_spread)
        {
            spread = (xWeapon == QVR_DECAP_SSG) ? '0.035 0.025 0' : '0.01 0.01 0'; // (W_FireSuperShotgun's, W_FireShotgun's)
        }
        FireBulletsImpl(xWeapon == QVR_DECAP_SSG ? 14 : 6, dir, spread, src, 4096, FALSE);
        vr_pellet_damage = 0;
    }
    vr_decap_weapon = 0;
    VR_ClearHitKind();
};

// A test blast's damage, all at the head (`xWeapon` as VR_Decap_TestShot's).
float(float xWeapon) VR_Decap_TestBlastDamage =
{
    float head = cvar_hget(cvarh_vr_positional_damage) ? PositionalDamageFactor(1) : 1;
    if(xWeapon == QVR_DECAP_SSG)
    {
        return 14 * cvar_hget(cvarh_vr_dmg_super_shotgun) * head;
    }
    return 6 * cvar_hget(cvarh_vr_dmg_shotgun) * head;
};

// Heads (the head gib models) in the level.
float() VR_Decap_TestHeads =
{
    float n = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_gib && e.gorging) // (a head: ThrowHead's, VR_Decap_ThrowHead's)
        {
            n++;
        }
    }
    return n;
};

// Test 19 (Debug > Tests > Decapitation Tests > Sweep the Head Zone): how forgiving the melee's zone is. At each height
// from 24 units below the head zone's middle to 16 above it (2 apart), a striking point (3 units thick, the melee's
// tolerance: hitmodel_segment) moves level towards its head's axis from 16 sides (96 units out); where it meets the
// model, whether that is on the melee's zone (VR_Decap_OnMeleeHead: vr_decap_head_size, vr_decap_neck). `decapsweep:`
// lines: each height's contacts on it, then in all.
void(entity m) VR_Decap_Sweep =
{
    VR_Decap_HeadZone(m);
    float up = xHeadUp;
    float yaw = m.angles_y * (3.14159265 / 180);
    vector axis = m.origin;
    axis_x = axis_x + cos(yaw) * xHeadForward;
    axis_y = axis_y + sin(yaw) * xHeadForward;
    float contacts = 0;
    float on = 0;
    string row = "";
    for(float z = up - 24; z <= up + 16; z = z + 2)
    {
        float n = 0;
        float k = 0;
        for(float a = 0; a < 16; a++)
        {
            vector dir = '0 0 0';
            dir_x = cos(a * (3.14159265 / 8));
            dir_y = sin(a * (3.14159265 / 8));
            vector end = axis + '0 0 1' * z;
            vector start = end + dir * 96;
            float t = hitmodel_segment(m, start, end, 3, HITCLASS_MELEE);
            if(t <= 0)
            {
                continue;
            }
            n++;
            if(VR_Decap_OnMeleeHead(m, start + (end - start) * t, end - start))
            {
                k++;
            }
        }
        contacts = contacts + n;
        on = on + k;
        row = strcat(row, sprintf(" %g:%g/%g", z - up, k, n));
    }
    bprint(sprintf("decapsweep: %s (head %g up, size %g, neck %g): height from the head's middle: on/contacts%s\n",
        m.classname, up, cvar_hget(cvarh_vr_decap_head_size), cvar_hget(cvarh_vr_decap_neck), row));
    bprint(sprintf("decapsweep: %s: %g of %g contacts on the zone\n", m.classname, on, contacts));
};

// Head pop chance's tests (vr_decap_test 40..46; Debug > Gore Tests): 40 the chance table (each weapon at ranges and
// shares of its pellets at the head). On the monster nearest you, shot from vr_decap_poptest_dist player lengths off
// (you moved back along the line from its head to you, or round it till the way is clear; put back after): 41 a
// shotgun headshot that kills (no spread: every pellet at the head), 42 the super shotgun's, 43 a lightning bolt's,
// 44 a super shotgun body shot that kills; 45 the shotgun's and 46 the super shotgun's blast with its spread at the
// head vr_decap_poptest_n times, not killing it (its health kept high): each blast's chance rolled (random), the rates by
// the pellets at the head; 47 a weapon (vr_decap_poptest_wid) thrown into its head (vr_decap_poptest_body: its body) at
// 16 m/s that kills, 48 an explosive box. `poptest:` lines.
float vr_poptest_bn[15];    // blasts with that many pellets at the head (VR_Decap_PopTest 45, 46)
float vr_poptest_bpop[15];  // of which popped
float vr_poptest_bsum[15];  // their chances summed

void(float xTest) VR_Decap_PopTest =
{
    if(xTest == 40)
    {
        float lengths[12] = {1, 2, 3, 3.5, 5, 7, 9, 11, 13, 14.5, 15, 25};
        for(float w = 0; w < 3; ++w)
        {
            float how = (w == 0) ? QVR_DECAP_SHOTGUN : (w == 1) ? QVR_DECAP_SSG : QVR_DECAP_LIGHTNING;
            for(float sh = 0; sh < 3; ++sh)
            {
                float share = (sh == 0) ? 1 : (sh == 1) ? 0.5 : 0.2;
                string line = sprintf("poptest: table %s share %.1f:", VR_Decap_HowName(how), share);
                for(float i = 0; i < 12; ++i)
                {
                    line = strcat(line, sprintf(" %g=%.2f", lengths[i],
                        VR_Decap_PopChance(how, lengths[i] * VR_DECAP_PLAYER_LENGTH, share)));
                }
                bprint(strcat(line, "\n"));
            }
        }
        return;
    }
    entity m = VR_Decap_TestTarget(TRUE);
    if(m == world)
    {
        bprint(sprintf("poptest: %g: no live monster near\n", xTest));
        return;
    }
    VR_Decap_HeadZone(m);
    makevectors('0 1 0' * m.angles_y);
    vector at = m.origin + v_forward * xHeadForward + '0 0 1' * xHeadUp;
    vector aim = at;
    if(xTest == 44)
    {
        aim = m.origin + '0 0 4';
    }
    // Where to shoot from: level with its head, `dist` off, the way from it to you first.
    float dist = cvar("vr_decap_poptest_dist") * VR_DECAP_PLAYER_LENGTH;
    vector away = self.origin - at;
    away_z = 0;
    float yaw0 = vectoyaw(away);
    vector eye = '0 0 0';
    float clear = FALSE;
    for(float k = 0; k < 16; ++k)
    {
        makevectors('0 1 0' * (yaw0 + k * 22.5));
        eye = aim + v_forward * dist;
        traceline(eye, aim, FALSE, self); // (nothing in the way: no wall, no other monster, no training dummy)
        if(!trace_startsolid && (trace_ent == m || trace_fraction == 1))
        {
            clear = TRUE;
            break;
        }
    }
    if(!clear)
    {
        bprint(sprintf("poptest: %g: no clear line %.0f units from it\n", xTest, dist));
        return;
    }
    vector old = self.origin;
    setorigin(self, eye - self.view_ofs);
    float range = vlen(at - eye);
    if(xTest == 47 || xTest == 48)
    {
        // A thing thrown at its head (or its body: vr_decap_poptest_body) at 16 m/s, met just before it: its real hit
        // (forcegrabbable_touch for a weapon, VR_SolidProp_Impact for the box).
        vector to = at;
        if(cvar("vr_decap_poptest_body"))
        {
            to = m.origin + '0 0 4';
        }
        vector dir = normalize(to - eye);
        vector vel = dir * 16 * VRMetersToUnits();
        entity thing = world;
        m.health = 1;
        if(xTest == 47)
        {
            float wid = cvar("vr_decap_poptest_wid");
            thing = CreateThrownWeapon(self, wid, 0, WeaponInst_New(0), to - dir * 2, vectoangles(dir), vel, '0 0 0',
                FALSE, TRUE, WeaponIdToThrowDamage(wid));
        }
        else
        {
            entity oldSelf = self;
            self = spawn();
            setorigin(self, to - dir * 2);
            misc_explobox();
            thing = self;
            self = oldSelf;
            thing.takedamage = DAMAGE_NO; // (never blows up)
            setorigin(thing, to - dir * 2);
            thing.velocity = vel;
            thing.owner = self;
            thing.throwhit = QVR_THROWHIT_NEVER_HIT;
            thing.carry_thrown = time;
            thing.throwdmgspeed = 700 * VRMetersToUnits() / 120; // (as VR_Carry_Release throws it)
            SetThrownWeaponBaseDamage(thing, cvar_hget(cvarh_vr_carry_throw_damage));
        }
        float pops0 = vr_decap_pops;
        entity me = self;
        self = thing;
        other = m;
        if(xTest == 47)
        {
            forcegrabbable_touch();
        }
        else
        {
            VR_SolidProp_Impact(16);
        }
        self = me;
        bprint(sprintf("poptest: %g %s (%s, %.1f kg) thrown at the %s of %s: ", xTest, thing.classname, thing.netname,
            VR_Thrown_Mass(thing), cvar("vr_decap_poptest_body") ? "body" : "head", m.classname));
        bprint(sprintf("%s, headless %g, gibbed %g, health %g\n", (vr_decap_pops > pops0) ? "popped" : "not popped",
            m.vr_headless, m.vr_gib, m.health));
        remove(thing);
    }
    else if(xTest >= 41 && xTest <= 44)
    {
        float weapon = (xTest == 41) ? QVR_DECAP_SHOTGUN : (xTest == 43) ? QVR_DECAP_LIGHTNING : QVR_DECAP_SSG;
        m.health = 1;
        float pops = vr_decap_pops;
        float rolls = vr_decap_rolls;
        VR_Decap_TestShot(weapon, aim);
        // (sprintf takes at most 7 values: QC builtins take 8 parameters)
        bprint(sprintf("poptest: %g %s at the %s of %s from %.0f units (%.1f lengths): ", xTest, VR_Decap_HowName(weapon),
            (xTest == 44) ? "body" : "head", m.classname, range, range / VR_DECAP_PLAYER_LENGTH));
        bprint(sprintf("%s, rolled %g, headless %g, gibbed %g, health %g\n", (vr_decap_pops > pops) ? "popped" : "not popped",
            vr_decap_rolls - rolls, m.vr_headless, m.vr_gib, m.health));
    }
    else if(xTest == 45 || xTest == 46)
    {
        float wpn = (xTest == 46) ? QVR_DECAP_SSG : QVR_DECAP_SHOTGUN;
        float n = max(1, cvar("vr_decap_poptest_n"));
        for(float b = 0; b < 15; ++b)
        {
            vr_poptest_bn[b] = 0;
            vr_poptest_bpop[b] = 0;
            vr_poptest_bsum[b] = 0;
        }
        float armed = 0, popped = 0, sum = 0;
        vr_poptest_spread = TRUE;
        vector morg = m.origin, mang = m.angles;
        float mframe = m.frame;
        for(float t = 0; t < n; ++t)
        {
            // (Each blast at it as it stood: the blasts before knocked it back, turned it, played its pain.)
            setorigin(m, morg);
            m.angles = mang;
            m.frame = mframe;
            m.velocity = '0 0 0';
            m.health = 100000;
            vr_decap_ent = world;
            vr_decap_time = -1;
            VR_Decap_TestShot(wpn, at);
            float hp = (m.vr_decap_bserial == vr_sgib_blastserial) ? m.vr_decap_bn : 0;
            if(vr_decap_time != time)
            {
                continue; // (not armed: no headshot, under the Head Share)
            }
            vr_decap_ent = world; // (not killed: VR_Decap_After let it go; the chance it would have rolled kept)
            float c = vr_decap_chance;
            float won = VR_Decap_Roll(m); // (its roll: random, vr_decap_pop_roll; Quad Damage pops all)
            armed++;
            sum += c;
            popped += won;
            hp = min(14, hp);
            vr_poptest_bn[hp] += 1;
            vr_poptest_bpop[hp] += won;
            vr_poptest_bsum[hp] += c;
        }
        vr_poptest_spread = FALSE;
        m.health = 100;
        bprint(sprintf("poptest: %g %s at %s's head from %.0f units (%.1f lengths): ", xTest, VR_Decap_HowName(wpn),
            m.classname, range, range / VR_DECAP_PLAYER_LENGTH));
        bprint(sprintf("%g blasts, %g headshots, mean chance %.3f, popped %g (rate %.3f)\n", n, armed, sum / max(1, armed),
            popped, popped / max(1, armed)));
        string byp = "poptest: by pellets at the head (n chance rate):";
        for(float q = 0; q < 15; ++q)
        {
            if(vr_poptest_bn[q] > 0)
            {
                byp = strcat(byp, sprintf(" %g:%g %.2f %.2f", q, vr_poptest_bn[q], vr_poptest_bsum[q] / vr_poptest_bn[q],
                    vr_poptest_bpop[q] / vr_poptest_bn[q]));
            }
        }
        bprint(strcat(byp, "\n"));
    }
    setorigin(self, old);
};

// Blunt melee head pops' and Quad Damage's tests (vr_decap_test 49..58; Debug > Gore Tests), on the nearest live
// monster: 49 each blunt class's (fist, gun, crowbar, pommel, club, Mjolnir) blow at its head at a soft, a medium and a
// hard hit (4, 7, 11 m/s; its weapon's base damage x 0.6, 1.2, 2, x the headshot multiplier) vr_decap_poptest_n times,
// not killing it: the chance (armed once) and the rate it pops (VR_Decap_Roll n times: Quad Damage pops all). 50..55 a hard
// killing blow of each class (50 fist .. 55 Mjolnir) at health 1; 56 a nail, 57 a rocket, 58 a grenade at its head at
// health 1 (popped only with Quad Damage: impulse 255). `poptest:` lines.
float VR_DECAP_TEST_SPEED[3] = {4, 7, 11};
float VR_DECAP_TEST_STRENGTH[3] = {0.6, 1.2, 2};

// The made-up blow of blunt class `xClass` at level `xLevel` (0 soft, 1 medium, 2 hard) at `xAt`: its damage (before
// the headshot multiplier).
float(vector xAt, float xClass, float xLevel) VR_Decap_TestBluntBlow =
{
    float thing = VR_MTHING_FIST, kind = VR_MKIND_PUNCH, f = 0.7, base = cvar_hget(cvarh_vr_dmg_fist);
    if(xClass == VR_DECAP_BCLASS_GUN) { thing = VR_MTHING_GUN; kind = VR_MKIND_SLASH; base = cvar_hget(cvarh_vr_dmg_gun_bash); }
    else if(xClass == VR_DECAP_BCLASS_CROWBAR) { thing = VR_MTHING_CROWBAR; kind = VR_MKIND_SLASH; base = cvar_hget(cvarh_vr_crowbar_damage); }
    else if(xClass == VR_DECAP_BCLASS_POMMEL) { thing = VR_MTHING_SWORD; kind = VR_MKIND_POMMEL; f = -0.15; base = 20; }
    else if(xClass == VR_DECAP_BCLASS_CLUB) { thing = VR_MTHING_CLUB; kind = VR_MKIND_SLASH; base = cvar_hget(cvarh_vr_walltorch_damage); }
    else if(xClass == VR_DECAP_BCLASS_MJOLNIR) { thing = VR_MTHING_AXE; kind = VR_MKIND_SLASH; base = cvar_hget(cvarh_vr_dmg_mjolnir); }
    VR_Decap_TestBlow(xAt, thing, kind, f, 0, VR_DECAP_TEST_SPEED[xLevel]);
    if(xClass == VR_DECAP_BCLASS_MJOLNIR)
    {
        VR_Blow(cVR_MainHand).mh_wid = WID_MJOLNIR;
    }
    return base * VR_DECAP_TEST_STRENGTH[xLevel];
};

void(float xTest) VR_Decap_BluntTest =
{
    entity m = VR_Decap_TestTarget(TRUE);
    if(m == world)
    {
        bprint(sprintf("poptest: %g: no live monster near\n", xTest));
        return;
    }
    VR_Decap_HeadZone(m);
    makevectors('0 1 0' * m.angles_y);
    vector at = m.origin + v_forward * xHeadForward + '0 0 1' * xHeadUp;
    vector dir = normalize(at - (self.origin + self.view_ofs));
    float quad = VR_Decap_QuadPops(self);
    m.frame = 0; // (standing: the head zone's middle is its head's, not where a pain or run frame put it)
    if(xTest == 49)
    {
        float n = max(1, cvar("vr_decap_poptest_n"));
        float oldHealth = m.health;
        float hm = max(1, cvar_hget(cvarh_vr_headshot_mult));
        for(float cls = 0; cls < 6; ++cls)
        {
            string line = sprintf("poptest: 49 %s on %s (quad %g):", VR_Decap_BluntName(cls), m.classname, quad);
            for(float lvl = 0; lvl < 3; ++lvl)
            {
                // (The blow armed once, its chance rolled n times: VR_Decap_Roll's roll.)
                float armed = 0, popped = 0, c = 0;
                float dmg = VR_Decap_TestBluntBlow(at, cls, lvl) * hm;
                m.health = 100000;
                vr_decap_ent = world;
                vr_decap_time = -1;
                VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
                float yes = VR_Decap_Blow(m, self, dmg);
                VR_ClearHitKind();
                vr_decap_ent = world;
                c = vr_decap_chance;
                for(float t = 0; yes && t < n; ++t)
                {
                    vr_decap_chance = c;
                    armed++;
                    popped += VR_Decap_Roll(m);
                }
                line = strcat(line, sprintf(" %s %.0fm/s %.0fdmg chance %.4f armed %g popped %g rate %.4f;",
                    (lvl == 0) ? "soft" : (lvl == 1) ? "medium" : "hard", VR_DECAP_TEST_SPEED[lvl], dmg, c, armed, popped,
                    popped / max(1, armed)));
            }
            bprint(strcat(line, "\n"));
        }
        m.health = oldHealth;
        return;
    }
    float player = self.takedamage;
    float pops = vr_decap_pops;
    float rolls = vr_decap_rolls;
    string what;
    m.health = (m.classname == "monster_zombie") ? 60 : 1;
    if(xTest >= 50 && xTest <= 55)
    {
        float k = xTest - 50;
        float d = VR_Decap_TestBluntBlow(at, k, 2);
        what = sprintf("a hard %s blow (%.0f damage, chance %.4f)", VR_Decap_BluntName(k), d,
            VR_Decap_BluntChance(k, VR_DECAP_TEST_SPEED[2], d * max(1, cvar_hget(cvarh_vr_headshot_mult))));
        if(m.classname == "monster_zombie") { d = max(d, 30); }
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
        T_Damage_VRMelee(m, self, self, d);
        VR_ClearHitKind();
    }
    else
    {
        entity p = spawn();
        p.owner = self;
        p.movetype = MOVETYPE_FLYMISSILE;
        p.solid = SOLID_BBOX;
        p.angles = vectoangles(dir);
        p.velocity = dir * ((xTest == 58) ? 600 : 1000);
        setorigin(p, at - dir * 6);
        entity old = self;
        self.takedamage = DAMAGE_NO; // (the blast spares you)
        self = p;
        other = m;
        if(xTest == 56) { what = "a nail"; p.classname = "spike"; spike_touch(); }
        else if(xTest == 57) { what = "a rocket"; p.classname = "missile"; T_MissileTouch(); }
        else { what = "a grenade"; p.classname = "grenade"; GrenadeTouch(); }
        self = old;
        self.takedamage = player;
    }
    bprint(sprintf("poptest: %g %s at the head of %s (quad %g): %s, rolled %g", xTest, what, m.classname, quad,
        (vr_decap_pops > pops) ? "popped" : "not popped", vr_decap_rolls - rolls));
    bprint(sprintf(", headless %g, gibbed %g, health %g\n", m.vr_headless, m.vr_gib, m.health));
};

void(float xTest) VR_Decap_Test =
{
    if(xTest >= 49 && xTest <= 58)
    {
        VR_Decap_BluntTest(xTest);
        return;
    }
    if(xTest >= 40 && xTest <= 48)
    {
        VR_Decap_PopTest(xTest);
        return;
    }
    if(xTest == 32)
    {
        VR_Gore_Print(sprintf("gremlingrabtest: held=%g\n", ragdollheld(self, cVR_MainHand)));
        return;
    }
    if(xTest == 30)
    {
        entity small = spawn(); small.vr_rigid = 1; small.classname = "vr_smallgib"; small.vr_sgib = time;
        float rejected = !VR_Button_Thrown(small);
        small.classname = "gib"; small.vr_sgib = 0;
        VR_Gore_Print(sprintf("buttongibtest: small_rejected=%g large_allowed=%g\n", rejected, VR_Button_Thrown(small)));
        remove(small);
        return;
    }
    if(xTest == 31)
    {
        entity g = find(world, classname, "monster_gremlin");
        if(g != world && g.health <= 0)
        {
            entity player = self; self = g;
            self.solid = SOLID_NOT;
            gremlin_flip8();
            VR_Corpse_Watch(g);
            self = player;
        }
        return;
    }
    float heads = VR_Decap_TestHeads();
    float before = vr_decap_count;
    float pops = vr_decap_pops;
    entity m = VR_Decap_TestTarget((xTest != 5 && xTest != 8 && xTest != 10 && (xTest < 20 || xTest >= 24)));
    if(xTest == 33 && m != world)
    {
        LightningDamage(self.origin + self.view_ofs, m.origin + 0.5 * (m.mins + m.maxs), self, 1);
        VR_Gore_Print(sprintf("bodyshocktest: target=%s health=%g corpse=%g\n", m.classname, m.health, m.vr_corpse));
        return;
    }
    if(xTest == 8)
    {
        // (A headless corpse first: gibbed, it throws no head.)
        for(entity hl = nextent(world); hl; hl = nextent(hl))
        {
            if(hl.vr_headless && !hl.vr_gib && hl.health <= 0 && vlen(hl.origin - self.origin) < 400)
            {
                m = hl;
            }
        }
    }
    if(m == world)
    {
        VR_Gore_Print(sprintf("decaptest: %g: no %s monster near\n", xTest, (xTest == 5 || xTest == 8 || xTest == 10) ? "dead" : "live"));
        return;
    }
    // Its head: the head zone's middle (a ragdoll: its head part's).
    vector at;
    if(isragdoll(m))
    {
        at = ragdollcut(m, 6);
    }
    else
    {
        VR_Decap_HeadZone(m);
        makevectors('0 1 0' * m.angles_y);
        at = m.origin + v_forward * xHeadForward + '0 0 1' * xHeadUp;
    }
    if(xTest == 19)
    {
        VR_Decap_Sweep(m);
        return;
    }
    if(xTest == 11)
    {
        m.health = 1; // (no blow: the next one kills it, a thrown axe's)
        VR_Gore_Print(sprintf("decaptest: 11 %s at health 1\n", m.classname));
        return;
    }
    float dmg = 30;
    string what = "";
    if(xTest == 8)
    {
        what = "gibbed";
        T_Damage(m, self, self, 1000);
    }
    else if(xTest == 5)
    {
        what = "a slash at a corpse's head";
        VR_Decap_TestBlow(at, VR_MTHING_SWORD, VR_MKIND_SLASH, 0.7, 0, 8);
        if(!VR_Decap_CorpseBlow(m, cVR_MainHand, at))
        {
            VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
            T_Damage(m, self, self, dmg);
            VR_ClearHitKind();
        }
    }
    else if(xTest == 6 || xTest == 10)
    {
        what = (xTest == 6) ? "the chainsaw" : "the chainsaw at a corpse's head";
        if(xTest == 6)
        {
            m.health = 1;
        }
        VR_Decap_TestBlow(at, VR_MTHING_SAW, VR_MKIND_SLASH, 0.8, 0, 0.5);
        float saw = VR_Decap_Saw(m, cVR_MainHand, at);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        self.damage_weapon = IT_AXE;
        if(saw != 2)
        {
            T_DamageImpl(m, self, self, 8, 1);
        }
        VR_ClearHitKind();
        if(saw == 1)
        {
            VR_Decap_After(m);
        }
    }
    else if(xTest >= 20 && xTest <= 22)
    {
        what = "a headshot on a corpse";
        // Shoot from the exposed side of its head, rather than through the
        // body or an arm lying between the player's eyes and the head.
        vector playerOrigin = self.origin;
        vector away = at - m.origin;
        away_z = 0;
        setorigin(self, at + normalize(away) * 64 + '0 0 24' - self.view_ofs);
        VR_Decap_TestShot(xTest == 22 ? QVR_DECAP_LIGHTNING : xTest == 21 ? QVR_DECAP_SSG : QVR_DECAP_SHOTGUN, at);
        setorigin(self, playerOrigin);
    }
    else if(xTest >= 23 && xTest <= 27)
    {
        what = "a blunt head blow";
        if(xTest == 24 || xTest == 26 || xTest == 27) { m.health = 1; }
        if(xTest == 25) { m.health = 500; }
        if(xTest == 26) { at = m.origin; what = "a blunt body blow"; }
        VR_Decap_TestBlow(at, VR_MTHING_FIST, VR_MKIND_POMMEL, 0.7, 0, 8);
        if(xTest == 27)
        {
            entity b = VR_Blow(cVR_MainHand);
            b.mh_thing = VR_MTHING_AXE;
            b.mh_kind = VR_MKIND_SLASH;
            b.mh_wid = WID_MJOLNIR;
            what = "Mjolnir's head blow";
        }
        if(xTest == 23)
        {
            if(!VR_Decap_CorpseBlow(m, cVR_MainHand, at)) { T_Damage(m, self, self, dmg); }
        }
        else
        {
            VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
            T_Damage_VRMelee(m, self, self, dmg);
            VR_ClearHitKind();
        }
    }
    else if(xTest >= 12 && xTest <= 18)
    {
        // Head pops: a shot at its head (or its body) from your eye.
        float weapon = QVR_DECAP_SHOTGUN;
        vector aim = at;
        if(xTest == 12) { what = "a shotgun headshot"; m.health = VR_Decap_TestBlastDamage(weapon) - 1; }
        else if(xTest == 13) { weapon = QVR_DECAP_SSG; what = "a super shotgun headshot"; m.health = VR_Decap_TestBlastDamage(weapon) - 1; }
        else if(xTest == 14) { weapon = QVR_DECAP_SSG; what = "a super shotgun headshot, overkill"; m.health = 1; }
        else if(xTest == 15) { weapon = QVR_DECAP_LIGHTNING; what = "a lightning bolt at the head"; m.health = 1; }
        else if(xTest == 16) { what = "a shotgun body shot"; m.health = 1; aim = m.origin + '0 0 4'; }
        else if(xTest == 17) { what = "a shotgun headshot not killing"; m.health = max(m.health, 500); }
        else { weapon = QVR_DECAP_LIGHTNING; what = "a lightning bolt at the body"; m.health = 1; aim = m.origin + '0 0 4'; }
        if(m.classname == "monster_zombie")
        {
            m.health = 60; // (a zombie: its own health, as it always is)
        }
        VR_Gore_Print(sprintf("decaptest: %g at health %g\n", xTest, m.health));
        VR_Decap_TestShot(weapon, aim);
    }
    else
    {
        float thing = (xTest == 9) ? VR_MTHING_AXE : VR_MTHING_SWORD;
        float kind = VR_MKIND_SLASH;
        float f = 0.7;
        float along = 0;
        float speed = 8;
        if(xTest == 2) { what = "a stab"; along = 1; }
        else if(xTest == 3) { what = "a pommel strike"; f = -0.15; kind = VR_MKIND_POMMEL; }
        else if(xTest == 4) { what = "a slow slash"; speed = max(0.2, cvar_hget(cvarh_vr_decap_speed) * 0.5); }
        else if(xTest == 7) { what = "a slash at full health"; dmg = 10; }
        else { what = (xTest == 9) ? "an axe's slash" : "a sword's slash"; f = (xTest == 9) ? 0.85 : 0.7; }
        if(xTest != 7)
        {
            m.health = 1;
        }
        VR_Decap_TestBlow(at, thing, kind, f, along, speed);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        T_Damage_VRMelee(m, self, self, dmg);
        VR_ClearHitKind();
    }
    float headsNow = VR_Decap_TestHeads(); // (not in the call's arguments: a call there overwrites the ones before it)
    float rag = isragdoll(m);
    // (sprintf takes at most 7 values: QC builtins take 8 parameters)
    VR_Gore_Print(sprintf("decaptest: %g %s on %s: %s, health %g, headless %g", xTest, what, m.classname,
        (vr_decap_count > before) ? "beheaded" : "not beheaded", m.health, m.vr_headless));
    VR_Gore_Print(sprintf(", ragdoll %g, heads %g -> %g, popped %g, gibbed %g\n", rag, heads, headsNow, vr_decap_pops - pops,
        m.vr_gib));
    if(vr_decap_count > before && vr_decap_lasthead != world)
    {
        entity h = vr_decap_lasthead;
        vector neck = ragdollcut(m, 4);
        VR_Gore_Print(sprintf("decaptest: head %s at %v (the neck %v), %.0f u/s %v", h.model, h.origin, neck, vlen(h.velocity),
            h.velocity));
        VR_Gore_Print(sprintf(", spin %.1f rad/s %v (gib %g, rigid %g)\n", vlen(h.vr_spin), h.vr_spin, h.vr_gib, h.vr_rigid));
    }
};

// Debug > Show Hit Zones (vr_debug_hitzones; drawn by the engine: debughitzone, vr_hitmodel.cpp zonesDraw): each
// monster's zones on its model standing (the engine maps these onto its animated surface for precise hits). 1
// positional damage's (PositionalRegion): the head sphere (red; VR_HEAD_MARGIN in with precise hits), the body
// (green: within 0.35 of its box's width of its middle, above its origin), the legs (blue: below it), the extremities
// (yellow: out to its sides beyond that, drawn to 0.75 of its width; open-ended). 2 melee beheading's
// (VR_Decap_MeleeZone, magenta: vr_decap_head_size, vr_decap_neck) and the head zone that shots and thrown axes use
// (red), painted on the animated surface as well (capsuleZone; the melee's test maps the point struck to the standing
// pose, VR_Decap_OnMeleeHead). 3 both (the magenta outlined over positional damage's colors).
void() VR_Decap_DebugFrame =
{
    float mode = cvar_hget(cvarh_vr_debug_hitzones);
    if(!mode)
    {
        return;
    }
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.vr_gib)
        {
            continue;
        }
        float precise = hitmodel_target(e);
        if(mode != 2 && PositionalHead(e))
        {
            float w = e.maxs_x - e.mins_x;
            float side = w * 0.35;
            float out = w * 0.75;
            vector lo = '0 0 0';
            vector hi = '0 0 0';
            lo_x = e.mins_x; hi_x = e.maxs_x;
            lo_y = -side; hi_y = side;
            lo_z = 0; hi_z = e.maxs_z;
            debughitzone(e, 0, lo, hi, 0);
            lo_z = e.mins_z; hi_z = 0;
            debughitzone(e, 3, lo, hi, 0);
            lo_y = side; hi_y = out; hi_z = e.maxs_z;
            debughitzone(e, 2, lo, hi, 0);
            lo_y = -out; hi_y = -side;
            debughitzone(e, 2, lo, hi, 0);
            vector c = '1 0 0' * xHeadForward + '0 0 1' * xHeadUp;
            debughitzone(e, 1, c, c, xHeadRadius + (precise ? VR_HEAD_MARGIN : 0));
        }
        if(mode >= 2 && e.health > 0 && VR_Decap_Judged(e) && VR_Decap_HeadZone(e))
        {
            vector top = '1 0 0' * xHeadForward + '0 0 1' * xHeadUp;
            if(mode == 2)
            {
                debughitzone(e, 1, top, top, xHeadRadius + (precise ? VR_HEAD_MARGIN : 0));
            }
            VR_Decap_MeleeZone(e);
            debughitzone(e, 4, top - '0 0 1' * (vr_decap_zone_top_z - vr_decap_zone_bottom_z), top, xHeadRadius);
        }
    }
};

// Every server frame (StartFrame): Show Hit Zones (VR_Decap_DebugFrame), a test asked for (vr_decap_test).
void() VR_Decap_TestFrame =
{
    VR_Decap_DebugFrame();
    float ask = cvar("vr_decap_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_decap_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    if(cvar("vr_gore_test_crowd") > 0)
    {
        VR_Gore_TestCrowd(ask, TRUE);
        self = oself;
        return;
    }
    VR_Decap_Test(ask);
    self = oself;
};
