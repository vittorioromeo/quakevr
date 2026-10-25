// vr_view_weapon.cpp -- the held weapon's set-up (setupWeapon) and its helpers: magazines' shapes and places, the
// weapon's text, hotspot frames, the drawn weapon's record, animation motion.
// Split from vr_view.cpp (a pure move); what they share is in vr_view_internal.hpp.

#include "vr_view_internal.hpp"

namespace view_detail
{
namespace
{
// The box round a magazine's vertices in its well (vr_mag_on_<gun>.mdl), along the mount's length; false for a model
// without Quake vertices.
[[nodiscard]] bool magazineShape(const MagMount& mount, qmodel_t* model, MagShape& out)
{
    for(const MagShape& m : magShapeCache.shapes)
    {
        if(m.model == model)
        {
            out = m;
            return true;
        }
    }
    if(!model || model->type != mod_alias)
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || hdr->numverts <= 0)
    {
        return false;
    }
    MagShape shape;
    shape.model = model;
    const glm::vec3 up = glm::normalize(mount.seat - mount.centre);
    glm::vec3 across = glm::cross(up, glm::vec3{0.f, 1.f, 0.f});
    if(glm::length(across) < 0.3f)
    {
        across = glm::cross(up, glm::vec3{1.f, 0.f, 0.f});
    }
    across = glm::normalize(across);
    shape.axis[0] = up;
    shape.axis[1] = across;
    shape.axis[2] = glm::cross(up, across);
    glm::vec3 lo{1e30f}, hi{-1e30f};
    entity_t ent{};
    ent.model = model;
    za::Vector<glm::vec3>& rest = scratch.magRest;
    za::Vector<glm::vec3>& now = scratch.magNow;
    if(!anchor::posedVertices(ent, 1.f, rest, now))
    {
        return false;
    }
    for(const glm::vec3& b : rest)
    {
        const glm::vec3 p{b.x * hdr->scale[0] + hdr->scale_origin[0], b.y * hdr->scale[1] + hdr->scale_origin[1],
            b.z * hdr->scale[2] + hdr->scale_origin[2]};
        for(int i = 0; i < 3; i++)
        {
            const float t = glm::dot(p, shape.axis[i]);
            lo[i] = za::min(lo[i], t);
            hi[i] = za::max(hi[i], t);
        }
    }
    for(int i = 0; i < 3; i++)
    {
        shape.half[i] = (hi[i] - lo[i]) * 0.5f;
        shape.centre += shape.axis[i] * ((hi[i] + lo[i]) * 0.5f);
    }
    magShapeCache.shapes.pushBack(shape);
    out = shape;
    return true;
}
} // namespace

void place(view::ViewEntity& ve, qmodel_t* model, const glm::vec3& origin, const glm::vec3& angles,
    int frame, bool mirrored)
{
    entity_t& e = ve.ent;

    if(model != ve.lastModel)
    {
        e.lerpflags |= LERP_RESETANIM;
        ve.lastModel = model;
    }

    e.model = model;
    e.frame = frame;
    e.colormap = vid.colormap;
    e.scale = ENTSCALE_DEFAULT;
    e.alpha = ENTALPHA_DEFAULT;
    for(int i = 0; i < 3; i++)
    {
        e.origin[i] = origin[i];
        e.angles[i] = angles[i];
    }

    ve.mirrored = mirrored;
    ve.scale = glm::vec3{1.f};
    ve.visible = model != nullptr;
}

// Angles `offsets` (in the weapon's frame, as the per-weapon attachment angles are tuned) turned by
// `rot` (a hand pose): composed, not added (added Euler angles only agree while the hand is level).
[[nodiscard]] glm::vec3 composeAngles(const glm::vec3& rot, const glm::vec3& offsets)
{
    const auto axes = [](const glm::vec3& a) {
        vec3_t in{a.x, a.y, a.z}, f, r, u;
        AngleVectors(in, f, r, u);
        return glm::mat3{glm::vec3{f[0], f[1], f[2]}, -glm::vec3{r[0], r[1], r[2]}, glm::vec3{u[0], u[1], u[2]}};
    };
    const glm::mat3 m = axes(rot) * axes(offsets);
    return hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
}
[[nodiscard]] glm::vec3 weaponAngleOffsets(int slot, bool mirrored)
{
    glm::vec3 o = weapons::vec(slot, Key::Pitch, Key::Yaw, Key::Roll);
    o.x += weapons::gunModelPitchBase; // (what the player sets past it turns the hand too: hands::wholeOffset)
    if(mirrored)
    {
        o.y = -o.y;
        o.z = -o.z;
    }
    return o;
}

namespace
{
// Quake angles (not an alias model's) to axes: forward, left, up.
[[nodiscard]] glm::mat3 angleAxes(const glm::vec3& a)
{
    vec3_t in{a.x, a.y, a.z}, f, r, u;
    AngleVectors(in, f, r, u);
    return glm::mat3{glm::vec3{f[0], f[1], f[2]}, -glm::vec3{r[0], r[1], r[2]}, glm::vec3{u[0], u[1], u[2]}};
}
} // namespace

// The angles of an attachment (ammo screen, button: `offsets` as tuned, composed with the hand holding the gun) on a
// weapon drawn at `aliasAngles` without a hand (holstered, lying in the world). A held gun is turned from the hand by
// its angle offsets (weaponAngleOffsets); the hand it would be held by is found by undoing them.
[[nodiscard]] glm::vec3 idleAttachmentAngles(const glm::vec3& aliasAngles, int slot, bool mirrored, const glm::vec3& offsets)
{
    const glm::vec3 o = weaponAngleOffsets(slot, mirrored);
    const glm::mat3 model = angleAxes({-aliasAngles.x, aliasAngles.y, aliasAngles.z});
    const glm::mat3 hand = model * glm::transpose(angleAxes({-o.x, o.y, o.z}));
    const glm::mat3 m = hand * angleAxes(offsets);
    return hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
}

