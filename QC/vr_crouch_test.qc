#ifdef QVR_TESTS
// vr_crouch_test.qc -- the crouched box's shots test (Debug > Tests > Crouch Shots; ROUND21.md, "Crouching: a smaller
// box"). `vr_crouch_test <n>`: a grunt put 280 units ahead of the first player (as he faces, short of a wall), facing
// him, fires n bullets at him without spread, as its own attack aims them (army_fire's aim at his origin, then
// EnemyFireBullets: VR_Crate_ShotAim lowers it); then it is taken away. Its `crouchtest:` line: whether he has his
// crouched box (vr_hull.cpp), his eyes over his feet, the height over his feet the aim passes him at, and the damage
// he took (his health set high for it, his armour off, then put back). Crouched behind cover (vrteleporters' crouching
// room: the 40-high walls) the shots should hit the cover: no damage; standing, they should hit him.

void() VR_CrouchTest_Run =
{
    float n = cvar_hget(cvarh_vr_crouch_test);
    entity player = VR_Player();
    if(player == world)
    {
        return;
    }
    float yaw = player.v_viewangle_y;
    makevectors('0 1 0' * yaw);
    vector fwd = v_forward;
    vector from = player.origin + '0 0 16';
    traceline(from, from + fwd * 280, TRUE, player);
    vector at = trace_endpos - fwd * 32 - '0 0 16';
    entity oldself = self;
    entity grunt = VR_Test_Spawn(player, 0, at, -1);
    self = oldself;
    if(!grunt)
    {
        bprint("crouchtest: no grunt\n");
        return;
    }
    grunt.angles_y = yaw + 180;

    float health0 = player.health, armor0 = player.armorvalue;
    player.health = 10000;
    player.armorvalue = 0;
    float aim = 0;
    self = grunt;
    self.enemy = player;
    for(float i = 0; i < n; i++)
    {
        makevectors(self.angles);
        vector dir = normalize(player.origin - self.origin); // army_fire's aim (without its lead)
        vector src = self.origin + v_forward * 10;
        src_z = self.absmin_z + self.size_z * 0.7;
        vector d = VR_Crate_ShotAim(src, dir, player);
        vector flat = d;
        flat_z = 0;
        vector to = player.origin - src;
        to_z = 0;
        aim = src_z + d_z * vlen(to) / max(0.001, vlen(flat)) - player.absmin_z;
        EnemyFireBullets(1, dir, '0 0 0');
    }
    self = oldself;
    float took = 10000 - player.health;
    player.health = health0;
    player.armorvalue = armor0;
    remove(grunt);
    float eye = player.headpos == '0 0 0' ? 0 : player.headpos_z - player.absmin_z;
    bprint(sprintf("crouchtest: crouched %g eye %.1f aim %.1f damage %g from %g shots\n",
        VRHasBit(player.vrbits0, QVR_VRBITS0_CROUCHED) ? 1 : 0, eye, aim, took, n));
};

