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
// Dawn of the Machine's map triggers (MG3_PLAN.md, M3-05/M3-06), adapted from the snapshot's mg3_triggers.qc and
// triggers.qc (docs/vr-port/OFFICIAL_QC_SOURCE.md). Classnames and mapper keys keep their authored names; helpers
// are MG3_-prefixed. None needs MG3's data, so every map may use them (the agglomeration principle: an official
// expansion's entities work in any campaign); only the spawnflags 32768 (coop only) and 131072 (not in coop) are
// read as MG3 authors them, here as upstream.

// Upstream INHIBIT_COOP + RemovedOutsideCoop (a removed self returns TRUE).
float() MG3_CoopRemoved = {
    if((!coop && (self.spawnflags & 32768)) || (coop && (self.spawnflags & 131072))) { remove(self); return TRUE; }
    return FALSE;
};

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
Fires its targets once, a tenth of a second after the map loads (Dawn of the Machine).
*/
void() trigger_always = {
    if(MG3_CoopRemoved()) return;
    self.think = SUB_UseTargets;
    self.nextthink = time + 0.1;
};

// ----------------------------------------------------------------------------
// trigger_door_relay: toggles the func_doors and func_buttons it targets by their state.
float MG3_DOOR_RELAY_IGNORE_IF_TOP = 1;
float MG3_DOOR_RELAY_IGNORE_IF_BOTTOM = 2;
void() MG3_door_relay_use = {
    entity oself = self;
    entity t = find(world, targetname, self.target);
    while(t) {
        if(t.state == STATE_TOP || t.state == STATE_UP) {
            if(!(oself.spawnflags & MG3_DOOR_RELAY_IGNORE_IF_TOP)) {
                self = t;
                if(t.classname == "func_door") door_go_down();
                else if(t.classname == "func_button") button_return();
                self = oself;
            }
        }
        else if(t.state == STATE_BOTTOM || t.state == STATE_DOWN) {
            if(!(oself.spawnflags & MG3_DOOR_RELAY_IGNORE_IF_BOTTOM)) {
                self = t;
                if(t.classname == "func_door") door_go_up();
                else if(t.classname == "func_button") button_fire();
                self = oself;
            }
        }
        t = find(t, targetname, oself.target);
    }
};
/*QUAKED trigger_door_relay (.5 .5 .5) (-8 -8 -8) (8 8 8) IGNORE_IF_TOP IGNORE_IF_BOTTOM
When used, opens each func_door and presses each func_button it targets that is closed (or closing), and closes each
one that is open (or opening). IGNORE_IF_TOP: leave open ones alone; IGNORE_IF_BOTTOM: leave closed ones alone
(Dawn of the Machine).
*/
void() trigger_door_relay = {
    if(!self.target) { dprint("trigger_door_relay without a target: removed\n"); remove(self); return; }
    self.use = MG3_door_relay_use;
};

// trigger_doorgroup_relay (upstream, unused by MG3's maps): fires its targets when every door and button of its
// category is at (or moving to) its goal_state.
.string category;
.float goal_state;
void() MG3_doorgroup_relay_use = {
    entity t = find(world, category, self.category);
    while(t) {
        float down = t.state == STATE_BOTTOM || t.state == STATE_DOWN;
        float up = t.state == STATE_TOP || t.state == STATE_UP;
        if((down && t.goal_state == STATE_TOP) || (up && t.goal_state == STATE_BOTTOM)) return;
        t = find(t, category, self.category);
    }
    SUB_UseTargets();
};
/*QUAKED trigger_doorgroup_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
When used, fires its targets if every door and button whose "category" is this one's is at its "goal_state"
(0 open, 1 closed): pressure plates, switch codes (Dawn of the Machine).
*/
void() trigger_doorgroup_relay = {
    if(!self.category || !self.target) { dprint("trigger_doorgroup_relay without a category or target: removed\n"); remove(self); return; }
    self.use = MG3_doorgroup_relay_use;
};

