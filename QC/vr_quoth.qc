// vr_quoth.qc -- Quoth's maps under Arcane Dimensions' rules (docs/vr-port/QUOTH.md). Quoth has no source: written from
// its maps' entity data and behaviour, never from its progs. AD re-implements nearly every Quoth classname; these are
// the rest, as thin aliases onto AD's entities and ours. ad_<classname> spawns only under AD's rules (Quoth's ruleset,
// vr_rules.cpp); the spawners are Quake VR's (the AD row's keptOurs: AD's own versions remove themselves).

// Quoth's map keys (inferred from its maps): a second and a third name an entity is also used by (targetname3: only
// kellbase1's count3; no map has a targetname4); the rubble model.
.string targetname2;
.string targetname3;
.string ad_target3, ad_target4, ad_killtarget2; // keys target3, target4, killtarget2 (AD fires target, target2)
.string modelpath;

// --- targetname2, targetname3: an alias entity of that name hands its use to its owner (doors, counters, relays,
// monsters) -------------------------------------------------------------------------------------------------------

float vr_quoth_aliased; // (this map's aliases made: once, after every entity spawned; saved with the game)

void() VR_Quoth_AliasUse =
{
    local entity oself;
    if (!self.owner.use)
        return;
    oself = self;
    self = self.owner;
    self.use();
    self = oself;
};

void(entity e, string name) VR_Quoth_Alias =
{
    local entity a;
    if (name == "")
        return;
    a = spawn();
    a.classname = "vr_quoth_alias";
    a.targetname = name;
    a.owner = e;
    a.use = VR_Quoth_AliasUse;
};

// --- target3, target4, killtarget2: AD's SUB_UseTargets fires target and target2 only, so an entity with more fires
// a relay of its own (by target2, a name made for it) that fires its old target2 and target3, then a second one that
// fires target4; the first also kills killtarget2. Its delay and activator carry over (kellbase1's three buttons:
// count1, count2, count3, their indicator) ----------------------------------------------------------------------

float vr_quoth_relays; // (relays made: their names)

void() VR_Quoth_RelayUse = { ad_SUB_UseTargets(); };

string(string first, string second) VR_Quoth_Relay =
{
    local entity r;
    vr_quoth_relays = vr_quoth_relays + 1;
    r = spawn();
    r.classname = "vr_quoth_relay";
    r.targetname = strzone(sprintf("vr_quoth_relay%g", vr_quoth_relays));
    r.target = first;
    r.ad_target2 = second;
    r.use = VR_Quoth_RelayUse;
    return r.targetname;
};

void(entity e) VR_Quoth_MoreTargets =
{
    local string last;
    last = e.ad_target3;
    if (e.ad_target4 != "")
        last = VR_Quoth_Relay(e.ad_target3, e.ad_target4);
    e.ad_target2 = VR_Quoth_Relay(e.ad_target2, last);
    find(world, targetname, e.ad_target2).killtarget = e.ad_killtarget2;
    e.ad_target3 = e.ad_target4 = e.ad_killtarget2 = "";
};

// kellbase1's corridor (modladders, his report 2026-11-21): between its top doors (`topdoors`, y -1176: TOGGLE doors
// that its lower chain's counter opens, "the top doors are open") and its south exit doors (`exitlock`, y -720, open only
// after the arena's 16 kills). The corridor's trigger_once by the exit doors toggles the top doors shut behind you, so
// coming up from below before the arena shuts you in. A fail-safe, once a map: a living player in the corridor for 10 s
// with the top doors and the south exit doors shut and no living monster in it gets the top doors opened again.
float vr_quoth_kell_trapped; // (seconds so)

float(entity e) VR_Quoth_KellInCorridor =
{
    return e.origin_x > -159 && e.origin_x < 223 && e.origin_y > -1169 && e.origin_y < -745 && e.origin_z > 600 &&
        e.origin_z < 800;
};

float(string name, float southOnly) VR_Quoth_KellDoorsShut =
{
    local entity d;
    for (d = find(world, targetname, name); d; d = find(d, targetname, name))
        if (d.classname == "func_door" && (!southOnly || d.absmax_y < -700) && (vlen(d.origin - d.pos1) > 1 || d.velocity != '0 0 0'))
            return FALSE;
    return TRUE;
};

