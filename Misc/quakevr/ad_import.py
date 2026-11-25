#!/usr/bin/env python3
"""Imports Arcane Dimensions' QuakeC into Quake VR's progs, namespaced (docs/vr-port/ARCANE_DIMENSIONS.md, 5.2).

    python Misc/quakevr/ad_import.py [--release DIR]

DIR holds the unpacked official release ad_v1_80p1final.zip (pak0.pak, pak1.pak, pak2.pak, ad_1_8.fgd); default
scratch/ad/ad. The QC is read from the paks (my_progs/, patch 1's pak2 first), which are checked against the pinned
SHA-256 below, so the output is reproducible. AD's QC is GPL-2.0-or-later (its readme): the generated files keep
AD's headers and credits; AD's assets are never copied.

    python Misc/quakevr/ad_import.py --verify-frames   (after a build: AD's frame numbers vs AD's own progs.dat)
    python Misc/quakevr/ad_import.py --check [DIR]     (writes to DIR, default scratch/ad_check, not QC/ad; diffs
                                                        it against QC/ad and progs.src; exit 1 on any difference)

Before committing any change under QC/ad: make it here (PATCHES, FUNC_PATCHES or a hook in main), regenerate, and
run --check (it must say 0 files differ). A hand edit in QC/ad is lost on the next import.

The import itself (renaming, headers, builtins, progs.src, --check) is qc_import.py's, shared with copper_import.py;
this script holds AD's configuration: its release pin, its reviewed patches and its report lines.

What it writes (QC/ad/, regenerated whole; never edit by hand, change this script):
  ad_builtins.qc   the extension builtins AD declares that Quake VR's QC does not: "#0", bound by name by the
                   engine when it has them (pr_edict.c PR_InitBuiltins), else an error only if called; standard
                   builtins AD declares under its own names or signatures keep their numbers (ad_strcat = #115)
  ad_<file>.qc     each AD source file, every identifier AD defines renamed with the prefix "ad_"
  AD_IMPORT.txt    the source hashes, what was kept, renamed, dropped (read it when re-importing)
and the block between the "ad_import.py" markers at the end of QC/progs.src: one entry per file, in AD's order
(fteqcc numbers $frame macros per progs.src entry; an #include would shift AD's frames).

Renaming: every identifier token outside strings, comments and $frame directives becomes ad_<name>, except
  - QC keywords and types, compiler intrinsics (__*), vector components (x, y, z);
  - the engine's system globals and fields (Quake VR's defs.qc before end_sys_fields) and their _x/_y/_z parts;
  - builtins Quake VR's QC already declares (AD's own declarations of them are dropped);
  - AD fields that are map keys (AD's FGD or AD's maps use them) and that Quake VR already has with the same type
    (target, wait, delay, alpha...): one field, as the map sets it.
So AD's spawn functions are ad_<classname> too: nothing of AD spawns, runs or is called by the engine until the
bridge (QC/vr_ad_bridge.qc) or the engine's spawn hook asks for it. AD's own system section, its builtin
declarations and its extension headers (dpextensions.qc: only what AD's code uses is kept) are not copied; the
CSQC HUD (csqc_*) is left out.
"""
import argparse
import hashlib
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # (run with python -I: the source is untrusted data)
import qc_import  # noqa: E402
from qc_import import pak_files, read_pak, sha256

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
QC_DIR = os.path.join(ROOT, "QC")
OUT_DIR = os.path.join(QC_DIR, "ad")


