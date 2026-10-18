#!/usr/bin/env python3
"""Writes the generated parts of Arcane Dimensions' monsters' Quake VR support (ARCANE_DIMENSIONS.md section 17) from
one monster table plus standalone pickups: the debug spawner's AD entities (QC/vr_ad_monsters.qc, between its "ad_monsters_gen.py" markers), the Debug
menu's spawns and shared engine catalog (Quake/vr/vr_menu_ad_monsters.inc, between its markers).
    python Misc/quakevr/ad_monsters_gen.py
A monster: its spawner number (200.., in order of AD's maps' use), its classname, its model (the spawner's "installed"
check), its menu name, a menu help line ("" none), optionally MAPS_ONLY (made only in an AD map)."""
import os
import textwrap

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

MAPS_ONLY = True  # a monster made only in an AD map (QVR_AD_SPAWN_MAP: not one of vr_ad.cpp's everywhereClasses)

MONSTERS = [
    (200, "monster_dknight", "progs/mon_dknight.mdl", "A Death Knight",
     "AD's death knight: its ragdoll, beheading, limbs, knockdowns (its death played backwards to get up), parries of its sword."),
    (201, "monster_zombiek", "progs/mon_zombiek.mdl", "A Zombie Knight",
     "AD's zombie knight: only gibbing, beheading or a cut limb kills it for good; knocked down, it gets up as AD's own."),
    (202, "monster_dcrossbow", "progs/mon_dcrossbow.mdl", "A Crossbow Knight", ""),
    (203, "monster_hogre", "progs/mon_hogre.mdl", "A Hunter Ogre", ""),
    (204, "monster_spider", "progs/mon_spiderg.mdl", "A Spider", ""),
    (205, "monster_dguard", "progs/mon_dguard.mdl", "A Death Guard", ""),
    (206, "monster_skullwiz", "progs/mon_skullwiz.mdl", "A Skull Wizard", ""),
    (207, "monster_lostsoul", "progs/mon_lostsoul.mdl", "A Lost Soul", ""),
    (208, "monster_hogremac", "progs/mon_hogremac.mdl", "A Mace Hunter Ogre", ""),
    (209, "monster_boil", "progs/mon_boil.mdl", "A Boil", ""),
    (210, "monster_gaunt", "progs/mon_gaunt.mdl", "A Gaunt", ""),
    (211, "monster_army_rocket", "progs/mon_soldier.mdl", "A Rocket Soldier", ""),
    (212, "monster_defender", "progs/mon_enforcer.mdl", "A Defender", ""),
    (213, "monster_hogreham", "progs/mon_hogreham.mdl", "A Hammer Hunter Ogre", ""),
    (214, "monster_gargoyle", "progs/mon_gargoyle.mdl", "A Gargoyle", ""),
    (215, "monster_minotaur", "progs/mon_minotaur.mdl", "A Minotaur", ""),
    (216, "monster_dfury", "progs/mon_dfury.mdl", "A Death Fury", ""),
    (217, "monster_eliminator", "progs/mon_enforcer.mdl", "An Eliminator", ""),
    (218, "monster_jim", "progs/mon_jim.mdl", "A Jim", ""),
    (219, "monster_swampling", "progs/mon_swampling.mdl", "A Swampling", ""),
    (220, "monster_centurion", "progs/mon_centurion.mdl", "A Centurion", ""),
    (221, "monster_voreling", "progs/mon_voreling.mdl", "A Voreling", ""),
    (222, "monster_golem", "progs/mon_golem.mdl", "A Stone Golem", ""),
    (223, "monster_army_plasma", "progs/mon_soldier.mdl", "A Plasma Soldier", ""),
    (224, "monster_wraith", "progs/mon_wraith.mdl", "A Wraith", ""),
    (225, "monster_dsergeant", "progs/mon_dsergeant.mdl", "A Death Sergeant", ""),
    (226, "monster_drole", "progs/mon_drole.mdl", "A Drole", ""),
    (227, "monster_army_grenade", "progs/mon_soldier.mdl", "A Grenade Soldier", ""),
    (228, "monster_boglord", "progs/mon_bossboglord.mdl", "A Bog Lord", ""),
    (229, "monster_scorpion", "progs/mon_scorpion.mdl", "A Scorpion", ""),
    (230, "monster_seeker", "progs/mon_seeker.mdl", "A Seeker", ""),
    (231, "monster_justice", "progs/mon_justice.mdl", "A Justice Knight", ""),
    (232, "monster_pyro", "progs/mon_pyro.mdl", "A Pyro", ""),
    (233, "monster_eel", "progs/mon_eel.mdl", "AD's Eel", ""),
    (234, "monster_freddie", "progs/mon_freddie.mdl", "A Freddie", ""),
    (235, "monster_fumigator", "progs/mon_fumigator.mdl", "A Fumigator", ""),
    (236, "monster_ogre_fishing", "progs/mon_ogrefish.mdl", "A Fishing Ogre", ""),
    (237, "monster_nour", "progs/mon_bossnour.mdl", "Nour (a Boss)", ""),
    (238, "monster_mummy", "progs/mon_zombie.mdl", "AD's Mummy", ""),
    (239, "monster_icegolem", "progs/mon_bossicegolem.mdl", "An Ice Golem (a Boss)", ""),
    (241, "monster_steelclaw", "progs/mon_steelclaw.mdl", "A Steel Claw", ""),
    (242, "monster_judicator", "progs/mon_judge.mdl", "The Judicator (a Boss)", ""),
    (245, "monster_wizard", "progs/mon_wizard.mdl", "AD's Scrag", ""),
    (250, "monster_hell_knight", "progs/mon_hknight.mdl", "AD's Hell Knight", ""),
    # AD's own variants and its last monsters with data (ROUND22, "The rest of AD's monsters and pickups")
    (251, "monster_gug", "progs/mon_bossboglord.mdl", "A Gug", ""),
    (252, "monster_polyp", "progs/mon_gaunt.mdl", "A Polyp", ""),
    (253, "monster_ogre_flak", "progs/mon_ogre.mdl", "A Nail Ogre", ""),
    (254, "monster_floyd", "progs/mon_floyd.mdl", "A Floyd Automaton", ""),
    (255, "monster_deflector", "progs/mon_deflector.mdl", "The Deflector", ""),
    (256, "monster_dprince", "progs/mon_dprince.mdl", "A Dark Prince", ""),
    (257, "monster_vermis", "progs/mon_bossnour.mdl", "Vermis (a Boss)", ""),
    # AD's versions of Quoth's monsters: only in AD's maps, so a Quoth map's own still spawn nothing
    (258, "monster_death_guard", "progs/mon_dguardq.mdl", "A Quoth Death Guard", "", MAPS_ONLY),
    (259, "monster_death_lord", "progs/mon_dlord.mdl", "A Death Lord", "", MAPS_ONLY),
    (260, "monster_sentinel", "progs/mon_sentinel.mdl", "A Sentinel", "", MAPS_ONLY),
    # AD's knight (id's model; AD's maps' monster_knight): Quake VR's classname, so only in AD's maps
    (261, "monster_knight", "progs/mon_knight.mdl", "AD's Knight", "", MAPS_ONLY),
]


