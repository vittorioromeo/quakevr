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
// Dawn of the Machine (MG3, campaign 5): the native port's shared definitions. Helpers are MG3_-prefixed; mapper keys
// and classnames keep their authored names. Plan and task list: docs/vr-port/MG3_PLAN.md. Source snapshot and
// license: docs/vr-port/OFFICIAL_QC_SOURCE.md, QC/COPYING-rerelease.txt (quakec_mg3).
// Gated: vr_gamedir.cpp keeps MG3 nativeReady false (developer path: `vr_campaign_native mg3`).
float() MG3_Campaign = { return cvar("vr_campaign") == 5; };

// ----------------------------------------------------------------------------
// Persistent state (MG3_PLAN.md 2.1). Upstream keeps its capacity upgrades (one bit per map) in parm10..14 and its
// bloody weapons in parm15, used live through the level; VR's hands and holsters own those slots, so here the live
// values are these globals (saved with the game) and the level-start values parm51..56 (defs.qc), which the engine
// carries through changelevel, keeps for a death's restart and stores in saves like parm17..50. Upstream's
// SetNewParms never touches them: neither does VR's, so only a new game (`map`: fresh globals) clears them.
float MG3_UPGRADE_HEALTH = 0; // upgrade kinds: health, or the ammunition's AID_SHELLS .. AID_CELLS (vr_defs.qc)
float MG3_BLOODY_SG = 1;      // MG3_bloody bits (upstream BLOODY_SG, BLOODY_SSG)
float MG3_BLOODY_SSG = 2;
float MG3_upgrade_health, MG3_upgrade_shells, MG3_upgrade_nails, MG3_upgrade_rockets, MG3_upgrade_cells;
float MG3_bloody;
float(float kind) MG3_UpgradeMask = {
    if(kind == AID_SHELLS) return MG3_upgrade_shells;
    if(kind == AID_NAILS) return MG3_upgrade_nails;
    if(kind == AID_ROCKETS) return MG3_upgrade_rockets;
    if(kind == AID_CELLS) return MG3_upgrade_cells;
    return MG3_upgrade_health;
};
void(float kind, float mask) MG3_SetUpgradeMask = {
    if(kind == AID_SHELLS) MG3_upgrade_shells = mask;
    else if(kind == AID_NAILS) MG3_upgrade_nails = mask;
    else if(kind == AID_ROCKETS) MG3_upgrade_rockets = mask;
    else if(kind == AID_CELLS) MG3_upgrade_cells = mask;
    else MG3_upgrade_health = mask;
};
float() MG3_BloodyBits = { return MG3_bloody; };
void(float bits) MG3_SetBloodyBits = { MG3_bloody = bits; };
// SetChangeParms (before anything else, so a dead player's SetNewParms keeps them, as upstream's does).
void() MG3_EncodeParms = {
    parm51 = MG3_upgrade_health; parm52 = MG3_upgrade_shells; parm53 = MG3_upgrade_nails;
    parm54 = MG3_upgrade_rockets; parm55 = MG3_upgrade_cells; parm56 = MG3_bloody;
};
// DecodeLevelParms: the level-start values, joined with what this level's server already has (fresh globals after a
// changelevel, a restart or a new game, so they alone; a coop player joining or respawning mid-level must not take
// back another's pickup: the state is the server's, like upstream's parm globals).
void() MG3_DecodeParms = {
    MG3_upgrade_health = MG3_upgrade_health | parm51; MG3_upgrade_shells = MG3_upgrade_shells | parm52;
    MG3_upgrade_nails = MG3_upgrade_nails | parm53; MG3_upgrade_rockets = MG3_upgrade_rockets | parm54;
    MG3_upgrade_cells = MG3_upgrade_cells | parm55; MG3_bloody = MG3_bloody | parm56;
};

