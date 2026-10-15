// vr_ragimpact.qc -- impacts and manhandling (Combat > Knockdowns, "Impacts"; Combat > Holding Enemies, "Manhandle";
// ROUND22.md, "Impacts and manhandling"). The author, 2026-10-14: slamming or throwing a lifted living enemy from height
// should hurt it meaningfully; landing on one from a height crushes the part under you; a held head smashed into a wall
// kills, and heads and limbs struck again and again pop.
//
// - Impacts (vr_ragimpact): the engine (vr_box3d.cpp noteRagImpacts) gives each ragdoll's hardest strike on the level (the
//   world, a door, a fixture) in a frame: the part's speed (m/s; no faster than vr_ragimpact_whip times its body's) and
//   the mass behind it (kg: the part's, plus vr_ragimpact_body of the rest). Damage: vr_ragimpact_damage x that mass x
//   (its speed - vr_ragimpact_min_speed), times vr_ragimpact_head on the head. Who counts: a living monster down as a
//   ragdoll (knocked down, thrown, lifted, held); a corpse held by a player, or let go of by one within
//   vr_ragimpact_time. A burst of strikes (a bounce, its parts one after another) counts once a VR_RIMP_GAP.
// - The living: the damage to its health (T_Damage: its pain, its death), and worn into the head or limb struck (the
//   corpse pieces' own damage, vr_limb_corpse_dmg: one count for the living and the dead, vr_limbs.qc VR_Limb_WearInto).
//   A piece's own health: its corpse health (vr_corpse_health_<monster>) times vr_ragimpact_piece_hp. A head worn past it
//   pops, and it dies; a limb worn past it pops when it dies (a strike that kills it), or at the next strike after.
// - The dead: worn into the head or limb struck as any weapon's hit on a corpse's piece (VR_Limb_WearInto: past its
//   health, popped). Its torso: nothing (the body stays whole).
// - Landing crush (vr_ragimpact_crush): you landing at vr_ragimpact_crush_speed or faster (client.qc's landing) on a living
//   ragdoll: the part under your feet struck as above, vr_ragimpact_crush_mass kg of you behind your feet at the speed you
//   landed at (its least speed vr_ragimpact_crush_speed's), times vr_ragimpact_crush.
// - Manhandling (vr_foegrab_manhandle_*; the engine's vr_foegrab.cpp manhandle): a hand taking hold of a humanoid that
//   was staggered within vr_foegrab_manhandle_stagger s (a shove landed on it, a hit or throw unbalanced it, it recovered
//   from a stagger: .vr_kd_shaken) or is at vr_foegrab_manhandle_health of its health or less knocks it down, held in the
//   hand as a lifted one (VR_FoeGrab_Manhandle). A staggering ragdoll a hand takes goes down (the engine's grabRagdoll).
// - vr_knockdown_debug 1 prints each strike dealt (2: the engine's every strike); vr_knockdown_test 50..57 test it
//   (VR_RagImpact_Test).

float VR_RIMP_GAP = 0.15; // s: strikes on one piece this close together count as one (the first: the hardest of its frame)
.float vr_rimp_next[17];  // until when a strike on each piece does not count again (the torso 0, the head 1, a limb its joint + 1)
float vr_rimp_test;       // in a test (VR_RagImpact_Test): each strike printed whatever vr_knockdown_debug says
float vr_rimp_dealt;      // the tests: strikes dealt, and the damage in all
float vr_rimp_total;

// A shove landed on `e`, a hit or a throw unbalanced it, or it recovered from a stagger: staggered now (manhandling).
void(entity e) VR_Manhandle_Shaken =
{
    e.vr_kd_shaken = time;
};

