// dw_bosses.qc -- Dwell support (DWELL.md P2a): Dwell's two bosses, the Awoken (monster_d1boss, d1end) and the Sculptor
// (monster_d2boss, d2end), with the Sculptor's trigger_killboss and target_bullethell. A first pass (P2b refines): their
// phases, attacks and map signals are Dwell's; their motion and the bullet patterns are simplified.
//
// Copyright (C) the Dwell team (2023: Fairweather, Khreathor, bmFbr, Shamblernaut, VoidForce and the others), whose
// bosses these reimplement on Copper (Lunaran, GPL-2) and Quake VR; hand-written from Dwell's behaviour, not its code.
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
// License version 2 or later. Dwell's models and sounds are read from the player's install, never shipped.

// ---------------------------------------------------------------------------------------------------------------------
// The Awoken (progs/mon_d1boss.mdl): a flyer, 1400 health, three phases. It channels (invulnerable) until it sees its
// enemy; it fires homing missiles (750 speed, 25/35/55 around by skill) in a big volley of three, or dashing sideways.
// Each time its health runs out with phases left it fires its next target (target, target2, target3), sleeps through its
// phase change and wakes where its `include` names, healed; the last time it dies, firing target4, and bursts.

float(float offSide, float isDash) dw_d1_missile =
{
    if(!self.enemy) { return FALSE; }
    makevectors(self.angles);
    vector org = self.origin + v_forward * 16 + v_up * 12 + v_right * offSide + '0 0 72';
    vector dir = normalize(VR_AI_Origin(self.enemy) - org);
    float d = vlen(self.enemy.origin - self.origin);
    float spread = isDash ? 0.4 : 0.1;
    float delay = 0.5;
    if(d <= 720) { spread = spread * 0.5; delay = d > 320 ? 0.25 : 0.01; }
    if(d <= 320) { spread = 0; }
    if(!offSide) { dir = dir + v_up * 0.3; }
    else { dir = dir + v_right * (offSide > 0 ? spread : -spread); }
    float dmg = (skill >= 2 || coop) ? 55 : (skill == 1 ? 35 : 25);
    entity m = DW_Missile(org, normalize(dir) * 750, "progs/proj_d1boss.mdl", 0, dmg, 5);
    m.avelocity = '300 300 300';
    m.lip = 750;
    m.pain_finished = time + 1.25;
    m.think = DW_MissileHome;
    m.nextthink = time + delay;
    sound(self, CHAN_WEAPON, "d1boss/proj.wav", 1, ATTN_NORM);
    return TRUE;
};

// Sideways (left or right of its enemy) as far as it can.
void(float dist, float toLeft) dw_d1_strafe =
{
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    ChangeYaw();
    float ofs = toLeft ? 90 : -90;
    if(!walkmove(self.ideal_yaw + ofs, dist)) { walkmove(self.ideal_yaw - ofs, dist); }
};

void() dw_d1_run =
{
    if(!self.takedamage) { self.takedamage = DAMAGE_AIM; }
    DW_Seq(0, 7, DW_AI_RUN, 24, __NULL__, __NULL__);
};

void() dw_d1_bigev =
{
    if(self.frame == 41) { sound(self, CHAN_VOICE, "d1boss/atk.wav", 1, ATTN_NORM); self.cnt = TRUE; }
    else if(self.frame == 58) { dw_d1_missile(-16, TRUE); }
    else if(self.frame == 59) { dw_d1_missile(0, FALSE); }
    else if(self.frame == 60) { dw_d1_missile(16, TRUE); self.attack_finished = time + 1.5; }
};
// A dash (fast, 64 down to 2 units a frame) or a side step (24 down to 16), one missile from that side, then another.
void() dw_d1_sideev =
{
    float left = self.dw_dist; // (1: to its left)
    float k = self.frame - (left ? 32 : 23);
    if(k == 0) { sound(self, CHAN_VOICE, "d1boss/atk.wav", 1, ATTN_NORM); self.lip = self.lip - 1; return; }
    float dash = self.state & 16;
    float step = dash ? (k == 1 ? 64 : k == 2 ? 50 : k == 3 ? 48 : k == 4 ? 32 : k == 8 ? 2 : 8) : (k < 4 ? 26 - k * 2 : 16);
    if(self.enemy) { dw_d1_strafe(step, left); }
    if(k == 5) { dw_d1_missile(left ? -64 : 64, dash); if(!dash) { self.attack_finished = time + 2; } }
};
void() dw_d1_side;
void() dw_d1_sidenext =
{
    if(self.lip > 0 && self.enemy && self.enemy.health > 0) { dw_d1_side(); return; }
    self.state = self.state - (self.state & 16);
    self.th_run();
};
void() dw_d1_side =
{
    float left = random() < 0.5;
    // (its side as the sequence's distance: no AI moves it, its event reads it)
    if(left) { DW_Seq(32, 40, DW_AI_NONE, 1, dw_d1_sideev, dw_d1_sidenext); }
    else { DW_Seq(23, 31, DW_AI_NONE, 0, dw_d1_sideev, dw_d1_sidenext); }
};
void() dw_d1_attack =
{
    if(random() > 0.8 && !self.cnt)
    {
        DW_Seq(41, 64, DW_AI_FACE, 0, dw_d1_bigev, DW_Run);
        return;
    }
    self.cnt = FALSE;
    if(random() > 0.5) { self.lip = 3; self.state = self.state | 16; }
    else { self.lip = 2; self.state = self.state - (self.state & 16); }
    dw_d1_side();
};

