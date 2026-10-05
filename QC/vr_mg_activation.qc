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
    if((!coop && (self.spawnflags & 32768)) || (coop && (self.spawnflags & 131072))) {
        remove(self); return TRUE;
    }
    return FALSE;
};
void() MG_MonsterBeginWalking = {
    self.use = monster_use;
    self.pausetime = 0;
    self.th_walk();
};
void() MG_MonsterRunStart = {
    entity oldact = activator;
    activator = self.MG_monster_activator;
    self.MG_monster_start();
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

entity MG_telefrag_victim;
entity MG_telefrag_monster;
void() MG_ActivationTestFinish = {
    MG_TestCheck(MG_telefrag_victim.health <= 0, "activation telefrag kills overlapping monster through VR damage");
    dprint(sprintf("mgactivationtest: %g passed, %g failed\n", MG_test_pass, MG_test_fail));
    remove(MG_telefrag_victim); remove(MG_telefrag_monster); remove(self);
};
void() MG_ActivationTest = {
    entity oldself = self;
    entity oldact = activator;
    entity p = find(world, classname, "player");
    MG_test_pass = MG_test_fail = 0;
    float total = total_monsters;
    entity e = spawn();
    self = e; e.classname = "monster_army"; e.spawnflags = 4 | 8 | 16;
    setorigin(e, p.origin + '128 0 32');
    monster_army();
    MG_TestCheck(e.MG_deferred && !e.modelindex && !e.solid && !e.takedamage && !e.nextthink, "deferred monster invisible non-solid idle invulnerable");
    MG_TestCheck(total_monsters == total + 1 && !(e.flags & FL_MONSTER) && e.MG_deferred, "deferred counted exactly once as future monster");
    activator = p; MG_MonsterActivate();
    MG_TestCheck(!e.MG_deferred && e.modelindex && e.solid == SOLID_SLIDEBOX && (e.flags & FL_MONSTER), "activation restores native VR monster model bounds");
    MG_MonsterRunStart();
    MG_TestCheck(e.enemy == p && e.think == FoundTarget, "angry activation attacks triggering player");
    MG_TestCheck(total_monsters == total + 1, "activation does not count monster twice");
    void() thought = e.think; MG_MonsterActivate();
    MG_TestCheck(e.enemy == p && e.think == thought, "repeated target use cannot corrupt active attack");
    e.MG_deferred = FALSE;
    float kills = killed_monsters; e.health = 1;
    Killed(e, p);
    MG_TestCheck(killed_monsters == kills + 1, "activated monster death counts once");
    remove(e);
    e = spawn(); self = e; e.classname = "monster_army"; e.spawnflags = 4;
    setorigin(e, p.origin + '128 0 32'); monster_army();
    MG_MonsterKillDeferred(e, p);
    MG_TestCheck(killed_monsters == kills + 2, "deferred killtarget counts once");
    e = spawn(); self = e; e.spawnflags = 32768;
    float before = total_monsters; monster_army();
    MG_TestCheck(total_monsters == before, "solo coop-only monster inhibited without affecting count");
    e = spawn(); self = e; e.classname = "monster_ogre_marksman";
    setorigin(e, p.origin + '256 0 32'); monster_ogre_marksman();
    MG_TestCheck(e.wad == "progs/ogre.mdl", "official marksman uses authored ogre without Honey crown"); remove(e);
    e = spawn(); self = e; e.classname = "monster_knight"; e.spawnflags = 2;
    setorigin(e, p.origin + '256 0 32'); monster_knight();
    MG_TestCheck(e.health == 75 && e.skin == 0, "official monster flags do not enable Rogue statue"); remove(e);
    e = spawn(); self = e; e.classname = "monster_army"; e.spawnflags = 4096;
    entity path = spawn(); path.classname = "path_corner"; path.targetname = "mg_activation_path";
    setorigin(path, p.origin + '256 0 32'); e.target = path.targetname;
    setorigin(e, p.origin + '128 0 32'); monster_army(); walkmonster_start_go();
    MG_TestCheck(e.use == MG_MonsterBeginWalking && e.movetarget == path, "waitwalk retains authored path and waits for use");
    MG_MonsterBeginWalking();
    MG_TestCheck(e.use == monster_use && !e.pausetime, "waitwalk use resumes authored walking"); remove(e); remove(path);
    float oldcoop = coop; coop = 1;
    float prior = total_monsters;
    e = spawn(); self = e; e.spawnflags = 131072; monster_army();
    MG_TestCheck(total_monsters == prior, "coop inhibition rejects not-in-coop monsters without counting");
    entity a = spawn(); self = a; a.classname = "info_player_coop"; a.targetname = "mg_checkpoint_a"; info_player_coop();
    entity b = spawn(); self = b; b.classname = "info_player_coop"; b.targetname = "mg_checkpoint_b"; info_player_coop();
    e = spawn(); self = e; e.target = "mg_checkpoint_b"; MG_CoopActivate();
    MG_TestCheck(a.state == 0 && b.state == 73, "checkpoint activation switches named spawn set");
    entity previous = lastspawn; self = p;
    MG_TestCheck(SelectSpawnPoint() == b, "coop respawn selects activated checkpoint");
    lastspawn = previous; coop = oldcoop; remove(a); remove(b); remove(e);
    MG_telefrag_victim = spawn(); self = MG_telefrag_victim;
    self.classname = "monster_army"; setorigin(self, p.origin + '128 0 32'); monster_army(); walkmonster_start_go();
    MG_telefrag_monster = spawn(); self = MG_telefrag_monster;
    self.classname = "monster_army"; self.spawnflags = 4 | 16;
    setorigin(self, MG_telefrag_victim.origin); monster_army(); activator = p; MG_MonsterActivate();
    entity fog = nextent(world); float found = FALSE;
    while(fog) { if(fog.think == play_teleport && fog.origin == self.origin) found = TRUE; fog = nextent(fog); }
    MG_TestCheck(found, "tfog flag emits authored teleport effect and sound helper");
    entity finish = spawn(); finish.think = MG_ActivationTestFinish; finish.nextthink = time + 0.4;
    self = oldself; activator = oldact;
};

void(float request) MG_ProgressTest = {
    entity p = find(world, classname, "player");
    entity oldself = self;
    entity oldother = other;
    entity oldact = activator;
    if(request == 4) {
        float dormant = 0; float active = 0;
        entity e = nextent(world);
        while(e) { if(e.MG_deferred) dormant++; else if((e.flags & FL_MONSTER) && e.health > 0) active++; e = nextent(e); }
        dprint(sprintf("mgprogresstest: map %s total %g active %g deferred %g kills %g health %g\n",mapname,total_monsters,active,dormant,killed_monsters,p.health));
        dprint(sprintf("mgprogresstest: shells %g hands %g %g clips %g %g ids %g\n",p.ammo_shells,p.weapon,p.weapon2,p.weaponinst.wi_clip,p.weaponinst2.wi_clip,p.weaponinst.wi_uid));
        dprint(sprintf("mgprogresstest: offid %g holster %g clip %g id %g flags %g\n",p.weaponinst2.wi_uid,p.holsterweapon0,p.holsterweaponinst0.wi_clip,p.holsterweaponinst0.wi_uid,serverflags));
    } else if(request == 8) {
        p.health = 73; p.ammo_shells = 42; p.ammo_nails = 67;
        p.weapon = WID_SHOTGUN; p.weapon2 = WID_NAILGUN;
        p.weaponflags = p.weaponflags2 = 0;
        p.weaponinst = WeaponInst_Carried(WID_SHOTGUN, 3, 0);
        p.weaponinst2 = WeaponInst_Carried(WID_NAILGUN, 7, 0);
        p.holsterweapon0 = WID_SUPER_SHOTGUN; p.holsterweaponflags0 = 0;
        p.holsterweaponinst0 = WeaponInst_Carried(WID_SUPER_SHOTGUN, 2, 0);
        dprint("mgprogresstest: seeded independent hands/holster clips and health\n");
    } else if(request == 5 || request == 6) {
        entity e = find(world, classname, "trigger_changelevel");
        while(e) {
            float secret = e.map == "e5sm1" || e.map == "e5sm2";
            if((request == 5 && !secret) || (request == 6 && secret)) break;
            e = find(e, classname, "trigger_changelevel");
        }
        if(e) { self = e; other = activator = p; changelevel_touch(); dprint(sprintf("mgprogresstest: exit %s -> %s text %s\n", mapname, nextmap, MG_intermissiontext)); }
    } else if(request == 9) {
        entity e = nextent(world);
        while(e && (!e.MG_deferred || e.targetname == "")) e = nextent(e);
        if(e) {
            entity relay = spawn(); self = relay; relay.target = e.targetname; activator = p;
            SUB_UseTargets(); dprint(sprintf("mgprogresstest: authored target relay %s activated %s\n", relay.target, e.classname)); remove(relay);
        }
    } else if(request == 7 || request == 10) {
        self = p; if(intermission_running) ExitIntermission();
        dprint(sprintf("mgprogresstest: intermission %g next %s\n", intermission_running, nextmap));
        // Acceptance driver only: queue its probe after the real QC changelevel, with no prequeued quit.
        if(request == 10) localcmd("exec mgactivation_after.cfg\n");
    }
    self = oldself; other = oldother; activator = oldact;
};
