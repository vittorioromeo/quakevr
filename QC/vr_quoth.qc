// vr_quoth.qc -- Quoth's maps under Arcane Dimensions' rules (docs/vr-port/QUOTH.md). Quoth has no source: written from
// its maps' entity data and behaviour, never from its progs. AD re-implements nearly every Quoth classname; these are
// the rest, as thin aliases onto AD's entities and ours. ad_<classname> spawns only under AD's rules (Quoth's ruleset,
// vr_rules.cpp); the spawners are Quake VR's (the AD row's keptOurs: AD's own versions remove themselves).

// Quoth's map keys (inferred from its maps): a second name an entity is also used by; the rubble model.
.string targetname2;
.string modelpath;

// --- targetname2: an alias entity named targetname2 hands its use to its owner (doors, counters, relays, monsters) --

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

// StartFrame: once a map, every entity with a targetname2 gets its alias (an entity never used by that name would be a
// door that never opens, a counter that never counts).
void() VR_Quoth_Frame =
{
    local entity e, a;
    if (vr_quoth_aliased)
        return;
    vr_quoth_aliased = 1;
    for (e = nextent(world); e; e = nextent(e))
    {
        if (e.targetname2 == "" || e.classname == "vr_quoth_alias")
            continue;
        a = spawn();
        a.classname = "vr_quoth_alias";
        a.targetname = e.targetname2;
        a.owner = e;
        a.use = VR_Quoth_AliasUse;
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
    self.model = "";
    spawnfn();
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
