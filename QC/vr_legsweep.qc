// vr_legsweep.qc -- the leg sweep (Combat > Knockdowns > Leg Sweep; docs/vr-port/ROUND22.md, "Leg sweep"). The engine
// (vr_legsweep.cpp) finds a staff-like thing in a hand swept level and fast enough through a humanoid's lower legs, and
// calls this to knock it down; its fall (its feet swept along, its body over the other way) is the engine's too
// (vr_foegrab.cpp sweepDown).

// Self the player, m swept, to fall towards yaw xYaw (against the sweep), dealt xDamage once down. Returns 1 knocked
// down, 2 the training dummy (shown, not knocked down: it stands on its spot), -2 it can't be knocked down now (no ragdoll
// or get-up, no room for one, down already, flying or swimming).
float(entity m, float xYaw, float xDamage) VR_LegSweep_Down =
{
    if(VR_IsDummy(m))
    {
        floattext(m.origin + '0 0 1' * (m.maxs_z + 8), "swept", '0.4 1 0.4', 0.6);
        return 2;
    }
    if((m.flags & (FL_FLY | FL_SWIM)) || m.vr_knockdown || !m.vr_kd_chance_h || !canragdoll(m) || !VR_Knockdown_MakeRoom(m))
    {
        return -2;
    }
    vector was = m.velocity;
    m.velocity = '0 0 0'; // (the engine's topple launches it: its feet along the sweep, its top over the other way)
    if(!VR_Knockdown_Start(m))
    {
        m.velocity = was;
        return -2;
    }
    // Swept by you: its impacts count (vr_ragimpact_time; vr_ragimpact.qc), no hand spared by it.
    m.vr_rag_letgo = self;
    m.vr_rag_letgo_hand = 0;
    m.vr_rag_letgo_time = time;
    dprint(sprintf("legsweep: %s knocked down, to fall towards yaw %.0f, %.0f damage\n", m.classname, xYaw, xDamage));
    if(xDamage > 0)
    {
        T_Damage(m, self, self, xDamage);
    }
    return 1;
};