float() dw_d1_check =
{
    if(time > self.attack_finished && enemy_vis)
    {
        self.attack_state = AS_MISSILE;
        return TRUE;
    }
    return FALSE;
};

void() dw_d1_channel = { self.takedamage = DAMAGE_NO; DW_Seq(65, 87, DW_AI_STAND, 0, __NULL__, __NULL__); };

// Its phase change over: where its `include` names (if any), healed, channelling again.
void() dw_d1_wake =
{
    self.takedamage = DAMAGE_AIM;
    self.health = 1400;
    entity dest = find(world, targetname, self.cu_include);
    if(dest)
    {
        setorigin(self, dest.origin);
        self.ideal_yaw = vectoyaw(dest.origin - self.origin);
    }
    self.dw_mon = DW_MON_D1BOSS;
    dw_d1_channel();
};

// Killed with phases left: its phase's target, asleep, then awake (dw_d1_wake). TRUE: it lives on.
float() dw_d1_survive =
{
    float phase = self.state & 15;
    if(!phase) { return FALSE; }
    activator = self.enemy.classname == "player" ? self.enemy : activator;
    if(phase == 3) { DW_UseNamed(self.target); }
    else if(phase == 2) { DW_UseNamed(self.cu_target2); }
    else { DW_UseNamed(self.cu_target3); }
    dprint(sprintf("dwboss: d1boss phase %g over\n", phase));
    self.state = self.state - 1;
    self.health = 1;
    self.takedamage = DAMAGE_NO;
    self.enemy = self.goalentity = world;
    sound(self, CHAN_VOICE, "d1boss/phasechange.wav", 1, ATTN_NONE);
    DW_Seq(89, 123, DW_AI_NONE, 0, __NULL__, dw_d1_wake);
    return TRUE;
};

void() dw_d1_deathev =
{
    if(self.frame != 112) { return; }
    float i;
    for(i = 0; i < 6; i++)
    {
        ThrowGib("progs/gib1.mdl", -50 - random() * 50);
        ThrowGib("progs/gib3.mdl", -50 - random() * 50);
    }
};
void() dw_d1_die =
{
    dprint("dwboss: d1boss died\n");
    self.solid = SOLID_NOT;
    sound(self, CHAN_VOICE, "d1boss/phasechange.wav", 1, ATTN_NONE);
    DW_UseNamed(self.cu_target4);
    DW_Seq(89, 123, DW_AI_NONE, 0, dw_d1_deathev, SUB_Remove);
};

void(entity attacker, float damage) dw_d1_pain =
{
    if(self.health <= 0 || random() * 70 + 20 > damage) { return; }
    if(random() > 0.5) { sound(self, CHAN_VOICE, "d1boss/pain1.wav", 1, ATTN_NORM); }
    else if(random() > 0.75) { sound(self, CHAN_VOICE, "d1boss/pain2.wav", 1, ATTN_NORM); }
};

void() dw_d1_walk = { DW_Seq(0, 22, DW_AI_WALK, 16, __NULL__, __NULL__); };

