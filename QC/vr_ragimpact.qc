// vr_ragimpact.qc -- impacts and manhandling (Combat > Knockdowns, "Impacts"; Combat > Holding Enemies, "Manhandle";
// ROUND22.md, "Impacts and manhandling"). The author, 2026-10-14: slamming or throwing a lifted living enemy from height
// should hurt it meaningfully; landing on one from a height crushes the part under you; a held head smashed into a wall
// kills, and heads and limbs struck again and again pop.
//
// - Impacts (vr_ragimpact): the engine (vr_box3d.cpp noteRagImpacts) gives each ragdoll part's hardest strike on the
//   level (the world, a door, a fixture) in a frame: its speed (m/s; no faster than vr_ragimpact_whip times its body's,
//   unless a hand holds the part) and the mass behind it (kg: the part's, plus vr_ragimpact_body of the rest, plus
//   vr_ragimpact_held_mass when a hand holds it: your arm drives it). Damage: vr_ragimpact_damage x that mass x (its speed
//   - vr_ragimpact_min_speed, or vr_ragimpact_held_speed held), times vr_ragimpact_head on the head. Who counts: a body
//   you drive, alive (down as a ragdoll) or dead: held in your hand, or let go of (thrown, dropped; a two-hand throw, a
//   lift) within vr_ragimpact_time. One falling over by itself (a shove's knockdown) takes only its fall damage. A burst
//   of strikes on one piece (a bounce) counts once a VR_RIMP_GAP.
// - The living: the damage to its health (T_Damage: its pain, its death), and worn into the head or limb struck (the
//   corpse pieces' own damage, vr_limb_corpse_dmg: one count for the living and the dead, vr_limbs.qc VR_Limb_WearInto).
//   A piece's own health: its corpse health (vr_corpse_health_<monster>) times vr_ragimpact_piece_hp. A head worn past it
//   pops, and it dies; a limb worn past it pops when it dies (a strike that kills it), or at the next strike after.
// - The dead: worn into the head or limb struck as any weapon's hit on a corpse's piece (VR_Limb_WearInto: past its
//   health, popped). Its torso: nothing (the body stays whole).
// - Landing crush (vr_ragimpact_crush): you landing at vr_ragimpact_crush_speed or faster (client.qc's landing) on a living
//   ragdoll: the part under your feet struck as above, vr_ragimpact_crush_mass kg of you behind your feet at the speed you
//   landed at (its least speed vr_ragimpact_crush_speed's), times vr_ragimpact_crush. A jump from the same level (about
//   270 units/s) crushes: a down grunt's head pops at the second, its torso and limbs take more (2026-10-16). A ragdoll
//   crushed is pushed (vr_ragimpact_crush_push: it squashes and jolts, VR_RagImpact_CrushPush); the gib's sound only when
//   something comes off, else a body's punch and thud (VR_RagImpact_CrushSound). A down one's hits keep it down
//   (vr_knockdown.qc VR_Knockdown_Keep).
// - Stomp (vr_ragimpact_stomp; the author, 2026-10-15): a standing one that lives through the landing on its top is
//   knocked down as a shove knocks it down (it gets up again), from vr_ragimpact_stomp_speed (heavy ones more:
//   vr_ragimpact_stomp_heavy), and you bounce off it (VR_RagImpact_Stomp).
// - Manhandling (vr_foegrab_manhandle_*; the engine's vr_foegrab.cpp manhandle): a hand taking hold of a humanoid that
//   was staggered within vr_foegrab_manhandle_stagger s (a shove landed on it, a hit or throw unbalanced it, it recovered
//   from a stagger: .vr_kd_shaken) or is at vr_foegrab_manhandle_health of its health or less knocks it down, held in the
//   hand as a lifted one (VR_FoeGrab_Manhandle). A staggering ragdoll a hand takes goes down (the engine's grabRagdoll).
// - vr_knockdown_debug 1 prints each strike dealt (2: the engine's every strike); vr_knockdown_test 50..59 test it
//   (VR_RagImpact_Test); Misc/quakevr/foegrab/ragimpact_test.py runs them.

