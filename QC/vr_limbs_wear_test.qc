// vr_limbs_wear_test.qc -- a corpse's pieces' own health (vr_limbs.qc, "Corpse pieces' own health"; VR_Limb_CorpseWear).
//
// `vr_limb_test 30..38` (vr_limbs_test.qc hands them here; after the AD bridge, whose pellet hit 35 uses): on the nearest
// ragdolled corpse with limbs, its test limb (VR_Limb_TestBone) hit by one weapon through its real hit code, again and
// again in one frame, until it comes off (at most 80 hits): 30 shotgun blasts, 31 super shotgun blasts, 32 lightning
// bolts, 33 nails, 34 rifle lasers, 35 AD projectile pellets, 36 axe blows (a blade's blow that does not cut), 37 shotgun
// blasts at the torso, 38 at the head. Meant with the pop chances 0 (vr_limbs_chance_scale 0; vr_decap_chance_scale 0):
// only the wear takes a piece off. PASS: 30-36 the limb off alone (38 the head), the whole body's health untouched, no
// gib; 37 gibbed, nothing worn into a piece.
// `limbtest: wear` lines (Misc/quakevr/ragdoll/limb_wear_test.sh).

vector vr_wtest_from; // (VR_Limb_WearTestAim: where the hits come from)
vector vr_wtest_dir;  // (and their way)

// A clear way into `m`'s piece `xPiece` (a joint; -1 the torso) at `xAt`: from 24 units off, the first trace that strikes
// it there. FALSE: none.
float(entity m, vector xAt, float xPiece) VR_Limb_WearTestAim =
{
    for(float up = 0; up < 3; up++)
    {
        for(float a = 0; a < 16; a++)
        {
            makevectors('0 1 0' * a * 22.5);
            vector from = xAt + normalize(v_forward + '0 0 1' * up) * 24;
            vector dir = normalize(xAt - from);
            traceline(from, xAt + dir * 8, MOVE_HITMODEL | MOVE_HITGIBS, self);
            if(trace_ent == m && !trace_startsolid && VR_Limb_WearPiece(m, trace_endpos) == xPiece)
            {
                vr_wtest_from = from;
                vr_wtest_dir = dir;
                return TRUE;
            }
        }
    }
    return FALSE;
};

// One hit of test `xTest`'s weapon along the aimed way (self the player).
void(float xTest, entity m) VR_Limb_WearTestHit =
{
    vector from = vr_wtest_from;
    vector dir = vr_wtest_dir;
    vector at = from + dir * 24;
    VR_ClearHitKind();
    if(xTest == 30 || xTest == 31 || xTest == 37 || xTest == 38)
    {
        float ssg = xTest == 31;
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
        vr_decap_weapon = ssg ? QVR_DECAP_SSG : QVR_DECAP_SHOTGUN;
        self.damage_weapon = IT_SHOTGUN;
        vr_pellet_damage = cvar_hget(ssg ? cvarh_vr_dmg_super_shotgun : cvarh_vr_dmg_shotgun);
        vr_tracer_hand = cVR_MainHand;
        vr_tracer_from = from;
        FireBulletsImpl(ssg ? 14 : 6, dir, '0 0 0', from, 4096, FALSE);
        vr_pellet_damage = 0;
        vr_decap_weapon = 0;
    }
    else if(xTest == 32)
    {
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
        vr_decap_weapon = QVR_DECAP_LIGHTNING;
        self.damage_weapon = IT_LIGHTNING;
        LightningDamage(from, at + dir * 40, self, cvar_hget(cvarh_vr_dmg_lightning));
        vr_decap_weapon = 0;
    }
    else if(xTest == 33 || xTest == 34 || xTest == 35)
    {
        entity os = self;
        entity s;
        if(xTest == 33)
        {
            s = launch_spike(at - dir * 4, dir);
        }
        else
        {
            s = spawn();
            s.owner = self;
            s.movetype = MOVETYPE_FLYMISSILE;
            s.solid = SOLID_BBOX;
            s.velocity = dir * 1000;
            s.angles = vectoangles(dir);
            s.think = SUB_Remove;
            s.nextthink = time + 0.1;
            setsize(s, '0 0 0', '0 0 0');
            setorigin(s, at - dir * 4);
            if(xTest == 34)
            {
                s.classname = "rifle_laser";
                s.vr_laser_damage = 18;
            }
            else
            {
                s.classname = "proj_bullet"; // AD support: an AD projectile pellet (vr_ad_bridge.qc VR_AD_PelletHit)
                s.vr_ad_impactdir = dir;
                s.vr_ad_launchfrom = from;
            }
        }
        if(xTest == 35)
        {
            VR_AD_PelletHit(m, s, 4); // AD support: one pellet of no blast (vr_ad_shotserial 0)
            remove(s);
        }
        else
        {
            other = m;
            self = s;
            if(xTest == 33)
            {
                spike_touch();
            }
            else
            {
                VR_RifleLaser_Touch();
            }
            self = os;
        }
    }
    else if(xTest == 36)
    {
        float oldWeapon = VRGetEntWeapon(self, cVR_MainHand);
        VRSetEntWeapon(self, cVR_MainHand, WID_AXE);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
        T_Damage(m, self, self, 20);
        VRSetEntWeapon(self, cVR_MainHand, oldWeapon);
    }
    VR_ClearHitKind();
};

