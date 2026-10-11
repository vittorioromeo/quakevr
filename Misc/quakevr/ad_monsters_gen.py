#!/usr/bin/env python3
"""Writes the generated parts of Arcane Dimensions' monsters' Quake VR support (ARCANE_DIMENSIONS.md section 17) from
one table: the debug spawner's AD monsters (QC/vr_ad_monsters.qc, between its "ad_monsters_gen.py" markers), the Debug
menu's spawns (Quake/vr/vr_menu_ad_monsters.inc, between its markers).
    python Misc/quakevr/ad_monsters_gen.py
A monster: its spawner number (200.., in order of AD's maps' use), its classname, its model (the spawner's "installed"
check), its menu name, a menu help line ("" none)."""
import os
import textwrap

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

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
    for n, c, m, _, _ in MONSTERS:
        qc.append('    if(choice == %d) { return "%s"; }' % (n, m))
    qc.append('    return "";\n};\n')
    qc.append("float(float choice) VR_AD_DispenserSpawn =\n{\n    if(choice < 200 || choice >= 300)\n    {\n        return FALSE;\n    }")
    qc.append("    if(!AD_Rules())\n    {\n        dprint(\"admon: AD's monsters spawn in a map played by AD's rules only\\n\");\n"
              "        remove(self);\n        return TRUE;\n    }")
    for i, (n, c, m, _, _) in enumerate(MONSTERS):
        qc.append('    %sif(choice == %d) { self.classname = "%s"; ad_%s(); }' % ("" if i == 0 else "else ", n, c, c))
    qc.append("    else\n    {\n        dprint(sprintf(\"admon: no AD monster %g\\n\", choice));\n        remove(self);\n    }")
    qc.append("    return TRUE;\n};\n")
    replace(os.path.join(ROOT, "QC", "vr_ad_monsters.qc"), "// ad_monsters_gen.py: begin\n", "// ad_monsters_gen.py: end",
            "\n".join(qc))
    menu = []
    for n, c, m, name, help in MONSTERS:
        line = '        command("%s Ahead", "vr_test_spawn %d; vr_test_spawn_dist 128; impulse 241")' % (name, n)
        if help:
            parts = textwrap.wrap(help, 96, drop_whitespace=False)
            line += '\n            .help("' + '"\n                  "'.join(parts) + '"),'
        else:
            line += ","
        menu.append(line)
    replace(os.path.join(ROOT, "Quake", "vr", "vr_menu_ad_monsters.inc"),
            "        // ad_monsters_gen.py: begin (Misc/quakevr/ad_monsters_gen.py writes these)\n",
            "        // ad_monsters_gen.py: end", "\n".join(menu) + "\n")
    print("ad_monsters_gen: %d monsters" % len(MONSTERS))


if __name__ == "__main__":
    main()
