// Runtime fixtures exercise real acquisition, damage and delayed attack callbacks.
entity vr_ai_test_monster, vr_ai_test_player;
vector vr_ai_test_origin;
float vr_ai_test_yaw;

void() VR_AI_TestFrame =
{
    float ask = cvar("vr_portals_ai_test");
    if(ask <= 0 || deathmatch || coop) { return; }
    cvar_set("vr_portals_ai_test", "0");
    entity player = self;
    if(ask == 1)
    {
        vr_ai_test_monster = world;
        float nearest = 10000;
        for(entity m = nextent(world); m != world; m = nextent(m))
        {
            if(!(m.flags & FL_MONSTER) || m.health <= 0) { continue; }
            float distance = vlen(m.origin - '544 1320 24');
            if(distance < nearest) { nearest = distance; vr_ai_test_monster = m; }
        }
        vr_ai_test_player = player;
        if(vr_ai_test_monster == world) { bprint("portalaitest: no monster\n"); return; }
        vr_ai_test_origin = vr_ai_test_monster.origin;
        vr_ai_test_yaw = player.origin_y >= vr_ai_test_origin_y ? 90 : 270;
        bprint(sprintf("portalaitest: monster=%s origin=%v eye=%v eligible_weapon=%g\n", vr_ai_test_monster.classname, vr_ai_test_monster.origin, vr_ai_test_monster.view_ofs, vr_ai_test_monster.th_missile != SUB_Null));
        player.health = 1000;
        precache_model("progs/w_spike.mdl");
        precache_sound("wizard/wattack.wav");
    }
    if(vr_ai_test_monster == world) { return; }
    self = vr_ai_test_monster;
    if(ask == 1 || ask == 7 || ask == 16)
    {
        self.enemy = world; self.vr_ai_target = world; self.vr_ai_gate = 0;
        self.angles_y = ask == 16 ? anglemod(vr_ai_test_yaw + 180) : vr_ai_test_yaw;
        sight_entity = world; sight_entity_time = -100;
        float pvs = checkclient() != world;
        float physical = point_visible(self.origin + self.view_ofs, player.origin + player.view_ofs);
        float found = FindTarget();
        bprint(sprintf("portalaitest: found=%g enemy_player=%g physical=%g pvs=%g gate=%g range=%g image=%v\n",
            found, self.enemy == player, physical, pvs, self.vr_ai_gate, range(player), VR_AI_Origin(player)));
        self.think = SUB_Null; self.nextthink = -1;
    }
    else if(ask == 2) { makevectors(self.angles); EnemyFireBullets(1, v_forward, '0 0 0'); }
    else if(ask == 3) { enforcer_fire(); }
    else if(ask == 4) { CastLightning(); }
    else if(ask == 5) { Wiz_StartFast(); }
    else if(ask == 6)
    {
        float projectiles = 0, pending = 0;
        for(entity e = nextent(world); e != world; e = nextent(e))
        {
            if(e.owner != self) { continue; }
            if(e.movetype == MOVETYPE_FLYMISSILE || e.movetype == MOVETYPE_BOUNCE) { projectiles++; }
            if(e.think == Wiz_FastFire) { pending++; }
        }
        bprint(sprintf("portalaitest: health=%g projectiles=%g pending=%g moved=%.2f enemy_player=%g gate=%g\n",
            player.health, projectiles, pending, vlen(self.origin - vr_ai_test_origin), self.enemy == player, self.vr_ai_gate));
    }
    else if(ask == 8 || ask == 9)
    {
        entity blocker = spawn(); blocker.classname = "portal_ai_blocker";
        blocker.solid = SOLID_BBOX; blocker.movetype = MOVETYPE_NONE;
        setsize(blocker, '-12 -12 -12', '12 12 12');
        vector point;
        if(ask == 8)
            point = self.origin + self.view_ofs + normalize(VR_AI_Origin(player) + player.view_ofs - self.origin - self.view_ofs) * 32;
        else
            point = player.origin + player.view_ofs - normalize(player.origin - self.origin) * 32;
        setorigin(blocker, point);
        bprint(sprintf("portalaitest: blocker=%g at=%v\n", ask, point));
    }
    else if(ask == 10)
    {
        for(entity block = find(world, classname, "portal_ai_blocker"); block != world;
            block = find(world, classname, "portal_ai_blocker")) { remove(block); }
    }
    else if(ask == 11)
    {
        bprint(sprintf("portalaitest: muzzle_clear=%g edge_clear=%g\n", VR_AI_ShotReady(self.origin + '0 0 16'),
            VR_AI_ShotReady(self.origin + '128 0 16')));
    }
    else if(ask == 12)
    {
        self.think = self.th_run; self.nextthink = time + 0.1;
    }
    else if(ask == 18)
    {
        for(entity trigger = find(world, classname, "trigger_teleport"); trigger != world;
            trigger = find(trigger, classname, "trigger_teleport"))
        {
            if(trigger.absmin_x > 544 || trigger.absmax_x < 544 ||
               trigger.absmin_y > 1384 || trigger.absmax_y < 1384) { continue; }
            entity destination = find(world, targetname, trigger.target);
            setorigin(destination, '400 400 28'); destination.mangle_y = 180;
            bprint("portalaitest: rotated destination\n"); break;
        }
    }
    else if(ask == 13) { self.think = SUB_Null; self.nextthink = -1; }
    self = player;
};
