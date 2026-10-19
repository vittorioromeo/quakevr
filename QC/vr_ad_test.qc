// Tests of Arcane Dimensions' rules outside and inside its maps (docs/vr-port/ARCANE_DIMENSIONS.md, section 16):
// vr_ad_test 10 and up (Debug > Tests > Arcane Dimensions), in any map while AD's data is installed. Each prints one
// "adrules:" line; scratch scripts compare them.

entity vr_ad_test_a, vr_ad_test_b; // the targets of a two-step test (12/13: the grunts in the Nail Piercer's line)
float vr_ad_test_health;           // a reading the next step compares with

// An AD entity of `xClass` made by its AD spawn function `xSpawn` at `xAt`.
entity(string xClass, void() xSpawn, vector xAt) AD_TestSpawn =
{
    entity os = self;
    entity e = spawn();
    e.classname = xClass;
    setorigin(e, xAt);
    self = e;
    xSpawn();
    self = os;
    return e;
};

// AD's own touch of artifact (or pickup) `e` by player `p` (its spawn's delay skipped).
void(entity e, entity p) AD_TestTouch =
{
    entity os = self, oo = other;
    self = e;
    other = p;
    e.ad_touch2();
    self = os;
    other = oo;
};

void(entity inflictor, entity attacker, float damage) AD_TestPelletPain = {};

// Frozen AD grunt for actual projectile-flight checks; 40-42 report after flight.
void() AD_TestPelletReport =
{
    bprint(sprintf("adhits: wid %g distance %g damage %g model %s\n", self.weapon, self.speed,
        10000 - self.enemy.health, self.enemy.model));
    remove(self.enemy);
    remove(self);
};

// Scope/model test: newly spawned real ammo boxes go through the production physics path.
void() AD_MenuAmmoReport =
{
    entity box = self.enemy;
    bprint(sprintf("admenu-ammo: kind %g big %g model %s count %g rigid %g\n",
        self.weapon, self.count, box.model, box.aflag, box.vr_rigid));
    bprint(sprintf("admenu-box: bounds %v..%v origin %v\n", box.mins, box.maxs, box.origin));
    remove(box);
    remove(self);
};