/*dw:QUAKED monster_d1boss (1 0 0) (-32 -32 -96) (32 32 96)
Dwell's first boss, the Awoken: 1400 health, three phases.
target, target2, target3: fired as each phase ends
target4: fired at its death
include: where it wakes after a phase
*/
void() dw_monster_d1boss =
{
    if(!DW_SpawnOk()) { return; }
    if(deathmatch) { remove(self); return; }
    precache_model("progs/mon_d1boss.mdl");
    precache_model("progs/proj_d1boss.mdl");
    precache_sound("d1boss/aggro.wav");
    precache_sound("d1boss/atk.wav");
    precache_sound("d1boss/phasechange.wav");
    precache_sound("d1boss/pain1.wav");
    precache_sound("d1boss/pain2.wav");
    precache_sound("d1boss/proj.wav");
    self.classname = "monster_d1boss";
    self.dw_mon = DW_MON_D1BOSS;
    self.health = 1400;
    self.state = 3; // (phases left; 16: a dash)
    self.yaw_speed = 30;
    self.th_stand = dw_d1_channel;
    self.th_walk = dw_d1_walk;
    self.th_run = dw_d1_run;
    self.th_missile = dw_d1_attack;
    self.th_melee = dw_d1_run;
    self.th_pain = dw_d1_pain;
    self.th_die = dw_d1_die;
    self.th_survive = dw_d1_survive;
    self.vr_th_checkattack = dw_d1_check;
    if(self.deathtype == "") { self.deathtype = " was put to sleep by The Awoken"; }
    monster_add_to_counter();
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, "progs/mon_d1boss.mdl");
    setsize(self, '-32 -32 -96', '32 32 140'); // (P2b: up to its head, 125 up: shots at it hit the box)
    DW_MonsterRow();
    flymonster_start();
};

// ---------------------------------------------------------------------------------------------------------------------
// The Sculptor (progs/mon_d2boss*.mdl): dormant until used (or, with no targetname, a moment after it appears: the
// toolgun's); then it breaks free and flies the path its `include` names at its `speed`, launching swarms of seven
// missiles that fall around its arena (its `target2`; 20/60/80 around by skill) every 2-4 s. Its health (`strength`,
// 1000) run out, it fires its `target` (the map's phase counter), shields itself and rests `wait` s (40), then comes
// back where target2 is; once trigger_killboss has marked it (666 health), it dies instead, firing target4.

const float DW_D2_DORMANT = 0;
const float DW_D2_FLYING = 4;
const float DW_D2_SHIELD = 2;
const float DW_D2_DYING = 14;

// Its next path corner (its goal), reached in a straight line at its speed.
void() dw_d2_nextcorner =
{
    entity c = self.goalentity ? find(world, targetname, self.goalentity.target) : find(world, targetname, self.cu_include);
    if(!c) { self.velocity = '0 0 0'; return; }
    self.goalentity = c;
    vector d = c.origin - self.origin;
    float t = vlen(d) / max(self.speed, 1);
    self.velocity = normalize(d) * self.speed;
    self.attack_state = time + max(t, 0.1); // (when it is there)
};

// A swarm: seven missiles up out of it, each curving down over 4 s onto a mark around a point near its arena's centre.
void() dw_d2_swarm =
{
    entity centre = find(world, targetname, self.cu_target2);
    vector mid = centre ? centre.origin : (self.enemy ? self.enemy.origin : self.origin);
    float a = random() * 360;
    vector aim = mid + DW_Yaw(a) * 320 * random() - '0 0 160';
    float dmg = (skill >= 2 || coop) ? 80 : (skill == 1 ? 60 : 20);
    float i;
    for(i = 0; i < 7; i++)
    {
        float b = i * 51.4;
        vector spot = aim + DW_Yaw(b) * (i ? 192 : 0);
        traceline(spot + '0 0 256', spot - '0 0 1024', TRUE, self);
        spot = trace_endpos;
        entity m = DW_Missile(self.origin + '0 0 160', '0 0 400', "progs/e2/hell_rocket.mdl", 0, dmg, 8);
        m.oldorigin = spot;
        m.lip = 400 + 50 * i; // (its speed: the swarm lands spread out)
        m.think = DW_MissileHome;
        m.enemy = world;
        m.nextthink = time + 0.1;
        // its mark on the ground: the homing goes for an entity, so a still marker entity is its enemy
        entity mk = spawn();
        setmodel(mk, "progs/e2/marker.mdl");
        setorigin(mk, spot);
        mk.think = SUB_Remove;
        mk.nextthink = time + 5;
        mk.health = 1;
        m.enemy = mk;
        m.pain_finished = time + 4;
    }
};

