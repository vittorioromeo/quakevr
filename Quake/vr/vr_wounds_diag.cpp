// vr_wounds_diag.cpp -- the wounds' tests and reports: vr_wounds_test, vr_wounds_dump, vr_wounds_info, vr_gore_hands_test,
// vr_wrist_blood_test, vr_gore_wash_test, vr_gore_hands_info and vr_gore_spatter_test.
// Split from vr_wounds.cpp (a pure move); what they share is in vr_wounds_internal.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_wounds_internal.hpp"
#include "vr_dents.hpp"
#include "vr_bloodstream.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_modelcollide.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"
#include "vr_avatar.hpp"
#include "vr_client.hpp"
#include "vr_held.hpp"

#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Remove.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Vocabulary/Span.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <string.h>


namespace qvr::wounds
{
using namespace detail;

namespace
{

// A brush model's box mask (`layer`): the texels with blood in each of its six cells (+x -x +y -y +z -z, the model's
// own axes), and the cell of the side facing `from` (the world).
int boxCells(int layer, int cells[6], const glm::vec3& from)
{
    const Mask& m = masks[static_cast<za::SizeT>(layer)];
    za::Vector<byte> rgba(static_cast<za::SizeT>(m.w) * static_cast<za::SizeT>(m.h) * 4);
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    attach(GL_READ_FRAMEBUFFER, layer);
    glReadPixels(0, 0, m.w, m.h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
    for(int i = 0; i < 6; i++)
    {
        cells[i] = 0;
    }
    for(int y = 0; y < m.h; y++)
    {
        for(int x = 0; x < m.w; x++)
        {
            if(rgba[(static_cast<za::SizeT>(y) * static_cast<za::SizeT>(m.w) + static_cast<za::SizeT>(x)) * 4] > 0)
            {
                cells[za::min(y * 2 / za::max(m.h, 1), 1) * 3 + za::min(x * 3 / za::max(m.w, 1), 2)]++;
            }
        }
    }
    // The side facing `from`: its way in the model's frame (its axes as a held brush model's are placed: vr_held.cpp).
    const entity_t& e = *m.ent;
    const glm::vec3 local = glm::transpose(held::axesFromAngles(e.angles, true)) * (from - thingCentre(e));
    const glm::vec3 a = glm::abs(local);
    const int axis = a.x >= a.y && a.x >= a.z ? 0 : a.y >= a.z ? 1 : 2;
    return axis * 2 + (local[axis] >= 0.f ? 0 : 1);
}

} // namespace

namespace
{

// The view masks' blood (off hand, main hand, body): texels with any (-1: no mask), and their blood summed (1 a texel
// full).
void viewBlood(int count[3], double sum[3])
{
    entity_t* own[3]{};
    view::woundTargets(own);
    const za::SizeT side = static_cast<za::SizeT>(za::max(layerSize, fineSize));
    za::Vector<byte> rgba(side * side * 4);
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    for(int part = 0; part < 3; part++)
    {
        entity_t* e = own[part < 2 ? 1 + part : 0];
        count[part] = -1;
        sum[part] = 0.0;
        const int layer = e && array ? acquire(e, true, false) : -1;
        if(layer < 0)
        {
            continue;
        }
        const Mask& m = masks[static_cast<za::SizeT>(layer)];
        count[part] = 0;
        int in[2], side[2];
        const int n = layersOf(layer, in, side);
        za::Vector<byte> other(static_cast<za::SizeT>(m.w) * static_cast<za::SizeT>(m.h), 0);
        for(int k = 0; k < n; k++) // (your body's: both sides; the blood on it not yours too)
        {
            if(hasOther(layer))
            {
                attachBlood(GL_READ_FRAMEBUFFER, in[k]);
                glPixelStorei(GL_PACK_ALIGNMENT, 1); // (rows of any width)
                glReadPixels(0, 0, m.w, m.h, GL_RED, GL_UNSIGNED_BYTE, other.data());
                glPixelStorei(GL_PACK_ALIGNMENT, 4);
            }
            attach(GL_READ_FRAMEBUFFER, in[k]);
            glReadPixels(0, 0, m.w, m.h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            for(int p = 0; p < m.w * m.h; p++)
            {
                const int r = za::max<int>(rgba[static_cast<za::SizeT>(p) * 4], other[static_cast<za::SizeT>(p)]);
                count[part] += r > 0 ? 1 : 0;
                sum[part] += r / 255.0;
            }
        }
    }
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
}

void printBoxCells(int layer, const glm::vec3& from, const char* fromName)
{
    int cells[6];
    const int facing = boxCells(layer, cells, from);
    static constexpr const char* names[6] = {"+x", "-x", "+y", "-y", "+z", "-z"};
    Con_Printf("    its sides: +x %d, -x %d, +y %d, -y %d, +z %d, -z %d; facing %s: %s (%d texels)\n", cells[0], cells[1], cells[2], cells[3],
        cells[4], cells[5], fromName, names[facing], cells[facing]);
}

} // namespace


// vr_gore_hands_test [off|main] [amount]: a gib's blood on a hand, as taking one (the main hand; vr_gore_hands).
void handsTest_f()
{
    const int hand = Cmd_Argc() > 1 && ZA_STRCMP(Cmd_Argv(1), "off") == 0 ? 0 : 1;
    const float amount = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : za::max(0.25f, vr_gore_hands.value);
    if(!vr_wounds.value)
    {
        skinParts[hand].gib = za::clamp(static_cast<int>(za::lround(amount * 2.f)), 1, 3);
        Con_Printf("vr_gore_hands_test: the %s hand's skin %d (the wound skins)\n", hand ? "main" : "off", skinParts[hand].gib);
        return;
    }
    if(!ensureTexture())
    {
        return;
    }
    smearHand(hand, za::max(1, static_cast<int>(amount * 4.f + 0.5f)), amount);
    end();
    Con_Printf("vr_gore_hands_test: a gib's blood on the %s hand\n", hand ? "main" : "off");
}

// A local seam wound starting on either model, for both-direction regression and the in-game Debug page.
// vr_wrist_blood_test hand|arm [main|off]: one wound at the shared hand/arm seam.
void wristTest_f()
{
    const int hand = Cmd_Argc() > 2 && ZA_STRCMP(Cmd_Argv(2), "off") == 0 ? 0 : 1;
    const bool arm = Cmd_Argc() > 1 && ZA_STRCMP(Cmd_Argv(1), "arm") == 0;
    entity_t* own[3]{};
    avatar::Skeleton sk;
    view::woundTargets(own);
    if(!vr_wounds.value || !own[0] || !own[hand + 1] || !avatar::skeleton(sk) || !ensureTexture())
    {
        Con_Printf("wrist blood: enable Dynamic Wounds and the body with jointed hands\n");
        return;
    }
    entity_t* source = own[arm ? 0 : hand + 1];
    const int layer = acquire(source, true, true);
    if(layer < 0)
    {
        return;
    }
    const glm::vec3 axis = glm::normalize(sk.wrist[hand] - sk.elbow[hand]);
    za::Vector<Splat> splats;
    splats.pushBack(woundSplat(sk.wrist[hand], 0.12f * sk.m2w, glm::vec3{0.f, 0.f, 1.f}, axis,
        1.f, 0.12f * sk.m2w, -1.f, 0.f, paintBlood));
    int before[2]{};
    for(int i = 0; i < 2; i++)
    {
        const int l = acquire(own[i == 0 ? 0 : hand + 1], true, true);
        double sum;
        if(l >= 0)
        {
            int in[2], side[2];
            const int n = layersOf(l, in, side);
            for(int j = 0; j < n; j++)
            {
                int count;
                maskBlood(in[j], count, sum, l);
                before[i] += count;
            }
        }
    }
    paint(layer, source, splats);
    bridgeOwnWound(source, splats);
    end();
    int after[2]{};
    for(int i = 0; i < 2; i++)
    {
        const int l = acquire(own[i == 0 ? 0 : hand + 1], true, false);
        double sum;
        if(l >= 0)
        {
            int in[2], side[2];
            const int n = layersOf(l, in, side);
            for(int j = 0; j < n; j++)
            {
                int count;
                maskBlood(in[j], count, sum, l);
                after[i] += count;
            }
        }
    }
    Con_Printf("wrist blood: from %s %s, body %d -> %d, hand %d -> %d texels\n", arm ? "arm" : "hand",
        hand ? "main" : "off", before[0], after[0], before[1], after[1]);
}

// vr_gore_wash_test [amount]: `amount` (0.25) of the blood washed off your hands and body at once, as water over them
// would (vr_gore_wash_once: each texel once).
void washTest_f()
{
    const float amount = Cmd_Argc() > 1 ? static_cast<float>(atof(Cmd_Argv(1))) : 0.25f;
    if(!vr_wounds.value || !ensureTexture())
    {
        return;
    }
    entity_t* own[3]{};
    view::woundTargets(own);
    int washed = 0;
    for(entity_t* e : own)
    {
        const int layer = e ? acquire(e, true, false) : -1;
        if(layer >= 0)
        {
            washUnder(layer, e, e->origin[2] + 256.f, za::clamp(amount, 0.f, 1.f));
            washed++;
        }
    }
    end();
    Con_Printf("vr_gore_wash_test: %.2f washed off %d of your masks (%s)\n", static_cast<double>(amount), washed,
        vr_gore_wash_once.value != 0.f ? "each texel once" : "edges more");
}

// vr_gore_hands_info: the player's wounds kept to re-open, the wash, the blood on the hands and the body (texels), and
// on your gear (vr_gore_gear: the weapons and props you hold or held).
void handsInfo_f()
{
    const double now = cl.time;
    Con_Printf("gore hands: %d wounds kept, %s, last wash %.1f s ago, re-open %s", static_cast<int>(playerWounds.size()),
        lastWash > -1e8 && now - lastWash < 0.25 ? "washing" : "dry", lastWash > -1e8 ? now - lastWash : -1.0,
        !reopenPending ? "none" : reopenStart < 0.0 ? "pending" : "under way");
    if(reopenPending && reopenStart < 0.0)
    {
        Con_Printf(" (in %.1f s)", static_cast<double>(vr_gore_reopen_delay.value) - (now - lastWash));
    }
    const int carried[2]{cl.stats[protocol::STAT_QVR_CARRYOFF], cl.stats[protocol::STAT_QVR_CARRYMAIN]};
    Con_Printf(", carried %d %s, %d %s\n", carried[0], isGib(carried[0]) ? "(a gib)" : "", carried[1], isGib(carried[1]) ? "(a gib)" : "");
    if(vr_wounds.value && array)
    {
        int count[3];
        double sum[3];
        viewBlood(count, sum);
        static constexpr const char* names[3] = {"off hand", "main hand", "body"};
        for(int i = 0; i < 3; i++)
        {
            if(count[i] < 0)
            {
                Con_Printf("  %s: no mask\n", names[i]);
            }
            else
            {
                Con_Printf("  %s: blood on %d texels (%.1f)\n", names[i], count[i], sum[i]);
            }
        }
    }
    else
    {
        Con_Printf("  wound skins: off hand %d, main hand %d (gib %d %d), body washed %s\n", skinParts[0].gib, skinParts[1].gib,
            skinParts[0].gibEnt, skinParts[1].gibEnt, skinParts[2].washed > -1e8 ? "yes" : "no");
    }
    // Your gear's blood (vr_gore_gear): held, lying about, kept for a hand.
    for(int i = 0; i < maskCount() && array; i++)
    {
        const Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.gear || !m.ent)
        {
            continue;
        }
        int count;
        double sum;
        maskBlood(i, count, sum);
        entity_t* inHolster[protocol::numHolsters];
        int holsterWeapon[protocol::numHolsters];
        holstered(inHolster, holsterWeapon);
        const char* where = parkIndex(m.ent) >= 0 ? "kept"
                            : m.ent == gearOf(0) ? "in the off hand"
                            : m.ent == gearOf(1) ? "in the main hand"
                            : inWorld(m.ent)     ? va("in the world (entity %d)", static_cast<int>(m.ent - cl_entities))
                                                 : "holstered";
        for(int stat = 0; stat < protocol::numHolsters; stat++)
        {
            where = m.ent == inHolster[stat] ? va("in holster %d", stat) : where;
        }
        Con_Printf("  gear #%d %s %s: blood on %d texels (%.1f)\n", m.weapon, m.model ? m.model->name : "?", where, count, sum);
        if(m.model && m.model->type == mod_brush && parkIndex(m.ent) < 0)
        {
            printBoxCells(i, glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]}, "you");
        }
    }
}