namespace
{
// The player's ammo for a gun not in a hand, or -1 when the client is not told it (the lava nails, multi-rockets
// and plasma are not in the stats).
[[nodiscard]] int idleAmmo(const qmodel_t* model)
{
    static constexpr za::Pair<const char*, int> ammo[] = {{"progs/v_shot.mdl", STAT_SHELLS},
        {"progs/v_shot2.mdl", STAT_SHELLS}, {"progs/v_nail.mdl", STAT_NAILS}, {"progs/v_nail2.mdl", STAT_NAILS},
        {"progs/v_rock.mdl", STAT_ROCKETS}, {"progs/v_rock2.mdl", STAT_ROCKETS}, {"progs/v_prox.mdl", STAT_ROCKETS},
        {"progs/v_light.mdl", STAT_CELLS}, {"progs/v_laserg.mdl", STAT_CELLS}, {"progs/v_hammer.mdl", STAT_CELLS},
        {"owned/mg3/progs/v_hammer.mdl", STAT_CELLS}, {"owned/mg3/progs/v_hammer_glow.mdl", STAT_CELLS}}; // (the Super Axe's burst)
    for(const auto& [name, stat] : ammo)
    {
        if(model && !strcmp(model->name, name))
        {
            return cl.stats[stat];
        }
    }
    return -1;
}
} // namespace

// The ammo screen's text of a gun not in a hand: as a held one's (its clip over the ammo when reloading), with what
// is known: the clip of a holstered gun (`clip` >= 0) and its size once seen in a hand, the player's ammo for it
// ("--" unknown).
// `total`: the reserve under the clip (a gun in a holster: the player's); not on a gun lying about, whose screen shows its
// clip and size alone, as the clip's line of a held gun's (the author's note vrfiringrange_2026-10-08_14-18-22).
[[nodiscard]] za::String idleWeaponText(const qmodel_t* model, int clip, bool total)
{
    const int ammo = idleAmmo(model);
    const za::String ammoText = ammo >= 0 ? za::toString(ammo) : za::String{"--"};
    const bool reloading = cl.stats[protocol::STAT_QVR_RELOADMODE] != 0; // (the server's mode as it applies)
    const auto size = clipSizes.find(model);
    if(reloading && clip >= 0 && size != clipSizes.end() && size->second != 0)
    {
        return total ? za::toString(clip) + "/" + za::toString(size->second) + "\n" + ammoText
                     : za::toString(clip) + "/" + za::toString(size->second);
    }
    return ammoText;
}

namespace
{
double carriedPrintAt[2]{-1.0, -1.0}; // vr_debug_2h_grip 2, per hand: the next print of its weapon's drawn pose

void queueWeaponText(const glm::vec3& handRot, bool mirrored, int hand, const view::ViewEntity& ve, int slot)
{
    // The view may be set up more than once per frame; queue once.
    int(&queuedFrame)[2] = oncePerFrame.weaponText;
    if(!vr_show_weapon_text.value || !ve.visible || weapons::value(slot, Key::WpnTextMode) == 0.f ||
        queuedFrame[hand] == host_framecount)
    {
        return;
    }
    queuedFrame[hand] = host_framecount;

    const glm::vec3 pos = view::anchorPosition(ve, weapons::anchor(slot, Key::WpnTextAnchor),
        weapons::vec(slot, Key::WpnTextX, Key::WpnTextY, Key::WpnTextZ));

    glm::vec3 angles = weapons::vec(slot, Key::WpnTextPitch, Key::WpnTextYaw, Key::WpnTextRoll);
    if(mirrored)
    {
        angles.z = -angles.z;
    }
    // Read from behind the weapon, along its aim: the weapon's offset turned by the hand (composed,
    // not added: added angles only agree while the hand is level, and a gun pointing up tilted the
    // screen the wrong way).
    angles = composeAngles(handRot, angles);

    const bool main = hand == HAND_MAIN;
    const int clip = cl.stats[main ? STAT_QVR_WEAPONCLIP : STAT_QVR_WEAPONCLIP2];
    const int clipSize = cl.stats[main ? STAT_QVR_WEAPONCLIPSIZE : STAT_QVR_WEAPONCLIPSIZE2];
    const int ammo = cl.stats[main ? STAT_QVR_AMMOCOUNTER : STAT_QVR_AMMOCOUNTER2];
    const bool reloading = cl.stats[protocol::STAT_QVR_RELOADMODE] != 0; // (the server's mode as it applies)
    if(clipSize != 0)
    {
        clipSizes[ve.ent.model] = clipSize;
    }

    char buf[64];
    if(reloading && clipSize != 0)
    {
        q_snprintf(buf, sizeof(buf), "%d/%d\n%d", clip, clipSize, ammo);
    }
    else
    {
        q_snprintf(buf, sizeof(buf), "%d", ammo);
    }

    text3d::queue(buf, pos, angles, text3d::Align::Centre, 0.1f * weapons::value(slot, Key::WpnTextScale),
        vr_weapon_screen.value != 0.f);
    if(vr_weapon_screen.value != 0.f)
    {
        emissive::weaponScreenLight(hand, pos, angles); // its faint glow (vr_weapon_screen_light)
    }
}
} // namespace

// Switching a gun's ammo (its button) swaps its model for the other ammo's (the same gun, another paint and a
// few parts: improve_weapons_alt.py); over vr_weapon_morph_time the old model dissolves where the new one appears,
// a glowing seam sweeping between (the alias shader: VR_AliasMorph). The pairs, and the seam's colour (kind).
[[nodiscard]] int morphKind(const qmodel_t* a, const qmodel_t* b)
{
    static constexpr modelmeta::Id pairs[][2] = {{modelmeta::Id::VNail, modelmeta::Id::VLava},
        {modelmeta::Id::VNail2, modelmeta::Id::VLava2}, {modelmeta::Id::VRock, modelmeta::Id::VMulti},
        {modelmeta::Id::VRock2, modelmeta::Id::VMulti2}, {modelmeta::Id::VLight, modelmeta::Id::VPlasma}};
    static constexpr int kinds[] = {0, 0, 1, 1, 2}; // lava, multi-rockets, plasma
    if(!a || !b)
    {
        return -1;
    }
    const auto ia = modelmeta::get(a).id, ib = modelmeta::get(b).id;
    for(int i = 0; i < static_cast<int>(za::getArraySize(pairs)); i++)
    {
        const auto x = pairs[i][0];
        const auto y = pairs[i][1];
        if((ia == x && ib == y) || (ia == y && ib == x))
        {
            return kinds[i];
        }
    }
    return -1;
}

