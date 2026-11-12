#ifdef QVR_TESTS
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
vector vr_wtest_at;   // (and where the first trace met the piece: a punch's place)

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
                vr_wtest_at = trace_endpos;
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
    VR_Hit_End();
    if(xTest == 30 || xTest == 31 || xTest == 37 || xTest == 38)
    {
        float ssg = xTest == 31;
        VR_Hit_Begin(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
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
        VR_Hit_Begin(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
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
    else if(xTest >= 41 && xTest <= 44)
    {
        // His note 2026-11-10 ("using fists on a ragdoll's head over and over causes the whole ragdoll to gib"): a
        // punch at 7 m/s, through the hand's corpse strike (41, 42: VR_Corpse_Struck, VR_Corpse_StrikeFrame's) or the
        // hand's blow swept into it (43, 44: T_Damage_VRMelee, VR_Melee_Sweep's on a corpse it reaches).
        float wasWeapon = VRGetEntWeapon(self, cVR_MainHand);
        entity wasInst = VRGetEntWeaponInst(self, cVR_MainHand);
        VRSetEntWeapon(self, cVR_MainHand, WID_FIST);
        VRSetEntWeaponInst(self, cVR_MainHand, world);
        m.vr_corpse_struck = 0; // (its 0.35 s between strikes: the test's hits come one a frame)
        at = vr_wtest_at; // (on the piece's skin, as the hand's trace meets it)
        if(xTest <= 42)
        {
            VR_Corpse_Struck(cVR_MainHand, m, at, dir, 7, FALSE);
        }
        else
        {
            entity bl = VR_Blow(cVR_MainHand);
            bl.mh_thing = VR_MTHING_FIST;
            bl.mh_kind = VR_MKIND_PUNCH;
            bl.mh_strength = 7 / max(0.1, cvar_hget(cvarh_vr_melee_speed) * VR_MELEE_GOOD); // (7 m/s on the melee's curve)
            bl.mh_c_pos = at;
            bl.mh_c_dir = dir;
            VR_Hit_Begin(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
            T_Damage_VRMelee(m, self, self, VRMeleeDmg(cVR_MainHand, cvar_hget(cvarh_vr_dmg_fist)));
        }
        VRSetEntWeapon(self, cVR_MainHand, wasWeapon);
        VRSetEntWeaponInst(self, cVR_MainHand, wasInst);
    }
    else if(xTest == 36)
    {
        float oldWeapon = VRGetEntWeapon(self, cVR_MainHand);
        VRSetEntWeapon(self, cVR_MainHand, WID_AXE);
        // (On the piece's skin, where the aim's trace met it and read it as the piece: `at`, the point on the bone
        // inside, reads as the next joint since the plate-aware lookup (ROUND23), and wore that one instead.)
        at = vr_wtest_at;
        VR_Hit_Begin(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
        T_Damage(m, self, self, 20);
        VRSetEntWeapon(self, cVR_MainHand, oldWeapon);
    }
    VR_Hit_End();
};

// Test 45 (his note 2026-11-10, "it should work for every limb"): every limb of the ragdoll in turn, then its head,
// punched (test 42's and 41's strike) until it comes off (at most 60 each). PASS: each comes off alone, the whole body's
// health untouched, never gibbed.
void(entity m) VR_Limb_WearTestEvery =
{
    float hpBefore = m.vr_corpse_hp;
    float ok = TRUE;
    float pieces = 0;
    float hitsAll = 0;
    for(float k = 0; k <= VR_LIMB_JOINTS; k++)
    {
        float piece = (k == VR_LIMB_JOINTS) ? 0 : k; // (the head last: off, the ragdoll still lies whole)
        if((k == 0) || ((piece > 0) && !(ragdolllimbs(m, 0) & VR_AgArmor_Bit(piece))) || m.vr_gib) // (not one cut off with its parent)
        {
            continue;
        }
        vector at = (piece == 0) ? ragdollcut(m, 6) : limbplace(m, piece, 3);
        if(!VR_Limb_WearTestAim(m, at, piece))
        {
            bprint(sprintf("limbtest: wear 45 joint %g: no clear way in (skipped)\n", piece));
            continue;
        }
        float cuts = vr_limb_cuts;
        float pops = vr_limb_pops;
        float headless = m.vr_headless;
        float hits = 0;
        while((hits < 60) && !m.vr_gib && m.vr_corpse && (vr_limb_cuts == cuts) && (vr_limb_pops == pops) && (m.vr_headless == headless))
        {
            VR_Limb_WearTestHit(41, m);
            hits++;
        }
        float came = (piece == 0) ? (m.vr_headless && !headless) : (((vr_limb_cuts - cuts) + (vr_limb_pops - pops)) >= 1);
        ok = (ok && came && !m.vr_gib);
        pieces++;
        hitsAll = hitsAll + hits;
        bprint(sprintf("limbtest: wear 45 joint %g: %s after %g punches, gib %g\n", piece, came ? "off" : "NOT off", hits, m.vr_gib));
    }
    ok = (ok && (pieces >= 3) && (m.vr_corpse_hp == hpBefore) && !m.vr_gib);
    bprint(sprintf("limbtest: wear 45 %s on %s: %g pieces punched off in %g punches, hp %.1f -> %.1f, gib %g\n", ok ? "PASS" : "FAIL",
        m.classname, pieces, hitsAll, hpBefore, m.vr_corpse_hp, m.vr_gib));
};

void(float xTest) VR_Limb_WearTest =
{
    entity m = VR_Limb_TestTarget(0);
    if(m == world || !isragdoll(m))
    {
        bprint(sprintf("limbtest: wear %g: no ragdolled corpse with limbs near FAIL\n", xTest));
        return;
    }
    if(xTest == 45)
    {
        VR_Limb_WearTestEvery(m);
        return;
    }
    if(xTest == 46)
    {
        // His note 2026-11-10 ("a quick melee attack with V when holding a gun also seems to gib ragdolls too easily"): a
        // shotgun swung hard (9 m/s, the flat V swing's) into its torso through the hand's corpse strike, 12 times: never
        // gibbed whole (a live grunt takes such a blow's 20 or so, never a gib).
        if(!VR_Limb_WearTestAim(m, limbplace(m, 0, 3), -1))
        {
            bprint("limbtest: wear 46: no clear way into the torso FAIL\n");
            return;
        }
        float hp0 = m.vr_corpse_hp;
        float wasW = VRGetEntWeapon(self, cVR_MainHand);
        entity wasI = VRGetEntWeaponInst(self, cVR_MainHand);
        VRSetEntWeapon(self, cVR_MainHand, WID_SHOTGUN);
        VRSetEntWeaponInst(self, cVR_MainHand, WeaponInst_New(0));
        float n = 0;
        float most = 0;
        for(n = 0; (n < 12) && !m.vr_gib && m.vr_corpse; n++)
        {
            m.vr_corpse_struck = 0;
            vr_corpse_last_hit = 0;
            VR_Corpse_Struck(cVR_MainHand, m, vr_wtest_at, vr_wtest_dir, 9, FALSE);
            most = max(most, vr_corpse_last_hit);
        }
        VRSetEntWeapon(self, cVR_MainHand, wasW);
        VRSetEntWeaponInst(self, cVR_MainHand, wasI);
        bprint(sprintf("limbtest: wear 46 %s on %s: %g gun swings at the torso, at most %.1f on its body each (overkill %g)",
            (!m.vr_gib && (n == 12)) ? "PASS" : "FAIL", m.classname, n, most, cvar_hget(cvarh_vr_corpse_gib_overkill)));
        bprint(sprintf(", hp %.1f -> %.1f, gib %g\n", hp0, m.vr_corpse_hp, m.vr_gib));
        return;
    }
    float torso = xTest == 37;
    float head = xTest == 38 || xTest == 41 || xTest == 43;
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
    for(float j = 0; j < VR_LIMB_JOINTS; j++)
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
        for(float i = 0; i < VR_LIMB_JOINTS; i++)
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
    bprint(sprintf(", worn %.1f, hp %.1f -> %.1f, gib %g, the hit's piece %g, plates %g\n", worn, hpBefore, m.vr_corpse_hp,
        m.vr_gib, VR_Limb_WearPiece(m, vr_wtest_at), m.vr_ar_mask));
};
#endif