// vr_gore_spatter_test [blow|saw|shot|gib] [distance]: blood thrown onto you as by a blow `distance` units ahead of what
// the main hand holds (6), a chainsaw's cut there, a shot hitting `distance` ahead of the eyes (40), a gib striking the
// main hand (vr_gore_spatter*; ROUND21.md, "Blood on you, your weapons and props"). `arm [main|off] [count]`: `count`
// (3) bleeding marks on that forearm alone, as wounds spread them (one arm's never on the other: ROUND21.md, "Chunky
// arms on their own texels").
void spatterTest_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "blow";
    if(!vr_wounds.value || !ensureTexture())
    {
        Con_Printf("vr_gore_spatter_test: Dynamic Wounds are off\n");
        return;
    }
    if(ZA_STRCMP(what, "arm") == 0)
    {
        const int hand = Cmd_Argc() > 2 && ZA_STRCMP(Cmd_Argv(2), "off") == 0 ? 0 : 1;
        armMarks(hand, Cmd_Argc() > 3 ? atoi(Cmd_Argv(3)) : 3, 1.f, 0.f, true);
        end();
        Con_Printf("vr_gore_spatter_test: bleeding marks on the %s forearm\n", hand ? "main" : "off");
        return;
    }
    entity_t* own[3]{};
    view::woundTargets(own);
    vec3_t fwd, right, upv;
    AngleVectors(r_refdef.viewangles, fwd, right, upv);
    const glm::vec3 ahead{fwd[0], fwd[1], fwd[2]};
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const bool offProp = ZA_STRCMP(what, "propoff") == 0; // (as prop, on what the off hand holds)
    entity_t* g = gearOf(offProp ? 0 : 1);
    const glm::vec3 hand = g ? originOf(*g) : own[2] ? originOf(*own[2]) : eye + ahead * 16.f;
    const bool shot = ZA_STRCMP(what, "shot") == 0, gib = ZA_STRCMP(what, "gib") == 0, saw = ZA_STRCMP(what, "saw") == 0;
    const bool propblow = ZA_STRCMP(what, "propblow") == 0;
    const bool prop = ZA_STRCMP(what, "prop") == 0 || propblow || offProp;
    if(ZA_STRCMP(what, "burst") == 0) // a gibbing `distance` units ahead (48), a little under your eyes: on what lies near
    {
        const float d = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 48.f;
        const glm::vec3 at = eye + glm::normalize(glm::vec3{ahead.x, ahead.y, 0.f}) * d - glm::vec3{0.f, 0.f, 24.f};
        const float org[3]{at.x, at.y, at.z};
        burst(org, Cmd_Argc() > 3 ? static_cast<float>(atof(Cmd_Argv(3))) : 2.f);
        Con_Printf("vr_gore_spatter_test: a gibbing at %.0f %.0f %.0f (%d splashes)\n", at.x, at.y, at.z, static_cast<int>(splashes.size()));
        return;
    }
    if(ZA_STRCMP(what, "holster") == 0) // a hit's blood just out from holster `slot`'s weapon (2: the left hip; 0..5)
    {
        const int stat = Cmd_Argc() > 2 ? za::clamp(atoi(Cmd_Argv(2)), 0, protocol::numHolsters - 1) : 2;
        entity_t* inHolster[protocol::numHolsters];
        int holsterWeapon[protocol::numHolsters];
        holstered(inHolster, holsterWeapon);
        if(!inHolster[stat])
        {
            Con_Printf("vr_gore_spatter_test: holster %d draws no weapon (id %d)\n", stat, holsterWeapon[stat]);
            return;
        }
        const glm::vec3 at = originOf(*inHolster[stat]) + glm::normalize(glm::vec3{ahead.x, ahead.y, 0.f}) * 8.f;
        za::Vector<Splat> splats;
        splats.pushBack(spatterSplat(at, 26.f, 1.5f, 8.f, 0.3f, 1.f));
        paintOnYou(splats, at, 26.f, true);
        end();
        Con_Printf("vr_gore_spatter_test: blood just out from holster %d's weapon #%d\n", stat, holsterWeapon[stat]);
        return;
    }
    const float dist = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : shot ? 40.f : gib ? 0.f : 6.f;
    Event ev;
    ev.num = 0;
    ev.dir = ahead;
    ev.amount = 30;
    ev.kind = shot ? KindShot : KindMelee;
    ev.org = shot ? eye + ahead * dist : hand + ahead * dist;
    if(prop) // a blow on what the main hand holds, on its side facing you (`distance` units past its bounding sphere, -4 by default): the blood seen on it
    {
        if(!g)
        {
            Con_Printf("vr_gore_spatter_test: the %s hand holds nothing\n", offProp ? "off" : "main");
            return;
        }
        glm::vec3 mid = originOf(*g);
        if(g->model && g->model->type == mod_brush)
        {
            float c[3];
            R_BModelCentre(g, c);
            mid = glm::vec3{c[0], c[1], c[2]};
        }
        const glm::vec3 toEye = glm::normalize(eye - mid);
        ev.org = mid + toEye * (modelRadius(g->model) + (Cmd_Argc() > 2 ? dist : -4.f)); // (just past its corners)
        ev.dir = toEye; // (struck there: the blow going your way)
        if(propblow)
        {
            // Swung away from you into a monster (vr_gore_spatter_test propblow): the blow's contact as the server finds
            // it, inside what struck, on your side of its middle; the far side struck.
            ev.org = mid + toEye * (modelRadius(g->model) * 0.3f);
            ev.dir = -toEye;
        }
    }
    if(gib)
    {
        const glm::vec3 at = (own[2] ? originOf(*own[2]) : hand) + ahead * dist;
        gibStrike(at, 3.f, vr_gore_spatter_gibs.value);
        Con_Printf("vr_gore_spatter_test: a gib struck at %.0f %.0f %.0f\n", at.x, at.y, at.z);
    }
    else
    {
        spatteredNow.clear();
        spatterFrom(ev, shot ? -1 : saw ? 1 : 0);
        Con_Printf("vr_gore_spatter_test: a %s's blood from %.0f %.0f %.0f\n", shot ? "shot" : saw ? "chainsaw cut" : prop ? "blow on your prop" : "blow", ev.org.x,
            ev.org.y, ev.org.z);
    }
    end();
    if(prop && g && g->model && g->model->type == mod_brush)
    {
        if(const int layer = acquire(g, false, false); layer >= 0)
        {
            printBoxCells(layer, eye, "you");
        }
    }
}