void(float xTest) VR_Limb_WearTest =
{
    entity m = VR_Limb_TestTarget(0);
    if(m == world || !isragdoll(m))
    {
        bprint(sprintf("limbtest: wear %g: no ragdolled corpse with limbs near FAIL\n", xTest));
        return;
    }
    float torso = xTest == 37;
    float head = xTest == 38;
    float b = head ? 0 : VR_Limb_TestBone(m);
    float piece = torso ? -1 : b;
    vector at = head ? ragdollcut(m, 6) : limbplace(m, torso ? 0 : b, 3);
    if(!VR_Limb_WearTestAim(m, at, piece))
    {
        bprint(sprintf("limbtest: wear %g: no clear way into %s's piece %g FAIL\n", xTest, m.classname, piece));
        return;
    }
    float cuts = vr_limb_cuts;
    float pops = vr_limb_pops;
    float hpBefore = m.vr_corpse_hp;
    float dmgBefore = 0; // (the test limb's worn damage; 37: every piece's)
    for(float j = 0; j < 16; j++)
    {
        if(torso || j == b)
        {
            dmgBefore = dmgBefore + m.vr_limb_corpse_dmg[j];
        }
    }
    float hits = 0;
    // (Until something comes off or it is gibbed: a gib throws its limbs too, VR_Limb_GibAll, counted as cuts.)
    float headless = m.vr_headless;
    while((hits < 80) && !m.vr_gib && m.vr_corpse && (vr_limb_cuts == cuts) && (vr_limb_pops == pops) &&
          (m.vr_headless == headless))
    {
        VR_Limb_WearTestHit(xTest, m);
        hits++;
    }
    float off = (vr_limb_cuts - cuts) + (vr_limb_pops - pops);
    float worn = 0;
    float ok = FALSE;
    if(torso)
    {
        for(float i = 0; i < 16; i++)
        {
            worn = worn + m.vr_limb_corpse_dmg[i]; // (none worn into a piece: the torso's blasts are the whole body's)
        }
        worn = worn - dmgBefore;
        if((m.vr_gib || !m.vr_corpse || (m.vr_corpse_hp <= 0)) && (worn == 0))
        {
            ok = TRUE;
        }
    }
    else
    {
        worn = m.vr_limb_corpse_dmg[b];
        float came = head ? (m.vr_headless && !headless && (off == 0)) : (off == 1); // (a head's pop is no limb's)
        if(!m.vr_gib && m.vr_corpse && (m.vr_corpse_hp == hpBefore) && came && (worn > dmgBefore))
        {
            ok = TRUE;
        }
    }
    bprint(sprintf("limbtest: wear %g %s on %s joint %g: %g hits, cut %g popped %g", xTest, ok ? "PASS" : "FAIL",
        m.classname, piece, hits, vr_limb_cuts - cuts, vr_limb_pops - pops));
    bprint(sprintf(", worn %.1f, hp %.1f -> %.1f, gib %g\n", worn, hpBefore, m.vr_corpse_hp, m.vr_gib));
};