namespace
{
// The morph's progress (0..1; 1 when not morphing), after following the hand's gun model.
[[nodiscard]] float updateMorph(int hand, qmodel_t* model)
{
    Morph& m = morphs[hand];
    const float time = vr_weapon_morph_time.value;
    if(model != m.last)
    {
        const int kind = morphKind(m.last, model);
        m.from = kind >= 0 && time > 0.f ? const_cast<qmodel_t*>(m.last) : nullptr;
        m.kind = za::max(kind, 0);
        m.start = vr_gametime;
        m.last = model;
    }
    if(!m.from)
    {
        return 1.f;
    }
    const float t = time > 0.f ? static_cast<float>((vr_gametime - m.start) / time) : 1.f;
    if(t >= 1.f || t < 0.f)
    {
        m.from = nullptr;
        return 1.f;
    }
    return t;
}

// The morph's value for the shader (vr_render.cpp VR_AliasMorph): + the model coming in, - the one going out.
[[nodiscard]] float morphValue(float t, int kind, bool incoming)
{
    const float v = 2.f * static_cast<float>(kind) + za::clamp(t, 1e-3f, 1.f);
    return incoming ? v : -v;
}
} // namespace

// Where a lava gun's glow comes from: along its barrels (between the hand and the muzzle), a little over them (a
// light inside the gun would light only the inside of its faces).
[[nodiscard]] glm::vec3 lavaGlowPosition(const entity_t& e, bool mirrored, float zeroBlend, int slot)
{
    const glm::vec3 grip{e.origin[0], e.origin[1], e.origin[2]}; // where the hand holds it (the controller)
    const glm::vec3 muzzle = view::anchorPosition(e, mirrored, zeroBlend,
        weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    const glm::mat3 axes = angleAxes({-e.angles[0], e.angles[1], e.angles[2]});
    return glm::mix(grip, muzzle, 0.55f) + axes[2] * 2.f;
}

namespace
{
// ----------------------------------------------------------------------------
// The weapons' hotspots (round 21, vr_weapons.hpp): where the other hand may hold them.

[[nodiscard]] bool hasGripHotspot(int slot)
{
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        if(weapons::isGripType(weapons::hotspot(slot, i).type))
        {
            return true;
        }
    }
    return false;
}
} // namespace

// Where a hotspot's point is drawn: the weapon entity's frame, as its model's vertices before their own scaling.
[[nodiscard]] glm::mat4 hotspotFrame(const entity_t& e, bool mirrored)
{
    float m[16];
    vec3_t origin, angles;
    VectorCopy(e.origin, origin);
    VectorCopy(e.angles, angles);
    R_EntityMatrix(m, origin, angles, ENTSCALE_DEFAULT);
    if(mirrored)
    {
        ApplyScale(m, 1.f, -1.f, 1.f);
    }
    if(const weapons::ModelTransform t = weapons::modelTransform(e.model); t.active)
    {
        ApplyScale(m, t.k, t.k, t.k);
        ApplyTranslation(m, t.offset.x, t.offset.y, t.offset.z);
    }
    glm::mat4 r;
    for(int c = 0; c < 4; c++)
    {
        for(int row = 0; row < 4; row++)
        {
            r[c][row] = m[c * 4 + row];
        }
    }
    return r;
}

namespace
{
void recordDrawnWeapon(const hands::State& s, int hand)
{
    const view::ViewEntity& ve = entities.weapon[hand];
    view::DrawnWeapon& d = drawnWeapons[hand];
    const int slot = weapons::slotForModel(ve.ent.model);
    if(!s.valid || !ve.ent.model || ve.ent.model->type != mod_alias || slot < 0 || slot == weapons::fistSlot())
    {
        d = view::DrawnWeapon{};
        return;
    }
    {
        float m[16];
        vec3_t origin, angles;
        VectorCopy(ve.ent.origin, origin);
        VectorCopy(ve.ent.angles, angles);
        R_EntityMatrix(m, origin, angles, ENTSCALE_DEFAULT);
        glm::mat4 entity;
        for(int c = 0; c < 4; c++)
        {
            for(int row = 0; row < 4; row++)
            {
                entity[c][row] = m[c * 4 + row];
            }
        }
        glm::mat4 handPose{held::axesFromAngles(&s.rot[hand][0], true)};
        handPose[3] = glm::vec4{s.pos[hand], 1.f};
        d.model = ve.ent.model;
        d.mirrored = ve.mirrored;
        d.inHand = glm::inverse(handPose) * entity;
        d.when = vr_gametime;
        d.mag = nullptr; // (magazineBox: its magazine, if one is in)
    }
}
} // namespace

// A turn given as the hands' angles (hands::angleVectors: the columns forward, left, up), and back.
[[nodiscard]] glm::mat3 anglesBasis(const glm::vec3& a)
{
    glm::vec3 f, r, u;
    hands::angleVectors(a, f, r, u);
    return glm::mat3{f, -r, u};
}
[[nodiscard]] glm::vec3 basisAngles(const glm::mat3& b)
{
    return hands::anglesFromVectors(glm::normalize(b[0]), glm::normalize(b[2]));
}

// A held weapon's turn at the holding hand's angles `rot`, as the hands' angles (setupWeapon draws its entity at
// these, the pitch negated).
[[nodiscard]] glm::vec3 weaponTurn(const glm::vec3& rot, int slot, bool mirrored)
{
    const glm::vec3 o = weaponAngleOffsets(slot, mirrored);
    return {rot.x - o.x, rot.y + o.y, rot.z + o.z};
}

namespace
{
[[nodiscard]] glm::vec3 heldWeaponTurn(const glm::vec3& rot, int holder, int slot, bool mirrored)
{
    const glm::mat3& r = wholeTurn[holder];
    if(r == glm::mat3{1.f})
    {
        return weaponTurn(rot, slot, mirrored);
    }
    const glm::vec3 before = basisAngles(glm::transpose(r) * anglesBasis(rot));
    return basisAngles(r * anglesBasis(weaponTurn(before, slot, mirrored)));
}
} // namespace