# ad_v1_80p1final.zip (SHA-256 7ad993da6c760c446ca31fad71e9f5b6c9eee99b6354f161aa746b572fe70a7d), unpacked.
PINNED = {
    "pak0.pak": "117caa759bf9f161599ff693f561c68954519eda9cefb719a97cd5c1cd863ef3",
    "pak2.pak": "4c82e0347952b38b0265fd34e0c3c3a6b6b4bea17642d18bf4189a97d93dd0f1",
    "ad_1_8.fgd": "2fdaef42f02c37370b36bd973e9ebfe08827f5847e26b4c250dd9b521a6b8431",
}
# Files of AD's progs.src not imported: the CSQC HUD (Quake VR draws its own) and extension headers whose needed
# parts are filtered in (HEADERS) rather than copied whole.
SKIP = {"csqc_hudvanilla.qc"}
# Declarations only: copied statement by statement, only what the imported code uses (transitively).
HEADERS = {"defs.qc", "dpextensions.qc", "csqc_defs.qc", "csqc_hudad.qc"}
PREFIX = "ad_"
BRIDGE = "VR_AD_"  # the bridge's functions the patches below call (QC/vr_ad_bridge.qc): never renamed
# Reviewed edits to AD's source, applied before renaming: (file, exact text, replacement, why). Each must match once.
#
# The precedence patches (qvr_checks qc_precedence): expressions that read differently under C priorities,
# parenthesised as fteqcc's QC priorities (AD's own compile, and ours) read them, so behaviour stays AD's as shipped
# (some look like AD bugs: kept, flagged "AD as shipped"). Each was verified to compile to the same instructions.
# The imports' one rule for a look-alike bug (DWELL.md 9.7; Copper's the same): kept as shipped, unless the shipped
# reading changes what a Quake VR player gets and the mod's intent is plain, then fixed as meant (AD's bugs 4 and 5).
P = "parenthesised as QC priorities read it (check_qc_precedence)"
B = "bridge"
PATCHES = [
    ('func_buttons.qc', 'else self.touch = func_button_touch;', 'else { self.touch = func_button_touch; VR_AD_ButtonSpawned(); }', 1, 'bridge: AD VR touch, carry and rung-only climbing'),
    ('triggers.qc', 'void() trigger_ladder_fire =\n{', 'void() trigger_ladder_fire =\n{\n\tif (VR_AD_LadderManual(self.bmodel_act)) return;', 1, 'bridge: AD VR touch, carry and rung-only climbing'),
    ('items.qc', 'void() item_restore =\n{', 'void() item_restore =\n{\n\tif (VR_AD_BackpackRestore()) return;', 1, 'bridge: AD VR touch, carry and rung-only climbing'),
    ('items.qc', '\titem.nextthink = time + 0.3;\t\t// Let backpack drop first', '\titem.nextthink = time + 0.3;\t\t// Let backpack drop first\n\tVR_AD_BackpackSetup(item);', 1, 'bridge: AD VR touch, carry and rung-only climbing'),
    ("client_debuff.qc", "if (targ.flags & FL_CLIENT == FALSE) return;", "if ((targ.flags & FL_CLIENT) == FALSE) return;", 3, P),
    ("client_debuff.qc", "if (self.items & ALL_ITARTIFACTS == 0) {", "if ((self.items & ALL_ITARTIFACTS) == 0) {", 1, P),
    # AD's shipped bugs 5 and 4 (ARCANE_DIMENSIONS.md 15.5), fixed as AD meant them now that its tints are drawn
    # (Vittorio): burning's colour gives way to bleeding and to poison; a book darkens the screen only while no debuff
    # colour and no powerup is showing.
    ("client_debuff.qc", "if (!self.bleeding && !self.poisoned) {", "if ((!self.bleeding) && (!self.poisoned)) {", 1,
     "AD's bug 5 fixed (QC priorities read it as !(bleeding && !poisoned))"),
    ("misc_textbook.qc", "if (!self.enemy.cshift_upd && self.enemy.items & ALL_ITARTIFACTS == 0) {",
     "if ((!self.enemy.cshift_upd) && ((self.enemy.items & ALL_ITARTIFACTS) == 0)) {", 2,
     "AD's bug 4 fixed (QC priorities read it as !(cshift_upd && no artifacts))"),
    ("mon_bossmammoth.qc", "if (self.walkframe >= 4  && self.walkframe <= 8 ||\n\t\t\tself.walkframe >= 11 && self.walkframe <= 15)",
     "if (((self.walkframe >= 4  && self.walkframe <= 8) ||\n\t\t\tself.walkframe >= 11) && self.walkframe <= 15)", 1,
     P + "; AD as shipped"),
    ("monsters.qc", "if (self.deadflag == DEAD_REMOVE || self.deadflag == DEAD_EXPLODE\n",
     "if ((self.deadflag == DEAD_REMOVE || self.deadflag == DEAD_EXPLODE)\n", 1, P),
    ("monsters.qc", "if (self.spawnflags & MON_ONFLOOR && self.classgroup == CG_ZOMBIE) {",
     "if ((self.spawnflags & MON_ONFLOOR) && self.classgroup == CG_ZOMBIE) {", 1, P),
    ("monsters.qc", "if (self.classtype == CT_MONGARGOYLE && self.spawnflags & MON_GARGOYLE_PERCH ||\n"
     "\t\tself.classtype == CT_MONGAUNT && self.spawnflags & MON_GAUNT_PERCH ) \n",
     "if (((self.classtype == CT_MONGARGOYLE && (self.spawnflags & MON_GARGOYLE_PERCH)) ||\n"
     "\t\tself.classtype == CT_MONGAUNT) && (self.spawnflags & MON_GAUNT_PERCH) ) \n", 1, P + "; AD as shipped"),
    ("monsters.qc", "if (self.classtype == CT_MONSPIDER && self.spawnflags & MON_SPIDER_CEILING ||\n"
     "\t\tself.classtype == CT_MONSWAMPLING && self.spawnflags & MON_SWAMPLING_CEILING ||\n"
     "\t\tself.classtype == CT_MONVORELING && self.spawnflags & MON_VORELING_CEILING) {",
     "if (((((self.classtype == CT_MONSPIDER && (self.spawnflags & MON_SPIDER_CEILING)) ||\n"
     "\t\tself.classtype == CT_MONSWAMPLING) && (self.spawnflags & MON_SWAMPLING_CEILING)) ||\n"
     "\t\tself.classtype == CT_MONVORELING) && (self.spawnflags & MON_VORELING_CEILING)) {", 2, P + "; AD as shipped"),
    ("triggers.qc", "if (query_configflag(SVR_RUNE_ALL) & self.customkey == self.customkey) {",
     "if ((query_configflag(SVR_RUNE_ALL) & self.customkey) == self.customkey) {", 1, P),
    ("misc.qc", "self.health = self.gibhealth = -1;", "self.gibhealth = -1; self.health = -1;", 1, P),
    ("misc.qc", "if (self.count == TE_LIGHTNING1 || self.count == TE_LIGHTNING2 || \n\t\tself.count == TE_LIGHTNING3 && !self.movetarget) {",
     "if ((self.count == TE_LIGHTNING1 || self.count == TE_LIGHTNING2 || \n\t\tself.count == TE_LIGHTNING3) && !self.movetarget) {",
     1, P + "; AD as shipped"),
] + [
    # The bridge's hooks (P1b): AD's code calls Quake VR's where the two meet. The VR_AD_ functions are QC/vr_ad_bridge.qc's
    # (the renaming keeps every VR_AD_ name). Keep this list short: everything else stays AD's as shipped.
    ("world.qc", "ext_active = cvar(\"pr_checkextension\");\t// Check for extra extensions",
     "ext_active = VR_AD_Extensions();\t// Check for extra extensions", 1,
     B + ": AD's Fitz path (sprite particles, no CSQC stats); our engine's extensions are not AD's"),
    ("world.qc", "cvar_set (\"r_wateralpha\", ftos(liquid_alpha));", "VR_AD_WaterAlpha(liquid_alpha);", 1,
     B + ": never the player's r_wateralpha; the engine reads the water_alpha key (gl_rmisc.c)"),
    ("globalfog.qc", "\tstuffcmd(client, \"\\n\\n\\nfog \");",
     "\tif (VR_AD_Fog(client, fog_control.fog_currden, fog_control.fog_currcol)) return;\n\tstuffcmd(client, \"\\n\\n\\nfog \");",
     1, B + ": fog by svc_fog (stuffed commands are filtered), as MG's"),
    # The sky's changes as the engine's own messages (P1b's loose ends): a trigger_skybox's sky to every client by
    # svc_skybox (and again after a load: AD_Frame), the sky fog by the player's STAT_QVR_ADSKYFOG (kept in saves).
    ("targets.qc", "\tstuffcmd(targ, \"sky \");",
     "\tif (VR_AD_Skybox(targ)) return;\n\tstuffcmd(targ, \"sky \");",
     1, B + ": a trigger_skybox's sky by svc_skybox to every client (not console text to one)"),
    ("globalfog.qc", "\tstuffcmd(client, \"\\n\\n\\nr_skyfog \");",
     "\tif (VR_AD_SkyFog(client, skyfog_control.skyfog_currden)) return;\n\tstuffcmd(client, \"\\n\\n\\nr_skyfog \");",
     1, B + ": the sky fog by the player's STAT_QVR_ADSKYFOG (not the player's r_skyfog)"),
    ("ai_combat.qc", "\tlocal float save, take, flinch, noarmordmg;\n",
     "\tlocal float save, take, flinch, noarmordmg;\n\tif (VR_AD_DamageOurs(targ, inflictor, attacker, damage, checkarmor)) return;\n",
     1, B + ": Quake VR's own entities (the player, our monsters) take damage by our T_Damage"),
    ("ai_combat.qc", "\ttarg.health = targ.health - take;\n",
     "\ttarg.health = targ.health - take;\n\tVR_AD_Splash(checkarmor == DAMSPLASH);\n"
     "\tVR_AD_Hit(targ, inflictor, attacker, take);\n\tVR_AD_Splash(0);\n",
     1, B + ": our hit hooks on AD's monsters (damage numbers, gore, small gibs, wounds)"),
    ("ai_pathcorner.qc", "void() path_corner_touch =\n{\n",
     "void() path_corner_touch =\n{\n\tif (VR_Rules_PathCornerTouch()) return;\n",
     1, B + ": our monsters follow AD's path_corners as ours (t_movetarget)"),
    ("items.qc", "void() weapon_touch =\n{\n\tlocal float pickupweapon, pickupmodweapon, pickupammo, moditem;\n",
     "void() weapon_touch =\n{\n\tlocal float pickupweapon, pickupmodweapon, pickupammo, moditem;\n\tif (VR_AD_WeaponTouch()) return;\n",
     1, B + ": never AD's weapon pickup on a Quake VR player (it sets .weapon, our hands' weapon id); ours are Quake VR's"),
    ("weapons.qc", "void(entity targ) W_SetCurrentAmmo =\n{\n",
     "void(entity targ) W_SetCurrentAmmo =\n{\n\tif (VR_AD_SetCurrentAmmo(targ)) return;\n",
     1, B + ": Quake VR players keep both hands' weapon models and ammo ids after AD ammo/backpack pickups"),
    ("ai_projectiles.qc", "\t\telse self.dmg = DAMAGE_PSHELL;",
     "\t\telse self.dmg = DAMAGE_PSHELL * VR_AD_PelletDamage(self);",
     1, B + ": the pellet damage settings (vr_ad_pellet_damage, Weapon Damage)"),
    # The follow-ups of decisions 16-30 (ARCANE_DIMENSIONS.md section 16; QC/vr_ad_items.qc, vr_ad_player.qc).
    ("items.qc", "\t// Update player item flags (regular + mod)\n\tother.items = other.items | self.items;\n",
     "\t// Update player item flags (regular + mod)\n\tother.items = other.items | VR_AD_ArtifactItems(self);\n",
     1, B + ": the Sharp Shooter's and Nail Piercer's Quad bit (AD's shared HUD icon) is Quake VR's Quad itself (its "
     "tint, icon, glow): not given"),
    ("client_debuff.qc", "void(entity targ) BurnDeBuff =\n{\n\tlocal float debuffqty;\n",
     "void(entity targ) BurnDeBuff =\n{\n\tlocal float debuffqty;\n\tif (VR_AD_BurnDebuff(targ)) return;\n",
     1, B + ": AD's burning is Quake VR's (decision 24): its fire on the player, AD's time and damage"),
    # AD's skill 4 (Evil) is Quake VR's Nightmare plus vr_ad_evil (decision 30): the engine and Quake VR's monsters know
    # skills 0-3 only. Every place AD sets the skill cvar.
    ("func.qc", "\t\tcvar_set (\"skill\", ftos(self.style));\n", "\t\tVR_AD_SetSkill(self.style);\n",
     1, B + ": the hub's skill choice; Evil as Nightmare plus vr_ad_evil"),
    ("triggers.qc", "\tcvar_set (\"skill\", self.message);\n", "\tVR_AD_SetSkill(stof(self.message));\n",
     1, B + ": trigger_setskill; Evil as Nightmare plus vr_ad_evil"),
    ("settings.qc", "if (cvar(\"scratch4\") > 0) cvar_set(\"skill\", ftos(SKILL_EVIL));",
     "if (cvar(\"scratch4\") > 0) VR_AD_SetSkill(SKILL_EVIL);",
     1, B + ": AD's Evil flag restored; Evil as Nightmare plus vr_ad_evil"),
    ("triggers.qc", "\tself.estate_fire = trigger_ladder_fire;\n\ttrigger_bmodel_setup();\n};",
     "\tself.estate_fire = trigger_ladder_fire;\n\ttrigger_bmodel_setup();\n\tVR_AD_LadderSpawned();\n};",
     1, B + ": the ladder's volume climbed by hand (Quake VR's climbing, decision 26)"),
    # Crouching (decision 20): AD's sight looks for Quake VR's crouched player at his real eyes (VR_SightOfs).
    ("ai_subs.qc", "spot2 = targ.origin + targ.view_ofs;", "spot2 = targ.origin + VR_AD_ViewOfs(targ);",
     5, B + ": a crouched VR player seen at his real eyes (vr_crouch_sight)"),
    ("ai_checkattack.qc", "self.enemy.view_ofs", "VR_AD_ViewOfs(self.enemy)",
     3, B + ": a crouched VR player seen at his real eyes (vr_crouch_sight)"),
    ("client_evilmode.qc", "void() Client_SetEvilSkill = { cvar_set(\"skill\", ftos(SKILL_EVIL)); skill = SKILL_EVIL; };",
     "void() Client_SetEvilSkill = { VR_AD_SetSkill(SKILL_EVIL); skill = SKILL_EVIL; };",
     1, B + ": Evil as Nightmare plus vr_ad_evil"),
    # AD's screen tints (section 16.14): every v_cshift AD stuffs is VR_AD_Tint's (vr_ad_player.qc), drawn by the engine
    # (stats, eased, vr_ad_tints), not console text.
    ("client_debuff.qc", "\tstuffcmd(targ, \"v_cshift 0 0 0 0\\n\");\n", "\tVR_AD_Tint(targ, '0 0 0', 0, 0);\n",
     1, B + ": the debuff tint cleared (VR_AD_Tint)"),
    ("client_debuff.qc", "\t\t\t\t\tstuffcmd(self, \"v_cshift \");\n",
     "\t\t\t\t\tif (VR_AD_Tint(self, self.cshift_col, self.cshift_den, 0)) return;\n\t\t\t\t\tstuffcmd(self, \"v_cshift \");\n",
     1, B + ": the debuff's colour (VR_AD_Tint)"),
    ("client_cshift.qc", "\tstuffcmd(targ, \"v_cshift 0 0 0 0\\n\");\n", "\tVR_AD_Tint(targ, '0 0 0', 0, 0);\n",
     1, B + ": the tint cleared (VR_AD_Tint)"),
    ("misc_textbook.qc", "\t\tstuffcmd(self.enemy, \"v_cshift 0 0 0 \");\n",
     "\t\tif (VR_AD_Tint(self.enemy, '0 0 0', self.lip, 0)) return;\n\t\tstuffcmd(self.enemy, \"v_cshift 0 0 0 \");\n",
     1, B + ": the book's dark background (VR_AD_Tint)"),
    ("misc_textbook.qc", "\t\t\t\tstuffcmd(self.enemy, \"v_cshift 0 0 0 0\\n\");\n",
     "\t\t\t\tVR_AD_Tint(self.enemy, '0 0 0', 0, 0);\n", 1, B + ": the book's background cleared (VR_AD_Tint)"),
    ("misc_camera.qc", "\tstuffcmd(targ, \"v_cshift \");\n",
     "\tif (VR_AD_Tint(targ, cs_col, cs_den, 1)) return;\n\tstuffcmd(targ, \"v_cshift \");\n",
     1, B + ": the cinematic camera's fades (VR_AD_Tint, not scaled by Screen Tints)"),
    # The Shadow Axe, the Widowmaker and the Plasma Gun are upgrades of a weapon here (its record's), never the player's
    # .moditems (vr_ad_bridge.qc AD_UpgradesToWeapons): AD's two questions of the player's .moditems that gate on them.
    ("monsters.qc", "\telse if (attacker.moditems & IT_UPGRADE_AXE && self.axhitme > 0 && self.pain_longanim) self.pain_check = 2;",
     "\telse if ((attacker.moditems & IT_UPGRADE_AXE || VR_AD_ShadowAxeHit()) && self.axhitme > 0 && self.pain_longanim) self.pain_check = 2;",
     1, B + ": the Shadow Axe's long pain, from the blow's mark (AD_AxeMark: the axe's upgrade is its own)"),
    ("triggers_states.qc", "\t\tif (self.upgrade_axe && !(self.bmodel_act.moditems & IT_UPGRADE_AXE)) return;\n"
     "\t\tif (self.upgrade_ssg && !(self.bmodel_act.moditems & IT_UPGRADE_SSG) ) return;\n"
     "\t\tif (self.upgrade_lg && !(self.bmodel_act.moditems & IT_UPGRADE_LG) ) return;",
     "\t\tif (self.upgrade_axe && !VR_AD_HasUpgrade(self.bmodel_act, IT_UPGRADE_AXE)) return;\n"
     "\t\tif (self.upgrade_ssg && !VR_AD_HasUpgrade(self.bmodel_act, IT_UPGRADE_SSG)) return;\n"
     "\t\tif (self.upgrade_lg && !VR_AD_HasUpgrade(self.bmodel_act, IT_UPGRADE_LG)) return;",
     1, B + ": a trigger's upgrade_axe/ssg/lg keys: an upgraded weapon carried (VR_AD_HasUpgrade)"),
    ("func_plats.qc", "void() func_plat_blocked =\n{\n", "void() func_plat_blocked =\n{\n\tVR_AD_ShaftBlocked(other);\n",
     1, B + ": a monster or bot in a blocked lift's way walks out of its shaft (VR_AD_ShaftBlocked; NAVMESH.md 24)"),
    ("func_doors.qc", "void() func_door_blocked =\n{\n", "void() func_door_blocked =\n{\n\tVR_AD_ShaftBlocked(other);\n",
     1, B + ": a monster or bot in a blocked lift's way walks out of its shaft (VR_AD_ShaftBlocked; NAVMESH.md 24)"),
    ("func_trains.qc", "void() func_train_blocked =\n{\n", "void() func_train_blocked =\n{\n\tVR_AD_ShaftBlocked(other);\n",
     1, B + ": a monster or bot in a blocked lift's way walks out of its shaft (VR_AD_ShaftBlocked; NAVMESH.md 24)"),
]
# The monsters' hooks (P2, ARCANE_DIMENSIONS.md section 17): AD's monsters get Quake VR's treatment (QC/vr_ad_monsters.qc;
# their declarations in QC/vr_ad_mondefs.qc). Applied after the bridge's (the T_Damage line is the bridge's patched one).
MON_PATCHES = [
    ("monsters.qc", "void() monster_start =\n{\n", "void() monster_start =\n{\n\tVR_AD_MonsterSetup();\n",
     1, B + ": the monster's Quake VR data by its class (head zone, knockdown, corpse, parries)"),
    ("ai_combat.qc", "\ttarg.health = targ.health - take;\n",
     "\tVR_AD_KdSave(targ, inflictor);\n\ttarg.health = targ.health - take;\n",
     1, B + ": a knocked-down monster's frame and think kept from AD's reaction (put back by VR_AD_PainDown); "
     "the blow's inflictor for the highlight log"),
    ("ai_combat.qc", "\t//----------------------------------------------------------------------\n"
     "\t// Check for ammo type resistance and immunity to pain\n",
     "\tif (VR_AD_PainDown(targ)) return;\n\t//----------------------------------------------------------------------\n"
     "\t// Check for ammo type resistance and immunity to pain\n",
     1, B + ": knocked down: no pain animation (its ragdoll), longer down"),
    ("ai_combat.qc", "\t\t\tif (self.flags & FL_MONSTER) monster_death_use();\n\t\t\tself.th_die ();\n",
     "\t\t\tif (self.flags & FL_MONSTER) monster_death_use();\n\t\t\tVR_AD_Dying(attacker);\n\t\t\tself.th_die ();\n"
     "\t\t\tVR_AD_Died();\n",
     1, B + ": Quake VR's dying around AD's death function (beheading, limbs, knockdown's death, corpse, gore)"),
    ("mon_bossicegolem.qc", "void() iceg_setup =\n{\n", "void() iceg_setup =\n{\n\tVR_AD_MonsterSetup();\n",
     1, B + ": the ice golem (a boss: no monster_start) registered as it wakes"),
    ("monsters.qc", "\tself.solid = SOLID_TRIGGER;\n\tself.touch = SUB_Null;\n",
     "\tself.solid = SOLID_TRIGGER;\n\tVR_AD_BodySolid();\n\tself.touch = SUB_Null;\n",
     1, B + ": the body touchable, not a trigger (its ragdoll and corpse are Quake VR's)"),
]
PATCHES = PATCHES + MON_PATCHES
# Reviewed edits inside one function (its body, "<type> <name> =\n{" to the next "\n};"): (file, function, exact text,
# replacement, why). The text must match once in that body (shared lines like AD's "Check for any custom light styles"
# repeat across functions).
_FIRE = "\t// Check for any custom light styles\n"
_FIRE_WHY = B + ": its flame heats blades and lights torches again (vr_walltorch.qc)"
FUNC_PATCHES = [
    ("lights.qc", "light_" + n, _FIRE, _FIRE + "\tVR_AD_FireSpot(%d); // QVR: its flame heats blades and lights torches again "
     "(vr_walltorch.qc)\n" % large, _FIRE_WHY)
    for n, large in (("torch_small_walltorch", 2), ("flame_large_yellow", 1), ("flame_small_yellow", 0), ("flame_pyre", 1),
                     ("flame_brazier_short", 0), ("flame_brazier_tall", 0))
] + [
    # AD support: Quake VR's stealth AI (STEALTH.md; vr_ad_stealth.qc) on AD's monsters: AD's FindTarget sees a player
    # through the suspicion meter, AD's stand and walk run the Alert state, AD's FoundTarget is Hostile, AD's run loses him.
    ("ai_subs.qc", "FindTarget", "\tself.enemyexit = FALSE;\n",
     "\tself.enemyexit = FALSE;\n\tif (VR_AD_StealthTick()) return TRUE;\n",
     B + ": stealth: the meter drains, beams, torches and bodies seen (Hostile now: found)"),
    ("ai_subs.qc", "FindTarget", "\tenemy_range = range (client);\n",
     "\tif (VR_AD_StealthSight(client)) return VR_AD_StealthSee(client);\n\tenemy_range = range (client);\n",
     B + ": stealth: a player is seen by the suspicion meter, not at once"),
    ("ai_subs.qc", "FoundTarget", "\tif (self.health < 1) return FALSE;\t// Dead monsters don't hunt!\n",
     "\tif (self.health < 1) return FALSE;\t// Dead monsters don't hunt!\n\tVR_AD_StealthHostile();\n",
     B + ": stealth: Hostile (Alert no more; the monsters near told)"),
    ("ai_subs.qc", "FoundHuntTarget", "\tif (check_passivestate_switch(self)) return;\n",
     "\tif (check_passivestate_switch(self)) return;\n\tVR_AD_StealthHostile();\n",
     B + ": stealth: Hostile (Alert no more; the monsters near told)"),
    ("ai_states.qc", "ai_stand", "\tif (FindTarget ()) return;\t\t\t// Found a player?\n",
     "\tif (FindTarget ()) return;\t\t\t// Found a player?\n\tif (VR_AD_StealthStand()) return;\n",
     B + ": stealth: Alert: turning to look, searching"),
    ("ai_states.qc", "ai_walk", "\tif (FindTarget ()) return;\t\t\t// Found a player?\n",
     "\tif (FindTarget ()) return;\t\t\t// Found a player?\n\tif (VR_AD_StealthWalk(dist)) return;\n",
     B + ": stealth: Alert: walking to what it noticed, or back to its post"),
    ("ai_states.qc", "ai_run", "\t\tself.enemylastseen = time;\n\t}\n",
     "\t\tself.enemylastseen = time;\n\t}\n\tif (VR_AD_StealthLose(enemy_vis)) return;\n",
     B + ": stealth: lost him a while: Alert, to where it saw him last"),
] + [
    # AD support: living hit reactions (vr_hitreact.qc) on AD's monsters: struck a moment ago, shorter steps, a leg hit's
    # hitch to that side (as ai.qc's ai_walk and ai_run; the blow itself is sent by VR_AD_Hit).
    (f, fn, "\tmovedist = dist;\n", "\tdist = VR_HitReact_Dist(dist); // VR: struck a moment ago: shorter steps, a leg hit's "
     "hitch (vr_hitreact.qc)\n\tmovedist = dist;\n", B + ": hit reactions: struck a moment ago, shorter steps (a leg hit: a limp)")
    for f, fn in (("ai_states.qc", "ai_walk"), ("ai_states.qc", "ai_run"))
] + [
    # AD support: a monster gibbed by AD's code gets id's gibbing's gore, small gibs and limbs, and its head and gibs are
    # Quake VR's props (carried, rigid bodies, burst by damage; vr_ad_monsters.qc VR_AD_Gibbed, VR_AD_GibThrown).
    ("ai_gibs.qc", "monster_ThrowGib", "\tself.gibbed = TRUE;\t\t// Monster has offical gone to pieces ;)\n",
     "\tself.gibbed = TRUE;\t\t// Monster has offical gone to pieces ;)\n\tVR_AD_Gibbed();\n",
     B + ": gibbed: Quake VR's gore, small gibs and limbs"),
    ("ai_gibs.qc", "ThrowGib", "\t\tgibitem.think = RemoveGib;\n",
     "\t\tgibitem.think = RemoveGib;\n\t\tVR_AD_GibThrown(gibitem, gib_no);\n",
     B + ": its gibs and head Quake VR's props (grabbed, rigid bodies, burst by damage)"),
]

