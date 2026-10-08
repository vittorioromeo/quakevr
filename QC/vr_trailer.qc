// vr_trailer.qc -- the trailer's opening scene: maps/vrtrailer (Misc/quakevr/maps/vrtrailer_gen.py; MAPPING.md,
// "vrtrailer"). A night lake, a bridge, the Super Axe on a pedestal half way, a grunt on the far islet looking out
// over the water. The author walks across, takes the axe, creeps up behind the grunt and beheads him with a two-handed
// swing; takes are recorded again and again, so:
//
//   - the grunt is oblivious until hurt (the map's "vr_oblivious" "1", vr_fields.qc): nothing wakes him, the stealth
//     AI's senses included; a beheading cut on him always kills (VR_Trailer_Take: whatever the swing's damage, the head
//     comes off: the blade's speed and angle are the decapitation's own rules, vr_decap_speed and _slash_angle);
//   - the map's player starts empty-handed (VR_Trailer_StripParms: the Super Axe the only weapon in the scene);
//   - recording mode (vr_recording_clean_map 1, set by info_vr_trailer; spawnflag 1 leaves it off): no map tips and
//     no wrist messages (vr_tips.cpp, vr_gadget.cpp; vr_recording_clean is the player's own switch, Graphics >
//     Recording);
//   - vr_trailer_reset 1 (Debug > Tests > Trailer Scene > Reset Trailer Scene): the grunt again where he stood (the
//     old one, his head, his gibs, his gun and his backpack taken away), the Super Axe again on its pedestal (taken
//     out of the player's hands and holsters), the player at the start facing along the bridge, full health;
//   - vr_trailer_log 1: the grunt's state four times a second (his enemy, frame, think, stealth state, oblivious), the
//     axe's place, the player's (the headless test: Misc/quakevr/vrtrailer_test.sh).

entity vr_trailer;              // the scene (info_vr_trailer), world: not this map
entity vr_trailer_grunt;        // the oblivious monster as last spawned
entity vr_trailer_axe;          // the Super Axe's spawner (the map's AS_PLACED func_weapon_grabbable)
vector vr_trailer_grunt_org;    // where the grunt stood once on the floor, and his turn
vector vr_trailer_grunt_ang;
string vr_trailer_grunt_class;
float vr_trailer_ready;         // the scene's things found (VR_Trailer_Setup)
float vr_trailer_next_log;
float vr_trailer_resets;        // resets done (the tests)

float VR_TRAILER_NO_RECORDING = 1; // info_vr_trailer's spawnflag: recording mode off
float VR_TRAILER_CLEAN_RADIUS = 600; // the grunt's leftovers within this of where he stood are taken away by a reset

// T_Damage (combat.qc): the damage `take` an oblivious monster takes from a player's blow armed to behead it
// (VR_Decap_Arm: the blade swung across the neck fast enough) is at least its health: the cut kills, so it beheads.
float(entity targ, entity attacker, float take) VR_Trailer_Take =
{
    if(!targ.vr_oblivious || !(attacker.flags & FL_CLIENT) || vr_decap_ent != targ || vr_decap_time != time ||
       vr_decap_limb >= 0 || (vr_decap_how != QVR_DECAP_SWING && vr_decap_how != QVR_DECAP_SAW) || take >= targ.health)
    {
        return take;
    }
    dprint(sprintf("trailer: an oblivious %s's beheading cut: %g damage made %g\n", targ.classname, take, ceil(targ.health)));
    return ceil(targ.health);
};

// DecodeLevelParms (client.qc), before the parms are read: in the scene's map the player has no weapons (hands and
// holsters empty, none owned).
void() VR_Trailer_StripParms =
{
    if(mapname != "vrtrailer")
    {
        return;
    }
    parm1 = parm1 - (parm1 & (IT_AXE | IT_SHOTGUN | IT_SUPER_SHOTGUN | IT_NAILGUN | IT_SUPER_NAILGUN |
                              IT_GRENADE_LAUNCHER | IT_ROCKET_LAUNCHER | IT_LIGHTNING));
    parm8 = WID_FIST; parm23 = 0; parm31 = 0; parm41 = 0;
    parm10 = WID_FIST; parm24 = 0; parm32 = 0; parm42 = 0;
    parm11 = WID_FIST; parm25 = 0; parm33 = 0; parm43 = 0;
    parm12 = WID_FIST; parm26 = 0; parm34 = 0; parm44 = 0;
    parm13 = WID_FIST; parm27 = 0; parm35 = 0; parm45 = 0;
    parm14 = WID_FIST; parm28 = 0; parm36 = 0; parm46 = 0;
    parm15 = WID_FIST; parm29 = 0; parm37 = 0; parm47 = 0;
    parm16 = WID_FIST; parm30 = 0; parm38 = 0; parm48 = 0;
    parm17 = 0; parm18 = 0;
};