void() VR_CrouchTest_Frame =
{
    if(!cvar_hget(cvarh_vr_crouch_test))
    {
        return;
    }
    VR_CrouchTest_Run();
    cvar_set("vr_crouch_test", "0");
};
// vr_sphit_test 1: a grunt 280 units ahead fires a grid of aimed bullets (17 sideways, -24..24 in 3s, by 17 up, -24..40
// in 4s; 289 shots) at the player and counts the hits: `sphit: hits N of M` (the body-shape hit, vr_hit_player_shape /
// vr_hit_player_fatten; the box when the shape is 0). Run it standing in the open (about as many hits with the default
// fattening as with the box), or crouched behind cover (none).
void() VR_SphitTest_Run =
{
    entity player = VR_Player();
    if(player == world)
    {
        return;
    }
    float yaw = player.v_viewangle_y;
    makevectors('0 1 0' * yaw);
    vector fwd = v_forward;
    vector from = player.origin + '0 0 16';
    traceline(from, from + fwd * 280, TRUE, player);
    vector at = trace_endpos - fwd * 32 - '0 0 16';
    entity oldself = self;
    entity grunt = VR_Test_Spawn(player, 0, at, -1);
    self = oldself;
    if(!grunt)
    {
        bprint("sphit: no grunt\n");
        return;
    }
    grunt.angles_y = yaw + 180;
    float health0 = player.health, armor0 = player.armorvalue;
    float hits = 0, shots = 0;
    self = grunt;
    self.enemy = player;
    makevectors(self.angles);
    vector side = v_right;
    for(float dy = -24; dy <= 24; dy = dy + 3)
    {
        for(float dz = -24; dz <= 40; dz = dz + 4)
        {
            player.health = 10000;
            player.armorvalue = 0;
            makevectors(self.angles);
            vector src = self.origin + v_forward * 10;
            src_z = self.absmin_z + self.size_z * 0.7;
            vector aimat = player.origin + side * dy;
            aimat_z = aimat_z + dz;
            EnemyFireBullets(1, normalize(aimat - src), '0 0 0');
            if(player.health < 10000)
            {
                hits = hits + 1;
            }
            shots = shots + 1;
        }
    }
    self = oldself;
    player.health = health0;
    player.armorvalue = armor0;
    remove(grunt);
    bprint(sprintf("sphit: hits %g of %g\n", hits, shots));
};

// vr_sphit_test 2: the lean case (`sphitlean: hits N of M ...`). Face yaw 90 (vr_mock_look 0 90), the player's right is +x.
// A wall (an inline brush model of the map, thin in y) stands 60 units ahead, its +x edge swept -16..16 to his right (one line each); the grunt is
// 280 ahead, beyond it. The script leans the mock head sideways (vr_mock_hand head 0.3 1.6 0) per case. Prints where the
// head is, whether the grunt's visible() (origin + view_ofs, never the head) sees him, and the hits of the same grid
// aimed at his origin (as the AI aims: enemy.origin).
void() VR_SphitTest_RemoveWall =
{
    for(entity w = find(world, classname, "sphitwall"); w; w = find(w, classname, "sphitwall"))
    {
        remove(w);
    }
};

void(float edge) VR_SphitTest_Lean =
{
    entity player = VR_Player();
    if(player == world)
    {
        return;
    }
    // (the wall: six copies of the map's first inline model, a 48 x 16 x 96 door, side by side to the left of its edge)
    for(float k = 0; k < 6; k = k + 1)
    {
        entity wall = spawn();
        wall.classname = "sphitwall";
        setmodel(wall, "*1");
        wall.solid = SOLID_BSP;
        wall.movetype = MOVETYPE_PUSH;
        vector o = player.origin;
        o_x = player.origin_x + edge - wall.maxs_x - 48 * k;
        o_y = player.origin_y + 60 - wall.mins_y;
        o_z = player.origin_z - 24 - wall.mins_z;
        setorigin(wall, o);
    }
    vector at = player.origin + '0 280 0';
    at_z = player.origin_z - 24;
    entity oldself = self;
    entity grunt = VR_Test_Spawn(player, 0, at, -1);
    self = oldself;
    if(!grunt)
    {
        VR_SphitTest_RemoveWall();
        bprint("sphitlean: no grunt\n");
        return;
    }
    grunt.angles_y = 270;
    self = grunt;
    self.enemy = player;
    float sees = visible(player);
    float health0 = player.health, armor0 = player.armorvalue;
    float hits = 0, shots = 0;
    for(float dx = -24; dx <= 24; dx = dx + 3)
    {
        for(float dz = -24; dz <= 40; dz = dz + 4)
        {
            player.health = 10000;
            player.armorvalue = 0;
            vector src = self.origin + '0 -10 0';
            src_z = self.absmin_z + self.size_z * 0.7;
            vector aimat = player.origin + '1 0 0' * dx;
            aimat_z = aimat_z + dz;
            EnemyFireBullets(1, normalize(aimat - src), '0 0 0');
            if(player.health < 10000)
            {
                hits = hits + 1;
            }
            shots = shots + 1;
        }
    }
    self = oldself;
    player.health = health0;
    player.armorvalue = armor0;
    remove(grunt);
    VR_SphitTest_RemoveWall();
    bprint(sprintf("sphitlean: edge %g hits %g of %g (head %.1f right %.1f up; grunt sees him %g)\n", edge, hits, shots,
        player.headpos_x - player.origin_x, player.headpos_z - player.origin_z, sees));
};

