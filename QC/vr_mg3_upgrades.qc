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

// Dawn of the Machine's capacity upgrades (MG3_PLAN.md, M3-04), adapted from the snapshot's mg3_upgrades.qc
// (docs/vr-port/OFFICIAL_QC_SOURCE.md). One per kind per map: taking one the first time sets the map's bit in that
// kind's mask (vr_mg3_defs.qc: kept through changelevel, saves and deaths), raises the capacity by 10 and fills it;
// taking it again on a later visit only says so. Physical: a hand takes it to a holster, like a key or a rune
// (VR_PickupObj_Mark; walking over it takes it when pickups are not objects).

.float upgrade_flag;      // authored override of the map's bit (upstream key)
.float MG3_upgrade_kind;  // MG3_UPGRADE_HEALTH or AID_SHELLS .. AID_CELLS (upstream used .armortype)

// The bit of this map (upstream GetUpgradeFlag; map2b, added late, took UPGRADE14).
float() MG3_UpgradeFlag = {
    if(mapname == "map1") return 1;
    if(mapname == "map2") return 2;
    if(mapname == "map2b") return 8192;
    if(mapname == "map3") return 4;
    if(mapname == "map4") return 8;
    if(mapname == "map5") return 16;
    if(mapname == "map6") return 32;
    if(mapname == "map7") return 64;
    if(mapname == "map8") return 128;
    if(mapname == "secret1") return 256;
    if(mapname == "secret2") return 512;
    if(mapname == "secret3") return 1024;
    if(mapname == "secret4") return 2048;
    if(mapname == "secret5") return 4096;
    if(mapname == "secret6") return 16384;
    dprint(sprintf("MG3: no upgrade flag for map %s\n", mapname));
    return 0;
};

// The engine's sprint with arguments for a localized message's {0}, {1} (the QC's sprint takes one string).
void(entity client, string s, ...) MG3_sprint_args = #24;

// Upstream ApplyUpgrade: the new bit is in the mask already, so the capacity is the new one; filled to it.
float(float kind, entity client) MG3_ApplyUpgrade = {
    if(kind == MG3_UPGRADE_HEALTH) {
        client.max_health = VR_MaxHealth(client);
        entity oo = other; other = client; T_Heal(client, client.max_health, FALSE); other = oo;
        return client.max_health;
    }
    float cap = VR_MaxAmmo(client, kind);
    if(kind == AID_SHELLS) client.ammo_shells = cap;
    else if(kind == AID_NAILS) client.ammo_nails = cap;
    else if(kind == AID_ROCKETS) client.ammo_rockets = cap;
    else if(kind == AID_CELLS) client.ammo_cells = cap;
    return cap;
};

// The pickup (itemTouch's th_die: itemTouch fires the targets).
void() MG3_UpgradeTouch = {
    if(other.classname != "player" || other.health <= 0 || self.model == string_null) return;
    float mask = MG3_UpgradeMask(self.MG3_upgrade_kind);
    if(mask & self.upgrade_flag) {
        if(other.ishuman) MG3_sprint_args(other, "$mg3_qc_upgrade_fail", self.netname);
    } else {
        MG3_SetUpgradeMask(self.MG3_upgrade_kind, mask | self.upgrade_flag);
        float newmax = MG3_ApplyUpgrade(self.MG3_upgrade_kind, other);
        if(other.ishuman) MG3_sprint_args(other, "$mg3_qc_upgrade_success", self.netname, ftos(newmax));
    }
    VRItemPickupHaptic(other);
    sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
    stuffcmd(other, "bf\n");

    // Upstream GiveUpgradeBonus: the bounds again and the hands' ammunition shown anew.
    entity os = self, oo = other;
    bound_other_ammo();
    self = other; W_SetCurrentAmmo(); self = os; other = oo;

    self.model = string_null;
    self.solid = SOLID_NOT;
};

// Upstream StartUpgradeItem, half a second after the spawn: by then the player is in and the level's masks known
// (DecodeLevelParms), so one taken on an earlier visit is drawn faded.
void() MG3_UpgradeStart = {
    if(!find(world, classname, "player")) { self.nextthink = time + 0.1; return; }
    if(MG3_UpgradeMask(self.MG3_upgrade_kind) & self.upgrade_flag) self.alpha = 0.6;
    self.handtouch = MG3_UpgradeTouch;
    StartItem();
    VR_PickupObj_Mark(QVR_PICKUPOBJ_HOLSTER);
};

void(float kind, string mdl, string snd, string name) MG3_UpgradeSpawn = {
    if(!MG3_Campaign()) { remove(self); return; }
    precache_model(mdl);
    precache_sound(snd);
    if(!self.upgrade_flag) self.upgrade_flag = MG3_UpgradeFlag();
    else dprint(sprintf("%s using overridden upgrade flag %g at %v\n", self.classname, self.upgrade_flag, self.origin));
    self.MG3_upgrade_kind = kind;
    self.netname = name;
    self.noise = snd;
    setmodel(self, mdl);
    if(kind == MG3_UPGRADE_HEALTH) { self.origin_z += 8; setsize(self, '-16 -16 -8', '16 16 48'); }
    else setsize(self, '-16 -16 0', '16 16 56');
    self.think = MG3_UpgradeStart;
    self.nextthink = time + 0.5;
};

/*QUAKED item_upgrade_health (0 .5 .8) (0 0 0) (32 32 32)
Dawn of the Machine: health capacity upgrade (+10 health capacity, once per map; heals to the new capacity).
*/
void() item_upgrade_health = { MG3_UpgradeSpawn(MG3_UPGRADE_HEALTH, "progs/item_h_player.mdl", "player/tornoff2.wav", "$mg3_qc_upgrade_health"); };

/*QUAKED item_upgrade_shells (0 .5 .8) (0 0 0) (32 32 32)
Dawn of the Machine: shell capacity upgrade (+10 shells capacity, once per map; fills it).
*/
void() item_upgrade_shells = { MG3_UpgradeSpawn(AID_SHELLS, "progs/backpackshells.mdl", "weapons/lock4.wav", "$mg3_qc_upgrade_shell"); };

/*QUAKED item_upgrade_nails (0 .5 .8) (0 0 0) (32 32 32)
Dawn of the Machine: nail capacity upgrade (+10 nails capacity, once per map; fills it).
*/
void() item_upgrade_nails = { MG3_UpgradeSpawn(AID_NAILS, "progs/backpacknails.mdl", "weapons/lock4.wav", "$mg3_qc_upgrade_nail"); };

/*QUAKED item_upgrade_rockets (0 .5 .8) (0 0 0) (32 32 32)
Dawn of the Machine: rocket capacity upgrade (+10 rockets capacity, once per map; fills it).
*/
void() item_upgrade_rockets = { MG3_UpgradeSpawn(AID_ROCKETS, "progs/backpacker.mdl", "weapons/lock4.wav", "$mg3_qc_upgrade_rocket"); };

/*QUAKED item_upgrade_cells (0 .5 .8) (0 0 0) (32 32 32)
Dawn of the Machine: cell capacity upgrade (+10 cells capacity, once per map; fills it).
*/
void() item_upgrade_cells = { MG3_UpgradeSpawn(AID_CELLS, "progs/backpackcells.mdl", "weapons/lock4.wav", "$mg3_qc_upgrade_cell"); };