[[nodiscard]] glm::vec3 attachedTurn(const glm::vec3& rot, int slot, bool mirrored, const glm::vec3& handAt0, int holder)
{
    const glm::mat3 w = anglesBasis(holder >= 0 ? heldWeaponTurn(rot, holder, slot, mirrored) : weaponTurn(rot, slot, mirrored));
    const glm::mat3 w0 = anglesBasis(weaponTurn(glm::vec3{0.f}, slot, mirrored));
    return basisAngles(w * glm::transpose(w0) * anglesBasis(handAt0));
}

// The firing animation's move of a drawn weapon at the world point `at`: the rigid motion (least squares: Kabsch, by
// a polar decomposition) of the model's vertices round `at`, weighted by their distance beyond the nearest one's
// (over 7 cm), from its rest pose (frame 0) to the pose drawn this frame, as a world transform. A hand holding the
// weapon there (its arm, a hand steadying it) moves with it: the shotgun's kick takes the arm back with it, as the
// hand drawn at a vertex of the weapon did before round 21. The identity where the model doesn't move.
[[nodiscard]] glm::mat4 animationMotion(const view::ViewEntity& ve, const glm::vec3& at)
{
    za::Vector<glm::vec3>& rest = scratch.restVerts;
    za::Vector<glm::vec3>& now = scratch.nowVerts;
    if(!ve.ent.model || ve.ent.model->type != mod_alias || !anchor::posedVertices(ve.ent, ve.zeroBlend, rest, now))
    {
        return glm::mat4{1.f};
    }
    bool moved = false;
    for(za::SizeT i = 0; i < rest.size() && !moved; i++)
    {
        moved = rest[i] != now[i];
    }
    if(!moved)
    {
        return glm::mat4{1.f};
    }

    float m[16];
    render::anchorMatrix(ve, glm::vec3{0.f}, m);
    glm::mat4 a;
    for(int c = 0; c < 16; c++)
    {
        a[c / 4][c % 4] = m[c];
    }
    const float sigma = 0.07f * units::metresToUnits();
    float nearest = 1e30f;
    for(glm::vec3& p : rest)
    {
        p = glm::vec3{a * glm::vec4{p, 1.f}};
        nearest = za::fmin(nearest, glm::distance(p, at));
    }
    double total = 0.0;
    glm::dvec3 restMid{0.0}, nowMid{0.0};
    za::Vector<float>& weight = scratch.weight;
    weight.resize(rest.size());
    for(za::SizeT i = 0; i < rest.size(); i++)
    {
        now[i] = glm::vec3{a * glm::vec4{now[i], 1.f}};
        const float d = (glm::distance(rest[i], at) - nearest) / sigma;
        weight[i] = d < 4.f ? za::exp(-d * d) : 0.f;
        total += weight[i];
        restMid += glm::dvec3{rest[i]} * static_cast<double>(weight[i]);
        nowMid += glm::dvec3{now[i]} * static_cast<double>(weight[i]);
    }
    restMid /= total;
    nowMid /= total;
    glm::dmat3 h{0.0};
    double spread = 0.0;
    for(za::SizeT i = 0; i < rest.size(); i++)
    {
        if(weight[i] > 0.f)
        {
            const glm::dvec3 r = glm::dvec3{rest[i]} - restMid, n = glm::dvec3{now[i]} - nowMid;
            h += glm::outerProduct(n, r) * static_cast<double>(weight[i]);
            spread += glm::dot(r, r) * weight[i];
        }
    }
    // The turn nearest h: its polar factor, with a touch of the identity (points in a line or a plane: the least turn).
    glm::dmat3 x = h + glm::dmat3{1.0} * (1e-4 * spread + 1e-12);
    for(int k = 0; k < 30; k++)
    {
        x = 0.5 * (x + glm::transpose(glm::inverse(x)));
    }
    if(!(glm::determinant(x) > 0.5))
    {
        x = glm::dmat3{1.0};
    }
    glm::mat4 out{glm::mat3{x}};
    out[3] = glm::vec4{glm::vec3{nowMid - x * restMid}, 1.f};
    return out;
}

// A share `t` of a rigid motion, about `centre`: its turn there slerped, the centre's move scaled.
[[nodiscard]] glm::mat4 partMotion(const glm::mat4& m, float t, const glm::vec3& centre)
{
    if(t >= 1.f)
    {
        return m;
    }
    t = za::fmax(t, 0.f);
    const glm::mat3 r = glm::mat3_cast(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, glm::quat_cast(glm::mat3{m}), t));
    const glm::vec3 move = glm::vec3{m * glm::vec4{centre, 1.f}} - centre;
    glm::mat4 out{r};
    out[3] = glm::vec4{centre - r * centre + move * t, 1.f};
    return out;
}

// A pose (a place, the hands' angles) moved by a rigid motion.
void movePose(const glm::mat4& m, glm::vec3& pos, glm::vec3& angles)
{
    pos = glm::vec3{m * glm::vec4{pos, 1.f}};
    angles = basisAngles(glm::mat3{m} * anglesBasis(angles));
}

