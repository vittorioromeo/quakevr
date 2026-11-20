// dw_bosses.qc -- Dwell support (DWELL.md P2a): Dwell's two bosses, the Awoken (monster_d1boss, d1end) and the Sculptor
// (monster_d2boss, d2end), with the Sculptor's trigger_killboss and target_bullethell: their phases, attacks and map
// signals are Dwell's, the Sculptor's motion and the bullet-hell patterns too (P2b part two).
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
// The Sculptor (progs/mon_d2boss*.mdl): a ring of crystal and stone, dormant (lying flat) until used (or, with no
// targetname, a moment after it appears: the toolgun's); then it breaks free and flies the path its `include` names at
// its `speed`, spinning about its axis (`fixangle` degrees a second, 180) and tilting its face toward its enemy, launching
// swarms of seven missiles that fall around its arena (its `target2`; 20/60/80 around by skill) every 2-4 s. Its health
// (`strength`, 1000) run out, it fires its `target` (the map's phase counter), lies flat again, raises its shield,
// teleports to target2 and rests there `wait` s (40) turning slowly the other way, then flies again; once
// trigger_killboss has marked it (666 health), it dies instead, firing target4. Dwell's motion (DWELL.md P2b part two).

const float DW_D2_DORMANT = 0;
const float DW_D2_INTRO = 1;
const float DW_D2_SHIELD = 2;   // lying flat again, then its shield fading in (1.1 s)
const float DW_D2_TELEPORT = 3; // out (frames 1-8), to target2, in (frames 9-15): 1.5 s
const float DW_D2_FLYING = 4;
const float DW_D2_REST = 5;
const float DW_D2_DYING = 14;
const string DW_D2_SHIELD_MDL = "progs/mon_d2bossf.mdl";

.float dw_r0;         // the Sculptor: when its current step began; a bullet-hell task: when it started
.entity dw_child;     // the Sculptor's shield
.entity dw_shooter;   // a bullet-hell projectile's emitter (its attacker; `owner` changes as it passes monsters)
.float dw_row;        // a bullet-hell task's pattern row (DW_HELL)
.vector dw_vel;       // a bullet-hell shot's velocity (on again past a monster)

// Heading for its path corner (its goal) in a straight line at its speed (Dwell's SUB_CalcMove along the path), set
// again each tick: nothing pushes it off its path (VR's knockback on flyers).
void() dw_d2_head =
{
    if(!self.goalentity)
    {
        self.velocity = '0 0 0';
        return;
    }
    vector d = self.goalentity.origin - self.origin;
    self.velocity = normalize(d) * self.speed;
    self.attack_state = time + max(vlen(d) / max(self.speed, 1), 0.05); // (when it is there)
};

// Its next path corner (the first: its `include`).
void() dw_d2_nextcorner =
{
    self.goalentity = self.goalentity ? find(world, targetname, self.goalentity.target) : find(world, targetname, self.cu_include);
    dw_d2_head();
};

// Turns its face to pitch `want` and its yaw to 0 (Dwell's: it blends there in 0.5 s, then follows), its roll (the spin
// about its axis) left to avelocity_z.
void(float want) dw_d2_turn =
{
    float d = want - self.angles_x;
    d = d - 360 * floor((d + 180) / 360);
    float y = -self.angles_y;
    y = y - 360 * floor((y + 180) / 360);
    self.avelocity_x = d * 5;
    self.avelocity_y = y * 5;
};

