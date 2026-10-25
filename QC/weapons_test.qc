#ifdef QVR_TESTS
// weapons's test harness (moved out of weapons.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// QVR: precise hit detection's test (impulse 238; ROUND21 "Precise hit detection (models, not boxes)"): at the monster
// (or corpse) spawned last, level from the player's side, each kind of test with Quake's boxes and then with its model:
// rays through its box's corner columns (2 units in from its edges: the empty part), at its middle, a ring of 24 at its
// head and 24 at its chest (their positional regions), and the same segments as melee blows (hitmodel_segment, 3 units
// thick) and thrown things (tracebox, vr_throw_hitbox 6), and a blade's ring at the head (positional melee's region of the
// point met). "on" counts what met the target; "behind" what went on to
// the world. Nothing is damaged.
float(entity xTarget, vector xFrom, vector xTo, float xFlags) VR_Test_HitRay =
{
    traceline(xFrom, xTo, xFlags, self);
    return trace_ent == xTarget;
};

void(entity t, float xPrecise) VR_Test_HitsMode =
{
    float saved = cvar("vr_hit_precise");
    cvar_set("vr_hit_precise", ftos(xPrecise));
    vector lo = t.origin + t.mins;
    vector hi = t.origin + t.maxs;
    vector c = (lo + hi) * 0.5;
    vector dir = c - self.origin;
    dir_z = 0;
    dir = normalize(dir);
    vector side;
    side_x = -dir_y;
    side_y = dir_x;
    side_z = 0;
    float halfw = (t.maxs_x - t.mins_x) * 0.5 - 2;
    float halfh = (t.maxs_z - t.mins_z) * 0.5 - 2;
    float gunFlags = MOVE_HITGIBS | MOVE_HITMODEL;
    float onC = 0, behindC = 0, onB = 0, meleeC = 0, meleeB = 0, thrownC = 0, thrownB = 0, grappleC = 0, grappleB = 0;
    float k;
    vector at;
    for(k = 0; k < 6; ++k)
    {
        float sx = 1, sz = 1;
        if(k & 1) { sx = -1; }
        if(k >= 4) { sz = 0; } else if(k >= 2) { sz = -0.5; } // (low: beside the legs, above the floor)
        at = c + side * halfw * sx + '0 0 1' * halfh * sz;
        if(VR_Test_HitRay(t, at - dir * 64, at + dir * 128, gunFlags)) { onC++; }
        else if(trace_fraction < 1 && trace_ent == world) { behindC++; }
        if(VR_Test_HitRay(t, at - dir * 64, at + dir * 128, MOVE_HITMODEL_GRAPPLE)) { grappleC++; }
        float mt;
        if(xPrecise && hitmodel_target(t)) { mt = hitmodel_segment(t, at - dir * 24, at + dir * 24, 3, HITCLASS_MELEE); }
        else { mt = VR_SegmentBox(at - dir * 24, at + dir * 24, lo - '3 3 3', hi + '3 3 3'); }
        if(mt >= 0) { meleeC++; }
        tracebox(at - dir * 64, '-6 -6 -6', '6 6 6', at + dir * 128, MOVE_HITMODEL_THROWN, self);
        if(trace_ent == t) { thrownC++; }
    }
    // Its middle (chest height: a third of the way down from the top) and its head.
    float heads = 0, headOn = 0, chestOn = 0, chestHead = 0, chestLimb = 0, chestLegs = 0, meleeHeadOn = 0, meleeHeads = 0;
    vector chest = c + '0 0 1' * halfh * 0.35;
    if(VR_Test_HitRay(t, chest - dir * 64, chest + dir * 128, gunFlags)) { onB++; }
    dprint(sprintf("hittest %s at %v size %v %v, solid %d;", t.classname, t.origin, t.mins, t.maxs, t.solid));
    dprint(sprintf(" the middle ray from %v met %s (at %v) at %v", chest - dir * 64, trace_ent.classname, trace_ent.origin, trace_endpos));
    dprint(sprintf(" (%f, startsolid %d, its solid %d)", trace_fraction, trace_startsolid, trace_ent.solid));
    dprint("\n");
    if(VR_Test_HitRay(t, chest - dir * 64, chest + dir * 128, MOVE_HITMODEL_GRAPPLE)) { grappleB++; }
    float mb;
    if(xPrecise && hitmodel_target(t)) { mb = hitmodel_segment(t, chest - dir * 24, chest + dir * 24, 3, HITCLASS_MELEE); }
    else { mb = VR_SegmentBox(chest - dir * 24, chest + dir * 24, lo - '3 3 3', hi + '3 3 3'); }
    if(mb >= 0) { meleeB++; }
    tracebox(chest - dir * 64, '-6 -6 -6', '6 6 6', chest + dir * 128, MOVE_HITMODEL_THROWN, self);
    if(trace_ent == t) { thrownB++; }
    float hasHead = PositionalHead(t);
    vector head = t.origin + '0 0 1' * xHeadUp;
    float yaw = t.angles_y * (3.14159265 / 180);
    head_x = head_x + cos(yaw) * xHeadForward;
    head_y = head_y + sin(yaw) * xHeadForward;
    head = head + VR_HeadSide('1 0 0' * cos(yaw) + '0 1 0' * sin(yaw));
    for(k = 0; k < 24; ++k)
    {
        float a = k * (3.14159265 * 2 / 24);
        vector r;
        r_x = cos(a);
        r_y = sin(a);
        r_z = 0;
        if(hasHead && VR_Test_HitRay(t, head - r * 64, head + r * 128, gunFlags))
        {
            headOn++;
            if(PositionalRegion(t, trace_endpos, r) == 1) { heads++; }
            if(xPrecise && cvar("developer") > 1)
            {
                dprint(sprintf("hittest head ray %d: the rest point %f from the head's middle (radius %f)\n", k,
                    vlen(hitmodel_rest(t, trace_endpos) - head), xHeadRadius));
            }
        }
        // A blade's blow at the head from the same side (positional melee: the point it met, PositionalRegion).
        if(hasHead && xPrecise && hitmodel_target(t))
        {
            float mt = hitmodel_segment(t, head - r * 24, head, 3, HITCLASS_MELEE);
            if(mt > 0)
            {
                meleeHeadOn++;
                if(PositionalRegion(t, head - r * 24 + r * 24 * mt, r) == 1) { meleeHeads++; }
            }
        }
        if(VR_Test_HitRay(t, chest - r * 64, chest + r * 128, gunFlags))
        {
            chestOn++;
            float reg = PositionalRegion(t, trace_endpos, r);
            if(reg == 1) { chestHead++; } else if(reg == 2) { chestLimb++; } else if(reg == 3) { chestLegs++; }
        }
    }
    string mode = "boxes";
    if(xPrecise) { mode = "model"; }
    dprint(sprintf("hittest %s %s: corner rays %d/6 on it (%d on to the world), middle %d/1;", t.classname, mode, onC, behindC, onB));
    dprint(sprintf(" grapple corners %d/6, middle %d/1; melee corners %d/6, middle %d/1;", grappleC, grappleB, meleeC, meleeB));
    dprint(sprintf(" thrown corners %d/6, middle %d/1; head ring %d/24 on it (%d headshots);", thrownC, thrownB, headOn, heads));
    dprint(sprintf(" chest ring %d/24 on it (%d head, %d limb, %d legs);", chestOn, chestHead, chestLimb, chestLegs));
    dprint(sprintf(" melee head ring %d/24 on it (%d head)\n", meleeHeadOn, meleeHeads));
    cvar_set("vr_hit_precise", ftos(saved));
};

