//
//
//
// ----------------------------------------------------------------------------
// Mutator: Enemy Count (vr_run_enemycount; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// 0 1x (the map's), 1 2x, 2 3x: each of a map's monsters comes with one or two more of its kind (after Randomized
// Enemies' swap: vr_run_enemyrandom.qc) as the map loads. The engine's ED_LoadFromFile spawns each copy from the map's
// own keys of its monster, parsed again (VR_Run_MonsterSpawning answers how many; VR_Run_MonsterCopy places one), so a
// copy is the map's monster in every way but these:
// - where: beside its monster, a body's width or two away, on a clear spot reached from it in a straight line (no
//   wall between), on the floor within a step of its own for a walker (not lava or slime), in the same water for a
//   swimmer, in the open for a flyer; no room: no copy;
// - what it does when it dies: nothing. It keeps its monster's targetname (it wakes, appears and is killtargeted with
//   it) but not its target, killtarget or AD's deathtarget, so a map's death-counting triggers (trigger_counter, a door
//   opened by its guards' deaths) count the map's own monsters only, as the map was made: they complete as before. It
//   stands guard where it is (no path of its own: its path is its monster's target).
// - It counts in the kill stats (total_monsters) as any monster: killing every one is 100%.
// Known kinds only (VR_RUN_ER_KINDS: Quake's, AD's, the packs' common ones): no boss, no scripted monster, no crucified
// zombie. A saved game keeps the copies (they are its entities); a changelevel's map gets its own.

float() VR_RunEnemyCount_Copies =
{
    float n = min(2, rint(cvar_hget(cvarh_vr_run_enemycount)));
    if(n <= 0) return 0;
    if(VR_RunEnemyRandom_Row(self.classname) < 0 || VR_RunEnemyRandom_Crucified(self)) return 0;
    return n;
};

// A copy of half-width `half` at `p` would stand in another monster of the map's (or a copy placed before it): their
// boxes as their kinds' hulls at their spots. The trace misses the ones not solid yet (AD's start later, MG's and AD's
// wait for their trigger; AD's then telefrag what stands in them).
float(vector p, float half) VR_RunEnemyCount_Crowded =
{
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_run_kind != "" && e != self)
        {
            float row = VR_RunEnemyRandom_Row(e.vr_run_kind);
            float reach = half + 4 + ((row >= 0 && VR_RUN_ER_KINDS[row].size == 1) ? 16 : 32);
            if(fabs(e.origin_x - p_x) < reach && fabs(e.origin_y - p_y) < reach && fabs(e.origin_z - p_z) < 88)
            {
                return TRUE;
            }
        }
        e = nextent(e);
    }
    return FALSE;
};

// A copy's box at `p` would be in a map's trigger volume (a teleporter, a hurt, a push: a monster closet's teleporter
// sending it out at once) its monster's box at `o` is not in.
float(vector o, vector p, vector mn, vector mx) VR_RunEnemyCount_NewTrigger =
{
    entity t = nextent(world);
    while(t)
    {
        if(t.solid == SOLID_TRIGGER && substring(t.classname, 0, 8) == "trigger_")
        {
            float atP = (p_x + mn_x < t.absmax_x && p_x + mx_x > t.absmin_x && p_y + mn_y < t.absmax_y &&
                         p_y + mx_y > t.absmin_y && p_z + mn_z < t.absmax_z && p_z + mx_z > t.absmin_z);
            float atO = (o_x + mn_x < t.absmax_x && o_x + mx_x > t.absmin_x && o_y + mn_y < t.absmax_y &&
                         o_y + mx_y > t.absmin_y && o_z + mn_z < t.absmax_z && o_z + mx_z > t.absmin_z);
            if(atP && !atO) return TRUE;
        }
        t = nextent(t);
    }
    return FALSE;
};