float VR_RIMP_GAP = 0.15; // s: strikes on one piece this close together count as one (the first: the hardest of its frame)
.float vr_rimp_next[17];  // until when a strike on each piece does not count again (the torso 0, the head 1, a limb its joint + 1)
float vr_rimp_test;       // in a test (VR_RagImpact_Test): each strike printed whatever vr_knockdown_debug says
float vr_rimp_dealt;      // the tests: strikes dealt, and the damage in all
float vr_rimp_total;
float vr_rimp_dents;      // and the strikes that dented (vr_ragimpact_dent)

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
    m.vr_kd_until = max(m.vr_kd_until, time + xHold * VR_KD_DownScale(m));
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

// The damage of a strike with `xMass` kg behind it at `xExcess` m/s past its least speed: vr_ragimpact_damage x mass x
// excess, bent by vr_ragimpact_curve about vr_ragimpact_curve_ref (1: linear, every curve the same at the pivot).
float(float xMass, float xExcess) VR_RagImpact_Amount =
{
    if(xExcess <= 0)
    {
        return 0;
    }
    float dmg = max(0, cvar_hget(cvarh_vr_ragimpact_damage)) * max(0, xMass) * xExcess;
    float curve = max(0.1, cvar_hget(cvarh_vr_ragimpact_curve));
    if(curve != 1)
    {
        dmg = dmg * pow(xExcess / max(0.1, cvar_hget(cvarh_vr_ragimpact_curve_ref)), curve - 1);
    }
    return dmg;
};

// `xDamage` on piece `xPiece` (0 the head, a limb's joint, -1 the torso): times the head's or a limb's multiplier, then
// capped at vr_ragimpact_max (0: none).
float(float xDamage, float xPiece) VR_RagImpact_Scale =
{
    if(xPiece == 0)
    {
        xDamage = xDamage * max(0, cvar_hget(cvarh_vr_ragimpact_head));
    }
    else if(xPiece > 0)
    {
        xDamage = xDamage * max(0, cvar_hget(cvarh_vr_ragimpact_limb));
    }
    float most = cvar_hget(cvarh_vr_ragimpact_max);
    return most > 0 ? min(most, xDamage) : xDamage;
};

