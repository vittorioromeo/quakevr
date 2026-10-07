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

// ----------------------------------------------------------------------------
// Bloody Nightmare (MG3_PLAN.md M3-09; upstream client.qc DecodeLevelParms, combat.qc T_Damage, triggers.qc
// setskill_use). Campaign 5 only: skill 3 with serverflags MG3_SF_BN_ACTIVE.
float() MG3_BloodyNightmare = { return MG3_Campaign() && (serverflags & MG3_SF_BN_ACTIVE); };

// The player's blows on others do 80%; what others do to the player 120% (not player against player).
float(entity targ, entity attacker, float damage) MG3_BloodyNightmareDamage = {
    if(attacker.classname == "player" && targ.classname != "player") return damage * 0.8;
    if(targ.classname == "player" && attacker.classname != "player") return damage * 1.2;
    return damage;
};

// Each level of a Bloody Nightmare game starts with the axe and the shotgun only (and the Super Axe, the bloody
// super shotgun once found, and the best armour worn); ammunition is kept. Upstream rewrites parm1 (its items) and
// the selected weapon; here the level parms of the hands and holsters (VR's inventory, client.qc SetChangeParms) are
// rewritten before DecodeLevelParms reads them: any other weapon (a gun, an enemy sword, the chainsaw...) is gone
// with its record, and the axe and shotgun (and that super shotgun) are put back in an empty holster when gone.
float(float wid) MG3_BNKeeps = {
    if(wid == WID_FIST || wid == WID_AXE || wid == WID_SHOTGUN || wid == WID_MJOLNIR) return TRUE;
    return wid == WID_SUPER_SHOTGUN && ((MG3_bloody | parm56) & MG3_BLOODY_SSG);
};
float(float wid) MG3_BNHas = {
    return parm8 == wid || parm10 == wid || parm11 == wid || parm12 == wid || parm13 == wid || parm14 == wid
        || parm15 == wid || parm16 == wid;
};
// Into the default holster when free (the axe's parm13, the shotgun's parm14: SetNewParms), else the first free one.
void(float wid, float preferred) MG3_BNPutBack = {
    if(MG3_BNHas(wid)) return;
    if(preferred == 13 && parm13 == WID_FIST) { parm13 = wid; parm27 = 0; parm35 = 0; parm45 = 0; return; }
    if(preferred == 14 && parm14 == WID_FIST) { parm14 = wid; parm28 = 0; parm36 = 0; parm46 = 0; return; }
    if(parm11 == WID_FIST) { parm11 = wid; parm25 = 0; parm33 = 0; parm43 = 0; return; }
    if(parm12 == WID_FIST) { parm12 = wid; parm26 = 0; parm34 = 0; parm44 = 0; return; }
    if(parm13 == WID_FIST) { parm13 = wid; parm27 = 0; parm35 = 0; parm45 = 0; return; }
    if(parm14 == WID_FIST) { parm14 = wid; parm28 = 0; parm36 = 0; parm46 = 0; return; }
    if(parm15 == WID_FIST) { parm15 = wid; parm29 = 0; parm37 = 0; parm47 = 0; return; }
    if(parm16 == WID_FIST) { parm16 = wid; parm30 = 0; parm38 = 0; parm48 = 0; return; }
    if(parm8 == WID_FIST) { parm8 = wid; parm23 = 0; parm31 = 0; parm41 = 0; return; }
    if(parm10 == WID_FIST) { parm10 = wid; parm24 = 0; parm32 = 0; parm42 = 0; }
};
void() MG3_BloodyNightmareStrip = {
    if(!MG3_Campaign() || !(serverflags & MG3_SF_BN_ACTIVE)) return;
    if(skill != 3) { // the skill was changed: no longer Bloody Nightmare
        serverflags = serverflags - (serverflags & MG3_SF_BN_ACTIVE);
        return;
    }
    if(!MG3_BNKeeps(parm8)) { parm8 = WID_FIST; parm23 = 0; parm31 = 0; parm41 = 0; }
    if(!MG3_BNKeeps(parm10)) { parm10 = WID_FIST; parm24 = 0; parm32 = 0; parm42 = 0; }
    if(!MG3_BNKeeps(parm11)) { parm11 = WID_FIST; parm25 = 0; parm33 = 0; parm43 = 0; }
    if(!MG3_BNKeeps(parm12)) { parm12 = WID_FIST; parm26 = 0; parm34 = 0; parm44 = 0; }
    if(!MG3_BNKeeps(parm13)) { parm13 = WID_FIST; parm27 = 0; parm35 = 0; parm45 = 0; }
    if(!MG3_BNKeeps(parm14)) { parm14 = WID_FIST; parm28 = 0; parm36 = 0; parm46 = 0; }
    if(!MG3_BNKeeps(parm15)) { parm15 = WID_FIST; parm29 = 0; parm37 = 0; parm47 = 0; }
    if(!MG3_BNKeeps(parm16)) { parm16 = WID_FIST; parm30 = 0; parm38 = 0; parm48 = 0; }
    MG3_BNPutBack(WID_AXE, 13);
    MG3_BNPutBack(WID_SHOTGUN, 14);
    float items = IT_SHOTGUN | IT_AXE;
    if((MG3_bloody | parm56) & MG3_BLOODY_SSG) { MG3_BNPutBack(WID_SUPER_SHOTGUN, 0); items = items | IT_SUPER_SHOTGUN; }
    if(parm3 <= 0) {} // (no armour left: none worn)
    else if(parm1 & IT_ARMOR3) items = items | IT_ARMOR3;
    else if(parm1 & IT_ARMOR2) items = items | IT_ARMOR2;
    else if(parm1 & IT_ARMOR1) items = items | IT_ARMOR1;
    parm1 = items;
};
// Upstream's level-select rule: boss2 entered on skill 3 is a Bloody Nightmare game.
void() MG3_BloodyNightmareLevel = {
    if(MG3_Campaign() && mapname == "boss2" && skill == 3) serverflags = serverflags | MG3_SF_BN_ACTIVE | MG3_SF_BN_DISCOVERED;
};

// Official Campaigns > Dawn of the Machine: Bloody Nightmare (shown once vr_mg3_bn_discovered): the menu sets skill 3
// and vr_mg3_bn_start before the start map loads; its first frame turns the game into a Bloody Nightmare one.
void() MG3_Frame = {
    if(!cvar("vr_mg3_bn_start")) return;
    if(time < 1 || !find(world, classname, "player")) return; // (once the player is in: he is told)
    cvar_set("vr_mg3_bn_start", "0");
    if(!MG3_Campaign()) return;
    serverflags = serverflags | MG3_SF_BN_ACTIVE | MG3_SF_BN_DISCOVERED;
    cvar_set("skill", "3");
    skill = 3;
    MG_PrintAll("$mg3_selected_bloody_nightmare"); // (centre printed: a broadcast this early may come before the client)
};