// The controller (every 0.1 s): along its path, swarms while flying, back after its rest.
void() dw_d2_think =
{
    self.nextthink = time + 0.1;
    if(self.state == DW_D2_FLYING)
    {
        if(time >= self.attack_state) { dw_d2_nextcorner(); }
        if(time >= self.attack_finished && self.enemy)
        {
            dw_d2_swarm();
            self.attack_finished = time + 2 + 2 * random();
        }
        if(!self.enemy || self.enemy.health <= 0) { self.enemy = checkclient(); }
    }
    else if(self.state == DW_D2_SHIELD && time >= self.pain_finished)
    {
        entity dest = find(world, targetname, self.cu_target2);
        if(dest) { setorigin(self, dest.origin); }
        self.alpha = 0;
        self.state = DW_D2_FLYING;
        self.takedamage = DAMAGE_AIM;
        self.goalentity = world;
        self.attack_finished = time + 3;
        dw_d2_nextcorner();
    }
};

void() dw_d2_wake =
{
    dprint("dwboss: d2boss fight\n");
    self.state = DW_D2_FLYING;
    self.takedamage = DAMAGE_AIM;
    self.solid = SOLID_BBOX;
    setsize(self, '-48 -48 -48', '48 48 48');
    self.avelocity_y = self.yaw_speed;
    self.attack_finished = time + 3;
    self.goalentity = world;
    self.enemy = checkclient();
    dw_d2_nextcorner();
    self.think = dw_d2_think;
    self.nextthink = time + 0.1;
};

void() dw_d2_intro =
{
    if(self.state != DW_D2_DORMANT) { return; }
    self.enemy = activator;
    setmodel(self, "progs/mon_d2bossb.mdl");
    self.angles_x = 0;
    ThrowGib("progs/gib_d2bosscrystala.mdl", -50);
    ThrowGib("progs/gib_d2bosscrystalb.mdl", -65);
    ThrowGib("progs/gib_d2bosscrystalc.mdl", -55);
    ThrowGib("progs/gib_d2bossstonec.mdl", -25);
    ThrowGib("progs/gib_d2bossstoned.mdl", -25);
    DW_Boom(self.origin);
    self.state = 1;
    self.think = dw_d2_wake;
    self.nextthink = time + 2;
};

// Its health run out: unmarked, it resets (its target, shielded, resting); marked, it dies.
float() dw_d2_survive =
{
    if(self.dw_marked) { return FALSE; } // (trigger_killboss's mark)
    activator = self.enemy.classname == "player" ? self.enemy : activator;
    dprint("dwboss: d2boss phase over\n");
    DW_UseNamed(self.target);
    self.health = self.max_health;
    self.takedamage = DAMAGE_NO;
    self.velocity = '0 0 0';
    self.state = DW_D2_SHIELD;
    self.alpha = 0.5;
    self.pain_finished = time + self.wait;
    return TRUE;
};

void() dw_d2_deathev =
{
    if(random() < 0.4) { DW_Boom(self.origin + '128 0 0' * crandom() + '0 128 0' * crandom() + '0 0 128' * crandom()); }
};
void() dw_d2_dead =
{
    activator = self.enemy.classname == "player" ? self.enemy : activator;
    DW_UseNamed(self.cu_target4);
    remove(self);
};
void() dw_d2_die =
{
    dprint("dwboss: d2boss died\n");
    sound(self, CHAN_VOICE, "d2boss/death.wav", 1, ATTN_NONE);
    self.state = DW_D2_DYING;
    self.solid = SOLID_NOT;
    self.velocity = '0 0 0';
    self.avelocity = '0 0 0';
    DW_Seq(16, 80, DW_AI_NONE, 0, dw_d2_deathev, dw_d2_dead);
};

