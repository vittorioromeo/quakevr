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
// Dawn of the Machine's sacrifices (MG3.md M3-20), adapted from quakec_mg3/monsters/mg3_sacrifice.qc and
// mg3_sacrifice_triggers.qc: misc_sacrifice (a hanging victim, gibbed when used or killed: its targets fired) and the
// trigger_sacrifice_counter its relays count (map8: 8 victims fire the rune's trigger), with trigger_check_sacrifices
// (says how many are left when touched). Their models are read in place from the owned pack (owned/mg3/), any campaign.

float mg3_sacrifices; // victims gibbed (the tests' count)

float() MG3_Sacrifice_Available =
{
    return fileexists("owned/mg3/progs/player_hanging.mdl") && fileexists("owned/mg3/progs/player_hanging_animated.mdl");
};

// Gibbed: by its use (`xTargets`: it fires its targets) or killed (Killed's monster_death_use has fired them: upstream
// fired them twice for a victim shot down, counting it twice on map8's counter).
void(float xTargets) MG3_Sacrifice_Gib =
{
    if(self.model == "")
        return; // (gibbed already: used and killed in one frame)
    self.origin = self.origin + '0 0 -32';
    ThrowGib("progs/gib1.mdl", -10);
    ThrowGib("progs/gib2.mdl", -10);
    ThrowGib("progs/gib3.mdl", -10);
    if(random() < 0.5)
        sound(self, CHAN_VOICE, "player/gib.wav", 1, ATTN_NONE);
    else
        sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NONE);
    mg3_sacrifices = mg3_sacrifices + 1;
    if(xTargets)
        SUB_UseTargets();
    remove(self);
};
void() sacrifice_gib = { MG3_Sacrifice_Gib(TRUE); };
void() sacrifice_die = { MG3_Sacrifice_Gib(FALSE); };

// The animated victim's sway: frames 5..75 at ten a second.
void() sacrifice_animate =
{
    self.count = self.count + 1;
    if(self.count > 75)
        self.count = 5;
    self.frame = self.count;
    self.nextthink = time + 0.1;
};

// The still victim's bob (16 units, a 4 s period: upstream's cos takes degrees) and turn (36 degrees a second).
void() sacrifice_think =
{
    self.count = self.count + 0.1;
    setorigin(self, self.dest + cos(self.count * 90 * 0.0174533) * '0 0 16');
    self.angles = self.angles + '0 36 0' * 0.1;
    self.nextthink = time + 0.1;
};

/*QUAKED misc_sacrifice (1 0 0) (-16 -16 -56) (16 16 0) CANNOT_BE_KILLED STILL_BOBBING
Dawn of the Machine's hanging sacrifice (needs the owned Dawn of the Machine data, else nothing spawns): gibbed when used or killed (100 health), firing its targets. CANNOT_BE_KILLED (1): only using it gibs it. STILL_BOBBING (2): a still body that bobs and turns, else one swaying.
*/
void() misc_sacrifice =
{
    if(MG3_RuneRemoved()) return;
    if(!VR_Pack_RequireSpawn(MG3_Sacrifice_Available(), "Dawn of the Machine")) return;
    precache_sound("player/gib.wav");
    precache_sound("player/udeath.wav");
    precache_model("owned/mg3/progs/player_hanging.mdl");
    precache_model("owned/mg3/progs/player_hanging_animated.mdl");
    self.use = sacrifice_gib;
    self.th_die = sacrifice_die;
    self.health = 100;
    self.solid = SOLID_SLIDEBOX;
    if(self.spawnflags & 1) // cannot be killed
        self.takedamage = DAMAGE_NO;
    else
        self.takedamage = DAMAGE_AIM;
    self.movetype = MOVETYPE_STEP;
    if(self.spawnflags & 2) // bouncing
    {
        setmodel(self, "owned/mg3/progs/player_hanging.mdl");
        self.count = 0;
        self.think = sacrifice_think;
        self.nextthink = time + 0.1;
        self.avelocity = '0 36 0';
        self.dest = self.origin;
    }
    else
    {
        setmodel(self, "owned/mg3/progs/player_hanging_animated.mdl");
        self.count = 5 + floor(random() * 65 + 0.5);
        self.frame = self.count;
        self.think = sacrifice_animate;
        self.nextthink = time + 0.1;
    }
    setsize(self, VEC_HULL_MIN - '0 0 32', VEC_HULL_MAX - '0 0 32');
};

// "N more" to every player (upstream's localized lines; more than 8: "more to go").
void(entity counter) get_sacrifice_count =
{
    if(counter.count <= 0)
        return;
    if(counter.count == 1) MG_PrintAll("$mg3_qc_sacricie_count_1_more");
    else if(counter.count == 2) MG_PrintAll("$mg3_qc_sacricie_count_2_more");
    else if(counter.count == 3) MG_PrintAll("$mg3_qc_sacricie_count_3_more");
    else if(counter.count == 4) MG_PrintAll("$mg3_qc_sacricie_count_4_more");
    else if(counter.count == 5) MG_PrintAll("$mg3_qc_sacricie_count_5_more");
    else if(counter.count == 6) MG_PrintAll("$mg3_qc_sacricie_count_6_more");
    else if(counter.count == 7) MG_PrintAll("$mg3_qc_sacricie_count_7_more");
    else if(counter.count == 8) MG_PrintAll("$mg3_qc_sacricie_count_8_more");
    else MG_PrintAll("$mg3_qc_sacricie_count_more");
};

void() sacrifice_counter_use =
{
    self.count = self.count - 1;
    if(self.count < 0)
        return;

    if(self.count != 0)
    {
        if(activator.classname == "player" && (self.spawnflags & SPAWNFLAG_NOMESSAGE) == 0)
            get_sacrifice_count(self);
        return;
    }

    if(activator.classname == "player" && (self.spawnflags & SPAWNFLAG_NOMESSAGE) == 0)
        MG_PrintAll("$mg3_qc_sacricie_count_complete");
    self.enemy = activator;
    multi_trigger();
};

/*QUAKED trigger_sacrifice_counter (.5 .5 .5) (-8 -8 -8) (8 8 8) NOMESSAGE
Dawn of the Machine's sacrifice counter: after it is used 'count' times (default 2) it fires its targets once. Unless NOMESSAGE (1), every player is told how many sacrifices are left, and when they are done.
*/
void() trigger_sacrifice_counter =
{
    if(!self.count)
        self.count = 2;
    if(!self.wait)
        self.wait = self.count;
    self.use = sacrifice_counter_use;
};

void() check_sacrifices_think =
{
    self.wait = 0; // (touchable again)
};

void() check_sacrifices_touch =
{
    if(other.classname != "player" || other.health <= 0)
        return;
    if(self.wait == 0)
    {
        self.wait = 1; // (not again until 0.2 s after the last touch)
        entity f = find(world, targetname, self.target);
        if(f)
            get_sacrifice_count(f);
    }
    self.think = check_sacrifices_think;
    self.nextthink = time + 0.2;
};

/*QUAKED trigger_check_sacrifices (.5 .5 .5) ?
Dawn of the Machine: touched by a player, tells everyone how many sacrifices its 'target' (a trigger_sacrifice_counter) still needs.
*/
void() trigger_check_sacrifices =
{
    InitTrigger();
    self.touch = check_sacrifices_touch;
    self.wait = 0;
};