# Quake VR's own features on AD's code (not bridge functions): the names they call are Quake VR's (QVR_NAMES, kept as
# written; AD's own names in the replacements are renamed as usual, so FALSE reads ad_FALSE).
Q = "Quake VR"
QVR_NAMES = {"VR_MeleeArc_Hits", "VR_MeleeBlow", "VR_MeleeReach", "VR_MeleeArc_Launch", "VR_MeleeArc_TurnBegin", "VR_MeleeArc_TurnEnd",
             "VR_MeleeSpace_Step", "vr_melee_time", "vr_spawn_live", "VR_ADGun_IsElectric", "VR_ADGun_BoltFx",
             "VR_ADGun_BoltShock", "VR_ADGun_StreamFx", "VR_ADGun_StreamMuzzle", "VR_ADGun_Poison", "VR_Hit_BeginAs", "VR_Hit_End",
             "VR_Proj_Begin", "VR_Proj_End", "VR_Proj_Mark", "QVR_PROJ_LASER", "QVR_PROJ_BOLT", "QVR_PROJ_BLAST"}
QVR_NAMES |= {"VR_HitReact_Dist"}  # (the hit reactions' patches above)
QVR_NAMES |= {"VR_Rules_PathCornerTouch"}  # (our monsters on AD's path_corners: every ruleset's, vr_rules_monsters.qc)
QVR_NAMES |= {"VR_ADGun_FlameDoused", "VR_ADGun_FlameScorch"}  # (the pyro's flames: water, scorch)
_STAMP = "\tself.vr_melee_time = time; // QVR: kept out of the player's head meanwhile (vr_enemy_melee_space)\n"
_STEP = " // QVR: (VR_MeleeSpace_Step: fight.qc)"
_FACE = "VR_MeleeArc_TurnBegin(); %s VR_MeleeArc_TurnEnd(); // AD support: melee arc: the swing's turn held (fight.qc)"
_ARC = Q + ": melee arc (fight.qc): the swing's turn held, its hits only in front"
_SPACE = Q + ": melee space (fight.qc VR_MeleeSpace_Step): kept out of the player's head"
FUNC_PATCHES += [
    # Melee arc and melee space (fight.qc): AD's charges and swings as id's monsters'.
    # (A function body ends before the "\n};": a patch at its end has no trailing newline.)
    ("ai_subs.qc", "ai_charge", "\tai_face ();\n\tmovetogoal (dist);",
     _STAMP + "\tVR_MeleeArc_TurnBegin(); // AD support: melee arc: the swing's turn held, ai_face's and movetogoal's "
     "step's (fight.qc)\n\tai_face ();\n\tlocal vector before = self.origin;" + _STEP + "\n\tmovetogoal (dist);\n"
     "\tVR_MeleeArc_TurnEnd(); // AD support: (melee arc)\n\tVR_MeleeSpace_Step(before);", _ARC),
    ("ai_subs.qc", "ai_chargenoturn", "\tmovetogoal (dist);",
     _STAMP + "\tlocal vector before = self.origin;\n\tVR_MeleeArc_TurnBegin(); // AD support: melee arc: movetogoal's "
     "step turns it too (fight.qc)\n\tmovetogoal (dist);\n\tVR_MeleeArc_TurnEnd(); // AD support: (melee arc)\n"
     "\tVR_MeleeSpace_Step(before);", _ARC),
    ("ai_subs.qc", "ai_charge_side", "\n\tlocal\tvector\tdtemp;\n", "\n" + _STAMP + "\tlocal\tvector\tdtemp;\n", _SPACE),
    ("ai_subs.qc", "ai_charge_side", "\tChangeYaw ();\n", "\t" + _FACE % "ChangeYaw ();" + "\n", _ARC),
    ("ai_subs.qc", "ai_charge_side", "\twalkmove(heading, 20);",
     "\tdtemp = self.origin;" + _STEP + "\n\twalkmove(heading, 20);\n\tVR_MeleeSpace_Step(dtemp);", _SPACE),
    ("ai_subs.qc", "ai_charge_front", "\n\tlocal\tfloat\theading;\n", "\n" + _STAMP + "\tlocal\tfloat\theading;\n", _SPACE),
    ("ai_subs.qc", "ai_charge_front", "\tai_face();", "\t" + _FACE % "ai_face();", _ARC),
    ("ai_subs.qc", "ai_charge_front", "\twalkmove(heading, 10);",
     "\tlocal vector before = self.origin;" + _STEP + "\n\twalkmove(heading, 10);\n\tVR_MeleeSpace_Step(before);",
     _SPACE),
    ("ai_subs.qc", "ai_melee", "\n\tlocal float ldmg;\n", "\n" + _STAMP + "\tlocal float ldmg;\n", _SPACE),
    ("ai_subs.qc", "ai_meleesmash", "\n\tlocal float ldmg;\n", "\n" + _STAMP + "\tlocal float ldmg;\n", _SPACE),
    ("ai_subs.qc", "ai_checkmelee", "\tif (delta <= attackdist && ",
     "\t// AD support: melee arc: the reach grows with size (fight.qc VR_MeleeReach)\n"
     "\tif (delta <= VR_MeleeReach(attackdist) && ", Q + ": melee arc (fight.qc VR_MeleeReach): the reach grows with size"),
    # The melee lunges at a swing's start (the zombie knight's 20 units, the fiend's claw's 12): a melee step as the charges'
    # (the zombie knight's came to 0.1 units of the head with Melee Space on).
    ("mon_zombiek.qc", "zombiek_melee", "\twalkmove (self.ideal_yaw, 20);",
     _STAMP + "\tlocal vector before = self.origin;" + _STEP + "\n\twalkmove (self.ideal_yaw, 20);\n"
     "\tVR_MeleeSpace_Step(before);", _SPACE),
    ("mon_demon.qc", "Demon_Claw", "\twalkmove (self.ideal_yaw, 12);",
     "\tlocal vector before = self.origin;" + _STEP + "\n\twalkmove (self.ideal_yaw, 12);\n\tVR_MeleeSpace_Step(before);",
     _SPACE),
    # A forward step inside a swing (a melee step stamped a moment ago, the stamp kept on: the hunter ogre's chainsaw
    # swing goes on with ai_forward after its charge steps, and came to 9-13 units of the head); its other steps as AD's.
    ("ai_subs.qc", "ai_forward", "\twalkmove (self.angles_y, dist);",
     "\tlocal vector before = self.origin;" + _STEP + "\n\twalkmove (self.angles_y, dist);\n"
     "\tif (time - self.vr_melee_time < 0.25) { self.vr_melee_time = time; VR_MeleeSpace_Step(before); } "
     "// QVR: a step in a swing", _SPACE),
] + [
    (fn, func, "\tai_face ();", "\t" + _FACE % "ai_face ();", _ARC)
    for fn, func in (("mon_demon.qc", "Demon_Claw"), ("mon_drole.qc", "drole_claw"), ("mon_minotaur.qc", "mino_claw"),
                     ("mon_seeker.qc", "seek_punch"))
] + [
    # The AD guns' effects (vr_ad_enemyguns.qc): the monsters' electric bolts and the pyro's flames as the player's.
    ("ai_projectiles.qc", "Touch_PlasmaProjectile", "\t\n\t// Default damage for plasma",
     "\t// Quake VR: an electric bolt's hit crackles (vr_ad_enemyguns.qc, as the player's with the dropped gun)\n"
     "\tif (VR_ADGun_IsElectric(self)) VR_ADGun_BoltFx(self.origin, self.velocity, TRUE);\n\n"
     "\t// Default damage for plasma", Q + ": an electric bolt's hit crackles"),
    ("ai_projectiles.qc", "Touch_PlasmaProjectile",
     "\t\tif (self.dmg > 0) T_Damage (other, self, self.owner, self.dmg, DAMARMOR);\n",
     "\t\ttself = other; org = self.origin; dir = normalize(self.velocity); vec_x = other.health > 0;\n"
     "\t\tif (self.dmg > 0) T_Damage (other, self, self.owner, self.dmg, DAMARMOR);\n"
     "\t\t// Quake VR: the soldiers', eliminators' and centurions' electric bolts electrocute (as the player's: "
     "vr_ad_enemyguns.qc)\n"
     "\t\tif (VR_ADGun_IsElectric(self)) VR_ADGun_BoltShock(tself, vec_x, org, dir);\n",
     Q + ": an electric bolt electrocutes"),
    ("ai_projectiles.qc", "launch_plasma", "\t\tnewmis.frame = 0;\t\t\t\t\t\t// Start really small\n\t}\n",
     "\t\tnewmis.frame = 0;\t\t\t\t\t\t// Start really small\n\t}\n"
     "\t// Quake VR: an electric bolt's muzzle crackles (vr_ad_enemyguns.qc: the monsters' and the player's with their "
     "guns)\n\t// and it strikes bodies too (a missile's move: corpses are only touchable by missiles), to electrocute "
     "them\n\tif (VR_ADGun_IsElectric(newmis)) {\n\t\tVR_ADGun_BoltFx(org, dir, FALSE);\n"
     "\t\tnewmis.movetype = MOVETYPE_FLYMISSILE;\n\t}\n", Q + ": an electric bolt's muzzle crackles, it strikes bodies"),
    ("mon_pyro.qc", "pyroflame_touch", "\t\telse T_Damage (other, self, self, DAMAGE_MONFLAME, DAMARMOR);\n",
     "\t\telse {\n\t\t\tT_Damage (other, self, self, DAMAGE_MONFLAME, DAMARMOR);\n"
     "\t\t\t// Quake VR: poisoned for a while too, its green puffs (vr_ad_enemyguns.qc: the poison sprayer's, as the "
     "player's)\n\t\t\tif (other.flags & FL_MONSTER) VR_ADGun_Poison(other, self, self.owner, "
     "cvar(\"vr_adgun_fumigator_poison\"), cvar(\"vr_adgun_fumigator_poisontime\"));\n\t\t}\n",
     Q + ": the pyro's flames poison monsters too (the attacker credited: vr_dmgcredit.qc)"),
    ("mon_pyro.qc", "pyroflame_touch", "\t// Check owner is a Pyro?\n",
     "\t// Quake VR: its fire or poison draws no blood, paints no wound (combat.qc VR_Hit_Bloodless)\n"
     "\tVR_Hit_BeginAs(-1, TRUE);\n\t// Check owner is a Pyro?\n", Q + ": the pyro's and fumigator's flames draw no blood"),
    ("mon_pyro.qc", "pyroflame_touch", "\n\t\t}\n\t}", "\n\t\t}\n\t}\n\tVR_Hit_End();",
     Q + ": the pyro's and fumigator's flames draw no blood (its end)"),
    ("mon_pyro.qc", "pyroflame_spawn", "\tnewmis.touch = pyroflame_touch;",
     "\tnewmis.touch = pyroflame_touch;\n"
     "\t// Quake VR: see-through, our fire with it, its muzzle's fire and smoke (vr_ad_enemyguns.qc: as the player's)\n"
     "\tVR_ADGun_StreamFx(newmis, self.classtype != CT_MONPYRO);\n"
     "\tVR_ADGun_StreamMuzzle(org, dir, self.classtype != CT_MONPYRO, pointcontents(org) <= CONTENT_WATER);",
     Q + ": the pyro's flames look like the player's"),
    # The toolgun's live wall torches (vr_walltorch.qc, vr_spawn_live).
    ("lights.qc", "setup_light_sound", "\t\tif (self.targetname) {",
     "\t\tif (self.targetname || vr_spawn_live) { // QVR: made live (the toolgun): no ambient sound reaches the game",
     Q + ": a torch made live by the toolgun keeps its sound"),
    ("lights.qc", "setup_light_static", "\tif (self.mdl) setmodel (self, self.mdl);\n",
     "\tif (self.mdl) setmodel (self, self.mdl);\n\t// QVR: made live (the toolgun): a static entity would never reach "
     "the game (vr_walltorch.qc, vr_spawn_live)\n\tif (vr_spawn_live) return;\n",
     Q + ": a torch made live by the toolgun is not made static"),
]
# pyroflame_think ends "}" (no ";"), so its body is not one func_patch can find: a file patch, count-checked.
PATCHES += [
    ("mon_pyro.qc", "\t\tself.velocity = self.velocity - (self.movedir * 0.125);\n\t\tself.nextthink = time + 0.1;\n",
     "\t\tself.velocity = self.velocity - (self.movedir * 0.125);\n\t\tself.nextthink = time + 0.1;\n"
     "\t\tVR_ADGun_StreamFx(self, self.owner.classtype != CT_MONPYRO); // Quake VR: our fire with it "
     "(vr_ad_enemyguns.qc)\n", 1, Q + ": the pyro's flames look like the player's"),
    ("mon_pyro.qc", "\tself.cnt = self.cnt + 1;\n\tif (self.cnt > 6) remove(self);\n",
     "\t// Quake VR: into water, out at the surface in a puff of steam (vr_ad_enemyguns.qc: as the player's flames)\n"
     "\tif (VR_ADGun_FlameDoused(self, 0.1)) { remove(self); return; }\n"
     "\tself.cnt = self.cnt + 1;\n\tif (self.cnt > 6) remove(self);\n", 1,
     Q + ": the pyro's flames go out in water"),
    # AD's v_plasma.mdl is skipped in its folder (vr_ad_data.inc: an official pak's name), so this name was always our
    # Thunderbolt's plasma model, now v_light_plasma.mdl: the same model, and no missing file without Rogue. The
    # player's Plasma Gun is progs/v_plasma.mdl#vr (vr_ad_bridge.qc, vr_adweapons.cpp: AD's own file, read by its size).
    ("defscustom.qc", 'string MODEL_VWEAP_UPLG = "progs/v_plasma.mdl";',
     'string MODEL_VWEAP_UPLG = "progs/v_light_plasma.mdl";', 1,
     Q + ": AD's view model name, our Thunderbolt's plasma model (renamed; AD's own is skipped)"),
]
FUNC_PATCHES += [
    ("mon_pyro.qc", "pyroflame_touch", "\tif (other.solid == SOLID_TRIGGER) return;\n",
     "\tif (other.solid == SOLID_TRIGGER) return;\n"
     "\t// Quake VR: spent on a wall or the floor, a scorch where it struck, once (vr_ad_enemyguns.qc: as the player's)\n"
     "\tif (other == world || other.solid == SOLID_BSP) VR_ADGun_FlameScorch(self);\n",
     Q + ": the pyro's flames scorch where they strike"),
]
# Every projectile's hit through one path (weapons.qc VR_Proj_Begin/End: pushes, where it struck, a corpse's wear,
# pops) and the small ones marked to meet corpses, gibs, knocked-down bodies (VR_Proj_Mark).
_PROJ_BEGIN = "\tVR_Proj_Begin(%s, 0); // Quake VR: every projectile's hit (weapons.qc)\n"
_PROJ_MARK = ("\tnewmis.solid = SOLID_BBOX;\n",
              "\tnewmis.solid = SOLID_BBOX; VR_Proj_Mark(newmis); // Quake VR: it meets corpses, gibs, knocked-down "
              "bodies (weapons.qc)\n")