ITEMS = [
    (300, 'Items', 'AD Tome of Power', 'item_artifact_tomeofpower', 'progs/artifact_tome.mdl', '24.f', 'false'),
    (301, 'Items', 'AD Sharp Shooter', 'item_artifact_sharp_shooter', 'progs/artifact_sharp.mdl', '24.f', 'false'),
    (302, 'Items', 'AD Nail Piercer', 'item_artifact_nail_piercer', 'progs/artifact_piercer.mdl', '24.f', 'false'),
    (303, 'Items', 'AD Air Tank', 'item_artifact_airtank', 'progs/artifact_airtank.mdl', '24.f', 'false'),
    (304, 'Items', 'AD Lava Shield', 'item_artifact_lavashield', 'progs/artifact_lavashield.mdl', '24.f', 'false'),
    (305, 'Items', 'AD Blast Belt', 'item_artifact_blastbelt', 'progs/artifact_blastbelt.mdl', '24.f', 'false'),
    (306, 'Items', 'AD Jump Boots', 'item_artifact_jumpboots', 'progs/artifact_jumpboots.mdl', '24.f', 'false'),
    (307, 'Items', 'AD Health Vial', 'item_healthvial', 'progs/health_5.mdl', '0.f', 'false'),
    (308, 'Items', 'AD Health Gem', 'item_healthgem', 'progs/health_5.mdl', '0.f', 'false'),
    (309, 'Weapons', 'AD Shadow Axe', 'weapon_upgrade_axe', 'progs/g_shadaxe.mdl', '8.f', 'true'),
    (310, 'Weapons', 'AD Widowmaker', 'weapon_upgrade_ssg', 'progs/g_shot3.mdl', '8.f', 'true'),
    (311, 'Weapons', 'AD Plasma Gun', 'weapon_upgrade_lg', 'progs/g_plasma.mdl', '8.f', 'true'),
]