// vr_ad_test 142-144: AD's health vials and gems (vr_ad_items.qc item_healthvial). 142: the player's AD megahealth cap
// cleared (as before AD_PlayerPostThink set it). 143, a frame later: the cap back; each pickup spawned ahead is a
// force-grabbable health box, a force grab's catch puts it in the main hand, let go of at the shoulder holster it heals
// (2-5); AD's own vial entity's heal (AD's touch, ad_T_Heal) heals too (it left 0 health); then a shotgun in the main
// hand, its magazine and the player's height noted. 144, after the script's +attack and +jump: they still work (the
// magazine emptier, the player higher). One "adhealth:" line each.
float vr_ad_test_clip, vr_ad_test_z;
void(float request, entity p) AD_HealthItemsTest =
{
    entity os = self, oo = other;
    if(request == 142)
    {
        p.ad_mega_health = 0;
        bprint(sprintf("adhealth: cap cleared %g health %g\n", p.ad_mega_health, p.health));
        return;
    }
    if(request == 144)
    {
        float clip = VRGetEntWeaponClip(p, cVR_MainHand);
        bprint(sprintf("adhealth: after attack and jump: clip %g -> %g (fired %g), rose %g, health %g dead %g\n",
            vr_ad_test_clip, clip, clip < vr_ad_test_clip, p.origin_z - vr_ad_test_z, p.health, p.deadflag));
        return;
    }
    bprint(sprintf("adhealth: cap now %g (Quake VR's %g)\n", p.ad_mega_health, VR_MegaHealthCap(p)));
    makevectors(p.angles);
    float savedbits = p.vrbits0, savedhotspot = p.mainhand_hotspot, savedflags = p.flags;
    p.flags -= p.flags & FL_NOTARGET;
    p.vrbits0 |= QVR_VRBITS0_HANDSTRACKED;
    float k = 0;
    while(k < 2)
    {
        entity e = spawn();
        e.classname = k == 0 ? "item_healthvial" : "item_healthgem";
        setorigin(e, p.origin + v_forward * 48);
        self = e;
        if(k == 0) item_healthvial(); else item_healthgem();
        VRPlaceItemNow();
        self = p;
        float eligible = VR_Forcegrab_IsEligible(e);
        float amount = e.healamount;
        p.health = 50;
        setorigin(e, VRGetEntHandPos(p, cVR_MainHand));
        p.touchinghand = cVR_MainHand;
        VRSetMainHandGrabbingBit(p, TRUE); VRSetMainHandPrevGrabbingBit(p, FALSE);
        forcegrabCatching = TRUE;
        self = e; other = p; e.handtouch();
        forcegrabCatching = FALSE;
        float held = VR_Carry_Held(p, cVR_MainHand) == e;
        VRSetMainHandGrabbingBit(p, FALSE);
        p.mainhand_hotspot = QVR_HS_RIGHT_SHOULDER_HOLSTER;
        self = p; VR_Carry_HandFrame(cVR_MainHand);
        bprint(sprintf("adhealth: %s grabbable %g eligible %g held %g -> health %g (+%g) taken %g\n",
            e.classname, (e.flags & FL_FORCEGRABBABLE) != 0, eligible, held,
            p.health, amount, e.model == string_null));
        k = k + 1;
    }
    p.mainhand_hotspot = savedhotspot;
    p.vrbits0 = savedbits;
    p.flags = savedflags;
    // AD's own vial (AD's spawn and touch): its heal no longer leaves 0 health.
    entity a = AD_TestSpawn("item_healthvial", ad_item_healthvial, p.origin + v_forward * 48);
    p.health = 50;
    AD_TestTouch(a, p);
    bprint(sprintf("adhealth: AD's own vial: health %g (+%g)\n", p.health, a.healamount));
    remove(a);
    p.health = 100;
    self = p;
    VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, WeaponIdToBaseClipSize(WID_SHOTGUN));
    W_SetCurrentAmmoFor(cVR_MainHand);
    vr_ad_test_clip = VRGetEntWeaponClip(p, cVR_MainHand);
    vr_ad_test_z = p.origin_z;
    bprint(sprintf("adhealth: shotgun clip %g at height %g\n", vr_ad_test_clip, vr_ad_test_z));
    self = os; other = oo;
};