// The engine (vr_foegrab.cpp manhandle): hand xHand of the player self just took hold of `m` (a humanoid: the lift's
// classes): staggered within vr_foegrab_manhandle_stagger s or weak (at vr_foegrab_manhandle_health of its health or
// less), it is knocked down where it stands, its get-up held off xHold s (the engine holds it as a lifted one). Returns 1
// knocked down (staggered), 2 knocked down (weak), 0 not.
float(entity m, float xHand, float xHold) VR_FoeGrab_Manhandle =
{
    if(m.health <= 0 || m.vr_knockdown || !m.vr_kd_chance_h || (m.flags & (FL_FLY | FL_SWIM)) || !(m.flags & FL_MONSTER) ||
       VR_IsDummy(m) || !canragdoll(m))
    {
        return 0;
    }
    float full = m.max_health > 0 ? m.max_health : m.health;
    float window = cvar_hget(cvarh_vr_foegrab_manhandle_stagger);
    float weak = cvar_hget(cvarh_vr_foegrab_manhandle_health);
    float why = 0;
    if(window > 0 && m.vr_kd_shaken > 0 && time - m.vr_kd_shaken <= window)
    {
        why = 1;
    }
    else if(weak > 0 && m.health <= full * weak)
    {
        why = 2;
    }
    if(!why || !VR_Knockdown_MakeRoom(m))
    {
        return 0;
    }
    vector was = m.velocity;
    m.velocity = '0 0 0';
    if(!VR_Knockdown_Start(m))
    {
        m.velocity = was;
        return 0;
    }
    m.vr_kd_until = max(m.vr_kd_until, time + xHold);
    haptic(xHand, 0, 0.12, 50, 0.7);
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("manhandle: %s (health %g of %g, staggered %.2f s ago): knocked down, %s\n", m.classname, m.health,
            full, m.vr_kd_shaken > 0 ? time - m.vr_kd_shaken : -1, why == 1 ? "staggered" : "weak"));
    }
    return why;
};

// The player who drives ragdoll `m` now: one holding it, else one who let go of it within vr_ragimpact_time;
// world: none.
entity(entity m) VR_RagImpact_By =
{
    for(entity p = find(world, classname, "player"); p != world; p = find(p, classname, "player"))
    {
        if(VR_Carry_Held(p, cVR_MainHand) == m || VR_Carry_Held(p, cVR_OffHand) == m)
        {
            return p;
        }
    }
    if(m.vr_rag_letgo != world && time - m.vr_rag_letgo_time <= max(0, cvar_hget(cvarh_vr_ragimpact_time)))
    {
        return m.vr_rag_letgo;
    }
    return world;
};

// The piece of `m` a strike at `xAt` wears: 0 its head, a limb's joint, -1 the torso (or none it may lose).
float(entity m, vector xAt) VR_RagImpact_Piece =
{
    if(m.health <= 0)
    {
        // (A corpse's: as any weapon's hit wears it, VR_Limb_CorpseWear's.)
        float p = VR_Limb_WearPiece(m, xAt);
        if(p > 0 && !(VR_Limb_Allowed(m) && cvar_hget(cvarh_vr_limbs_corpses)))
        {
            return -1;
        }
        return p;
    }
    if(VR_Decap_Allowed(m) && ragdollhead(m, xAt, 0) > 0)
    {
        return 0;
    }
    if(!VR_Limb_Allowed(m))
    {
        return -1;
    }
    float bone = VR_Limb_At(m, xAt);
    return (bone > 0 && bone < 16) ? bone : -1;
};