/*dw:QUAKED monster_d2boss (1 0 0) (-48 -48 -48) (48 48 48)
Dwell's second boss, the Sculptor: dormant until used.
strength: health (1000)
speed: flight speed (300)
wait: rest after a phase (40)
include: its first path corner
target: fired as each phase ends
target2: its arena's centre
target4: fired at its death
*/
void() dw_monster_d2boss =
{
    if(!DW_SpawnOk()) { return; }
    if(deathmatch) { remove(self); return; }
    precache_model("progs/mon_d2boss.mdl");
    precache_model("progs/mon_d2bossb.mdl");
    precache_model("progs/gib_d2bosscrystala.mdl");
    precache_model("progs/gib_d2bosscrystalb.mdl");
    precache_model("progs/gib_d2bosscrystalc.mdl");
    precache_model("progs/gib_d2bossstonec.mdl");
    precache_model("progs/gib_d2bossstoned.mdl");
    precache_model("progs/e2/marker.mdl");
    precache_model("progs/e2/hell_rocket.mdl");
    precache_sound("d2boss/death.wav");
    self.classname = "monster_d2boss";
    self.dw_mon = DW_MON_D2BOSS;
    self.health = self.cu_strength > 0 ? self.cu_strength : 1000;
    self.max_health = self.health;
    if(!self.speed) { self.speed = 300; }
    if(!self.wait) { self.wait = 40; }
    self.yaw_speed = self.fixangle ? self.fixangle : 180;
    self.fixangle = 0;
    self.flags = self.flags | FL_MONSTER;
    total_monsters = total_monsters + 1;
    monster_add_to_counter();
    self.solid = SOLID_NOT;
    self.takedamage = DAMAGE_NO;
    self.movetype = MOVETYPE_NOCLIP;
    setmodel(self, "progs/mon_d2boss.mdl");
    setsize(self, '0 0 0', '0 0 0');
    self.angles_x = -90;
    self.state = DW_D2_DORMANT;
    self.th_die = dw_d2_die;
    self.th_survive = dw_d2_survive;
    self.use = dw_d2_intro;
    if(self.deathtype == "") { self.deathtype = " was killed by the Sculptor"; }
    if(self.targetname == "") // (the toolgun's: no map to wake it)
    {
        self.think = dw_d2_intro;
        self.nextthink = time + 1;
    }
};

// trigger_killboss: used, it marks the boss its target names to die next time (666 health left).
void() dw_killboss_use =
{
    entity b = find(world, targetname, self.target);
    if(!b) { return; }
    dprint("dwboss: d2boss marked\n");
    b.dw_marked = TRUE;
    b.health = 666;
};
/*dw:QUAKED trigger_killboss (.5 .5 .5) (-8 -8 -8) (8 8 8)
Dwell: used, marks the boss its target names to die next time (666 health left).
*/
void() dw_trigger_killboss =
{
    if(!DW_Data()) { remove(self); return; }
    self.use = dw_killboss_use;
};

// ---------------------------------------------------------------------------------------------------------------------
// target_bullethell: used, a pattern of slow bullets (200 speed, 5 to a player) out from it by its `style`: 1 a line at
// the nearest player, 2 two lines, 3 a ring, 4 a spiral (turning each use), 5 a wheel of spokes. (Dwell's patterns move
// and turn more: P2b.)

void() dw_bullet_touch =
{
    if(other.classname == "player") { T_Damage(other, self, self.owner, self.dmg); }
    if(other.classname == "player" || other.solid == SOLID_BSP) { remove(self); }
};

void(vector dir) dw_bullet =
{
    entity b = spawn();
    b.owner = self;
    b.classname = "dw_bullet";
    b.movetype = MOVETYPE_FLYMISSILE;
    b.solid = SOLID_TRIGGER;
    setmodel(b, "progs/e2/hell_bullet.mdl");
    setsize(b, '-2 -2 -2', '2 2 2');
    setorigin(b, self.origin);
    b.velocity = normalize(dir) * (self.speed ? self.speed : 200);
    b.angles = vectoangles(b.velocity);
    b.dmg = self.dmg ? self.dmg : 5;
    b.touch = dw_bullet_touch;
    b.think = SUB_Remove;
    b.nextthink = time + 10;
};

void() dw_bullethell_use =
{
    entity p = checkclient();
    vector at = p ? p.origin - self.origin : '1 0 0';
    at_z = 0;
    float yaw = vectoyaw(at);
    float i, n, a;
    float s = self.style ? self.style : 1 + floor(random() * 4.99);
    if(s == 1 || s == 2)
    {
        n = s == 1 ? 1 : 2;
        for(i = 0; i < n; i++)
        {
            a = yaw + (n == 2 ? (i ? 15 : -15) : 0);
            dw_bullet(DW_Yaw(a));
        }
        return;
    }
    n = s == 5 ? 8 : 16;
    self.cnt = self.cnt + (s == 4 ? 11 : 0);
    for(i = 0; i < n; i++)
    {
        dw_bullet(DW_Yaw(self.cnt + i * 360 / n));
    }
};

/*dw:QUAKED target_bullethell (.5 .5 .5) (-8 -8 -8) (8 8 8)
Dwell: used, a pattern of slow bullets (5 to a player).
style: 1 line, 2 two lines, 3 ring, 4 spiral, 5 wheel
*/
void() dw_target_bullethell =
{
    if(!DW_Data()) { remove(self); return; }
    precache_model("progs/e2/hell_bullet.mdl");
    self.use = dw_bullethell_use;
};
