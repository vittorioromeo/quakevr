/*  Copyright (C) 1996-2026 id Software LLC

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
// Dawn of the Machine's infected (MG3_PLAN.md, M3-15), adapted from the snapshot's monsters/mg3_infected_generic.qc,
// mg3_soldier_infected.qc, mg3_knight_infected.qc, mg3_enforcer_infected.qc, mg3_hknight_infected.qc and combat.qc
// (docs/vr-port/OFFICIAL_QC_SOURCE.md). Stock models only, so they are there in any campaign (the agglomeration
// principle: Debug > Tests > Ahead of You, 30..33).
//
// An infected grunt or knight is the stock monster (its classname the stock one's, as upstream: no infighting, and every
// Quake VR table keyed on the class is his: head zone, beheading, ragdoll settings, hulls, parry, grapple), marked
// .vr_mg3_infected. Killed the first time, he bursts (the gib sound, his gibs; never beheaded, never a ragdoll or a
// corpse: Killed hands him here before any of that) and gets up as a zombie; an infected enforcer or death knight gets
// up as a fiend. Upstream swaps only the modelindex (its .model and box stay the grunt's); here he becomes the zombie or
// fiend through and through (setmodel: the engine's ragdoll rig, limb gore, hit tests and hull all go by the model and
// class he has now; his box the zombie's or fiend's), with the zombie's or fiend's knockdown. Counted once: his first
// death not at all (upstream's .infected), the zombie's or fiend's as any monster's. Wedged where he stands (no room for
// the new body), he dies at once as upstream, gibbed and counted.
//
// An infected death knight placed as a corpse (spawnflags 65536 or 8388608: MG3's CORPSE_FLAG_A, _B) lies in his death's
// last frame, not solid, until he is woken (his targetname used: monster_use), then rises (his death backwards; tried
// again every 5 s while something stands in his way). Alive all the while: no ragdoll, no corpse (his health is his).

// .vr_mg3_infected (combat.qc): MG3_INFECTED_*, what he becomes when first killed (0: not infected, or already turned)
float vr_mg3_infected_turned;   // infected turned since the map started (the tests)

float MG3_INFECTED_ZOMBIE = 1;  // a grunt or a knight: a zombie
float MG3_INFECTED_FIEND = 2;   // an enforcer or a death knight: a fiend

float MG3_CORPSE_A = 65536;     // (MG3's CORPSE_FLAG_A: lying in $death12, rises through $death11..1)
float MG3_CORPSE_B = 8388608;   // (CORPSE_FLAG_B: lying in $deathb9, rises through $deathb8..1)

// The zombie's and the fiend's frames these need (orig_mon_zombie.qc, orig_mon_demon.qc, orig_mon_hknight.qc $frame).
float MG3_ZOMBIE_PAINE12 = 173;
float MG3_DEMON_DEATH1 = 45;
float MG3_DEMON_DEATH9 = 53;
float MG3_HKNIGHT_DEATH1 = 42;
float MG3_HKNIGHT_DEATHB1 = 54;

// The sound a rising death knight makes: MG3's own (owned/mg3: vr_gamedir.cpp VR_OwnedFile), else his sight sound.
string() MG3_Infected_RiseSound =
{
    return VR_SoundOr("owned/mg3/infected/death1_rev.wav", "hknight/sight1.wav", "sound/owned/mg3/infected/death1_rev.wav");
};

void() MG3_Infected_PrecacheZombie =
{
    precache_model("progs/zombie.mdl");
    precache_model("progs/h_zombie.mdl");
    precache_model("progs/zom_gib.mdl");
    precache_sound("zombie/z_idle.wav");
    precache_sound("zombie/z_idle1.wav");
    precache_sound("zombie/z_shot1.wav");
    precache_sound("zombie/z_gib.wav");
    precache_sound("zombie/z_pain.wav");
    precache_sound("zombie/z_pain1.wav");
    precache_sound("zombie/z_fall.wav");
    precache_sound("zombie/z_miss.wav");
    precache_sound("zombie/z_hit.wav");
    precache_sound("zombie/idle_w2.wav");
    precache_sound("player/udeath.wav");
};

void() MG3_Infected_PrecacheFiend =
{
    precache_model("progs/demon.mdl");
    precache_model("progs/h_demon.mdl");
    precache_sound("demon/ddeath.wav");
    precache_sound("demon/dhit2.wav");
    precache_sound("demon/djump.wav");
    precache_sound("demon/dpain1.wav");
    precache_sound("demon/idle1.wav");
    precache_sound("demon/sight2.wav");
    precache_sound("player/udeath.wav");
};

// After the change: an enemy of the class he now is (infighting he was drawn into) gives way to a living player
// (upstream's infected_set_enemy), and that enemy's grudge against him too.
void(string xClass) MG3_Infected_SetEnemy =
{
    if(!self.enemy || self.enemy.classname != xClass)
    {
        return;
    }
    entity f = find(world, classname, "player");
    while(f != world && f.health <= 0)
    {
        f = find(f, classname, "player");
    }
    if(f != world)
    {
        if(self.enemy.enemy == self)
        {
            self.enemy.enemy = f;
        }
        self.enemy = f;
    }
};

// Killed's turn (combat.qc Killed, before it counts or does any of Quake VR's dying): the burst, then the new body.
void(entity attacker) MG3_Infected_Turn =
{
    float kind = self.vr_mg3_infected;
    float wasDown = self.vr_knockdown != 0; // (knocked down: his origin his ragdoll's pelvis, on the floor)
    self.vr_mg3_infected = 0;
    self.enemy = attacker;
    vr_mg3_infected_turned = vr_mg3_infected_turned + 1;

    // The burst: the gib sound and the old body's gibs (the grunt's head among them, thrown as a gib: never a head to
    // carry, nor a beheading).
    sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
    if(self.classname == "monster_army")
    {
        ThrowGib("progs/h_guard.mdl", self.health);
    }
    ThrowGib("progs/gib1.mdl", self.health);
    ThrowGib("progs/gib2.mdl", self.health);
    ThrowGib("progs/gib3.mdl", self.health);

    // Nothing of the old body's dying carries over: an armed blow's beheading (VR_Decap_After would take the new
    // zombie's head), a knockdown's ragdoll, a cut limb, a corpse's watch.
    if(vr_decap_ent == self)
    {
        vr_decap_ent = world;
    }
    self.vr_decap_died = 0;
    self.vr_headless = 0;
    self.vr_limbcut = 0;
    self.vr_corpse = 0;
    self.vr_corpse_dying = 0;
    self.vr_knockdown = 0;
    self.deadflag = DEAD_NO;
    self.velocity = '0 0 0';
    self.avelocity = '0 0 0';

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    self.takedamage = DAMAGE_AIM;
    self.spawnflags = 0;
    self.frame = 0;
    self.skin = 0;
    self.angles_x = 0;
    self.angles_z = 0;
    self.pain_finished = time + 1;
    self.attack_finished = 0;

    if(kind == MG3_INFECTED_FIEND)
    {
        setmodel(self, "progs/demon.mdl");
        setsize(self, '-16 -16 -24', '16 16 24'); // (monster_demon1's)
        self.classname = "monster_demon1";
        self.noise = "demon/sight2.wav";
        self.health = 300;
        self.max_health = 300;
        self.th_stand = demon1_stand1;
        self.th_walk = demon1_walk1;
        self.th_run = demon1_run1;
        self.th_die = demon_die;
        self.th_melee = Demon_MeleeAttack;
        self.th_missile = demon1_jump1;
        self.th_pain = demon1_pain;
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_demon, MG3_DEMON_DEATH9, MG3_DEMON_DEATH1, -1, -1);
    }
    else
    {
        setmodel(self, "progs/zombie.mdl");
        setsize(self, '-12 -12 -24', '12 12 24'); // (monster_zombie's)
        self.classname = "monster_zombie";
        self.noise = "zombie/z_idle.wav";
        self.health = 60;
        self.max_health = 60;
        self.th_stand = zombie_stand1;
        self.th_walk = zombie_walk1;
        self.th_run = zombie_run1;
        self.th_die = zombie_die;
        self.th_missile = zombie_missile;
        self.th_melee = world.th_melee; // (none: the zombie's flesh is its missile)
        self.th_pain = zombie_pain;
        VR_Knockdown_SetupOwn(cvarh_vr_knockdown_chance_zombie, MG3_ZOMBIE_PAINE12, zombie_paine13);
    }
    self.th_vrshove = world.th_vrshove; // (the grunt's and the enforcer's shove: vr_enemyshove.qc)
    setorigin(self, self.origin);
    MG3_Infected_SetEnemy(self.classname);

    // Room for the new body where it stands (upstream's walkmove(0, 0); here as on partial ground, so a body the blow
    // threw into the air or knocked off a step is not taken for wedged: it falls as any monster does).
    // Knocked down, he stands up where his body lay: his box on the floor under him.
    if(wasDown)
    {
        traceline(self.origin + '0 0 8', self.origin - '0 0 64', TRUE, self);
        if(!trace_startsolid)
        {
            setorigin(self, trace_endpos + '0 0 1' * (1 - self.mins_z));
        }
    }
    float oldFlags = self.flags;
    float room = FALSE;
    float lift;
    for(lift = 0; lift < 4 && !room; lift = lift + 1) // (else a little higher: a body lying on a step, or on the gibs)
    {
        if(lift)
        {
            setorigin(self, self.origin + '0 0 8');
        }
        self.flags = self.flags | FL_ONGROUND | FL_PARTIALGROUND;
        room = walkmove(0, 0);
        self.flags = oldFlags;
    }
    if(!room)
    {
        // Wedged: no room for the new body. It dies at once, gibbed, and is counted (upstream's edge case).
        self.health = -100;
        Killed(self, attacker);
        return;
    }
    if(kind == MG3_INFECTED_FIEND)
    {
        demon1_pain1();
    }
    else
    {
        zombie_paina1();
    }
};

void() MG3_Infected_Mark =
{
    if(self.vr_mg3_infected == MG3_INFECTED_FIEND)
    {
        MG3_Infected_PrecacheFiend();
    }
    else
    {
        MG3_Infected_PrecacheZombie();
    }
};

/*QUAKED monster_army_infected (1 0 0) (-16 -16 -24) (16 16 40) Ambush
Dawn of the Machine: an infected grunt. 30 health, a grunt's shotgun; killed, he bursts and gets up as a zombie
(counted once, as the zombie).
*/
void() monster_army_infected =
{
    self.vr_mg3_infected = MG3_INFECTED_ZOMBIE;
    MG3_Infected_Mark();
    self.classname = "monster_army";
    monster_army();
};

