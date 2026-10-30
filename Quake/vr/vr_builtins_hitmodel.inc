// vr_builtins_hitmodel.inc -- PF_ builtins: precise hit detection (vr_hitmodel.cpp), model anchors, debug hit zones.
// Split from vr_builtins.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// Precise hit detection (vr_hitmodel.cpp). hitmodel_target(e): whether e's model is what is hit (the option on, a
// monster or corpse with a Quake model).
void PF_hitmodel_target()
{
    G_FLOAT(OFS_RETURN) = hitmodel::target(G_EDICT(OFS_PARM0)) ? 1.f : 0.f;
}

// hitmodel_segment(e, a, b, radius, class): the segment a..b, a sphere of radius `radius` plus the class's tolerance
// (0 guns, 1 grapple, 2 melee, 3 thrown), against e's model as drawn: where along it it first meets it (0: it starts
// in it), -1 missed; -2: e's model is not tested (the option off, not a target): its box as before. The point on the
// model: hitmodel_rest's (the last hit). Class -1 (the hose cut, QC vr_ad_enemyguns.qc VR_HoseCut_Melee): e a tank lying
// about, its hose to its gun as drawn (vr_adtank.cpp crossHose, a listen server's): how far along the hose (0 at the
// tank, 1 at the gun) its nearest point within `radius` of a..b lies, trace_endpos that point; -1 missed; -2 no hose.
void PF_hitmodel_segment()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* a = G_VECTOR(OFS_PARM1);
    const float* b = G_VECTOR(OFS_PARM2);
    const float radius = za::max(0.f, G_FLOAT(OFS_PARM3));
    if(G_FLOAT(OFS_PARM4) < 0.f)
    {
        glm::vec3 at{0.f};
        float share = 0.f;
        const int got = adtank::crossHose(NUM_FOR_EDICT(e), geom::vec(a), geom::vec(b), radius, at, share);
        if(got > 0)
        {
            for(int i = 0; i < 3; i++)
            {
                pr_global_struct->trace_endpos[i] = at[i];
            }
        }
        G_FLOAT(OFS_RETURN) = got > 0 ? share : static_cast<float>(got);
        return;
    }
    const auto c = static_cast<hitmodel::Class>(za::clamp(static_cast<int>(G_FLOAT(OFS_PARM4)), 0, 3));
    if(!hitmodel::target(e))
    {
        G_FLOAT(OFS_RETURN) = -2.f;
        return;
    }
    hitmodel::Hit h;
    const bool hit = hitmodel::segment(e, geom::vec(a), geom::vec(b), radius + hitmodel::tolerance(c), 1.f, c, h);
    G_FLOAT(OFS_RETURN) = hit ? h.t : -1.f;
}

// hitmodel_any(e, a, b, radius): the segment a..b, a sphere of `radius`, against e's model as drawn, whatever e is (a
// prop, an item: the grappling hook bites only what it touches): where along it it first meets it, -1 missed, -2 e has no
// shape of its own (its box is). A weapon lying about, drawn by its Weapon Offsets, and a brush prop (an explosive box):
// its Box3D body, as it is turned (-2 without one).
void PF_hitmodel_any()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* a = G_VECTOR(OFS_PARM1);
    const float* b = G_VECTOR(OFS_PARM2);
    const float radius = za::max(0.f, G_FLOAT(OFS_PARM3));
    hitmodel::Hit h;
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(model && model->type == mod_alias && weapons::modelTransform(model).active)
    {
        // A weapon's own model lying about, drawn by its Weapon Offsets: its body in Box3D (its drawn shape; its box is
        // a cube round its handle, which a long gun reaches well out of), else its box.
        float fraction = 1.f;
        bool body = false;
        const bool hit = box3d::castAt(NUM_FOR_EDICT(e), geom::vec(a), geom::vec(b), radius,
            fraction, body);
        G_FLOAT(OFS_RETURN) = hit ? fraction : body ? -1.f : -2.f;
        return;
    }
    if(model && model->type == mod_brush)
    {
        // A brush prop (an explosive box, a health box) loose in Box3D: its body as it is turned (Quake's box round a
        // tilted one is much bigger than it); placed by the map, not a body yet: its box.
        float fraction = 1.f;
        bool body = false;
        const bool hit = box3d::castAt(NUM_FOR_EDICT(e), geom::vec(a), geom::vec(b), radius,
            fraction, body);
        G_FLOAT(OFS_RETURN) = hit ? fraction : body ? -1.f : -2.f;
        return;
    }
    if(!model || model->type != mod_alias)
    {
        G_FLOAT(OFS_RETURN) = -2.f; // (no model: its box)
        return;
    }
    const bool hit =
        hitmodel::segment(e, geom::vec(a), geom::vec(b), radius, 1.f, hitmodel::Class::Grapple, h);
    G_FLOAT(OFS_RETURN) = hit ? h.t : -1.f;
}

