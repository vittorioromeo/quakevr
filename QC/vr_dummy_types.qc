// vr_dummy_types.qc -- the training dummy's enemies (vr_dummy_type; vr_dummy.qc): which monster it stands for, and that
// monster's own spawn function making it. After every monster's file (progs.src): it calls their spawn functions and
// keeps the Scourge of Armagon's gremlin count.
//
// The numbers are vr_dummy_type's, saved in configs and recorded in the motion takes (docs/vr-port/MOTIONS.md): never
// renumber them; a new enemy takes the next number. Its frames (the stand loop, the pain animation, an attack for
// vr_dummy_attacks) are the monster's own, from its file's $frame list; its pain sounds the ones its pain function plays.

// Loads `xType`'s row into the vr_dt_ globals (vr_dummy.qc); anything unknown is a grunt (0).
void(float xType) VR_Dummy_Type =
{
    vr_dt_pack = 0;
    vr_dt_snd1 = "";
    vr_dt_snd2 = "";
    if(xType == 1)
    {
        vr_dt_class = "monster_enforcer"; vr_dt_name = "Enforcer";
        vr_dt_stand = 0; vr_dt_stands = 7; vr_dt_pain = 66; vr_dt_pains = 4; vr_dt_atk = 31; vr_dt_atks = 10;
        vr_dt_snd1 = "enforcer/pain1.wav"; vr_dt_snd2 = "enforcer/pain2.wav";
        return;
    }
    if(xType == 2)
    {
        vr_dt_class = "monster_knight"; vr_dt_name = "Knight";
        vr_dt_stand = 0; vr_dt_stands = 9; vr_dt_pain = 28; vr_dt_pains = 3; vr_dt_atk = 42; vr_dt_atks = 10;
        vr_dt_snd1 = "knight/khurt.wav";
        return;
    }
    if(xType == 3)
    {
        vr_dt_class = "monster_hell_knight"; vr_dt_name = "Death Knight";
        vr_dt_stand = 0; vr_dt_stands = 9; vr_dt_pain = 37; vr_dt_pains = 5; vr_dt_atk = 112; vr_dt_atks = 10;
        vr_dt_snd1 = "hknight/pain1.wav";
        return;
    }
    if(xType == 4)
    {
        vr_dt_class = "monster_ogre"; vr_dt_name = "Ogre";
        vr_dt_stand = 0; vr_dt_stands = 9; vr_dt_pain = 67; vr_dt_pains = 5; vr_dt_atk = 33; vr_dt_atks = 14;
        vr_dt_snd1 = "ogre/ogpain1.wav";
        return;
    }
    if(xType == 5)
    {
        vr_dt_class = "monster_demon1"; vr_dt_name = "Fiend";
        vr_dt_stand = 0; vr_dt_stands = 13; vr_dt_pain = 39; vr_dt_pains = 6; vr_dt_atk = 54; vr_dt_atks = 15;
        vr_dt_snd1 = "demon/dpain1.wav";
        return;
    }
    if(xType == 6)
    {
        vr_dt_class = "monster_shambler"; vr_dt_name = "Shambler";
        vr_dt_stand = 0; vr_dt_stands = 17; vr_dt_pain = 77; vr_dt_pains = 6; vr_dt_atk = 35; vr_dt_atks = 12;
        vr_dt_snd1 = "shambler/shurt2.wav";
        return;
    }
    if(xType == 7)
    {
        vr_dt_class = "monster_zombie"; vr_dt_name = "Zombie";
        vr_dt_stand = 0; vr_dt_stands = 15; vr_dt_pain = 91; vr_dt_pains = 12; vr_dt_atk = 52; vr_dt_atks = 13;
        vr_dt_snd1 = "zombie/z_pain.wav"; vr_dt_snd2 = "zombie/z_pain1.wav";
        return;
    }
    if(xType == 8)
    {
        vr_dt_class = "monster_shalrath"; vr_dt_name = "Vore";
        vr_dt_stand = 23; vr_dt_stands = 1; vr_dt_pain = 11; vr_dt_pains = 5; vr_dt_atk = 0; vr_dt_atks = 11; // (it stands on walk1)
        vr_dt_snd1 = "shalrath/pain.wav";
        return;
    }
    if(xType == 9)
    {
        vr_dt_class = "monster_wizard"; vr_dt_name = "Scrag";
        vr_dt_stand = 0; vr_dt_stands = 15; vr_dt_pain = 42; vr_dt_pains = 4; vr_dt_atk = 29; vr_dt_atks = 13;
        vr_dt_snd1 = "wizard/wpain.wav";
        return;
    }
    if(xType == 10)
    {
        vr_dt_class = "monster_dog"; vr_dt_name = "Rottweiler";
        vr_dt_stand = 69; vr_dt_stands = 9; vr_dt_pain = 26; vr_dt_pains = 6; vr_dt_atk = 0; vr_dt_atks = 8;
        vr_dt_snd1 = "dog/dpain1.wav";
        return;
    }
    if(xType == 11)
    {
        vr_dt_class = "monster_tarbaby"; vr_dt_name = "Spawn";
        vr_dt_stand = 0; vr_dt_stands = 1; vr_dt_pain = 0; vr_dt_pains = 0; vr_dt_atk = 50; vr_dt_atks = 6; // (no pain)
        return;
    }
    if(xType == 12)
    {
        vr_dt_class = "monster_fish"; vr_dt_name = "Rotfish";
        vr_dt_stand = 39; vr_dt_stands = 18; vr_dt_pain = 57; vr_dt_pains = 9; vr_dt_atk = 0; vr_dt_atks = 18;
        return;
    }
    if(xType == 13)
    {
        vr_dt_class = "monster_gremlin"; vr_dt_name = "Gremlin"; vr_dt_pack = 1;
        vr_dt_stand = 0; vr_dt_stands = 17; vr_dt_pain = 100; vr_dt_pains = 4; vr_dt_atk = 71; vr_dt_atks = 13;
        vr_dt_snd1 = "grem/pain1.wav"; vr_dt_snd2 = "grem/pain2.wav";
        return;
    }
    if(xType == 14)
    {
        vr_dt_class = "monster_scourge"; vr_dt_name = "Centroid"; vr_dt_pack = 1;
        vr_dt_stand = 0; vr_dt_stands = 12; vr_dt_pain = 31; vr_dt_pains = 5; vr_dt_atk = 20; vr_dt_atks = 11;
        vr_dt_snd1 = "scourge/pain.wav"; vr_dt_snd2 = "scourge/pain2.wav";
        return;
    }
    if(xType == 15)
    {
        vr_dt_class = "monster_mummy"; vr_dt_name = "Mummy"; vr_dt_pack = 2;
        vr_dt_stand = 0; vr_dt_stands = 15; vr_dt_pain = 91; vr_dt_pains = 12; vr_dt_atk = 52; vr_dt_atks = 13;
        vr_dt_snd1 = "zombie/z_pain.wav"; vr_dt_snd2 = "zombie/z_pain1.wav";
        return;
    }
    if(xType == 16)
    {
        vr_dt_class = "monster_wrath"; vr_dt_name = "Wrath"; vr_dt_pack = 2;
        vr_dt_stand = 1; vr_dt_stands = 12; vr_dt_pain = 55; vr_dt_pains = 6; vr_dt_atk = 13; vr_dt_atks = 14;
        vr_dt_snd1 = "wrath/wpain.wav";
        return;
    }
    if(xType == 17)
    {
        vr_dt_class = "monster_super_wrath"; vr_dt_name = "Overlord"; vr_dt_pack = 2;
        vr_dt_stand = 1; vr_dt_stands = 15; vr_dt_pain = 66; vr_dt_pains = 14; vr_dt_atk = 16; vr_dt_atks = 10;
        vr_dt_snd1 = "wrath/wpain.wav";
        return;
    }
    if(xType == 18)
    {
        vr_dt_class = "monster_eel"; vr_dt_name = "Electric Eel"; vr_dt_pack = 2;
        vr_dt_stand = 0; vr_dt_stands = 6; vr_dt_pain = 0; vr_dt_pains = 0; vr_dt_atk = 0; vr_dt_atks = 6; // (no pain)
        vr_dt_snd1 = "eel/epain3.wav";
        return;
    }
    // Dawn of the Machine's own (vr_dt_pack 3: its data, read in place): from 19.
    if(xType == 19)
    {
        vr_dt_class = "monster_ogre_rocket"; vr_dt_name = "Rocket Ogre"; vr_dt_pack = 3; // (vr_mg3_ogre.qc: the ogre's frames)
        vr_dt_stand = 0; vr_dt_stands = 9; vr_dt_pain = 67; vr_dt_pains = 5; vr_dt_atk = 33; vr_dt_atks = 14;
        vr_dt_snd1 = "owned/mg3/armagon/pain.wav";
        return;
    }
    if(xType == 20)
    {
        vr_dt_class = "monster_demodog"; vr_dt_name = "Demo Dog"; vr_dt_pack = 3; // (vr_mg3_demodog.qc: the rottweiler's frames)
        vr_dt_stand = 69; vr_dt_stands = 9; vr_dt_pain = 26; vr_dt_pains = 6; vr_dt_atk = 0; vr_dt_atks = 8;
        vr_dt_snd1 = "dog/dpain1.wav";
        return;
    }
    if(xType == 21)
    {
        vr_dt_class = "monster_ranged_knight"; vr_dt_name = "Ranged Knight"; vr_dt_pack = 3; // (vr_mg3_rknight.qc: the death knight's frames)
        vr_dt_stand = 0; vr_dt_stands = 9; vr_dt_pain = 37; vr_dt_pains = 5; vr_dt_atk = 155; vr_dt_atks = 11; // (magicc)
        vr_dt_snd1 = "owned/mg3/rknight/pain_01.wav"; vr_dt_snd2 = "owned/mg3/rknight/pain_02.wav";
        return;
    }
    // Dawn of the Machine's, from 30 (19..29 left to its other monsters), read in place from the owned pack.
    if(xType == 30)
    {
        vr_dt_class = "monster_orb"; vr_dt_name = "Orb"; vr_dt_pack = 3;
        vr_dt_stand = 0; vr_dt_stands = 1; vr_dt_pain = 2; vr_dt_pains = 1; vr_dt_atk = 1; vr_dt_atks = 2; // (three frames: hover, two blinks)
        vr_dt_snd1 = "owned/mg3/orb/orb_pain.wav";
        return;
    }
    if(xType == 31)
    {
        vr_dt_class = "monster_tarbaby"; vr_dt_name = "Slime"; // (the spawn's model: no MG3 data needed; killed, it splits)
        vr_dt_stand = 0; vr_dt_stands = 1; vr_dt_pain = 0; vr_dt_pains = 0; vr_dt_atk = 50; vr_dt_atks = 6; // (no pain)
        return;
    }
    vr_dt_class = "monster_army"; vr_dt_name = "Grunt";
    vr_dt_stand = 0; vr_dt_stands = 8; vr_dt_pain = 40; vr_dt_pains = 6; vr_dt_atk = 81; vr_dt_atks = 9;
    vr_dt_snd1 = "soldier/pain1.wav"; vr_dt_snd2 = "soldier/pain2.wav";
};

