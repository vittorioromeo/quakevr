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

void(float request, entity p) AD_RulesTest =
{
    makevectors(p.angles);
    vector ahead = p.origin + v_forward * 64;
    entity os = self;
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
    bprint(sprintf("adrules: unknown test %g\n", request));
};
