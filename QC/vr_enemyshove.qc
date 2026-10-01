// vr_enemyshove.qc -- grunts and enforcers shove you away when you stand too close (ROUND21.md, "Grunts and enforcers
// shove you"; Combat > Enemy Shoves):
// - within reach (vr_enemy_shove_range, between the middles, flat) for vr_enemy_shove_delay seconds, a grunt or an
//   enforcer able to (VR_EnemyShove_Ready: alive, on its feet, not staggered, not sliding from a push) starts its shove
//   (its model's shove1..shove6 frames: Misc/quakevr/make_enemyshove.py) instead of what it was about to do;
// - its blow (the 4th frame) deals vr_enemy_shove_damage and pushes you vr_enemy_shove_distance units away, if you are
//   still in reach and ahead of it (an enforcer's: vr_enemy_shove_enforcer_damage and _distance times as much); hanging
//   from a ledge or a rung, you let go (VR_Climb_KnockOff) and are pushed;
// - the blow goes through the parry (combat.qc VR_Parry, as a melee monster's blow: .vr_eshove_striking): parried, its
//   damage is cut as any parry's, and vr_enemy_shove_parry_reduction of the push is taken off (as vr_parry_reduction
//   takes off the damage; the parry's own push of you is left out for it);
// - then vr_enemy_shove_cooldown seconds before its next shove (from its start: parried or not, landed or not).
// The check runs in the AI's stand, walk and run thinks (ai.qc ai_stand, ai_walk, ai_run), so never while it fires
// (its attack frames call none of them), is in pain (its pain frames neither), or is dying.
//
// The push: a slide of the player's, a set distance (not an impulse that the ground's friction eats unevenly: a
// parried shove's 0.35x would go a fifth as far), decelerating at VR_ESHOVE_DECEL (VR_EnemyShove_PlayerFrame, every
// player frame before the player moves: client.qc PlayerPreThink).

.void() th_vrshove;          // its shove animation (monster_army, monster_enforcer); none for the others
.float vr_eshove_near;       // since when the player has been in reach (0: not)
.float vr_eshove_next;       // its next shove not before (the cooldown)
.entity vr_eshove_target;    // the player its shove is for
.vector vr_eshove_dir;       // on a player: the way a shove slides them
.float vr_eshove_speed;      // and how fast now (0: not sliding)
.vector vr_eshove_from;      // and where it started (developer 1 prints the distance)
.float vr_eshove_logged;     // its next "can't shove" line not before (developer 1)

float VR_ESHOVE_DECEL = 1200; // units/s/s: 64 units take 0.33 s
float VR_ESHOVE_HEIGHT = 48;  // the player's middle at most this far above or below the monster's

// Whether `m` can shove: alive, standing on the floor, not staggered (its pain), not sliding from a push (VR_Push).
float(entity m) VR_EnemyShove_Ready =
{
    return m.health > 0 && m.movetype == MOVETYPE_STEP && (m.flags & FL_ONGROUND) && m.vr_shove == world &&
           time >= m.pain_finished && !m.charmed;
};

// The player in `m`'s reach (the nearest), or world.
entity(entity m) VR_EnemyShove_Target =
{
    float reach = cvar_hget(cvarh_vr_enemy_shove_range);
    entity best = world;
    entity p = find(world, classname, "player");
    while(p)
    {
        vector to = p.origin - m.origin;
        float up = to_z;
        to_z = 0;
        float d = vlen(to);
        if(d <= reach && fabs(up) <= VR_ESHOVE_HEIGHT && p.health > 0 && p.takedamage && !(p.flags & FL_NOTARGET) &&
           visible(p))
        {
            best = p;
            reach = d;
        }
        p = find(p, classname, "player");
    }
    return best;
};

// In the AI's stand, walk and run thinks (self: the monster): TRUE when it starts its shove (the think is its own).
float() VR_EnemyShove_Check =
{
    if(!self.th_vrshove)
    {
        return FALSE;
    }
    if(!cvar_hget(cvarh_vr_enemy_shove))
    {
        self.vr_eshove_near = 0;
        return FALSE;
    }

    entity p = VR_EnemyShove_Target(self);
    if(p == world || (self.enemy != world && self.enemy != p))
    {
        self.vr_eshove_near = 0; // (busy with another enemy: no shoving its own way through)
        return FALSE;
    }
    if(!self.vr_eshove_near)
    {
        self.vr_eshove_near = time;
    }
    if(time - self.vr_eshove_near < cvar_hget(cvarh_vr_enemy_shove_delay) || time < self.vr_eshove_next)
    {
        return FALSE;
    }
    if(!VR_EnemyShove_Ready(self))
    {
        if(time >= self.vr_eshove_logged) // (developer 1, at most twice a second)
        {
            self.vr_eshove_logged = time + 0.5;
            dprint(sprintf("enemy shove: %s can't shove: %s (at %v, velocity %v)\n", self.classname,
                self.vr_shove != world ? "knocked away" : (time < self.pain_finished ? "staggered" :
                (!(self.flags & FL_ONGROUND) ? "off the floor" : "charmed")), self.origin, self.velocity));        }
        return FALSE;
    }
    // Facing you (within 60 degrees: the wind-up turns it the rest of the way); else it turns to you first (its AI).
    makevectors('0 1 0' * self.angles_y);
    vector toP = p.origin - self.origin;
    toP_z = 0;
    if(v_forward * normalize(toP) < 0.5)
    {
        if(self.enemy == world)
        {
            self.enemy = p;
            FoundTarget(); // (idle, you behind it: it notices you, as Quake's FindTarget would up close)
            return TRUE;
        }
        return FALSE;
    }

    self.vr_eshove_next = time + cvar_hget(cvarh_vr_enemy_shove_cooldown);
    self.vr_eshove_near = 0; // (the delay counts again after it)
    self.vr_eshove_target = p;
    if(self.enemy != p)
    {
        // Idle: it has noticed you (as Quake's FindTarget would), and goes for you after the shove.
        self.enemy = p;
        FoundTarget();
        if(self.enemy != p)
        {
            return FALSE; // (charmed)
        }
    }
    dprint(sprintf("enemy shove: %s starts its shove at %.2f (from frame %g), the player %.0f units away\n",
        self.classname, time, self.frame, vlen(toP)));
    self.th_vrshove();
    return TRUE;
};

