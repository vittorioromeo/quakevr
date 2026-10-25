#ifdef QVR_TESTS
// vr_enemyshove's test harness (moved out of vr_enemyshove.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// QVR: impulse 219 (weapons.qc; Debug > Tests > Enemy Shoves): the nearest live monster within 200 units, shoved as your
// two-handed shove does (vr_melee.qc VR_Bash_Hit: knocked away, staggered), to see that it can't shove you meanwhile.
void() VR_EnemyShove_TestShoveNearest =
{
    entity best = world;
    float bestDist = 200;
    entity e = findradius(self.origin, 200);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e.health > 0 && vlen(e.origin - self.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
        e = e.chain;
    }
    if(best == world)
    {
        dprint("enemy shove: no monster within 200 units to shove\n");
        return;
    }
    vector dir = best.origin - self.origin;
    dir_z = 0;
    vr_bash_exhaust_dmg = 0;
    vr_bash_exhaust_push = 0;
    dprint(sprintf("enemy shove: test: shoving %s %.0f units away (frame %g)\n", best.classname, bestDist, best.frame));
    VR_Bash_Hit(best, best.origin, normalize(dir), QVR_HITKIND_SHOVE, -1, 1, 1);
};

// QVR: impulse 221 (weapons.qc; Debug > Tests > Enemy Shoves): a blast of 60 (about 47 to you) at your feet 3 s from
// now, 12 units ahead (towards the wall of a ledge or a rung you hang from by then): it knocks you off at
// vr_climb_blast_letgo 47 or less (combat.qc T_DamageDeal).
void() VR_Climb_TestBlastGo =
{
    entity p = self.owner;
    makevectors('0 1 0' * p.v_viewangle_y);
    setorigin(self, p.origin + v_forward * 12 - '0 0 20');
    dprint(sprintf("climb test: a blast of 60 at %v, the player at %v\n", self.origin, p.origin));
    T_RadiusDamage(self, world, 60, world);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);
    BecomeExplosion();
};

void() VR_Climb_TestBlast =
{
    entity b = spawn();
    b.classname = "vr_testblast";
    b.owner = self;
    b.think = VR_Climb_TestBlastGo;
    b.nextthink = time + 3;
    dprint("climb test: a blast at your feet in 3 s\n");
};
#endif
