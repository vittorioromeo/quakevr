#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedknight_test.qc (ROUND23.md, "Knights as armed humanoids"):
//
// Test 95: the knights as armed humanoids (vr_agknight). In vrtesthall, a knight 90 ahead of you, made by its own spawn
// function (monster_knight) as a map's would be:
//   off      vr_agknight 0: a plain knight (its own model, not armed): nothing changes with the setting off;
//   spawn    vr_agknight 1: an armed knight (kind 2, progs/vrknight.mdl) holding a sword, the sword at its hand;
//   swing    it hunts you: its sword (the armed humanoids' melee) hurts you;
//   disarm   its sword knocked out (VR_AG_Disarm): lying by it; it takes it up again (you unseen);
//   death    killed (not gibbed): its sword falls, once (its own sword drop, VR_DropKnightSword, not as well).
// `vr_agrunt_test_kind 3`: the same for the death knight (vr_agdknight, monster_hell_knight, progs/vrhknight.mdl), with
//   spell    after its swing, moved out of reach (you in sight): it casts its fireball volley (its knightspikes) with
//            its sword in its hand, and still holds it after; cast with his off hand (the Cast pose), the volley from
//            its palm as drawn (out from his shoulder, ahead of his middle towards you); last, a gun holder's cast
//            cooldown's rule (vr_agdknight_cast_cooldown; its play over minutes: vr_agtune_test 3).
// Each prints `agtest: PASS|FAIL <what>`; the run ends on `agtest: test 95 over: N passed, M failed` (-QuitOn).
// A case that never gets there FAILs as "timed out" (a safety net).
// ----------------------------------------------------------------------------

float vr_aknt_case;  // 0 off, 1 spawn, 2 swing, 3 disarm, 4 death, 5 done
float vr_aknt_t0;
float vr_aknt_flag0; // vr_agknight as it was
float vr_aknt_hp;    // the player's health as the swing case began
vector vr_aknt_home;

// The knight kind under test (vr_agrunt_test_kind 3: the death knight; else the knight) and its names.
float() VR_AKNT_Dk = { return rint(cvar_hget(cvarh_vr_agrunt_test_kind)) == VR_AM_DKNIGHT; };
string() VR_AKNT_Flag = { return VR_AKNT_Dk() ? "vr_agdknight" : "vr_agknight"; };
string() VR_AKNT_Name = { return VR_AKNT_Dk() ? "death knight" : "knight"; };

// Its fireballs in flight (knightspikes it cast).
float(entity g) VR_AKNT_Spells =
{
    float n = 0;
    entity e = find(world, classname, "knightspike");
    while(e)
    {
        n = n + (e.owner == g);
        e = find(e, classname, "knightspike");
    }
    return n;
};

// A knight `dist` ahead of the player, made by monster_knight.
entity(float dist) VR_AKNT_Spawn =
{
    entity p = vr_agt_player;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    traceline(p.origin, p.origin + fw * dist, TRUE, p);
    vector at = trace_endpos - fw * 24;
    entity oldself = self;
    self = spawn();
    self.classname = VR_AKNT_Dk() ? "monster_hell_knight" : "monster_knight";
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    if(VR_AKNT_Dk())
    {
        monster_hell_knight();
    }
    else
    {
        monster_knight();
    }
    entity g = self;
    self = oldself;
    return g;
};

// Swords lying within 96 of `at`.
float(vector at) VR_AKNT_Lying =
{
    float n = 0;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(vlen(e.origin - at) < 200) // (a dying one throws it 150 away)
        {
            n = n + 1;
        }
        e = find(e, classname, "thrown_weapon");
    }
    return n;
};

// Its sword knocked out (you unseen): lying by it; the re-arm case next.
void(entity g) VR_AKNT_Disarm =
{
    entity p = vr_agt_player;
    p.health = 100;
    p.flags = p.flags | FL_NOTARGET;
    g.enemy = world;
    makevectors(g.angles);
    VR_AG_Disarm(g, v_right, 120, "test 95");
    VR_AGT_Check(sprintf("knight disarm: empty-handed (%g), its sword lying by it (%g)", g.vr_ag_wid,
        VR_AKNT_Lying(g.origin)), !g.vr_ag_prop && VR_AKNT_Lying(g.origin) == 1);
    vr_aknt_case = 3;
    vr_aknt_t0 = time;
};