// `xType`'s spawn function on self (VR_Dummy_Make: its classname set, its spawnflags cleared).
void(float xType) VR_Dummy_TypeSpawn =
{
    if(xType == 1) { monster_enforcer(); return; }
    if(xType == 2) { monster_knight(); return; }
    if(xType == 3) { monster_hell_knight(); return; }
    if(xType == 4) { monster_ogre(); return; }
    if(xType == 5) { monster_demon1(); return; }
    if(xType == 6) { monster_shambler(); return; }
    if(xType == 7) { monster_zombie(); return; }
    if(xType == 8) { monster_shalrath(); return; }
    if(xType == 9) { monster_wizard(); return; }
    if(xType == 10) { monster_dog(); return; }
    if(xType == 11) { monster_tarbaby(); return; }
    if(xType == 12) { monster_fish(); return; }
    if(xType == 13) { monster_gremlin(); return; }
    if(xType == 14) { monster_scourge(); return; }
    if(xType == 15) { monster_mummy(); return; }
    if(xType == 16) { monster_wrath(); return; }
    if(xType == 17) { monster_super_wrath(); return; }
    if(xType == 18) { monster_eel(); return; }
    if(xType == 19) { monster_ogre_rocket(); return; }
    if(xType == 20) { monster_demodog(); return; }
    if(xType == 21) { monster_ranged_knight(); return; }
    if(xType == 30) { monster_orb(); return; }
    if(xType == 31) { monster_slime(); return; }
    monster_army();
};

