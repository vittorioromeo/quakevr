// dw_monsters.qc -- Dwell support (DWELL.md P2a): Dwell's Kamikaze, Brute and Construct as Quake VR monsters (our
// monster start, AI, damage; dw_common.qc's sequences), with Dwell's numbers, models and sounds.
//
// Copyright (C) the Dwell team (2023: Fairweather, Khreathor, bmFbr, Shamblernaut, VoidForce and the others), whose
// monsters these reimplement on Copper (Lunaran, GPL-2) and Quake VR; hand-written from Dwell's behaviour, not its code.
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
// License version 2 or later. Dwell's models and sounds are read from the player's install, never shipped.

// ---------------------------------------------------------------------------------------------------------------------
// The Kamikaze (progs/e1/mon_kami.mdl): 40 health; it runs screaming at its enemy and bursts on contact (60 within 32
// units, nothing at 190), or when killed. It gives up a target it has not seen for 60 s.

void() dw_kami_stand = { DW_Seq(24, 48, DW_AI_STAND, 0, __NULL__, __NULL__); };

void() dw_kami_explode =
{
    sound(self, CHAN_VOICE, "zombie/z_gib.wav", 1, ATTN_NORM);
    ThrowGib("progs/gib3.mdl", -60);
    ThrowGib("progs/gib3.mdl", -60);
    ThrowGib("progs/gib1.mdl", -60);
    DW_Blast(self, self, 60, 190);
    sound(self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
    BecomeExplosion();
};

void() dw_kami_die =
{
    if(self.think == dw_kami_explode) { return; }
    self.solid = SOLID_NOT;
    self.takedamage = DAMAGE_NO;
    self.think = dw_kami_explode;
    self.nextthink = time + 0.05;
};

// Each running frame: at its enemy (bursting when touching it or within melee range), screaming every few frames, and
// back to standing after 60 s with no enemy in sight.
void() dw_kami_runev =
{
    if(!self.enemy) { return; }
    if(EntitiesTouching(self, self.enemy) || range(self.enemy) == RANGE_MELEE)
    {
        Killed(self, self.enemy);
        return;
    }
    if(self.cnt <= 0)
    {
        sound(self, CHAN_VOICE, "kamikaze/kami_scream.wav", 1, ATTN_NORM);
        self.cnt = 6;
    }
    else { self.cnt = self.cnt - 1; }
    if(!self.rad_time) { self.rad_time = time + 60; }
    if(time >= self.rad_time && !visible(self.enemy))
    {
        self.enemy = world;
        self.rad_time = 0;
        self.th_stand();
    }
};

void() dw_kami_run = { DW_Seq(0, 10, DW_AI_RUN, 28, dw_kami_runev, __NULL__); };

void(entity attacker, float damage) dw_kami_pain =
{
    self.rad_time = time + 60;
    if(self.pain_finished > time) { return; }
    self.pain_finished = time + 1.5;
    sound(self, CHAN_VOICE, "kamikaze/kami_pain.wav", 1, ATTN_NORM);
    float r = random();
    if(r < 0.34)      { DW_Seq(49, 57, DW_AI_NONE, 0, __NULL__, DW_Run); }
    else if(r < 0.67) { DW_Seq(59, 67, DW_AI_NONE, 0, __NULL__, DW_Run); }
    else              { DW_Seq(69, 77, DW_AI_NONE, 0, __NULL__, DW_Run); }
};

void() dw_kami_walk = { DW_Seq(11, 23, DW_AI_WALK, 3, __NULL__, __NULL__); };

/*dw:QUAKED monster_kamikaze (1 0 0) (-16 -16 -24) (16 16 24) AMBUSH
Dwell's Kamikaze: 40 health; runs screaming at its enemy and bursts on contact (60 near, nothing at 190 units).
*/
void() dw_monster_kamikaze =
{
    if(!DW_SpawnOk()) { return; }
    if(deathmatch) { remove(self); return; }
    precache_model("progs/e1/mon_kami.mdl");
    precache_sound("kamikaze/kami_idle.wav");
    precache_sound("kamikaze/kami_scream.wav");
    precache_sound("kamikaze/kami_pain.wav");
    precache_sound("zombie/z_gib.wav");
    self.classname = "monster_kamikaze";
    self.dw_mon = DW_MON_KAMIKAZE;
    self.health = 40;
    self.th_stand = dw_kami_stand;
    self.th_walk = dw_kami_walk;
    self.th_run = dw_kami_run;
    self.th_pain = dw_kami_pain;
    self.th_die = dw_kami_die;
    monster_add_to_counter();
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, "progs/e1/mon_kami.mdl");
    setsize(self, '-16 -16 -24', '16 16 24');
    walkmonster_start();
};