/*QUAKED monster_knight_infected (1 0 0) (-16 -16 -24) (16 16 40) Ambush
Dawn of the Machine: an infected knight. 75 health, a knight's sword; killed, he bursts and gets up as a zombie
(counted once, as the zombie).
*/
void() monster_knight_infected =
{
    self.vr_mg3_infected = MG3_INFECTED_ZOMBIE;
    MG3_Infected_Mark();
    self.classname = "monster_knight";
    monster_knight();
};

/*QUAKED monster_enforcer_infected (1 0 0) (-16 -16 -24) (16 16 40) Ambush
Dawn of the Machine: an infected enforcer. 80 health, an enforcer's laser; killed, he bursts and gets up as a fiend
(counted once, as the fiend).
*/
void() monster_enforcer_infected =
{
    self.vr_mg3_infected = MG3_INFECTED_FIEND;
    MG3_Infected_Mark();
    self.classname = "monster_enforcer";
    monster_enforcer();
};

// ---- The death knight lying as a corpse until woken --------------------------------------------------------------

void() MG3_HKnight_Resurrect =
{
    self.th_pain = hknight_pain;
    self.th_run = hknight_run1;
    self.th_stand = hknight_stand1;
    ai_stand();
};

// Room to stand: not solid until there is (tried every 5 s), then the sound of his rising.
float() MG3_HKnight_TestRise =
{
    self.solid = SOLID_SLIDEBOX;
    if(!walkmove(0, 0))
    {
        self.solid = SOLID_NOT;
        return FALSE;
    }
    sound(self, CHAN_VOICE, MG3_Infected_RiseSound(), 1, ATTN_NORM);
    return TRUE;
};