// vr_wounds_test <entity|self|ahead|nearest> <kind> [amount] [right] [up]: a wound as the server would send it, on entity
// number n (or the player; the model nearest the view's line; the monster model nearest the view), from the view (the player: from ahead), kind QVR_WOUND_* (1 shot .. 9 liquid), `right`/`up` units
// off the model's middle (a liquid: `up` is its surface's height over the model's feet).
void test_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_wounds_test <entity number | self | ahead | nearest> <kind 1 shot, 2 nail, 3 melee, 4 blast, 5 burn, 6 zap, 7 lava, 8 slime, "
                   "9 liquid> [amount] [right] [up] [extra]\n");
        return;
    }
    if(ZA_STRCMP(Cmd_Argv(1), "all") == 0) // every model drawn with an alias model (monsters, corpses, items): a stress test
    {
        char args[256];
        q_snprintf(args, sizeof(args), "%s %s %s %s %s", Cmd_Argc() > 2 ? Cmd_Argv(2) : "1", Cmd_Argc() > 3 ? Cmd_Argv(3) : "20",
            Cmd_Argc() > 4 ? Cmd_Argv(4) : "0", Cmd_Argc() > 5 ? Cmd_Argv(5) : "0", Cmd_Argc() > 6 ? Cmd_Argv(6) : "0");
        int n = 0;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& c = cl_entities[i];
            if(i != cl.viewentity && c.model && c.model->type == mod_alias && c.msgtime >= cl.mtime[1] - 0.2 &&
                !modelmeta::has(c.model, modelmeta::Trait::ViewWeapon))
            {
                Cbuf_InsertText(va("vr_wounds_test %d %s\n", i, args));
                n++;
            }
        }
        Con_Printf("vr_wounds_test: %d models\n", n);
        return;
    }
    Event ev;
    const bool self = ZA_STRCMP(Cmd_Argv(1), "self") == 0;
    ev.num = self ? cl.viewentity : atoi(Cmd_Argv(1));
    if(ZA_STRCMP(Cmd_Argv(1), "ahead") == 0) // the model nearest the view's line, ahead
    {
        const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
        vec3_t fwd, right, upv;
        AngleVectors(r_refdef.viewangles, fwd, right, upv);
        float best = 0.6f;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& c = cl_entities[i];
            if(i == cl.viewentity || !c.model || c.model->type != mod_alias || c.msgtime < cl.mtime[1] - 0.2)
            {
                continue;
            }
            const glm::vec3 to = glm::vec3{c.origin[0], c.origin[1], c.origin[2]} - eye;
            const float cosA = glm::dot(glm::normalize(to), glm::vec3{fwd[0], fwd[1], fwd[2]});
            if(vr_wounds_debug.value)
            {
                Con_Printf("  %d %s cos %.2f dist %.0f\n", i, c.model->name, cosA, glm::length(to));
            }
            if(cosA > best && glm::length(to) < 1000.f)
            {
                best = cosA;
                ev.num = i;
            }
        }
        if(ev.num > 0)
        {
            const entity_t& c = cl_entities[ev.num];
            Con_Printf("vr_wounds_test: entity %d (%s) at %.0f %.0f %.0f\n", ev.num, c.model->name, c.origin[0], c.origin[1], c.origin[2]);
        }
    }
    if(ZA_STRCMP(Cmd_Argv(1), "nearest") == 0) // the monster model nearest the view (its .mdl: progs/, not an item, a weapon, a gib)
    {
        const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
        float best = 1000.f;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& c = cl_entities[i];
            if(i == cl.viewentity || !c.model || c.model->type != mod_alias || c.msgtime < cl.mtime[1] - 0.2 ||
                !modelmeta::has(c.model, modelmeta::Trait::Progs) || modelmeta::has(c.model, modelmeta::Trait::ViewWeapon) ||
                modelmeta::has(c.model, modelmeta::Trait::Gib) || modelmeta::has(c.model, modelmeta::Trait::Hand) ||
                modelmeta::has(c.model, modelmeta::Trait::Body) || ZA_STRNCMP(c.model->name, "progs/vr_", 9) == 0)
            {
                continue;
            }
            const float d = glm::distance(glm::vec3{c.origin[0], c.origin[1], c.origin[2]}, eye);
            if(d < best)
            {
                best = d;
                ev.num = i;
            }
        }
    }
    ev.kind = atoi(Cmd_Argv(2));
    ev.amount = Cmd_Argc() > 3 ? atoi(Cmd_Argv(3)) : 20;
    const float right = Cmd_Argc() > 4 ? static_cast<float>(atof(Cmd_Argv(4))) : 0.f;
    const float up = Cmd_Argc() > 5 ? static_cast<float>(atof(Cmd_Argv(5))) : 0.f;
    ev.extra = Cmd_Argc() > 6 ? atoi(Cmd_Argv(6)) : 0;
    if(ev.num <= 0 || ev.num >= cl.num_entities || !cl_entities[ev.num].model)
    {
        Con_Printf("vr_wounds_test: no entity %d\n", ev.num);
        return;
    }
    const entity_t& e = cl_entities[ev.num];
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    const glm::vec3 mid = origin + glm::vec3{(e.model->mins[0] + e.model->maxs[0]) * 0.5f, (e.model->mins[1] + e.model->maxs[1]) * 0.5f,
                                        (e.model->mins[2] + e.model->maxs[2]) * 0.5f};
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    glm::vec3 dir;
    if(self)
    {
        const float yaw = glm::radians(cl.viewangles[YAW]);
        dir = -glm::vec3{za::cos(yaw), za::sin(yaw), 0.f}; // from ahead
    }
    else
    {
        dir = glm::normalize(mid - eye);
    }
    glm::vec3 side = glm::cross(dir, glm::vec3{0.f, 0.f, 1.f});
    side = glm::length(side) > 0.1f ? glm::normalize(side) : glm::vec3{1.f, 0.f, 0.f};
    ev.dir = dir;
    ev.org = (self ? origin + glm::vec3{0.f, 0.f, 4.f} : mid) + side * right + glm::vec3{0.f, 0.f, up};
    if(ev.kind == KindBlast)
    {
        ev.org = mid - dir * 40.f + side * right + glm::vec3{0.f, 0.f, up};
    }
    if(ev.kind >= KindLava)
    {
        ev.org = origin + glm::vec3{0.f, 0.f, e.model->mins[2] + up}; // the surface: `up` over the feet
    }
    events.pushBack(ev);
}