// ---------------------------------------------------------------------------------------------------------------------
// The Brute (progs/e2/mon_brute.mdl, its head h_brute.mdl): 600 health; it punches (60, throwing its enemy up) and lobs
// grenades in volleys of three from either hand (Dwell's 632 speed, 40 splash: the ogre's grenade). It drops 3 rockets.

void() dw_brute_standev =
{
    if(random() < 0.02) { sound(self, CHAN_VOICE, random() < 0.5 ? "brute/idle1.wav" : "brute/idle2.wav", 1, ATTN_IDLE); }
};
void() dw_brute_stand = { DW_Seq(0, 20, DW_AI_STAND, 0, dw_brute_standev, __NULL__); };
void() dw_brute_walk = { DW_Seq(76, 98, DW_AI_WALK, 3, __NULL__, __NULL__); };
void() dw_brute_run = { DW_Seq(99, 108, DW_AI_RUN, 9, __NULL__, __NULL__); };

void() dw_brute_punch =
{
    if(!self.enemy || vlen(self.enemy.origin - self.origin) > 100 || !CanDamage(self.enemy, self)) { return; }
    if(self.enemy.flags & FL_ONGROUND)
    {
        self.enemy.flags = self.enemy.flags - FL_ONGROUND;
        self.enemy.velocity_z = self.enemy.velocity_z + 270;
    }
    T_Damage(self.enemy, self, self, 60);
    sound(self, CHAN_WEAPON, "shambler/smack.wav", 1, ATTN_NORM);
    makevectors(self.angles);
    SpawnMeatSpray(self.origin + v_forward * 16, crandom() * 100 * v_right);
};

// One grenade: from the hand `hand` (1 left, 2 right, 3 either), sprayed sideways a little after the first.
void(float hand, float spray) dw_brute_grenade =
{
    if(!self.enemy) { return; }
    makevectors(self.angles);
    vector org = self.origin + v_forward * 10 + v_up * 40;
    float side = spray ? -5 + random() * 30 : 0;
    if(hand == 1) { org = org - v_right * 12; }
    else if(hand == 2) { org = org + v_right * 38; side = -side; }
    else if(random() < 0.5) { side = -side; }
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound(self, CHAN_WEAPON, "brute/shoot.wav", 1, ATTN_NORM);
    entity g = spawn();
    g.owner = self;
    VR_AI_Tag(g);
    g.movetype = MOVETYPE_BOUNCE;
    g.solid = SOLID_BBOX;
    g.classname = "ogre_grenade";
    setmodel(g, "progs/grenade.mdl");
    setsize(g, '0 0 0', '0 0 0');
    setorigin(g, org);
    OgreGrenadeAim(g); // (our ogre's lob, shared: Z-aware toward its enemy)
    g.velocity = normalize(g.velocity) * 632 + v_right * (side * 11);
    g.avelocity = '300 300 300';
    g.angles = vectoangles(g.velocity);
    g.touch = OgreGrenadeTouch;
    g.think = OgreGrenadeExplode;
    g.nextthink = time + VR_Grenade_Fuse();
    VR_Grenade_Make(g, OgreGrenadeExplode, g.nextthink - time, FALSE);
};

void() dw_brute_grenaev =
{
    if(self.frame == 120) { dw_brute_grenade(1, 0); }
    else if(self.frame == 121 || self.frame == 122) { dw_brute_grenade(1, 1); }
    else if(self.frame == 131) { dw_brute_grenade(2, 0); }
    else if(self.frame == 132 || self.frame == 133) { dw_brute_grenade(2, 1); }
    else if(self.frame == 140) { self.attack_finished = time + 1 + 2 * random(); }
};
void() dw_brute_grenbev =
{
    if(self.frame == 151 || self.frame == 161) { dw_brute_grenade(3, 0); }
    else if(self.frame == 152 || self.frame == 153 || self.frame == 162 || self.frame == 163) { dw_brute_grenade(3, 1); }
    else if(self.frame == 175) { self.attack_finished = time + 1 + 2 * random(); }
};
void() dw_brute_missile =
{
    if(random() < 0.5) { DW_Seq(112, 140, DW_AI_FACE, 0, dw_brute_grenaev, DW_Run); }
    else               { DW_Seq(145, 175, DW_AI_FACE, 0, dw_brute_grenbev, DW_Run); }
};