// A copy of row `row` at `p` beside its monster's spot `o` (the map spawned; the copy not yet): reached from `o` in a
// straight line, clear of the world, the map's entities and its monsters, in no trigger its monster is not in,
// standing as its kind stands there.
float(vector o, vector p, float row) VR_RunEnemyCount_Fits =
{
    vector mn = VR_RunEnemyRandom_Mins(row);
    vector mx = VR_RunEnemyRandom_Maxs(row);
    float move = VR_RUN_ER_KINDS[row].move;
    vector up = '0 0 1';
    tracebox(o + up, mn, mx, p + up, TRUE, self);
    if(trace_fraction < 1 || trace_allsolid) return FALSE;
    tracebox(p + up, mn, mx, p + up, FALSE, self);
    if(trace_startsolid || trace_allsolid) return FALSE;
    if(VR_RunEnemyCount_Crowded(p, mx_x)) return FALSE;
    if(VR_RunEnemyCount_NewTrigger(o, p, mn, mx)) return FALSE;
    float here = pointcontents(p);
    if(move == RUN_ER_SWIM) return here == CONTENT_WATER && pointcontents(o) == CONTENT_WATER;
    if(here == CONTENT_LAVA || here == CONTENT_SLIME || here == CONTENT_SKY) return FALSE;
    if(move == RUN_ER_FLY) return here == CONTENT_EMPTY;
    // A walker: the floor under it within a step of its monster's (its spot's own floor, as the map placed it).
    tracebox(o + up, mn, mx, o - '0 0 64', TRUE, self);
    float ofloor = trace_fraction < 1 ? trace_endpos_z : o_z;
    tracebox(p + up, mn, mx, p - '0 0 64', TRUE, self);
    if(trace_fraction >= 1 || trace_plane_normal_z < 0.7 || fabs(trace_endpos_z - ofloor) > 24) return FALSE;
    float feet = pointcontents(trace_endpos + '0 0 1' * (mn_z + 2));
    return feet != CONTENT_LAVA && feet != CONTENT_SLIME;
};

// Copy k (self: the map's keys of its monster parsed again, the whole map spawned, it not yet; its kind its monster's):
// a spot beside the monster found (rings of eight around it, a body's width and a gap apart, the second ring twice
// that; k turns the ring, so two copies take different sides) and its death's targets dropped. FALSE: no room.
float(float k) VR_RunEnemyCount_Place =
{
    float row = VR_RunEnemyRandom_Row(self.classname);
    if(row < 0) return FALSE;
    vector o = self.origin;
    vector mx = VR_RunEnemyRandom_Maxs(row);
    float step = mx_x * 2 + 16;
    VR_RunEnemyRandom_Seed(self, 50 + k);
    float a0 = VR_RunEnemyRandom_Roll() * 360 + (k - 1) * 180;
    float ring;
    float i;
    for(ring = 1; ring <= 2; ring++)
    {
        for(i = 0; i < 8; i++)
        {
            vector ang = '0 0 0';
            ang_y = a0 + i * 45 + (ring - 1) * 22.5;
            makevectors(ang);
            vector p = o + v_forward * step * ring;
            if(VR_RunEnemyCount_Fits(o, p, row))
            {
                self.origin = p;
                self.vr_run_copy = k;
                self.target = "";
                self.killtarget = "";
                AD_ForgetDeathTarget(self);
                return TRUE;
            }
        }
    }
    return FALSE;
};

