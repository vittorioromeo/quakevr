//
//
//
// ----------------------------------------------------------------------------
// Mutator: Randomized Enemies (vr_run_enemyrandom; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// A map's monsters swapped for others as the map loads (the engine's ED_LoadFromFile asks VR_Run_MonsterSpawning before
// a monster's spawn function runs: self is its keys, parsed; the swap is its classname, so the new kind spawns from the
// map's own keys: its targetname, target, spawnflags, angle). 1 Balanced: a kind of the same threat tier that moves as
// it does (walks, flies, swims); 2 Anything Goes: any tier, a walker's spot may hold a flyer. Either way the new kind
// fits the spot (never larger than what stood there: Quake's two hulls), never a boss; Arcane Dimensions' kinds only while AD's data
// is mounted, the official expansions' (Scourge of Armagon, Dissolution of Eternity, Dimension of the Machine) while
// theirs is (and spawning outside AD's maps: vr_ad_everywhere). The kind it was is kept (vr_run_mapclass). Enemy Count's
// copies (vr_run_enemycount.qc) come after, of the swapped kind. Armed humanoids: a grunt, enforcer, knight or death
// knight swapped in is armed as the run's Armed Humanoids say (its spawn function decides, as for the map's own).
// The roll: the map's name, the spawn point and the map's content seed (VR_AGR_SeedOf, vr_map_content_seed): the same
// map and seed give the same monsters; a saved game keeps them (they are its entities).

.string vr_run_mapclass; // the map's own kind of a monster the run's enemy mutators looked at (set: they did)
.string vr_run_kind;     // ... the kind it spawned as (the swap's; its spawn function may rename it after)
.float vr_run_copy;      // Enemy Count's copy k of its map's monster (0: the map's own)
.float vr_run_kindflags; // the spawnflags a monster looked at spawned with (its copies' too)

// The kinds: vr_monster_kinds.qc's table (VR_MONSTER_KINDS: how each moves, its hull, its tier, whose, its height; its
// never rows: bosses and scripted monsters, never swapped or copied).

// A row's box for placing it: 32 wide and 64 tall (AD's tall ones), or Quake's 64-wide hull (64 wide, 88 tall): the
// engine traces each monster's own size (vr_hull's compiled hulls), so the tallest of a width.
vector(float xRow) VR_RunEnemyRandom_Mins = { return VR_MONSTER_KINDS[xRow].size == 2 ? VEC_HULL2_MIN : VEC_HULL_MIN; };
vector(float xRow) VR_RunEnemyRandom_Maxs = { return VR_MONSTER_KINDS[xRow].size == 2 ? VEC_HULL2_MAX : '16 16 40'; };

// Arcane Dimensions' kinds can spawn in this map (its data mounted; in AD's own maps, or anywhere with vr_ad_everywhere:
// the engine's VR_AD_SpawnsOutside).
float() VR_RunEnemyRandom_ADHere =
{
    if(!cvar_hget(cvarh_vr_ad_mounted)) return FALSE;
    return AD_Rules() || cvar_hget(cvarh_vr_ad_everywhere);
};

// A crucified zombie (Quake's spawnflag 1 on monster_zombie: nailed to its wall), a spider on AD's ceiling (4): placed as
// only its kind can stand there; never swapped, never copied.
float(entity e) VR_RunEnemyRandom_Crucified =
{
    return (e.classname == "monster_zombie" && (e.spawnflags & 1)) ||
           (e.classname == "monster_spider" && AD_Rules() && (e.spawnflags & 4));
};

// The roll: a small generator seeded from `e`'s map, spawn point and `salt`, and the map's content seed.
float vr_run_er_state;

void(entity e, float salt) VR_RunEnemyRandom_Seed =
{
    vector s = VR_AGR_SeedOf(e, salt);
    float c = VR_AGR_Mod(fabs(rint(vr_map_content_seed)), 30011);
    vr_run_er_state = 1 + VR_AGR_Mod(s_x * 7 + s_y * 13 + s_z * 101 + c * 17, 30260);
};

float() VR_RunEnemyRandom_Roll =
{
    vr_run_er_state = VR_AGR_Mod(vr_run_er_state * 171, 30269);
    return vr_run_er_state / 30269;
};

