// The spawn list's other entries (ROUND22.md, "Every expansion's entities in the toolgun's spawn panel"): id's own not
// listed before, Quake VR's training dummy, and the expansions' (Scourge of Armagon "SoA", Dissolution of Eternity
// "DoE", Dimension of the Machine's and Dawn of the Machine's "MG"/"MG3", Honey). Appended after the first entries
// (vr_toolgun_select's numbers stay); the panel shows each pack as a group of its own from a fresh page.
// QVR_PACK(section, pack, label, classname, model, lift, key, value, skin, needs, probe, frame):
//   needs: 0 always; 1 while `probe` (else the model) is on the search path (an installed pack's, or an owned pack's
//   read in place: "owned/mg3/..."); 3 only in Dawn of the Machine's campaign (its once-a-map upgrades).
//   Each spawn function checks the same files (VR_Pack_RequireSpawn) and removes itself without them.
#define QVR_PACK(section, pack, label, classname, model, lift, key, value, skin, needs, probe, frame) \
    {section, label, classname, model, lift, key, value, skin, false, false, false, -1, false, needs, probe, pack, frame},

// Monsters: id's and Quake VR's.
QVR_PACK("Monsters", nullptr, "Rotfish", "monster_fish", "progs/fish.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", nullptr, "Ogre Marksman", "monster_ogre_marksman", "progs/ogre.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", nullptr, "Training Dummy", "vr_dummy", "progs/soldier.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
// Scourge of Armagon.
QVR_PACK("Monsters", "SoA", "SoA Gremlin", "monster_gremlin", "progs/grem.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "SoA", "SoA Centroid", "monster_scourge", "progs/scor.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "SoA", "SoA Spike Mine", "monster_spikemine", "progs/spikmine.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "SoA", "SoA Armagon", "monster_armagon", "progs/armalegs.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
// Dissolution of Eternity.
QVR_PACK("Monsters", "DoE", "DoE Mummy", "monster_mummy", "progs/mummy.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Wrath", "monster_wrath", "progs/wrath.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Overlord", "monster_super_wrath", "progs/s_wrath.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Phantom Swordsman", "monster_sword", "progs/sword.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Electric Eel", "monster_eel", "progs/eel2.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Aztec Guardian", "monster_morph", "progs/morph_az.mdl", 24.f, "spawnflags", 2.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Egyptian Guardian", "monster_morph", "progs/morph_eg.mdl", 24.f, "spawnflags", 4.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Greek Guardian", "monster_morph", "progs/morph_gr.mdl", 24.f, "spawnflags", 8.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Hephaestus", "monster_lava_man", "progs/lavaman.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "DoE", "DoE Dragon", "monster_dragon", "progs/dragon.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
// Dawn of the Machine (its models read in place from the owned pack: owned/mg3/...).
QVR_PACK("Monsters", "MG3", "MG3 Infected Grunt", "monster_army_infected", "progs/soldier.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Infected Enforcer", "monster_enforcer_infected", "progs/enforcer.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Infected Knight", "monster_knight_infected", "progs/knight.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Infected Death Knight", "monster_hell_knight_infected", "progs/hknight.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Demo Dog", "monster_demodog", "owned/mg3/progs/dog_explosive.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Rocket Ogre", "monster_ogre_rocket", "owned/mg3/progs/ogre_rocket.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Ranged Knight", "monster_ranged_knight", "owned/mg3/progs/rknight.mdl", 24.f, nullptr, 0.f, 0, 1, "owned/mg3/progs/diamond_trail.mdl", 0)
QVR_PACK("Monsters", "MG3", "MG3 Slime", "monster_slime", "progs/tarbaby.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Shub Zombie", "monster_szombie", "progs/zombie.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Lava Man", "monster_lava_man_mg3", "owned/mg3/progs/lavaman.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Monsters", "MG3", "MG3 Blood Shambler", "monster_super_shambler", "owned/mg3/progs/shambler_blood.mdl", 24.f, nullptr, 0.f, 0, 1, "owned/mg3/progs/rogue/plasma.mdl", 0)
QVR_PACK("Monsters", "MG3", "MG3 Orb", "monster_orb", "owned/mg3/progs/teleporter_eye_blink.mdl", 24.f, nullptr, 0.f, 0, 1, "owned/mg3/progs/rogue/sphere.mdl", 0)
QVR_PACK("Monsters", "MG3", "MG3 Ghost", "monster_ghost", "progs/player.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
// Honey (its spectral knight: the death knight's model, Honey's spear: only with Honey's data, as precaching a missing
// model is a Host_Error).
QVR_PACK("Monsters", "Honey", "Honey Spectral Knight", "monster_sknight", "progs/hknight.mdl", 24.f, nullptr, 0.f, 0, 1, "progs/spear.mdl", 0)

// Weapons: the mission packs' (Quake VR ships the Hipnotic weapons' models and sounds: VR_Pack_WeaponAvailable).
QVR_PACK("Weapons", "SoA", "SoA Mjolnir", "func_weapon_grabbable", "progs/v_hammer.mdl", 8.f, "weapon", 3.f, 0, 0, nullptr, 0)
QVR_PACK("Weapons", "SoA", "SoA Proximity Gun", "func_weapon_grabbable", "progs/v_prox.mdl", 8.f, "weapon", 9.f, 0, 0, nullptr, 0)
QVR_PACK("Weapons", "SoA", "SoA Laser Cannon", "func_weapon_grabbable", "progs/v_laserg.mdl", 8.f, "weapon", 12.f, 0, 0, nullptr, 0)
QVR_PACK("Weapons", "MG3", "MG3 Super Axe", "func_weapon_grabbable", "owned/mg3/progs/v_hammer.mdl", 8.f, "weapon", 18.f, 0, 1, "owned/mg3/progs/v_hammer_glow.mdl", 0)
QVR_PACK("Weapons", "MG3", "MG3 Bloody Shotgun", "weapon_bloody_sg", "progs/g_shot0.mdl", 8.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Weapons", "MG3", "MG3 Bloody Super Shotgun", "weapon_bloody_ssg", "progs/g_shot.mdl", 8.f, nullptr, 0.f, 0, 0, nullptr, 0)

// Items: id's not listed before.
QVR_PACK("Items", nullptr, "Small Health", "item_health", "maps/b_bh10.bsp", 0.f, "spawnflags", 1.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Large Shells", "item_shells", "maps/b_shell1.bsp", 0.f, "spawnflags", 1.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Large Nails", "item_spikes", "maps/b_nail1.bsp", 0.f, "spawnflags", 1.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Large Rockets", "item_rockets", "maps/b_rock1.bsp", 0.f, "spawnflags", 1.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Large Cells", "item_cells", "maps/b_batt1.bsp", 0.f, "spawnflags", 1.f, 0, 0, nullptr, 0)
// The keys in each of id's looks, whatever the map's (their "worldtype": 3 medieval, 1 runic, 2 base: QC VR_KeyStyle).
QVR_PACK("Items", nullptr, "Silver Key", "item_key1", "progs/w_s_key.mdl", 24.f, "worldtype", 3.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Gold Key", "item_key2", "progs/w_g_key.mdl", 24.f, "worldtype", 3.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Silver Runekey", "item_key1", "progs/m_s_key.mdl", 24.f, "worldtype", 1.f, 0, 1, nullptr, 0)
QVR_PACK("Items", nullptr, "Gold Runekey", "item_key2", "progs/m_g_key.mdl", 24.f, "worldtype", 1.f, 0, 1, nullptr, 0)
QVR_PACK("Items", nullptr, "Silver Keycard", "item_key1", "progs/b_s_key.mdl", 24.f, "worldtype", 2.f, 0, 1, nullptr, 0)
QVR_PACK("Items", nullptr, "Gold Keycard", "item_key2", "progs/b_g_key.mdl", 24.f, "worldtype", 2.f, 0, 1, nullptr, 0)
QVR_PACK("Items", nullptr, "Rune of Earth Magic", "item_sigil", "progs/end1.mdl", 24.f, "spawnflags", 1.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Rune of Black Magic", "item_sigil", "progs/end2.mdl", 24.f, "spawnflags", 2.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Rune of Hell Magic", "item_sigil", "progs/end3.mdl", 24.f, "spawnflags", 4.f, 0, 0, nullptr, 0)
QVR_PACK("Items", nullptr, "Rune of Elder Magic", "item_sigil", "progs/end4.mdl", 24.f, "spawnflags", 8.f, 0, 0, nullptr, 0)
// Scourge of Armagon.
QVR_PACK("Items", "SoA", "SoA Wetsuit", "item_artifact_wetsuit", "progs/wetsuit.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Items", "SoA", "SoA Empathy Shields", "item_artifact_empathy_shields", "progs/empathy.mdl", 24.f, nullptr, 0.f, 0, 1, "sound/hipitems/empathy2.wav", 0)
QVR_PACK("Items", "SoA", "SoA Horn of Conjuring", "item_hornofconjuring", "progs/horn.mdl", 24.f, nullptr, 0.f, 0, 1, "sound/hipitems/horn.wav", 0)
// Dissolution of Eternity (its ammo's projectiles: VR_Pack_AmmoAvailable).
QVR_PACK("Items", "DoE", "DoE Lava Nails", "item_lava_spikes", "maps/b_lnail0.bsp", 0.f, nullptr, 0.f, 0, 1, "progs/lspike.mdl", 0)
QVR_PACK("Items", "DoE", "DoE Multi Rockets", "item_multi_rockets", "maps/b_mrock0.bsp", 0.f, nullptr, 0.f, 0, 1, "progs/rockup.mdl", 0)
QVR_PACK("Items", "DoE", "DoE Plasma", "item_plasma", "maps/b_plas0.bsp", 0.f, nullptr, 0.f, 0, 1, "sound/plasma/explode.wav", 0)
QVR_PACK("Items", "DoE", "DoE Power Shield", "item_powerup_shield", "progs/shield.mdl", 24.f, nullptr, 0.f, 0, 1, "sound/shield/fadeout.wav", 0)
QVR_PACK("Items", "DoE", "DoE Anti-Grav Belt", "item_powerup_belt", "progs/beltup.mdl", 24.f, nullptr, 0.f, 0, 1, "sound/belt/fadeout.wav", 0)
QVR_PACK("Items", "DoE", "DoE Vengeance Sphere", "item_sphere", "progs/sphere.mdl", 24.f, nullptr, 0.f, 0, 1, "sound/sphere/sphere.wav", 0)
QVR_PACK("Items", "DoE", "DoE Random Powerup", "item_random_powerup", "progs/shield.mdl", 24.f, nullptr, 0.f, 0, 1, "sound/belt/fadeout.wav", 0)
// Dawn of the Machine.
QVR_PACK("Items", "MG3", "MG3 Armour Shard", "item_armor_shard", "owned/mg3/progs/armorshard.mdl", 0.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Items", "MG3", "MG3 Draught of Insight", "item_draught_insight", "owned/mg3/progs/gold_ring.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Items", "MG3", "MG3 Draught of Stupor", "item_draught_stupor", "owned/mg3/progs/onyx_ring.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Items", "MG3", "MG3 Lava Suit", "item_artifact_lavasuit", "owned/mg3/progs/lavasuit.mdl", 24.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Items", "MG3", "MG3 Health Upgrade", "item_upgrade_health", "owned/mg3/progs/item_h_player.mdl", 0.f, nullptr, 0.f, 0, 3, nullptr, 0)
QVR_PACK("Items", "MG3", "MG3 Shells Upgrade", "item_upgrade_shells", "owned/mg3/progs/backpackshells.mdl", 0.f, nullptr, 0.f, 0, 1, nullptr, 0) // (any map: an ammo backpack outside the campaign)
QVR_PACK("Items", "MG3", "MG3 Nails Upgrade", "item_upgrade_nails", "owned/mg3/progs/backpacknails.mdl", 0.f, nullptr, 0.f, 0, 1, nullptr, 0) // (any map: an ammo backpack outside the campaign)
QVR_PACK("Items", "MG3", "MG3 Rockets Upgrade", "item_upgrade_rockets", "owned/mg3/progs/backpacker.mdl", 0.f, nullptr, 0.f, 0, 1, nullptr, 0) // (any map: an ammo backpack outside the campaign)
QVR_PACK("Items", "MG3", "MG3 Cells Upgrade", "item_upgrade_cells", "owned/mg3/progs/backpackcells.mdl", 0.f, nullptr, 0.f, 0, 1, nullptr, 0) // (any map: an ammo backpack outside the campaign)

// Props: Dimension of the Machine's placed corpses (misc_corpse's style: vr_mg_corpses.qc MG_poses; a dead monster's
// body in the physics, a ragdoll once a player comes near: vr_corpse_placed), and the packs' own.
QVR_PACK("Props", "Corpses", "Grunt Corpse", "misc_corpse", "progs/soldier.mdl", 24.f, "style", 14.f, 0, 0, nullptr, 17)
QVR_PACK("Props", "Corpses", "Enforcer Corpse", "misc_corpse", "progs/enforcer.mdl", 24.f, "style", 3.f, 0, 0, nullptr, 54)
QVR_PACK("Props", "Corpses", "Knight Corpse", "misc_corpse", "progs/knight.mdl", 24.f, "style", 8.f, 0, 0, nullptr, 85)
QVR_PACK("Props", "Corpses", "Death Knight Corpse", "misc_corpse", "progs/hknight.mdl", 24.f, "style", 6.f, 0, 0, nullptr, 53)
QVR_PACK("Props", "Corpses", "Ogre Corpse", "misc_corpse", "progs/ogre.mdl", 24.f, "style", 10.f, 0, 0, nullptr, 125)
QVR_PACK("Props", "Corpses", "Fiend Corpse", "misc_corpse", "progs/demon.mdl", 24.f, "style", 0.f, 0, 0, nullptr, 53)
QVR_PACK("Props", "Corpses", "Shambler Corpse", "misc_corpse", "progs/shambler.mdl", 24.f, "style", 13.f, 0, 0, nullptr, 93)
QVR_PACK("Props", "Corpses", "Player Corpse", "misc_corpse", "progs/player.mdl", 24.f, "style", 17.f, 0, 0, nullptr, 49)
QVR_PACK("Props", "DoE", "DoE Buzzsaw", "buzzsaw", "progs/buzzsaw.mdl", 16.f, nullptr, 0.f, 0, 1, nullptr, 0)
QVR_PACK("Props", "Honey", "Honey Tree", "misc_tree", "progs/tree1.mdl", 0.f, nullptr, 0.f, 0, 1, nullptr, 0)

// Fire: id's fireball spout (its lava balls), the Machine packs' gas flare (MG1's and MG3's flame3.mdl).
QVR_PACK("Fire", nullptr, "Fireball Spout", "misc_fireball", "progs/lavaball.mdl", 8.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Fire", "MG", "MG Gas Flare", "light_flame_gas", "progs/flame3.mdl", 8.f, nullptr, 0.f, 0, 1, nullptr, 0)

// Armed grunts (experimental; QC vr_armedgrunt.qc, ROUND22.md "Armed grunts"): a grunt holding any of your weapons, its
// "weapon" key the weapon (QC's WID_*; 0 one from Combat > Armed Grunts' pool, -1 its fists).
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt (Pool)", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, nullptr, 0.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Fists", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", -1.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Axe", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 2.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Sword", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 13.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Crowbar", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 17.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Chainsaw", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 14.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Shotgun", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 4.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Super Shotgun", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 5.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Nailgun", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 6.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Super Nailgun", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 7.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Grenade Launcher", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 8.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Rocket Launcher", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 10.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Thunderbolt", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 11.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Grunt's Rifle", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 15.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Laser Rifle", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 16.f, 0, 0, nullptr, 0)
// Arcane Dimensions' weapons (with AD's data; without it the grunt spawns with its fists): the soldiers' guns (their
// WID_*), the upgrades (101 the Shadow Axe, 102 the Widowmaker, 103 the Plasma Gun) and the melee weapons (111..116:
// the death fury's, death knight's, zombie knight's and death guard's swords, the hammer, the mace).
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: AD Rocket Launcher", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 20.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: AD Grenade Launcher", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 21.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: AD Plasma Rifle", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 22.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Defender's Shotgun", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 23.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Centurion's Plasma Gun", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 25.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Flamethrower", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 26.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Poison Sprayer", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 27.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Crossbow", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 28.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Shadow Axe", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 101.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Widowmaker", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 102.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Plasma Gun", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 103.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Death Fury's Sword", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 111.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Death Knight's Sword", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 112.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Zombie Knight's Sword", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 113.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Death Guard's Sword", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 114.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Hunter Ogre's Hammer", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 115.f, 0, 0, nullptr, 0)
QVR_PACK("Monsters", "Armed Grunts", "Armed Grunt: Hunter Ogre's Mace", "monster_vr_grunt", "progs/vrgrunt.mdl", 24.f, "weapon", 116.f, 0, 0, nullptr, 0)

#undef QVR_PACK