void() VR_Quoth_KellThink =
{
    local entity e, p = world;
    self.nextthink = time + 1;
    for (e = VR_Player(); e; e = VR_NextPlayer(e))
        if (e.health > 0 && VR_Quoth_KellInCorridor(e))
            p = e;
    for (e = nextent(world); e && p; e = nextent(e))
        if ((e.flags & FL_MONSTER) && e.health > 0 && VR_Quoth_KellInCorridor(e))
            p = world;
    if (!p || !VR_Quoth_KellDoorsShut("topdoors", FALSE) || !VR_Quoth_KellDoorsShut("exitlock", TRUE))
    {
        vr_quoth_kell_trapped = 0;
        return;
    }
    vr_quoth_kell_trapped = vr_quoth_kell_trapped + 1;
    if (vr_quoth_kell_trapped < 10)
        return;
    dprint("quoth: kellbase1's corridor: shut in, the top doors opened again\n");
    activator = p;
    self.target = "topdoors";
    ad_SUB_UseTargets();
    remove(self);
};

// StartFrame: once a map, every entity with a targetname2 or targetname3 gets an alias for each (an entity never used
// by that name would be a door that never opens, a counter that never counts); one with a target3, target4 or
// killtarget2 its relays.
void() VR_Quoth_Frame =
{
    local entity e;
    if (vr_quoth_aliased)
        return;
    vr_quoth_aliased = 1;
    if (mapname == "kellbase1" && find(world, targetname, "topdoors") && find(world, targetname, "exitlock"))
    {
        e = spawn();
        e.classname = "vr_quoth_kellguard";
        e.think = VR_Quoth_KellThink;
        e.nextthink = time + 1;
    }
    for (e = nextent(world); e; e = nextent(e))
        if (e.classname != "vr_quoth_alias" && e.classname != "vr_quoth_relay")
        {
            VR_Quoth_Alias(e, e.targetname2);
            VR_Quoth_Alias(e, e.targetname3);
            if (e.ad_target3 != "" || e.ad_target4 != "" || e.ad_killtarget2 != "")
                VR_Quoth_MoreTargets(e);
        }
};

// --- point triggers: AD's trigger, its volume from size (else mangle; else the point itself: Copper's quoth2-style
// bounding-box triggers, cu_InitTriggerBounds, read the same keys), centred on its origin --------------------------

void(void() spawnfn) VR_Quoth_PointTrigger =
{
    local vector sz;
    sz = self.size;
    if (sz == '0 0 0')
    {
        sz = self.mangle;
        self.mangle = '0 0 0'; // (not its angle)
    }
    // (a model it has not: AD's trigger without one is a marker that does nothing, never touched nor used; AD's empty
    // one-point model, precached by its worldspawn, then hidden by its InitTrigger)
    self.model = ad_MODEL_EMPTY;
    spawnfn();
    self.mdl = "";
    setsize(self, sz * -0.5, sz * 0.5);
    self.ad_bbmins = self.mins; // (AD's entity state restores these)
    self.ad_bbmaxs = self.maxs;
    setorigin(self, self.origin);
};

void() ad_trigger_once_point = { VR_Quoth_PointTrigger(ad_trigger_once); };
void() ad_trigger_secret_point = { VR_Quoth_PointTrigger(ad_trigger_secret); };
void() ad_trigger_monsterjump_point = { VR_Quoth_PointTrigger(ad_trigger_monsterjump); };
void() ad_trigger_drolejump_point = { VR_Quoth_PointTrigger(ad_trigger_drolejump); };

// --- spawners: when used, `count` monsters (default 3) of the classname `spawnfunction` names, `wait` seconds apart
// (default 0.3: AD's documented defaults), each with the spawner's target (fired as it dies: a counter counts them),
// angles and flags. info_multispawn and func_hordespawn alike (AD's notes: the same). --------------------------------