FUNC_PATCHES += [
    ("ai_projectiles.qc", "Touch_PlasmaProjectile", "// plasma touch has been disabled\n",
     "// plasma touch has been disabled\n" + _PROJ_BEGIN % "QVR_PROJ_LASER", Q + ": a plasma bolt's hit (VR_Proj_Begin)"),
    ("ai_projectiles.qc", "Touch_PlasmaProjectile", "// Remove plasma bolt\n\tentity_remove(self, 1);",
     "// Remove plasma bolt\n\tentity_remove(self, 1);\n\tVR_Proj_End();", Q + ": a plasma bolt's hit (VR_Proj_End)"),
    ("ai_projectiles.qc", "Touch_Projectile", "\n\n\t// No more touch/world interaction",
     "\n\t// Quake VR: every projectile's hit (weapons.qc VR_Proj_Begin: pushes, where it struck, a corpse's wear, pops)\n"
     "\tVR_Proj_Begin((self.classtype == CT_PROJ_LASER || self.classtype == CT_PROJ_RICLASER) ? QVR_PROJ_LASER : "
     "QVR_PROJ_BOLT, 0);\n\n\t// No more touch/world interaction", Q + ": a bolt's, laser's or lump's hit (VR_Proj_Begin)"),
    ("ai_projectiles.qc", "Touch_Projectile", "// Hide projectile and wait for any sounds to finish playing\n"
     "\tentity_remove(self,2);", "// Hide projectile and wait for any sounds to finish playing\n"
     "\tentity_remove(self,2);\n\tVR_Proj_End();", Q + ": a bolt's, laser's or lump's hit (VR_Proj_End)"),
    ("ai_projectiles.qc", "Touch_HomingMissile", "// trigger field, do nothing\n",
     "// trigger field, do nothing\n" + _PROJ_BEGIN % "QVR_PROJ_BLAST", Q + ": a homing missile's hit (VR_Proj_Begin)"),
    ("ai_projectiles.qc", "Touch_HomingMissile", "// Wait for any sounds to finish playing\n\tentity_remove(self, 4);",
     "// Wait for any sounds to finish playing\n\tentity_remove(self, 4);\n\tVR_Proj_End();",
     Q + ": a homing missile's hit (VR_Proj_End)"),
    ("ai_projectiles.qc", "Touch_Missile", "self.th_updmissile(); return; }\n",
     "self.th_updmissile(); return; }\n" + _PROJ_BEGIN % "QVR_PROJ_BLAST", Q + ": a missile's hit (VR_Proj_Begin)"),
    ("ai_projectiles.qc", "Touch_Missile", "// Hide + Delete rocket, no longer needed\n\tentity_remove(self, 1);",
     "// Hide + Delete rocket, no longer needed\n\tentity_remove(self, 1);\n\tVR_Proj_End();",
     Q + ": a missile's hit (VR_Proj_End)"),
    ("ai_projectiles.qc", "Touch_Grenade", "// Always keep track of what to damage\n",
     "// Always keep track of what to damage\n" + _PROJ_BEGIN % "QVR_PROJ_BLAST", Q + ": a grenade's hit (VR_Proj_Begin)"),
    ("ai_projectiles.qc", "Touch_Grenade", "self.avelocity = '0 0 0';\n\t\t}\n\t}",
     "self.avelocity = '0 0 0';\n\t\t}\n\t}\n\tVR_Proj_End();", Q + ": a grenade's hit (VR_Proj_End)"),
] + [
    ("ai_projectiles.qc", func, _PROJ_MARK[0], _PROJ_MARK[1], Q + ": its projectile meets bodies (VR_Proj_Mark)")
    for func in ("launch_plasma", "launch_projectile", "Launch_Missile", "Launch_Grenade")
]
# Melee arc (fight.qc VR_MeleeBlow): a swing hits only what is in front (side swings: either side). (file, the exact
# text of the line's test, the side argument): ai_checkmelee(...) there becomes (ai_checkmelee(...) &&
# VR_MeleeArc_Hits(side)) and the line gets the marker. Each must be on exactly one line of its file.
ARC_HITS = [
    ("ai_subs.qc", "if (!ai_checkmelee(MONAI_MELEEFRONT,FALSE))", "FALSE"),
    ("ai_subs.qc", "if (!ai_checkmelee(self.meleerange, MONAI_MELEEZTALL))", "FALSE"),
    ("ai_subs.qc", "if (!ai_checkmelee(MONAI_MELEESIDE,FALSE))", "FALSE"),
    ("mon_demon.qc", "if (!ai_checkmelee(MONAI_MELEEDEMON,FALSE) ||", "FALSE"),
    ("mon_dguardquoth.qc", "if (ai_checkmelee(MONAI_MELEEDGUARDQ,FALSE) && CanDamage", "FALSE"),
    ("mon_drole.qc", "if (!ai_checkmelee(MONAI_MELEEDROLE1,MONAI_MELEEZTALL)", "side != 0"),
    ("mon_freddie.qc", "if (!ai_checkmelee(self.meleerange, MONAI_MELEEZTALL))", "side != 0"),
    ("mon_minotaur.qc", "if (!ai_checkmelee(MONAI_MELEEMINOTAUR, MONAI_MELEEZTALL) ||", "side != 0"),
    ("mon_ogre.qc", "if (!ai_checkmelee(self.meleerange,FALSE))", "!vr_melee_ahead"),
    ("mon_seeker.qc", "if (!ai_checkmelee(MONAI_MELEESEEKER, MONAI_MELEEZTALL))", "FALSE"),
    ("mon_shambler.qc", "if (!ai_checkmelee(MONAI_MELEESHAM, MONAI_MELEEZTALL))", "side != 0"),
] + [
    ("mon_%s.qc" % fn, "if (!ai_checkmelee(MONAI_MELEE%s,FALSE))" % const, "FALSE")
    for fn, const in (("dog", "DOG"), ("fish", "FISH"), ("lostsoul", "LOSTSOUL"), ("scorpion", "SCORPION"),
                      ("spider", "SPIDER"), ("steelclaw", "STEELCLAW"), ("swampling", "SWAMPLING"),
                      ("voreling", "VORELING"), ("xmas_elf", "ELF"), ("xmas_raindeer", "RAINDEER"),
                      ("xmas_santa", "SANTA"), ("xmas_snowman", "SNOWMAN"))
]
ARC_LAUNCHES = 21  # ai_checkattack.qc's `self.th_melee ();` calls, each behind VR_MeleeArc_Launch (it turns first)
# The ogres' overhead smash comes down ahead, as id's ogre's (orig_mon_ogre.qc; ROUND22.md, "Melee arc: ... the ogre's
# smash", "1.1 open issues"): (file, frame function prefix, first and last hit frame). On those frames, with the arc on,
# ai_charge steps straight ahead (VR_MeleeArc_ChargeAhead), ai_face doesn't turn, and the chainsaw's blow is tested
# with the normal arc, not the swing's wide one (vr_melee_ahead around the call: mon_ogre.qc's ARC_HITS side).
SMASH_AHEAD = [("mon_ogre.qc", "ogre_smash", 6, 11), ("mon_hogre.qc", "hogre_smash", 6, 11)]
_SMASH = " // AD support: melee arc: the smash comes down ahead (fight.qc)"
QVR_NAMES |= {"VR_MeleeArc_On", "VR_MeleeArc_ChargeAhead", "vr_melee_ahead"}

