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
// Dawn of the Machine's infected (MG3.md, M3-15), adapted from the snapshot's monsters/mg3_infected_generic.qc,
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
//
// Generic (ROUND23.md, "Arena wave editor: generic infection"): .vr_mg3_infected is a row of VR_INFECT_ROWS, any monster
// that can carry the mark rises as that row's monster. The rows zombie and fiend are Dawn of the Machine's own, set up by
// hand as they always were; the others go through their monster's own spawn function, so a new row is one line. What
// falls as he bursts: an armed humanoid's held and stowed weapons as at any death (VR_AG_Dying), his spare rounds and his
// worn pack as at any death (VR_AG_SoldierDrop), his armour's plates as at a gibbing (VR_AgArmor_Shed). Room: the
// outcome's own box (its hull), then a row's `fallback` (a zombie, the smallest body) with its box; the fiend and the
// zombie then the old body's box, as ever; none fits: he does not rise (gibbed and counted, as upstream).
// Counting: his first death never counts; in the arena he stays the same foe (.ar_slot kept, health > 0 at once), so a
// wave waits for the riser and pays once, for the riser's fall. A riser with no room falls at once and is paid there.

// .vr_mg3_infected (combat.qc): a row of VR_INFECT_ROWS, what he becomes when first killed (0: not infected, or turned)
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

void() VR_Infect_PrecacheDog =
{
    precache_model("progs/h_dog.mdl");
    precache_model("progs/dog.mdl");
    precache_sound("dog/dattack1.wav");
    precache_sound("dog/ddeath.wav");
    precache_sound("dog/dpain1.wav");
    precache_sound("dog/dsight.wav");
    precache_sound("dog/idle.wav");
};

void() VR_Infect_PrecacheKnight =
{
    precache_model("progs/knight.mdl");
    precache_model("progs/h_knight.mdl");
    precache_sound("knight/kdeath.wav");
    precache_sound("knight/khurt.wav");
    precache_sound("knight/ksight.wav");
    precache_sound("knight/sword1.wav");
    precache_sound("knight/sword2.wav");
    precache_sound("knight/idle.wav");
};

void() VR_Infect_PrecacheHKnight =
{
    precache_model("progs/hknight.mdl");
    precache_model("progs/k_spike.mdl");
    precache_model("progs/h_hellkn.mdl");
    precache_sound("hknight/attack1.wav");
    precache_sound("hknight/death1.wav");
    precache_sound("hknight/pain1.wav");
    precache_sound("hknight/sight1.wav");
    precache_sound("hknight/hit.wav");
    precache_sound("hknight/slash1.wav");
    precache_sound("hknight/idle.wav");
    precache_sound("hknight/grunt.wav");
    precache_sound("knight/sword1.wav");
    precache_sound("knight/sword2.wav");
};

void() VR_Infect_PrecacheShalrath =
{
    precache_model("progs/shalrath.mdl");
    precache_model("progs/h_shal.mdl");
    precache_model("progs/v_spike.mdl");
    precache_sound("shalrath/attack.wav");
    precache_sound("shalrath/attack2.wav");
    precache_sound("shalrath/death.wav");
    precache_sound("shalrath/idle.wav");
    precache_sound("shalrath/pain.wav");
    precache_sound("shalrath/sight.wav");
};

