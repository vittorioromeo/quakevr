// Quake VR's single-player test and debug impulses (ImpulseCommands calls VR_TestImpulse first, outside
// deathmatch and coop). TRUE: the impulse is handled, ImpulseCommands returns (as each did when it lived there);
// FALSE: ImpulseCommands goes on. Each block keeps its place in the old order.

float() VR_TestImpulse =
{
    if(self.impulse == 120) // QVR test: the weapons moved, and what has which (vr_weaponinst_test.qc)
    {
        VR_WeaponInst_Test();
        self.impulse = 0;
        return TRUE;
    }

    if(self.impulse == 125) // QVR test: immersive reloading's steps (vr_reload_test.qc)
    {
        VR_Reload_Test();
        self.impulse = 0;
        return TRUE;
    }

    if(self.impulse == 243) // QVR: a backpack of your ammo ahead (physics tests)
    {
        vector playerOrigin = self.origin;
        makevectors(self.v_angle);
        self.origin = playerOrigin + v_forward * 64 + '0 0 16';
        DropBackpack();
        self.origin = playerOrigin;
    }

    if(self.impulse == 244) // QVR: a grunt 64 units ahead (shove and ledge tests)
    {
        makevectors('0 1 0' * self.v_angle_y);
        vector gruntAt = self.origin + v_forward * 64;
        entity player = self;
        self = spawn();
        self.classname = "monster_army";
        setorigin(self, gruntAt);
        float gruntFacing = cvar("vr_test_spawn_facing");
        if(gruntFacing == 1 || gruntFacing == 2)
        {
            self.angles_y = VR_TestSpawn_FacingYaw(player);
        }
        else if(gruntFacing != 3)
        {
            self.angles_y = player.v_angle_y + 180;
        }
        monster_army();
        self = player;
    }

    if(self.impulse == 238) // QVR: precise hit detection's test (VR_Test_Hits)
    {
        VR_Test_Hits();
    }

    if(self.impulse == 237) // QVR: and with nails (VR_Test_Missiles),
    {
        VR_Test_Missiles(FALSE);
    }

    if(self.impulse == 236) // rockets,
    {
        VR_Test_Missiles(TRUE);
    }

    if(self.impulse == 234) // a nail with the grappling hook's class,
    {
        VR_Test_Missiles(2);
    }

    if(self.impulse == 235) // and what they touched
    {
        VR_Test_MissilesReport();
    }

    if(self.impulse == 227) // QVR test: the chainsaws in your hands nearly empty (vr_chainsaw.qc)
    {
        VR_Chainsaw_Impulse(TRUE);
    }

    if(self.impulse == 229) // QVR test: the chainsaw lying nearest you into an empty hand
    {
        VR_Chainsaw_TakeNearest();
    }

    if(self.impulse == 216) // QVR test: the crowbar lying nearest you into an empty hand
    {
        VR_Crowbar_TakeNearest();
    }

    if(self.impulse == 217) // QVR test: a crowbar dropped ahead of you (VR_Crowbar_Spawn)
    {
        VR_Crowbar_SpawnAhead();
    }

    if(self.impulse == 212) // QVR test: the enemy gun lying nearest you into an empty hand
    {
        VR_EnemyGuns_TakeNearest();
    }

    if(self.impulse == 214) // QVR test: the enemy guns in your hands left with one shot
    {
        VR_EnemyGuns_Impulse(TRUE);
    }

    if(self.impulse == 215) // QVR test: Mjolnir in the main hand strikes its lightning now
    {
        // (as a blow with 15 cells does, from its head: VRGetMuzzlePos; in water, the shock)
        if(VRGetEntWeapon(self, cVR_MainHand) == WID_MJOLNIR && self.ammo_cells >= 15)
        {
            HIP_FireMjolnirLightning(cVR_MainHand, VRGetMuzzlePos(cVR_MainHand));
        }
        else
        {
            bprint(sprintf("impulse 215: no Mjolnir in the main hand (weapon %g), or under 15 cells (%g)\n",
                VRGetEntWeapon(self, cVR_MainHand), self.ammo_cells));
        }
    }

    if(self.impulse == 220) // QVR test: the chainsaws in your hands dropped (running: they run on)
    {
        VR_Chainsaw_DropAll();
    }

    if(self.impulse == 230) // QVR test: the chainsaws in your hands started (no cord pulled)
    {
        VR_Chainsaw_StartAll();
    }

    if(self.impulse == 232) // QVR test: the nearest prop flung at a monster or you (VR_Test_Fling)
    {
        VR_Test_Fling();
    }

    if(self.impulse == 204) // QVR test: a two-handed throw's sparing of blows (VR_Throw_Test2H)
    {
        VR_Throw_Test2H();
    }

    if(self.impulse == 222) // QVR test: the nearest prop thrown up as yours (VR_Throw_TestUp)
    {
        VR_Throw_TestUp();
    }

    if(self.impulse == 231) // QVR test: an empty hand takes the nearest grappling gun lying about
    {
        VR_Grapple_TestTake();
    }

    if(self.impulse == 233) // QVR test: you touch what the hooks are in (a pickup is taken)
    {
        VR_Grapple_TestTouch();
    }

    if(self.impulse == 209) // QVR test: an axe thrown ahead (vr_test_axe; vr_axestick.qc)
    {
        VR_AxeStick_TestThrow();
    }

    if(self.impulse == 207) // QVR test: your main hand on the nearest stuck axe's handle
    {
        VR_AxeStick_TestReach();
    }

    if(self.impulse == 223) // QVR test: crate cover (a crate to crouch behind, a grunt beyond)
    {
        VR_Test_CrateCover();
    }

    if(self.impulse == 224) // QVR test: whether the crate cover test's grunt sees you
    {
        VR_Test_CrateCoverReport();
    }

    if(self.impulse == 241) // QVR: a monster or a box ahead, facing you (vr_test_spawn; weapons against models)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector spawnAt = self.origin + v_forward * cvar("vr_test_spawn_dist");
        float spawnKind = cvar("vr_test_spawn");
        entity spawnPlayer = self;
        entity spawned = world;
        if(spawnKind == 105 || spawnKind == 106)
        {
            traceline(spawnAt, spawnAt - '0 0 256', TRUE, self); // (on the floor)
            spawnAt = trace_endpos;
        }
        if(spawnKind == 105)
        {
            // An ogre's head lying on the floor (gib destroying tests): an ogre gibbed, its head (the ogre itself) put
            // there.
            entity ogreDispenser = spawn();
            ogreDispenser.weapon = 1;
            setorigin(ogreDispenser, spawnAt);
            self = ogreDispenser;
            activator = spawnPlayer;
            func_enemy_dispenser_use();
            spawned = self;
            self = spawnPlayer;
            remove(ogreDispenser);
            if(spawned != ogreDispenser)
            {
                spawned.takedamage = DAMAGE_AIM;
                T_Damage(spawned, world, world, spawned.health + 200);
                setorigin(spawned, spawnAt + '0 0 1');
                spawned.velocity = '0 0 0';
                spawned.avelocity = '0 0 0';
                dprint(sprintf("test spawn: %s's head at %v, box %v .. %v, gib %g\n", spawned.classname, spawned.origin,
                    spawned.mins, spawned.maxs, spawned.vr_gib));
            }
            spawned = world;
        }
        else if(spawnKind == 106)
        {
            // A gib lying on the floor (gib destroying tests).
            self = spawn();
            setorigin(self, spawnAt + '0 0 1');
            ThrowGib("progs/gib1.mdl", -10);
            entity lying = find(world, classname, "");
            while(lying)
            {
                if(lying.vr_gib && lying.ltime == time && lying.carry_player == world && lying.origin == self.origin)
                {
                    lying.velocity = '0 0 0';
                    lying.avelocity = '0 0 0';
                    dprint(sprintf("test spawn: a gib at %v, box %v .. %v\n", lying.origin, lying.mins, lying.maxs));
                }
                lying = find(lying, classname, "");
            }
            remove(self);
            self = spawnPlayer;
        }
        else if(spawnKind >= 107 && spawnKind <= 109)
        {
            // A crate (vr_crates.qc), turned 30 degrees off facing you; 109: a large one with a small one on it.
            traceline(spawnAt + '0 0 8', spawnAt - '0 0 256', TRUE, self); // (on the floor)
            spawned = VR_Crate_TestSpawn(trace_endpos, spawnKind == 107 ? 1 : 2, spawnPlayer.v_viewangle_y + 30, spawnKind == 109);
        }
        else if(spawnKind >= 120 && spawnKind <= 124)
        {
            // The climbing tests' crates, square to you (VR_Crate_ClimbTestSpawn): 120 a large one, 121 two large ones
            // stacked, 122 a large one on its side, 123 a small one on a large one, 124 a small one.
            traceline(spawnAt + '0 0 8', spawnAt - '0 0 256', TRUE, self); // (on the floor)
            spawned = VR_Crate_ClimbTestSpawn(trace_endpos, spawnPlayer.v_viewangle_y, spawnKind - 119);
        }
        else if(spawnKind == 111 || spawnKind == 112)
        {
            // A barrel (vr_crates.qc's vr_barrel: vrstart's), upright or lying across your view (it may roll).
            traceline(spawnAt + '0 0 8', spawnAt - '0 0 256', TRUE, spawnPlayer); // (on the floor)
            self = spawn();
            self.angles = '0 1 0' * (spawnPlayer.v_viewangle_y + 90);
            self.spawnflags = spawnKind == 112 ? 1 : 0;
            setorigin(self, trace_endpos + '0 0 20');
            vr_barrel();
            spawned = self;
            self = spawnPlayer;
        }
        else if(spawnKind == 110)
        {
            // The rocks and bricks (vr_debris.qc), a row of each across your view, each model in another skin (rocks
            // 1-5: grey, beige, brown, mossy, red-brown; bricks 1-4: red, brown, yellow, blue-grey): their textures
            // beside the map's.
            makevectors('0 1 0' * spawnPlayer.v_viewangle_y);
            for(float pieceIndex = 0; pieceIndex < 9; pieceIndex++)
            {
                float pieceRow = pieceIndex < 5 ? 0 : 1;
                float pieceCol = pieceRow ? pieceIndex - 5 - 1.5 : pieceIndex - 2;
                self = spawn();
                self.model = pieceRow ? sprintf("progs/vr_brick%d.mdl", pieceIndex - 4) : sprintf("progs/vr_rock%d.mdl", pieceIndex + 1);
                self.skin = pieceRow ? pieceIndex - 5 : pieceIndex;
                self.angles = '0 1 0' * (spawnPlayer.v_viewangle_y + 90 + pieceIndex * 37);
                setorigin(self, spawnAt + v_right * pieceCol * 12 + v_forward * pieceRow * 14 + '0 0 8');
                vr_debris_piece();
            }
            self = spawnPlayer;
        }
        else if(spawnKind >= 100 && spawnKind < 200) // (AD support: 200.. AD's monsters, by the dispenser below)
        {
            self = spawn();
            setorigin(self, spawnAt - '16 16 0'); // a box's origin is its corner: its middle there
            if(spawnKind == 100)      { item_health(); }
            else if(spawnKind == 102) { misc_explobox(); }  // 40 kg (grappling hook tests)
            else if(spawnKind == 103) { misc_explobox2(); } // 25 kg
            else if(spawnKind == 104) { misc_explobox(); self.takedamage = DAMAGE_NO; } // one that never blows up (shot pushes)
            else if(spawnKind == 113) { self.classname = "item_key1"; item_key1(); } // the silver key (taken at a holster: vr_collect_fx tests)
            else                      { item_shells(); }
            if((spawnKind == 100 || spawnKind == 101 || spawnKind == 113) && !VRPlaceItemNow()) // (its final size from the first frame)
            {
                self = world;
            }
            spawned = self;
            float spawnYaw = cvar("vr_test_spawn_yaw");
            float spawnTilt = cvar("vr_test_spawn_tilt");
            if(spawnYaw != 0 || spawnTilt != 0)
            {
                // Loose (a physics prop), turned about its middle and tipped, its lowest corner on the floor where it
                // stood (grappling hook tests: a turned box's empty corners).
                vector spawnMid = (self.mins + self.maxs) * 0.5;
                vector spawnAt2 = self.origin + spawnMid;
                float spawnFloor = self.origin_z + self.mins_z;
                VR_Carry_Loose(self);
                self.angles = '0 1 0' * (spawnPlayer.v_viewangle_y + spawnYaw) + '0 0 1' * spawnTilt;
                vector spawnTurned = spawnAt2 - modelpoint(self, self.angles, spawnMid);
                float spawnLow = 999999;
                for(float c = 0; c < 8; c++)
                {
                    vector corner = self.mins;
                    if(c & 1) { corner_x = self.maxs_x; }
                    if(c & 2) { corner_y = self.maxs_y; }
                    if(c & 4) { corner_z = self.maxs_z; }
                    corner = modelpoint(self, self.angles, corner);
                    spawnLow = min(spawnLow, spawnTurned_z + corner_z);
                }
                spawnTurned_z = spawnTurned_z + spawnFloor + 0.5 - spawnLow;
                setorigin(self, spawnTurned);
                self.velocity = '0 0 0';
                dprint(sprintf("test spawn: %s turned %v at %v, its middle at %v\n", self.classname, self.angles, self.origin,
                    self.origin + modelpoint(self, self.angles, spawnMid)));
            }
        }
        else
        {
            // The firing range's dispenser, as if its button were pressed (the same numbers).
            entity dispenser = spawn();
            dispenser.weapon = spawnKind;
            setorigin(dispenser, spawnAt);
            float spawnFacing = cvar("vr_test_spawn_facing");
            float spawnFaceYaw = VR_TestSpawn_FacingYaw(spawnPlayer);
            if(spawnFaceYaw >= 0)
            {
                dispenser.angles_y = spawnFaceYaw; // (before its spawn function: its ideal_yaw too)
            }
            self = dispenser;
            activator = spawnPlayer;
            func_enemy_dispenser_use();
            if(self != dispenser)
            {
                spawned = self;
                if(spawnFaceYaw >= 0)
                {
                    spawned.angles_y = spawnFaceYaw;
                }
                else if(spawnFacing != 3)
                {
                    spawned.angles_y = spawnPlayer.v_viewangle_y + 180;
                }
            }
            remove(dispenser);
        }
        self = spawnPlayer;
        if(spawned != world && spawnKind < 100 && cvar("vr_test_spawn_dead"))
        {
            spawned.takedamage = DAMAGE_AIM; // (walkmonster_start gives it a moment later)
            // 2: gibbed (its head left lying: the flies' tests, VR_Head_StopFlies); 3: a ragdoll (vr_ragdoll on: he goes limp
            // as he falls, Gibs and Corpses > Ragdolls).
            if(cvar("vr_test_spawn_dead") == 3 && !cvar("vr_ragdoll"))
            {
                cvar_set("vr_ragdoll", "1");
                dprint("vr_test_spawn_dead 3: ragdolls on (vr_ragdoll 1)\n");
            }
            T_Damage(spawned, world, world, spawned.health + (cvar("vr_test_spawn_dead") == 2 ? 100 : 1));
        }
        // A box or a crate put into the empty main hand (vr_test_spawn_hold: held props' tests, the blood on them).
        if(spawned != world && spawnKind >= 100 && cvar("vr_test_spawn_hold"))
        {
            if(spawned.handtouch == VR_Carry_Handtouch && spawned.carry_player == world && VRIsHandEmpty(self, cVR_MainHand))
            {
                setorigin(spawned, VRGetHandPos(cVR_MainHand) - (spawned.mins + spawned.maxs) * 0.5);
                VR_Carry_Start(self, cVR_MainHand, spawned);
            }
            else
            {
                bprint(sprintf("test spawn: %s not put in the main hand (not a carried prop, or the hand is full)\n", spawned.classname));
            }
        }
    }

    if(self.impulse == 245) // QVR: a gib in the empty off hand (held gib tests)
    {
        ThrowGib("progs/gib1.mdl", -10);
        entity gib = find(world, classname, "");
        entity newest = world;
        while(gib)
        {
            if(gib.vr_gib && gib.carry_player == world && gib.ltime == time && gib.model == "progs/gib1.mdl")
            {
                newest = gib;
            }
            gib = find(gib, classname, "");
        }
        if(newest != world && VRIsHandEmpty(self, cVR_OffHand))
        {
            setorigin(newest, VRGetHandPos(cVR_OffHand));
            VR_Carry_Start(self, cVR_OffHand, newest);
        }
    }

    // QVR: a gib or a head in the empty off hand, each in turn (fitted hands tests, round 21).
    float testHeldHand = cvar("vr_test_held_hand") ? cVR_MainHand : cVR_OffHand;
    if(self.impulse == 252 && VRIsHandEmpty(self, testHeldHand) && cvar("vr_test_held_pick") >= 9)
    {
        VR_Test_HeldProp(floor(cvar("vr_test_held_pick")), testHeldHand);
    }
    else if(self.impulse == 252 && VRIsHandEmpty(self, testHeldHand))
    {
        if(cvar("vr_test_held_pick") >= 0)
        {
            vr_test_held_kind = floor(cvar("vr_test_held_pick")); // (vr_test_held_pick: one model only)
        }
        string testModel = "progs/gib1.mdl";
        if(vr_test_held_kind == 1) testModel = "progs/h_player.mdl";
        else if(vr_test_held_kind == 2) testModel = "progs/gib2.mdl";
        else if(vr_test_held_kind == 3) testModel = "progs/gib3.mdl";
        else if(vr_test_held_kind == 4) testModel = "progs/h_guard.mdl";
        else if(vr_test_held_kind == 5) testModel = "progs/h_ogre.mdl";
        else if(vr_test_held_kind == 6) testModel = "progs/h_knight.mdl";
        else if(vr_test_held_kind == 7) testModel = "progs/h_zombie.mdl";
        else if(vr_test_held_kind == 8) testModel = "progs/h_demon.mdl";
        vr_test_held_kind = vr_test_held_kind >= 8 ? 0 : vr_test_held_kind + 1;
        entity testGib = spawn();
        setmodel(testGib, testModel);
        testGib.movetype = MOVETYPE_BOUNCE;
        testGib.solid = SOLID_NOT;
        testGib.think = SUB_Remove;
        testGib.nextthink = time + 60;
        VR_MakeGibGrabbable(testGib);
        if(cvar("vr_test_held_destroy"))
        {
            VR_Gib_MakeDestroyable(testGib, (testModel == "progs/gib1.mdl" || testModel == "progs/gib2.mdl" || testModel == "progs/gib3.mdl") ? 1 : 1.5); // (a head 1.5)
        }
        setorigin(testGib, VRGetHandPos(testHeldHand));
        VR_Carry_Start(self, testHeldHand, testGib);
        dprint("test: "); dprint(testModel); dprint(testHeldHand == cVR_MainHand ? " in the main hand\n" : " in the off hand\n");
    }

    // QVR: a monster's projectile from 300 units away at your face, for batting and catching tests (vr_deflect,
    // vr_grenade.qc): 246 the kind vr_test_projectile says (0 a knight's spike at 600 units/s, 1 an enforcer's laser,
    // 2 a scrag's spit, 3 a vore's ball, 4 an ogre's grenade, 5 a zombie's flesh; the lobbed ones on the arc that
    // reaches you), from vr_test_projectile_side degrees to your left of ahead; 247 a hell knight's spike (300 u/s).
    if((self.impulse == 246 || self.impulse == 247))
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector aim = VRGetHeadPos(self) + v_forward * 16 - '0 0 10';
        float side = cvar("vr_test_projectile_side") * 0.0174533;
        vector away = v_forward * cos(side) - v_right * sin(side);
        entity shooter = spawn();
        shooter.classname = "monster_test_shooter";
        shooter.flags = FL_MONSTER;
        shooter.enemy = self;
        setorigin(shooter, aim + away * 300);
        shooter.angles = vectoangles(away * -1);
        shooter.think = SUB_Remove;
        shooter.nextthink = time + 6;
        entity player = self;
        float kind = 0;
        if(player.impulse == 246)
        {
            kind = cvar("vr_test_projectile");
        }
        vector dir = normalize(aim - shooter.origin);
        self = shooter;
        entity shot = world;
        if(kind == 1)
        {
            shot = LaunchLaser(shooter.origin, dir);
        }
        else if(kind == 2)
        {
            shot = launch_spike(shooter.origin, dir);
            shot.velocity = dir * 600;
            shot.classname = "wizspike";
            setmodel(shot, "progs/w_spike.mdl");
            setsize(shot, '0 0 0', '0 0 0');
        }
        else if(kind == 3)
        {
            setorigin(shooter, shooter.origin - '0 0 10'); // (it fires from 10 units above itself)
            ShalMissile();
        }
        else if(kind == 4 || kind == 5)
        {
            if(kind == 4)
            {
                OgreFireGrenade();
            }
            else
            {
                makevectors(shooter.angles);
                ZombieFireGrenade('0 0 24'); // (from its origin)
            }
            for(shot = nextent(world); shot; shot = nextent(shot))
            {
                if(shot.owner == shooter)
                {
                    shot.velocity = VR_Lob_Velocity(shot.origin, aim, VR_LOB_SPEED, shooter); // (at your face)
                    shot.vr_gren_lastvel = shot.velocity;
                    break;
                }
            }
        }
        else
        {
            shot = launch_spike(shooter.origin, dir);
            shot.velocity = dir * (player.impulse == 246 ? 600 : 300);
            shot.classname = "knightspike";
        }
        self = player;
        dprint(sprintf("test: projectile %g from %v at %v\n", kind, shooter.origin, aim));
    }

    // QVR: shooting grenades' tests (vr_grenade.qc, "Shooting grenades"): 210 shoots the nearest grenade shots set off
    // (from your eyes, with vr_test_grenade_shot: a pellet, a nail, a rocket, a laser); 211 takes a hand grenade from the
    // pouch into the empty off hand, lets go of it unarmed (a dud), and puts it on the floor vr_test_grenade_dist ahead.
    if(self.impulse == 210)
    {
        VR_GrenShot_TestShoot();
    }

    if(self.impulse == 211)
    {
        VR_GrenShot_TestDud();
    }

    // QVR: the nearest ogre or zombie throws at you now (grenade and lob tests; vr_grenade.qc); an ogre a multi-grenade
    // with vr_test_projectile 6.
    if(self.impulse == 240)
    {
        entity lobber = world;
        float lobberDist = 4000;
        for(entity near = findradius(self.origin, 4000); near; near = near.chain)
        {
            if((near.flags & FL_MONSTER) && near.health > 0 && vlen(near.origin - self.origin) < lobberDist &&
               (substring(near.classname, 0, 12) == "monster_ogre" || near.classname == "monster_zombie"))
            {
                lobber = near;
                lobberDist = vlen(near.origin - self.origin);
            }
        }
        if(lobber != world)
        {
            entity lobPlayer = self;
            self = lobber;
            self.enemy = lobPlayer;
            self.angles_y = vectoyaw(lobPlayer.origin - self.origin);
            dprint(sprintf("test: %s at %v throws at you at %v\n", self.classname, self.origin, lobPlayer.origin));
            makevectors(self.angles);
            if(self.classname == "monster_zombie")
            {
                ZombieFireGrenade('-10 -22 30');
            }
            else
            {
                float lobFlags = self.spawnflags;
                if(cvar("vr_test_projectile") == 6)
                {
                    self.spawnflags = self.spawnflags | OGRE_BOSS; // (a multi-grenade)
                }
                OgreFireGrenade();
                self.spawnflags = lobFlags;
            }
            self = lobPlayer;
        }
    }

    if(self.impulse == 198) // QVR: the monster spawned last punched in the face: teeth (vr_teeth.qc)
    {
        VR_Teeth_Test();
    }

    if(self.impulse == 242) // QVR: the nearest melee monster strikes you now (parry tests; vr_melee.qc)
    {
        VR_Parry_TestBlow();
    }

    if(self.impulse == 248) // QVR: a knight 64 units ahead (parry tests)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector knightAt = self.origin + v_forward * 64;
        entity knightPlayer = self;
        self = spawn();
        self.classname = "monster_knight";
        setorigin(self, knightAt);
        self.angles_y = knightPlayer.v_viewangle_y + 180;
        monster_knight();
        self = knightPlayer;
    }

    if(self.impulse == 219) // QVR: shove the nearest monster (enemy shove tests; vr_enemyshove.qc)
    {
        VR_EnemyShove_TestShoveNearest();
    }

    if(self.impulse == 221) // QVR: a blast at your feet in 3 s (knocked off a hold; vr_enemyshove.qc)
    {
        VR_Climb_TestBlast();
    }

    if(self.impulse == 249) // QVR: does the pose parry a blow from ahead? (developer 1)
    {
        VR_Parry_Probe();
    }

    if(self.impulse == 250) // QVR: an armour 24 units ahead: green, yellow, red in turn (wearable armour tests)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector armorAt = self.origin + v_forward * 24;
        entity armorPlayer = self;
        float armorKind = vr_test_armor_kind;
        vr_test_armor_kind = armorKind >= 2 ? 0 : armorKind + 1;
        self = spawn();
        setorigin(self, armorAt);
        self.angles_y = armorPlayer.v_viewangle_y + 90;
        if(armorKind == 0)
        {
            self.classname = ITEM_NAME_ARMOR1;
            item_armor1();
        }
        else if(armorKind == 1)
        {
            self.classname = ITEM_NAME_ARMOR2;
            item_armor2();
        }
        else
        {
            self.classname = ITEM_NAME_ARMORINV;
            item_armorInv();
        }
        dprint("test: "); dprint(self.classname); dprint(" at "); dprint(vtos(armorAt)); dprint(", head ");
        dprint(vtos(VRGetHeadPos(armorPlayer))); dprint(" yaw "); dprint(ftos(armorPlayer.v_viewangle_y)); dprint(", hands ");
        dprint(vtos(armorPlayer.offhandpos)); dprint(" "); dprint(vtos(armorPlayer.handpos)); dprint("\n");
        vr_test_armor_last = self;
        self = armorPlayer;
    }

    // QVR: the last armour impulse 250 dropped comes back 3 s after it is worn (deathmatch's respawn, sped up).
    if(self.impulse == 251 && vr_test_armor_last != world)
    {
        vr_test_armor_last.wait = 3;
    }

    return FALSE;
};