void() VR_AKNT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = time - vr_aknt_t0;
    if(vr_agt_step == 0)
    {
        if(vr_aknt_case == 0)
        {
            vr_aknt_home = p.origin;
            vr_aknt_flag0 = cvar(VR_AKNT_Flag());
            VR_AGT_ClearWeapons();
            cvar_set(VR_AKNT_Flag(), "0");
            g = VR_AKNT_Spawn(90);
            VR_AGT_Check(sprintf("%s off: a plain one (armed %g, model %s)", VR_AKNT_Name(), g.vr_ag, g.model),
                !g.vr_ag && g.model == (VR_AKNT_Dk() ? "progs/hknight.mdl" : "progs/knight.mdl"));
            remove(g);
            cvar_set(VR_AKNT_Flag(), "1");
            p.flags = p.flags | FL_NOTARGET;
            vr_agt_grunt = g = VR_AKNT_Spawn(90);
            VR_AGT_Check(sprintf("%s on: armed (%g), kind %g, model %s, a sword (%g)", VR_AKNT_Name(), g.vr_ag,
                g.vr_ag_kind, g.model, g.vr_ag_wid), g.vr_ag &&
                g.vr_ag_kind == (VR_AKNT_Dk() ? VR_AM_DKNIGHT : VR_AM_KNIGHT) &&
                g.model == (VR_AKNT_Dk() ? "progs/vrhknight.mdl" : "progs/vrknight.mdl") &&
                g.vr_ag_wid == WID_SWORD && g.vr_ag_prop != world);
            vr_aknt_case = 1;
            vr_aknt_t0 = time;
            vr_agt_step = 1;
        }
        return;
    }
    if(vr_aknt_case == 1)
    {
        if(t < 0.5)
        {
            return; // (its pose sent, its sword placed)
        }
        float d = g.vr_ag_prop ? vlen(g.vr_ag_prop.origin - VR_AM_Hand(g)) : 999;
        VR_AGT_Check(sprintf("knight spawn: its sword %.1f from its hand", d), d < 16);
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        p.health = 1000;
        vr_aknt_hp = p.health;
        VR_AGT_Hunt(g);
        vr_aknt_case = 2;
        vr_aknt_t0 = time;
        return;
    }
    if(vr_aknt_case == 2)
    {
        if(p.health < vr_aknt_hp || t > 15)
        {
            VR_AGT_Check(sprintf("knight swing: hurt you %g in %.1f s%s, holding %g", vr_aknt_hp - p.health, t,
                t > 15 ? " (timed out)" : "", g.vr_ag_wid), p.health < vr_aknt_hp && g.vr_ag_wid == WID_SWORD);
            if(VR_AKNT_Dk())
            {
                // Out of its reach, you in sight: its spell (vr_am_spell).
                vector dir = normalize(g.origin - p.origin);
                dir_z = 0;
                traceline(g.origin, g.origin + dir * 360, TRUE, g);
                setorigin(g, trace_endpos - dir * 32);
                g.angles_y = vectoyaw(p.origin - g.origin);
                p.health = 1000;
                vr_aknt_case = 6;
                vr_aknt_t0 = time;
                return;
            }
            VR_AKNT_Disarm(g);
        }
        return;
    }
    if(vr_aknt_case == 6)
    {
        if(VR_AKNT_Spells(g) > 0 || t > 15)
        {
            VR_AGT_Check(sprintf("death knight spell: cast %g fireballs at %.0f in %.1f s%s, holding %g",
                VR_AKNT_Spells(g), vlen(g.origin - p.origin), t, t > 15 ? " (timed out)" : "", g.vr_ag_wid),
                VR_AKNT_Spells(g) > 0 && g.vr_ag_wid == WID_SWORD && g.vr_ag_prop != world);
            // Cast with his off hand (the Cast pose: its palm at you): the volley from that palm as drawn, his arm out
            // (the palm well off his shoulder) and ahead of his middle towards you.
            vector mid = g.origin + (g.mins + g.maxs) * 0.5;
            vector tw = p.origin - g.origin;
            tw_z = 0;
            float ahead = (vr_am_cast_org - mid) * normalize(tw);
            float out = vr_am_cast_grip != '0 0 0' ? vlen(vr_am_cast_org - vr_am_cast_grip) : 0;
            VR_AGT_Check(sprintf("death knight spell from his off hand's palm: by %g, %.1f from his shoulder, %.1f ahead of "
                "his middle, %g fireballs", vr_am_cast_by, out, ahead, vr_am_cast_shots),
                vr_am_casts > 0 && vr_am_cast_by == 4 && out > 8 && ahead > 8);
            VR_AKNT_Disarm(g);
        }
        return;
    }
    if(vr_aknt_case == 3)
    {
        if(g.vr_ag_prop || t > 15)
        {
            VR_AGT_Check(sprintf("knight re-arm: took up %g in %.1f s%s", g.vr_ag_wid, t, t > 15 ? " (timed out)" : ""),
                g.vr_ag_wid == WID_SWORD);
            T_Damage(g, p, p, g.health + 10); // (dies, not gibbed)
            vr_aknt_case = 4;
            vr_aknt_t0 = time;
        }
        return;
    }
    if(vr_aknt_case == 4)
    {
        if(g.health > 0 && t < 5)
        {
            T_Damage(g, p, p, g.health + 10); // (its armour, rolled by the skill, took some of the blow)
            vr_aknt_t0 = time;
            return;
        }
        if(t < 0.3)
        {
            // (Plate Blocks All: a plate takes its whole share of any blow, so an armoured knight can outlive it, on seeds
            // that dress it so: "dead (50)". This case is about its death: the blow again until it dies.)
            if(g.health > 0)
            {
                T_Damage(g, p, p, g.health + 10);
            }
            return;
        }
        VR_AGT_Check(sprintf("knight death: its sword fell once (%g lying), dead (%g)", VR_AKNT_Lying(g.origin),
            g.health), VR_AKNT_Lying(g.origin) == 1 && g.health <= 0);
        if(VR_AKNT_Lying(g.origin) != 1)
        {
            entity e = findradius(g.origin, 300);
            while(e)
            {
                VR_AGT_Print(sprintf("  near: %s %s %.0f", e.classname, e.model, vlen(e.origin - g.origin)));
                e = e.chain;
            }
        }
        if(VR_AKNT_Dk())
        {
            // A gun's holder: his cast's cooldown: a fresh one's clock on a random part of it (500 of them: all of it
            // spread over), ready he casts and not again until it has passed (last: its random() draws would move the
            // cases' own rolls).
            float er = enemy_range;
            enemy_range = RANGE_MID;
            float cd = max(1, cvar_hget(cvarh_vr_agdknight_cast_cooldown));
            float k;
            float least = 999999;
            float most = -1;
            float early = 0;
            for(k = 0; k < 500; k++)
            {
                g.vr_ag_cast_ready = 0;
                early = early + VR_AM_GunCasts(g);
                least = min(least, g.vr_ag_cast_ready - time);
                most = max(most, g.vr_ag_cast_ready - time);
            }
            VR_AGT_Check(sprintf("death knight with a gun: a fresh one's first cast %.2f..%.2f s off (cooldown %g), cast at once %g of 500",
                least, most, cd, early), least >= 0 && most <= cd && least < 0.1 * cd && most > 0.9 * cd && early <= 5);
            g.vr_ag_cast_ready = time;
            float now = VR_AM_GunCasts(g);
            float again = VR_AM_GunCasts(g);
            VR_AGT_Check(sprintf("death knight with a gun: ready he casts (%g), then fires the gun (%g), %.2f s to his next",
                now, again, g.vr_ag_cast_ready - time), now && !again && fabs(g.vr_ag_cast_ready - time - cd) < 0.01);
            enemy_range = er;
        }
        VR_AGT_ClearWeapons();
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        setorigin(p, vr_aknt_home);
        cvar_set(VR_AKNT_Flag(), ftos(vr_aknt_flag0));
        vr_aknt_case = 0;
        vr_agt_step = 0;
        vr_agt_grunt = world;
        cvar_set("vr_agrunt_test", "0");
    }
};

