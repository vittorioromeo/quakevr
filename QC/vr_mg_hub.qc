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

// MG1 progression; licensed source and provenance: docs/vr-port/OFFICIAL_QC_SOURCE.md.
.float MG1_rune_locked;
.float MG1_electrode_pressed;
float() MG1_Campaign = { return cvar("vr_campaign") == 4; };
float() MG1_Hub = { return MG1_Campaign() && world.worldtype == 3; };
float() MG1_LastRune = { return floor(serverflags / 64) & 31; };
void() MG1_ClearLastRune = { serverflags = serverflags - (serverflags & 1984); };
entity() MG1_ReturnSpawn = {
    float rune = MG1_LastRune();
    if(!MG1_Campaign() || !rune) return world;
    string name = "";
    if(rune & 1) name = "start_1"; else if(rune & 2) name = "start_2";
    else if(rune & 4) name = "start_3"; else if(rune & 8) name = "start_4"; else if(rune & 16) name = "start_5";
    entity spot = find(world, netname, name);
    while(spot) {
        if(spot.classname == "info_player_start_hub") { MG1_ClearLastRune(); return spot; }
        spot = find(spot, netname, name);
    }
    return world;
};
void() info_player_start_hub = { if(!MG1_Campaign()) remove(self); };
/*QUAKED hub_trigger_changelevel (0 .5 .8) ?
Machine-specific authored progression.
*/
void() hub_trigger_changelevel = {
    if(!MG1_Campaign() || (serverflags & 31) != 31) { remove(self); return; }
    trigger_changelevel();
};
void() MG1_sigil_touch = {
    if(other.classname != "player" || other.health <= 0 || self.MG1_rune_locked || self.model == string_null) return;
    VRItemPickupHaptic(other);
    centerprint(other, self.netname);
    sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
    stuffcmd(other, "bf\n");
    self.solid = SOLID_NOT; self.model = string_null;
    MGH_RuneHunger();
    float rune = self.spawnflags & 31;
    serverflags = serverflags | rune | (rune * 64);
    activator = other; SUB_UseTargets();
};
void() MG1_item_sigil = {
    if(!self.spawnflags) self.spawnflags = 1;
    float bit = 1; float i = 1;
    while(i < 6 && !(self.spawnflags & bit)) { bit *= 2; i++; }
    self.spawnflags = bit;
    precache_sound("misc/runekey.wav"); self.noise = "misc/runekey.wav";
    string mdl = i == 1 ? "progs/mg1_rune1.mdl" : i == 2 ? "progs/mg1_rune2.mdl" : i == 3 ? "progs/mg1_rune3.mdl" : i == 4 ? "progs/mg1_rune4.mdl" : i == 5 ? "progs/mg1_rune5.mdl" : "progs/mg1_rune6.mdl";
    precache_model(mdl); setmodel(self, mdl);
    self.netname = i == 1 ? "$qc_mg1_pickup_rune1" : i == 2 ? "$qc_mg1_pickup_rune2" : i == 3 ? "$qc_mg1_pickup_rune3" : i == 4 ? "$qc_mg1_pickup_rune4" : i == 5 ? "$qc_mg1_pickup_rune5" : "$qc_mg1_pickup_rune6";
    self.handtouch = MG1_sigil_touch;
    setsize(self, '-16 -16 -24', '16 16 32'); StartItem();
    VR_PickupObj_Mark(QVR_PICKUPOBJ_HOLSTER);
};
// All physical button paths pass button_fire, including held props and weapons.
void() MG1_ElectrodePressed = {
    self.MG1_electrode_pressed = TRUE;
    self.touch = SUB_Null; self.handtouch = SUB_Null; self.vr_wpntouch = SUB_Null;
    entity t = find(world, classname, "mge2m2_electrode_target");
    while(t) {
        if(t.cnt == self.cnt) { t.think = SUB_Remove; t.nextthink = time + 0.1; }
        t = find(t, classname, "mge2m2_electrode_target");
    }
};
/*QUAKED mge2m2_electrode_target (0 .5 .8) ?
Machine-specific authored progression.
*/
void() mge2m2_electrode_target = { if(!MG1_Campaign()) remove(self); };
/*QUAKED mge2m2_electrode_button (0 .5 .8) ?
Machine-specific authored progression.
*/
void() mge2m2_electrode_button = { if(!MG1_Campaign()) { remove(self); return; } func_button(); };
void() mge2m2_rune_egg_opener_use = {
    entity caller = self;
    entity t = find(world, targetname, self.target);
    while(t) {
        if(t.classname == "door" && t.use == door_use) {
            self = t; self.movedir = self.dest2; self.pos1 = self.origin;
            self.pos2 = self.pos1 + self.movedir; self.state = STATE_BOTTOM; self.speed = 500;
            self.noise1 = "doors/drclos4.wav"; self.noise2 = "doors/doormv1.wav";
            door_go_up();
        }
        t = find(t, targetname, caller.target);
    }
    self = caller;
};
/*QUAKED mge2m2_rune_egg_opener (0 .5 .8) ?
Machine-specific authored progression.
*/
void() mge2m2_rune_egg_opener = { if(!MG1_Campaign()) { remove(self); return; } self.use = mge2m2_rune_egg_opener_use; };
void() mge2m2_rune_pickup_fixer_use = {
    float locked = !self.state;
    self.state = 1;
    entity rune = find(world, classname, "item_sigil");
    while(rune) {
        // Gate every pickup path, including already converted physical objects and save/load.
        rune.MG1_rune_locked = locked;
        rune.carry_use = MG1_sigil_touch;
        rune = find(rune, classname, "item_sigil");
    }
};
/*QUAKED mge2m2_rune_pickup_fixer (0 .5 .8) ?
Machine-specific authored progression.
*/
void() mge2m2_rune_pickup_fixer = {
    if(!MG1_Campaign()) { remove(self); return; }
    self.use = mge2m2_rune_pickup_fixer_use; self.think = mge2m2_rune_pickup_fixer_use; self.nextthink = time + 0.8;
};