// A strike of `xDamage` (the head's multiplier in) at `xAt` on piece `xPiece` of `m` (VR_RagImpact_Piece), by `xBy`
// (world: none). xWhat: for the prints.
void(entity m, entity xBy, vector xAt, float xPiece, float xDamage, string xWhat) VR_RagImpact_Deal =
{
    VR_Corpse_Parts(m);
    float hp = max(1, VR_Corpse_Health() * max(0, cvar_hget(cvarh_vr_ragimpact_piece_hp)));
    vector mid = (m.absmin + m.absmax) * 0.5;
    vector vel = normalize(xAt - mid) * 200; // (a pop's burst: its blood and bits go out from the body)
    vr_rimp_dealt = vr_rimp_dealt + 1;
    vr_rimp_total = vr_rimp_total + xDamage;
    string piece = xPiece == 0 ? "head" : (xPiece > 0 ? sprintf("limb %g", xPiece) : "torso");
    float loud = vr_rimp_test || cvar_hget(cvarh_vr_knockdown_debug);
    if(m.health <= 0)
    {
        // A corpse: its head or limb worn as any weapon's hit wears it (past its health: popped). Its torso: nothing.
        if(loud)
        {
            dprint(sprintf("ragimpact: %s corpse, %s: %s, %.1f into its %s\n", m.classname, xWhat, xBy.classname, xDamage,
                piece));
        }
        if(xPiece >= 0)
        {
            VR_Limb_WearInto(m, xBy, xPiece, xDamage, vel, QVR_DECAP_BLUNT, hp, "impact");
        }
        return;
    }
    float worn = 0;
    if(xPiece >= 0)
    {
        m.vr_limb_corpse_dmg[xPiece] = m.vr_limb_corpse_dmg[xPiece] + xDamage;
        worn = m.vr_limb_corpse_dmg[xPiece];
    }
    float dmg = xDamage;
    string pops = "";
    if(xPiece == 0 && worn >= hp)
    {
        // Its head worn through: it pops, and it dies.
        if(VR_Decap_Arm(m, xBy, xAt, vel, QVR_DECAP_BLUNT))
        {
            vr_decap_chance = 1000; // (sure: past its health, whatever the pop chances)
            vr_decap_why = "its head worn through by impacts";
            pops = ", its head pops";
        }
        dmg = max(dmg, m.health + 1);
    }
    else if(xPiece > 0 && worn >= hp && dmg >= m.health && VR_Limb_Arm(m, xBy, xAt, vel, QVR_DECAP_BLUNT, xPiece))
    {
        vr_decap_chance = 1000;
        vr_decap_why = "a limb worn through by impacts";
        pops = ", the limb pops";
    }
    if(loud)
    {
        dprint(sprintf("ragimpact: %s (health %g), %s: %.1f damage", m.classname, m.health, xWhat, dmg));
        dprint(sprintf(" to its %s (worn %.1f of %.1f)%s\n", piece, worn, hp, pops));
    }
    float oldKind = vr_hitkind;
    vr_hitkind = QVR_HITKIND_NONE;
    T_Damage(m, world, xBy, dmg);
    vr_hitkind = oldKind;
};

// The engine (vr_box3d.cpp callRagImpacts): ragdoll `m`'s hardest strike on the level this frame at `xAt`, its part at
// xSpeed m/s (xRaw before the whip's cap), xMass kg behind it; xHeld: a hand holds the part (its least speed halved).
void(entity m, vector xAt, float xSpeed, float xMass, float xRaw, float xHeld) VR_RagImpact_Hit =
{
    if(!cvar_hget(cvarh_vr_ragimpact) || !(m.flags & FL_MONSTER) || m.vr_gib || !isragdoll(m))
    {
        return;
    }
    // (Driven by you only: one you hold or threw. One falling over by itself, a shove's knockdown, takes only its fall.)
    entity by = VR_RagImpact_By(m);
    if(by == world || (m.health > 0 && (m.vr_knockdown != 1 || !m.takedamage)))
    {
        return;
    }
    float piece = VR_RagImpact_Piece(m, xAt);
    float slot = piece + 1; // (the torso 0, the head 1, a limb its joint + 1)
    if(time < m.vr_rimp_next[slot])
    {
        return;
    }
    float least = xHeld ? max(0.25, cvar_hget(cvarh_vr_ragimpact_held_speed)) :
                          max(0.5, cvar_hget(cvarh_vr_ragimpact_min_speed));
    float dmg = max(0, cvar_hget(cvarh_vr_ragimpact_damage)) * xMass * max(0, xSpeed - least);
    if(piece == 0)
    {
        dmg = dmg * max(0, cvar_hget(cvarh_vr_ragimpact_head));
    }
    if(dmg < 0.5)
    {
        return;
    }
    m.vr_rimp_next[slot] = time + VR_RIMP_GAP;
    VR_RagImpact_Deal(m, by, xAt, piece, dmg, sprintf("struck at %.1f m/s (%.1f raw)%s, %.1f kg behind it", xSpeed, xRaw,
        xHeld ? " held" : "", xMass));
};