// ----------------------------------------------------------------------------
// A map's own knights and death knights (ROUND23.md, "Armed knights by default"; run on a map with them, `god; notarget`):
//   207  the census: `knightmap: <map> ...`, one line, the same frame: every one armed by the rule (not statues, a
//        trigger's copies, MG3's infected, AD's variants, a campaign switched off), each with a sword alone, no pack,
//        taking no gun up. The map's monster count is in the line: the same with the settings off.
//   206  the census; one with a target that is no path killed: counted, its target used (a door moves); one's sword
//        knocked out and gone, a gun lying by it: it never takes it (the same one takes it with
//        vr_agknight_ranged_pickup 1: the wait is long enough); on MG's maps one waiting for its trigger comes armed;
//        a monster_vr_knight and monster_vr_hknight spawned with a shotgun hold it, wear the pack and, disarmed, take a gun
//        up with the setting off (his note of 2026-11-03: .vr_ag_born_ranged), one spawned with its sword takes none; an
//        arena wave's hold their swords; a knight hurt by an ogre turns on it, by another knight doesn't.
//   208  a save from before (`vr_agknight 0`, saved, then loaded with it on): the census with none armed, the kill.
// Ends on `agtest: test N over: P passed, F failed`.
// ----------------------------------------------------------------------------