# Ranged aim cone (vr_aimcone.qc; ROUND22.md, "Ranged aim cone"): AD's launchers bend a monster's shot toward the player
# at most its aim cone off its facing (VR_AimCone_Bend: its shooter's facing; the player's, traps' and monster-on-monster
# shots untouched; bosses none by ad_bossflag), and its main ranged attacks wait, turning, with the player far off its
# facing (AIM_WAITS: VR_AimCone_Wait at the start of the fire function or the shot's frame). Homing missiles
# (Launch_HomingMissile: the vore's, the sergeant's) are left alone.
QVR_NAMES |= {"VR_AimCone_Bend", "VR_AimCone_Wait", "VR_AIM_NARROW", "VR_AIM_WIDE"}
_AIM = Q + ": aim cone (vr_aimcone.qc): a monster's shot bent to its facing's aim cone"
_BEND = "\t%s = VR_AimCone_Bend(%s, VR_AIM_%s); // AD support: aim cone: bent to the shooter's facing (vr_aimcone.qc)\n"
FUNC_PATCHES += [
    ("ai_projectiles.qc", func, old, _BEND % (var, var, kind) + old, _AIM)
    for func, old, var, kind in (
        ("launch_plasma", "\tnewmis = spawn ();\n", "dir", "NARROW"),
        ("launch_projectile", "\tnewmis = spawn ();\n", "dir", "NARROW"),
        ("Launch_Missile", "\t// Check if there is space to spawn entity\n", "dir", "NARROW"),
        ("Launch_Grenade", "\t// Check if there is space to spawn entity\n", "dir", "WIDE"))  # (its dir: a velocity)
] + [
    ("ai_projectiles.qc", "Launch_Shells", "\t\tbullet_dir = normalize (bullet_dir - self.origin);\n",
     "\t\tbullet_dir = normalize (bullet_dir - self.origin);\n\t" + _BEND % ("bullet_dir", "bullet_dir", "NARROW"), _AIM),
    ("weapons.qc", "FireBullets", "\t\t\tdir = normalize (dir - src);\n\t\t}\n",
     "\t\t\tdir = normalize (dir - src);\n\t\t}\n\t" + _BEND % ("dir", "dir", "NARROW"), _AIM),
]
# A zombie's wake-up (getting off the floor: its fall's, or a map's dormant one's use) never on a dead one. AD's only
# death for a zombie gibs it; Quake VR's beheading or cut limb kills it whole, for good (VR_AD_ZombieDie: not gibbed), and
# a dormant one's trigger would raise the headless body, running (ROUND22.md, "A beheaded zombie knight ran again").
_WAKE = "\tif (self.gibbed) return;\t\t\t// Do nothing dead\n"
FUNC_PATCHES += [
    (f, func, _WAKE, "\tif (self.gibbed || self.deadflag) return;\t// Do nothing dead (QVR: or killed whole, beheaded)\n",
     B + ": a zombie killed whole (beheaded, a limb cut) never gets up")
    for f, func in (("mon_zombie.qc", "zombie_wakeup"), ("mon_zombiek.qc", "zombiek_wakeup"))
]
# AD's radius damage (every AD explosion: the Plasma Gun's, its rockets', grenades', mines', bosses'...) throws the
# props and the ragdolls round it as Quake VR's T_RadiusDamage does (physicsblast; the author's 2026-10-22 note: the
# Plasma Gun's blasts pushed no ragdolls; ROUND22.md, "Prop pushback, heavy throws, light ammo").
FUNC_PATCHES += [
    ("ai_combat.qc", "T_RadiusDamage", "\thead = findradius(imporg, damage+40);\n",
     "\tphysicsblast(imporg, VR_ADGun_BlastPush(inflictor, damage)); // QVR: the props and ragdolls round it thrown (Box3D; a plasma blast harder)\n"
     "\thead = findradius(imporg, damage+40);\n",
     B + ": AD's explosions throw props and ragdolls (physicsblast)"),
]
QVR_NAMES |= {"physicsblast", "VR_ADGun_BlastPush"}
# Edits made by hand in QC/ad after the 2026-10-16 cleanup, moved here (2026-11-19, Copper's import: --check said 17
# files differ). A leap's touch: dead of its own blow (a barrel's blast), its death's frames stand.
_DEAD = ("\tif (self.health < 1) return; // QVR: dead of its own blow (a barrel's blast): its death's frames stand "
         "(VR_Leap_BlowEnded)\n")