// Room for row `to`'s head over `e`'s spot (it stood there with its top `xTop` over its origin): the world clear straight
// up from the origin to `to`'s top, at the middle and at its box's four corners (a corner in a wall beside it is not
// looked at: only what is over it). Lines, not a box: the engine's box traces of a size are its hulls'.
float(entity e, float xTop, float to) VR_RunEnemyRandom_Headroom =
{
    float top = VR_MONSTER_KINDS[to].top;
    if(top <= xTop) return TRUE;
    float r = 15;
    if(VR_MONSTER_KINDS[to].size == 2) { r = 31; }
    for(float k = 0; k < 5; k++)
    {
        vector at = e.origin;
        if(k == 1 || k == 2) { at_x = at_x + r; }
        if(k == 3 || k == 4) { at_x = at_x - r; }
        if(k == 1 || k == 3) { at_y = at_y + r; }
        if(k == 2 || k == 4) { at_y = at_y - r; }
        traceline(at, at + '0 0 1' * top, TRUE, e);
        if(trace_startsolid || trace_allsolid)
        {
            if(k == 0) return FALSE; // (its origin in solid: no room known)
            continue;
        }
        if(trace_fraction < 1) return FALSE;
    }
    return TRUE;
};

// Kind `to` may stand where `e` (a monster of row `from`, not yet spawned) stands, as the choice `mode` allows.
// Kinds refused on this map (their spawn function left nothing: the engine's ED_LoadFromFile, VR_Run_KindRefused), the
// map they were refused on, the row of the last swap and the monster count before its spawn function ran.
nosave float vr_run_er_bad[VR_MONSTER_KINDS_MAX];
nosave string vr_run_er_badmap;
float vr_run_er_lastrow;
float vr_run_er_tm;

// Kind `to` may stand where `e` (a monster of row `from`, not yet spawned) stands, as the choice `mode` allows.
float(entity e, float from, float to, float mode) VR_RunEnemyRandom_Allowed =
{
    if(to != from && vr_run_er_bad[to]) return FALSE;
    float src = VR_MONSTER_KINDS[to].src;
    float fmove = VR_MONSTER_KINDS[from].move;
    float tmove = VR_MONSTER_KINDS[to].move;
    if(VR_MONSTER_KINDS[to].tier <= 0 || src == RUN_ER_PACK) return FALSE;
    if(src == RUN_ER_AD && !VR_RunEnemyRandom_ADHere()) return FALSE;
    if(src == RUN_ER_EXP && !VR_MonsterKind_DataHere(to)) return FALSE;
    if(mode == 1)
    {
        if(tmove != fmove || VR_MONSTER_KINDS[to].tier != VR_MONSTER_KINDS[from].tier) return FALSE;
    }
    else if(tmove != fmove && !(fmove == RUN_ER_WALK && tmove == RUN_ER_FLY))
    {
        return FALSE; // (a flyer's spot may be over a drop, a swimmer's is water)
    }
    if(VR_MONSTER_KINDS[to].cls == "monster_zombie" && (e.spawnflags & 1)) return FALSE; // (1: a zombie's crucifixion)
    // Never larger than what stood there: the map's doors, lifts and walls spawn around its spot after it, so a hull
    // test now would miss them (a large kind in a small one's spot was crushed or telefragged as the map started).
    if(VR_MONSTER_KINDS[to].size > VR_MONSTER_KINDS[from].size) return FALSE;
    // ... and no taller, unless the world leaves it the room over the spot (a tall kind in a short one's spot under a
    // low ceiling stood with its head in it: 5-13 of a map's monsters, ROUND25.md "mutenemies").
    return VR_RunEnemyRandom_Headroom(e, VR_MONSTER_KINDS[from].top, to);
};

// The swap (self: a map's monster, its keys parsed, before its spawn function): its classname may change. The keys
// that only mean something to the kind it was are dropped: a mapper's health and skin, the class-specific spawnflags
// (2; 4 in AD's maps, where a monster that has them is never swapped), AD's box and skin keys; a zombie's ambush (2)
// becomes everyone's (1).
void() VR_RunEnemyRandom_Swap =
{
    float mode = cvar_hget(cvarh_vr_run_enemyrandom);
    vr_run_er_lastrow = -1;
    vr_run_er_tm = total_monsters;
    if(vr_run_er_badmap != mapname)
    {
        for(float b = 0; b < VR_MONSTER_KINDS_MAX; b++) vr_run_er_bad[b] = FALSE;
        vr_run_er_badmap = mapname;
    }
    if(mode <= 0) return;
    float from = VR_MonsterKind_Row(self.classname);
    if(from < 0 || VR_MONSTER_KINDS[from].src == RUN_ER_PACK || VR_RunEnemyRandom_Crucified(self)) return;
    if(AD_Rules() && (self.spawnflags & 6)) return; // (AD's placed it for its kind's own: a lying zombie, a ceiling spider)
    float n = 0;
    float i;
    for(i = 0; i < VR_MONSTER_KINDS_N; i++)
    {
        if(VR_RunEnemyRandom_Allowed(self, from, i, mode)) { n = n + 1; }
    }
    if(n <= 1) return;
    VR_RunEnemyRandom_Seed(self, 31);
    float pick = floor(VR_RunEnemyRandom_Roll() * n);
    for(i = 0; i < VR_MONSTER_KINDS_N; i++)
    {
        if(!VR_RunEnemyRandom_Allowed(self, from, i, mode)) continue;
        if(pick > 0) { pick = pick - 1; continue; }
        if(i == from) return; // (it stays what it was: a kind has its own chance too)
        float ambush = (self.spawnflags & 1) || (self.classname == "monster_zombie" && (self.spawnflags & 2));
        float flags = self.spawnflags - (self.spawnflags & (AD_Rules() ? 7 : 3));
        if(ambush)
        {
            flags = flags | (VR_MONSTER_KINDS[i].cls == "monster_zombie" ? 2 : 1);
        }
        vr_run_er_lastrow = i;
        VR_RunEnemyRandom_Become(VR_MONSTER_KINDS[i].cls, flags);
        return;
    }
};