const float MG1_RUNE_INDICATOR_ACTIVE = 64;

void() misc_rune_indicator_use =
{
	self.alpha = 1.0;
	SUB_UseTargets();
}

void() misc_rune_indicator =
{
    if(cvar("vr_campaign") != 4) { remove(self); return; }
	float active = self.spawnflags & MG1_RUNE_INDICATOR_ACTIVE ? TRUE : FALSE;
	self.spawnflags = self.spawnflags - (self.spawnflags & MG1_RUNE_INDICATOR_ACTIVE);
	if (!self.spawnflags)
	{
		self.spawnflags|= 1;
	}
	self.spawnflags &= 63;

    string mdl = string_null;

	if (self.spawnflags & 1)
	{
        self.spawnflags = 1;
		mdl = "progs/mg1_rune1.mdl";
	}
	else if (self.spawnflags & 2)
	{
		self.spawnflags = 2;
        mdl = "progs/mg1_rune2.mdl";
	}
	else if (self.spawnflags & 4)
	{
		self.spawnflags = 4;
        mdl = "progs/mg1_rune3.mdl";
	}
	else if (self.spawnflags & 8)
	{
		self.spawnflags = 8;
        mdl = "progs/mg1_rune4.mdl";
	}
	else if (self.spawnflags & 16)
	{
		self.spawnflags = 16;
        mdl = "progs/mg1_rune5.mdl";
	}
	else if (self.spawnflags & 32)
	{
		self.spawnflags = 32;
        mdl = "progs/mg1_rune6.mdl";
	}
    
    precache_model (mdl);
    setmodel (self, mdl);

	self.use = misc_rune_indicator_use;

	if(((self.spawnflags & serverflags) == self.spawnflags) || active )
    {
		self.think = SUB_UseTargets;
		self.nextthink = time + 0.2;
	}
	else
	{
		//Show a ghost of the rune before you collect it.
		self.alpha = 0.2;
	}
}