/*QUAKED info_vr_trailer (0 .8 .8) (-16 -16 -24) (16 16 32) NO_RECORDING
The trailer scene (Quake VR; maps/vrtrailer, QC vr_trailer.qc): its origin and angle are where vr_trailer_reset puts
the player. It finds the map's oblivious monster ("vr_oblivious" "1") and its AS_PLACED func_weapon_grabbable, and
puts them back as they were for each retake. Recording mode on (no map tips, no wrist messages) unless NO_RECORDING.
*/
void() info_vr_trailer =
{
    vr_trailer = self;
    vr_trailer_ready = FALSE;
    cvar_set("vr_recording_clean_map", (self.spawnflags & VR_TRAILER_NO_RECORDING) ? "0" : "1");
};

void() VR_Trailer_Setup =
{
    vr_trailer_ready = TRUE;
    vr_trailer_grunt = world;
    vr_trailer_axe = world;
    entity e = nextent(world);
    while(e != world)
    {
        if(vr_trailer_grunt == world && (e.flags & FL_MONSTER) && e.vr_oblivious)
        {
            vr_trailer_grunt = e;
        }
        if(vr_trailer_axe == world && e.classname == "func_weapon_grabbable" && (e.spawnflags & WEAPON_GRABBABLE_AS_PLACED))
        {
            vr_trailer_axe = e;
        }
        e = nextent(e);
    }
    if(vr_trailer_grunt != world)
    {
        vr_trailer_grunt_org = vr_trailer_grunt.origin;
        vr_trailer_grunt_ang = vr_trailer_grunt.angles;
        vr_trailer_grunt_class = vr_trailer_grunt.classname;
    }
    dprint(sprintf("trailer: the scene: grunt %s at %v, axe spawner %s at %v\n", vr_trailer_grunt.classname,
        vr_trailer_grunt_org, vr_trailer_axe.classname, vr_trailer_axe.origin));
};

// The Super Axe out of player `p`'s hands and holsters, and not his any more.
void(entity p, float xWid) VR_Trailer_TakeWeapon =
{
    if(VRGetEntWeapon(p, cVR_MainHand) == xWid) { VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0); }
    if(VRGetEntWeapon(p, cVR_OffHand) == xWid) { VRGiveEntWeapon(p, cVR_OffHand, WID_FIST, 0, 0); }
    string m = WeaponIdToModel(WID_FIST, 0);
    if(p.holsterweapon0 == xWid) { p.holsterweapon0 = WID_FIST; p.holsterweaponflags0 = 0; p.holsterweaponmodel0 = m; p.holsterweaponinst0 = WeaponInst_Carried(WID_FIST, 0, 0); }
    if(p.holsterweapon1 == xWid) { p.holsterweapon1 = WID_FIST; p.holsterweaponflags1 = 0; p.holsterweaponmodel1 = m; p.holsterweaponinst1 = WeaponInst_Carried(WID_FIST, 0, 0); }
    if(p.holsterweapon2 == xWid) { p.holsterweapon2 = WID_FIST; p.holsterweaponflags2 = 0; p.holsterweaponmodel2 = m; p.holsterweaponinst2 = WeaponInst_Carried(WID_FIST, 0, 0); }
    if(p.holsterweapon3 == xWid) { p.holsterweapon3 = WID_FIST; p.holsterweaponflags3 = 0; p.holsterweaponmodel3 = m; p.holsterweaponinst3 = WeaponInst_Carried(WID_FIST, 0, 0); }
    if(p.holsterweapon4 == xWid) { p.holsterweapon4 = WID_FIST; p.holsterweaponflags4 = 0; p.holsterweaponmodel4 = m; p.holsterweaponinst4 = WeaponInst_Carried(WID_FIST, 0, 0); }
    if(p.holsterweapon5 == xWid) { p.holsterweapon5 = WID_FIST; p.holsterweaponflags5 = 0; p.holsterweaponmodel5 = m; p.holsterweaponinst5 = WeaponInst_Carried(WID_FIST, 0, 0); }
    VR_WeaponUtil_EntDelWeapon(p, xWid);
};