void() VR_Test_Hits =
{
    // The monster (or corpse) spawned last (impulse 241).
    entity t = world;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e != self)
        {
            t = e;
        }
        e = nextent(e);
    }
    if(t == world)
    {
        dprint("hittest: no monster\n");
        return;
    }
    VR_Test_HitsMode(t, 0);
    VR_Test_HitsMode(t, 1);
    // Then removed, with anything else there (the next one is tested where it stood).
    vector at = t.origin;
    e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e != self && vlen(e.origin - at) < 64)
        {
            remove(e);
        }
        e = nextent(e);
    }
};

void() VR_Test_MissileTouch =
{
    if(other == vr_test_target) { vr_test_on++; }
    else if(other == world) { vr_test_world++; }
    else { vr_test_other++; }
    if(self.classname == "spike") { spike_touch(); }
    else { T_MissileTouch(); }
};

void(float xRockets) VR_Test_Missiles =
{
    entity t = world;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e != self) { t = e; }
        e = nextent(e);
    }
    if(t == world) { dprint("hittest: no monster\n"); return; }
    if(t != vr_test_target)
    {
        vr_test_target = t;
        vr_test_home = t.origin;
        vr_test_shot = 0;
    }
    setorigin(t, vr_test_home);
    t.velocity = '0 0 0';
    t.takedamage = DAMAGE_NO; // (no pain, no shove: it stands as it was)
    vr_test_on = vr_test_world = vr_test_other = 0;
    vector lo = t.origin + t.mins;
    vector hi = t.origin + t.maxs;
    vector c = (lo + hi) * 0.5;
    vector dir = c - self.origin;
    dir_z = 0;
    dir = normalize(dir);
    vector side;
    side_x = -dir_y;
    side_y = dir_x;
    side_z = 0;
    float halfw = (t.maxs_x - t.mins_x) * 0.5 - 2;
    float halfh = (t.maxs_z - t.mins_z) * 0.5 - 2;
    float k = vr_test_shot;
    vr_test_shot = (vr_test_shot + 1) % 7;
    float sx = 1, sz = 1;
    if(k & 1) { sx = -1; }
    if(k >= 4) { sz = 0; } else if(k >= 2) { sz = -0.5; }
    vector at = c + side * halfw * sx + '0 0 1' * halfh * sz;
    if(k == 6) { at = c + '0 0 1' * halfh * 0.35; } // the middle (chest height)
    entity m = launch_spike(at - dir * 64, dir); // (in front of the player's hands, which it would meet)
    if(xRockets == 2)
    {
        m.vr_hitclass = HITCLASS_GRAPPLE; // the grappling hook's class (its tolerance): a hook's hit test
    }
    else if(xRockets)
    {
        m.classname = "missile";
        setmodel(m, "progs/missile.mdl");
        setsize(m, '0 0 0', '0 0 0');
        m.velocity = dir * 1000;
    }
    m.touch = VR_Test_MissileTouch;
    string kind = "nail";
    if(xRockets == 2) { kind = "hook"; } else if(xRockets) { kind = "rocket"; }
    dprint(sprintf("hittest %s: %s %d (precise %d):", t.classname, kind, k, cvar("vr_hit_precise")));
};

