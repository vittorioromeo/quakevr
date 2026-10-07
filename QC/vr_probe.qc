// vr_probe.qc -- the kinds of monster that can appear on a map after it has loaded, made ready as it loads: no hitch at
// a kind's first appearance (its head, missiles and sounds precached late, its compiled hull built, its limb models
// made: 17-30 ms frames on the firing range).
//
// The engine calls VR_Probe_Kinds as the map's entities have spawned (VR_OnSpawnServerSpawned, still loading: precaches
// are allowed). It makes one monster of each kind that can appear later, on a new entity and counted nowhere (the
// training dummy's VR_Dummy_Make): its spawn function precaches its models and sounds. The engine then makes the limb
// models and compiled hulls of every monster there (the probes with the map's own) and removes every entity made here
// (vr_progs.cpp probesEnd), before the map's first frame: nothing of them is ever seen, saved or run. Their models and
// sounds stay precached (in every client's list, from the start).
//
// The kinds that can appear: the firing range's monster dispensers' (func_enemy_dispenser), every type a training
// dummy can stand as (vr_dummy: the menu's vr_dummy_type changes it at any time), monsters waiting for a trigger to
// appear (Honey's trigger-spawned monsters, Machine Games' deferred ones: their size and limbs are unknown until then),
// and, with vr_probe_test_spawn, the debug spawner's (vr_test_spawn, impulse 241). A kind a monster of the map already is
// needs none.
//
// A kind: 0..18 the dummy's types (VR_Dummy_Type's numbers), then the dispensers' kinds that are not dummy types:
// 19 phantom swordsman, 20 Guardian (Dissolution of Eternity's monster_morph), 21 dragon, 22 marksman ogre.

float VR_PROBE_KINDS = 23;

nosave float vr_probe_ready;   // the kinds made ready this load (bits)
nosave float vr_probe_counted; // the level's monsters as the probes began (dprint: the same after them)

// The kind a dispenser's `choice` makes (func_enemy_dispenser's "weapon"; -1: none).
float(float choice) VR_Probe_DispenserKind =
{
    if(choice == 0) { return 0; }   // grunt
    if(choice == 1) { return 4; }   // ogre
    if(choice == 2) { return 7; }   // zombie
    if(choice == 3) { return 6; }   // shambler
    if(choice == 4) { return 9; }   // scrag
    if(choice == 5) { return 2; }   // knight
    if(choice == 6) { return 3; }   // death knight
    if(choice == 7) { return 10; }  // rottweiler
    if(choice == 8) { return 1; }   // enforcer
    if(choice == 9) { return 5; }   // fiend
    if(choice == 10) { return 8; }  // vore
    if(choice == 11) { return 11; } // spawn
    if(choice == 12) { return 13; } // gremlin
    if(choice == 13) { return 14; } // centroid
    if(choice == 14) { return 15; } // mummy
    if(choice == 15) { return 19; } // phantom swordsman
    if(choice == 16) { return 16; } // wrath
    if(choice == 17) { return 17; } // overlord
    if(choice == 18) { return 20; } // Guardian
    if(choice == 19) { return 21; } // dragon
    if(choice == 20) { return 22; } // marksman ogre
    return -1;
};

// The kind a monster of classname `c` is (-1: none of these).
float(string c) VR_Probe_ClassKind =
{
    if(c == "monster_army") { return 0; }
    if(c == "monster_enforcer") { return 1; }
    if(c == "monster_knight") { return 2; }
    if(c == "monster_hell_knight") { return 3; }
    if(c == "monster_ogre") { return 4; }
    if(c == "monster_demon1") { return 5; }
    if(c == "monster_shambler") { return 6; }
    if(c == "monster_zombie") { return 7; }
    if(c == "monster_shalrath") { return 8; }
    if(c == "monster_wizard") { return 9; }
    if(c == "monster_dog") { return 10; }
    if(c == "monster_tarbaby") { return 11; }
    if(c == "monster_fish") { return 12; }
    if(c == "monster_gremlin") { return 13; }
    if(c == "monster_scourge") { return 14; }
    if(c == "monster_mummy") { return 15; }
    if(c == "monster_wrath") { return 16; }
    if(c == "monster_super_wrath") { return 17; }
    if(c == "monster_eel") { return 18; }
    if(c == "monster_sword") { return 19; }
    if(c == "monster_morph") { return 20; }
    if(c == "monster_dragon") { return 21; }
    if(c == "monster_ogre_marksman") { return 22; }
    return -1;
};