// The enemy mutators' check of the running map (VR_Run_EnemiesCheck): `count` copies of each map monster that may have
// them (at least most: some find no room; none at 1x), each beside a monster of its kind, not stuck, firing nothing;
// every trigger_counter still has as many targeting it as it counts; the kill count is every monster there (Quake's
// maps, before a kill). TRUE: as expected.
float(float xCount) VR_RunEnemyCount_Check =
{
    float ok = TRUE;
    float copyable = 0;
    float copies = 0;
    float alive = 0;
    entity e = nextent(world);
    entity o;
    while(e)
    {
        if((e.flags & FL_MONSTER) || e.MG_deferred || (substring(e.classname, 0, 8) == "monster_" && e.health > 0))
        {
            alive = alive + 1; // (a monster still to come: one in a closet, a trigger-spawned one)
        }
        else if(e.vr_run_mapclass != "" && VR_RunEnemyRandom_Row(e.vr_run_kind) >= 0 && e.health <= 0)
        {
            ok = FALSE; // (the checks run as the map starts, before any fight)
            dprint(sprintf("vr_run_enemies_check: %s (%s, copy %g) at %v dead at the start: health %g, its enemy %s at %v\n",
                           e.classname, e.vr_run_mapclass, e.vr_run_copy, e.origin, e.health, e.enemy.classname,
                           e.enemy.origin));
        }
        if(e.vr_run_mapclass != "" && !e.vr_run_copy && VR_RunEnemyRandom_Row(e.vr_run_kind) >= 0 &&
           !(e.vr_run_mapclass == "monster_zombie" && (e.spawnflags & 1)))
        {
            copyable = copyable + 1;
        }
        if(e.vr_run_copy)
        {
            copies = copies + 1;
            float near = FALSE;
            o = nextent(world);
            while(o && !near)
            {
                near = (!o.vr_run_copy && o.vr_run_kind == e.vr_run_kind && vlen(o.origin - e.origin) < 400);
                o = nextent(o);
            }
            float stuck = FALSE;
            if(!e.MG_deferred && e.solid != SOLID_NOT)
            {
                tracebox(e.origin, e.mins, e.maxs, e.origin, TRUE, e);
                stuck = trace_startsolid;
            }
            if(!near || stuck || e.target != "" || e.killtarget != "")
            {
                ok = FALSE;
                dprint(sprintf("vr_run_enemies_check: copy %s at %v: beside its kind %g, stuck %g, target \"%s\"\n",
                               e.vr_run_kind, e.origin, near, stuck, e.target));
            }
        }
        e = nextent(e);
    }
    float want = copyable * min(2, xCount);
    if(xCount <= 0 ? copies != 0 : (copies > want || copies < want * 0.6))
    {
        ok = FALSE;
        dprint(sprintf("vr_run_enemies_check: %g copies of %g monsters at enemy count %g\n", copies, copyable, xCount));
    }
    if(!killed_monsters && !AD_Rules() && total_monsters != alive)
    {
        ok = FALSE;
        dprint(sprintf("vr_run_enemies_check: total_monsters %g, %g monsters there\n", total_monsters, alive));
    }
    dprint(sprintf("vr_run_enemies_check: %g copies of %g monsters (%g there, total_monsters %g)\n", copies, copyable,
                   alive, total_monsters));
    return ok;
};

// The map's trigger chains as one number (the engine's check compares it with the same map's without the mutators):
// each target and killtarget that names something there (a copy's excluded: it has none) times 1000, plus the uses the
// map's trigger_counters can get (the entities targeting them). The same: the map's progression is as it was made.
float() VR_RunEnemyCount_Chains =
{
    float links = 0;
    float uses = 0;
    entity e = nextent(world);
    while(e)
    {
        if(!e.vr_run_copy)
        {
            if(e.target != "" && find(world, targetname, e.target)) { links = links + 1; }
            if(e.killtarget != "" && find(world, targetname, e.killtarget)) { links = links + 1; }
        }
        e = nextent(e);
    }
    e = find(world, classname, "trigger_counter");
    while(e)
    {
        entity o = e.targetname != "" ? find(world, target, e.targetname) : world;
        while(o) { uses = uses + 1; o = find(o, target, e.targetname); }
        e = find(e, classname, "trigger_counter");
    }
    dprint(sprintf("vr_run_enemies_check: chains %g links, %g counter uses\n", links, uses));
    return links * 1000 + uses;
};
