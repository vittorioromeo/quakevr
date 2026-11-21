// dw_common.qc -- Dwell support (DWELL.md P2a): what Dwell's own entities share. Dwell's monsters run on our monster
// code (walk/flymonster_start, our ai_* and T_Damage: stealth senses, smarter AI hooks, infighting, parries, VR hit
// zones), their animations as table rows (DW_Seq) instead of one function a frame; their numbers are Dwell's.
//
// Copyright (C) the Dwell team (2023: Fairweather, Khreathor, bmFbr, Shamblernaut, VoidForce and the others), whose
// entities these reimplement on Copper (Lunaran, GPL-2) and Quake VR; hand-written from Dwell's behaviour, not its code.
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
// License version 2 or later. Dwell's models and sounds are read from the player's install, never shipped.

const float DW_AI_NONE = 0;
const float DW_AI_STAND = 1;
const float DW_AI_WALK = 2;
const float DW_AI_RUN = 3;
const float DW_AI_CHARGE = 4;
const float DW_AI_FACE = 5;

const float DW_MON_KAMIKAZE = 1;
const float DW_MON_BRUTE = 2;
const float DW_MON_CONSTRUCT = 3;
const float DW_MON_D1BOSS = 4;
const float DW_MON_D2BOSS = 5;

// Dwell's data is in play (its folder on the search path: vr_dwell_status 1 mounts it in every campaign). The engine
// spawns a dw_ entity only then (vr_rules.cpp's DWELL row); this guards the toolgun's and a save's.
float() DW_Data =
{
    return cvar_hget(cvarh_vr_dwell_status) == 1;
};

// A spawn function's start: FALSE (and self removed) where it must not spawn.
float() DW_SpawnOk =
{
    if(!DW_Data() || VR_SF_SpawnInhibited())
    {
        remove(self);
        return FALSE;
    }
    return TRUE;
};

// One step of the running sequence: the next frame (past the last, its follower or the first again), its AI, its event.
void() DW_Tick =
{
    float f = self.frame + 1;
    if(f > self.dw_f1)
    {
        if(self.dw_after)
        {
            self.dw_after();
            return;
        }
        f = self.dw_f0;
    }
    if(f < self.dw_f0) { f = self.dw_f0; }
    self.frame = f;
    self.think = DW_Tick;
    self.nextthink = time + 0.1;
    float id = self.dw_seqid;
    if(self.dw_ai == DW_AI_STAND)       { ai_stand(); }
    else if(self.dw_ai == DW_AI_WALK)   { ai_walk(self.dw_dist); }
    else if(self.dw_ai == DW_AI_RUN)    { ai_run(self.dw_dist); }
    else if(self.dw_ai == DW_AI_CHARGE) { ai_charge(self.dw_dist); }
    else if(self.dw_ai == DW_AI_FACE)   { ai_face(); }
    if(self.dw_seqid == id && self.dw_ev) // (not when its AI started another sequence: an attack, a pain)
    {
        self.dw_ev();
    }
};

// Starts a sequence: frames f0..f1, AI `kind` moving `dist` a frame, `ev` each frame (null: none), `after` past the last
// (null: it loops). Its first frame at once.
void(float f0, float f1, float kind, float dist, void() ev, void() after) DW_Seq =
{
    self.dw_seqid = self.dw_seqid + 1;
    self.dw_f0 = f0;
    self.dw_f1 = f1;
    self.dw_ai = kind;
    self.dw_dist = dist;
    self.dw_ev = ev;
    self.dw_after = after;
    self.frame = f0 - 1;
    DW_Tick();
};

// A death sequence's end: it lies still.
void() DW_Lie =
{
    self.nextthink = -1;
};

// Back to running (a sequence's follower).
void() DW_Run =
{
    self.th_run();
};

// Fires the entities named by `t` (a boss's target2..4: Copper's keys) as SUB_UseTargets does for target.
void(string t) DW_UseNamed =
{
    if(t == "") { return; }
    string keep = self.target;
    string kill = self.killtarget;
    float keepDelay = self.delay;
    self.target = t;
    self.killtarget = string_null;
    self.delay = 0;
    activator = self.enemy.classname == "player" ? self.enemy : activator;
    SUB_UseTargets();
    self.target = keep;
    self.killtarget = kill;
    self.delay = keepDelay;
};