// ----------------------------------------------------------------------------
// trigger_teleport_silent: moves the player straight up or down by its height, keeping speed and view: the seamless
// stacked rooms of secret6. In VR what the hands carry comes along (VR_Carry_Teleported).
void() MG3_teleport_silent_touch = {
    if(other.classname != "player") return;
    vector from = other.origin;
    vector org = other.origin;
    org_z = other.origin_z + self.height;
    if(self.height < 0) org_z = org_z + 1;
    spawn_tdeath(org, other);
    setorigin(other, org);
    VR_Carry_Teleported(other, from);
};
/*QUAKED trigger_teleport_silent (.5 .5 .5) ?
Moves a player who touches it by "height" units straight up (positive) or down (negative; default -2048), keeping
speed and view, without effect or sound (Dawn of the Machine).
*/
void() trigger_teleport_silent = {
    if(!self.height) self.height = -2048;
    InitTrigger();
    self.touch = MG3_teleport_silent_touch;
};

// ----------------------------------------------------------------------------
// trigger_multitouch: fires on the first touch, when emptied (no player for 0.2 s), or both.
float MG3_MULTITOUCH_IGNORE_FIRST_TOUCH = 16;
float MG3_MULTITOUCH_IGNORE_EMPTIED = 32;
float MG3_MULTITOUCH_REQUIRE_ONGROUND = 64;
void() MG3_multitouch_think = {
    self.wait = 0;
    if(!(self.spawnflags & MG3_MULTITOUCH_IGNORE_EMPTIED)) SUB_UseTargets();
};
void() MG3_multitouch_touch = {
    if(other.classname != "player" || other.health <= 0) return;
    if((self.spawnflags & MG3_MULTITOUCH_REQUIRE_ONGROUND) && !(other.flags & FL_ONGROUND)) return;
    if(self.wait == 0) {
        self.wait = 1;
        activator = other;
        if(!(self.spawnflags & MG3_MULTITOUCH_IGNORE_FIRST_TOUCH)) SUB_UseTargets();
    }
    self.think = MG3_multitouch_think;
    self.nextthink = time + 0.2;
};
/*QUAKED trigger_multitouch (.5 .5 .5) ? X X X X IGNORE_FIRST_TOUCH IGNORE_EMPTIED REQUIRE_ONGROUND
Fires its targets when a player first enters it and again when the last one has left it (0.2 s without a touch).
IGNORE_FIRST_TOUCH / IGNORE_EMPTIED drop either; REQUIRE_ONGROUND counts only players standing (Dawn of the Machine).
*/
void() trigger_multitouch = {
    InitTrigger();
    self.touch = MG3_multitouch_touch;
    self.wait = 0; // 0 empty, 1 touched
};

// ----------------------------------------------------------------------------
// trigger_explosion_repeater: once used, explodes at its origin every wait + random() * pausetime seconds (after a
// first delay), forever, or count times with spawnflag 4. Each blast is a short-lived child: 120 radius damage (the
// repeater the attacker, as upstream), an explosion and its sprite.
.entity MG3_explosion_child;
.float MG3_explosions;      // this repeater's blasts so far (vr_mg3_test)
float MG3_explosion_blasts; // every repeater's (vr_mg3_test)
void() MG3_explosion_child_activate = {
    self.delay = 0;
    T_RadiusDamage(self, self.owner, 120, self);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);
    BecomeExplosion();
};
void() MG3_explosion_repeater_use = {
    if(self.delay > 0) { // the first use waits its delay
        self.think = MG3_explosion_repeater_use;
        self.nextthink = time + self.delay;
        self.delay = 0;
        return;
    }
    entity c = self.MG3_explosion_child;
    if(c && c.classname == "spawned_explosion" && c.owner == self) remove(c);
    c = spawn();
    c.classname = "spawned_explosion";
    c.owner = self;
    setorigin(c, self.origin);
    self.MG3_explosion_child = c;
    self.MG3_explosions = self.MG3_explosions + 1;
    MG3_explosion_blasts = MG3_explosion_blasts + 1;
    entity oself = self;
    self = c; MG3_explosion_child_activate(); self = oself;
    if(self.spawnflags & 4) {
        self.count = self.count - 1;
        if(self.count == 0) { remove(self); return; }
    }
    self.think = MG3_explosion_repeater_use;
    self.nextthink = time + self.wait + random() * self.pausetime;
};
/*QUAKED trigger_explosion_repeater (.5 .5 .5) (-8 -8 -8) (8 8 8) X X USE_COUNT
Once used, explodes at its origin (120 damage) after "delay", then every "wait" (default 0.8) plus up to "pausetime"
random seconds; with USE_COUNT only "count" times (Dawn of the Machine).
*/
void() trigger_explosion_repeater = {
    self.use = MG3_explosion_repeater_use;
    if(!self.wait) self.wait = 0.8;
};

