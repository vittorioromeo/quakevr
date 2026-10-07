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
// Dawn of the Machine's lava man (MG3_PLAN.md M3-21), adapted from quakec_mg3/monsters/mg3_lavaman.qc ("pulled directly
// from rogue, and then a few tweaks") and its combat.qc rule: Rogue's lava man (rogue_lavaman.qc) with .mg3_lavaman's
// branches. It stays at the lava's surface (it flies: no drop to the floor), rises with a splash in the lava below it when
// its trigger fires (a map's one with a targetname; else at once), stands as it throws its two lava balls (from lower
// hands), staggers at its first hit (with Chthon's pain cry), takes 0.8 of anything but the lightning gun's bolt and the
// laser cannon's (nothing from Chthon), dies when used again (counted, its targets fired), sinks without a blast. Its
// model is MG3's, read in place (owned/mg3/progs/lavaman.mdl): no Dissolution of Eternity needed, any campaign.

float() MG3_LavaMan_Available =
{
    return fileexists("owned/mg3/progs/lavaman.mdl") && fileexists("progs/lavaball.mdl");
};

// Used again once risen: it dies at once (upstream lavaman_force_death: counted here, its targets fired).
void() lavaman_force_death =
{
    if(self.health <= 0)
        return;
    self.takedamage = DAMAGE_NO;
    self.health = 0;
    killed_monsters = killed_monsters + 1;
    WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
    SUB_UseTargets();
    self.use = SUB_Null;
    lavaman_death1();
};

// T_Damage's (combat.qc): 0.8 of a blow unless it is the lightning gun's bolt (a shot of a hand holding it) or a laser
// cannon's bolt; Chthon's lava balls do nothing (upstream: the attacker's selected weapon; Quake VR asks what struck).
float(entity targ, entity inflictor, entity attacker, float damage) MG3_LavaManDamage =
{
    if(attacker.classname == "monster_boss")
        return 0;
    if(inflictor.classname == "hiplaser")
        return damage;
    if(attacker.classname == "player" && vr_hitkind == QVR_HITKIND_SHOT && vr_hithand >= 0 &&
       VRGetEntWeapon(attacker, vr_hithand) == WID_LIGHTNING)
        return damage;
    return damage * 0.8;
};

// monster_lava_man's (rogue_lavaman.qc) for a lava man of MG3's.
void() MG3_LavaMan_Spawn =
{
    if(MG_MonsterInhibited()) return;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    if(!VR_Pack_RequireSpawn(MG3_LavaMan_Available(), "Dawn of the Machine")) return;

    precache_model("owned/mg3/progs/lavaman.mdl");
    precache_model("progs/lavaball.mdl");
    precache_sound("weapons/rocket1i.wav");
    precache_sound("boss1/out1.wav");
    precache_sound("boss1/sight1.wav");
    precache_sound("misc/power.wav");
    precache_sound("boss1/throw.wav");
    precache_sound("boss1/pain.wav");
    precache_sound("boss1/death.wav");

    self.classname = "monster_lava_man";
    self.mg3_lavaman = TRUE;
    monster_add_to_counter();

    // It rises from the lava when its trigger fires; one nothing names rises at once.
    if(self.targetname != "")
    {
        self.use = lavaman_awake;
        return;
    }
    lavaman_awake();
};

/*QUAKED monster_lava_man_mg3 (1 0 0) (-32 -32 -24) (32 32 64) Ambush
Dawn of the Machine's lava man (needs the owned Dawn of the Machine data, else nothing spawns; in Dawn of the Machine's maps every monster_lava_man is this one): it stays at the lava's surface, rises when its trigger fires (with a targetname; else at once), throws lava balls standing, takes 0.8 of anything but the lightning gun and the laser cannon, and dies when used again.
*/
void() monster_lava_man_mg3 =
{
    self.mg3_lavaman = TRUE;
    MG3_LavaMan_Spawn();
};