// One of a swarm (the Sculptor's, and the bullet hell's rocket barrage): a rocket up out of `from`, curving down onto
// its mark at `spot` (Dwell's marker on the ground: the homing goes for an entity) at `spd`, bursting for `dmg` around.
entity(vector from, vector spot, float dmg, float spd) DW_SwarmShot =
{
    entity m = DW_Missile(from, '0 0 400', "progs/e2/hell_rocket.mdl", 0, dmg, 8);
    m.vr_foeshot = TRUE; // (an enemy's shot whatever fired it: a swing bats it back, vr_deflect)
    m.lip = spd;
    m.think = DW_MissileHome;
    m.nextthink = time + 0.1;
    entity mk = spawn();
    mk.classname = "dw_hellmark";
    setmodel(mk, "progs/e2/marker.mdl");
    setorigin(mk, spot);
    mk.think = SUB_Remove;
    mk.nextthink = time + 5;
    mk.health = 1;
    m.enemy = mk;
    m.pain_finished = time + 4;
    return m;
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
        vector spot = aim + DW_Yaw(i * 51.4) * (i ? 192 : 0);
        traceline(spot + '0 0 256', spot - '0 0 1024', TRUE, self);
        DW_SwarmShot(self.origin + '0 0 160', trace_endpos, dmg, 400 + 50 * i); // (the swarm lands spread out)
    }
};

// Flying again (Dwell's flying_start): its shield down, hurt again, spinning, from its path's first corner.
void() dw_d2_fly =
{
    setmodel(self.dw_child, "");
    self.dw_child.avelocity = '0 0 0';
    self.frame = 0;
    self.state = DW_D2_FLYING;
    self.takedamage = DAMAGE_AIM;
    self.avelocity_z = self.yaw_speed;
    self.goalentity = world;
    dw_d2_nextcorner();
};

// The controller (every 0.1 s; on time for each path corner): along its path, swarms while flying; after a phase, flat
// again, shield, teleport, rest, fly.
void() dw_d2_think =
{
    self.nextthink = time + 0.1;
    entity sh = self.dw_child;
    float e = time - self.dw_r0;
    if(self.state == DW_D2_FLYING)
    {
        if(time >= self.attack_state)
        {
            if(self.goalentity) { setorigin(self, self.goalentity.origin); }
            dw_d2_nextcorner();
        }
        else { dw_d2_head(); }
        if(self.attack_state > time) { self.nextthink = min(self.nextthink, self.attack_state); }
        if(time >= self.attack_finished && self.enemy)
        {
            dw_d2_swarm();
            self.attack_finished = time + 2 + 2 * random();
        }
        if(!self.enemy || self.enemy.health <= 0) { self.enemy = checkclient(); }
        vector look = vectoangles(self.enemy.origin - self.origin);
        dw_d2_turn(self.enemy ? look_x : -90);
    }
    else { self.velocity = '0 0 0'; } // (still: nothing pushes it)
    if(self.state == DW_D2_SHIELD)
    {
        dw_d2_turn(-90);
        if(e >= 0.5)
        {
            if(sh.model == "")
            {
                setmodel(sh, DW_D2_SHIELD_MDL);
                setorigin(sh, self.origin);
            }
            sh.angles = self.angles;
            sh.frame = 16 + min(5, floor((e - 0.5) * 10));
            sh.alpha = 0.1 * (sh.frame - 15);
        }
        if(e >= 1.1)
        {
            self.avelocity = '0 0 0';
            self.state = DW_D2_TELEPORT;
            self.dw_r0 = time;
        }
    }
    else if(self.state == DW_D2_TELEPORT)
    {
        self.frame = min(15, 1 + floor(e * 10));
        entity dest = find(world, targetname, self.cu_target2);
        if(self.frame >= 9 && dest && self.origin != dest.origin) // (out: to its arena's centre, coming in)
        {
            setorigin(self, dest.origin);
            setorigin(sh, self.origin);
        }
        sh.frame = self.frame;
        if(e >= 1.5)
        {
            self.frame = 0;
            sh.frame = 0;
            self.state = DW_D2_REST;
            self.avelocity_z = self.yaw_speed * -0.5;
            sh.avelocity_z = self.avelocity_z;
            self.pain_finished = time + self.wait;
        }
    }
    else if(self.state == DW_D2_REST && time >= self.pain_finished)
    {
        dw_d2_fly();
    }
};