// vr_sphit_test 2, then (leanaim) the received damage per second from a live grunt (its own AI: sight, attack decisions,
// aim; held on its spot), each case VR_SPHITDPS_SECS seconds: 1 standing in the open, 2 leaning 30 cm right (the mock
// head, vr_mock_hand head 0.3 1.6 0) out from a wall 20 ahead whose edge is 16 to his right, 3 behind it not leaning.
// The grunt stands 280 ahead and 150 right, so the wall hides his eyes' point (origin + view_ofs) in both wall cases, and
// a lean shows his head and right shoulder. Prints `sphitlean: dps open X lean Y hidden Z` and the verdict
// `sphitlean: N passed, M failed` (open > lean > 0, hidden 0).
float VR_SPHITDPS_SECS = 40;
float VR_SPHITDPS_SETTLE = 12; // frames for the mock head to move before the grunt comes
float vr_sphitdps_case;        // 0 idle; 1 open, 2 lean, 3 hidden
float vr_sphitdps_frame;
float vr_sphitdps_dmg;
float vr_sphitdps_reaims0;
float vr_sphitdps_t0; // the grunt's coming (time)
entity vr_sphitdps_grunt;
vector vr_sphitdps_spot;
float vr_sphitdps_open, vr_sphitdps_lean, vr_sphitdps_hidden;

void(entity player, float edge, float ahead) VR_SphitTest_Wall =
{
    for(float k = 0; k < 6; k = k + 1)
    {
        entity wall = spawn();
        wall.classname = "sphitwall";
        setmodel(wall, "*1");
        wall.solid = SOLID_BSP;
        wall.movetype = MOVETYPE_PUSH;
        vector o = player.origin;
        o_x = player.origin_x + edge - wall.maxs_x - 48 * k;
        o_y = player.origin_y + ahead - wall.mins_y;
        o_z = player.origin_z - 24 - wall.mins_z;
        setorigin(wall, o);
    }
};

void(entity player) VR_SphitDps_End =
{
    if(vr_sphitdps_grunt)
    {
        remove(vr_sphitdps_grunt);
        vr_sphitdps_grunt = world;
    }
    VR_SphitTest_RemoveWall();
    float dps = vr_sphitdps_dmg / VR_SPHITDPS_SECS;
    bprint(sprintf("sphitlean: case %g dps %.2f (damage %g in %g s, %g shots onto a part showing)\n", vr_sphitdps_case, dps,
        vr_sphitdps_dmg, VR_SPHITDPS_SECS, vr_lean_reaims - vr_sphitdps_reaims0));
    if(vr_sphitdps_case == 1) { vr_sphitdps_open = dps; }
    else if(vr_sphitdps_case == 2) { vr_sphitdps_lean = dps; }
    else { vr_sphitdps_hidden = dps; }
    vr_sphitdps_case = vr_sphitdps_case + 1;
    vr_sphitdps_frame = 0;
    if(vr_sphitdps_case <= 3)
    {
        return;
    }
    vr_sphitdps_case = 0;
    localcmd("vr_mock_hand head 0 1.6 0\n");
    bprint(sprintf("sphitlean: dps open %.2f lean %.2f hidden %.2f (lean %.0f%% of open)\n", vr_sphitdps_open, vr_sphitdps_lean,
        vr_sphitdps_hidden, vr_sphitdps_open > 0 ? 100 * vr_sphitdps_lean / vr_sphitdps_open : 0));
    VR_Test_Begin("sphitlean");
    VR_Test_Check(vr_sphitdps_open > vr_sphitdps_lean, "leaning from cover takes less than standing in the open");
    VR_Test_Check(vr_sphitdps_lean > 0, "leaning from cover is shot at");
    VR_Test_Check(vr_sphitdps_hidden == 0, "hidden behind the wall takes nothing");
    VR_Test_Verdict();
};