// You (`p`) landed at xSpeed units/s (client.qc's landing, PlayerPostThink): a living ragdoll's part under your feet is
// crushed (vr_ragimpact_crush).
void(entity p, float xSpeed) VR_RagImpact_Crush =
{
    float k = max(0, cvar_hget(cvarh_vr_ragimpact_crush));
    float least = cvar_hget(cvarh_vr_ragimpact_crush_speed);
    if(!cvar_hget(cvarh_vr_ragimpact) || k <= 0 || xSpeed < least)
    {
        return;
    }
    vector feet = p.origin + '0 0 1' * p.mins_z;
    float half = max(p.maxs_x, p.maxs_y) + 8; // (its part's middle within your feet's square, a little more)
    entity best = world;
    vector bestAt = '0 0 0';
    float bestDist = 1000000;
    for(entity m = findradius(feet, 128); m != world; m = m.chain)
    {
        if(!(m.flags & FL_MONSTER) || m.health <= 0 || m.vr_knockdown != 1 || !m.takedamage || !isragdoll(m))
        {
            continue;
        }
        float bone = ragdollbone(m, feet);
        if(bone < 0)
        {
            continue;
        }
        vector at = ragdollpoint(m, bone, '0 0 0', 1);
        vector off = at - feet;
        if(fabs(off_x) > half || fabs(off_y) > half || off_z < -24 || off_z > 40)
        {
            continue;
        }
        if(vlen(off) < bestDist)
        {
            bestDist = vlen(off);
            best = m;
            bestAt = at;
        }
    }
    if(best == world)
    {
        if(vr_rimp_test)
        {
            dprint(sprintf("ragimpact: landed at %.0f u/s: no living ragdoll under your feet\n", xSpeed));
        }
        return;
    }
    float m2u = max(1, VRMetersToUnits());
    float speed = xSpeed / m2u;
    float dmg = max(0, cvar_hget(cvarh_vr_ragimpact_damage)) * max(0, cvar_hget(cvarh_vr_ragimpact_crush_mass)) *
                max(0, speed - least / m2u) * k;
    float piece = VR_RagImpact_Piece(best, bestAt);
    if(piece == 0)
    {
        dmg = dmg * max(0, cvar_hget(cvarh_vr_ragimpact_head));
    }
    if(dmg < 0.5)
    {
        return;
    }
    sound(best, CHAN_AUTO, "player/udeath.wav", min(1, dmg / 40), ATTN_NORM);
    VR_RagImpact_Deal(best, p, bestAt, piece, dmg, sprintf("crushed, you landing at %.1f m/s", speed));
};