entity vr_kmt_gun1, vr_kmt_gun2; // the knight and the death knight spawned with a gun
float vr_kmt_step;   // 0 census, 1 the kill, 2 gun holder's made, 3 its gun down, 4 no gun taken, 5 the control, 6 the rest, 7 infighting, 8 a deferred one woken, 9 the gun's turn, 10, 11 spawned gun holders disarmed
float vr_kmt_t0;
float vr_kmt_killed; // killed_monsters before the kill
float vr_kmt_fired;  // its target's use ran
float vr_kmt_state0; // its target's state before
entity vr_kmt_k;     // the knight
entity vr_kmt_h;     // the gun holder it gets its gun from
entity vr_kmt_t;     // its target
entity vr_kmt_keep;  // holds the target's own use

void() VR_KMT_TargetUse =
{
    vr_kmt_fired = vr_kmt_fired + 1;
    self.use = vr_kmt_keep.use;
    self.use();
};

float(entity e) VR_KMT_IsKnight = { return e.classname == "monster_knight" || e.classname == "monster_hell_knight"; };
float(entity e) VR_KMT_Armed = { return e.vr_ag ? TRUE : FALSE; };

float(entity e) VR_KMT_ExpectArmed =
{
    if(!cvar_hget(e.classname == "monster_knight" ? cvarh_vr_agknight : cvarh_vr_agdknight) ||
       e.vr_mg3_infected || !VR_AGR_InScope() || VR_AGR_ADVariant(e))
    {
        return FALSE;
    }
    return VR_OtherFlags() || !(e.spawnflags & (2 | SPAWNFLAG_TRIGGER_SPAWN | SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE));
};

void(float t) VR_KMT_Census =
{
    float kn = 0;
    float dk = 0;
    float armed = 0;
    float kept = 0;
    float deferred = 0;
    float wrong = 0;
    float notsword = 0;
    float packs = 0;
    float guns = 0;
    entity e = nextent(world);
    while(e)
    {
        if(VR_KMT_IsKnight(e) && !e.vr_corpse_placed) // (MG's authored bodies wear the names too: vr_mg_corpses.qc)
        {
            if(e.classname == "monster_knight") { kn = kn + 1; } else { dk = dk + 1; }
            if(e.vr_ag) { armed = armed + 1; } else { kept = kept + 1; }
            if(e.MG_deferred) { deferred = deferred + 1; }
            if(VR_KMT_Armed(e) != (t == 208 ? FALSE : VR_KMT_ExpectArmed(e)))
            {
                wrong = wrong + 1;
                bprint(sprintf("knightmap: wrong: %s at %v health %g dead %g flags %g model %s\n", e.classname, e.origin, e.health,
                    e.deadflag, e.spawnflags, e.model));
            }
            if(e.vr_ag)
            {
                if(e.vr_ag_wid != WID_SWORD || !e.vr_ag_prop || e.vr_ag_prop2) { notsword = notsword + 1; }
                if(e.vr_wp_pack) { packs = packs + 1; }
                if(VR_AM_Takes(e, WID_SHOTGUN)) { guns = guns + 1; }
            }
        }
        e = nextent(e);
    }
    bprint(sprintf("knightmap: %s otherflags %g", mapname, VR_OtherFlags()));
    bprint(sprintf(" monsters %g knights %g dknights %g", total_monsters, kn, dk));
    bprint(sprintf(" armed %g plain %g deferred %g wrong %g\n", armed, kept, deferred, wrong));
    VR_AGT_Check(sprintf("%s: armed by the rule (%g armed, %g plain, %g wrong), swords alone (%g not), no pack (%g), no gun taken (%g)",
        mapname, armed, kept, wrong, notsword, packs, guns), wrong == 0 && notsword == 0 && packs == 0 && guns == 0);
};

// A knight or death knight of the kind's own spawn with `w` (0: its sword), as the toolgun's `spawnarmed`.
entity(float dk, float w, vector at) VR_KMT_SpawnArmed =
{
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.weapon = w;
    if(dk) { monster_vr_hknight(); } else { monster_vr_knight(); }
    entity g = self;
    self = oldself;
    return g;
};

// One of the map's by the rule, with a target that is no path (else world).
entity(float armed) VR_KMT_TargetKnight =
{
    entity e = nextent(world);
    while(e)
    {
        if(VR_KMT_IsKnight(e) && VR_KMT_Armed(e) == armed && e.health > 0 && !e.MG_deferred && e.target != "")
        {
            entity t = find(world, targetname, e.target);
            if(t && t.use && t.classname != "path_corner")
            {
                return e;
            }
        }
        e = nextent(e);
    }
    return world;
};