# Pickups only in an AD map (Quake's backpack rules outside them: vr_ad.cpp), each with a field set before its spawn
# function: spawner number, section, menu name, classname, model, lift, field, value, the ghost's skin.
MAP_ITEMS = [
    (320, 'Items', 'AD Ammo Backpack', 'item_backpack', 'progs/w_backpack.mdl', '0.f', 'spawnflags', 15, 0),
    (321, 'Items', 'AD Shards Backpack', 'item_backpack_armour', 'progs/w_backpack.mdl', '0.f', 'spawnflags', 0, 4),
    (322, 'Items', 'AD Green Armour Backpack', 'item_backpack_armour', 'progs/w_backpack.mdl', '0.f', 'spawnflags', 1, 1),
    (323, 'Items', 'AD Yellow Armour Backpack', 'item_backpack_armour', 'progs/w_backpack.mdl', '0.f', 'spawnflags', 2, 4),
    (324, 'Items', 'AD Red Armour Backpack', 'item_backpack_armour', 'progs/w_backpack.mdl', '0.f', 'spawnflags', 4, 3),
]

# AD's soldiers' own guns (QC vr_ad_enemyguns.qc, WID 20..27) as func_weapon_grabbable: spawner number, menu name, weapon
# id, the gun's model (our burst rifle's variant: vr_adgunart.cpp), the monster's model (the spawner's "installed" check).
GUNS = [
    (312, "AD Rocket Launcher", 20, "progs/v_gruntgun.mdl#adv0", "progs/mon_soldier.mdl"),
    (313, "AD Grenade Launcher", 21, "progs/v_gruntgun.mdl#adv1", "progs/mon_soldier.mdl"),
    (314, "AD Plasma Rifle", 22, "progs/v_gruntgun.mdl#adv2", "progs/mon_soldier.mdl"),
    (315, "AD Defender's Shotgun", 23, "progs/v_gruntgun.mdl#adv3", "progs/mon_enforcer.mdl"),
    # (316, the eliminator's plasma gun, is gone: Vittorio, 2026-10-18, the same gun as the centurion's, which the
    # eliminator drops now; an older save's is the centurion's, QC VR_ADGun_LegacyIds.)
    (317, "AD Centurion's Plasma Gun", 25, "progs/v_gruntgun.mdl#adv5", "progs/mon_centurion.mdl"),
    (318, "AD Flamethrower", 26, "progs/v_gruntgun.mdl#adv6", "progs/mon_pyro.mdl"),
    (319, "AD Poison Sprayer", 27, "progs/v_gruntgun.mdl#adv7", "progs/mon_fumigator.mdl"),
    # The crossbow knight's crossbow (WID_ADCROSSBOW 28: AD's w_dcrossbow.mdl turned to face +x). (Freddie's cut gun
    # arm drops Quake VR's own super nailgun, weapon 7, QC VR_ADGun_LimbCut: the toolgun's Weapons list has it; its own
    # slot here, 326, is gone, Vittorio 2026-10-18.)
    (325, "AD Crossbow Knight's Crossbow", 28, "progs/w_dcrossbow.mdl#gun0", "progs/w_dcrossbow.mdl"),
    # AD's melee monsters' weapons (QC vr_ad_melee.qc): the knight's sword (13) of a kind (its weapon flags, kind x 256),
    # AD's weapon model laid as the knight's sword (vr_admelee.cpp "#melee").
    (340, "AD Death Fury's Sword", 13, "progs/w_dfurysword1.mdl#melee", "progs/w_dfurysword1.mdl", 256),
    (341, "AD Death Knight's Sword", 13, "progs/w_dknightsword.mdl#melee", "progs/w_dknightsword.mdl", 512),
    (342, "AD Zombie Knight's Sword", 13, "progs/w_zombiesword.mdl#melee", "progs/w_zombiesword.mdl", 768),
    (343, "AD Death Guard's Sword", 13, "progs/w_dguardsword.mdl#melee", "progs/w_dguardsword.mdl", 1024),
    (344, "AD Hunter Ogre's Hammer", 13, "progs/w_ogreham.mdl#melee", "progs/w_ogreham.mdl", 1280),
    (345, "AD Hunter Ogre's Mace", 13, "progs/w_ogremac.mdl#melee", "progs/w_ogremac.mdl", 1536),
]