// `xType`'s spawn function on self, as the map's would run it but counted nowhere: not among the level's monsters, nor
// the gremlins (it never splits), in any game (deathmatch removes monsters as they spawn). FALSE: it removed self (a
// mission pack's monster whose files are not here: VR_Pack_RequireSpawn).
float(float xType) VR_Dummy_Make =
{
    VR_Dummy_Type(xType);
    self.classname = vr_dt_class;
    self.spawnflags = 0;
    float total = total_monsters;
    float gremlins = NumGremlins;
    float dm = deathmatch;
    deathmatch = 0;
    VR_Dummy_TypeSpawn(xType);
    deathmatch = dm;
    total_monsters = total;
    NumGremlins = gremlins;
    self.spawnflags = 0; // (an ogre made a boss at random)
    // Nothing of its AI: it never starts (its think is the dummy's), and nothing wakes it (monster_use, alerts, a parry's
    // interrupt: none has a th_run to call).
    self.th_stand = world.th_stand;
    self.th_walk = world.th_walk;
    self.th_run = world.th_run;
    self.th_missile = world.th_missile;
    self.th_melee = world.th_melee;
    self.th_vrshove = world.th_vrshove;
    self.use = world.use;
    return self.model != ""; // (a removed entity's model is cleared)
};

nosave float vr_dummy_tried;     // types tried on a stand-in this map (bits): whether they can be made here,
nosave float vr_dummy_available; // and which can (types 0..23; 24 on: the second pair, bit xType - 24: a float's exact bits)
nosave float vr_dummy_tried2, vr_dummy_available2;

// Whether `xType` can be made here (its pack's files present): tried once a map, on a stand-in (VR_Dummy_Make).
float(float xType) VR_Dummy_Available =
{
    if(xType < 1 || xType >= VR_DUMMY_TYPES)
    {
        return xType == 0;
    }
    float high = xType >= 24;
    float bit = pow(2, high ? xType - 24 : xType);
    if(!((high ? vr_dummy_tried2 : vr_dummy_tried) & bit))
    {
        if(high) { vr_dummy_tried2 = vr_dummy_tried2 | bit; } else { vr_dummy_tried = vr_dummy_tried | bit; }
        entity oldSelf = self;
        self = spawn();
        entity probe = self;
        if(VR_Dummy_Make(xType))
        {
            if(high) { vr_dummy_available2 = vr_dummy_available2 | bit; } else { vr_dummy_available = vr_dummy_available | bit; }
            remove(probe);
        }
        self = oldSelf;
    }
    return ((high ? vr_dummy_available2 : vr_dummy_available) & bit) != 0;
};