// vr_show_weapon_hotspots: the held weapons' hotspots marked (grips green, blades' grips orange, the one the Weapon
// Offsets page edits white), each with a faint ball as big as its bias.
// The hotspots of the weapon in `hand` marked, `edited` (-1 none) white.
void drawHotspots(int hand, int edited)
{
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        const WorldHotspot& w = worldHotspots[hand][i];
        if(w.type == weapons::HotspotType::None)
        {
            continue;
        }
        const glm::vec4 colour = i == edited                               ? glm::vec4{1.f, 1.f, 1.f, 1.f}
                                 : w.type == weapons::HotspotType::Grip ? glm::vec4{0.2f, 1.f, 0.3f, 1.f}
                                 : w.type == weapons::HotspotType::Cup  ? glm::vec4{0.3f, 0.6f, 1.f, 1.f}
                                                                        : glm::vec4{1.f, 0.6f, 0.1f, 1.f};
        if(w.type == weapons::HotspotType::Blade)
        {
            lines::line(w.pos, w.end, 0.35f, colour, colour);
        }
        lines::point(w.pos, 1.2f, colour);
        if(w.bias > 0.f)
        {
            lines::point(w.pos, 2.f * w.bias, glm::vec4{colour.r, colour.g, colour.b, 0.15f});
        }
    }
}
void showHotspots()
{
    if(!vr_show_weapon_hotspots.value)
    {
        return;
    }
    const int edited = CLAMP(1, static_cast<int>(vr_weapon_hotspot.value), weapons::maxHotspots) - 1;
    for(int hand = 0; hand < 2; hand++)
    {
        if(entities.weapon[hand].visible)
        {
            drawHotspots(hand, edited);
        }
    }
}
void setupGhosts()
{
    for(int hand = 0; hand < 2; hand++)
    {
        view::ViewEntity& ve = entities.ghost[hand];
        const GhostRequest& g = ghostRequests[hand];
        if(g.frame != host_framecount || !g.model)
        {
            ve.visible = false;
            continue;
        }
        // Placed as setupWeapon places a held weapon (the hand's pose, the weapon's angle offsets; no carried gun,
        // no Hand and Weapon Together turn: a take doesn't record them), the empty hand as the fist's slot.
        const bool mirrored = hand == HAND_OFF;
        const int slot = isHandModel(g.model) ? weapons::fistSlot() : weapons::slotForModel(g.model);
        const glm::vec3 wt = slot >= 0 ? weaponTurn(g.rot, slot, mirrored) : g.rot;
        place(ve, g.model, g.pos, {-wt.x, wt.y, wt.z}, 0, mirrored);
        ve.ent.alpha = static_cast<unsigned char>(ENTALPHA_ENCODE(CLAMP(0.05f, g.alpha, 1.f)));
        ve.zeroBlend = 0.f;
        ve.morph = 0.f;
        ve.lightMultiply = true;
        ve.lightMod = glm::vec3{1.2f, 1.7f, 2.6f}; // a bright cold tint: not the player's own
    }
}

namespace
{
// The magazine attached to the gun in `hand` (`ve`: drawn; held or carried), as a box (hands::State::magBox: grips and hits
// on it, the server's too): immersive, the gun's magazine in (the server's flags), not the posing mode's weapon.
void magazineBox(hands::State& s, int hand, const view::ViewEntity& ve, qmodel_t* model, bool floating)
{
    s.magBoxValid[hand] = false;
    const int flags = cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
    const MagMount* mount = !floating && cl.stats[protocol::STAT_QVR_RELOADMODE] == 3 && !(flags & weaponFlagNoMag)
                                ? magMountFor(model)
                                : nullptr;
    MagShape shape;
    if(!mount || !magazineShape(*mount, viewModel(mount->model), shape))
    {
        return;
    }
    glm::vec3(&box)[4] = s.magBox[hand];
    box[0] = view::modelPoint(ve, shape.centre);
    for(int i = 0; i < 3; i++)
    {
        box[i + 1] = view::modelPoint(ve, shape.centre + shape.axis[i] * shape.half[i]) - box[0];
    }
    s.magBoxValid[hand] = true;
    // For its collision (view::DrawnWeapon::mag): the magazine and its box in the hand's frame, as the gun drawn this frame.
    view::DrawnWeapon& d = drawnWeapons[hand];
    if(d.model == ve.ent.model && d.when == vr_gametime && vr_reload_mag_collide.value)
    {
        glm::mat4 handPose{held::axesFromAngles(&s.rot[hand][0], true)};
        handPose[3] = glm::vec4{s.pos[hand], 1.f};
        const glm::mat4 toHand = glm::inverse(handPose);
        d.mag = viewModel(mount->model);
        d.magBox[0] = glm::vec3{toHand * glm::vec4{box[0], 1.f}};
        for(int i = 1; i < 4; i++)
        {
            d.magBox[i] = glm::mat3{toHand} * box[i];
        }
    }
    if(vr_reload_show_ports.value)
    {
        // Its box (blue): where a hand holds it (with Pull Reach round it) and a hit knocks it out.
        const glm::vec4 colour{0.3f, 0.5f, 1.f, 0.7f};
        for(int a = 1; a < 4; a++)
        {
            const int b = a % 3 + 1, c = (a + 1) % 3 + 1;
            for(const float sb : {-1.f, 1.f})
            {
                for(const float sc : {-1.f, 1.f})
                {
                    const glm::vec3 m = box[0] + box[b] * sb + box[c] * sc;
                    lines::line(m - box[a], m + box[a], 0.08f, colour, colour);
                }
            }
        }
    }
}

// Whether the empty other hand is on the magazine of the gun in `hand` (hands::State::onMagazine: its hotspot
// HS_MAGAZINE next frame, gripping there holds it): holding it two-handed, or within Pull Reach of its box and nearer it
// than the gun's chosen grip (held by its handle) or its handle (carried).
void magazineHand(hands::State& s, int hand, bool carried)
{
    const int other = 1 - hand;
    bool on = false;
    if(s.magBoxValid[hand] && held::handEmpty(other) && !twohand::carrying(other) && !flashlight::holds(other))
    {
        // (As tracked: the drawn hand and gun are held out of each other, vr_body_collide.)
        const float d = boxDistance(s.magBox[hand], s.pos[other] - selfcollide::drawnOffset(other) + selfcollide::drawnOffset(hand));
        const bool chosenMag = magSpot[hand] >= 0 && chosenGrip[hand] == magSpot[hand];
        if(vr_reload_debug.value >= 3.f)
        {
            Con_Printf("reload: %s hand %.2f units off the magazine's box (Pull Reach %g), the gun's chosen grip %d (the "
                       "magazine's %d)\n", other == HAND_MAIN ? "main" : "off", static_cast<double>(d),
                static_cast<double>(vr_reload_pull_reach.value), chosenGrip[hand], magSpot[hand]);
        }
        if(s.grip2HValid[hand] && chosenMag && twohand::helping(other))
        {
            on = true; // (held two-handed by it)
        }
        else if(d <= za::max(vr_reload_pull_reach.value, 0.f))
        {
            on = true;
            if(s.grip2HValid[hand] && chosenGrip[hand] >= 0 && !chosenMag)
            {
                on = false; // (nearer the gun's other grip)
            }
            const view::ViewEntity& gun = entities.weapon[hand];
            if(carried && glm::distance(s.pos[other], glm::vec3{gun.ent.origin[0], gun.ent.origin[1], gun.ent.origin[2]}) < d)
            {
                on = false; // (nearer the carried gun's handle: taken back)
            }
        }
    }
    if(vr_reload_debug.value >= 2.f && on != s.onMagazine[other])
    {
        Con_Printf("reload: %s hand %s the magazine of the %s gun\n", other == HAND_MAIN ? "main" : "off", on ? "on" : "off",
            carried ? "carried" : "held");
    }
    s.onMagazine[other] = on;
}
} // namespace

