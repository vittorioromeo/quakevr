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
// Dawn of the Machine's items (MG3_PLAN.md, M3-07), adapted from the snapshot's mg3_items.qc and items.qc
// (docs/vr-port/OFFICIAL_QC_SOURCE.md): armour shards, the draughts (rings that move you up or down), the lava suit
// and the hell knight's head (Bloody Nightmare's discovery). Each works in any map when its model is on the search
// path (the agglomeration principle; the lava suit and the head fall back to the biosuit's and the hell knight's head
// gib model), else it is skipped. VR: picked up as the other items are (itemTouch: a hand or the body touching it);
// the lava suit and the head are objects taken at a holster, like the biosuit and the keys.

// ----------------------------------------------------------------------------
// Armour shards: +5 armour (green, 0.3, when none is worn) up to 200.
void() MG3_armor_shard_touch = {
    if(other.health <= 0 || other.classname != "player" || self.model == string_null) return;
    if(other.armortype < 0.3) other.armortype = 0.3;
    if(other.armorvalue >= 200) return;
    if(!VR_ItemUtil_EntHasItem(other, IID_ARMOR1) && !VR_ItemUtil_EntHasItem(other, IID_ARMOR2) && !VR_ItemUtil_EntHasItem(other, IID_ARMOR3))
        VR_ItemUtil_EntAddItem(other, IID_ARMOR1);
    other.armorvalue = min(200, other.armorvalue + 5);
    if(other.ishuman) sprint(other, "$mg3_qc_armor_shard_touch");
    VRItemPickupHaptic(other);
    sound(other, CHAN_ITEM, "items/armor1.wav", 1, ATTN_NORM);
    stuffcmd(other, "bf\n");
    self.solid = SOLID_NOT;
    self.model = string_null;
};
/*QUAKED item_armor_shard (0 .5 .8) (-16 -16 0) (16 16 56)
5 armour (green when none is worn), up to 200 (Dawn of the Machine; needs its model).
*/
void() item_armor_shard = {
    if(!VR_Pack_RequireSpawn(VR_Has("progs/armorshard.mdl"), "Dawn of the Machine")) return;
    precache_model("progs/armorshard.mdl");
    precache_sound("items/armor1.wav");
    setmodel(self, "progs/armorshard.mdl");
    self.handtouch = MG3_armor_shard_touch;
    setsize(self, '-16 -16 0', '16 16 56');
    StartItem();
};

// ----------------------------------------------------------------------------
// Draughts: rings that move the player straight up or down by "height" (insight -2048, stupor 2048 by default), or,
// with OVERRIDE_DEST and a target, to that teleport destination as a teleporter does. They stay: a ring is used each
// time it is touched. VR: what the hands carry comes along; the teleport flash and sound mark the move.
float MG3_RING_OVERRIDE_DEST = 1;
void() MG3_ring_touch = {
    if(other.classname != "player" || other.health <= 0 || self.model == string_null) return;
    if((self.spawnflags & MG3_RING_OVERRIDE_DEST) && self.target != string_null) { teleport_touch(); return; }
    vector from = other.origin;
    vector org = other.origin;
    org_z = other.origin_z + self.height;
    if(self.height < 0) org_z = org_z + 1;
    setorigin(other, org);
    spawn_tfog(org);
    spawn_tdeath(org, other);
    VR_Carry_Teleported(other, from);
};
// The ring's hum and the particles drawn in to it (a placed ring only; one triggered into being has neither).
void() MG3_draught_fx = {
    ambientsound(self.origin + '0 0 8', "ambience/hum1.wav", 0.7, ATTN_STATIC);
    entity oself = self;
    self = spawn();
    setorigin(self, oself.origin + '0 0 8');
    self.size = '24 24 0';
    self.distance = 48;
    particle_tele();
    self = oself;
};
void(string mdl, string name, float defheight) MG3_DraughtSpawn = {
    if(!VR_Pack_RequireSpawn(VR_Has(mdl), "Dawn of the Machine")) return;
    precache_sound("ambience/hum1.wav");
    precache_model(mdl);
    setmodel(self, mdl);
    self.netname = name;
    setsize(self, '-16 -16 -24', '16 16 32');
    self.health = 1;
    if(!self.height) self.height = defheight;
    self.handtouch = MG3_ring_touch;
    if(!(self.spawnflags & MG3_ITEM_SPAWNED)) MG3_draught_fx();
    StartItem();
};
/*QUAKED item_draught_insight (0 .5 .8) (0 0 0) (32 32 32) OVERRIDE_DEST
A ring that moves the player who touches it by "height" (default -2048: down), or with OVERRIDE_DEST to its target
teleport destination (Dawn of the Machine; needs its model).
*/
void() item_draught_insight = { MG3_DraughtSpawn("progs/gold_ring.mdl", "$mg3_qc_ring_of_insight", -2048); };
/*QUAKED item_draught_stupor (0 .5 .8) (0 0 0) (32 32 32) OVERRIDE_DEST
A ring that moves the player who touches it by "height" (default 2048: up), or with OVERRIDE_DEST to its target
teleport destination (Dawn of the Machine; needs its model).
*/
void() item_draught_stupor = { MG3_DraughtSpawn("progs/onyx_ring.mdl", "$mg3_qc_ring_of_oblivion", 2048); };