FUNC_PATCHES += [
    (f, func, "\t// Is the %s floating" % who, _DEAD + "\t// Is the %s floating" % who,
     Q + ": a leaping monster dead of its own blow stays dead (VR_Leap_BlowEnded)")
    for f, func, who in (("mon_bossjudicator.qc", "judge_JumpTouch", "Judge"), ("mon_demon.qc", "Demon_JumpTouch", "demon"),
                         ("mon_dfury.qc", "dfury_JumpTouch", "fury knight"), ("mon_dog.qc", "Dog_JumpTouch", "dog"),
                         ("mon_minotaur.qc", "mino_JumpTouch", "minotaur"), ("mon_spider.qc", "Spider_JumpTouch", "spider"),
                         ("mon_swampling.qc", "swampling_JumpTouch", "swampling"), ("mon_tarbaby.qc", "Tar_JumpTouch", "tarbaby"),
                         ("mon_voreling.qc", "Voreling_JumpTouch", "voreling"), ("mon_xmas_elf.qc", "Elf_JumpTouch", "elf"),
                         ("mon_zombiek.qc", "zombiek_JumpTouch", "zombie knight"))
] + [
    ("items.qc", "ammo_touch", "\tlocal float best;\n\n",
     "\tlocal float best;\n\n\tif (VR_Run_RefusesEnt(self)) return; // QVR: Melee Only, no rounds (vr_run_meleeonly.qc)\n",
     Q + ": Melee Only refuses AD's ammo (vr_run_meleeonly.qc)"),
    ("items.qc", "BackpackTouch", "\tlocal float pickupweapon, acount;\n\t\n",
     "\tlocal float pickupweapon, acount;\n\n\tif (VR_Run_RefusesEnt(self)) return; // QVR: Melee Only, its rounds left in it "
     "(vr_run_meleeonly.qc)\n", Q + ": Melee Only refuses AD's backpacks (vr_run_meleeonly.qc)"),
    ("mon_dcrossbow.qc", "dcrossbow_shot", "self.attack_speed = SPEED_DCROSS + (skill*SPEED_BOLTSKILL);",
     "self.attack_speed = VR_ADKnightBoltSpeed();", Q + ": the knights' bolts' speed (vr_adknight_bolt_scale)"),
    ("mon_dcrossbow.qc", "dcrossbow_checkattack", "boltspeed = SPEED_DCROSS + (skill*SPEED_BOLTSKILL);",
     "boltspeed = VR_ADKnightBoltSpeed();", Q + ": the knights' bolts' lead (vr_adknight_bolt_scale)"),
    ("mon_soldier.qc", "army_fire_rocket", "ATTN_NORM);\t\n",
     "ATTN_NORM);\n\tweaponfired(self, QVR_FIRED_ROCKET); // AD support: its tube's flash, smoke and backblast "
     "(vr_weaponfx.cpp, vr_gunfx.cpp)\n", B + ": the rocket soldier's tube flash, smoke and backblast (weaponfired)"),
    ("weapons.qc", "LightningDamage", "\tloffset = 8;\n",
     "\tloffset = 8;\n\n\tVR_LGWater_Zap(lstart, lfinish); // QVR: a bolt into water electrifies its surface (weapons.qc)\n"
     "\tVR_Lightning_Burn(lstart, lfinish); // QVR: and on a surface leaves a lasting burn\n",
     Q + ": AD's lightning zaps water and burns surfaces (weapons.qc)"),
    ("ai_explosion.qc", "spawn_touchblood", "\t// The vel calculation uses v_up/right",
     "\t// AD support: no flesh (a fuel tank, an explosive box, a crate, a grenade): sparks or splinters as the hitscan\n"
     "\t// pellets' and the nails' (vr_crates.qc VR_HitNonFlesh), not blood. AD's own breakables keep their debris colour.\n"
     "\tif (targ.classgroup != CG_BREAKABLE &&\n"
     "\t\tVR_HitNonFlesh(targ, source.origin, source.velocity, max(damage, 1))) return;\n"
     "\tvr_ad_blood_sent++;\n"
     "\tif (targ == vr_ad_blood_watch) vr_ad_blood_watched++;\n"
     "\tif (cvar_hget(cvarh_vr_debug_shots)) bprint(sprintf(\"adshots: blood on %s (%s)\\n\", targ.classname, "
     "source.classname));\n\n\t// The vel calculation uses v_up/right",
     B + ": no flesh, no blood (VR_HitNonFlesh)"),
]
PATCHES += [
    ("mon_dcrossbow.qc", "void() dcrossbow_crossbow_sound;\n",
     "void() dcrossbow_crossbow_sound;\n"
     "// QVR: the knights' bolts' speed (the launch and the tracking's lead): AD's skill formula (500 + 200 a skill) times\n"
     "// vr_adknight_bolt_scale (1.15: 15% faster, 2026-11-11).\n"
     "float() VR_ADKnightBoltSpeed =\n{\n"
     "\treturn (SPEED_DCROSS + (skill*SPEED_BOLTSKILL)) * cvar_hget(cvarh_vr_adknight_bolt_scale);\n};\n",
     1, Q + ": the knights' bolts' speed, AD's formula times vr_adknight_bolt_scale"),
    ("breakable.qc", "\"player/axhit1.wav\";\t\t\t// ax hit meat (another player)",
     "\"vr/derived/axhit1_voice.wav\";\t// ax hit meat (another player): axhit1 as the player's voice "
     "(QVR_SOUND_AXHIT_VOICE, vr_voice)", 1, Q + ": an axe hit on another player in the player's voice"),
]
QVR_NAMES |= {"VR_Run_RefusesEnt", "cvar_hget", "cvarh_vr_adknight_bolt_scale", "weaponfired", "QVR_FIRED_ROCKET",
              "VR_LGWater_Zap", "VR_Lightning_Burn", "VR_HitNonFlesh", "vr_ad_blood_sent", "vr_ad_blood_watch",
              "vr_ad_blood_watched", "cvarh_vr_debug_shots", "sprintf"}