// The explosion's sight and sound at `org`.
float dw_boom_count; // (DW_Boom: explosions drawn, for tests)
void(vector org) DW_Boom =
{
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    dw_boom_count = dw_boom_count + 1;
    WriteCoord(MSG_BROADCAST, org_x);
    WriteCoord(MSG_BROADCAST, org_y);
    WriteCoord(MSG_BROADCAST, org_z);
};

// A missile's hit: `dmg` on what it struck, `cnt` around (our T_RadiusDamage), the explosion.
void() DW_MissileTouch =
{
    if(other == self.owner || (other.owner == self.owner && other.classname == self.classname)) { return; }
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    if(other.takedamage && self.dmg > 0) { T_Damage(other, self, self.owner, self.dmg); }
    if(self.cnt > 0) { T_RadiusDamage(self, self.owner, self.cnt, other); }
    sound(self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
    DW_Boom(self.origin);
    remove(self);
};

// Its fuse run out: it bursts where it is.
void() DW_MissileBurst =
{
    other = world;
    DW_MissileTouch();
};

// A monster's missile from self: model `mdl`, `direct` on a hit, `splash` around; it bursts after `life` s.
entity(vector org, vector vel, string mdl, float direct, float splash, float life) DW_Missile =
{
    entity m = spawn();
    m.owner = self;
    m.enemy = self.enemy;
    m.classname = "dw_missile";
    m.movetype = MOVETYPE_FLYMISSILE;
    m.solid = SOLID_BBOX;
    setmodel(m, mdl);
    setsize(m, '0 0 0', '0 0 0');
    setorigin(m, org);
    m.velocity = vel;
    m.angles = vectoangles(vel);
    m.dmg = direct;
    m.cnt = splash;
    m.touch = DW_MissileTouch;
    m.think = DW_MissileBurst;
    m.nextthink = time + life;
    m.attack_finished = time + life;
    VR_AI_Tag(m);
    return m;
};

// A homing missile's steering (Dwell's bosses'): toward its enemy at `speed` (its `lip`), turning harder the further it
// points away, until its tracking time (`pain_finished`) ends; then straight on until its fuse (`attack_finished`).
void() DW_MissileHome =
{
    if(time >= self.attack_finished)
    {
        DW_MissileBurst();
        return;
    }
    self.think = DW_MissileHome;
    self.nextthink = time + 0.1;
    if(time > self.pain_finished || !self.enemy || self.enemy.health <= 0) { return; }
    vector want = normalize(VR_AI_Origin(self.enemy) + '0 0 10' - self.origin);
    vector have = normalize(self.velocity);
    float turn = 0.25 + 0.35 * (1 - have * want);
    self.velocity = normalize(have + want * turn) * self.lip;
    self.angles = vectoangles(self.velocity);
};

// Dwell's kamikaze blast: full `dmg` within 32 units, a sine falloff to nothing at `radius` (Dwell's own curve).
void(entity inflictor, entity attacker, float dmg, float radius) DW_Blast =
{
    entity e = findradius(inflictor.origin, radius);
    while(e)
    {
        if(e.takedamage && e != inflictor && CanDamage(e, inflictor))
        {
            float d = vlen(inflictor.origin - (e.origin + (e.mins + e.maxs) * 0.5));
            float f = 1;
            if(d > 32) { f = (sin(0.02 * d + 0.93) * 50 + 50) / 100; } // (radians: 1 at 32 units, 0 at 190)
            float take = rint(f * dmg);
            if(take > 0) { T_Damage(e, inflictor, attacker, take); }
        }
        e = e.chain;
    }
};

// CheckAnyAttack's per-class choice for Dwell's monsters (their vr_th_checkattack), Dwell's rules: Dwell's Brute
// punches in melee range, lobs grenades at short and middle range.
float() DW_BruteCheckAttack =
{
    if(enemy_range == RANGE_MELEE && CanDamage(self.enemy, self))
    {
        self.attack_state = AS_MELEE;
        return TRUE;
    }
    if(time < self.attack_finished || !enemy_vis) { return FALSE; }
    if(enemy_range == RANGE_FAR) { return FALSE; }
    if(!VR_AI_ShotReady(self.origin + '0 0 40')) { return FALSE; }
    self.attack_state = AS_MISSILE;
    return TRUE;
};

// The Construct fires whenever it sees its enemy within middle range.
float() DW_ConstructCheckAttack =
{
    if(time < self.attack_finished || !enemy_vis) { return FALSE; }
    if(enemy_range == RANGE_FAR)
    {
        self.attack_state = AS_STRAIGHT;
        return FALSE;
    }
    if(!VR_AI_ShotReady(self.origin + '0 0 35')) { return FALSE; }
    self.attack_state = AS_MISSILE;
    return TRUE;
};

// T_DamageImpl: Dwell's Berserk: melee blows by its holder eleven times as hard (Dwell's axe: 264 for 24), the damage
// it takes halved (rounded up).
float(entity targ, entity attacker, float damage, float isVRMelee) DW_DamageScale =
{
    if(isVRMelee && attacker.dw_berserk_finished > time) { damage = damage * 11; }
    if(targ.dw_berserk_finished > time) { damage = ceil(damage * 0.5); }
    return damage;
};

// Self's VR registration (DWELL.md P2b; S7: the monster row AD's monsters use, vr_ad_mondefs.qc, VR_ADM_OURS: our own
// monster code runs it): its head zone (PositionalHead: head shots, melee head hits, beheading), its head gib, its
// corpse, its knockdown (a death played backwards; the rig: vr_ragdoll.cpp's mon_brute seeds), melee (parries) and
// armour. Head zones: Misc/quakevr/ragdoll/headfit.py on the stand frames (RIG_MDL=<Dwell>/progs/<model>.mdl).
void() DW_MonsterRow =
{
    float m = self.dw_mon;
    // The Kamikaze (e1/mon_kami.mdl: no death frames, it bursts): headfit.py mon_kami 16 -100 8 (-0.3 0.4 18.7, 5.9).
    // No rig, no corpse, never beheaded (it bursts first).
    if(m == DW_MON_KAMIKAZE)
    {
        VR_AD_Mon(VR_ADM_OURS | VR_ADM_NOHEAD, 0, 19, 6, 0);
    }
    // The Brute (e2/mon_brute.mdl: deaths 199-211, 221-233): headfit.py mon_brute 53 -7 9 (4.9 -0.2 61.4, 10.2; its
    // rig's head 6.1 -0.3 60.9). Its head gib Dwell's h_brute; tuned as the shambler (its size: corpse, knockdown
    // chance; the engine's ragdoll settings by vr_ad_monsters.cpp's alias). Its punch is parried.
    else if(m == DW_MON_BRUTE)
    {
        VR_AD_MonKd(VR_ADM_OURS | VR_ADM_MELEE, 5, 61, 10, cvarh_vr_corpse_health_shambler,
                    cvarh_vr_knockdown_chance_shambler, 211, 199, 233, 221);
        self.ad_headmdl = "progs/e2/h_brute.mdl";
    }
    // The Construct (e2/mon_excavator.mdl): a machine of crystal and stone, no head (decided: no zone; it breaks into
    // its four pieces at death, no rig); blunt blows sound on armour.
    else if(m == DW_MON_CONSTRUCT)
    {
        VR_AD_Mon(VR_ADM_OURS | VR_ADM_ARMOUR, 0, 0, 0, 0);
    }
    // The Awoken (mon_d1boss.mdl, 248 units tall; its death sinks away, no rig): headfit.py mon_d1boss 110 -100 20 (0
    // -0.5 125, 15.8); a boss: never beheaded, no knockdown, no corpse (as our bosses).
    else if(m == DW_MON_D1BOSS)
    {
        VR_AD_Mon(VR_ADM_OURS | VR_ADM_NOHEAD, 0, 125, 16, 0);
    }
};