// `floating`: the posing mode's weapon (vr_posing.cpp), never a carried gun.
void setupWeapon(hands::State& s, int hand, qmodel_t* model, int frame, bool floating)
{
    view::ViewEntity& ve = entities.weapon[hand];
    const int slot = weapons::slotForModel(model);

    // A gun hanging from its foregrip (the hand-off, vr_twohand.cpp): drawn as the hand that let it go
    // held it, carried by this hand.
    twohand::HeldAs held{s.pos[hand], s.visualRot[hand], hand == HAND_OFF};
    const bool carried = !floating && twohand::carriedWeapon(s, hand, held);
    weaponCarried[hand] = carried;
    const bool mirrored = held.mirrored;
    drawnAs[hand] = held; // what the weapon's attachments (its button) follow

    const glm::vec3& rot = held.rot;
    // (A carried gun is drawn as the hand that let it go held it, without this hand's offset turn.)
    const glm::vec3 wt = carried ? weaponTurn(rot, slot, mirrored) : heldWeaponTurn(rot, hand, slot, mirrored);

    // A spent enemy rifle crackling in the hand shakes (vr_shock.cpp gunShake; vr_enemygun_spent_shake): a small jitter
    // of its place and turn, fading with its arcs (the author's note map1_2026-10-08_13-56-46).
    glm::vec3 drawnPos = held.pos;
    glm::vec3 drawnAngles{-wt.x, wt.y, wt.z};
    if(const float k = floating ? 0.f : shock::gunShake(hand) * vr_enemygun_spent_shake.value; k > 0.f)
    {
        const float t = static_cast<float>(vr_gametime);
        drawnPos += k * glm::vec3{std::sin(t * 97.f), std::sin(t * 113.f + 1.3f), std::sin(t * 89.f + 2.1f)};
        drawnAngles += 4.f * k * glm::vec3{std::sin(t * 71.f + 0.7f), std::sin(t * 83.f + 2.9f), std::sin(t * 101.f + 4.2f)};
    }
    place(ve, model, drawnPos, drawnAngles, frame, mirrored);
    if(!floating)
    {
        recordDrawnWeapon(s, hand); // (for its body in Box3D)
    }
    // (vr_grapple_debug: the grappling gun's frame as drawn, when it changes: 0 the hook in it, 2 out.)
    int(&drawnFrame)[2] = grappleDebug.drawnFrame;
    if(vr_grapple_debug.value && developer.value && model && modelmeta::is(model, modelmeta::Id::VGrpple) &&
        frame != drawnFrame[hand])
    {
        Con_Printf("grapple: hand %d's gun drawn with frame %d (was %d)\n", hand, frame, drawnFrame[hand]);
    }
    drawnFrame[hand] = model && modelmeta::is(model, modelmeta::Id::VGrpple) ? frame : -1;

    // Steadied in the "fixed" two-handed display mode (a grip hotspot: the helping hand drawn on it): the weapon's
    // own blend towards frame 0 for that grip (old engine's V_SetupHandViewEnts).
    // Not held anywhere: a hotspot's own setting (NOTES.md vrfiringrange_2026-10-01_17-13-04).
    const bool fixed2H = model && slot >= 0 && hasGripHotspot(slot);
    ve.zeroBlend = weapons::value(slot, fixed2H && twohand::helping(1 - hand) && !twohand::freeHelping(1 - hand) ? Key::TwoHZeroBlend : Key::ZeroBlend);

    // The super shotgun broken open: how far its barrels are drawn down now (its parts: setupSsgParts).
    const float ssgOpen = ssgStep(ssgHands[hand], floating ? nullptr : model,
        cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2]);

    // Its loading port (a gun held by its handle or carried; vr_reload_mode 3): where a round from the ammo pouch goes in.
    s.loadPortValid[hand] = false;
    if(model && slot >= 0)
    {
        const auto& info = modelmeta::get(model);
        for(const LoadPort& port : loadPorts)
        {
            if(info.is(port.model))
            {
                // (Its point and ways in the model's space: modelLoadPort, as the server's guns lying about.)
                glm::vec3 at, axis, out;
                if(!view::modelLoadPort(model, ssgOpen, at, axis, out))
                {
                    break;
                }
                s.loadPort[hand] = view::modelPoint(ve, at);
                s.loadPortValid[hand] = true;
                const glm::vec3 way = view::modelPoint(ve, at + axis) - s.loadPort[hand];
                s.loadPortAxis[hand] = glm::length(way) > 1e-4f ? glm::normalize(way) : glm::vec3{0.f};
                const glm::vec3 face = view::modelPoint(ve, at + out) - s.loadPort[hand];
                s.loadPortFace[hand] = glm::length(face) > 1e-4f ? glm::normalize(face) : glm::vec3{0.f};
                if(vr_reload_show_ports.value && cl.stats[protocol::STAT_QVR_RELOADMODE] == 3)
                {
                    // The gun's point and radius (a held magazine's reference point and radius: heldRoundRef's, drawn by
                    // setupMagazines); a magazine's well also its pull's reach (blue), where a gripping hand holds it.
                    const float within = za::max(port.radius->value, 0.f);
                    lines::point(s.loadPort[hand], 0.6f, glm::vec4{1.f, 1.f, 0.2f, 0.9f});
                    lines::point(s.loadPort[hand], 2.f * within, glm::vec4{0.2f, 1.f, 0.3f, 0.3f});
                }
            }
        }
    }
    magazineBox(s, hand, ve, model, floating);

    if(model && slot >= 0 && !carried) // a carried gun has no aim
    {
        s.muzzle[hand] = view::anchorPosition(ve,
            weapons::anchor(slot, Key::MuzzleAnchor),
            weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
        s.muzzleValid[hand] = true;
    }
    else
    {
        s.muzzleValid[hand] = false;
    }

    // The hotspots, as drawn; a blade's grip between its share of the way from the hand to the tip, less and more.
    const glm::mat4 hsFrame = hotspotFrame(ve.ent, mirrored);
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        const weapons::Hotspot h = model && slot >= 0 ? weapons::hotspot(slot, i) : weapons::Hotspot{};
        WorldHotspot& w = worldHotspots[hand][i];
        w = WorldHotspot{h.type, glm::vec3{0.f}, glm::vec3{0.f}, h.bias, 0.f, h.angles, h.style, h.overlap, h.visualPos,
            h.visualAngles, h};
        if(weapons::isGripType(h.type))
        {
            w.pos = w.end = glm::vec3{hsFrame * glm::vec4{h.pos, 1.f}};
            // The super shotgun open: its fore-end (where the other hand holds it) turned down with the barrels.
            if(ssgOpen > 0.f)
            {
                const glm::vec3 was = w.pos;
                w.pos = w.end = ssgTurnWorld(ve, w.pos, ssgOpen);
                w.ssgTurned = w.pos != was;
            }
        }
        else if(h.type == weapons::HotspotType::Blade && s.muzzleValid[hand])
        {
            w.share = h.pos.x;
            const glm::vec3 toTip = s.muzzle[hand] - held.pos;
            w.pos = held.pos + toTip * weapons::bladeFrom(h);
            w.end = held.pos + toTip * weapons::bladeTo(h);
        }
    }

    // Immersive: the attached magazine is a grip too, beside the gun's own (the author: whichever the hand is on).
    magSpot[hand] = -1;
    if(s.magBoxValid[hand] && !carried && model && slot >= 0)
    {
        for(int i = 0; i < weapons::maxHotspots && magSpot[hand] < 0; i++)
        {
            if(worldHotspots[hand][i].type == weapons::HotspotType::None)
            {
                magSpot[hand] = i;
            }
        }
        if(magSpot[hand] >= 0)
        {
            const MagHold& hold = magHolds[magMountFor(model)->kind];
            weapons::Hotspot def{weapons::HotspotType::Grip, glm::vec3{0.f}, 0.f};
            def.visualPos = {hold.offset[0]->value, hold.offset[1]->value, hold.offset[2]->value};
            def.visualAngles = {hold.offset[3]->value, hold.offset[4]->value, hold.offset[5]->value};
            def.manual = hold.fingers->value >= 0.5f;
            for(int f = 0; f < handrig::FingerCount; f++)
            {
                def.curl[f] = CLAMP(0.f, hold.curl[f]->value, 1.f);
            }
            WorldHotspot& w = worldHotspots[hand][magSpot[hand]];
            w = WorldHotspot{def.type, s.magBox[hand][0], s.magBox[hand][0], 0.f, 0.f, glm::vec3{0.f}, def.style,
                def.overlap, def.visualPos, def.visualAngles, def};
        }
    }
    s.grip2HValid[hand] = fixed2H && !carried;
    if(model && slot >= 0 && slot != weapons::fistSlot() && vr_debug_2h_grip.value >= 2.f && vr_gametime >= carriedPrintAt[hand])
    {
        // vr_debug_2h_grip 2: where its weapon is drawn (held or carried: a hand-off's continuity); carried, how far the
        // hand carrying it is drawn from each of its grip and blade hotspots.
        carriedPrintAt[hand] = vr_gametime + 0.5;
        Con_Printf("2h grip: %s hand %s %s: drawn at %.1f %.1f %.1f, angles %.1f %.1f %.1f%s\n",
            hand == HAND_MAIN ? "main" : "off", carried ? "carrying" : "holding", model->name, ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2],
            ve.ent.angles[0], ve.ent.angles[1], ve.ent.angles[2], mirrored ? ", mirrored" : "");
        glm::vec3 handPos, handRot;
        if(carried && twohand::carryingHand(s, hand, handPos, handRot))
        {
            const glm::vec3 tip = view::anchorPosition(ve, weapons::anchor(slot, Key::MuzzleAnchor),
                weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
            for(int i = 0; i < weapons::maxHotspots; i++)
            {
                const weapons::Hotspot h = weapons::hotspot(slot, i);
                glm::vec3 a{hsFrame * glm::vec4{h.pos, 1.f}}, b = a;
                if(h.type == weapons::HotspotType::Blade)
                {
                    a = held.pos + (tip - held.pos) * weapons::bladeFrom(h);
                    b = held.pos + (tip - held.pos) * weapons::bladeTo(h);
                }
                else if(!weapons::isGripType(h.type))
                {
                    continue;
                }
                const float len2 = glm::dot(b - a, b - a);
                const glm::vec3 p = len2 > 0.f ? a + (b - a) * za::clamp(glm::dot(handPos - a, b - a) / len2, 0.f, 1.f) : a;
                Con_Printf("2h grip: %s hand carrying %s: %.1f units from its hotspot %d\n", hand == HAND_MAIN ? "main" : "off",
                    model->name, glm::distance(handPos, p), i);
            }
        }
    }
    carriedTipValid[hand] = false;
    if(carried && model && slot >= 0)
    {
        // Where the hand that takes it back closes: its handle.
        twohand::setCarriedHandle(hand, glm::vec3{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]});
        // Its tip, as drawn (view::carriedWeaponPose: the melee's line).
        carriedTip[hand] = view::anchorPosition(ve, weapons::anchor(slot, Key::MuzzleAnchor),
            weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
        carriedTipValid[hand] = true;
        carriedAngles[hand] = glm::vec3{-ve.ent.angles[0], ve.ent.angles[1], ve.ent.angles[2]};
        glm::vec3 f, r, u;
        hands::angleVectors(s.rot[hand], f, r, u);
        const glm::vec3 points[2]{held.pos - s.pos[hand], carriedTip[hand] - s.pos[hand]};
        for(int i = 0; i < 2; i++)
        {
            carriedInHand[hand][i] = {glm::dot(points[i], f), glm::dot(points[i], r), glm::dot(points[i], u)};
        }
    }
    if(s.grip2HValid[hand])
    {
        // The grip the other hand takes: the one nearest it, less its bias; kept while it holds it.
        int& chosen = chosenGrip[hand];
        if(chosenGripSlot[hand] != slot)
        {
            chosen = -1;
            chosenGripSlot[hand] = slot;
        }
        const bool holdsOne = twohand::helping(1 - hand) && chosen >= 0 && weapons::isGripType(worldHotspots[hand][chosen].type);
        if(!holdsOne)
        {
            float best = 1e30f;
            for(int i = 0; i < weapons::maxHotspots; i++)
            {
                // A cup is where the helping hand's palm goes: taken by the palm (round 21, third pass).
                // (As tracked: the hands drawn out of each other and the body, vr_body_collide, choose as without it.)
                const WorldHotspot& w = worldHotspots[hand][i];
                const glm::vec3 from = w.type == weapons::HotspotType::Cup ? hands::palmPoint(s, 1 - hand) : s.pos[1 - hand];
                // (The magazine: its box's surface, wherever along it.)
                const float d = i == magSpot[hand]
                                    ? boxDistance(s.magBox[hand], from - selfcollide::drawnOffset(1 - hand) + selfcollide::drawnOffset(hand))
                                    : glm::distance(from - selfcollide::drawnOffset(1 - hand), w.pos - selfcollide::drawnOffset(hand)) - w.bias;
                if(weapons::isGripType(w.type) && d < best)
                {
                    best = d;
                    chosen = i;
                }
            }
        }
        s.grip2H[hand] = worldHotspots[hand][chosen].pos;
        s.grip2HBias[hand] = worldHotspots[hand][chosen].bias;
        s.grip2HSticky[hand] = weapons::hotspot(slot, chosen).sticky;
        if(chosen == magSpot[hand])
        {
            // The magazine is taken within Pull Reach of its box (vr_twohand.cpp takes a grip within gripTakeUnits of its
            // point, less its bias: the bias makes that the box's).
            // (As tracked: the drawn hand and gun are held out of each other, vr_body_collide.)
            const float reach = za::max(vr_reload_pull_reach.value, 0.f);
            const glm::vec3 at = s.pos[1 - hand] - selfcollide::drawnOffset(1 - hand) + selfcollide::drawnOffset(hand);
            const float atBox = boxDistance(s.magBox[hand], at);
            s.grip2HBias[hand] = glm::distance(at, s.grip2H[hand]) - (atBox - reach + twohand::gripTakeUnits);
            s.grip2HSticky[hand] = 1.f;
        }
        // A cup: a cup hotspot, or a grip not ahead of the holding hand (beside it, under it: a grip the two hands can't
        // aim by; it is held as a cup).
        const WorldHotspot& spot = worldHotspots[hand][chosen];
        glm::vec3 fwd, right, up;
        hands::angleVectors(s.rot[hand], fwd, right, up);
        const glm::vec3 toGrip = spot.pos - held.pos;
        const float along = glm::dot(toGrip, fwd);
        s.grip2HCup[hand] = spot.type == weapons::HotspotType::Cup ||
                            along < 5.f || along < za::cos(glm::radians(35.f)) * glm::length(toGrip);
        s.grip2HPalm[hand] = spot.type == weapons::HotspotType::Cup;
    }
    magazineHand(s, hand, carried);

    // Just drawn from a holster: eased from its holstered pose into the hand (vr_drawblend.cpp; drawn only, the muzzle
    // and the grips above are its real place).
    drawblend::hand(s, hand, ve.ent, !floating && !carried && model && slot >= 0 && slot != weapons::fistSlot() && !isHandModel(model));

    // The empty hand's "weapon" is a hand model; hands are drawn separately.
    if(isHandModel(model))
    {
        ve.visible = false;
    }
    else if(model && slot >= 0)
    {
        queueWeaponText(held.rot, mirrored, hand, ve, slot);
    }

    // Morphing from the other ammo's model (vr_weapon_morph_time): that one too, where it would be held, going out.
    view::ViewEntity& old = entities.weaponMorph[hand];
    const float t = updateMorph(hand, model);
    const Morph& m = morphs[hand];
    const int oldSlot = weapons::slotForModel(m.from);
    ve.morph = 0.f;
    old.morph = 0.f;
    if(m.from && oldSlot >= 0 && ve.visible)
    {
        const glm::vec3 ot = carried ? weaponTurn(rot, oldSlot, mirrored) : heldWeaponTurn(rot, hand, oldSlot, mirrored);
        place(old, m.from, held.pos, {-ot.x, ot.y, ot.z},
            za::clamp(frame, 0, za::max(m.from->numframes - 1, 0)), mirrored);
        const bool oldFixed2H = hasGripHotspot(oldSlot);
        old.zeroBlend = weapons::value(oldSlot, oldFixed2H && twohand::helping(1 - hand) && !twohand::freeHelping(1 - hand) ? Key::TwoHZeroBlend : Key::ZeroBlend);
        ve.morph = morphValue(t, m.kind, true);
        old.morph = morphValue(t, m.kind, false);
    }
    else
    {
        old.visible = false;
    }

    // The lava nailguns glow (vr_lavagun_light), fading in and out with a morph.
    const bool lavaIn = ve.visible && slot >= 0 && emissive::isLavaGun(model);
    const bool lavaOut = old.visible && emissive::isLavaGun(m.from);
    if(lavaIn || lavaOut)
    {
        const view::ViewEntity& lit = lavaIn ? ve : old;
        emissive::lavaGunLight(hand, lavaGlowPosition(lit.ent, mirrored, lit.zeroBlend, lavaIn ? slot : oldSlot),
            (lavaIn ? t : 0.f) + (lavaOut ? 1.f - t : 0.f));
    }
}
} // namespace view_detail