// One waiting for its trigger unseen (MG's maps), armed: world when none.
entity() VR_KMT_Deferred =
{
    entity e = nextent(world);
    while(e)
    {
        if(VR_KMT_IsKnight(e) && e.vr_ag && e.MG_deferred && e.health > 0)
        {
            return e;
        }
        e = nextent(e);
    }
    return world;
};

// The armed one nearest the player that has its sword.
entity() VR_KMT_Nearest =
{
    entity best = world;
    float d = 99999;
    entity e = nextent(world);
    while(e)
    {
        if(VR_KMT_IsKnight(e) && e.vr_ag && e.health > 0 && !e.MG_deferred && e.vr_ag_prop && vlen(e.origin - vr_agt_player.origin) < d)
        {
            best = e;
            d = vlen(e.origin - vr_agt_player.origin);
        }
        e = nextent(e);
    }
    return best;
};

// The weapon of `w` lying nearest `at` (world: none).
entity(float w, vector at) VR_KMT_Lying =
{
    entity best = world;
    float d = 99999;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(e.weapon == w && vlen(e.origin - at) < d)
        {
            best = e;
            d = vlen(e.origin - at);
        }
        e = find(e, classname, "thrown_weapon");
    }
    return best;
};

// Every weapon lying about but shotguns gone (a knight takes melee ones up whatever the settings: a dead grunt's axe, an
// ogre's chainsaw would fill the hand the gun is tried on).
void() VR_KMT_OnlyGuns =
{
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        entity n = find(e, classname, "thrown_weapon");
        if(e.weapon != WID_SHOTGUN)
        {
            remove(e);
        }
        e = n;
    }
};

// The last checks: the guns of spawned ones, who a knight turns on.
void() VR_KMT_Rest =
{
    entity p = vr_agt_player;
    makevectors([0, p.angles_y, 0]);
    traceline(p.origin, p.origin + v_forward * 220, TRUE, p);
    vector at = trace_endpos - v_forward * 48;
    // (a wearer by the pack's rule; Dawn of the Machine's monsters carry none)
    entity g = VR_KMT_SpawnArmed(FALSE, WID_SHOTGUN, at + v_right * 40);
    VR_AGT_Check(sprintf("a knight spawned with a shotgun holds it (%g), wears the pack (%g), takes guns up (%g) with the setting %g",
        g.vr_ag_wid, g.vr_wp_pack != world, VR_AM_Takes(g, WID_SHOTGUN), cvar_hget(cvarh_vr_agknight_ranged_pickup)),
        g.vr_ag_wid == WID_SHOTGUN && (g.vr_wp_pack != world || MG3_Campaign()) && VR_AM_Takes(g, WID_SHOTGUN));
    vr_kmt_gun1 = g;
    g.health = 2000; // (the map's own may fight them meanwhile: an Arcane Dimensions room's did)
    g = VR_KMT_SpawnArmed(TRUE, WID_SHOTGUN, at - v_right * 40);
    VR_AGT_Check(sprintf("a death knight spawned with a shotgun holds it (%g), wears the pack (%g), takes guns up (%g) with the setting %g",
        g.vr_ag_wid, g.vr_wp_pack != world, VR_AM_Takes(g, WID_SHOTGUN), cvar_hget(cvarh_vr_agknight_ranged_pickup)),
        g.vr_ag_wid == WID_SHOTGUN && (g.vr_wp_pack != world || MG3_Campaign()) && VR_AM_Takes(g, WID_SHOTGUN));
    vr_kmt_gun2 = g;
    g.health = 2000;

    entity oldself = self;
    self = spawn();
    setorigin(self, at + '0 0 512');
    self.classname = VR_ARENA_ClassName(AR_SP_KNIGHT);
    vr_ar_hold_roll = 1;
    VR_ARENA_CallSpawn(AR_SP_KNIGHT);
    vr_ar_hold_roll = 0;
    VR_AGT_Check(sprintf("an arena wave's knight: armed %g, holds %g (the sword)", self.vr_ag, self.vr_ag_wid),
        self.vr_ag && self.vr_ag_wid == WID_SWORD);
    self = spawn();
    setorigin(self, at + '0 96 512');
    self.classname = VR_ARENA_ClassName(AR_SP_HKNIGHT);
    vr_ar_hold_roll = 1;
    VR_ARENA_CallSpawn(AR_SP_HKNIGHT);
    vr_ar_hold_roll = 0;
    VR_AGT_Check(sprintf("an arena wave's death knight: armed %g, holds %g (the sword)", self.vr_ag, self.vr_ag_wid),
        self.vr_ag && self.vr_ag_wid == WID_SWORD);
    self = spawn();
    setorigin(self, at + '0 192 512');
    self.classname = VR_ARENA_ClassName(AR_SP_AKNIGHT);
    self.weapon = WID_SHOTGUN;
    vr_ar_hold_roll = 1;
    VR_ARENA_CallSpawn(AR_SP_AKNIGHT);
    vr_ar_hold_roll = 0;
    VR_AGT_Check(sprintf("a wave's armed knight given a shotgun: holds %g, takes guns up (%g)", self.vr_ag_wid, VR_AM_Takes(self, WID_SHOTGUN)),
        self.vr_ag_wid == WID_SHOTGUN && VR_AM_Takes(self, WID_SHOTGUN));
    self = spawn();
    setorigin(self, at + '0 288 512');
    self.classname = VR_ARENA_ClassName(AR_SP_ADKNIGHT);
    self.weapon = WID_SHOTGUN;
    vr_ar_hold_roll = 1;
    VR_ARENA_CallSpawn(AR_SP_ADKNIGHT);
    vr_ar_hold_roll = 0;
    VR_AGT_Check(sprintf("a wave's armed death knight given a shotgun: holds %g, takes guns up (%g)", self.vr_ag_wid, VR_AM_Takes(self, WID_SHOTGUN)),
        self.vr_ag_wid == WID_SHOTGUN && VR_AM_Takes(self, WID_SHOTGUN));
    self = oldself;
    vr_kmt_k = VR_KMT_SpawnArmed(FALSE, 0, at);
    vr_kmt_h = VR_KMT_SpawnArmed(FALSE, 0, at + v_forward * 40);
    self = spawn();
    setorigin(self, at + v_right * 80);
    self.classname = "monster_ogre";
    vr_agr_hold = vr_agr_hold + 1;
    monster_ogre();
    vr_agr_hold = vr_agr_hold - 1;
    vr_kmt_t = self;
    self = oldself;
};