// Lying (th_stand): his death's last frame, not solid, never looking about (woken only by his targetname).
void() MG3_HKnight_Lie =
{
    self.frame = (self.spawnflags & MG3_CORPSE_B) ? MG3_HKNIGHT_DEATHB1 + 8 : MG3_HKNIGHT_DEATH1 + 11;
    self.solid = SOLID_NOT;
    self.think = MG3_HKnight_Lie;
    self.nextthink = time + 9999;
};

// Rising (th_run): his death backwards, a frame a tenth of a second (death12 -> death1, or deathb9 -> deathb1), then
// stand1, stand2, and he runs. self.count the frame he is at.
void() MG3_HKnight_RiseStep =
{
    float first = (self.spawnflags & MG3_CORPSE_B) ? MG3_HKNIGHT_DEATHB1 : MG3_HKNIGHT_DEATH1;
    self.nextthink = time + 0.1;
    self.think = MG3_HKnight_RiseStep;
    if(self.frame > first)
    {
        self.frame = self.frame - 1;
        if(!(self.spawnflags & MG3_CORPSE_B))
        {
            // (upstream's steps back: death9 -11, death8 -10, death3 -7, death2 -8)
            float d = self.frame - MG3_HKNIGHT_DEATH1 + 1;
            if(d == 9) { ai_forward(-11); }
            else if(d == 8) { ai_forward(-10); }
            else if(d == 3) { ai_forward(-7); }
            else if(d == 2) { ai_forward(-8); }
        }
        return;
    }
    if(self.frame == first)
    {
        self.frame = 0; // $stand1
        return;
    }
    self.frame = 1; // $stand2
    if(!(self.spawnflags & MG3_CORPSE_B))
    {
        ai_forward(-10);
    }
    self.spawnflags = self.spawnflags - (self.spawnflags & (MG3_CORPSE_A | MG3_CORPSE_B));
    self.think = hknight_run1;
    MG3_HKnight_Resurrect();
};

