/*  Copyright (C) 1996-2022 id Software LLC

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
// Adapted from official monsters.qc/client.qc/subs.qc; retain VR archetypes and frame functions.
.float MG_deferred;
.string MG_monster_model;
.vector MG_monster_mins;
.vector MG_monster_maxs;
.float MG_monster_solid;
.float MG_monster_move;
.void() MG_monster_start;
.entity MG_monster_activator;
float() MG_MonsterInhibited = {
    if(!MG_WorldCampaign()) return FALSE;
    if(MG3_RuneRemoved()) return TRUE; // Dawn of the Machine: not with this many runes (vr_mg3_defs.qc)
    if((!coop && (self.spawnflags & 32768)) || (coop && (self.spawnflags & 131072))) {
        remove(self); return TRUE;
    }
    return FALSE;
};
void() MG_MonsterBeginWalking = {
    if(self.health <= 0 || self.deadflag) return; // QVR: killed while it stood paused: its trigger never raises the body
    self.use = monster_use;
    self.pausetime = 0;
    self.th_walk();
};
// A monster with a path that starts paused (spawnflags 4096, MG maps): it stands until used, then walks its path.
// TRUE when it paused (monsters.qc's walk, fly and swim starts walk it otherwise).
float() MG_MonsterStartPaused = {
    if(!(MG_WorldCampaign() && (self.spawnflags & 4096))) return FALSE;
    self.pausetime = 99999999;
    self.th_stand();
    self.use = MG_MonsterBeginWalking;
    return TRUE;
};
void() MG_MonsterRunStart = {
    entity oldact = activator;
    activator = self.MG_monster_activator;
    self.MG_monster_start();
    if(MGH_Active()) MGH_MarkMonster();
    if((self.spawnflags & 4) && (self.spawnflags & 8)) monster_use();
    activator = oldact;
};
void() MG_MonsterActivate = {
    if(!self.MG_deferred) return;
    self.MG_deferred = FALSE;
    self.solid = self.MG_monster_solid;
    self.movetype = self.MG_monster_move;
    setmodel(self, self.MG_monster_model);
    setsize(self, self.MG_monster_mins, self.MG_monster_maxs);
    self.flags = self.flags | FL_MONSTER;
    self.MG_monster_activator = activator;
    // Spawn telefrag before making the monster damageable; telefrag deaths still use VR kill handling.
    spawn_tdeath(self.origin, self);
    if(self.spawnflags & 16) spawn_tfog(self.origin);
    self.think = MG_MonsterRunStart;
    self.nextthink = time + 0.1;
};
float(void() start) MG_MonsterPrepare = {
    if(MG3_RuneRemoved()) return TRUE; // Dawn of the Machine: not with this many runes (every monster's start)
    if(!MG_WorldCampaign() || !(self.spawnflags & 4)) return FALSE;
    self.MG_monster_start = start;
    self.MG_monster_model = self.model;
    self.MG_monster_mins = self.mins;
    self.MG_monster_maxs = self.maxs;
    self.MG_monster_solid = self.solid;
    self.MG_monster_move = self.movetype;
    self.MG_deferred = TRUE;
    self.use = MG_MonsterActivate;
    self.nextthink = 0;
    self.think = SUB_Null;
    self.takedamage = DAMAGE_NO;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.flags = self.flags - (self.flags & FL_MONSTER); // Official FL_FUTUREMONSTER collides with VR FL_SPECIFICDAMAGE; MG_deferred owns it.
    setmodel(self, "");
    return TRUE;
};
// A killtarget's victim on MG maps (subs.qc SUB_KillTarget): a future monster's kill counted, a live monster killed by
// the activator, anything else removed.
void(entity t) MG_KillTarget = {
    if(t.MG_deferred) MG_MonsterKillDeferred(t, activator);
    else if(t.flags & FL_MONSTER && t.health > 0) Killed(t, activator);
    else remove(t);
};
void(entity m, entity attacker) MG_MonsterKillDeferred = {
    // Official future-monster killtargets count a kill without activating their attack or ragdoll.
    if(!m.MG_deferred) return;
    m.MG_deferred = FALSE;
    killed_monsters++;
    WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
    remove(m);
};
void() MG_CoopActivate = {
    entity s = find(world, classname, "info_player_coop");
    while(s) { s.state = s.targetname == self.target ? 73 : 0; s = find(s, classname, "info_player_coop"); }
};
/*QUAKED trigger_activate_coop_spawns (1 0 1) (-8 -8 -8) (8 8 8)
Activates the named cooperative checkpoint spawnpoints and disables the previous set.
*/
void() trigger_activate_coop_spawns = {
    if(!MG_WorldGuard()) return;
    if(!coop) { remove(self); return; }
    self.use = MG_CoopActivate;
};
string MG_intermissiontext;

#ifdef QVR_TESTS
entity MG_telefrag_victim;
entity MG_telefrag_monster;
#endif