// vr_wounds_dump: every mask in use written to <gamedir>/wounds/mask_<layer>_<model>.png (r blood, g char, b wetness,
// heat as white), laid out as its skin.
void dump_f()
{
    if(!array)
    {
        Con_Printf("vr_wounds_dump: no masks\n");
        return;
    }
    const za::SizeT side = static_cast<za::SizeT>(za::max(layerSize, fineSize));
    za::Vector<byte> rgba(side * side * 4);
    za::Vector<byte> rgb(side * side * 3);
    Sys_mkdir(va("%s/wounds", com_gamedir));
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    for(int i = 0; i < maskCount(); i++)
    {
        const Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.ent)
        {
            continue;
        }
        int in[2], side[2];
        const int n = layersOf(i, in, side);
        for(int k = 0; k < n; k++) // (your body's: its left side and middle, then its right side)
        {
            attach(GL_READ_FRAMEBUFFER, in[k]);
            glReadPixels(0, 0, m.w, m.h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            for(int p = 0; p < m.w * m.h; p++)
            {
                // r blood, g char, b wetness; heat shown as white over them
                const int heat = rgba[static_cast<za::SizeT>(p) * 4 + 3];
                for(int c = 0; c < 3; c++)
                {
                    rgb[static_cast<za::SizeT>(p) * 3 + c] = static_cast<byte>(za::max<int>(rgba[static_cast<za::SizeT>(p) * 4 + c], heat));
                }
            }
            const char* slash = strrchr(m.model->name, '/');
            char name[MAX_OSPATH];
            q_snprintf(name, sizeof(name), "wounds/mask_%02d_%s%s.png", i, slash ? slash + 1 : m.model->name, side[k] == 1 ? "_right" : "");
            Image_WritePNG(name, rgb.data(), m.w, m.h, 24, true);
            Con_Printf("vr_wounds_dump: %s (%dx%d)\n", name, m.w, m.h);
        }
    }
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
}