OWN_NAMES = {"VR_ADKnightBoltSpeed"}  # Quake VR's names the patches define in AD's code: kept as written
# (file, function: a fire function with no arguments or the shot's frame, its kind): VR_AimCone_Wait at its start.
AIM_WAITS = [
    ("mon_soldier.qc", "army_fire", "NARROW"), ("mon_soldier.qc", "army_fire_rocket", "NARROW"),
    ("mon_soldier.qc", "army_plasma_fire", "NARROW"), ("mon_soldier.qc", "army_grenade_atk5", "WIDE"),
    ("mon_enforcer.qc", "enforcer_fire", "NARROW"), ("mon_enforcer.qc", "defender_ssg", "NARROW"),
    ("mon_enforcer.qc", "eliminator_fire", "NARROW"), ("mon_enforcer.qc", "def_atkgl6", "WIDE"),
    ("mon_centurion.qc", "cent_fire", "NARROW"), ("mon_ogre.qc", "OgreFireNails", "NARROW"),
    ("mon_ogre.qc", "ogre_shoot4", "WIDE"), ("mon_hknight.qc", "hknight_magicc6", "WIDE"),
    ("mon_hknight.qc", "hknight_magica7", "WIDE"), ("mon_hknight.qc", "hknight_magicb7", "WIDE"),
    ("mon_dknight.qc", "dknight_magicc6", "WIDE"), ("mon_gargoyle.qc", "garg_fbthrow", "NARROW"),
    ("mon_drole.qc", "drole_fire", "NARROW"), ("mon_dguardquoth.qc", "dguardq_fireball", "NARROW"),
    ("mon_golem.qc", "golem_magic13", "WIDE"), ("mon_wizard.qc", "Wiz_FastFire", "WIDE"),
    ("mon_shambler.qc", "sham_lightning", "NARROW"), ("mon_dcrossbow.qc", "dcrossbow_shot", "NARROW"),
    ("mon_eel.qc", "eel_plasma", "NARROW"), ("mon_jim.qc", "jim_laser", "NARROW"), ("mon_jim.qc", "jim_rocket", "NARROW"),
    ("mon_gaunt.qc", "gaunt_plasma", "NARROW"), ("mon_zombie.qc", "zombie_atta12", "WIDE"),
    ("mon_zombie.qc", "zombie_attb13", "WIDE"), ("mon_zombie.qc", "zombie_attc11", "WIDE"),
    ("mon_spider.qc", "spider_spit8", "WIDE"), ("mon_swampling.qc", "swampling_spit4", "WIDE"),
    ("mon_voreling.qc", "voreling_spit4", "WIDE"), ("mon_minotaur.qc", "mino_magic8", "NARROW"),
    ("mon_skullwiz.qc", "skullw_attack6", "WIDE"), ("mon_scorpion.qc", "pinch_tail7", "WIDE"),
    ("mon_dlord.qc", "dlord_magica7", "NARROW"), ("mon_dlord.qc", "dlord_magicb9", "NARROW"),
]


def aim_wait_patch(text, fn, func, kind):
    """VR_AimCone_Wait(func, kind) first in func's body (a function's `= {` or a frame's `= [...] {`). Exactly once."""
    hits = list(re.finditer(r"(?m)^[^\n]*\b" + re.escape(func) + r"\s*=\s*(?:\[[^\]\n]*\]\s*)?\{", text))
    if len(hits) != 1:
        sys.exit("ad_import: aim cone wait for %s %s matches %d functions, not 1" % (fn, func, len(hits)))
    at = hits[0].end()
    return (text[:at] + "\n\tif (VR_AimCone_Wait(%s, VR_AIM_%s)) return; // AD support: aim cone: far off its facing it "
            "waits, turning (vr_aimcone.qc)\n" % (func, kind) + text[at:])


def line_patch(text, fn, needle, edit, mark, count=1):
    """Every line of text holding needle: edit(line), right-stripped, plus mark. Exactly count lines."""
    lines = text.split("\n")
    hits = [i for i, l in enumerate(lines) if needle in l]
    if len(hits) != count:
        sys.exit("ad_import: %s: %r is on %d lines, not %d" % (fn, needle, len(hits), count))
    for i in hits:
        lines[i] = edit(lines[i]).rstrip() + mark
    return "\n".join(lines)


def func_patch(text, fn, func, old, new, why):
    m = re.search(r"(?m)^[^\n]*\b" + re.escape(func) + r"\s*=\s*\{", text)  # ("name\t= {" too)
    end = text.find("\n};", m.end()) if m else -1
    if not m or end < 0:
        sys.exit("ad_import: function %s not found in %s (%s)" % (func, fn, why))
    body = text[m.end():end]
    if body.count(old) != 1:
        sys.exit("ad_import: patch for %s %s (%s) matches %d times, not 1: %r" % (fn, func, why, body.count(old), old))
    return text[:m.end()] + body.replace(old, new) + text[end:]
# (Mechanical, not patches: a global array's length named by a constant, `float a[MAX];`, is written as the number,
# as our fteqcc flags (autoproto) take no named constant there.)

ENGINE_DATA = os.path.join(ROOT, "Quake", "vr", "vr_ad_data.inc")