// (They have started: a monster takes no damage before walkmonster_start_go.)
void() VR_KMT_Infight =
{
    entity k1 = vr_kmt_k;
    entity k2 = vr_kmt_h;
    entity ogre = vr_kmt_t;
    T_Damage(k1, k2, k2, 1);
    VR_AGT_Check(sprintf("infighting: hurt by a knight, a knight stays off it (%g, enemy %s)", k1.health, k1.enemy.classname),
        k1.enemy != k2 && k1.health < 75);
    T_Damage(k1, ogre, ogre, 1);
    VR_AGT_Check(sprintf("infighting: hurt by an ogre, a knight turns on it (enemy %s)", k1.enemy.classname), k1.enemy == ogre);
    VR_AGT_Check(sprintf("a knight spawned with its sword takes no gun up (%g)", VR_AM_Takes(k2, WID_SHOTGUN)), !VR_AM_Takes(k2, WID_SHOTGUN));
    VR_AGT_Remove(k1); // (gone, with what they hold: nothing else to take up below)
    VR_AGT_Remove(k2);
    remove(ogre);
};

// A spawned gun holder awake, you unseen, its gun knocked out: it goes back for it.
void(entity g) VR_KMT_DisarmGun =
{
    VR_AGT_Hunt(g);
    vr_agt_player.flags = vr_agt_player.flags | FL_NOTARGET;
    g.enemy = world;
    makevectors(g.angles);
    VR_AG_Disarm(g, v_right, 120, "test 206");
    VR_KMT_OnlyGuns();
};