// Self (its keys parsed, not yet spawned) becomes kind `xCls` with `xFlags`: the keys only its old kind read dropped.
void(string xCls, float xFlags) VR_RunEnemyRandom_Become =
{
    self.classname = xCls;
    self.spawnflags = xFlags;
    self.health = 0;
    self.skin = 0;
    AD_ForgetKindKeys(self);
};

// The enemy mutators' check of the running map (the engine's vr_run_enemies_check; QC VR_Run_EnemiesCheck): every
// swapped monster's kind as the choice `mode` allows (none at 0; some, on a map with several). TRUE: as expected.
float(float mode) VR_RunEnemyRandom_Check =
{
    float ok = TRUE;
    float swapped = 0;
    float adin = 0;
    float expin = 0;
    float looked = 0;
    float hash = 0; // (the outcome: each map monster's kind and place, compared across changes of the kinds' data)
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_run_mapclass != "" && !e.vr_run_copy)
        {
            looked = looked + 1;
            float kl = strlen(e.vr_run_kind);
            hash = VR_AGR_Mod(hash * 31 + kl * 7 + str2chr(e.vr_run_kind, 8) * 13 + str2chr(e.vr_run_kind, kl - 1) * 3 +
                              VR_AGR_Mod(fabs(rint(e.origin_x + e.origin_y * 3)), 1009), 1000003);
            if(e.vr_run_mapclass != e.vr_run_kind)
            {
                swapped = swapped + 1;
                float f = VR_MonsterKind_Row(e.vr_run_mapclass);
                float t = VR_MonsterKind_Row(e.vr_run_kind);
                if(t >= 0 && VR_MONSTER_KINDS[t].src == RUN_ER_AD) { adin = adin + 1; }
                if(t >= 0 && VR_MONSTER_KINDS[t].src == RUN_ER_EXP) { expin = expin + 1; }
                if(mode <= 0 || f < 0 || t < 0 || VR_MONSTER_KINDS[t].src == RUN_ER_PACK ||
                   (VR_MONSTER_KINDS[t].src == RUN_ER_AD && !VR_RunEnemyRandom_ADHere()) ||
                   (VR_MONSTER_KINDS[t].src == RUN_ER_EXP && !VR_MonsterKind_DataHere(t)) ||
                   (mode == 1 && (VR_MONSTER_KINDS[t].move != VR_MONSTER_KINDS[f].move ||
                                  VR_MONSTER_KINDS[t].tier != VR_MONSTER_KINDS[f].tier)))
                {
                    ok = FALSE;
                    dprint(sprintf("vr_run_enemies_check: %s at %v swapped for %s (mode %g)\n", e.vr_run_mapclass,
                                   e.origin, e.vr_run_kind, mode));
                }
            }
        }
        e = nextent(e);
    }
    if(mode > 0 && looked >= 8 && !swapped)
    {
        ok = FALSE;
        dprint(sprintf("vr_run_enemies_check: none of %g monsters swapped (mode %g)\n", looked, mode));
    }
    dprint(sprintf("vr_run_enemies_check: %g monsters looked at, %g swapped (%g for AD's kinds, %g for the expansions'; %g kinds listed; outcome %g)\n",
                   looked, swapped, adin, expin, VR_MONSTER_KINDS_N, hash));
    return ok;
};

// The engine (ED_LoadFromFile): the kind just swapped in was removed by its own spawn function (its data short of
// its probe's). Refused on this map; the monster count as before it (it may have counted itself); the engine
// spawns the monster's keys again.
void() VR_Run_KindRefused =
{
    if(vr_run_er_lastrow < 0) return;
    vr_run_er_bad[vr_run_er_lastrow] = TRUE;
    total_monsters = vr_run_er_tm;
    dprint(sprintf("Randomized enemies: %s refused on this map (its spawn function removed it), its spot spawned again\n",
                   VR_MONSTER_KINDS[vr_run_er_lastrow].cls));
};