// ----------------------------------------------------------------------------
// trigger_music: plays CD track "style" (default 3) when used, unless it is already playing.
void() MG3_music_use = {
    if(self.sounds == self.style) return;
    WriteByte(MSG_ALL, SVC_CDTRACK);
    WriteByte(MSG_ALL, self.style);
    WriteByte(MSG_ALL, self.style);
    entity t = find(world, classname, "trigger_music");
    while(t) { t.sounds = self.style; t = find(t, classname, "trigger_music"); }
};
/*QUAKED trigger_music (.5 .5 .5) (-8 -8 -8) (8 8 8)
When used (it needs a targetname), switches the music to track "style" (default 3) (Dawn of the Machine).
*/
void() trigger_music = {
    self.sounds = world.sounds;
    if(!self.targetname) { remove(self); return; }
    if(!self.style) self.style = 3;
    self.use = MG3_music_use;
};

// ----------------------------------------------------------------------------
// trigger_heal: heals a player inside it "dmg" (default 1) every "wait" (default 0.1) seconds, up to max_health.
void() MG3_heal_touch = {
    if(other.classname != "player") return;
    if(self.attack_finished > time) return;
    T_Heal(other, self.dmg, FALSE);
    self.attack_finished = time + self.wait;
};
/*QUAKED trigger_heal (.5 .5 .5) ?
Heals a player inside it "dmg" health (default 1) every "wait" seconds (default 0.1), up to the player's maximum
(Dawn of the Machine).
*/
void() trigger_heal = {
    if(!self.dmg) self.dmg = 1;
    if(!self.wait) self.wait = 0.1;
    InitTrigger();
    self.touch = MG3_heal_touch;
};

// trigger_quad (upstream, unused by MG3's maps): Quad Damage while inside it.
void() MG3_quad_touch = {
    if(other.classname != "player") return;
    if(!VR_ItemUtil_EntHasItem(other, IID_QUAD)) {
        VR_ItemUtil_EntAddItem(other, IID_QUAD);
        sound(other, CHAN_ITEM, "items/damage.wav", 1, ATTN_NORM);
    }
    other.super_damage_finished = time + 0.1;
    other.super_time = 2;
};
/*QUAKED trigger_quad (.5 .5 .5) ?
A player inside it has Quad Damage (it ends a tenth of a second after leaving) (Dawn of the Machine).
*/
void() trigger_quad = {
    precache_sound("items/damage.wav");
    InitTrigger();
    self.touch = MG3_quad_touch;
};

// trigger_relay_killmonster (upstream, unused by MG3's maps): kills the living monsters it targets.
void() MG3_killmonster_use = {
    entity f = find(world, targetname, self.target);
    while(f) {
        if((f.flags & FL_MONSTER) && f.health > 0) T_Damage(f, activator, activator, f.health * 2);
        f = find(f, targetname, self.target);
    }
};
/*QUAKED trigger_relay_killmonster (.5 .5 .5) (-8 -8 -8) (8 8 8)
When used, kills the living monsters it targets (Dawn of the Machine).
*/
void() trigger_relay_killmonster = {
    if(!self.targetname || !self.target) { dprint("trigger_relay_killmonster without a target or targetname: removed\n"); remove(self); return; }
    self.use = MG3_killmonster_use;
};