// For tests (vr_knockdown_test 50..57; self the player), the nearest monster down as a ragdoll, alive (dead: 57): 50 its
// state (health, its head's and limbs' worn damage, the strikes dealt so far), 51 flung away from you at
// vr_test_throw_up_speed m/s and 3 m/s up (into a wall), 52 you put 256 units over its middle (a two-storey drop onto
// it), 53 the same 96 units (a short hop), 54 the nearest standing monster made staggered now (.vr_kd_shaken), 55 its
// health set to a fifth, 56 strikes printed from now (57: off again).
void(float xMode) VR_RagImpact_Test =
{
    if(xMode == 56 || xMode == 57)
    {
        vr_rimp_test = xMode == 56;
        dprint(sprintf("ragimpact test: prints %s\n", vr_rimp_test ? "on" : "off"));
        return;
    }
    entity best = world;
    float bestDist = 100000;
    for(entity m = nextent(world); m; m = nextent(m))
    {
        if(!(m.flags & FL_MONSTER) || vlen(m.origin - self.origin) >= bestDist)
        {
            continue;
        }
        float down = m.health > 0 && m.vr_knockdown == 1;
        if((xMode == 54 || xMode == 55 || xMode == 58) ? (m.health > 0 && !m.vr_knockdown) : (xMode == 50 ? isragdoll(m) || m.health > 0 : down))
        {
            best = m;
            bestDist = vlen(m.origin - self.origin);
        }
    }
    if(best == world)
    {
        dprint("ragimpact test: no monster\n");
        return;
    }
    if(xMode == 50)
    {
        float limbs = 0;
        for(float i = 1; i < 16; i++)
        {
            limbs = max(limbs, best.vr_limb_corpse_dmg[i]);
        }
        dprint(sprintf("ragimpact state: %s health %g knockdown %g headless %g head worn %.1f, most on a limb %.1f",
            best.classname, best.health, best.vr_knockdown, best.vr_headless, best.vr_limb_corpse_dmg[0], limbs));
        dprint(sprintf("; strikes %g, %.1f damage; decaps %g, limb pops %g", vr_rimp_dealt, vr_rimp_total,
            vr_decap_count, vr_limb_pops));
        float head = ragdolllimbs(best, 3);
        vector at = (isragdoll(best) && head >= 0) ? ragdollpoint(best, head, '0 0 0', 1) : best.origin;
        dprint(sprintf("; its head at %v, its feet at %.0f; your main hand at %v\n", at, best.absmin_z,
            VRGetHandPos(cVR_MainHand)));
        return;
    }
    if(xMode == 51)
    {
        // Towards the nearest wall level with its middle (8 ways looked at), as if you threw it (its letting go yours).
        vector from = (best.absmin + best.absmax) * 0.5;
        vector dir = '0 0 0';
        float near = 2;
        for(float k = 0; k < 8; k++)
        {
            makevectors('0 1 0' * k * 45);
            traceline(from, from + v_forward * 512, TRUE, best);
            if(trace_fraction < near)
            {
                near = trace_fraction;
                dir = v_forward;
            }
        }
        if(dir == '0 0 0')
        {
            dir = best.origin - self.origin;
            dir_z = 0;
            dir = normalize(dir);
        }
        float speed = cvar("vr_test_throw_up_speed") * VRMetersToUnits();
        best.velocity = dir * speed + '0 0 1' * 3 * VRMetersToUnits();
        best.vr_rag_letgo = self;
        best.vr_rag_letgo_hand = 0;
        best.vr_rag_letgo_time = time;
        dprint(sprintf("ragimpact test: %s flung at %.1f m/s at a wall %.0f units away\n", best.classname,
            cvar("vr_test_throw_up_speed"), near * 512));
        return;
    }
    if(xMode == 52 || xMode == 53)
    {
        float up = xMode == 52 ? 256 : 96;
        vector mid = (best.absmin + best.absmax) * 0.5;
        setorigin(self, mid + '0 0 1' * (up - self.mins_z));
        self.velocity = '0 0 0';
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        dprint(sprintf("ragimpact test: you %g units over %s\n", up, best.classname));
        return;
    }
    if(xMode == 58)
    {
        // Moved to 48 units off the nearest wall level with its middle (8 ways, within 512), its feet where they were.
        vector mid = (best.absmin + best.absmax) * 0.5;
        float wall = 2;
        vector way = '0 0 0';
        for(float w = 0; w < 8; w++)
        {
            makevectors('0 1 0' * w * 45);
            traceline(mid, mid + v_forward * 512, TRUE, best);
            if(trace_fraction < wall)
            {
                wall = trace_fraction;
                way = v_forward;
            }
        }
        if(way == '0 0 0')
        {
            dprint("ragimpact test: no wall near\n");
            return;
        }
        float move = wall * 512 - 48 - best.maxs_x;
        setorigin(best, best.origin + way * move);
        dprint(sprintf("ragimpact test: %s moved %.0f units towards a wall, 48 off it\n", best.classname, move));
        return;
    }
    if(xMode == 54)
    {
        VR_Manhandle_Shaken(best);
        dprint(sprintf("ragimpact test: %s staggered now\n", best.classname));
        return;
    }
    if(xMode == 55)
    {
        float full = best.max_health > 0 ? best.max_health : best.health;
        best.health = max(1, rint(full * 0.2));
        dprint(sprintf("ragimpact test: %s at %g of %g health\n", best.classname, best.health, full));
        return;
    }
};