// ----------------------------------------------------------------------------
// Capacities (MG3_PLAN.md 2.2; upstream defs.qc BASE_*, GetUpgradeAmount, GetCurrentUpgradedMaxValue). Single player:
// these bases and 10 more per map whose upgrade was collected; deathmatch: id1's. VR_MaxAmmo/VR_MaxHealth
// (vr_ammoutil.qc) ask here for campaign 5 only.
float MG3_BASE_HEALTH = 50;
float MG3_BASE_SHELLS = 50;
float MG3_BASE_NAILS = 100;
float MG3_BASE_ROCKETS = 20;
float MG3_BASE_CELLS = 100;
float MG3_UPGRADE_AMOUNT = 10;
float(float mask) MG3_BitCount = {
    float n = 0;
    while(mask >= 1) { if(mask & 1) n++; mask = floor(mask / 2); }
    return n;
};
float(float kind) MG3_Capacity = {
    if(deathmatch) {
        if(kind == AID_NAILS || kind == AID_CELLS) return 200;
        return 100;
    }
    float base = MG3_BASE_HEALTH;
    if(kind == AID_SHELLS) base = MG3_BASE_SHELLS;
    else if(kind == AID_NAILS) base = MG3_BASE_NAILS;
    else if(kind == AID_ROCKETS) base = MG3_BASE_ROCKETS;
    else if(kind == AID_CELLS) base = MG3_BASE_CELLS;
    return base + MG3_UPGRADE_AMOUNT * MG3_BitCount(MG3_UpgradeMask(kind));
};

// ----------------------------------------------------------------------------
// serverflags in campaign 5 (upstream items_runes.qc SIGIL_E1..4, mg3_items.qc): the four runes and the Bloody
// Nightmare bits (ACTIVE: this game is one; DISCOVERED: the hell knight's head found; NEWGAME: the new game it starts;
// TETTE: an easter egg).
float MG3_SF_RUNE1 = 1;
float MG3_SF_RUNE2 = 2;
float MG3_SF_RUNE3 = 4;
float MG3_SF_RUNE4 = 8;
float MG3_SF_RUNES = 15;
float MG3_SF_BN_ACTIVE = 64;
float MG3_SF_BN_DISCOVERED = 128;
float MG3_SF_BN_NEWGAME = 256;
float MG3_SF_TETTE = 512;

// Keys only MG3's boss maps and editor leave (no behaviour of their own here): the finales' waves and teleport target
// (monster_boss_final, monster_oldone_new: their tasks), a mapper's note, a light compiler key; KEX's worldspawn sky
// fog factor, which the engine reads as Ironwail's "skyfog" (gl_sky.c).
.string wave1, wave2, wave3, tele_target;
.string comment;
.float dirt;
.float fog_sky_factor;

// The runes brought home (upstream RemovedRuneCheck's count).
float() MG3_RuneCount = {
    float n = 0;
    if(serverflags & MG3_SF_RUNE1) n++;
    if(serverflags & MG3_SF_RUNE2) n++;
    if(serverflags & MG3_SF_RUNE3) n++;
    if(serverflags & MG3_SF_RUNE4) n++;
    return n;
};
// Upstream RemovedRuneCheck: campaign 5's maps are played again after runes come home (map1 first and later), and
// an entity whose spawnflag NOT_IF_<n>_RUNES (262144 << n, n = 0..4) matches the count is not there (removed: TRUE).
// Items, monsters, triggers, corpses and intermission views ask (StartItem and the monsters' and triggers' starts
// through MG_MonsterInhibited/MG_MonsterPrepare, misc_corpse, info_intermission).
float MG3_NOT_IF_0_RUNES = 262144;
float() MG3_RuneRemoved = {
    if(!MG3_Campaign() || self.spawnflags < MG3_NOT_IF_0_RUNES) return FALSE;
    float bit = MG3_NOT_IF_0_RUNES, n = MG3_RuneCount();
    while(n > 0) { bit = bit * 2; n--; }
    if(!(self.spawnflags & bit)) return FALSE;
    remove(self);
    return TRUE;
};
