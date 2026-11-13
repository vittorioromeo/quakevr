//
//
//
// ----------------------------------------------------------------------------
// The monster kinds: every monster class classified, in one table (docs/vr-port/MUTATORS.md, "What a new weapon, monster
// or item needs")
// ----------------------------------------------------------------------------
//
// What a kind is to whatever treats monsters by kind: the run's enemy mutators (Randomized Enemies, vr_run_enemyrandom.qc:
// a kind of the same tier and movement; Enemy Count, vr_run_enemycount.qc: copies of it) and anything else that needs
// its tier, how it moves, its size or its height. A kind row: how it moves, its hull, its threat tier, whose it is, its
// box's top, and the file whose presence means its data is here (an expansion's). A never row (tier 0): a monster class
// that is never swapped or copied (a boss, a scripted or placed-for-its-trick one, a minion, one not tiered yet), listed
// so that every monster spawn function is one or the other: vr_run_enemies_check fails on a class in neither
// (VR_MonsterKinds_Unclassified, by the progs' function names).

float RUN_ER_WALK = 1;
float RUN_ER_FLY  = 2;
float RUN_ER_SWIM = 3;

float RUN_ER_ID   = 0; // Quake's: always there
float RUN_ER_AD   = 1; // Arcane Dimensions' (vr_ad_mounted)
float RUN_ER_PACK = 2; // an unofficial pack's (Honey's): copied (Enemy Count), never swapped either way
float RUN_ER_EXP  = 3; // an official expansion's (Scourge of Armagon, Dissolution of Eternity, Dimension of the Machine):
                       // swapped either way while its data is here (its probe file)

// A row: classname, how it moves (RUN_ER_WALK, _FLY, _SWIM), its hull (1 Quake's 32-wide, 2 the 64-wide: a large one),
// its threat tier (1 light, 2 medium, 3 heavy; 0 never swapped or copied), whose it is, its box's top over its origin
// (its feet 24 under it; Quake VR's kinds their spawn functions' setsize, AD's ad_monster_bbox), and the file its data
// needs ("" none: its game's always there).
typedef struct
{
    string cls;
    float move, size, tier, src, top;
    string probe;
} vr_monster_kind;