void(float t) VR_KMT_Frame =
{
    entity p = vr_agt_player;
    float dt = time - vr_kmt_t0;
    if(vr_kmt_step == 0)
    {
        VR_KMT_Census(t);
        if(t == 207)
        {
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        vr_kmt_k = VR_KMT_TargetKnight(t == 206);
        if(!vr_kmt_k)
        {
            VR_AGT_Print(sprintf("%s: none with a target that is no path: the kill not run", mapname));
            vr_kmt_step = 2;
            vr_kmt_t0 = time;
            return;
        }
        vr_kmt_t = find(world, targetname, vr_kmt_k.target);
        vr_kmt_keep = spawn();
        vr_kmt_keep.use = vr_kmt_t.use;
        vr_kmt_t.use = VR_KMT_TargetUse;
        vr_kmt_fired = 0;
        vr_kmt_state0 = vr_kmt_t.state;
        vr_kmt_killed = killed_monsters;
        vr_kmt_k.health = 1;
        vr_kmt_t0 = time;
        vr_kmt_step = 1;
        return;
    }
    if(vr_kmt_step == 1)
    {
        if(vr_kmt_k.health > 0 && dt < 5)
        {
            T_Damage(vr_kmt_k, p, p, 30); // (its armour may take some)
            return;
        }
        if(!vr_kmt_fired && dt < 5)
        {
            return; // (its target's use, a delay on it)
        }
        VR_AGT_Print(sprintf("%s (armed %g) with target %s killed; its %s, state %g -> %g", vr_kmt_k.classname, vr_kmt_k.vr_ag,
            vr_kmt_t.targetname, vr_kmt_t.classname, vr_kmt_state0, vr_kmt_t.state));
        VR_AGT_Check(sprintf("the kill: counted (%g -> %g), its target used (%g times), a door moved", vr_kmt_killed,
            killed_monsters, vr_kmt_fired), killed_monsters == vr_kmt_killed + 1 && vr_kmt_fired >= 1 &&
            (vr_kmt_t.classname != "func_door" || vr_kmt_t.state != vr_kmt_state0));
        if(t == 208)
        {
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        vr_kmt_step = 2;
        vr_kmt_t0 = time;
        vr_kmt_k = world;
        return;
    }
    if(vr_kmt_step == 2) // (and 8: a deferred one woken first)
    {
        if(t == 208)
        {
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        vr_kmt_k = VR_KMT_Deferred();
        if(vr_kmt_k)
        {
            entity oldself = self;
            self = vr_kmt_k;
            activator = p;
            self.use();
            self = oldself;
            vr_kmt_t0 = time;
            vr_kmt_step = 8;
            return;
        }
        vr_kmt_step = 9;
        return;
    }
    if(vr_kmt_step == 8)
    {
        if(dt < 1 && !(vr_kmt_k.flags & FL_MONSTER && vr_kmt_k.takedamage != DAMAGE_NO))
        {
            return;
        }
        VR_AGT_Check(sprintf("woken by its trigger: %s, model %s, its sword's model %s, takes damage %g", vr_kmt_k.classname, vr_kmt_k.model,
            vr_kmt_k.vr_ag_prop ? vr_kmt_k.vr_ag_prop.model : "none", vr_kmt_k.takedamage),
            !vr_kmt_k.MG_deferred && substring(vr_kmt_k.model, 0, 8) == "progs/vr" && vr_kmt_k.vr_ag_prop && vr_kmt_k.vr_ag_prop.model != "" &&
            vr_kmt_k.takedamage != DAMAGE_NO);
        vr_kmt_step = 9;
        return;
    }
    if(vr_kmt_step == 9)
    {
        // A gun to lie by the nearest one: a holder of one, made beside it and killed.
        vr_kmt_k = VR_KMT_Nearest();
        if(!vr_kmt_k)
        {
            VR_AGT_Check("a knight to try: none armed on this map", FALSE);
            vr_kmt_step = 6;
            return;
        }
        makevectors(vr_kmt_k.angles);
        traceline(vr_kmt_k.origin, vr_kmt_k.origin + v_forward * 64, TRUE, vr_kmt_k);
        vr_kmt_h = VR_KMT_SpawnArmed(FALSE, WID_SHOTGUN, trace_endpos - v_forward * 24);
        vr_kmt_t0 = time;
        vr_kmt_step = 3;
        return;
    }
    if(vr_kmt_step == 3)
    {
        if(dt < 0.3)
        {
            return; // (its prop placed)
        }
        if(vr_kmt_h.health > 0)
        {
            T_Damage(vr_kmt_h, p, p, 5000);
        }
        if(!VR_KMT_Lying(WID_SHOTGUN, vr_kmt_k.origin) && dt < 5)
        {
            return;
        }
        // Awake and hunting, you unseen, its sword knocked out and gone: the gun the only weapon by it.
        VR_AGT_Hunt(vr_kmt_k);
        p.flags = p.flags | FL_NOTARGET;
        vr_kmt_k.enemy = world;
        makevectors(vr_kmt_k.angles);
        VR_AG_Disarm(vr_kmt_k, v_right, 120, "test 206");
        VR_KMT_OnlyGuns();
        vr_kmt_t0 = time;
        vr_kmt_step = 4;
        return;
    }
    if(vr_kmt_step == 4)
    {
        if(dt < 8 && !vr_kmt_k.vr_ag_prop)
        {
            return;
        }
        entity lies = VR_KMT_Lying(WID_SHOTGUN, vr_kmt_k.origin);
        VR_AGT_Check(sprintf("a map's knight, disarmed, leaves the gun %.0f away for %.1f s (%g pickups, holds %s)",
            lies ? vlen(lies.origin - vr_kmt_k.origin) : -1, dt, vr_kmt_k.vr_ag_pickups,
            vr_kmt_k.vr_ag_prop ? vr_kmt_k.vr_ag_prop.netname : "nothing"),
            lies != world && !vr_kmt_k.vr_ag_prop && vr_kmt_k.vr_ag_pickups == 0);
        cvar_set("vr_agknight_ranged_pickup", "1");
        vr_kmt_t0 = time;
        vr_kmt_step = 5;
        return;
    }
    if(vr_kmt_step == 5)
    {
        if((!vr_kmt_k.vr_ag_prop || vr_kmt_k.vr_ag_pickups < 1) && dt < 20)
        {
            return;
        }
        VR_AGT_Check(sprintf("control: with vr_agknight_ranged_pickup 1 it takes it in %.1f s (%g pickups, holds %s)", dt,
            vr_kmt_k.vr_ag_pickups, vr_kmt_k.vr_ag_prop ? vr_kmt_k.vr_ag_prop.netname : "nothing"),
            vr_kmt_k.vr_ag_pickups >= 1 && vr_kmt_k.vr_ag_wid == WID_SHOTGUN);
        cvar_set("vr_agknight_ranged_pickup", "0");
        vr_kmt_step = 6;
        return;
    }
    if(vr_kmt_step == 6)
    {
        VR_KMT_Rest();
        vr_kmt_t0 = time;
        vr_kmt_step = 7;
        return;
    }
    if(vr_kmt_step == 7 && dt < 3 && !((vr_kmt_k.flags & FL_MONSTER) && (vr_kmt_k.takedamage != DAMAGE_NO) && (vr_kmt_t.flags & FL_MONSTER) &&
        (vr_kmt_gun1.flags & FL_MONSTER) && (vr_kmt_gun2.flags & FL_MONSTER)))
    {
        return;
    }
    if(vr_kmt_step == 7)
    {
        VR_KMT_Infight();
        VR_KMT_DisarmGun(vr_kmt_gun1);
        vr_kmt_t0 = time;
        vr_kmt_step = 10;
        return;
    }
    if(vr_kmt_step == 10)
    {
        if((!vr_kmt_gun1.vr_ag_prop || vr_kmt_gun1.vr_ag_pickups < 1) && dt < 20)
        {
            return;
        }
        VR_AGT_Check(sprintf("the setting %g: a knight spawned with a shotgun, disarmed, takes it up in %.1f s (%g pickups, holds %s)",
            cvar_hget(cvarh_vr_agknight_ranged_pickup), dt, vr_kmt_gun1.vr_ag_pickups, vr_kmt_gun1.vr_ag_prop ? vr_kmt_gun1.vr_ag_prop.netname : "nothing"),
            vr_kmt_gun1.vr_ag_pickups >= 1 && vr_kmt_gun1.vr_ag_wid == WID_SHOTGUN);
        T_Damage(vr_kmt_gun1, p, p, 9000);
        VR_KMT_DisarmGun(vr_kmt_gun2);
        vr_kmt_t0 = time;
        vr_kmt_step = 11;
        return;
    }
    if((!vr_kmt_gun2.vr_ag_prop || vr_kmt_gun2.vr_ag_pickups < 1) && dt < 20)
    {
        return;
    }
    entity lying = VR_KMT_Lying(WID_SHOTGUN, vr_kmt_gun2.origin);
    VR_AGT_Print(sprintf("death knight: a shotgun %.0f away (%.0f up), it reaches it %g, seek %s, health %g, frame %g", lying ? vlen(lying.origin - vr_kmt_gun2.origin) : -1,
        lying ? lying.origin_z - vr_kmt_gun2.origin_z : 0, lying ? VR_AG_Reachable(vr_kmt_gun2, lying) : 0, vr_kmt_gun2.vr_ag_seek.classname, vr_kmt_gun2.health, vr_kmt_gun2.frame));
    VR_AGT_Check(sprintf("the setting %g: a death knight spawned with a shotgun, disarmed, takes a gun up in %.1f s (%g pickups, holds %s)",
        cvar_hget(cvarh_vr_agknight_ranged_pickup), dt, vr_kmt_gun2.vr_ag_pickups, vr_kmt_gun2.vr_ag_prop ? vr_kmt_gun2.vr_ag_prop.netname : "nothing"),
        vr_kmt_gun2.vr_ag_pickups >= 1 && vr_kmt_gun2.vr_ag_wid == WID_SHOTGUN);
    cvar_set("vr_agrunt_test", "0");
};
#endif