void() VR_Test_MissilesReport =
{
    string what = "nothing";
    if(vr_test_on) { what = "it"; } else if(vr_test_world) { what = "the world"; } else if(vr_test_other) { what = "something else"; }
    dprint(sprintf(" touched %s\n", what));
};

// vr_physics_fire (vr_box3d.cpp): a missile or a shot of yours from your eyes at the world point xAt, flying straight
// at it (shape tests: missiles and shots against a turned solid prop; vr_debug_missiles prints where a missile hits,
// vr_debug_shots what a pellet hit). xKind: 0 a rocket, 1 a nail, 2 a grenade, 3 a super nail, 4 an enforcer's laser,
// 10 a shotgun pellet (the burst rifle's and the lightning's trace: the same); 5..9, 11..16 the other games' projectiles
// (VR_ProjTest_Launch, vr_projhit_test.qc: AD's crossbow bolt, plasma, nails, lasers, Hipnotic's laser, Rogue's...).
// The missile, or world for a shot.
entity(float xKind, vector from, vector dir) VR_Test_FireFrom;
entity(float xKind, vector xAt) VR_Test_Fire =
{
    vector from = self.origin + self.view_ofs;
    vector dir = normalize(xAt - from);
    return VR_Test_FireFrom(xKind, from + dir * 24, dir); // (ahead of your hands)
};