vr_monster_kind VR_MONSTER_KINDS[] =
{
    // Quake's
    { "monster_army", 1, 1, 1, 0, 27, "" },
    { "monster_dog", 1, 1, 1, 0, 16, "" }, // (its box 24 wide: orig_mon_dog.qc; id's was 64)
    { "monster_zombie", 1, 1, 1, 0, 24, "" },
    { "monster_knight", 1, 1, 1, 0, 26, "" },
    { "monster_enforcer", 1, 1, 1, 0, 28, "" },
    { "monster_wizard", 2, 1, 1, 0, 42, "" },
    { "monster_fish", 3, 1, 1, 0, 24, "" },
    { "monster_tarbaby", 1, 1, 2, 0, 24, "" },
    { "monster_hell_knight", 1, 1, 2, 0, 36, "" },
    { "monster_ogre", 1, 2, 2, 0, 40, "" },
    { "monster_demon1", 1, 2, 2, 0, 24, "" },
    { "monster_shalrath", 1, 2, 3, 0, 64, "" },
    { "monster_shambler", 1, 2, 3, 0, 64, "" },
    // Arcane Dimensions'
    { "monster_zombiek", 1, 1, 1, 1, 32, "" },
    { "monster_dcrossbow", 1, 1, 1, 1, 32, "" },
    { "monster_dguard", 1, 1, 1, 1, 32, "" },
    { "monster_spider", 1, 1, 1, 1, 16, "" },
    { "monster_voreling", 1, 1, 1, 1, 16, "" },
    { "monster_swampling", 1, 1, 1, 1, 16, "" },
    { "monster_scorpion", 1, 1, 1, 1, 16, "" },
    { "monster_steelclaw", 1, 1, 1, 1, 16, "" },
    { "monster_army_grenade", 1, 1, 1, 1, 32, "" },
    { "monster_defender", 1, 1, 1, 1, 40, "" },
    { "monster_lostsoul", 2, 1, 1, 1, 16, "" },
    { "monster_jim", 2, 1, 1, 1, 32, "" },
    { "monster_army_rocket", 1, 1, 2, 1, 32, "" },
    { "monster_army_plasma", 1, 1, 2, 1, 32, "" },
    { "monster_eliminator", 1, 1, 2, 1, 40, "" },
    { "monster_skullwiz", 1, 1, 2, 1, 40, "" },
    { "monster_pyro", 1, 1, 2, 1, 40, "" },
    { "monster_dknight", 1, 1, 2, 1, 40, "" },
    { "monster_deflector", 1, 1, 2, 1, 40, "" },
    { "monster_hogre", 1, 2, 2, 1, 40, "" },
    { "monster_hogremac", 1, 2, 2, 1, 40, "" },
    { "monster_gargoyle", 2, 1, 2, 1, 40, "" },
    { "monster_gaunt", 2, 1, 2, 1, 40, "" },
    { "monster_centurion", 2, 1, 2, 1, 40, "" },
    { "monster_hogreham", 1, 2, 3, 1, 40, "" },
    { "monster_dfury", 1, 1, 3, 1, 40, "" },
    { "monster_dsergeant", 1, 1, 3, 1, 40, "" },
    { "monster_dprince", 1, 1, 3, 1, 40, "" },
    { "monster_minotaur", 1, 2, 3, 1, 64, "" },
    { "monster_drole", 1, 2, 3, 1, 64, "" },
    { "monster_wraith", 2, 2, 3, 1, 40, "" },
    // the official expansions' (swapped while their data is here: the probe, as their spawn functions test it):
    // Scourge of Armagon's,
    { "monster_gremlin", 1, 1, 1, 3, 18, "progs/grem.mdl" },
    { "monster_scourge", 1, 2, 2, 3, 24, "progs/scor.mdl" },
    // Honey's (an unofficial pack's: copied, never swapped),
    { "monster_sknight", 1, 1, 1, 2, 0, "" },
    { "monster_ogre_marksman", 1, 2, 2, 2, 0, "" },
    // Dissolution of Eternity's,
    { "monster_mummy", 1, 1, 2, 3, 40, "progs/mummy.mdl" },
    { "monster_wrath", 2, 1, 2, 3, 40, "progs/wrath.mdl" },
    { "monster_eel", 3, 1, 1, 3, 40, "progs/eel2.mdl" },
    // Dimension of the Machine's (its owned data)
    { "monster_demodog", 1, 1, 1, 3, 16, "owned/mg3/progs/dog_explosive.mdl" },
    { "monster_ranged_knight", 1, 1, 2, 3, 36, "owned/mg3/progs/rknight.mdl" },
    { "monster_ogre_rocket", 1, 2, 2, 3, 40, "owned/mg3/progs/ogre_rocket.mdl" },
    { "monster_orb", 2, 2, 2, 3, 64, "owned/mg3/progs/teleporter_eye_blink.mdl" },

    // Never swapped or copied (tier 0). Bosses:
    { "monster_boss", 0, 0, 0, 0, 0, "" }, { "monster_oldone", 0, 0, 0, 0, 0, "" },
    { "monster_armagon", 0, 0, 0, 3, 0, "" }, { "monster_dragon", 0, 0, 0, 3, 0, "" },
    { "monster_lava_man", 0, 0, 0, 3, 0, "" }, { "monster_morph", 0, 0, 0, 3, 0, "" },
    { "monster_super_wrath", 0, 0, 0, 3, 0, "" }, { "monster_boss_final", 0, 0, 0, 3, 0, "" },
    { "monster_lava_man_mg3", 0, 0, 0, 3, 0, "" }, { "monster_oldone_new", 0, 0, 0, 3, 0, "" },
    { "monster_super_shambler", 0, 0, 0, 3, 0, "" },
    // ... scripted, or placed for a trick of their own: Dimension of the Machine's Shub's eyes and seekers, its lying
    // shub zombies, its infected (a scene's), its splitting slime, its players' ghosts (not monsters); Scourge of
    // Armagon's decoy and spike mine; Dissolution's invisible swordsman; Quake VR's own maps' (vr_*):
    { "monster_shub_eye", 0, 0, 0, 3, 0, "" }, { "monster_shub_seeker", 0, 0, 0, 3, 0, "" },
    { "monster_szombie", 0, 0, 0, 3, 0, "" }, { "monster_slime", 0, 0, 0, 3, 0, "" },
    { "monster_ghost", 0, 0, 0, 3, 0, "" },
    { "monster_army_infected", 0, 0, 0, 3, 0, "" }, { "monster_enforcer_infected", 0, 0, 0, 3, 0, "" },
    { "monster_hell_knight_infected", 0, 0, 0, 3, 0, "" }, { "monster_knight_infected", 0, 0, 0, 3, 0, "" },
    { "monster_decoy", 0, 0, 0, 3, 0, "" }, { "monster_spikemine", 0, 0, 0, 3, 0, "" },
    { "monster_sword", 0, 0, 0, 3, 0, "" },
    { "monster_vr_grunt", 0, 0, 0, 0, 0, "" }, { "monster_vr_enforcer", 0, 0, 0, 0, 0, "" },
    { "monster_vr_knight", 0, 0, 0, 0, 0, "" }, { "monster_vr_hknight", 0, 0, 0, 0, 0, "" },
    // ... Arcane Dimensions' bosses, minions and set pieces, and its kinds not tiered yet (a row above to take part):
    { "monster_baron", 0, 0, 0, 1, 0, "" }, { "monster_bob", 0, 0, 0, 1, 0, "" },
    { "monster_boglord", 0, 0, 0, 1, 0, "" }, { "monster_boil", 0, 0, 0, 1, 0, "" },
    { "monster_chthon", 0, 0, 0, 1, 0, "" }, { "monster_death_guard", 0, 0, 0, 1, 0, "" },
    { "monster_death_knight", 0, 0, 0, 1, 0, "" }, { "monster_death_lord", 0, 0, 0, 1, 0, "" },
    { "monster_dknightghost", 0, 0, 0, 1, 0, "" }, { "monster_edie", 0, 0, 0, 1, 0, "" },
    { "monster_eidolon", 0, 0, 0, 1, 0, "" }, { "monster_fade", 0, 0, 0, 1, 0, "" },
    { "monster_firetopboss", 0, 0, 0, 1, 0, "" }, { "monster_floyd", 0, 0, 0, 1, 0, "" },
    { "monster_freddie", 0, 0, 0, 1, 0, "" }, { "monster_fumigator", 0, 0, 0, 1, 0, "" },
    { "monster_golem", 0, 0, 0, 1, 0, "" }, { "monster_gug", 0, 0, 0, 1, 0, "" },
    { "monster_hell_pointknight", 0, 0, 0, 1, 0, "" }, { "monster_icegolem", 0, 0, 0, 1, 0, "" },
    { "monster_judgeminion", 0, 0, 0, 1, 0, "" }, { "monster_judicator", 0, 0, 0, 1, 0, "" },
    { "monster_juggernaut", 0, 0, 0, 1, 0, "" }, { "monster_justice", 0, 0, 0, 1, 0, "" },
    { "monster_mammoth", 0, 0, 0, 1, 0, "" }, { "monster_minotaurminion", 0, 0, 0, 1, 0, "" },
    { "monster_nour", 0, 0, 0, 1, 0, "" }, { "monster_nourminion", 0, 0, 0, 1, 0, "" },
    { "monster_ogre_fishing", 0, 0, 0, 1, 0, "" }, { "monster_ogre_fishing_ghost", 0, 0, 0, 1, 0, "" },
    { "monster_ogre_flak", 0, 0, 0, 1, 0, "" }, { "monster_polyp", 0, 0, 0, 1, 0, "" },
    { "monster_seeker", 0, 0, 0, 1, 0, "" }, { "monster_sentinel", 0, 0, 0, 1, 0, "" },
    { "monster_shalrathminion", 0, 0, 0, 1, 0, "" }, { "monster_shub", 0, 0, 0, 1, 0, "" },
    { "monster_shubupsd", 0, 0, 0, 1, 0, "" }, { "monster_skullwizminion", 0, 0, 0, 1, 0, "" },
    { "monster_sweeper", 0, 0, 0, 1, 0, "" }, { "monster_turretbal", 0, 0, 0, 1, 0, "" },
    { "monster_vermis", 0, 0, 0, 1, 0, "" }, { "monster_wraithminion", 0, 0, 0, 1, 0, "" },
    { "monster_wraithminion2", 0, 0, 0, 1, 0, "" }, { "monster_xmas_elf", 0, 0, 0, 1, 0, "" },
    { "monster_xmas_raindeer", 0, 0, 0, 1, 0, "" }, { "monster_xmas_santa", 0, 0, 0, 1, 0, "" },
    { "monster_xmas_snowman", 0, 0, 0, 1, 0, "" }
};
#define VR_MONSTER_KINDS_N ((float)(sizeof(VR_MONSTER_KINDS) / sizeof(VR_MONSTER_KINDS[0]))) // (a new row counts itself)
#define VR_MONSTER_KINDS_MAX 160 // (a per-row array's size: past the table's)