// vr_trailer_reset: everything as at the scene's start.
void() VR_Trailer_Reset =
{
    if(vr_trailer == world)
    {
        bprint("vr_trailer_reset: not in the trailer scene (map vrtrailer)\n");
        return;
    }
    if(!vr_trailer_ready)
    {
        VR_Trailer_Setup();
    }
    float removed = 0;
    // the grunt (alive, dying, a corpse), his head (a head gib has his class), his gibs, his gun, his backpack
    entity e = nextent(world);
    while(e != world)
    {
        entity nx = nextent(e);
        if(!(e.flags & FL_CLIENT) && e != vr_trailer && e != vr_trailer_axe &&
           (e == vr_trailer_grunt || (vr_trailer_grunt_class != "" && e.classname == vr_trailer_grunt_class) ||
            ((e.classname == "gib" || (e.flags & FL_ITEM) ||
              (e.classname == "thrown_weapon" && e.weapon != vr_trailer_axe.weapon)) &&
             vlen(e.origin - vr_trailer_grunt_org) < VR_TRAILER_CLEAN_RADIUS)))
        {
            dprint(sprintf("trailer: reset: removed %s at %v\n", e.classname, e.origin));
            remove(e);
            removed = removed + 1;
        }
        e = nx;
    }
    // the grunt again, as the map made him
    if(vr_trailer_grunt_class == "monster_army")
    {
        entity oself = self;
        entity g = spawn();
        g.classname = vr_trailer_grunt_class;
        g.vr_oblivious = 1;
        g.angles = vr_trailer_grunt_ang;
        setorigin(g, vr_trailer_grunt_org);
        self = g;
        monster_army(); // (the scene's one kind: a grunt)
        self = oself;
        vr_trailer_grunt = g;
    }
    // the Super Axe: out of the player's hands, off the bridge, on its pedestal again
    entity p = find(world, classname, "player");
    if(vr_trailer_axe != world)
    {
        while(p != world)
        {
            VR_Trailer_TakeWeapon(p, vr_trailer_axe.weapon);
            p = find(p, classname, "player");
        }
        e = find(world, classname, "thrown_weapon");
        while(e != world)
        {
            entity nt = find(e, classname, "thrown_weapon");
            if(e.weapon == vr_trailer_axe.weapon)
            {
                remove(e);
                removed = removed + 1;
            }
            e = nt;
        }
        VR_WeaponGrabbable_Make(vr_trailer_axe);
    }
    // the player at the start, facing along the bridge
    p = find(world, classname, "player");
    while(p != world)
    {
        setorigin(p, vr_trailer.origin);
        p.velocity = '0 0 0';
        p.angles = vr_trailer.angles;
        p.v_angle = vr_trailer.angles;
        p.fixangle = TRUE;
        p.flags = p.flags - (p.flags & FL_ONGROUND);
        if(p.health > 0 && p.health < 100) { p.health = 100; }
        p = find(p, classname, "player");
    }
    vr_trailer_resets = vr_trailer_resets + 1;
    bprint(sprintf("trailer: reset %g: the grunt at %v, the axe on its pedestal, you at the start (%g things taken away)\n",
        vr_trailer_resets, vr_trailer_grunt_org, removed));
};

// The log (vr_trailer_log 1): the grunt's state, the axe's place, the player's.
void() VR_Trailer_Log =
{
    entity g = vr_trailer_grunt;
    entity a = find(world, classname, "thrown_weapon");
    while(a != world && a.weapon != vr_trailer_axe.weapon)
    {
        a = find(a, classname, "thrown_weapon");
    }
    entity p = find(world, classname, "player");
    string axe = (a != world) ? sprintf("axe %s", vtos(a.origin)) : sprintf("axe in hand %g/%g", p.weapon, p.weapon2);
    string doing = "other";
    if(g.think == army_stand1 || g.think == army_stand2 || g.think == army_stand3 || g.think == army_stand4 ||
       g.think == army_stand5 || g.think == army_stand6 || g.think == army_stand7 || g.think == army_stand8)
    {
        doing = "stand";
    }
    // (two short lines: the console wraps long ones)
    bprint(sprintf("trailer: t %.2f grunt health %g enemy %s frame %g think %s stl %g oblivious %g\n",
        time, g.health, g.enemy.classname, g.frame, doing, g.stl_state, g.vr_oblivious));
    bprint(sprintf("trailer: t %.2f yaw %.0f at %s | %s | player %s dist %.0f\n", time, g.angles_y, vtos(g.origin), axe,
        vtos(p.origin), vlen(p.origin - g.origin)));
};

// StartFrame (world.qc).
void() VR_Trailer_Frame =
{
    if(vr_trailer == world)
    {
        if(cvar_hget(cvarh_vr_trailer_reset))
        {
            cvar_set("vr_trailer_reset", "0");
            bprint("vr_trailer_reset: not in the trailer scene (map vrtrailer)\n");
        }
        return;
    }
    if(!vr_trailer_ready && time > 1)
    {
        VR_Trailer_Setup(); // (the grunt on the floor by now: walkmonster_start's drop)
    }
    if(cvar_hget(cvarh_vr_trailer_reset))
    {
        cvar_set("vr_trailer_reset", "0");
        VR_Trailer_Reset();
    }
    if(cvar_hget(cvarh_vr_trailer_log) && time >= vr_trailer_next_log && vr_trailer_ready)
    {
        vr_trailer_next_log = time + 0.25;
        VR_Trailer_Log();
    }
};