def replace(path, begin, end, body):
    s = open(path, encoding="utf-8").read()
    a = s.index(begin) + len(begin)
    b = s.index(end, a)
    s = s[:a] + body.rstrip("\n") + "\n" + s[b:]
    open(path, "w", encoding="utf-8", newline="\n").write(s)


def main():
    qc = []
    qc.append("string(float choice) VR_AD_DispenserModel =\n{")
    for n, c, m, *_ in MONSTERS:
        qc.append('    if(choice == %d) { return "%s"; }' % (n, m))
    for n, section, label, c, m, lift, rules in ITEMS:
        qc.append('    if(choice == %d) { return "%s"; }' % (n, m))
    for n, section, label, c, m, *_ in MAP_ITEMS:
        qc.append('    if(choice == %d) { return "%s"; }' % (n, m))
    for n, label, wid, m, base, *fl in GUNS:
        qc.append('    if(choice == %d) { return "%s"; }' % (n, base))
    qc.append('    return "";\n};\n')
    qc.append("float(float choice) VR_AD_DispenserSpawn =\n{\n    if(choice < 200 || choice >= 400)\n    {\n        return FALSE;\n    }")
    qc.append("    if(!cvar(\"vr_ad_mounted\") || (!AD_Rules() && !cvar(\"vr_ad_everywhere\")))\n    {\n        remove(self);\n        return TRUE;\n    }")
    maps = [r[0] for r in MONSTERS if len(r) > 5 and r[5]] + [r[0] for r in MAP_ITEMS]
    qc.append("    if(!AD_Rules() && (%s)) // (only in an AD map: QVR_AD_SPAWN_MAP)\n    {\n        remove(self);\n        return TRUE;\n    }"
              % " || ".join("choice == %d" % n for n in maps))
    qc.append("    if(!AD_Rules()) { VR_AD_WorldOnDemand(); }")
    for i, (n, c, m, *_) in enumerate(MONSTERS):
        qc.append('    %sif(choice == %d) { self.classname = "%s"; ad_%s(); }' % ("" if i == 0 else "else ", n, c, c))
    for n, section, label, c, m, lift, rules in ITEMS:
        fn = c if section == "Weapons" else "ad_" + c
        call = '%s();' % fn  # (an upgrade's pickup removes itself outside Weapon Upgrades' scope: AD_UpgradeItem)
        qc.append('    else if(choice == %d) { self.classname = "%s"; %s }' % (n, c, call))
    for n, section, label, c, m, lift, key, value, skin in MAP_ITEMS:
        qc.append('    else if(choice == %d) { self.classname = "%s"; self.%s = %d; ad_%s(); }' % (n, c, key, value, c))
    for n, label, wid, m, base, *fl in GUNS:
        flags = (' self.weaponflags = %d;' % fl[0]) if fl else ''
        qc.append('    else if(choice == %d) { self.classname = "func_weapon_grabbable"; self.weapon = %d;%s func_weapon_grabbable(); }'
                  % (n, wid, flags))
    qc.append("    else\n    {\n        dprint(sprintf(\"admon: no AD monster %g\\n\", choice));\n        remove(self);\n    }")
    qc.append("    return TRUE;\n};\n")
    replace(os.path.join(ROOT, "QC", "vr_ad_monsters.qc"), "// ad_monsters_gen.py: begin\n", "// ad_monsters_gen.py: end",
            "\n".join(qc))
    menu = []
    for n, c, m, name, help, *_ in MONSTERS:
        line = '        adSpawnRow(%d, command("%s Ahead", "vr_test_spawn %d; vr_test_spawn_dist 128; impulse 241"))' % (n, name, n)
        if help:
            parts = textwrap.wrap(help, 96, drop_whitespace=False)
            line += '\n            .help("' + '"\n                  "'.join(parts) + '"),'
        else:
            line += ","
        menu.append(line)
    replace(os.path.join(ROOT, "Quake", "vr", "vr_menu_ad_monsters.inc"),
            "        // ad_monsters_gen.py: begin (Misc/quakevr/ad_monsters_gen.py writes these)\n",
            "        // ad_monsters_gen.py: end", "\n".join(menu) + "\n")
    rows = ["// Generated by ad_monsters_gen.py: AD support, shared spawn catalog."]
    rows.append("// QVR_AD_SPAWN_MAP(section, label, classname, model, lift, choice, key, value, skin): only in an AD map, not one")
    rows.append("// of vr_ad.cpp's everywhereClasses.")
    for n, c, m, name, _, *flags in MONSTERS:
        label = name.removeprefix("A ").removeprefix("An ").removeprefix("The ")
        if not label.startswith("AD's"): label = "AD " + label
        if flags and flags[0]:
            rows.append('QVR_AD_SPAWN_MAP("Monsters", "%s", "ad_%s", "%s", 24.f, %d, nullptr, 0.f, 0)' % (label, c, m, n))
        else:
            rows.append('QVR_AD_SPAWN("Monsters", "%s", "ad_%s", "%s", 24.f, false, %d)' % (label, c, m, n))
    with open(os.path.join(ROOT, "Quake", "vr", "vr_ad_spawns.inc"), "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(rows) + "\n")
    rows = ["// Generated by ad_monsters_gen.py: AD support, pickups and AD-rules-only upgrades."]
    for n, section, label, c, m, lift, rules in ITEMS:
        rows.append('QVR_AD_SPAWN("%s", "%s", "%s", "%s", %s, %s, %d)' % (section, label, c, m, lift, rules, n))
    for n, section, label, c, m, lift, key, value, skin in MAP_ITEMS:
        rows.append('QVR_AD_SPAWN_MAP("%s", "%s", "ad_%s", "%s", %s, %d, "%s", %d.f, %d)'
                    % (section, label, c, m, lift, n, key, value, skin))
    with open(os.path.join(ROOT, "Quake", "vr", "vr_ad_spawnitems.inc"), "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(rows) + "\n")
    rows = ["// Generated by ad_monsters_gen.py: AD support, AD's soldiers' guns (func_weapon_grabbable, weapon 20..28; not 24,"]
    rows.append("// the eliminator's, now the centurion's 25) and AD's melee monsters' weapons (13, of a kind: its")
    rows.append("// weapon flags, the last argument; the toolgun sets them, or it makes Quake VR's plain knight's sword).")
    for n, label, wid, m, base, *fl in GUNS:
        rows.append('QVR_AD_GUN("%s", "%s", %d, %d, %d)' % (label, m, wid, n, fl[0] if fl else 0))
    with open(os.path.join(ROOT, "Quake", "vr", "vr_ad_spawnguns.inc"), "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(rows) + "\n")
    print("ad_monsters_gen: %d monsters" % len(MONSTERS))


if __name__ == "__main__":
    main()