// A classname's kind row (tier 1 to 3), or -1 (not a kind: never swapped or copied, or not listed).
float(string xCls) VR_MonsterKind_Row =
{
    for(float i = 0; i < VR_MONSTER_KINDS_N; i++)
    {
        if(VR_MONSTER_KINDS[i].cls == xCls) { return VR_MONSTER_KINDS[i].tier > 0 ? i : -1; }
    }
    return -1;
};

// Kind row `xRow`'s data is here: Quake's and AD's always (AD's own test is the mutator's: VR_RunEnemyRandom_ADHere);
// an expansion's while its probe file is (its spawn function tests more: one that still refuses leaves nothing).
float(float xRow) VR_MonsterKind_DataHere =
{
    return VR_Has(VR_MONSTER_KINDS[xRow].probe);
};

// Functions named monster_* (and AD's ad_monster_*) that are not a monster's spawn function: the monsters' helpers.
float(string xName) VR_MonsterKinds_Helper =
{
    float n = strlen(xName);
    if(n > 9 && substring(xName, n - 9, 9) == "_precache") return TRUE;
    return xName == "monster_SPAWNFLAG_REMOVE_CORPSE" || xName == "monster_add_to_counter" || xName == "monster_use" ||
           xName == "monster_check_remove_corpse" || xName == "monster_death_use" || xName == "monster_ogre_real" ||
           xName == "monster_ThrowGib" || xName == "monster_bbox" || xName == "monster_check_gib" ||
           xName == "monster_deadbody_check" || xName == "monster_death_postcheck" || xName == "monster_death_precheck" ||
           xName == "monster_idle_sound" || xName == "monster_liquid_check" || xName == "monster_sightsound" ||
           xName == "monster_spawn" || xName == "monster_spawndelay" || xName == "monster_start" ||
           xName == "monster_targets" || xName == "monster_eidolon_wavetrig" || xName == "monster_icegolem_wavetrig" ||
           xName == "monster_gibrandomizer" || xName == "monster_base_enforcer";
};

// The monster spawn functions (monster_*, AD's ad_monster_*: its classname without "ad_") in no row of the table, each
// printed: their count (0: every monster classified).
float() VR_MonsterKinds_Unclassified =
{
    float n = 0;
    for(float f = 1; f < 1000000; f++)
    {
        string name = functionname(f);
        if(name == "") break;
        if(substring(name, 0, 3) == "ad_") name = substring(name, 3, -1);
        if(substring(name, 0, 8) != "monster_" || VR_MonsterKinds_Helper(name)) continue;
        float found = FALSE;
        for(float i = 0; i < VR_MONSTER_KINDS_N && !found; i++) { found = VR_MONSTER_KINDS[i].cls == name; }
        if(!found)
        {
            n++;
            dprint(sprintf("monster kinds: %s is not classified: a kind row of vr_monster_kinds.qc, or a never row\n",
                           name));
        }
    }
    return n;
};

// The enemies check's (the engine's vr_run_enemies_check): TRUE when every monster spawn function is classified.
float() VR_MonsterKinds_AllClassified = { return !VR_MonsterKinds_Unclassified(); };
