// Native environment acceptance aid (Debug > Tests). Reload the map after the destructive barrel test.
float MG_test_pass;
float MG_test_fail;
float MG_test_targets;
entity MG_test_barrel;
float MG_test_blood;
.float MG_test_tag;
entity MG_test_actor;
void(float ok, string label) MG_TestCheck = {
    if(ok) MG_test_pass++; else MG_test_fail++;
    dprint(sprintf("mgworldtest: %s %s\n", ok ? "PASS" : "FAIL", label));
};
void() MG_TestTarget = { MG_test_targets++; MG_test_actor = activator; };
void() MG_TestFinish = {
    MG_TestCheck(MG_test_targets == 1, "barrel fires its target once");
    MG_TestCheck(!MG_test_barrel.MG_test_tag || MG_test_barrel.solid == SOLID_NOT,
                 "barrel ceases blocking after its delayed explosion");
    MG_TestCheck(MG_test_actor.classname == "player", "target retains the attacking player as activator");
    dprint(sprintf("mgworldtest: %g passed, %g failed\n", MG_test_pass, MG_test_fail));
    remove(self);
};
void() MG_TestFrame = {
    float request = cvar("vr_mg_world_test");
    if(!request) return;
    cvar_set("vr_mg_world_test", "0");
    if(!MG_WorldCampaign()) { dprint("mgworldtest: select a native developer campaign first\n"); return; }
    entity oself = self;
    entity oother = other;
    entity p = find(world, classname, "player");
    if(!p) { dprint("mgworldtest: no player\n"); return; }
    entity fog = MG_GetFogInfoEntity(world);
    dprint(sprintf("mgworldtest: map %s world fog %.6f %v; player fog %.6f %v\n",
        mapname, fog.fog_density, fog.fog_color, p.fog_density, p.fog_color));
    if(request == 1) return;
    MG_test_pass = MG_test_fail = MG_test_targets = 0;
    entity e = spawn();
    self = e;
    e.dest = '0 0 64';
    MG_TestCheck(fabs((MG_func_bob_GetPositionForAngle(90) * '0 0 1') - 64) < 0.001, "bob uses authored degrees");
    e.count = 0;
    e.dest2 = '32 0 0';
    MG_TestCheck(fabs((MG_func_bob_GetPositionForAngle(0) * '1 0 0') - 32) < 0.001, "bob cosine second amplitude");
    e.angles = '0 0 -15';
    e.avelocity = '0 90 0';
    MG_rotate_object_continuously_tick(1);
    MG_TestCheck(e.angles_y == 90 && e.angles_z == 345, "rotation and negative roll normalization");
    e.delay = 1;
    e.distance = 1;
    e.speed = 0;
    e.state = RotateStateOff;
    e.MG_tick = MG_rotate_object_continuously_tick;
    MG_rotate_object_continuously_use();
    MG_rotate_object_tween_tick(1);
    MG_TestCheck(e.state == RotateStateOn && e.speed == 1, "rotation accelerates on");
    MG_rotate_object_continuously_use();
    MG_rotate_object_tween_tick(1);
    MG_TestCheck(e.state == RotateStateOff && !e.MG_registered, "rotation decelerates off");
    e.style = 0;
    e.mins = '-10 0 0';
    e.size = '20 20 20';
    e.origin = '100 0 0';
    vector porigin = p.origin;
    p.origin = '95 0 0';
    MG_TestCheck(fabs(MG_trigger_fog_transition_touch_get_tween(p) - 0.25) < 0.001,
                 "fog interpolates translated brush bounds");
    p.origin = porigin;
    MG_TestCheck(p.MG_fog_valid && fabs(p.fog_density - fog.fog_density) < 0.001,
                 "spawnpoint falls back to authored world fog");
    remove(e);
    MG_test_barrel = find(world, classname, "func_explode");
    if(!MG_test_barrel) {
        dprint(sprintf("mgworldtest: %g passed, %g failed; barrel test needs e5m1\n", MG_test_pass, MG_test_fail));
    } else {
        // Reuse the authored brush hull in place, avoiding a bounding-box approximation for hand hazards.
        entity barrel = MG_test_barrel;
        float oldsolid = barrel.solid;
        barrel.solid = SOLID_NOT;
        entity hazard = spawn();
        self = hazard;
        hazard.classname = "func_hurt";
        hazard.model = barrel.model;
        hazard.spawnflags = FUNC_HURT_START_ON;
        func_hurt();
        entity player = spawn();
        player.classname = "player";
        player.ishuman = TRUE;
        player.flags = FL_CLIENT;
        player.vrbits0 = QVR_VRBITS0_HANDSTRACKED;
        player.health = 100;
        player.takedamage = DAMAGE_YES;
        player.origin = p.origin;
        player.handpos = (hazard.absmin + hazard.absmax) * 0.5;
        player.offhandpos = p.origin;
        MG_func_hurt_tick(0.1);
        dprint(sprintf("mgworldtest: hazard health %.2f dmg %.2f next %.2f time %.2f\n", player.health, hazard.dmg, hazard.attack_finished, time));
        MG_TestCheck(player.health == 90, "hazard damages tracked main hand outside body");
        hazard.attack_finished = 0;
        player.offhandpos = player.handpos;
        player.handpos = p.origin;
        MG_func_hurt_tick(0.1);
        dprint(sprintf("mgworldtest: hazard health %.2f dmg %.2f next %.2f time %.2f\n", player.health, hazard.dmg, hazard.attack_finished, time));
        MG_TestCheck(player.health == 80, "hazard damages tracked off hand outside body");
        hazard.attack_finished = 0;
        player.offhandpos = p.origin;
        MG_func_hurt_tick(0.1);
        dprint(sprintf("mgworldtest: hazard health %.2f dmg %.2f next %.2f time %.2f\n", player.health, hazard.dmg, hazard.attack_finished, time));
        MG_TestCheck(player.health == 80, "hazard spares hands outside its brush hull");
        other = player;
        MG_func_hurt_touch();
        dprint(sprintf("mgworldtest: hazard health %.2f dmg %.2f next %.2f time %.2f\n", player.health, hazard.dmg, hazard.attack_finished, time));
        MG_TestCheck(player.health == 70, "authored body touch damages once");
        MG_func_hurt_touch();
        dprint(sprintf("mgworldtest: hazard health %.2f dmg %.2f next %.2f time %.2f\n", player.health, hazard.dmg, hazard.attack_finished, time));
        MG_TestCheck(player.health == 70, "hand and body share authored damage cooldown");
        MG_func_hurt_use();
        hazard.attack_finished = 0;
        MG_func_hurt_touch();
        dprint(sprintf("mgworldtest: hazard health %.2f dmg %.2f next %.2f time %.2f\n", player.health, hazard.dmg, hazard.attack_finished, time));
        MG_TestCheck(player.health == 70, "toggled hazard off stops damage");
        remove(player);
        remove(hazard);
        barrel.solid = oldsolid;
        entity receiver = spawn();
        receiver.targetname = "MG_world_test_receiver";
        receiver.use = MG_TestTarget;
        MG_test_barrel.MG_test_tag = 1;
        MG_test_barrel.target = receiver.targetname;
        MG_test_barrel.killtarget = "";
        MG_test_blood = vr_blood_sent;
        MG_TestCheck(VR_Hit_Metal(MG_test_barrel), "breakable brush follows VR metal impact path");
        entity projectile = spawn();
        self = projectile;
        other = MG_test_barrel;
        projectile.origin = (other.absmin + other.absmax) * 0.5;
        spawn_touchblood(10);
        MG_TestCheck(vr_blood_sent == MG_test_blood, "brush impact emits no blood");
        remove(projectile);
        self = p;
        T_Damage(MG_test_barrel, p, p, 25);
        MG_TestCheck(MG_test_barrel.takedamage == DAMAGE_NO && MG_test_barrel.nextthink > MG_test_barrel.ltime,
                     "barrel starts delayed detonation");
        e = spawn();
        e.think = MG_TestFinish;
        e.nextthink = time + 0.5;
    }
    self = oself;
    other = oother;
};