void() VR_Quoth_SpawnOne =
{
    local entity m, oself;
    m = spawn();
    m.classname = self.spawnclassname;
    m.angles = self.angles;
    m.target = self.target;
    m.killtarget = self.killtarget;
    m.spawnflags = self.spawnflags;
    setorigin(m, self.origin);
    oself = self;
    self = m;
    oself.spawnfunction();
    self = oself;
    if (m.flags & FL_MONSTER)
    {
        WriteByte(MSG_ALL, SVC_UPDATESTAT);
        WriteByte(MSG_ALL, STAT_TOTALMONSTERS);
        WriteLong(MSG_ALL, total_monsters);
    }
    spawn_tfog(m.origin);
    self.enemy = m; // (its last one)
};

void() VR_Quoth_SpawnerThink =
{
    VR_Quoth_SpawnOne();
    self.cnt = self.cnt - 1;
    if (self.cnt > 0)
    {
        self.think = VR_Quoth_SpawnerThink;
        self.nextthink = time + self.wait;
    }
};

void() VR_Quoth_SpawnerUse =
{
    self.cnt = self.count;
    VR_Quoth_SpawnerThink();
};

/*QUAKED info_multispawn (1 0.5 0.5) (-16 -16 -16) (16 16 16)
Quoth's monster spawner (Quoth maps, under Arcane Dimensions' rules). When used, spawns "count" monsters (default 3)
of the classname "spawnfunction" names, "wait" seconds apart (default 0.3), each with this entity's target (fired as
it dies), angles and spawnflags.
*/
void() info_multispawn =
{
    if (!self.spawnfunction)
    {
        remove(self); // (its monster unknown: the engine said which)
        return;
    }
    if (self.count <= 0)
        self.count = 3;
    if (self.wait <= 0)
        self.wait = 0.3;
    if (self.spawnclassname == "")
        self.spawnclassname = "monster"; // (the engine names it from spawnfunction: pr_edict.c ED_ParseEdict)
    self.use = VR_Quoth_SpawnerUse;
};

/*QUAKED func_hordespawn (1 0.5 0.5) (-16 -16 -16) (16 16 16)
Quoth's horde spawner: the same as info_multispawn (Quoth maps, under Arcane Dimensions' rules). When used, spawns
"count" monsters (default 3) of the classname "spawnfunction" names, "wait" seconds apart (default 0.3).
*/
void() func_hordespawn = { info_multispawn(); };

// --- the rest: aliases -------------------------------------------------------------------------------------------

// Quoth's death guard's other spelling.
void() ad_monster_deathguard = { self.classname = "monster_death_guard"; ad_monster_death_guard(); };

// Quoth's grilles: its sprites (Quoth's folder), as AD's custom map objects.
void() ad_mapobject_grill64 = { self.model = "progs/grill64.spr"; ad_mapobject_custom(); };
void() ad_mapobject_grill128 = { self.model = "progs/grill128.spr"; ad_mapobject_custom(); };

// Quoth's machinery hums: its looped sounds (Quoth's folder), as AD's custom looped ambient sound (always playing).
void() ad_ambient_driveshaft = { self.noise = "necros/drivshft.wav"; ad_ambient_custom_loop(); };
void() ad_ambient_thrum = { self.noise = "ambience/thrum.wav"; ad_ambient_custom_loop(); };

// Quoth's bio and plasma boxes: AD's exploding boxes (large and small).
void() ad_misc_biobox_l = { ad_misc_explobox(); };
void() ad_misc_biobox_s = { ad_misc_explobox2(); };
void() ad_misc_plasmabox_l = { ad_misc_explobox(); };
void() ad_misc_plasmabox_s = { ad_misc_explobox2(); };

// Rubble thrown when used: AD's breakable spawner, its material from Quoth's rubble model (glass, metal; else rock).
void() ad_info_rubble =
{
    if (!self.style)
        self.style = self.modelpath == "progs/glass.mdl" ? 3 : self.modelpath == "progs/metal.mdl" ? 4 : 1;
    ad_func_breakable_spawner();
};

// Console commands a map runs (info_command_spawn at its start, trigger_command_contact on touch): AD runs none
// (its info_command removes itself), so neither do these.
void() ad_info_command_spawn = { ad_info_command(); };
void() ad_trigger_command_contact = { remove(self); };

// Quoth's flashlight pickup: Quake VR's flashlight is always at hand.
void() ad_item_flashlight = { remove(self); };