void() VR_SphitDps_Frame =
{
    entity player = VR_Player();
    if(!vr_sphitdps_case || player == world)
    {
        return;
    }
    if(vr_sphitdps_frame == 0)
    {
        localcmd(vr_sphitdps_case == 2 ? "vr_mock_hand head 0.3 1.6 0\n" : "vr_mock_hand head 0 1.6 0\n");
        vr_sphitdps_reaims0 = vr_lean_reaims;
    }
    vr_sphitdps_frame = vr_sphitdps_frame + 1;
    if(vr_sphitdps_frame < VR_SPHITDPS_SETTLE)
    {
        return;
    }
    if(vr_sphitdps_frame == VR_SPHITDPS_SETTLE)
    {
        if(vr_sphitdps_case >= 2)
        {
            VR_SphitTest_Wall(player, 16, 20);
        }
        vr_sphitdps_spot = player.origin + '150 280 0';
        vr_sphitdps_spot_z = player.origin_z - 24;
        entity oldself = self;
        vr_sphitdps_grunt = VR_Test_Spawn(player, 0, vr_sphitdps_spot, 270);
        self = oldself;
        if(!vr_sphitdps_grunt)
        {
            bprint("sphitlean: no grunt\n");
            vr_sphitdps_case = 3;
            VR_SphitDps_End(player);
            return;
        }
        vr_sphitdps_spot = vr_sphitdps_grunt.origin;
        self = vr_sphitdps_grunt;
        self.enemy = player;
        FoundTarget();
        self = oldself;
        player.health = 1000;
        player.armorvalue = 0;
        vr_sphitdps_dmg = 0;
        vr_sphitdps_t0 = time;
        vr_sphitdps_grunt.vr_lean_seen = -10;
        bprint(sprintf("sphitlean: case %g head %.1f right; points showing %g of 7\n", vr_sphitdps_case,
            player.headpos_x - player.origin_x, 7 * VR_Lean_Scan(vr_sphitdps_grunt, player,
            vr_sphitdps_grunt.origin + vr_sphitdps_grunt.view_ofs)));
        return;
    }
    // (held on its spot: its AI turns, sees and shoots, never walks round the wall)
    if(vr_sphitdps_grunt)
    {
        setorigin(vr_sphitdps_grunt, vr_sphitdps_spot);
        vr_sphitdps_grunt.velocity = '0 0 0';
    }
    if(player.health < 1000)
    {
        vr_sphitdps_dmg = vr_sphitdps_dmg + 1000 - player.health;
        player.health = 1000;
    }
    if(time - vr_sphitdps_t0 >= VR_SPHITDPS_SECS ||!vr_sphitdps_grunt || vr_sphitdps_grunt.health <= 0)
    {
        VR_SphitDps_End(player);
    }
};

void() VR_SphitTest_Frame =
{
    VR_SphitDps_Frame();
    if(!cvar_hget(cvarh_vr_sphit_test))
    {
        return;
    }
    if(cvar_hget(cvarh_vr_sphit_test) == 2)
    {
        for(float e = -16; e <= 16; e = e + 8)
        {
            VR_SphitTest_Lean(e);
        }
        cvar_set("vr_sphit_test", "0");
        vr_sphitdps_case = 1; // (then the damage per second: VR_SphitDps_Frame)
        vr_sphitdps_frame = 0;
        return;
    }
    VR_SphitTest_Run();
    cvar_set("vr_sphit_test", "0");
};
#endif