// The same from `from` along `dir` (vr_projhit_test.qc: at a body from where its part is in clear view).
entity(float xKind, vector from, vector dir) VR_Test_FireFrom =
{
    if(xKind == 10)
    {
        traceline(from, from + dir * 2048, MOVE_HITGIBS | MOVE_HITMODEL | MOVE_PORTALS, self); // (a pellet's: FireBulletsImpl)
        string what = "nothing";
        if(trace_ent != world) { what = trace_ent.classname; } else if(trace_fraction < 1) { what = "the world"; }
        dprint(sprintf("test fire: shot from %.3f %.3f %.3f dir %.5f %.5f %.5f ", from_x, from_y, from_z, dir_x, dir_y, dir_z));
        if(portal_crossings() > 0)
        {
            dprint(sprintf("through %d teleporter(s) (in at %v, out at %v, on along %v) ", portal_crossings(), portal_entry(0),
                portal_exit(portal_crossings() - 1), portal_turn(dir)));
        }
        dprint(sprintf("hit %s at %.3f %.3f %.3f\n", what, trace_endpos_x, trace_endpos_y, trace_endpos_z));
        vr_tracer_hand = -1;
        vr_tracer_from = from;
        FireBulletsImpl(1, dir, '0 0 0', from, 2048, FALSE); // (and the shot itself)
        return world;
    }
    entity m;
    if(xKind == 2)
    {
        m = spawn();
        m.owner = self;
        m.movetype = MOVETYPE_BOUNCE;
        m.solid = SOLID_BBOX;
        m.classname = "grenade";
        m.touch = GrenadeTouch;
        m.nextthink = time + 2.5;
        m.think = GrenadeExplode;
        setmodel(m, "progs/grenade.mdl");
        setsize(m, '0 0 0', '0 0 0');
        setorigin(m, from);
        m.velocity = dir * 600;
        VR_Grenade_Make(m, GrenadeExplode, 2.5, TRUE); // (as the launcher's: W_FireGrenade)
    }
    else if(xKind == 4)
    {
        m = LaunchLaser(from, dir);
    }
    else if(xKind >= 5)
    {
        m = VR_ProjTest_Launch(xKind, from, dir); // (the other games' projectiles: vr_projhit_test.qc)
        if(m == world)
        {
            return world;
        }
    }
    else
    {
        m = launch_spike(from, dir);
        if(xKind == 0)
        {
            m.classname = "missile";
            m.touch = T_MissileTouch;
            m.vr_shot_push_kind = 0;
            setmodel(m, "progs/missile.mdl");
            m.velocity = dir * 1000;
        }
        else if(xKind == 3)
        {
            m.touch = superspike_touch;
            m.vr_shot_push_kind = QVR_SHOTPUSH_SUPERNAIL;
            setmodel(m, "progs/s_spike.mdl");
        }
        setsize(m, '0 0 0', '0 0 0');
    }
    m.angles = vectoangles(m.velocity);
    return m;
};