// Pushes the player `p` along `xDir` (flat) by `xDist` units: a slide, decelerating (VR_EnemyShove_PlayerFrame).
void(entity p, vector xDir, float xDist) VR_EnemyShove_Push =
{
    xDir_z = 0;
    xDir = normalize(xDir);
    if(xDist <= 0 || xDir == '0 0 0')
    {
        return;
    }
    p.vr_eshove_dir = xDir;
    // The speed whose slide, a frame's move at a time (each frametime long, the speed falling by VR_ESHOVE_DECEL), adds
    // up to the distance: v * v / (2 * a) + v * dt / 2 = d.
    float half = VR_ESHOVE_DECEL * frametime * 0.5;
    p.vr_eshove_speed = sqrt(half * half + 2 * VR_ESHOVE_DECEL * xDist) - half;
    p.vr_eshove_from = p.origin;
};

// Every player frame, before the player moves (self: the player).
void() VR_EnemyShove_PlayerFrame =
{
    if(self.vr_eshove_speed <= 0)
    {
        return;
    }
    if(self.health <= 0 || self.movetype != MOVETYPE_WALK)
    {
        self.vr_eshove_speed = 0;
        return;
    }
    float s = self.vr_eshove_speed;
    self.velocity_x = self.vr_eshove_dir_x * s;
    self.velocity_y = self.vr_eshove_dir_y * s;
    self.vr_eshove_speed = max(0, s - VR_ESHOVE_DECEL * frametime);
    if(self.vr_eshove_speed <= 0)
    {
        vector moved = self.origin - self.vr_eshove_from;
        moved_z = 0;
        dprint(sprintf("enemy shove: the player slid %.1f units\n", vlen(moved)));
    }
};

// The shove's blow (its 4th frame; self: the monster).
void() VR_EnemyShove_Hit =
{
    entity p = self.vr_eshove_target;
    self.vr_eshove_target = world;
    if(p == world || p.health <= 0 || !p.takedamage)
    {
        return;
    }

    vector to = p.origin - self.origin;
    float up = to_z;
    to_z = 0;
    makevectors('0 1 0' * self.angles_y);
    if(vlen(to) > cvar_hget(cvarh_vr_enemy_shove_range) + 16 || fabs(up) > VR_ESHOVE_HEIGHT ||
       v_forward * normalize(to) < 0.5)
    {
        dprint(sprintf("enemy shove: %s missed (the player %.0f units away, facing %.2f)\n", self.classname, vlen(to),
            v_forward * normalize(to)));
        return;
    }

    // Through the parry, as a melee monster's blow (combat.qc VR_Parry_MeleeMonster).
    float enforcer = self.classname == "monster_enforcer";
    float dmg = cvar_hget(cvarh_vr_enemy_shove_damage) *
                (enforcer ? max(0, cvar_hget(cvarh_vr_enemy_shove_enforcer_damage)) : 1);
    float healthBefore = p.health;
    vr_parried = 0;
    self.vr_eshove_striking = TRUE;
    if(dmg > 0)
    {
        T_Damage(p, self, self, dmg);
    }
    self.vr_eshove_striking = FALSE;
    float parried = vr_parried;

    float dist = cvar_hget(cvarh_vr_enemy_shove_distance) *
                 (enforcer ? max(0, cvar_hget(cvarh_vr_enemy_shove_enforcer_distance)) : 1);
    if(parried)
    {
        dist = dist * (1 - min(1, max(0, cvar_hget(cvarh_vr_enemy_shove_parry_reduction))));
    }
    sound(self, CHAN_WEAPON, "vr/shove.wav", parried ? 0.5 : 1, ATTN_NORM);
    VR_EnemyShove_Push(p, to, dist);
    if(dist > 0)
    {
        VR_Climb_KnockOff(p, sprintf("%s's shove", self.classname)); // (hanging from a hold: you let go, and slide)
    }
    if(!parried)
    {
        entity oldSelf = self;
        self = p;
        haptic(cVR_MainHand, 0.0, 0.2, 50, 0.8);
        haptic(cVR_OffHand, 0.0, 0.2, 50, 0.8);
        self = oldSelf;
    }
    dprint(sprintf("enemy shove: %s shoves the player%s: %g damage (health %g -> %g), pushed %.1f units\n",
        self.classname, parried == 1 ? " (parried)" : (parried == 2 ? " (parried, crossed arms)" : ""), dmg,
        healthBefore, p.health, dist));
};

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