// A strike of `xDamage` (the head's multiplier in) at `xAt` on piece `xPiece` of `m` (VR_RagImpact_Piece), by `xBy`
// (world: none). xShare: the share of the piece's own health it wears (a landing's crush), -1: the damage times
// vr_ragimpact_wear. xWhat: for the prints.
void(entity m, entity xBy, vector xAt, float xPiece, float xDamage, float xShare, string xWhat) VR_RagImpact_Deal =
{
    VR_Corpse_Parts(m);
    float hp = max(1, VR_Corpse_Health() * max(0, cvar_hget(cvarh_vr_ragimpact_piece_hp)));
    vector mid = (m.absmin + m.absmax) * 0.5;
    vector vel = normalize(xAt - mid) * 200; // (a pop's burst: its blood and bits go out from the body)
    string piece = xPiece == 0 ? "head" : (xPiece > 0 ? sprintf("limb %g", xPiece) : "torso");
    float loud = vr_rimp_test || cvar_hget(cvarh_vr_knockdown_debug);
    float wear = xShare >= 0 ? xShare * hp : xDamage * max(0, cvar_hget(cvarh_vr_ragimpact_wear)); // (its own health's loss)
    if(m.health <= 0)
    {
        // A corpse: its head or limb worn as any weapon's hit wears it (past its health: popped). Its torso: nothing.
        if(xPiece < 0)
        {
            return;
        }
        vr_rimp_dealt = vr_rimp_dealt + 1;
        vr_rimp_total = vr_rimp_total + xDamage;
        if(loud)
        {
            dprint(sprintf("ragimpact: %s corpse, %s: %s, %.1f into its %s (worn %.1f of %.1f)\n", m.classname, xWhat,
                xBy.classname, xDamage, piece, m.vr_limb_corpse_dmg[xPiece] + wear, hp));
        }
        if(wear > 0)
        {
            VR_Limb_WearInto(m, xBy, xPiece, wear, vel, QVR_DECAP_BLUNT, hp, "impact");
        }
        return;
    }
    vr_rimp_dealt = vr_rimp_dealt + 1;
    vr_rimp_total = vr_rimp_total + xDamage;
    float worn = 0;
    if(xPiece >= 0)
    {
        m.vr_limb_corpse_dmg[xPiece] = m.vr_limb_corpse_dmg[xPiece] + wear;
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
// xSpeed m/s (xRaw before the whip's cap), xMass kg behind it; xHeld: a hand holds the part (its least speed halved);
// xInto the blow's way into the part (from the level struck). A strike dealing vr_ragimpact_dent or more dents and
// wounds it there as a blunt melee blow does (VR_Wound_Slam).
void(entity m, vector xAt, float xSpeed, float xMass, float xRaw, float xHeld, vector xInto) VR_RagImpact_Hit =
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
    float dmg = VR_RagImpact_Scale(VR_RagImpact_Amount(xMass, xSpeed - least), piece);
    if(dmg < 0.5)
    {
        return;
    }
    m.vr_rimp_next[slot] = time + VR_RIMP_GAP;
    float dent = cvar_hget(cvarh_vr_ragimpact_dent);
    if(dent > 0 && dmg >= dent)
    {
        vr_rimp_dents = vr_rimp_dents + 1;
        if(vr_rimp_test || cvar_hget(cvarh_vr_knockdown_debug))
        {
            dprint(sprintf("ragimpact: %s dented at its %s (%.1f damage, into %v)\n", m.classname,
                piece == 0 ? "head" : (piece > 0 ? "limb" : "torso"), dmg, xInto));
        }
        VR_Wound_Slam(m, xAt, xInto, dmg, xHeld);
    }
    VR_RagImpact_Deal(m, by, xAt, piece, dmg, -1, sprintf("struck at %.1f m/s (%.1f raw)%s, %.1f kg behind it", xSpeed, xRaw,
        xHeld ? " held" : "", xMass));
};

// Where on standing monster `m` (no ragdoll) your feet at `xFeet` land, and the piece there (vr_rimp_piece: 0 its head,
// a limb's joint, -1 its torso): its drawn model met straight down from your feet (or from halfway to its middle), the
// head by positional damage's head zone (VR_Decap_OnHead), a limb by the rig's (VR_Limb_At). '0 0 0': not on it.
float vr_rimp_piece;
vector(entity m, vector xFeet) VR_RagImpact_StandingAt =
{
    vector top = xFeet;
    top_x = min(m.absmax_x, max(m.absmin_x, top_x));
    top_y = min(m.absmax_y, max(m.absmin_y, top_y));
    top_z = m.absmax_z;
    vector at = top;
    for(float k = 0; k < 2; k++)
    {
        vector from = k == 0 ? top : (top + m.origin) * 0.5;
        from_z = m.absmax_z + 8;
        float f = hitmodel_segment(m, from, from - '0 0 1' * (m.size_z * 0.6 + 8), 2, HITCLASS_MELEE);
        if(f >= 0)
        {
            at = from - '0 0 1' * (m.size_z * 0.6 + 8) * f;
            break;
        }
    }
    vr_rimp_piece = -1;
    if(VR_Decap_Allowed(m) && VR_Decap_OnHead(m, at, '0 0 -1'))
    {
        vr_rimp_piece = 0;
    }
    else if(VR_Limb_Allowed(m))
    {
        float bone = VR_Limb_At(m, at);
        vr_rimp_piece = (bone > 0 && bone < 16) ? bone : -1;
    }
    return at;
};

// What of ragdoll `m` (down or a corpse) your feet at `xFeet` land on: its parts touched in a column under your foot
// (VR_RIMP_FOOT units about its middle, VR_RIMP_FOOT_LOW under your feet to VR_RIMP_FOOT_HIGH over them: you may land
// on it or through it to the floor), the head before a limb before the torso, then the one nearest the middle. Returns
// its rank (lower: better; 1000000: none), vr_rimp_piece its piece (VR_RagImpact_Piece), vr_rimp_at where.
float VR_RIMP_FOOT = 6;
float VR_RIMP_FOOT_LOW = 6;
float VR_RIMP_FOOT_HIGH = 26;
vector vr_rimp_at;
float(entity m, vector xFeet) VR_RagImpact_Underfoot =
{
    float rank = 1000000;
    float loud = vr_rimp_test;
    for(float i = 0; i < 9; i++)
    {
        float row = floor(i / 3);
        vector off = '1 0 0' * (row - 1) * VR_RIMP_FOOT + '0 1 0' * (i - row * 3 - 1) * VR_RIMP_FOOT;
        for(float z = -VR_RIMP_FOOT_LOW; z <= VR_RIMP_FOOT_HIGH; z = z + 8)
        {
            vector at = xFeet + off + '0 0 1' * z;
            if(ragdollbone(m, at, 2) < 0)
            {
                continue;
            }
            float piece = VR_RagImpact_Piece(m, at);
            float r = ((piece == 0) ? 0 : ((piece > 0) ? 100 : 200)) + vlen(off) + z * 0.1;
            if(r < rank)
            {
                rank = r;
                vr_rimp_piece = piece;
                vr_rimp_at = at;
            }
        }
    }
    if(loud)
    {
        dprint(sprintf("ragimpact crush: %s under your feet at %v: %s\n", m.classname, xFeet, (rank < 1000000) ?
            sprintf("piece %g at %v", vr_rimp_piece, vr_rimp_at) : "nothing touched"));
    }
    return rank;
};

// Stomp (vr_ragimpact_stomp): you (`p`) landed at xSpeed units/s on standing `m`'s top (its crush dealt, it lives):
// fast enough (vr_ragimpact_stomp_speed; one heavier than vr_ragimpact_stomp_heavy kg needs that times the cube root of
// its mass over it), it is knocked down as a shove's knockdown (VR_Knockdown_Try's gates and the ragdoll pool's room; it gets up
// again), toppled along your way (or away from you, or backwards), and you bounce off it (vr_ragimpact_stomp_bounce).
// TRUE: knocked down.
float(entity m, entity p, float xSpeed) VR_RagImpact_Stomp =
{
    if(!cvar_hget(cvarh_vr_ragimpact_stomp) || !cvar_hget(cvarh_vr_knockdown) || m.health <= 0 || m.vr_knockdown ||
       isragdoll(m) || !m.vr_kd_chance_h || (m.flags & (FL_FLY | FL_SWIM)) || !(m.flags & FL_MONSTER) || !canragdoll(m) ||
       (VR_IsDummy(m) && cvar_hget(cvarh_vr_dummy_anchored)))
    {
        if(vr_rimp_test || cvar_hget(cvarh_vr_knockdown_debug))
        {
            dprint(sprintf("ragimpact stomp: %s cannot be knocked down (knockdown %g, ragdoll %g, canragdoll %g)\n",
                m.classname, m.vr_knockdown, isragdoll(m), canragdoll(m)));
        }
        return FALSE;
    }
    float need = cvar_hget(cvarh_vr_ragimpact_stomp_speed);
    float heavy = cvar_hget(cvarh_vr_ragimpact_stomp_heavy);
    float mass = VR_Grapple_MonsterMass(m);
    if(heavy > 0 && mass > heavy)
    {
        need = need * pow(mass / heavy, 1 / 3); // (a cube root: a shambler about a storey higher than a grunt)
    }
    float loud = vr_rimp_test || cvar_hget(cvarh_vr_knockdown_debug);
    if(xSpeed < need)
    {
        if(loud)
        {
            dprint(sprintf("ragimpact stomp: %s (%.0f kg) stands: landed at %.0f u/s, under %.0f\n", m.classname, mass,
                xSpeed, need));
        }
        return FALSE;
    }
    if(!VR_KD_SureRoll(m)) // (a tough monster group's: by its Knockdown Chance)
    {
        if(loud)
        {
            dprint(sprintf("ragimpact stomp: %s stands: its group's Knockdown Chance %.2f\n", m.classname, VR_KD_Scale(m)));
        }
        return FALSE;
    }
    vector dir = p.velocity;
    dir_z = 0;
    if(vlen(dir) < 50)
    {
        dir = m.origin - p.origin;
        dir_z = 0;
    }
    if(vlen(dir) < 1)
    {
        makevectors(m.angles);
        dir = '0 0 0' - v_forward; // (straight down on it: over backwards)
    }
    dir = normalize(dir);
    if(!VR_Knockdown_MakeRoom(m) || !VR_Knockdown_Start(m))
    {
        if(loud)
        {
            dprint(sprintf("ragimpact stomp: %s: no ragdoll (no room)\n", m.classname));
        }
        return FALSE;
    }
    ragdollshovetopple(m, dir, 1, FALSE, 24);
    float bounce = max(0, cvar_hget(cvarh_vr_ragimpact_stomp_bounce));
    if(bounce > 0)
    {
        p.velocity_z = max(p.velocity_z, bounce);
        p.flags = p.flags - (p.flags & FL_ONGROUND);
    }
    if(loud)
    {
        dprint(sprintf("ragimpact stomp: %s (%.0f kg) knocked down, health %g: landed at %.0f u/s (least %.0f), you bounce "
            "at %.0f u/s\n", m.classname, mass, m.health, xSpeed, need, p.velocity_z));
    }
    return TRUE;
};

// A landing's crush on ragdoll `m` (down or a corpse; the author, 2026-10-16: "it stays completely still"): the part at
// xAt under your (`p`) feet pushed down and out from its body's middle (along your way when over the middle) at
// vr_ragimpact_crush_push times your landing speed xSpeed (units/s; at most VR_RIMP_PUSH_MOST), and its other parts
// near it (VR_RIMP_PUSH_NEAR..VR_RIMP_PUSH_FAR units) flung up and away from it at up to VR_RIMP_PUSH_BODY of that, less
// the farther: it squashes and the body jolts round your feet (which stand on it and pin the part under them). Your
// vr_ragimpact_crush_mass behind each push (physicspush).
float VR_RIMP_PUSH_BODY = 0.6;
float VR_RIMP_PUSH_MOST = 350;
float VR_RIMP_PUSH_NEAR = 6;
float VR_RIMP_PUSH_FAR = 56;
void(entity m, entity p, vector xAt, float xSpeed) VR_RagImpact_CrushPush =
{
    float v = min(VR_RIMP_PUSH_MOST, xSpeed * max(0, cvar_hget(cvarh_vr_ragimpact_crush_push)));
    if(v < 1 || !isragdoll(m))
    {
        return;
    }
    vector mid = (m.absmin + m.absmax) * 0.5;
    vector out = xAt - mid;
    out_z = 0;
    if(vlen(out) < 4)
    {
        out = p.velocity;
        out_z = 0;
    }
    if(vlen(out) < 1)
    {
        makevectors(p.angles);
        out = v_forward;
        out_z = 0;
    }
    out = normalize(out);
    float mass = max(1, cvar_hget(cvarh_vr_ragimpact_crush_mass));
    physicspush(m, xAt, normalize(out - '0 0 0.5') * v, mass);
    float flung = 0;
    for(float b = 0; b < 16; b++)
    {
        vector at = ragdollpoint(m, b, '0 0 0', 1);
        vector away = at - xAt;
        away_z = 0;
        float d = vlen(away);
        if(at == '0 0 0' || d < VR_RIMP_PUSH_NEAR || d > VR_RIMP_PUSH_FAR)
        {
            continue;
        }
        float f = 1 - (d - VR_RIMP_PUSH_NEAR) / (VR_RIMP_PUSH_FAR - VR_RIMP_PUSH_NEAR);
        physicspush(m, at, normalize(normalize(away) * 0.6 + '0 0 1') * v * VR_RIMP_PUSH_BODY * f, mass);
        flung = flung + 1;
    }
    if(vr_rimp_test || cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("ragimpact crush push: %s's part at %v down and out at %.0f u/s, %g parts round it flung up at "
            "up to %.0f u/s\n", m.classname, xAt, v, flung, v * VR_RIMP_PUSH_BODY));
    }
};