// impulse 210: shoots the nearest grenade shots set off (VR_GrenShot_Is) from your eyes (VR_Test_Fire, the kind
// vr_test_grenade_shot says) at its middle.
void() VR_GrenShot_TestShoot =
{
    entity best = world;
    float bestDist = 4000;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(VR_GrenShot_Is(e) && e.takedamage && vlen(e.origin - self.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
    }
    if(best == world)
    {
        dprint("test: no grenade to shoot\n");
        return;
    }
    vector at = best.origin + (best.mins + best.maxs) * 0.5;
    float kind = cvar("vr_test_grenade_shot");
    dprint(sprintf("test: shooting %s (%s's) at %v, %.0f units away, %.0f u/s, shot box %.1f (kind %g)\n", best.classname,
        best.owner.classname, at, vlen(at - (self.origin + self.view_ofs)), vlen(best.velocity), best.vr_shot_radius, kind));
    VR_Test_Fire(kind, at);
};

// QVR: impulse 252 with vr_test_held_pick 9-13 (tests): the nearest rock, a hand grenade from the pouch, the nearest box
// of shells, the nearest brick, the nearest crate or barrel (13) put into the empty off hand (the two-handed re-grip tests: ROUND21.md, "Every prop in
// both hands"); the main hand with vr_test_held_hand 1.
void(float xPick, float xHand) VR_Test_HeldProp =
{
    if(xPick == 10)
    {
        if(self.ammo_rockets < 1)
        {
            self.ammo_rockets = 1;
        }
        VR_HandGrenade_Take(xHand);
        dprint(xHand == cVR_MainHand ? "test: a hand grenade in the main hand\n" : "test: a hand grenade in the off hand\n");
        return;
    }
    if(xPick == 13)
    {
        // The nearest crate or barrel (prop_2h_test.sh's REPEAT: thrown, taken again), between the hands.
        entity crateNear = world;
        float crateDist = 512;
        for(entity crateE = nextent(world); crateE; crateE = nextent(crateE))
        {
            if(VR_Crate_Is(crateE) && crateE.carry_player == world && vlen(crateE.origin - self.origin) < crateDist)
            {
                crateNear = crateE;
                crateDist = vlen(crateE.origin - self.origin);
            }
        }
        if(crateNear == world)
        {
            dprint("test: no crate or barrel\n");
            return;
        }
        setorigin(crateNear, (VRGetHandPos(cVR_OffHand) + VRGetHandPos(cVR_MainHand)) * 0.5 - (crateNear.mins + crateNear.maxs) * 0.5);
        VR_Carry_Start(self, cVR_OffHand, crateNear);
        dprint(sprintf("test: %s (health %.0f) in the off hand\n", crateNear.model, crateNear.health));
        return;
    }
    string heldClass = xPick == 9 ? "vr_rock" : (xPick == 11 ? "item_shells" : "vr_brick");
    entity heldNear = world;
    float heldDist = 100000;
    for(entity heldE = find(world, classname, heldClass); heldE; heldE = find(heldE, classname, heldClass))
    {
        if(heldE.carry_player == world && heldE.model != string_null && vlen(heldE.origin - self.origin) < heldDist)
        {
            heldNear = heldE;
            heldDist = vlen(heldE.origin - self.origin);
        }
    }
    if(heldNear == world)
    {
        dprint(sprintf("test: no %s\n", heldClass));
        return;
    }
    setorigin(heldNear, VRGetHandPos(xHand));
    VR_Carry_Start(self, xHand, heldNear);
    dprint(sprintf("test: %s (%s) in the %s hand\n", heldNear.model, heldClass, xHand == cVR_MainHand ? "main" : "off"));
};

// impulse 211: a hand grenade from the pouch into the empty off hand (a rocket for it if you have none), let go of
// unarmed (a dud: VR_Carry_Release, VR_Grenade_Released), and put on the floor vr_test_grenade_dist ahead.
void() VR_GrenShot_TestDud =
{
    if(!VRIsHandEmpty(self, cVR_OffHand) || VR_Carry_Held(self, cVR_OffHand) != world)
    {
        dprint("test: the off hand isn't empty\n");
        return;
    }
    if(self.ammo_rockets < 1)
    {
        self.ammo_rockets = 1;
    }
    VR_HandGrenade_Take(cVR_OffHand);
    entity g = VR_Carry_Held(self, cVR_OffHand);
    if(g == world || !g.vr_hgren)
    {
        dprint("test: no hand grenade taken\n");
        return;
    }
    VR_Carry_Release(cVR_OffHand, g);
    makevectors('0 1 0' * self.v_viewangle_y);
    vector ahead = self.origin + v_forward * cvar("vr_test_grenade_dist");
    traceline(ahead + '0 0 16', ahead - '0 0 256', TRUE, self);
    // (vr_test_grenade_height: in the air, it falls)
    setorigin(g, trace_endpos - g.mins + '0 0 1' * (1 + max(0, cvar("vr_test_grenade_height"))));
    g.velocity = '0 0 0';
    g.vr_spin = '0 0 0';
    g.vr_gren_lastvel = '0 0 0';
    g.vr_gren_lastorg = g.origin;
    if(!cvar("vr_test_grenade_yours"))
    {
        g.owner = world; // (nobody's, as an enemy's lying about: your blows meet it, VR_Blow_Strikable)
    }
    dprint(sprintf("test: a dud hand grenade on the floor at %v, %.0f units ahead (armed %g, fuse %g)\n", g.origin,
        vlen(g.origin - self.origin), g.vr_hgren_armed, g.vr_gren_fuse));
};

void() VR_Test_CrateCover =
{
    entity player = self;
    float yaw = player.v_viewangle_y;
    makevectors('0 1 0' * yaw);
    vector fwd = v_forward;
    vector right = v_right;
    // The large crate, its face square to you.
    vector at = player.origin + fwd * 56;
    traceline(at + '0 0 8', at - '0 0 256', TRUE, player); // (on the floor)
    entity cover = VR_Crate_TestSpawn(trace_endpos, 2, yaw, FALSE);
    // The small one, at your right, to pick up and hold in front of you.
    at = player.origin + right * 48 + fwd * 8;
    traceline(at + '0 0 8', at - '0 0 256', TRUE, player);
    VR_Crate_TestSpawn(trace_endpos, 1, yaw + 15, FALSE);
    // The grunt 320 units ahead (short of a wall there), facing you.
    vector from = player.origin + '0 0 16';
    traceline(from, from + fwd * 320, TRUE, player);
    at = trace_endpos - fwd * 32 - '0 0 16';
    entity dispenser = spawn();
    dispenser.weapon = 0;
    setorigin(dispenser, at);
    self = dispenser;
    activator = player;
    func_enemy_dispenser_use();
    entity grunt = self;
    self = player;
    remove(dispenser);
    if(grunt == dispenser)
    {
        vr_test_cover_grunt = world;
        return;
    }
    grunt.angles_y = yaw + 180;
    vr_test_cover_grunt = grunt;
    dprint(sprintf("cover test: large crate at %v (box %v .. %v), grunt at %v, %.0f units away\n", cover.origin,
        cover.mins, cover.maxs, grunt.origin, vlen(grunt.origin - player.origin)));
    centerprint(player, "Crouch behind the large crate:\nthe grunt shouldn't see you.\nStand up, or hold up the small one:\nit should.");
};

// QVR test: whether the cover test's grunt sees you now, and whether it woke (impulse 224).
void() VR_Test_CrateCoverReport =
{
    entity player = self;
    entity grunt = vr_test_cover_grunt;
    if(grunt == world || grunt.classname != "monster_army" || grunt.health <= 0)
    {
        sprint(player, "cover test: no grunt (Crate Cover first)\n");
        return;
    }
    self = grunt;
    float sees = visible(player);
    self = player;
    vector head = VRGetHeadPos(player);
    string msg = sprintf("cover test: the grunt %s you (your head %.0f units up), %s\n", sees ? "SEES" : "can't see",
        head_z - (player.origin_z + player.mins_z), grunt.enemy == player ? "awake, after you" : "asleep");
    sprint(player, msg);
    centerprint(player, msg);
};

// The yaw a test spawner's monster is given ahead of player `p` (vr_test_spawn_facing; Debug > Tests > Ahead of You:
// Facing): 1 facing `p`, 2 facing away (trailer shots: it looks where you look), else -1 (0: the spawner's own way, as
// before; 3: the spawn's own angle), 4 its left side to `p` (facing `p`'s left), 5 its right side to `p`.
float(entity p) VR_TestSpawn_FacingYaw =
{
    float f = cvar("vr_test_spawn_facing");
    if(f == 1)
    {
        return anglemod(p.v_viewangle_y + 180);
    }
    if(f == 2)
    {
        return anglemod(p.v_viewangle_y);
    }
    if(f == 4 || f == 5)
    {
        return anglemod(p.v_viewangle_y + (f == 4 ? 90 : -90));
    }
    return -1;
};

// Tests (impulse 232; Debug > Tests > Flung Props): the loose prop nearest you (a rigid body not carried) sent at
// vr_test_fling_speed m/s at the nearest monster (vr_test_fling_at 1: at you), from 40 units off the side of its box
// facing you, at its middle's height, not thrown by anyone (VR_Prop_Flung). vr_test_fling_away 1: from inside the edge
// of its box, away from it (a batted prop leaving you, still in your box).
void() VR_Test_Fling =
{
    entity p = self;
    entity target = p;
    float best;
    entity e;
    if(!cvar("vr_test_fling_at"))
    {
        target = world;
        best = 100000;
        for(e = nextent(world); e != world; e = nextent(e))
        {
            if(e.flags & FL_MONSTER && e.health > 0 && vlen(e.origin - p.origin) < best)
            {
                best = vlen(e.origin - p.origin);
                target = e;
            }
        }
    }
    entity prop = world;
    best = 512;
    for(e = nextent(world); e != world; e = nextent(e))
    {
        if(e.vr_rigid && e.carry_player == world && (e.touch == forcegrabbable_touch || e.vr_impact) &&
           vlen(e.origin - p.origin) < best)
        {
            best = vlen(e.origin - p.origin);
            prop = e;
        }
    }
    if(target == world || prop == world)
    {
        dprint("fling test: no monster or no prop\n");
        return;
    }
    vector mid = 0.5 * (target.absmin + target.absmax);
    vector dir;
    if(target == p)
    {
        makevectors('0 1 0' * p.v_viewangle_y);
        dir = -1 * v_forward; // (towards you from ahead)
    }
    else
    {
        dir = mid - p.origin;
        dir_z = 0;
        dir = normalize(dir);
    }
    float half = 0.5 * max(target.size_x, target.size_y);
    float speed = cvar("vr_test_fling_speed") * VRMetersToUnits();
    vector at = mid - dir * (half + 40);
    vector vel = dir * speed;
    if(cvar("vr_test_fling_away"))
    {
        at = mid - dir * (half - 4);
        vel = -1 * vel;
    }
    prop.owner = world;
    prop.throwhit = 0;
    prop.carry_letgo = world;
    prop.vr_fling_hit = 0;
    prop.fg_state = 0;
    prop.flags = prop.flags - (prop.flags & FL_ONGROUND);
    setorigin(prop, at - 0.5 * (prop.mins + prop.maxs));
    prop.velocity = vel;
    dprint(sprintf("fling test: %s (%.1f kg) at %s, %.1f m/s (%.0f u/s)%s; its health %.0f\n", prop.classname,
        VR_Thrown_Mass(prop), target.classname, cvar("vr_test_fling_speed"), speed,
        cvar("vr_test_fling_away") ? ", away from it" : "", target.health));
};

// Tests (impulse 204; Debug > Tests > Two-Handed Throw): a two-handed throw's sparing of blows (VR_Throw_TwoHanded): a
// gib ahead of you, as if just thrown with both hands, and the hand that let go first a moment before: whether each
// hand may strike and the gib can be struck, now and 0.5 s later ("2h test:" lines).
void() VR_Throw_Test2H_Print =
{
    entity g = self;
    entity p = g.vr_2h_thrower;
    self = p;
    dprint(sprintf("2h test: %.2f s after the throw: main hand strikes %g, off hand %g, the gib strikable %g\n",
        time - g.vr_2h_thrown, VR_Throw_HandMayStrike(p, cVR_MainHand), VR_Throw_HandMayStrike(p, cVR_OffHand),
        VR_Blow_Strikable(g)));
    self = g;
    if(g.cnt)
    {
        remove(g);
        return;
    }
    g.cnt = 1;
    g.nextthink = time + 0.5;
};

void() VR_Throw_Test2H =
{
    makevectors(self.v_angle);
    entity g = spawn();
    g.classname = "test_gib";
    setmodel(g, "progs/gib1.mdl");
    setsize(g, '0 0 0', '0 0 0');
    g.movetype = MOVETYPE_NONE;
    g.solid = SOLID_NOT_BUT_TOUCHABLE;
    g.takedamage = DAMAGE_YES;
    g.health = 10;
    g.vr_gib = 1;
    setorigin(g, self.origin + v_forward * 40 + '0 0 16');
    dprint(sprintf("2h test: before: main hand strikes %g, off hand %g, the gib strikable %g\n",
        VR_Throw_HandMayStrike(self, cVR_MainHand), VR_Throw_HandMayStrike(self, cVR_OffHand), VR_Blow_Strikable(g)));
    self.vr_2h_letgo_hand = cVR_OffHand + 1;
    self.vr_2h_letgo_time = time;
    dprint(sprintf("2h test: the off hand let go first: main hand strikes %g, off hand %g\n",
        VR_Throw_HandMayStrike(self, cVR_MainHand), VR_Throw_HandMayStrike(self, cVR_OffHand)));
    VR_Throw_TwoHanded(self, g);
    g.think = VR_Throw_Test2H_Print;
    g.nextthink = time + 0.01;
};

// Tests (impulse 222; Debug > Tests > Flung Props): the loose prop nearest you (a rigid body not carried, within 512
// units) thrown straight up as yours (VR_Carry_Throw) from over your head (vr_test_throw_up_body: at your chest) at
// vr_test_throw_up_speed m/s (0: let go of gently, it never hurts you): it falls back
// on you, past vr_throw_self_grace (vrfiringrange_2026-10-01_16-41-40: a backpack thrown high).
void() VR_Throw_TestUp =
{
    entity prop = world;
    float best = 512;
    for(entity e = nextent(world); e != world; e = nextent(e))
    {
        if(e.vr_rigid && e.carry_player == world && (e.touch == forcegrabbable_touch || e.vr_impact) &&
           vlen(e.origin - self.origin) < best)
        {
            best = vlen(e.origin - self.origin);
            prop = e;
        }
    }
    if(prop == world)
    {
        dprint("throw test: no prop\n");
        return;
    }
    prop.throwhit = QVR_THROWHIT_HIT;
    prop.owner = world;
    prop.vr_fling_hit = 0;
    prop.fg_state = 0;
    prop.flags = prop.flags - (prop.flags & FL_ONGROUND);
    vector top = VRGetHeadPos(self);
    top_z = max(top_z, self.absmax_z) + 4 - prop.mins_z; // (its bottom just over your head)
    if(cvar("vr_test_throw_up_body"))
    {
        top = self.origin + '0 0 16' - '0 0 0.5' * (prop.mins_z + prop.maxs_z); // (at your chest, in your box)
    }
    prop.carry_letgo = self; // (let go of by you: VR_Carry_Loose)
    prop.carry_letgo_time = time;
    setorigin(prop, top - '1 0 0' * 0.5 * (prop.mins_x + prop.maxs_x) - '0 1 0' * 0.5 * (prop.mins_y + prop.maxs_y));
    prop.vr_spin = '0 0 0';
    prop.avelocity = '0 0 0';
    VR_Carry_Throw(prop, '0 0 1' * cvar("vr_test_throw_up_speed") * VRMetersToUnits());
    dprint(sprintf("throw test: %s thrown up from %v at %.1f m/s; your health %.0f\n", prop.classname, prop.origin,
        cvar("vr_test_throw_up_speed"), self.health));
};
#endif