void() dw_brute_meleeev =
{
    if(self.frame == 178 || self.frame == 190) { sound(self, CHAN_WEAPON, "brute/meleeswing.wav", 1, ATTN_NORM); }
    else if(self.frame == 179 || self.frame == 191) { dw_brute_punch(); }
    else if(self.frame == 185 || self.frame == 198) { self.attack_finished = time + 1 + 2 * random(); }
};
void() dw_brute_melee =
{
    self.cnt = !self.cnt; // (the two swings in turn)
    if(self.cnt) { DW_Seq(176, 185, DW_AI_CHARGE, 6, dw_brute_meleeev, DW_Run); }
    else         { DW_Seq(186, 198, DW_AI_CHARGE, 4, dw_brute_meleeev, DW_Run); }
};

void(entity attacker, float damage) dw_brute_pain =
{
    if(self.health <= 0 || self.pain_finished > time || random() * 700 > damage) { return; }
    self.pain_finished = time + 1;
    sound(self, CHAN_VOICE, random() < 0.5 ? "brute/injure1.wav" : "brute/injure2.wav", 1, ATTN_NORM);
    DW_Seq(212, 218, DW_AI_NONE, 0, __NULL__, DW_Run);
};

void() dw_brute_dieev =
{
    if(self.frame == 199 || self.frame == 221)
    {
        self.solid = SOLID_NOT;
        self.ammo_rockets = 3;
        DropBackpack();
    }
};
void() dw_brute_die =
{
    sound(self, CHAN_VOICE, random() < 0.5 ? "brute/death1.wav" : "brute/death2.wav", 1, ATTN_NORM);
    if(self.health < -150)
    {
        sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
        ThrowGib("progs/gib1.mdl", self.health);
        ThrowGib("progs/gib2.mdl", self.health);
        ThrowGib("progs/gib3.mdl", self.health);
        ThrowHead("progs/e2/h_brute.mdl", self.health);
        return;
    }
    if(random() < 0.5) { DW_Seq(199, 211, DW_AI_NONE, 0, dw_brute_dieev, DW_Lie); }
    else               { DW_Seq(221, 233, DW_AI_NONE, 0, dw_brute_dieev, DW_Lie); }
};

/*dw:QUAKED monster_brute (1 0 0) (-32 -32 -24) (32 32 64) AMBUSH
Dwell's Brute: 600 health; punches (60, throwing up) and lobs grenade volleys; drops 3 rockets.
*/
void() dw_monster_brute =
{
    if(!DW_SpawnOk()) { return; }
    if(deathmatch) { remove(self); return; }
    precache_model("progs/e2/mon_brute.mdl");
    precache_model("progs/e2/h_brute.mdl");
    precache_model("progs/grenade.mdl");
    precache_sound("brute/death1.wav");
    precache_sound("brute/death2.wav");
    precache_sound("brute/idle1.wav");
    precache_sound("brute/idle2.wav");
    precache_sound("brute/injure1.wav");
    precache_sound("brute/injure2.wav");
    precache_sound("brute/meleeswing.wav");
    precache_sound("brute/shoot.wav");
    precache_sound("brute/sight1.wav");
    precache_sound("brute/sight2.wav");
    precache_sound("shambler/smack.wav");
    self.classname = "monster_brute";
    self.dw_mon = DW_MON_BRUTE;
    self.health = 600;
    self.th_stand = dw_brute_stand;
    self.th_walk = dw_brute_walk;
    self.th_run = dw_brute_run;
    self.th_melee = dw_brute_melee;
    self.th_missile = dw_brute_missile;
    self.th_pain = dw_brute_pain;
    self.th_die = dw_brute_die;
    self.vr_th_checkattack = DW_BruteCheckAttack;
    monster_add_to_counter();
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, "progs/e2/mon_brute.mdl");
    setsize(self, '-32 -32 -24', '32 32 64');
    walkmonster_start();
};

// ---------------------------------------------------------------------------------------------------------------------
// The Construct (progs/e2/mon_excavator.mdl): a flyer, 600 health; it fires crystals (700 speed, 40); below 340 health it
// stops shooting and dives at its enemy (600 speed, steering), bursting on what it hits (60 around). Killed, it breaks
// into its four pieces.

void() dw_exca_stand = { DW_Seq(1, 1, DW_AI_STAND, 0, __NULL__, __NULL__); };
void() dw_exca_walk = { DW_Seq(6, 25, DW_AI_WALK, 8, __NULL__, __NULL__); };
void() dw_exca_run = { DW_Seq(6, 25, DW_AI_RUN, 16, __NULL__, __NULL__); };

void() dw_exca_fireev =
{
    if(self.frame == 57) { sound(self, CHAN_WEAPON, "exca/ex_fire.wav", 1, ATTN_NORM); }
    else if(self.frame == 63 && self.enemy)
    {
        self.effects = self.effects | EF_MUZZLEFLASH;
        vector org = self.origin + '0 0 35';
        vector dir = normalize(VR_AI_Origin(self.enemy) + '0 0 10' - org);
        DW_Missile(org, dir * 700, "progs/e2/proj_crystal.mdl", 40, 40, 6);
    }
    else if(self.frame == 65) { self.attack_finished = time + 1.25; }
};
void() dw_exca_missile = { DW_Seq(56, 65, DW_AI_FACE, 0, dw_exca_fireev, DW_Run); };