def engine_lists(rel, official):
    """Writes Quake/vr/vr_ad_data.inc (vr_ad.cpp): the release's pin the engine checks (pak2.pak's size and the SHA-256
    of its header and directory: cheap at every start, and only patch 1 has it) and the files of AD's paks that an
    official pak (id1, hipnotic, rogue: their names only, read from `official`) also has. The engine skips those in AD's
    folder, so AD supplies only its own files (ARCANE_DIMENSIONS.md 5.5); maps/start.bsp excepted (AD's hub)."""
    with open(os.path.join(rel, "pak2.pak"), "rb") as f:
        hdr = f.read(12)
        off, ln = struct.unpack("<ii", hdr[4:])
        f.seek(off)
        directory = f.read(ln)
        f.seek(0, 2)
        pak2_size = f.tell()
    dirsha = hashlib.sha256(hdr + directory).hexdigest()
    ad = set()
    for p in ("pak0.pak", "pak1.pak", "pak2.pak"):
        ad |= {n.lower() for n in read_pak(os.path.join(rel, p))}
    owner = {}
    for game, tag in (("id1", 0), ("hipnotic", 1), ("rogue", 2)):
        folder = os.path.join(official, game)
        paks = sorted(fn for fn in os.listdir(folder) if re.match(r"pak\d+\.pak$", fn, re.I)) if os.path.isdir(folder) else []
        if game == "id1" and not paks:
            sys.exit("ad_import: no id1 paks in %s (--official)" % official)
        for fn in paks:
            for n in read_pak(os.path.join(folder, fn)):
                owner.setdefault(n.lower(), tag)
    shadowed = sorted(n for n in ad if n in owner and n != "maps/start.bsp")
    lines = ["// Generated by Misc/quakevr/ad_import.py --engine-lists from Arcane Dimensions 1.80 patch 1 and the official",
             "// paks' file names (no content); never edit. vr_ad.cpp: the release pin and AD's files an official pak has too",
             "// (skipped in AD's folder; the second argument is the official owner: 0 id1, 1 hipnotic, 2 rogue).",
             "#define VR_AD_PAK2_SIZE %d" % pak2_size,
             "#define VR_AD_PAK2_DIRSHA \"%s\"" % dirsha]
    lines += ["VR_AD_SHADOWED(\"%s\", %d)" % (n, owner[n]) for n in shadowed]
    with open(ENGINE_DATA, "w", encoding="latin1", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("ad_import: %s: %d shadowed files (id1 %d, hipnotic %d, rogue %d); pak2 %d bytes" % (
        os.path.relpath(ENGINE_DATA, ROOT), len(shadowed), sum(owner[n] == 0 for n in shadowed),
        sum(owner[n] == 1 for n in shadowed), sum(owner[n] == 2 for n in shadowed), pak2_size))


def map_keys(release):
    """Every key AD's FGD documents or AD's maps set."""
    keys = set()
    with open(os.path.join(release, "ad_1_8.fgd"), encoding="latin1") as f:
        for line in f:
            m = re.match(r"\s*([A-Za-z_]\w*)\s*\(", line)
            if m:
                keys.add(m.group(1))
    for pak in ("pak1.pak", "pak2.pak"):
        path = os.path.join(release, pak)
        if not os.path.isfile(path):
            continue
        with open(path, "rb") as f:
            for name, (o, s) in read_pak(path).items():
                if not name.endswith(".bsp"):
                    continue
                f.seek(o)
                head = f.read(12)
                eo, el = struct.unpack("<ii", head[4:12])
                f.seek(o + eo)
                lump = f.read(el).decode("latin1")
                keys.update(re.findall(r'"([A-Za-z_]\w*)"\s*"', lump))
    return keys


def verify_frames(rel):
    """Every AD frame function's frame number (its OP_STATE operand) in our compile equals AD's own progs.dat's
    (patch 1's, in pak2.pak). Reads the asm listing the precedence check writes (build/qcprec/qc/qc.asm): run the
    build first. fteqcc numbers $frame macros per progs.src entry, so a file folded into another would shift them."""
    d = pak_files(os.path.join(rel, "pak2.pak"), lambda n: n == "progs.dat")["progs.dat"]
    h = struct.unpack("<15i", d[:60])
    ofs_st, nst, ofs_fn, nfn, ofs_str, ofs_gl, ngl = h[2], h[3], h[8], h[9], h[10], h[12], h[13]
    gl = struct.unpack("<%df" % ngl, d[ofs_gl:ofs_gl + ngl * 4])
    starts = []
    for i in range(nfn):
        first, _, _, _, s_name = struct.unpack("<5i", d[ofs_fn + i * 36:ofs_fn + i * 36 + 20])
        if first > 0:
            e = d.index(b"\0", ofs_str + s_name)
            starts.append((first, d[ofs_str + s_name:e].decode("latin1")))
    starts.sort()
    theirs = {}
    for k, (first, name) in enumerate(starts):
        end = starts[k + 1][0] if k + 1 < len(starts) else nst
        for j in range(first, end):
            op, a = struct.unpack("<HH", d[ofs_st + j * 8:ofs_st + j * 8 + 4])
            if op == 60:  # OP_STATE
                theirs[name] = gl[a]
                break
    ours, cur = {}, None
    with open(os.path.join(ROOT, "build", "qcprec", "qc", "qc.asm"), errors="replace") as f:
        for line in f:
            if line.endswith(" = asm\n"):
                nm = line[:-7].split(")")[-1].strip().split()[-1]
                cur = nm[len(PREFIX):] if nm.startswith(PREFIX) else None
            elif cur and line.startswith("\tSTATE"):
                m = re.match(r"\tSTATE\s+(-?[\d.]+)f", line)
                if m:
                    ours[cur] = float(m.group(1))
                cur = None
            elif line.startswith("}"):
                cur = None
    bad = sorted(n for n in theirs if n in ours and abs(theirs[n] - ours[n]) > 1e-6)
    missing = sorted(n for n in theirs if n not in ours)
    print("ad_import --verify-frames: %d frame functions, %d compared, %d different, %d missing%s" % (
        len(theirs), len(theirs) - len(missing), len(bad), len(missing),
        (": " + ", ".join(bad[:10] + missing[:10])) if bad or missing else ""))
    return 1 if bad or missing else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--release", default=os.path.join(ROOT, "scratch", "ad", "ad"))
    ap.add_argument("--verify-frames", action="store_true",
                    help="after a build: compare AD's frame numbers with AD's own progs.dat; writes nothing")
    ap.add_argument("--engine-lists", action="store_true",
                    help="write Quake/vr/vr_ad_data.inc (the engine's release pin and shadowed-file list) and stop")
    ap.add_argument("--official", default="C:/OHWorkspace/qvr-kit/qbase",
                    help="a Quake folder with id1 (and hipnotic, rogue) paks, for --engine-lists")
    ap.add_argument("--check", metavar="DIR", nargs="?", const=os.path.join(ROOT, "scratch", "ad_check"),
                    help="write to DIR (default scratch/ad_check) instead of QC/ad, then diff against QC/ad and "
                         "progs.src; exit 1 on any difference (run before committing a QC/ad change)")
    args = ap.parse_args()
    rel = args.release
    for fn, want in PINNED.items():
        got = sha256(os.path.join(rel, fn))
        if got != want:
            sys.exit("ad_import: %s is not AD 1.80 patch 1's (sha256 %s)" % (fn, got))
    if args.verify_frames:
        sys.exit(verify_frames(rel))
    if args.engine_lists:
        engine_lists(rel, args.official)
        return

    src = pak_files(os.path.join(rel, "pak0.pak"), lambda n: n.startswith("my_progs/"))
    src.update(pak_files(os.path.join(rel, "pak2.pak"), lambda n: n.startswith("my_progs/")))  # patch 1 wins
    src = {k[len("my_progs/"):]: v.decode("latin1").replace("\r\n", "\n") for k, v in src.items()}
    for fn, old, new, count, why in PATCHES:
        if src[fn].count(old) != count:
            sys.exit("ad_import: patch for %s (%s) matches %d times, not %d: %r" % (fn, why, src[fn].count(old), count, old))
        src[fn] = src[fn].replace(old, new)
    for fn, func, old, new, why in FUNC_PATCHES:
        src[fn] = func_patch(src[fn], fn, func, old, new, why)
    for fn, needle, side in ARC_HITS:
        call = re.search(r"ai_checkmelee\([^()]*\)", needle)[0]
        src[fn] = line_patch(src[fn], fn, needle, lambda l: l.replace(
            needle, needle.replace(call, "VR_MeleeBlow(%s, %s, FALSE)" % (call, side))),
            " // AD support: reach and melee arc (fight.qc VR_MeleeBlow)")
    for fn, prefix, first, last in SMASH_AHEAD:
        for n in range(first, last + 1):
            src[fn] = line_patch(src[fn], fn, "void() %s%d" % (prefix, n), lambda l: re.sub(
                r"\bchainsaw\(([^()]*)\);", r"vr_melee_ahead = TRUE; chainsaw(\1); vr_melee_ahead = FALSE;",
                re.sub(r"\bai_charge\((\d+)\);", r"if (VR_MeleeArc_On()) VR_MeleeArc_ChargeAhead(\1); else ai_charge(\1);",
                       l.replace("ai_face();", "if (!VR_MeleeArc_On()) ai_face();"))), _SMASH)
    for fn, func, kind in AIM_WAITS:
        src[fn] = aim_wait_patch(src[fn], fn, func, kind)
    src["ai_checkattack.qc"] = line_patch(
        src["ai_checkattack.qc"], "ai_checkattack.qc", "self.th_melee ();",
        lambda l: l.replace("self.th_melee ();", "if (VR_MeleeArc_Launch()) self.th_melee ();"),
        " // AD support: melee arc: facing away, it turns first (ad_ai_run_melee; fight.qc)", ARC_LAUNCHES)
    resist = src["ai_ammoresist.qc"]
    marker = "float(entity targ, float resist_type, float damage) Resist_Damage =\n{"
    if resist.count(marker) != 1:
        sys.exit("ad_import: resistance scope hook missing")
    resist = resist.replace(marker, marker + '\n\tif (!VR_AD_FeatureScope(cvar("vr_ad_resistance"))) return damage;')
    tell = r"(void\([^\n]*\) Resist_(?:Axe|Shells|Nails|Rockets|Lightning|Plasma)\s*=\s*\{)"
    resist, count = re.subn(tell, lambda m: m[1] + '\n\tif (!VR_AD_FeatureScope(cvar("vr_ad_resistance"))) return;', resist)
    if count != 6:
        sys.exit("ad_import: resistance scope tell hooks do not match six functions")
    src["ai_ammoresist.qc"] = resist
    # AD support: flight impacts use VR positional/selective damage; completed ordnance setup joins VR interaction.
    projectile = src["ai_projectiles.qc"]
    pattern = r"(?ms)(^void\(\) Touch_Bullet =\s*\{)(.*?)(^\};)"
    def bullet_hook(m):
        body = m[2]
        old = "if (self.dmg > 0) T_Damage (other, self, self.owner, self.dmg, DAMARMOR);"
        if body.count(old) != 1:
            sys.exit("ad_import: bullet VR damage hook missing")
        body = body.replace(old, "if (self.dmg > 0) VR_AD_PelletHit(other, self, self.dmg);")
        body = body.replace("entity_remove(self, 1);", "VR_AD_PelletPrepare(self);\n\tentity_remove(self, 1);", 1)
        return m[1] + body + m[3]
    projectile, count = re.subn(pattern, bullet_hook, projectile)
    if count != 1:
        sys.exit("ad_import: bullet VR impact hook does not match exactly once")
    for name in ("Launch_Missile", "Launch_Grenade"):
        pattern = r"(?ms)(^void\([^\n]*\) " + name + r" =\s*\{)(.*?)(^\};)"
        projectile, count = re.subn(pattern, lambda m: m[1] + m[2] + "\tVR_AD_Ordnance(newmis);\n" + m[3], projectile)
        if count != 1:
            sys.exit("ad_import: VR ordnance hook missing: " + name)
    pattern = r"(?ms)(^void\([^\n]*\) Launch_Bullet =\s*\{)(.*?)(^\};)"
    projectile, count = re.subn(pattern, lambda m: m[1] + "\n\tproj_speed = VR_AD_PelletSpeed(self, proj_speed);" + m[2] +
                               "\tVR_AD_PelletLaunch(newmis);\n" + m[3], projectile)
    if count != 1:
        sys.exit("ad_import: monster pellet speed hook missing")
    src["ai_projectiles.qc"] = projectile
    # Mechanical: the beam id byte after every beam's entity (qc_import.beam_id_bytes, every import's rule).
    beam_ids = qc_import.beam_id_bytes(src)
    mod = qc_import.Mod(
        name="Arcane Dimensions", short="AD", script="ad_import.py", folder="ad", prefix=PREFIX, skip=SKIP,
        headers=HEADERS, keep_prefixes=(BRIDGE,), qvr_names=QVR_NAMES, own_names=OWN_NAMES, keys=map_keys(rel),
        key_fill="in an AD map the engine fills them from the key, pr_edict.c",
        source_name=lambda fn: "my_progs/" + fn,
        banner=("// Arcane Dimensions 1.80 patch 1 (Simon \"sock\" O'Callaghan and team; GPL-2.0-or-later, see\n"
                "// AD's readme and docs/vr-port/CREDITS.md), %s, generated by Misc/quakevr/ad_import.py: every name\n"
                "// AD defines is prefixed \"ad_\" (ARCANE_DIMENSIONS.md 5.2). Do not edit; change the importer.\n\n"),
        # AD as shipped: `if (string)` tests null (F322; the same in AD's own compile) and locals fteqcc can't prove
        # set on every path (F302, reported after the last file is read). Quiet from here on: the imports are the
        # last files in progs.src.
        builtins_text=("#pragma warning disable F302\n#pragma warning disable F322\n\n"
                       "// Not declared by Quake VR's QC: bound by name (#0) when the engine has them.\n\n"),
        report_name="AD_IMPORT.txt",
        report_head=["Arcane Dimensions import (Misc/quakevr/ad_import.py); regenerate, never edit.",
                     "Source: ad_v1_80p1final.zip, " + ", ".join("%s %s" % kv for kv in sorted(PINNED.items()))])
    if args.check:
        mod.write_dir = qc_import.check_dir(mod, args.check)
    report, _ = qc_import.run(mod, src)
    report += [
        "Beam temp entities given Quake VR's beam id byte after WriteEntity (mechanical): %d" % beam_ids,
        "Reviewed patches: %d precedence, %d bridge hooks (VR_AD_*, QC/vr_ad_bridge.qc):" % (
            sum(1 for p in PATCHES if p[4].startswith(P)), sum(1 for p in PATCHES if p[4].startswith(B))),
    ] + ["  %s: %s" % (p[0], p[4]) for p in PATCHES if p[4].startswith(B) or p[4].startswith(Q)] + [
        "  %s %s: %s" % (p[0], p[1], p[4]) for p in FUNC_PATCHES] + [
        "  %s: %s: reach and melee arc (VR_MeleeBlow(.., %s, FALSE))" % (fn, needle, side) for fn, needle, side in ARC_HITS] + [
        "  ai_checkattack.qc: %d melee launches behind VR_MeleeArc_Launch" % ARC_LAUNCHES] + [
        "  %s %s: aim cone wait (VR_AimCone_Wait, %s)" % (fn, func, kind) for fn, func, kind in AIM_WAITS]
    report.append("  ai_projectiles.qc: unique Touch_Bullet prepare/hit, Launch_Bullet enemy speed/flight origin, "
                  "Launch_Missile/Launch_Grenade VR ordnance hooks (function-boundary checked)")
    qc_import.write_report(mod, report)
    print("\n".join(report[:12]))



if __name__ == "__main__":
    main()
