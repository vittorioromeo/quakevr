// vr_crouch_test.qc -- the crouched box's shots test (Debug > Tests > Crouch Shots; ROUND21.md, "Crouching: a smaller
// box"). `vr_crouch_test <n>`: a grunt put 280 units ahead of the first player (as he faces, short of a wall), facing
// him, fires n bullets at him without spread, as its own attack aims them (army_fire's aim at his origin, then
// EnemyFireBullets: VR_Crate_ShotAim lowers it); then it is taken away. Its `crouchtest:` line: whether he has his
// crouched box (vr_crouch_hull), his eyes over his feet, the height over his feet the aim passes him at, and the damage
// he took (his health set high for it, his armour off, then put back). Crouched behind cover (vrteleporters' crouching
// room: the 40-high walls) the shots should hit the cover: no damage; standing, they should hit him.

void() VR_CrouchTest_Run =
{
    float n = cvar("vr_crouch_test");
    entity player = find(world, classname, "player");
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
    entity dispenser = spawn();
    dispenser.weapon = 0;
    setorigin(dispenser, at);
    self = dispenser;
    activator = player;
    func_enemy_dispenser_use();
    entity grunt = self;
    self = oldself;
    remove(dispenser);
    if(grunt == dispenser)
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
    if(!cvar("vr_crouch_test"))
    {
        return;
    }
    VR_CrouchTest_Run();
    cvar_set("vr_crouch_test", "0");
};