void(float request, entity p) AD_RulesTest =
{
    if(request >= 142 && request <= 144) { AD_HealthItemsTest(request, p); return; } // AD's health vials and gems
    if(request >= 61 && request <= 64) { AD_WorldTest(request, p); return; } // AD support
    if(request >= 70 && request <= 101) { AD_GunTest(request, p); return; } // AD guns: vr_ad_enemyguns.qc (91: the flamethrowers draw no blood, 101: their flames hurt a monster, a big one too; 93, 94: the crossbow; 95: Freddie's super nailgun; 96-100: the upgraded weapons' VR models)
    if(request >= 110 && request <= 129) { AD_MeleeTest(request, p); return; } // AD melee weapons: vr_ad_melee.qc (126 the fury's curve, 127 the holsters, 128-129 the hammer or mace in the main hand)
    if(request == 140 || request == 141 || request == 149) { AD_UpgradesTest(request, p); return; } // the upgrades' rules: vr_ad_upgrades_test.qc

    makevectors(p.angles);
    vector ahead = p.origin + v_forward * 64;
    entity os = self;
    if(request == 66)
    {
        // AD's sky changes as messages: a trigger_skybox's sky (AD's green sky) to every client, AD's sky fog 0.3 on you.
        ad_trig_skybox = "adgreensky";
        VR_AD_Skybox(p);
        VR_AD_SkyFog(p, 0.3);
        bprint(sprintf("adrules: sky %s sent %g, sky fog field %g\n", ad_trig_skybox, vr_ad_skysent, p.vr_ad_skyfog));
        return;
    }
    if(request == 65)
    {
        string mainBefore = p.weaponmodel, offBefore = p.weaponmodel2;
        entity standIn = spawn();
        standIn.ad_moditems = ad_IT_UPGRADE_AXE | ad_IT_UPGRADE_SSG | ad_IT_UPGRADE_LG;
        standIn.ad_resist_nails = 0.5;
        standIn.ad_classtype = ad_CT_MONARMY;
        standIn.flags = FL_MONSTER;
        entity upInst = spawn(); // (an axe's record with the Shadow Axe: the upgrades are the weapons')
        upInst.wi_adup = ad_IT_UPGRADE_AXE;
        bprint(sprintf("admenu: map %g mounted %g ammo %g resistance %g upgrades %g projectiles %g\n",
            AD_Rules(), cvar("vr_ad_mounted"), VR_AD_FeatureScope(cvar("vr_ad_ammo_models")),
            VR_AD_FeatureScope(cvar("vr_ad_resistance")), VR_AD_FeatureScope(AD_UpgradeScope()), AD_ProjectileRules()));
        bprint(sprintf("admenu-speed: player %g enemy %g\n", cvar("vr_ad_pellet_speed"), VR_AD_PelletSpeed(standIn, 1000)));
        bprint(sprintf("admenu-rules: resist %g importedDamage %g upgrade %g\n", AD_Resists(standIn),
            ad_Resist_Damage(standIn, ad_IT_NAILS, 10), AD_WeaponUpgraded(standIn, upInst, ad_IT_UPGRADE_AXE)));
        remove(standIn);
        remove(upInst);
        float kind = 0;
        while(kind < 4)
        {
            float big = 0;
            while(big < 2)
            {
                entity box = spawn();
                self = box;
                box.spawnflags = big;
                setorigin(box, p.origin + '0 0 64'); // The player's verified floor; narrow AD corridors cannot fit a sideways row.
                if(kind == 0) item_shells();
                else if(kind == 1) item_spikes();
                else if(kind == 2) item_rockets();
                else item_cells();
                entity report = spawn();
                report.enemy = box;
                report.weapon = kind;
                report.count = big;
                report.think = AD_MenuAmmoReport;
                report.nextthink = time + 0.9;
                big = big + 1;
            }
            kind = kind + 1;
        }
        self = os;
        bprint(sprintf("admenu-hands: main %g off %g %s %s\n", p.weaponmodel == mainBefore,
            p.weaponmodel2 == offBefore, p.weaponmodel, p.weaponmodel2));
        return;
    }
    if(request == 130) { AD_ProjHitTest(request, p); return; } // every projectile's one path: vr_projhit_test.qc
    if(request >= 50 && request <= 60)
    {
        AD_ShotsTest(request, p);
        return;
    }
    if(request >= 40 && request <= 42)
    {
        self = p;
        float wid = request == 40 ? WID_SHOTGUN : (request == 41 ? WID_SUPER_SHOTGUN : WID_GRUNTGUN);
        VRGiveEntWeapon(p, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
        W_SetCurrentAmmoFor(cVR_MainHand);
        vector dir = v_forward;
        dir_z = 0;
        dir = normalize(dir);
        vector muzzle = p.origin + dir * 32 + '0 0 24';
        float distance = cvar("scratch1");
        if(distance <= 0) distance = 128;
        entity g = AD_TestSpawn("monster_army", ad_monster_army, muzzle + dir * distance);
        setmodel(g, g.mdl);
        setsize(g, '-16 -16 -24', '16 16 32');
        g.solid = SOLID_SLIDEBOX;
        g.movetype = MOVETYPE_NONE;
        g.health = 10000;
        g.think = SUB_Null;
        g.nextthink = -1;
        entity report = spawn();
        report.enemy = g;
        report.weapon = wid;
        report.speed = distance;
        report.think = AD_TestPelletReport;
        report.nextthink = time + 0.8;
        g.ad_th_pain = AD_TestPelletPain;
        g.ad_th_run = g.ad_th_stand = g.ad_th_walk = g.ad_th_missile = SUB_Null;
        g.takedamage = DAMAGE_AIM;
        g.weapon = wid;
        g.speed = distance;
        setorigin(g, g.origin);
        traceline(muzzle, muzzle + dir * distance, MOVE_HITMODEL, p);
        bprint(sprintf("adhits: source %v target %v dir %v trace %g %s solid %g flags %g\n", muzzle, g.origin,
            dir, trace_fraction, trace_ent.classname, g.solid, g.flags));
        vr_pellet_damage = wid == WID_GRUNTGUN ? cvar_hget(cvarh_vr_gruntgun_damage) : 4;
        AD_FirePellets(cVR_MainHand, wid == WID_GRUNTGUN ? 1 : (wid == WID_SHOTGUN ? 6 : 14), dir,
            wid == WID_GRUNTGUN ? VR_BURST_SPREAD : '0.01 0.01 0', muzzle);
        vr_pellet_damage = 0;
        self = os;
        return;
    }
    if(request == 44)
    {
        self = p;
        VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, 6);
        W_SetCurrentAmmoFor(cVR_MainHand);
        VRGiveEntWeapon(p, cVR_OffHand, WID_SUPER_SHOTGUN, 0, 2);
        W_SetCurrentAmmoFor(cVR_OffHand);
        string offModel = p.weaponmodel2;
        string model = p.weaponmodel;
        float wid = p.weapon;
        ad_W_SetCurrentAmmo(p);
        bprint(sprintf("adhits: ammo update wid %g -> %g model %s -> %s\n", wid, p.weapon, model, p.weaponmodel));
        bprint(sprintf("adhits: off hand wid %g model %s -> %s\n", p.weapon2, offModel, p.weaponmodel2));
        self = os;
        return;
    }
    if(request == 10)
    {
        // The new artifacts but the Nail Piercer (it excludes the Sharp Shooter, as in AD) touched: what they give.
        AD_TestTouch(AD_TestSpawn("item_artifact_lavashield", ad_item_artifact_lavashield, ahead), p);
        AD_TestTouch(AD_TestSpawn("item_artifact_airtank", ad_item_artifact_airtank, ahead), p);
        AD_TestTouch(AD_TestSpawn("item_artifact_blastbelt", ad_item_artifact_blastbelt, ahead), p);
        AD_TestTouch(AD_TestSpawn("item_artifact_jumpboots", ad_item_artifact_jumpboots, ahead), p);
        AD_TestTouch(AD_TestSpawn("item_artifact_sharp_shooter", ad_item_artifact_sharp_shooter, ahead), p);
        self = p;
        AD_ArtifactsTick();
        self = os;
        bprint(sprintf("adrules: artifacts moditems %g hud %g time %g quad %g sharp %g boots %g/%g\n",
            p.ad_moditems & (ad_IT_ARTLAVASHIELD | ad_IT_ARTAIRTANK | ad_IT_ARTBLASTBELT | ad_IT_ARTJUMPBOOTS | ad_IT_ARTSHARP | ad_IT_ARTPIERCE),
            p.vr_ad_hud, p.vr_ad_hudtime, (p.items & IT_QUAD) != 0, p.ad_sharpshoot_finished - time,
            p.ad_jumpboots_airmax, p.ad_jumpboots_height));
        return;
    }
    if(request == 11)
    {
        // Their effects: the Sharp Shooter's spreads, the Blast Belt's blast (with and without), the Lava Shield, the
        // Air Tank's air, the Jump Boots' jumps.
        self = p;
        vector sg = AD_SharpSpread('0.04 0.04 0');
        vector ssg = AD_SharpSpread('0.14 0.08 0');
        self = os;
        entity boom = spawn();
        setorigin(boom, p.origin + v_forward * 32);
        float h0 = p.health, a0 = p.armorvalue;
        vector v0 = p.velocity;
        T_RadiusDamage(boom, world, 40, world);
        float beltHealth = p.health - h0, beltArmor = p.armorvalue - a0, beltPush = vlen(p.velocity - v0);
        ad_ResetPowerBlastbelt(p, 0);
        h0 = p.health;
        T_RadiusDamage(boom, world, 40, world);
        float bareHealth = p.health - h0;
        remove(boom);
        p.air_finished = time;
        self = p;
        AD_ArtifactsTick();
        float air = p.air_finished - time;
        p.flags = (p.flags - (p.flags & FL_ONGROUND)) | FL_JUMPRELEASED;
        p.velocity = '0 0 0';
        AD_JumpBoots();
        float jump1 = p.velocity_z;
        p.flags = p.flags | FL_JUMPRELEASED;
        p.velocity = '0 0 0';
        AD_JumpBoots();
        float jump2 = p.velocity_z;
        self = os;
        bprint(sprintf("adrules: effects sharp %v %v belt health %g armour %g push %.0f bare %g", sg, ssg, beltHealth,
            beltArmor, beltPush, bareHealth));
        bprint(sprintf(" lava %g air %.0f jumps %g %g\n", AD_LavaShieldOn(p), air, jump1, jump2));
        return;
    }
    if(request == 12)
    {
        // The Nail Piercer: two grunts in a line ahead, a nail through the first into the second (13 reports).
        ad_ResetPowerSharp(p, 0);
        AD_TestTouch(AD_TestSpawn("item_artifact_nail_piercer", ad_item_artifact_nail_piercer, ahead), p);
        vector flat = v_forward;
        flat_z = 0;
        flat = normalize(flat);
        vr_ad_test_a = AD_TestSpawn("monster_army", monster_army, p.origin + flat * 96);
        vr_ad_test_b = AD_TestSpawn("monster_army", monster_army, p.origin + flat * 168);
        vr_ad_test_a.ideal_yaw = vr_ad_test_b.ideal_yaw = vectoyaw(flat * -1);
        vr_ad_test_a.takedamage = vr_ad_test_b.takedamage = DAMAGE_AIM;
        self = p;
        entity n = launch_spike(p.origin + flat * 40, flat);
        self = os;
        n.velocity = flat * 1000;
        bprint(sprintf("adrules: piercer %g grunts %g %g fired\n", (p.ad_moditems & ad_IT_ARTPIERCE) != 0,
            vr_ad_test_a.health, vr_ad_test_b.health));
        return;
    }
    if(request == 13)
    {
        bprint(sprintf("adrules: pierced grunts %g %g\n", vr_ad_test_a.health, vr_ad_test_b.health));
        vr_ad_test_a.health = vr_ad_test_b.health = -1;
        VR_Scene_Remove(vr_ad_test_a);
        VR_Scene_Remove(vr_ad_test_b);
        return;
    }
    if(request == 20)
    {
        // A custom key (AD's item_keyx: key 1, the gold key's model) taken: the HUD's key and its skin.
        entity k = spawn();
        k.classname = "item_keyx";
        k.mdl = "progs/w_g_key.mdl";
        k.spawnflags = ad_ITEM_CKEY1;
        k.skin = 2;
        setorigin(k, ahead);
        self = k;
        ad_item_keyx();
        self = os;
        AD_TestTouch(k, p);
        self = p;
        AD_ArtifactsTick();
        self = os;
        bprint(sprintf("adrules: custom key %g name %s hud %g keys %g\n", (p.ad_moditems & ad_IT_CKEY1) != 0,
            p.ad_ckeyname1, p.vr_ad_hud & 7680, p.vr_ad_hudkeys));
        return;
    }
    if(request == 14)
    {
        // AD's burning on the player (as a pyro's flame): our fire, AD's time and damage (15 reports).
        entity pyro = spawn();
        pyro.classname = "ad_test_pyro";
        self = pyro;
        ad_BurnDeBuff(p);
        self = os;
        remove(pyro);
        vr_ad_test_health = p.health;
        bprint(sprintf("adrules: burning %g ad %g for %.1f s; ad counters %g %g\n", AD_Burning(p),
            p.burn_fire.vr_ad_burn, p.burn_fire.burn_end - time, p.ad_burning, p.ad_burning_qty));
        return;
    }
    if(request == 15)
    {
        bprint(sprintf("adrules: burnt %g health lost; burning %g hud %g\n", vr_ad_test_health - p.health, AD_Burning(p),
            p.vr_ad_hud));
        return;
    }
    if(request == 16)
    {
        // AD's poison (17 reports).
        ad_PoisonDeBuff(p);
        vr_ad_test_health = p.health;
        bprint(sprintf("adrules: poisoned %g for %g\n", p.ad_poisoned, p.ad_poisoned_qty));
        return;
    }
    if(request == 17)
    {
        bprint(sprintf("adrules: poison %g health lost; poisoned %g hud %g\n", vr_ad_test_health - p.health,
            p.ad_poisoned, p.vr_ad_hud));
        return;
    }
    if(request == 18)
    {
        // AD's bleeding (19 reports): the health down to a quarter at once, given back over time.
        vr_ad_test_health = p.health;
        entity claw = spawn();
        self = claw;
        ad_BleedDeBuff(p);
        self = os;
        remove(claw);
        bprint(sprintf("adrules: bleeding %g health %g -> %g, %g to come back\n", p.ad_bleeding, vr_ad_test_health,
            p.health, p.ad_bleeding_qty));
        return;
    }
    if(request == 19)
    {
        bprint(sprintf("adrules: bled health %g (was %g); bleeding %g hud %g\n", p.health, vr_ad_test_health,
            p.ad_bleeding, p.vr_ad_hud));
        return;
    }
    if(request == 21)
    {
        // Ammo resistance: one of our grunts given AD's keys (shells 0.5, nails 1, rockets -0.5), hit 10 by a shotgun's
        // pellets, a nail, a blow, a rocket, a bolt; then AD_Resist on a stand-in for one of AD's monsters (nails 0.5)
        // and on ours with the same key: AD maps resist on both, elsewhere only AD's.
        entity g = AD_TestSpawn("monster_army", monster_army, ahead);
        g.health = 200;
        g.takedamage = DAMAGE_AIM;
        g.ad_resist_shells = 0.5;
        g.ad_resist_nails = 1;
        g.ad_resist_rockets = -0.5;
        entity nail = spawn();
        nail.classname = "spike";
        entity rocket = spawn();
        rocket.classname = "missile";
        float h = g.health;
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', v_forward);
        T_Damage(g, p, p, 10);
        VR_ClearHitKind();
        float dShot = h - g.health;
        h = g.health;
        T_Damage(g, nail, p, 10);
        float dNail = h - g.health;
        h = g.health;
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, '0 0 0', v_forward);
        T_Damage(g, p, p, 10);
        VR_ClearHitKind();
        float dBlow = h - g.health;
        h = g.health;
        T_Damage(g, rocket, p, 10);
        float dRocket = h - g.health;
        h = g.health;
        vr_ad_ammo = AD_AMMO_CELLS;
        T_Damage(g, p, p, 10);
        vr_ad_ammo = 0;
        float dBolt = h - g.health;
        entity standIn = spawn();
        standIn.ad_classtype = ad_CT_MONDKNIGHT;
        standIn.ad_resist_nails = 0.5;
        float adOne = AD_Resist(standIn, nail, p, 10, FALSE);
        standIn.ad_classtype = 0;
        float ourOne = AD_Resist(standIn, nail, p, 10, FALSE);
        bprint(sprintf("adrules: resist rules %g shot %g nail %g blow %g rocket %g bolt %g;", AD_Rules(),
            dShot, dNail, dBlow, dRocket, dBolt));
        bprint(sprintf(" AD's %g ours %g\n", adOne, ourOne));
        remove(nail);
        remove(rocket);
        remove(standIn);
        g.health = -1;
        VR_Scene_Remove(g);
        return;
    }
    if(request == 22)
    {
        // Evil: what plays now.
        self = p;
        float axe = AD_AxeScale(cVR_MainHand);
        self = os;
        bprint(sprintf("adrules: evil %g on %g cvar %g skill %g adskill %g", AD_Evil(), vr_ad_evil_on, cvar("vr_ad_evil"),
            skill, ad_skill));
        bprint(sprintf(" health %g/%g mega %g axe %g\n", p.health, p.max_health, VR_MegaHealthCap(p), axe));
        return;
    }
    if(request == 23)
    {
        // AD's skill choices: Evil (4) then Normal (1), as AD's hub sets them.
        VR_AD_SetSkill(4);
        float s4 = cvar("skill"), e4 = cvar("vr_ad_evil");
        VR_AD_SetSkill(1);
        bprint(sprintf("adrules: setskill 4 -> skill %g evil %g; 1 -> skill %g evil %g\n", s4, e4, cvar("skill"),
            cvar("vr_ad_evil")));
        return;
    }
    if(request == 24)
    {
        // Ladders: the map's trigger_ladders, the first one's volume; the player stood at its foot facing it, and the
        // engine's hold for a hand in its middle and one out of it (vr_climb_try).
        float n = 0;
        entity first = world;
        for(entity l = find(world, classname, "trigger_ladder"); l; l = find(l, classname, "trigger_ladder"))
        {
            n = n + 1;
            if(!first) first = l;
        }
        if(!first)
        {
            bprint("adrules: ladders 0\n");
            return;
        }
        vector mid = (first.absmin + first.absmax) * 0.5;
        bprint(sprintf("adrules: ladders %g first %v..%v climbable %g solid %g\n", n, first.absmin, first.absmax,
            first.vr_climb_ladder, first.solid));
        localcmd(sprintf("vr_climb_try %g %g %g main\n", mid_x, mid_y, mid_z));
        localcmd(sprintf("vr_climb_try %g %g %g main\n", mid_x, mid_y, first.absmax_z + 64));
        return;
    }
    if(request == 27)
    {
        // Crouching: where monsters (ours, AD's) look for the player now.
        bprint(sprintf("adrules: sight crouched %g view_ofs %v ours %v AD's %v head %v\n",
            VRHasBit(p.vrbits0, QVR_VRBITS0_CROUCHED), p.view_ofs, VR_SightOfs(p), VR_AD_ViewOfs(p), p.headpos - p.origin));
        return;
    }
    if(request == 26)
    {
        // Spawnflags: the map's ruleset; a stand-in for one of our monsters with AD's 64|128|32, one of our items with
        // AD's 16|64, one of AD's artifacts with 16: whose meaning each reads.
        entity mon = spawn();
        mon.spawnflags = 64 | 128 | 32;
        entity item = spawn();
        item.spawnflags = 16 | 64;
        entity art = spawn();
        art.spawnflags = 16;
        art.ad_classtype = ad_CT_ARTJUMPBOOTS;
        bprint(sprintf("adrules: spawnflags map %g other %g; ours delayed %g angry %g nofx %g; item respawn %g startoff %g;",
            VR_SF_MapRuleset(), VR_OtherFlags(), VR_SF_Has(mon, VR_SF_MON_DELAYED), VR_SF_Has(mon, VR_SF_MON_ANGRY),
            VR_SF_Has(mon, VR_SF_MON_NOSPAWNFX), VR_SF_Has(item, VR_SF_ITEM_RESPAWN), VR_SF_Has(item, VR_SF_STARTOFF)));
        bprint(sprintf(" AD's artifact %g respawn %g\n", VR_SF_Ruleset(art), VR_SF_Has(art, VR_SF_ITEM_RESPAWN)));
        remove(mon);
        remove(item);
        remove(art);
        return;
    }
    if(request == 25)
    {
        bprint(sprintf("adrules: player at %v onground %g velocity %v hands %v %v\n", p.origin,
            (p.flags & FL_ONGROUND) != 0, p.velocity, p.handpos, p.offhandpos));
        bprint(sprintf("adrules: eyes view_ofs %v head %v crouched %g box %v..%v\n", p.view_ofs, p.headpos - p.origin,
            VRHasBit(p.vrbits0, QVR_VRBITS0_CROUCHED), p.mins, p.maxs));
        return;
    }
    bprint(sprintf("adrules: unknown test %g\n", request));
};