void() dw_exca_die =
{
    self.solid = SOLID_NOT;
    ThrowGib("progs/e2/gib_excavatord.mdl", -100);
    ThrowGib("progs/e2/gib_excavatorc.mdl", -100);
    ThrowGib("progs/e2/gib_excavatorb.mdl", -100);
    ThrowGib("progs/e2/gib_excavatora.mdl", -100);
    sound(self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
    BecomeExplosion();
};

void() dw_exca_divetouch =
{
    if(other.classname == "spike" || other.classname == "rocket" || other.classname == "grenade") { return; }
    if(self.health <= 0) { return; }
    T_RadiusDamage(self, self, 60, world);
    Killed(self, self.enemy);
};

// The dive's steering (from its 18th frame on): toward its enemy at 600, turning harder the further it points away.
void() dw_exca_diveev =
{
    if(self.frame < 44) { return; }
    if(self.frame == 44)
    {
        makevectors(self.angles);
        self.velocity = v_forward * 600;
        self.movetype = MOVETYPE_FLY;
    }
    if(self.enemy && (self.frame & 1) == 0)
    {
        vector want = normalize(VR_AI_Origin(self.enemy) - '0 0 16' - self.origin);
        vector have = normalize(self.velocity);
        self.velocity = normalize(have + want * (0.3 + 0.45 * (1 - have * want))) * 600;
    }
    self.angles = vectoangles(self.velocity);
};
void() dw_exca_diveloop = { DW_Seq(46, 54, DW_AI_NONE, 0, dw_exca_diveev, dw_exca_diveloop); };
void() dw_exca_dive =
{
    sound(self, CHAN_VOICE, "exca/ex_kami.wav", 1, ATTN_NORM);
    self.touch = dw_exca_divetouch;
    self.th_missile = SUB_Null;
    self.th_run = dw_exca_dive; // (no way back: it dives on)
    DW_Seq(27, 54, DW_AI_FACE, 0, dw_exca_diveev, dw_exca_diveloop);
};

void(entity attacker, float damage) dw_exca_pain =
{
    if(self.touch == dw_exca_divetouch) { return; }
    if(self.health < 340)
    {
        dw_exca_dive();
        return;
    }
    if(random() * 70 + 20 > damage || self.pain_finished > time) { return; }
    self.pain_finished = time + 1;
    sound(self, CHAN_VOICE, random() > 0.5 ? "exca/ex_pain1.wav" : "exca/ex_pain2.wav", 1, ATTN_NORM);
    DW_Seq(66, 71, DW_AI_NONE, 0, __NULL__, DW_Run);
};

/*dw:QUAKED monster_excavator (1 0 0) (-32 -32 -16) (32 32 72) AMBUSH
Dwell's Construct: a flyer, 600 health; fires crystals (40); below 340 health it dives and bursts (60 around).
*/
void() dw_monster_excavator =
{
    if(!DW_SpawnOk()) { return; }
    if(deathmatch) { remove(self); return; }
    precache_model("progs/e2/mon_excavator.mdl");
    precache_model("progs/e2/proj_crystal.mdl");
    precache_model("progs/e2/gib_excavatora.mdl");
    precache_model("progs/e2/gib_excavatorb.mdl");
    precache_model("progs/e2/gib_excavatorc.mdl");
    precache_model("progs/e2/gib_excavatord.mdl");
    precache_sound("exca/ex_alert1.wav");
    precache_sound("exca/ex_alert2.wav");
    precache_sound("exca/ex_kami.wav");
    precache_sound("exca/ex_pain1.wav");
    precache_sound("exca/ex_pain2.wav");
    precache_sound("exca/ex_fire.wav");
    self.classname = "monster_excavator";
    self.dw_mon = DW_MON_CONSTRUCT;
    self.health = 600;
    self.yaw_speed = 30;
    self.th_stand = dw_exca_stand;
    self.th_walk = dw_exca_walk;
    self.th_run = dw_exca_run;
    self.th_missile = dw_exca_missile;
    self.th_pain = dw_exca_pain;
    self.th_die = dw_exca_die;
    self.vr_th_checkattack = DW_ConstructCheckAttack;
    monster_add_to_counter();
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, "progs/e2/mon_excavator.mdl");
    setsize(self, '-32 -32 -16', '32 32 72');
    flymonster_start();
};