// A landing's crush sound on `m`: the gib's (player/udeath) only when something came off it (xPopped); else a body's
// punch and thud (the arms-crossed parry's: fisthit, demon/dland2), louder the faster you landed (xSpeed units/s, from
// vr_ragimpact_crush_speed to vr_ragimpact_crush_pop), never as loud as a gib.
void(entity m, float xPopped, float xSpeed, float xDamage, float xShare) VR_RagImpact_CrushSound =
{
    float vol;
    if(xPopped)
    {
        vol = min(1, max(0.7, max(xDamage / 40, xShare)));
        sound(m, CHAN_AUTO, "player/udeath.wav", vol, ATTN_NORM);
    }
    else
    {
        float least = cvar_hget(cvarh_vr_ragimpact_crush_speed);
        float t = min(1, max(0, (xSpeed - least) / max(1, cvar_hget(cvarh_vr_ragimpact_crush_pop) - least)));
        vol = 0.45 + 0.4 * t;
        VR_SoundVaried(m, CHAN_AUTO, "fisthit.wav", vol, ATTN_NORM);
        sound(m, CHAN_AUTO, "demon/dland2.wav", vol * 0.6, ATTN_NORM);
    }
    if(vr_rimp_test || cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("ragimpact crush sound: %s, volume %.2f\n", xPopped ? "gib (something popped)" : "thud (nothing popped)",
            vol));
    }
};