// Its ring as the shots meet it (DWELL.md 9.7): Dwell ringed it with 50-odd small hit boxes so that its stones and
// crystals take hits round its body, the gaps between them not. Our precise hits (vr_hitmodel.cpp) do that from the
// model itself, inside the entity's box, so the box reaches the ring's rim whichever way it turns (120-140 units from
// its middle); a shot between its stones meets no triangle and flies on. (It moves noclip: the box never stops it.)
void(entity e) DW_D2_FightBox =
{
    e.solid = SOLID_BBOX;
    setsize(e, '-144 -144 -144', '144 144 144');
};

void() dw_d2_wake =
{
    dprint("dwboss: d2boss fight\n");
    DW_D2_FightBox(self);
    self.attack_finished = time + 3;
    self.enemy = checkclient();
    dw_d2_fly();
    self.think = dw_d2_think;
    self.nextthink = time + 0.1;
};

void() dw_d2_intro =
{
    if(self.state != DW_D2_DORMANT) { return; }
    self.enemy = activator;
    setmodel(self, "progs/mon_d2bossb.mdl");
    ThrowGib("progs/gib_d2bosscrystala.mdl", -50);
    ThrowGib("progs/gib_d2bosscrystalb.mdl", -65);
    ThrowGib("progs/gib_d2bosscrystalc.mdl", -55);
    ThrowGib("progs/gib_d2bossstonec.mdl", -25);
    ThrowGib("progs/gib_d2bossstoned.mdl", -25);
    DW_Boom(self.origin);
    self.state = DW_D2_INTRO;
    self.think = dw_d2_wake;
    self.nextthink = time + 2;
};

// Its health run out: unmarked, it resets (its target; flat again, shield, teleport, rest: dw_d2_think); marked, it
// dies.
float() dw_d2_survive =
{
    if(self.dw_marked) { return FALSE; } // (trigger_killboss's mark)
    activator = self.enemy.classname == "player" ? self.enemy : activator;
    dprint("dwboss: d2boss phase over\n");
    DW_UseNamed(self.target);
    self.health = self.max_health;
    self.takedamage = DAMAGE_NO;
    self.velocity = '0 0 0';
    self.avelocity = '0 0 0';
    self.state = DW_D2_SHIELD;
    self.dw_r0 = time;
    self.nextthink = time + 0.1;
    return TRUE;
};