void() MG3_HKnight_Rise =
{
    if(!MG3_HKnight_TestRise())
    {
        self.think = MG3_HKnight_Rise;
        self.nextthink = time + 5;
        return;
    }
    self.th_run = MG3_HKnight_RiseStep; // (rising: woken again or hurt, it goes on rising: HuntTarget thinks th_run)
    self.th_stand = MG3_HKnight_RiseStep;
    self.pain_finished = time + 1.5;
    MG3_HKnight_RiseStep();
};

/*QUAKED monster_hell_knight_infected (1 0 0) (-16 -16 -24) (16 16 40) Ambush
Dawn of the Machine: an infected death knight. 250 health, a death knight's sword and flames; killed, he bursts and
gets up as a fiend (counted once, as the fiend). With spawnflags 65536 (MG3's corpse A) or 8388608 (corpse B) he
lies as a corpse, not solid, until his targetname wakes him: then he rises (his death backwards) and fights.
*/
void() monster_hell_knight_infected =
{
    self.vr_mg3_infected = MG3_INFECTED_FIEND;
    MG3_Infected_Mark();
    self.classname = "monster_hell_knight";
    float corpse = self.spawnflags & (MG3_CORPSE_A | MG3_CORPSE_B);
    monster_hell_knight();
    if(!self.model || !corpse)
    {
        return; // (removed, or an ordinary one)
    }
    precache_sound(MG3_Infected_RiseSound());
    self.th_stand = MG3_HKnight_Lie;
    self.th_run = MG3_HKnight_Rise;
    self.th_walk = MG3_HKnight_Lie;
    self.th_pain = SUB_NullPain;
};