// vr_wounds_info: the pool (layers used, what they are on) and the last paint's cost.
void info_f()
{
    int used = 0;
    for(const Mask& m : masks)
    {
        used += m.ent ? 1 : 0;
    }
    Con_Printf("wounds: %d of %d masks used (%dx%d layers, %.1f MB; yours %dx%d, %.1f MB), %d paints so far, last frame's work "
               "%.3f ms (CPU)\n",
        used, maskCount(), layerSize, layerSize, static_cast<double>(layers) * layerSize * layerSize * 4.0 / (1024.0 * 1024.0), fineSize,
        fineSize, static_cast<double>(maskCount() - layers) * fineSize * fineSize * 5.0 / (1024.0 * 1024.0), paintsTotal,
        static_cast<double>(lastPaintMs));
    Con_Printf("wounds: own resolution hands %d body %d (shared array %d)\n", ownRes(), bodyRes(), fineSize);
    for(int i = 0; i < maskCount(); i++)
    {
        const Mask& m = masks[static_cast<za::SizeT>(i)];
        if(m.ent)
        {
            Con_Printf("  %2d %s%s %dx%d, foreign blood slot %d, drawn %.1f s ago%s%s\n", i, m.model ? m.model->name : "?",
                m.view ? (isFine(i) ? " (you, fine)" : " (you)") : "", m.w, m.h, bloodLayerOf(i),
                vr_gametime - m.lastDrawn, m.wetLeft > 0.f ? ", wet" : "", m.hotLeft > 0.f ? ", hot" : "");
        }
    }
}

void registerDiagCommands()
{
    Cmd_AddCommand("vr_wounds_test", test_f);
    Cmd_AddCommand("vr_wounds_info", info_f);
    Cmd_AddCommand("vr_gore_hands_test", handsTest_f);
    Cmd_AddCommand("vr_wrist_blood_test", wristTest_f);
    Cmd_AddCommand("vr_gore_wash_test", washTest_f);
    Cmd_AddCommand("vr_gore_hands_info", handsInfo_f);
    Cmd_AddCommand("vr_gore_spatter_test", spatterTest_f);
    Cmd_AddCommand("vr_wounds_dump", dump_f);
}

} // namespace qvr::wounds