// You (`p`) landed at xSpeed units/s (client.qc's landing, PlayerPostThink; xOnGround: on the floor, else stopped on a
// standing monster's top): a monster's part under your feet is crushed (vr_ragimpact_crush): a ragdoll's (knocked
// down, staggering or a corpse) or, with vr_ragimpact_crush_standing, a standing one's; its head or limb worn by a share
// of its own health (vr_ragimpact_crush_wear, all of it from vr_ragimpact_crush_pop up), a big one only hurt; a standing
// one that lives through it may be knocked down (VR_RagImpact_Stomp). TRUE: one was crushed or knocked down (it broke
// your fall).
float(entity p, float xSpeed, float xOnGround) VR_RagImpact_Crush =
{
    float k = max(0, cvar_hget(cvarh_vr_ragimpact_crush));
    float least = cvar_hget(cvarh_vr_ragimpact_crush_speed);
    float crush = k > 0 && xSpeed >= least;
    float stomp = cvar_hget(cvarh_vr_ragimpact_stomp) && xSpeed >= cvar_hget(cvarh_vr_ragimpact_stomp_speed);
    if(!crush && !stomp)
    {
        return FALSE;
    }
    vector feet = p.origin + '0 0 1' * p.mins_z;
    float standingCrush = cvar_hget(cvarh_vr_ragimpact_crush_standing);
    float standing = standingCrush || stomp;
    entity best = world;
    vector bestAt = '0 0 0';
    float bestDist = 1000000;
    float bestPiece = -1;
    for(entity m = findradius(feet, 128); m != world; m = m.chain)
    {
        if(!(m.flags & FL_MONSTER) || !m.takedamage || m.vr_gib || m == p || (m.health <= 0 && !isragdoll(m)))
        {
            continue; // (a corpse: as a ragdoll only, its head and limbs worn as a weapon's hits wear them)
        }
        if(!isragdoll(m))
        {
            // Standing: your feet on its box's top (your box over its), its model met under them.
            if(!standing || feet_z < m.absmax_z - 8 || feet_z > m.absmax_z + 16 || p.absmax_x <= m.absmin_x ||
               p.absmin_x >= m.absmax_x || p.absmax_y <= m.absmin_y || p.absmin_y >= m.absmax_y)
            {
                continue;
            }
            vector sat = VR_RagImpact_StandingAt(m, feet);
            if(vlen(sat - feet) < bestDist)
            {
                bestDist = vlen(sat - feet);
                best = m;
                bestAt = sat;
                bestPiece = vr_rimp_piece;
            }
            continue;
        }
        if(!crush)
        {
            continue; // (a ragdoll is only crushed)
        }
        float rank = VR_RagImpact_Underfoot(m, feet);
        if(rank < bestDist)
        {
            bestDist = rank;
            best = m;
            bestAt = vr_rimp_at;
            bestPiece = vr_rimp_piece;
        }
    }
    if(best == world)
    {
        if(vr_rimp_test && xOnGround)
        {
            dprint(sprintf("ragimpact: landed at %.0f u/s: no monster under your feet\n", xSpeed));
        }
        return FALSE;
    }
    float m2u = max(1, VRMetersToUnits());
    float speed = xSpeed / m2u;
    float upright = !isragdoll(best);
    // A big one (heavier than vr_ragimpact_crush_big kg: an ogre, a fiend, a shambler): hurt only, its head and limbs
    // never worn, at most vr_ragimpact_crush_big_max of its full health a landing (never killed outright).
    float bigKg = cvar_hget(cvarh_vr_ragimpact_crush_big);
    float big = bigKg > 0 && VR_Grapple_MonsterMass(best) > bigKg;
    if(big)
    {
        bestPiece = -1;
    }
    float dmg = 0;
    float share = 0;
    if(crush && (!upright || standingCrush))
    {
        dmg = VR_RagImpact_Scale(VR_RagImpact_Amount(cvar_hget(cvarh_vr_ragimpact_crush_mass), speed - least / m2u) * k,
                                 bestPiece);
        float most = cvar_hget(cvarh_vr_ragimpact_crush_big_max);
        if(big && most > 0 && best.health > 0)
        {
            dmg = min(dmg, max(1, (best.max_health > 0 ? best.max_health : best.health) * most));
        }
        // Its head's or limb's wear: vr_ragimpact_crush_wear of its own health at the least speed (a jump from the same
        // level), rising with the square of the way to vr_ragimpact_crush_pop, where it pops at once.
        float per = max(0, cvar_hget(cvarh_vr_ragimpact_crush_wear));
        float pop = cvar_hget(cvarh_vr_ragimpact_crush_pop);
        if(bestPiece >= 0 && per > 0)
        {
            float t = pop > least ? min(1, max(0, (xSpeed - least) / (pop - least))) : 1;
            share = xSpeed >= pop ? 1 : min(1, per + (1 - per) * t * t);
        }
    }
    float hit = FALSE;
    if(dmg >= 0.5 || share > 0)
    {
        if(!upright)
        {
            VR_RagImpact_CrushPush(best, p, bestAt, xSpeed);
        }
        float came = vr_decap_count + vr_limb_pops + vr_limb_cuts; // (what comes off: a pop's gib sound)
        VR_RagImpact_Deal(best, p, bestAt, bestPiece, dmg, share, sprintf("crushed (%s%s), you landing at %.1f m/s, "
            "wearing %.2f", upright ? "standing" : (best.health > 0 ? "a ragdoll" : "a corpse"), big ? ", big" : "", speed,
            share));
        VR_RagImpact_CrushSound(best, best.vr_gib || vr_decap_count + vr_limb_pops + vr_limb_cuts > came, xSpeed, dmg,
            share);
        hit = TRUE;
    }
    // Standing and still alive (and not made a ragdoll by the crush's pain): knocked down by the stomp.
    if(upright && stomp && best.health > 0 && VR_RagImpact_Stomp(best, p, xSpeed))
    {
        hit = TRUE;
    }
    return hit;
};