// ----------------------------------------------------------------------------
// The lava suit: 30 seconds without lava or slime damage (and without drowning). Taken by powerup_touch (items.qc),
// worn out in CheckPowerups (client.qc); WaterMove skips the damage.
/*QUAKED item_artifact_lavasuit (0 .5 .8) (-16 -16 -24) (16 16 32)
The player takes no damage from lava or slime for 30 seconds (Dawn of the Machine; the biosuit's model without its
data).
*/
void() item_artifact_lavasuit = {
    string mdl = VR_ModelOr("progs/lavasuit.mdl", "progs/suit.mdl");
    self.handtouch = powerup_touch;
    precache_model(mdl);
    precache_sound("items/suit.wav");
    precache_sound("items/suit2.wav");
    self.noise = "items/suit.wav";
    setmodel(self, mdl);
    self.netname = "Lavasuit";
    self.vr_itemId = IID_SUIT;
    setsize(self, '-16 -16 -24', '16 16 32');
    StartItem();
    VR_PickupObj_Mark(QVR_PICKUPOBJ_HOLSTER);
};

// ----------------------------------------------------------------------------
// The hell knight's head (map4): taking it turns Bloody Nightmare on (skill 3 and its serverflags) and discovers it,
// for this game and for good (vr_mg3_bn_discovered: the Official Campaigns menu's Bloody Nightmare row). In a
// Bloody Nightmare game it is a megahealth, or, in the new game Bloody Nightmare starts, a bunny once.
void() MG3_hellknight_head_touch = {
    if(other.classname != "player" || other.health <= 0 || self.model == string_null) return;
    bprint("$mg3_selected_bloody_nightmare");
    serverflags = serverflags | MG3_SF_BN_ACTIVE | MG3_SF_BN_DISCOVERED;
    cvar_set("vr_mg3_bn_discovered", "1");
    skill = 3;
    cvar_set("skill", "3");
    VRItemPickupHaptic(other);
    sound(other, CHAN_ITEM, "player/tornoff2.wav", 1, ATTN_NORM);
    stuffcmd(other, "bf\n");
    self.solid = SOLID_NOT;
    self.model = string_null;
};
void() MG3_tette_touch = { // dedicated to Hanna (upstream)
    if(other.classname != "player" || other.health <= 0 || self.model == string_null) return;
    if(other.ishuman) MG3_sprint_args(other, "$qc_got_item", self.netname);
    serverflags = serverflags | MG3_SF_TETTE;
    VRItemPickupHaptic(other);
    sound(other, CHAN_ITEM, "misc/secret.wav", 1, ATTN_NORM);
    stuffcmd(other, "bf\n");
    self.solid = SOLID_NOT;
    self.model = string_null;
};
/*QUAKED item_head_hellknight (0 .5 .8) (-16 -16 -16) (16 16 40)
The hell knight's head: taking it starts Bloody Nightmare (skill 3, harder monsters) and makes it selectable for good
(Dawn of the Machine).
*/
void() item_head_hellknight = {
    if(serverflags & MG3_SF_BN_ACTIVE) {
        if((serverflags & MG3_SF_BN_NEWGAME) && !(serverflags & MG3_SF_TETTE) && VR_Has("progs/g_bunny.mdl")) {
            precache_model("progs/g_bunny.mdl");
            setmodel(self, "progs/g_bunny.mdl");
            self.handtouch = MG3_tette_touch;
            self.netname = "$mg3_qc_newgameplus_item";
        }
        else { // a megahealth instead
            self.classname = "item_health";
            self.spawnflags = H_MEGA;
            makevectors(self.angles);
            setorigin(self, self.origin + v_right * 16 + v_forward * -16);
            item_health();
            return;
        }
    }
    else {
        string mdl = VR_ModelOr("progs/item_h_hellkn.mdl", "progs/h_hellkn.mdl");
        precache_model(mdl);
        precache_sound("player/tornoff2.wav");
        setmodel(self, mdl);
        self.netname = "the Hell Knight's head";
        self.handtouch = MG3_hellknight_head_touch;
    }
    setsize(self, '-16 -16 -16', '16 16 40');
    StartItem();
    VR_PickupObj_Mark(QVR_PICKUPOBJ_HOLSTER);
};

// ----------------------------------------------------------------------------
// Items a trigger brings (upstream SPAWNED, spawnflag 4, campaign 5; PlaceItem): hidden and untouchable until used,
// then back with the respawn sound (upstream SUB_regen), through Honey's trigger-spawn pair (item_unspawn/item_spawn).
void() MG3_ItemSpawn = {
    item_spawn();
    sound(self, CHAN_VOICE, "items/itembk2.wav", 1, ATTN_NORM);
};
void() MG3_ItemUnspawn = {
    precache_sound("items/itembk2.wav");
    item_unspawn();
    self.use = MG3_ItemSpawn;
};