// After the change: an enemy of the class he now is (infighting he was drawn into) gives way to a living player
// (upstream's infected_set_enemy), and that enemy's grudge against him too.
void(string xClass) MG3_Infected_SetEnemy =
{
    if(!self.enemy || self.enemy.classname != xClass)
    {
        return;
    }
    entity f = VR_Player();
    while(f != world && f.health <= 0)
    {
        f = VR_NextPlayer(f);
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

// Room for his new body at `xAt` (upstream's walkmove(0, 0); here as on partial ground, so a body the blow threw into the
// air or knocked off a step is not taken for wedged: it falls as any monster does), else up to 24 units higher (a body
// lying on a step, or on the gibs), else 24 units to a side (crowded: a monster over a body knocked down, or pushed
// against him). FALSE: none (he is left at `xAt`).
float(vector xAt) MG3_Infected_Room =
{
    float oldFlags = self.flags;
    float side, lift;
    for(side = 0; side < 5; side = side + 1)
    {
        vector at = xAt;
        if(side == 1) { at_x = at_x + 24; }
        else if(side == 2) { at_x = at_x - 24; }
        else if(side == 3) { at_y = at_y + 24; }
        else if(side == 4) { at_y = at_y - 24; }
        for(lift = 0; lift < 4; lift = lift + 1)
        {
            setorigin(self, at + '0 0 8' * lift);
            self.flags = self.flags | FL_ONGROUND | FL_PARTIALGROUND;
            float room = walkmove(0, 0);
            self.flags = oldFlags;
            if(room)
            {
                return TRUE;
            }
        }
    }
    setorigin(self, xAt);
    return FALSE;
};

// His weapon falls as the stock monster's does (army_die, knight_die, enf_die, hknight_die: never run for him, the turn
// takes their place): the grunt's gun, the knight's or death knight's sword, the enforcer's rifle. The zombie or fiend he
// becomes is drawn with its own model, which holds none.
float vr_mg3_infected_drops; // the weapons infected dropped as they turned (the tests)

void() MG3_Infected_DropWeapon =
{
    float before = vr_mg3_infected_drops;
    if(self.vr_ag)
    {
        // An armed humanoid: all he has goes as at any death of his (see the file's head), then nothing of the armed
        // layer stays on the new body.
        VR_AG_Dying();
        if(self.vr_ag_kind < 2 || self.vr_wp_had) { VR_AG_SoldierDrop(); } // (the gun holders: VR_AM_GUN_KINDS)
        VR_AgArmor_Shed(self, self.health);
        self.vr_ag = 0;
        self.vr_agr = 0;
        vr_mg3_infected_drops++;
    }
    else if(self.classname == "monster_army")
    {
        VR_DropEnemyGun(WID_GRUNTGUN);
        vr_mg3_infected_drops++;
    }
    else if(self.classname == "monster_enforcer")
    {
        VR_DropEnemyGun(WID_ENFRIFLE);
        vr_mg3_infected_drops++;
    }
    else if((self.classname == "monster_knight" || self.classname == "monster_hell_knight") &&
            !(!VR_OtherFlags() && (self.spawnflags & 2))) // (a statue has none: knight_die's test)
    {
        VR_DropKnightSword(self.classname == "monster_hell_knight");
        vr_mg3_infected_drops++;
    }
    if(vr_mg3_infected_drops > before)
    {
        dprint(sprintf("mg3: the infected %s drops his weapon as he turns\n", self.classname));
    }
};

// Dawn of the Machine's two, by hand (as they always were: their first frames are their pain's).
void() MG3_Infected_BecomeFiend =
{
    setmodel(self, "progs/demon.mdl");
    setsize(self, '-16 -16 -24', '16 16 24'); // (monster_demon1's)
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
};

void() MG3_Infected_BecomeZombie =
{
    setmodel(self, "progs/zombie.mdl");
    setsize(self, '-12 -12 -24', '12 12 24'); // (monster_zombie's)
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
};

// The outcomes. Its `name` the presets' word and the toolgun's choice, `classname` what he is then, `precache` what he
// needs (the map's start for a preset's list; the toolgun's spawn for its choice: the engine's late precache, as all its
// spawn list), `become` his monster's own spawn function (rows 1 and 2: the ones above), `rise` the sound he rises with
// ("": his first frames' own), `fallback` the row to try when he doesn't fit (0: he doesn't rise).
typedef struct
{
    string name, classname;
    void() precache, become;
    string rise;
    float fallback;
} vr_infect_row;

vr_infect_row VR_INFECT_ROWS[] =
{
    { "none", "", SUB_Null, SUB_Null, "", 0 },
    { "zombie", "monster_zombie", MG3_Infected_PrecacheZombie, MG3_Infected_BecomeZombie, "", 0 },
    { "demon", "monster_demon1", MG3_Infected_PrecacheFiend, MG3_Infected_BecomeFiend, "", 0 },
    { "dog", "monster_dog", VR_Infect_PrecacheDog, monster_dog, "dog/dsight.wav", MG3_INFECTED_ZOMBIE },
    { "knight", "monster_knight", VR_Infect_PrecacheKnight, monster_knight, "knight/ksight.wav", MG3_INFECTED_ZOMBIE },
    { "ogre", "monster_ogre", monster_ogre_precache, monster_ogre, "ogre/ogwake.wav", MG3_INFECTED_ZOMBIE },
    { "hellknight", "monster_hell_knight", VR_Infect_PrecacheHKnight, monster_hell_knight, "hknight/sight1.wav", MG3_INFECTED_ZOMBIE },
    { "shambler", "monster_shambler", monster_shambler_precache, monster_shambler, "shambler/ssight.wav", MG3_INFECTED_ZOMBIE },
    { "shalrath", "monster_shalrath", VR_Infect_PrecacheShalrath, monster_shalrath, "shalrath/sight.wav", MG3_INFECTED_ZOMBIE },
    { "wizard", "monster_wizard", monster_wizard_precache, monster_wizard, "wizard/wsight.wav", MG3_INFECTED_ZOMBIE }
};
#define VR_INFECT_N 10

// A word's row (presets), -1: none such.
float(string xName) VR_Infect_ByName =
{
    for(float i = 0; i < VR_INFECT_N; i++)
    {
        if(VR_INFECT_ROWS[i].name == xName) { return i; }
    }
    return -1;
};

// What row `xRow` can come to need: its own, and its fallback's.
void(float xRow) VR_Infect_Precache =
{
    if(xRow <= 0 || xRow >= VR_INFECT_N) { return; }
    void() f = VR_INFECT_ROWS[xRow].precache;
    f();
    if(VR_INFECT_ROWS[xRow].fallback) { VR_Infect_Precache(VR_INFECT_ROWS[xRow].fallback); }
    precache_sound("player/udeath.wav");
};

// The engine's lists (vr_toolgun_panel.cpp, vr_waveeditor.cpp).
float() VR_Infect_UiCount = { return VR_INFECT_N; };
string(float xRow) VR_Infect_UiName = { return VR_INFECT_ROWS[max(0, min(VR_INFECT_N - 1, xRow))].name; };

// The toolgun's armed humanoid just made (vr_toolgun_pick.inc): he carries row `xRow` (precached now: the toolgun's
// spawns are the engine's late precaches, which reach the client before they are used).
void(entity e, float xRow) VR_Infect_Toolgun =
{
    xRow = max(0, min(VR_INFECT_N - 1, floor(xRow)));
    e.vr_mg3_infected = xRow;
    VR_Infect_Precache(xRow);
    if(xRow) { dprint(sprintf("infect: toolgun: %s infected with %s\n", e.classname, VR_INFECT_ROWS[xRow].name)); }
};

// self made row `xRow`'s monster (a fallback's after another's: all of it again).
void(float xRow) VR_Infect_Become =
{
    self.classname = VR_INFECT_ROWS[xRow].classname;
    self.th_melee = world.th_melee;
    self.th_missile = world.th_missile;
    self.th_vrshove = world.th_vrshove; // (the grunt's and the enforcer's shove: vr_enemyshove.qc)
    void() f = VR_INFECT_ROWS[xRow].become;
    if(xRow == MG3_INFECTED_ZOMBIE || xRow == MG3_INFECTED_FIEND)
    {
        f();
    }
    else
    {
        // Its own spawn function, as if placed here: never replaced by its armed version, nor a second count; its start
        // (walkmonster_start's think) never runs: he comes to below, at once.
        float monsters = total_monsters;
        vr_agr_hold = vr_agr_hold + 1;
        f();
        vr_agr_hold = vr_agr_hold - 1;
        total_monsters = monsters;
        self.think = SUB_Null;
        self.nextthink = 0;
        self.max_health = self.health;
        self.takedamage = DAMAGE_AIM;
        self.flags = self.flags | FL_MONSTER;
        self.view_ofs = '0 0 25';
        self.use = monster_use;
        if(!self.yaw_speed) { self.yaw_speed = 20; }
    }
};

// Killed's turn (combat.qc Killed, before it counts or does any of Quake VR's dying): the burst, then the new body.
void(entity attacker) MG3_Infected_Turn =
{
    float kind = self.vr_mg3_infected;
    float wasDown = self.vr_knockdown != 0; // (knocked down: his origin his ragdoll's pelvis, on the floor)
    vector oldMins = wasDown ? self.vr_kd_mins : self.mins; // (the box he stood in: knocked down, the one he had standing)
    vector oldMaxs = wasDown ? self.vr_kd_maxs : self.maxs;
    self.vr_mg3_infected = 0;
    self.enemy = attacker;
    vr_mg3_infected_turned = vr_mg3_infected_turned + 1;
    MG3_Infected_DropWeapon(); // (before the new body: his class and spawnflags still the old monster's)

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

    // The new body: the row's box first, then its fallback's (a zombie); knocked down, each stands up where his body
    // lay: its box on the floor under him. Rows 1 and 2 then try the old body's box, as ever (a fiend is wider than a
    // death knight; monsters crowd), where it stood.
    vector at = self.origin;
    float row = kind;
    float room = FALSE;
    while(row > 0 && row < VR_INFECT_N)
    {
        VR_Infect_Become(row);
        setorigin(self, at);
        MG3_Infected_SetEnemy(self.classname);
        if(wasDown)
        {
            traceline(self.origin + '0 0 8', self.origin - '0 0 64', TRUE, self);
            if(!trace_startsolid)
            {
                setorigin(self, trace_endpos + '0 0 1' * (1 - self.mins_z));
            }
        }
        room = MG3_Infected_Room(self.origin);
        if(!room && oldMaxs_x > 0 && (row == MG3_INFECTED_ZOMBIE || row == MG3_INFECTED_FIEND))
        {
            vector newMins = self.mins, newMaxs = self.maxs;
            setsize(self, oldMins, oldMaxs);
            room = MG3_Infected_Room(self.origin);
            if(!room)
            {
                setsize(self, newMins, newMaxs);
            }
        }
        if(room || !VR_INFECT_ROWS[row].fallback)
        {
            break;
        }
        row = VR_INFECT_ROWS[row].fallback;
    }
    if(!room)
    {
        // Wedged: no room for the new body. It dies at once, gibbed, and is counted (upstream's edge case).
        self.health = -100;
        Killed(self, attacker);
        return;
    }
    if(row == MG3_INFECTED_FIEND)
    {
        demon1_pain1();
    }
    else if(row == MG3_INFECTED_ZOMBIE)
    {
        zombie_paina1();
    }
    else
    {
        // (his first step his run: a pain's flinch is a matter of chance for these)
        if(VR_INFECT_ROWS[row].rise != "") { sound(self, CHAN_VOICE, VR_INFECT_ROWS[row].rise, 1, ATTN_NORM); }
        self.th_run();
    }
    dprint(sprintf("infect: rose as %s (row %g of %g)\n", self.classname, row, kind));
};

void() MG3_Infected_Mark =
{
    VR_Infect_Precache(self.vr_mg3_infected);
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