// Dying: bursts about it, its spin running down (Dwell's, over about 2 s).
void() dw_d2_deathev =
{
    self.avelocity_z = self.avelocity_z * 0.85;
    if(random() < 0.4) { DW_Boom(self.origin + '128 0 0' * crandom() + '0 128 0' * crandom() + '0 0 128' * crandom()); }
};
void() dw_d2_dead =
{
    activator = self.enemy.classname == "player" ? self.enemy : activator;
    DW_UseNamed(self.cu_target4);
    if(self.dw_child) { remove(self.dw_child); }
    remove(self);
};
void() dw_d2_die =
{
    dprint("dwboss: d2boss died\n");
    sound(self, CHAN_VOICE, "d2boss/death.wav", 1, ATTN_NONE);
    self.state = DW_D2_DYING;
    self.solid = SOLID_NOT;
    self.velocity = '0 0 0';
    self.avelocity_x = 0;
    self.avelocity_y = 0;
    setmodel(self.dw_child, "");
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
    precache_model(DW_D2_SHIELD_MDL);
    precache_sound("d2boss/death.wav");
    // its shield (shown after a phase: dw_d2_think), following it as it teleports
    entity sh = spawn();
    sh.classname = "dw_d2_shield";
    sh.movetype = MOVETYPE_NOCLIP;
    sh.owner = self;
    setorigin(sh, self.origin);
    self.dw_child = sh;
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
// target_bullethell: used, Dwell's bullet-hell pattern by its `style` (0: one of 1-7 at random), out from where it
// stands: 1 a line of 10 shots at each player, 2 two lines straddling where he stood, 3 a ring of 24 (half a step
// turned each use), 4 two spirals of 48, 5 a moving wheel (16 shots circling a point that drifts away; its ring hurts),
// 6 a rocket barrage (6 rockets falling on marks around each player: Dwell left it empty, ours is its swarm's), 7 a pulse
// (a ring of 24 swelling from 64 to 320 units and back, twice; its ring hurts), 8 Dwell's unused program A once through
// (lines, rings, spirals, three wheels: 28 s). Each pattern is rows of DW_HELL, each row a task (one per player for
// lines and barrages) that fires its shots on its own clock. The shots are enemy projectiles (a swing bats them back),
// fly through monsters (Dwell's), glow and leave a trail (the engine gives hell_bullet.mdl one: vr_view.cpp), fade out
// at the end of their life. Speeds are Dwell's, slower for the aimed lines (500-550 for 700-800: a standing VR player
// steps out of them). Ring damage (wheel, pulse) is Dwell's rate (40 a second on the ring) in ticks of 4 every 0.1 s, the
// ring's sweep between ticks included.

const float DW_HK_LINE = 1;     // a: follow (1: at him each shot; 0: where he stood), n shots every cycle
const float DW_HK_TWOLINES = 2; // as a line; b: each line's side offset at 100 units
const float DW_HK_CIRCLE = 3;   // a..b of 24 steps, n rings every cycle; flag: turned half a step each use
const float DW_HK_SPIRAL = 4;   // a to b of 48 steps, one shot every cycle; flag: clockwise
const float DW_HK_WHEEL = 5;    // a: radius / 100; n shots; life: its life; speed: its drift
const float DW_HK_BARRAGE = 6;  // n rockets every cycle, each on a mark within a of the player; speed, dmg (splash)
const float DW_HK_PULSE = 7;    // a..b radius at speed, n pulses, cycle s still at each end

typedef struct
{
    float style, kind, a, b, n, cycle, life, delay, speed, dmg, flag;
} dw_hell_row;

dw_hell_row DW_HELL[] =
{
    // style kind           a     b    n    cycle life delay speed dmg flag
    { 1, DW_HK_LINE,        1,    0,   10,  0.1,  2.8, 0,    500,  10, 0 },
    { 2, DW_HK_TWOLINES,    0,    15,  10,  0.1,  2.9, 0,    550,  5,  0 },
    { 3, DW_HK_CIRCLE,      0,    24,  1,   0.1,  2,   0,    300,  5,  1 },
    { 4, DW_HK_SPIRAL,      0,    48,  0,   0.05, 3,   0,    200,  5,  1 },
    { 4, DW_HK_SPIRAL,      0,    48,  0,   0.05, 3,   2.4,  200,  5,  1 },
    { 5, DW_HK_WHEEL,       1,    0,   16,  0,    8,   0,    100,  4,  0 },
    { 6, DW_HK_BARRAGE,     128,  0,   6,   0.35, 0,   0,    450,  20, 0 },
    { 7, DW_HK_PULSE,       64,   320, 2,   0.5,  0,   0,    400,  4,  0 },
    // program A (Dwell's runProgramA, never placed in its maps), once through; its delays are its steps' starts
    { 8, DW_HK_LINE,        1,    0,   10,  0.1,  2.8, 1,    500,  10, 0 },
    { 8, DW_HK_TWOLINES,    0,    15,  10,  0.1,  2.9, 2.5,  550,  5,  0 },
    { 8, DW_HK_CIRCLE,      0,    24,  2,   1,    2,   4,    300,  5,  1 },
    { 8, DW_HK_CIRCLE,      0,    12,  1,   0,    2,   6.5,  300,  5,  1 },
    { 8, DW_HK_CIRCLE,      12,   24,  1,   0,    2,   7,    300,  5,  1 },
    { 8, DW_HK_CIRCLE,      0,    12,  1,   0,    2,   7.5,  300,  5,  1 },
    { 8, DW_HK_CIRCLE,      12,   24,  1,   0,    2,   8,    300,  5,  1 },
    { 8, DW_HK_SPIRAL,      0,    24,  0,   0.05, 2,   8,    200,  5,  1 },
    { 8, DW_HK_SPIRAL,      24,   0,   0,   0.05, 2,   8,    200,  5,  0 },
    { 8, DW_HK_SPIRAL,      24,   48,  0,   0.05, 2,   10,   200,  5,  1 },
    { 8, DW_HK_SPIRAL,      0,    24,  0,   0.05, 2,   10,   200,  5,  1 },
    { 8, DW_HK_SPIRAL,      0,    48,  0,   0.05, 0,   12,   200,  5,  1 }, // (life 0: six arms that fade at once,
    { 8, DW_HK_SPIRAL,      8,    7,   0,   0.05, 0,   12,   200,  5,  1 }, // a flower about it: Dwell's)
    { 8, DW_HK_SPIRAL,      16,   15,  0,   0.05, 0,   12,   200,  5,  1 },
    { 8, DW_HK_SPIRAL,      24,   23,  0,   0.05, 0,   12,   200,  5,  1 },
    { 8, DW_HK_SPIRAL,      32,   31,  0,   0.05, 0,   12,   200,  5,  1 },
    { 8, DW_HK_SPIRAL,      40,   39,  0,   0.05, 0,   12,   200,  5,  1 },
    { 8, DW_HK_WHEEL,       0.5,  0,   16,  0,    6,   16,   100,  4,  0 },
    { 8, DW_HK_WHEEL,       0.75, 0,   16,  0,    8,   17,   100,  4,  0 },
    { 8, DW_HK_WHEEL,       1,    0,   16,  0,    10,  18,   100,  4,  0 },
};
const float DW_HELL_N = 28;
const string DW_HELL_MDL = "progs/e2/hell_bullet.mdl";

nosave float dw_hell_shots, dw_hell_hits; // (counted for the test, dw_test.qc; never saved)

// A bullet-hell hit: `dmg` to `victim` (counted). A player is not pushed (comfort: a pattern's many small hits would
// shove him about, off ledges too); the hit's direction, pain and armour are T_Damage's as ever.
void(entity victim, entity inflictor, entity attacker, float dmg) DW_HellHit =
{
    dw_hell_hits = dw_hell_hits + 1;
    vector keep = victim.velocity;
    T_Damage(victim, inflictor, attacker, dmg);
    if(victim.flags & FL_CLIENT) { victim.velocity = keep; }
};

// Fading out from its `pain_finished` (Dwell's: 0.9, 0.6, 0.3 a tenth of a second apart), then gone.
void() dw_hb_fade =
{
    self.alpha = 0.9 - 0.3 * floor((time - self.pain_finished) * 10 + 0.01);
    if(self.alpha < 0.1)
    {
        remove(self);
        return;
    }
    self.think = dw_hb_fade;
    self.nextthink = time + 0.1;
};

// Past a monster it flew into: on again at its speed.
void() dw_hb_resume =
{
    self.velocity = self.dw_vel;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.think = dw_hb_fade;
    self.nextthink = max(time, self.pain_finished);
};

// A shot meets something: a player is hurt (batted back by a swing, vr_deflect: a monster), a monster passed through
// (Dwell's: it ignores that one from now on), anything else stops it.
void() dw_hb_touch =
{
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    if(other.solid == SOLID_TRIGGER || other.solid == SOLID_NOT) { return; } // (another shot overlapping it: a trigger)
    float batted = self.owner.flags & FL_CLIENT;    if(other.takedamage && (batted ? other != self.owner : other.flags & FL_CLIENT))
    {
        DW_HellHit(other, self, batted ? self.owner : self.dw_shooter, self.dmg);
        remove(self);
        return;
    }
    if(!batted && (other.flags & FL_MONSTER))
    {
        self.owner = other;
        self.think = dw_hb_resume;
        self.nextthink = time;
        return;
    }
    remove(self);
};

// A shot from the task self (its emitter `self.owner`) out of `org` along `dir`; it fades out after `life` s.
entity(vector org, vector dir, float spd, float hurt, float life) DW_HellShot =
{
    entity b = spawn();
    b.classname = "dw_hellshot";
    b.owner = self.owner;
    b.dw_shooter = self.owner;
    b.vr_foeshot = TRUE;
    b.movetype = MOVETYPE_FLYMISSILE;
    b.solid = SOLID_TRIGGER; // (it hits what it flies into, never another of its kind: they start together)
    setmodel(b, DW_HELL_MDL);
    setsize(b, '0 0 0', '0 0 0');
    setorigin(b, org);
    b.velocity = normalize(dir) * spd;
    b.dw_vel = b.velocity;
    b.angles = vectoangles(b.velocity);
    b.dmg = hurt;
    b.touch = dw_hb_touch;
    b.pain_finished = time + life;
    b.think = dw_hb_fade;
    b.nextthink = b.pain_finished;
    dw_hell_shots = dw_hell_shots + 1;
    return b;
};

// A drawn piece of a ring (a wheel's shot, a pulse's sector): it hurts nobody itself (its ring does), moved by its task.
entity(vector org) DW_HellPiece =
{
    entity b = spawn();
    b.classname = "dw_hellshot";
    b.owner = self;
    b.movetype = MOVETYPE_NOCLIP;
    setmodel(b, DW_HELL_MDL);
    setsize(b, '0 0 0', '0 0 0');
    setorigin(b, org);
    dw_hell_shots = dw_hell_shots + 1;
    return b;
};

// The players `c` is within [lo, hi] of across, at its height: hurt `dmg` by the ring the task self draws.
void(vector c, float lo, float hi, float dmg) DW_HellRing =
{
    entity p = find(world, classname, "player");
    while(p)
    {
        if(p.health > 0 && p.takedamage && c_z > p.absmin_z && c_z < p.absmax_z)
        {
            vector d = p.origin - c;
            d_z = 0;
            if(vlen(d) >= lo && vlen(d) <= hi) { DW_HellHit(p, self, self.owner, dmg); }
        }
        p = find(p, classname, "player");
    }
};

// A pulse's radius `e` s after it began: 0.4 s fading in at `lo`, then `n` times: still `idle` s, out to `hi` at `spd`,
// still, back.
float(float e, float lo, float hi, float idle, float spd, float n) DW_PulseRadius =
{
    e = e - 0.4;
    float d = (hi - lo) / spd;
    float per = 2 * idle + 2 * d;
    if(e < 0 || e >= n * per) { return lo; }
    e = e - per * floor(e / per);
    if(e < idle) { return lo; }
    e = e - idle;
    if(e < d) { return lo + e * spd; }
    e = e - d;
    if(e < idle) { return hi; }
    return hi - min(e - idle, d) * spd;
};

// A wheel's shot: out from its drifting centre (its task) at 100 a second to the wheel's radius, then round it at 225
// degrees a second (Dwell's: a sixteenth of a turn every tenth); fading out a second before the wheel ends.
void() dw_hell_wheelshot =
{
    entity w = self.owner;
    if(w.classname != "dw_helltask" || time >= self.pain_finished)
    {
        dw_hb_fade();
        return;
    }
    float rad = w.lip;
    float e = time + 0.1 - self.dw_r0;
    float r = min(rad, 100 * e);
    float ang = self.cnt + 225 * max(0, e - rad / 100);
    vector want = w.origin + w.velocity * 0.1 + DW_Yaw(ang) * r;
    self.velocity = (want - self.origin) * 10;
    self.angles = vectoangles(self.velocity);
    self.think = dw_hell_wheelshot;
    self.nextthink = time + 0.1;
};

// A bullet-hell task's clock: its row's pattern, a step at a time.
void() dw_hell_tick =
{
    dw_hell_row r = DW_HELL[self.dw_row];
    float i;
    vector at;
    self.nextthink = time + max(r.cycle, 0.05);
    if(r.kind == DW_HK_LINE || r.kind == DW_HK_TWOLINES || r.kind == DW_HK_BARRAGE)
    {
        if(self.cnt < 1 || self.enemy.health <= 0)
        {
            remove(self);
            return;
        }
        at = r.a ? self.enemy.origin : self.oldorigin;
        if(r.kind == DW_HK_LINE) { DW_HellShot(self.origin, at - self.origin, r.speed, r.dmg, r.life); }
        else if(r.kind == DW_HK_TWOLINES)
        {
            makevectors('0 1 0' * vectoyaw(at - self.origin));
            DW_HellShot(self.origin, v_forward * 100 + v_right * r.b, r.speed, r.dmg, r.life);
            DW_HellShot(self.origin, v_forward * 100 - v_right * r.b, r.speed, r.dmg, r.life);
        }
        else
        {
            vector spot = at + DW_Yaw(random() * 360) * random() * r.a;
            traceline(spot + '0 0 64', spot - '0 0 1024', TRUE, self.enemy);
            entity m = DW_SwarmShot(self.origin, trace_endpos, r.dmg, r.speed);
            m.owner = self.owner;
            m.classname = "dw_hellshot"; // (its siblings pass each other: DW_MissileTouch)
            dw_hell_shots = dw_hell_shots + 1;
        }
        self.cnt = self.cnt - 1;
        return;
    }
    if(r.kind == DW_HK_CIRCLE)
    {
        if(self.cnt < 1)
        {
            remove(self);
            return;
        }
        float steps = r.b - r.a;
        if(steps <= 0) { steps = steps + 24; }
        float half = self.owner.cnt ? 0.5 : 0;
        for(i = 0; i < steps; i++) { DW_HellShot(self.origin, DW_Yaw((r.a + i + half) * 15), r.speed, r.dmg, r.life); }
        if(r.flag) { self.owner.cnt = !self.owner.cnt; }
        self.cnt = self.cnt - 1;
        return;
    }
    if(r.kind == DW_HK_SPIRAL)
    {
        if(self.cnt < 1)
        {
            remove(self);
            return;
        }
        DW_HellShot(self.origin, DW_Yaw(self.count * 7.5), r.speed, r.dmg, r.life);
        self.count = self.count + (r.flag ? 1 : -1);
        self.count = self.count - 48 * floor(self.count / 48);
        self.cnt = self.cnt - 1;
        return;
    }
    self.nextthink = time + 0.1;
    float e = time - self.dw_r0;
    if(r.kind == DW_HK_WHEEL)
    {
        if(!self.lip) // (its start: drifting off, its shots out)
        {
            self.lip = r.a * 100;
            self.movetype = MOVETYPE_NOCLIP;
            self.velocity = DW_Yaw(random() * 360) * r.speed;
            for(i = 0; i < r.n; i++)
            {
                entity s = DW_HellPiece(self.origin);
                s.cnt = i * 360 / r.n;
                s.dw_r0 = time;
                s.pain_finished = time + r.life - 1;
                s.think = dw_hell_wheelshot;
                s.nextthink = time;
            }
            return;
        }
        if(e >= r.life)
        {
            remove(self);
            return;
        }
        if(e >= r.a) { DW_HellRing(self.origin, self.lip - 9, self.lip + 9, r.dmg); }
        return;
    }
    // DW_HK_PULSE: its 24 sectors (a chain through `enemy`) moved to its radius each tick, its ring swept for players
    entity sct;
    if(!self.lip)
    {
        self.lip = 1;
        self.count = r.a;
        for(i = 23; i >= 0; i--)
        {
            sct = DW_HellPiece(self.origin + DW_Yaw(i * 15) * r.a);
            sct.angles = '0 1 0' * (i * 15);
            sct.alpha = 0.3;
            sct.enemy = self.enemy;
            self.enemy = sct;
        }
        return;
    }
    float total = 0.4 + r.n * (2 * r.cycle + 2 * (r.b - r.a) / r.speed);
    float now = DW_PulseRadius(e, r.a, r.b, r.cycle, r.speed, r.n);
    float next = DW_PulseRadius(e + 0.1, r.a, r.b, r.cycle, r.speed, r.n);
    float seen = e < 0.4 ? min(1, 0.3 + e * 1.75) : 1;
    if(e >= total) { seen = 0.9 - 3 * (e - total); }
    sct = self.enemy;
    i = 0;
    while(sct)
    {
        entity nx = sct.enemy;
        if(seen < 0.1) { remove(sct); }
        else
        {
            sct.velocity = (self.origin + DW_Yaw(i * 15) * next - sct.origin) * 10;
            sct.alpha = seen;
        }
        sct = nx;
        i = i + 1;
    }
    if(seen < 0.1)
    {
        remove(self);
        return;
    }
    if(e >= 0.3 && e < total) { DW_HellRing(self.origin, min(self.count, now) - 17, max(self.count, now) + 17, r.dmg); }
    self.count = now;
};

// Row `row`'s task(s) from the emitter self: one per living player for lines and barrages (aimed at him), else one.
void(float row) DW_HellStart =
{
    dw_hell_row r = DW_HELL[row];
    float each = r.kind == DW_HK_LINE || r.kind == DW_HK_TWOLINES || r.kind == DW_HK_BARRAGE;
    entity p = each ? find(world, classname, "player") : world;
    while(TRUE)
    {
        if(!each || (p.health > 0 && p.deadflag == DEAD_NO))
        {
            entity t = spawn();
            t.classname = "dw_helltask";
            t.owner = self;
            t.dw_row = row;
            setorigin(t, self.origin);
            t.enemy = p;
            t.oldorigin = p.origin;
            t.cnt = r.n;
            t.count = r.a;
            if(r.kind == DW_HK_SPIRAL) // (its steps round from a to b, Dwell's count)
            {
                t.cnt = r.flag ? (r.b > r.a ? r.b - r.a : 48 - r.a + r.b) : (r.a < r.b ? r.a + 48 - r.b : r.a - r.b);
            }
            t.dw_r0 = time + r.delay;
            t.think = dw_hell_tick;
            t.nextthink = time + r.delay;
        }
        if(!each) { return; }
        p = find(p, classname, "player");
        if(!p) { return; }
    }
};

void() dw_bullethell_use =
{
    float s = self.style ? self.style : 1 + floor(random() * 6.99);
    float i;
    for(i = 0; i < DW_HELL_N; i++)
    {
        dw_hell_row r = DW_HELL[i];
        if(r.style == s) { DW_HellStart(i); }
    }
};

/*dw:QUAKED target_bullethell (.5 .5 .5) (-8 -8 -8) (8 8 8)
Dwell: used, a bullet-hell pattern out from it.
style: 0 random (1-7), 1 a line at each player, 2 two lines, 3 ring, 4 two spirals, 5 moving wheel, 6 rocket barrage, 7 pulse, 8 Dwell's program A
*/
void() dw_target_bullethell =
{
    if(!DW_Data()) { remove(self); return; }
    precache_model(DW_HELL_MDL);
    precache_model("progs/e2/hell_rocket.mdl");
    precache_model("progs/e2/marker.mdl");
    self.use = dw_bullethell_use;
};