// One monster of `kind`, made at `org` on a new entity, counted nowhere (the engine removes it after the load's
// limbs and hulls).
void(float kind, vector org) VR_Probe_Make =
{
    if(kind < 0 || kind >= VR_PROBE_KINDS)
    {
        return;
    }
    float bit = pow(2, kind);
    if(vr_probe_ready & bit)
    {
        return;
    }
    vr_probe_ready = vr_probe_ready | bit;

    entity oldSelf = self;
    self = spawn();
    setorigin(self, org);
    if(kind < VR_DUMMY_TYPES)
    {
        // (and the dummy knows whether it can stand as it: VR_Dummy_Available tries no more)
        vr_dummy_tried = vr_dummy_tried | bit;
        if(VR_Dummy_Make(kind))
        {
            vr_dummy_available = vr_dummy_available | bit;
        }
        self = oldSelf;
        return;
    }

    // The dispensers' kinds that are not dummy types, as a dispenser makes them (its model there: not installed, none).
    float choice = 15;
    if(kind == 20) { choice = 18; }
    else if(kind == 21) { choice = 19; }
    else if(kind == 22) { choice = 20; }
    string mdl = VR_EnemyDispenser_Model(choice);
    if(mdl != NULLSTR && !fileexists(mdl))
    {
        remove(self);
        self = oldSelf;
        return;
    }
    float total = total_monsters;
    float gremlins = NumGremlins;
    float dm = deathmatch;
    deathmatch = 0;
    VR_EnemyDispenser_Spawn(choice);
    deathmatch = dm;
    total_monsters = total;
    NumGremlins = gremlins;
    self = oldSelf;
};

// The engine (VR_OnSpawnServerSpawned): every kind that can appear later made once (see the top), in two passes:
// `pass` 1 the monsters waiting for a trigger (the map's own: their limbs are made with the map's monsters'), 2 the
// kinds the dispensers, dummies and the debug spawner make (vr_limbs_prebuild 2: their limbs too).
void(float pass) VR_Probe_Kinds =
{
    entity e;
    float kind;
    if(pass == 1)
    {
        vr_probe_ready = 0;
        vr_probe_counted = total_monsters;
    }
    if(deathmatch || coop)
    {
        return; // (no dispensers, dummies' types or test spawns in a multiplayer game; its monsters spawn as the map's)
    }

    if(pass == 1)
    {
        // The kinds the map's monsters already are: made ready with them.
        for(e = nextent(world); e; e = nextent(e))
        {
            if(e.flags & FL_MONSTER)
            {
                kind = VR_Probe_ClassKind(e.classname);
                if(kind >= 0)
                {
                    vr_probe_ready = vr_probe_ready | pow(2, kind);
                }
            }
        }
    }

    float dummies = FALSE;
    entity last = world; // (the map's entities only: the probes come after them)
    for(e = nextent(world); e; e = nextent(e))
    {
        last = e;
    }
    for(e = nextent(world); e; e = nextent(e))
    {
        if(pass == 2 && e.classname == "func_enemy_dispenser")
        {
            VR_Probe_Make(VR_Probe_DispenserKind(e.weapon), e.origin);
        }
        else if(pass == 2 && VR_IsDummy(e))
        {
            dummies = TRUE;
        }
        else if(pass == 1 && !(e.flags & FL_MONSTER) && substring(e.classname, 0, 8) == "monster_")
        {
            VR_Probe_Make(VR_Probe_ClassKind(e.classname), e.origin); // (waiting for a trigger)
        }
        if(e == last)
        {
            break;
        }
    }
    if(pass == 1)
    {
        return;
    }

    if(dummies)
    {
        for(kind = 0; kind < VR_DUMMY_TYPES; kind++)
        {
            VR_Probe_Make(kind, '0 0 0');
        }
    }

    float test = cvar("vr_probe_test_spawn");
    if(test >= 2)
    {
        for(kind = 0; kind < VR_PROBE_KINDS; kind++)
        {
            VR_Probe_Make(kind, '0 0 0');
        }
    }
    else if(test >= 1)
    {
        VR_Probe_Make(VR_Probe_DispenserKind(cvar("vr_test_spawn")), '0 0 0');
    }
    dprint(sprintf("probes: kinds ready %g (bits), monsters counted %g before, %g after\n", vr_probe_ready,
        vr_probe_counted, total_monsters));
};