// float(entity player, entity monster) bodygap: the level gap between the two bodies as their moves meet (units; 0
// touching): the boxes as the hull code narrows and insets them (vr_bodycontact.cpp levelGap), not QC's 32-wide ones.
void PF_bodygap()
{
    G_FLOAT(OFS_RETURN) = bodycontact::levelGap(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1));
}

// hitmodel_rest(e, p): the point p on e's drawn model, as it is on the model standing (its stand frame), placed at its
// origin and turned with its yaw (positional damage's head sphere and regions); p itself if e is no target.
void PF_hitmodel_rest()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* p = G_VECTOR(OFS_PARM1);
    glm::vec3 out{p[0], p[1], p[2]};
    hitmodel::restPoint(e, out, out);
    float* r = G_VECTOR(OFS_RETURN);
    r[0] = out.x;
    r[1] = out.y;
    r[2] = out.z;
}

// The entity's own Quake .mdl, a body's (a monster's, a corpse's, a player's): not a brush model, nor a weapon drawn by
// its Weapon Offsets (modelanchor, modelanchorpoint).
[[nodiscard]] bool bodyModel(const edict_t* e)
{
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    return model && model->type == mod_alias && !weapons::modelTransform(model).active;
}

// modelanchor(e, p): the point of e's Quake .mdl as drawn (a monster's, a corpse's, a ragdoll's as its parts carry it, a
// player's) nearest p, as an anchor on its surface: x its triangle, y and z where on it (modelanchorpoint finds it again
// as the model moves); trace_endpos the point, trace_plane_normal its outward normal, trace_plane_dist how far p is from
// it. x -1: no such model (the trace globals untouched). (A fire's flames on the body: QC vr_burning.qc.)
void PF_modelanchor()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* p = G_VECTOR(OFS_PARM1);
    float* r = G_VECTOR(OFS_RETURN);
    r[0] = -1.f;
    r[1] = r[2] = 0.f;
    hitmodel::Hit h;
    float dist = 0.f;
    if(!bodyModel(e) || !hitmodel::nearestAny(e, geom::vec(p), h, dist))
    {
        return;
    }
    r[0] = static_cast<float>(h.tri);
    r[1] = h.u;
    r[2] = h.v;
    for(int i = 0; i < 3; i++)
    {
        pr_global_struct->trace_endpos[i] = h.surface[i];
        pr_global_struct->trace_plane_normal[i] = h.normal[i];
    }
    pr_global_struct->trace_plane_dist = dist;
}

// modelanchorpoint(e, anchor): where modelanchor's anchor is on e's model as drawn now (trace_plane_normal its normal,
// trace_fraction 1); trace_fraction 0 and e's origin when it is no longer there (another model: gibbed, made a ragdoll's).
// An anchor whose x is -2 - n is vertex n of the model (the armed grunts, QC vr_armedgrunt.qc modelvertex: their hands
// and their model's hidden gun's line; its normal not set).
void PF_modelanchorpoint()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* a = G_VECTOR(OFS_PARM1);
    float* r = G_VECTOR(OFS_RETURN);
    glm::vec3 point{e->v.origin[0], e->v.origin[1], e->v.origin[2]}, normal{0.f, 0.f, 1.f};
    if(a[0] <= -2.f)
    {
        const bool found = hitmodel::vertexPoint(e, static_cast<int>(-a[0] - 2.f + 0.5f), point);
        pr_global_struct->trace_fraction = found ? 1.f : 0.f;
        for(int i = 0; i < 3; i++)
        {
            r[i] = point[i];
        }
        return;
    }
    const bool found = a[0] >= 0.f && bodyModel(e) && hitmodel::surfacePoint(e, static_cast<int>(a[0]), a[1], a[2], point, normal);
    pr_global_struct->trace_fraction = found ? 1.f : 0.f;
    for(int i = 0; i < 3; i++)
    {
        r[i] = point[i];
        if(found)
        {
            pr_global_struct->trace_plane_normal[i] = normal[i];
        }
    }
}

// debughitzone(e, zone, lo, hi, radius): Show Hit Zones' zone of `e` this server frame (hitmodel::zoneAdd: in its own
// frame, x forward, y left, z up from its origin; a box lo..hi, or a capsule of `radius` from lo to hi).
void PF_debughitzone()
{
    const float* lo = G_VECTOR(OFS_PARM2);
    const float* hi = G_VECTOR(OFS_PARM3);
    hitmodel::zoneAdd(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), static_cast<int>(G_FLOAT(OFS_PARM1)), geom::vec(lo),
        geom::vec(hi), G_FLOAT(OFS_PARM4));
}
