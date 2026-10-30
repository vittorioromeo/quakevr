// vr_builtins_held.inc -- PF_ builtins: the motion recorder, weapon hotspots, magnetic holsters, debris and crates.
// Split from vr_builtins.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// ----------------------------------------------------------------------------
// The motion recorder (vr_motion.cpp; QC vr_motion.qc): kept only while a take is recorded or played.

// void(string kind, string sub, float hand, float value, vector at, entity targ, string detail) motionevent
void PF_motionevent()
{
    motion::qcEvent(G_STRING(OFS_PARM0), G_STRING(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)), G_FLOAT(OFS_PARM3),
        G_VECTOR(OFS_PARM4), G_EDICT(OFS_PARM5), G_STRING(OFS_PARM6));
}

// void(float hand, vector at, string name) motionpoint
void PF_motionpoint()
{
    motion::qcPoint(static_cast<int>(G_FLOAT(OFS_PARM0)), G_VECTOR(OFS_PARM1), G_STRING(OFS_PARM2));
}

// void(string key, vector value) motionvalue
void PF_motionvalue()
{
    motion::qcValue(G_STRING(OFS_PARM0), G_VECTOR(OFS_PARM1));
}

// Round 21: the weapons' hotspots (where the other hand may hold them; vr_weapons.hpp), as the local player's view
// draws them this frame. vector(entity player, float hand, float index) weaponhotspot: hotspot `index` (0..3) of the
// weapon in `hand` (cVR_MainHand, cVR_OffHand): a grip's point, a blade grip's middle, in the world ('0 0 0' if none);
// float(entity player, float hand, float index, float what) weaponhotspotinfo: its type (what 0: 0 none, 1 grip,
// 2 blade), bias in units (1), a blade's share of the way from the hand to the tip (2). Only the local player's (the
// server's first client, as its view draws it); others' are none.
[[nodiscard]] view::WeaponHotspot localHotspot()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    if(!sv.active || cls.state != ca_connected || NUM_FOR_EDICT(player) != 1)
    {
        return {};
    }
    return view::weaponHotspot(static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1, static_cast<int>(G_FLOAT(OFS_PARM2)));
}

void PF_weaponhotspot()
{
    const view::WeaponHotspot h = localHotspot();
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = h.pos.x;
    out[1] = h.pos.y;
    out[2] = h.pos.z;
}

void PF_weaponhotspotinfo()
{
    const view::WeaponHotspot h = localHotspot();
    const int what = static_cast<int>(G_FLOAT(OFS_PARM3));
    G_FLOAT(OFS_RETURN) = what == 0 ? static_cast<float>(h.type) : what == 1 ? h.bias : h.share;
}

// float(entity player, float hand, entity weapon) weapongrabspot: the hotspot (index + 1; 0 none: its handle) by which
// `player`'s empty `hand` takes `weapon` lying about (vr_weapon_grab_hotspots: the local player's view found the hand
// nearer that grip or blade hotspot than the handle; vr_twohand.cpp groundSpot). Others' are 0.
void PF_weapongrabspot()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    edict_t* weapon = G_EDICT(OFS_PARM2);
    const int hand = static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1;
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    G_FLOAT(OFS_RETURN) = local && (vr_weapon_grab_hotspots.value || vr_weapon_grab_anywhere.value)
                              ? static_cast<float>(twohand::groundSpot(hand, NUM_FOR_EDICT(weapon)))
                              : 0.f;
}

// float(entity player, float hand) weaponanygrip: how `player`'s empty `hand` holds the weapon in its other hand: 0 not,
// 1 by a hotspot, 2 anywhere on it (vr_weapon_grab_anywhere: a free grip, or both hands on a carried weapon; or a moment
// ago). QC VRTryHandOff: letting go of the other hand leaves the weapon carried by this one where it holds it. The local
// player's only (others' are 0).
void PF_weaponanygrip()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    const int hand = static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1;
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    G_FLOAT(OFS_RETURN) = local ? static_cast<float>(twohand::helpKind(hand)) : 0.f;
}

// vector(entity player, float hand, float what) weapondrawnpose: the weapon `player`'s `hand` carries off its handle (the
// hand-off, a hotspot, anywhere), as the local player's view draws it: what 0 its handle's place, 1 its angles as a thrown
// weapon is given them (MakeThrown negates the pitch: then drawn as it was), 2 '1 0 0' if known ('0 0 0': not carried, or another player),
// 3 its tip (the melee's line: QC VR_Melee_Line); 4 its handle and 5 its tip in the frame of the hand carrying it as drawn
// (forward, right, up: QC places them on the hand of the move it runs, VR_Melee_FreeLine). QC: thrown from where it is drawn.
// weapondrawnpose(e, hand, 6): the muzzle of the weapon model `e` itself is drawn with, at its origin and angles, as a
// weapon lying there or held is drawn (its Weapon Offsets' model transform, its Muzzle anchor and offset, at its first
// frame); e's origin if its model is no weapon's (an armed grunt's held weapon, QC vr_armedgrunt.qc: its shots' start).
static void weaponModelMuzzle(edict_t* e, float* r)
{
    for(int i = 0; i < 3; i++)
    {
        r[i] = e->v.origin[i];
    }
    const int index = static_cast<int>(e->v.modelindex);
    qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const int slot = model && model->type == mod_alias ? weapons::slotForModel(model) : -1;
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return;
    }
    entity_t ent{};
    ent.model = model;
    VectorCopy(e->v.origin, ent.origin);
    VectorCopy(e->v.angles, ent.angles);
    const glm::vec3 muzzle = view::anchorPosition(ent, false, 1.f, weapons::anchor(slot, weapons::Key::MuzzleAnchor),
        weapons::vec(slot, weapons::Key::MuzzleOffsetX, weapons::Key::MuzzleOffsetY, weapons::Key::MuzzleOffsetZ));
    for(int i = 0; i < 3; i++)
    {
        r[i] = muzzle[i];
    }
}

void PF_weapondrawnpose()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    const int hand = static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1;
    const int what = static_cast<int>(G_FLOAT(OFS_PARM2));
    if(what == 6)
    {
        weaponModelMuzzle(player, G_VECTOR(OFS_RETURN));
        return;
    }
    if(what == 21 || what == 22)
    {
        // weapondrawnpose(p, 0, 21): an armed humanoid's held weapon `p` as the arms drew it lately (this listen server's
        // own view): its drawn forward's turn off the server's (world; the axis times degrees); 22 its drawn origin less
        // the server's. '0 0 0' (both): not drawn lately (a dedicated server, out of view). vr_agrunt_shot_drawn.
        glm::vec3 turn{0.f}, off{0.f};
        const bool local = sv.active && cls.state == ca_connected;
        if(!local || !hitreact::drawnWeaponTurn(NUM_FOR_EDICT(player), turn, off))
        {
            turn = off = glm::vec3{0.f};
        }
        const glm::vec3 out = what == 21 ? turn : off;
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(what == 20)
    {
        // weapondrawnpose(e, 0, 20): '<e's entity number> 0 0' (the glitch finder's subject: vr_agglitch_ent).
        progs::returnVec({static_cast<float>(NUM_FOR_EDICT(player)), 0.f, 0.f});
        return;
    }
    if(what == 23)
    {
        // weapondrawnpose(e, +-(code + share), 23): the Pose Studio's Live subject `e` had key `code` laid on at that
        // share this frame (negative: mirrored, a left hook's); recorded with its frame (vr_poselive.hpp).
        poselive::qcKey(NUM_FOR_EDICT(player), G_FLOAT(OFS_PARM1));
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = 0.f;
        }
        return;
    }
    if(what == 24)
    {
        // weapondrawnpose(world, kind * 1000 + code, 24): the Weapon Holds (vr_agholds.inc; QC VR_AGH_Hands): '<hands>
        // <setting> <class>', hands 1 or 2 (0: no row for that weapon), setting 0 Auto, 1 One-Handed, 2 Two-Handed; class
        // its pose category (agpose::Class: the Pose Category's, a gun with no row or a melee weapon agpose::Gun).
        const int arg = static_cast<int>(G_FLOAT(OFS_PARM1));
        int set = 0;
        const int h = agholds::hands(arg / 1000, arg % 1000, &set);
        progs::returnVec({static_cast<float>(h), static_cast<float>(set), static_cast<float>(agholds::poseClass(arg / 1000, arg % 1000, nullptr))});
        return;
    }
    if(what == 28)
    {
        // weapondrawnpose(world, kind * 100 + class, 28): '<factor> 0 0', the IK stretch of that kind and pose class
        // (agpose::stretch; 1 none), sent in the pose message's off-hand block (QC VR_AGP_Pose).
        const int arg = static_cast<int>(G_FLOAT(OFS_PARM1));
        progs::returnVec({agpose::stretch(arg / 100, arg % 100), 0.f, 0.f});
        return;
    }
    if(what >= 25 && what <= 27)
    {
        // weapondrawnpose(world, code, 25..27): pose key `code`'s off-hand parts (agpose::GripPos, GripTurn, OffHand:
        // x its OffMode), '0 0 0' none.
        const agpose::Key k = agpose::value(static_cast<int>(G_FLOAT(OFS_PARM1)));
        const glm::vec3 out = k.v[agpose::GripPos + (what - 25)];
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(what >= 11 && what <= 19)
    {
        // The armed humanoids' pose keys (vr_agpose.hpp; QC vr_armedgrunt_pose.qc VR_AGK_*), the builtin slots being full:
        // `hand` a key's code, 11..17 its parts (agpose::Part; 25..27 the off hand's, above), 18 '1 0 0' if it has one
        // ('0 0 0': all zero). 19 the Pose Studio's hold on `player` (an armed humanoid; `hand` its kind * 100 + its weapon
        // class): posestudio::qcHold.
        const int arg = static_cast<int>(G_FLOAT(OFS_PARM1));
        glm::vec3 out{0.f};
        if(what == 19)
        {
            out = posestudio::qcHold(NUM_FOR_EDICT(player), arg / 100, arg % 100);
            glm::vec3 studio;
            if(rigstudio::qcHold(NUM_FOR_EDICT(player), arg / 100, arg % 100, studio))
            {
                out = studio; // (the Rig Studio's subject: its aim target's Aim stance, or the Pose Handles' key)
            }
        }
        else
        {
            agpose::Key k;
            const bool has = agpose::get(arg, k);
            out = what == 18 ? glm::vec3{has ? 1.f : 0.f, 0.f, 0.f} : k.v[what - 11];
        }
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(what >= 8 && what <= 10)
    {
        // The weapon model `player` (a weapon entity) is drawn with, in its own frame (view::weaponLocalGeometry): 8 its
        // muzzle (a blade's tip), 9 its butt, 10 its second grip (z +1000: a Cup's, no point); '0 0 0' none.
        const int index = static_cast<int>(player->v.modelindex);
        qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
        glm::vec3 muzzle{0.f}, butt{0.f}, second{0.f};
        int type = 0;
        glm::vec3 out{0.f};
        if(view::weaponLocalGeometry(model, muzzle, butt, second, type))
        {
            out = what == 8 ? muzzle : what == 9 ? butt : (type == 2 ? glm::vec3{0.f, 0.f, 1000.f} : second);
        }
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(what == 7)
    {
        // x: the Weapon Offsets slot (-1 none) of the weapon `player`'s `hand` holds (its weaponmodel, weaponmodel2): the
        // blade's shape (QC VR_Melee_ShapeOf; admeleebend, admeleehead with -1 - slot).
        const FieldOffsets& f = fields();
        const char* name = hand ? PR_GetString(player->v.weaponmodel)
                                : (f.weaponmodel2 >= 0 ? PR_GetString(fieldInt(player, f.weaponmodel2)) : "");
        G_VECTOR(OFS_RETURN)[0] = static_cast<float>(name && *name ? weapons::slotForName(name) : -1);
        G_VECTOR(OFS_RETURN)[1] = G_VECTOR(OFS_RETURN)[2] = 0.f;
        return;
    }
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    glm::vec3 pos{0.f}, rot{0.f}, tip{0.f};
    bool mirrored = false;
    bool known = local && twohand::carrying(hand) && view::carriedWeaponPose(hand, pos, rot, mirrored, tip);
    if(known && (what == 4 || what == 5))
    {
        known = view::carriedWeaponInHand(hand, pos, tip);
        rot = what == 4 ? pos : tip;
    }
    const glm::vec3 out = !known ? glm::vec3{0.f} : what == 0 ? pos : what == 1 || what >= 4 ? rot : what == 3 ? tip : glm::vec3{1.f, 0.f, 0.f};
    for(int i = 0; i < 3; i++)
    {
        G_VECTOR(OFS_RETURN)[i] = out[i];
    }
}

// vector(float kind, float f) admeleebend: AD sword kind `kind`'s blade at share `f` of the line from the hand to its tip,
// off that line (admelee::bend, as shares of its length): x in the blade's plane (the model's +y crossed with the line),
// y along the model's +y; z 1 if the blade is curved (0: straight, nothing off it). QC VR_Melee_At.
// A kind below 0: Weapon Offsets slot -1 - kind's own curve (Blade Spine, vr_blade.hpp; else its model's, a curved AD
// sword's) and its blade start's bend (Blade From); z 1 if it has either: Edit Blade Shape.
void PF_admeleebend()
{
    if(const int kind = static_cast<int>(G_FLOAT(OFS_PARM0)); kind < 0)
    {
        const blade::Shape s = blade::shape(-1 - kind);
        const float f = G_FLOAT(OFS_PARM1);
        glm::vec2 k{0.f};
        if(s.spine)
        {
            k = blade::spineAt(s, f);
        }
        else if(const cvar_t* id = weapons::cvar(-1 - kind, weapons::Key::ID); id && f > 0.f)
        {
            k = admelee::bend(admelee::kindOfModel(id->string), f);
        }
        k += blade::fromAt(s, f);
        progs::returnVec({k.x, k.y, s.spine || blade::hasFrom(s) ? 1.f : 0.f});
        return;
    }
    const glm::vec2 b = admelee::bend(static_cast<int>(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
    progs::returnVec({b.x, b.y, b != glm::vec2{0.f} ? 1.f : 0.f});
}

// vector(float kind, float which) admeleehead: AD heavy kind `kind`'s (the hammer, the mace) head's box (admelee::head,
// shares of the line from the hand to its tip): `which` 0 its low corner, 1 its high; '0 0 0' for none. QC
// VR_Melee_Sweep (the head's volume strikes).
// A kind below 0: Weapon Offsets slot -1 - kind's own blade shape (vr_blade.hpp; Edit Blade Shape): `which` 0 and 1 its
// own head's corners ('0 0 0': none of its own), 2 '<near end> <width> <flags>' (the near end's share behind the hand, -1
// the kind's own, QC VR_Melee_NearBack; the swept points' radius's multiplier; 1 a curve of its own + 2 a head of its own
// + 4 a blade start of its own + 8 a line start or tip of its own), 3 '<blade start> <off> <side>' (Blade From: its share
// of the line, -1 the kind's own), 4 the line's start off the hand and 5 its tip off the muzzle (Blade Org, Blade Tip:
// units along the hand's forward, right, up).
void PF_admeleehead()
{
    glm::vec3 lo{0.f}, hi{0.f};
    if(const int kind = static_cast<int>(G_FLOAT(OFS_PARM0)); kind < 0)
    {
        const blade::Shape s = blade::shape(-1 - kind);
        const int which = static_cast<int>(G_FLOAT(OFS_PARM1));
        const glm::vec3 out = which == 2   ? glm::vec3{s.nearBack, s.width,
                                                 (s.spine ? 1.f : 0.f) + (s.head ? 2.f : 0.f) + (blade::hasFrom(s) ? 4.f : 0.f) +
                                                     (blade::hasEnds(s) ? 8.f : 0.f)}
                              : which == 3 ? glm::vec3{blade::hasFrom(s) ? s.from : -1.f, s.fromOff.x, s.fromOff.y}
                              : which == 4 ? s.origin
                              : which == 5 ? s.tipOffset
                              : !s.head    ? glm::vec3{0.f}
                              : which == 1 ? s.headHi
                                           : s.headLo;
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(!admelee::head(static_cast<int>(G_FLOAT(OFS_PARM0)), lo, hi))
    {
        lo = hi = glm::vec3{0.f};
    }
    const glm::vec3& out = G_FLOAT(OFS_PARM1) != 0.f ? hi : lo;
    for(int i = 0; i < 3; i++)
    {
        G_VECTOR(OFS_RETURN)[i] = out[i];
    }
}

// void(float hand, float what, vector a, vector b, float v) meleeshow: Show Melee Blades' data (vr_meleeshow.hpp; QC
// VR_Melee_Show, sent only while vr_debug_melee_blades is on).
void PF_meleeshow()
{
    const float* a = G_VECTOR(OFS_PARM2);
    const float* b = G_VECTOR(OFS_PARM3);
    meleeshow::add(static_cast<int>(G_FLOAT(OFS_PARM0)), static_cast<int>(G_FLOAT(OFS_PARM1)),
        geom::vec(a), geom::vec(b), G_FLOAT(OFS_PARM4));
}

// Magnetic holsters (QC vr_magholster.qc; vr_magholster.cpp). float(entity player) magholsterpoll: what the local
// player asked since the last call, taken (1 the off hand's bind button at a holster, 2 the main hand's, 4 every bound
// weapon home at once: vr_magholster_return); others' 0. vector(entity player, float slot) holsterpos: holster `slot`'s
// place (body::Holster, the holsterweapon<slot> fields' order) as the local player's body has it ('0 0 0': unknown, or
// another player: QC's own estimate then).
void PF_magholsterpoll()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    G_FLOAT(OFS_RETURN) = local ? static_cast<float>(magholster::poll()) : 0.f;
}

void PF_holsterpos()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    const int slot = static_cast<int>(G_FLOAT(OFS_PARM1));
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    glm::vec3 out{0.f};
    if(local && slot >= 0 && slot < body::HolsterCount && hands::current().valid)
    {
        out = body::holsterPosition(hands::current(), static_cast<body::Holster>(slot));
    }
    for(int i = 0; i < 3; i++)
    {
        G_VECTOR(OFS_RETURN)[i] = out[i];
    }
}

// Rocks and bricks lying about (vr_debris.cpp, QC vr_debris.qc). float() debrisplan: places them (after the map's
// entities), how many; string(float i) debrismodel: piece i's model (to set); float(entity e, float i) debrisput: puts
// piece i on e (its model set): its skin, size, turn, place resting on the floor and box; its kind (1 a rock, 2 a
// brick; 0 none).
void PF_debrisplan()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(debris::plan());
}

void PF_debrismodel()
{
    G_INT(OFS_RETURN) = PR_SetEngineString(debris::modelOf(static_cast<int>(G_FLOAT(OFS_PARM0))));
}

void PF_debrisput()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(debris::put(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1))));
}

// float(entity e) debrisplace: a piece the map placed (vr_debris_piece, its model set): sized, turned by its yaw and
// resting on the floor at its origin, its box; its kind (1 a rock, 2 a brick; 0: not one of their models).
void PF_debrisplace()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(debris::putPlaced(G_EDICT(OFS_PARM0)));
}

// Wooden crates lying about (vr_crates.cpp, QC vr_crates.qc), as the rocks and bricks: float() crateplan, string(float
// i) cratemodel, float(entity e, float i) crateput (its kind: 1 small, 2 large; 0 none), float(entity e) crateplace (a
// crate the map or a test placed, its model set: level, turned by its yaw, resting on the floor under its origin).
void PF_crateplan()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(crates::plan());
}

void PF_cratemodel()
{
    G_INT(OFS_RETURN) = PR_SetEngineString(crates::modelOf(static_cast<int>(G_FLOAT(OFS_PARM0))));
}

void PF_crateput()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(crates::put(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1))));
}

// float(float i) cratecrowbar: whether the plan puts a crowbar on crate i (vr_crate_crowbar: 1), or a flare gun
// (vr_crate_flaregun: 2; no builtin numbers left for one of its own); float(entity e, float i)
// cratecrowbarput: lays the crowbar `e` (a thrown weapon, its model set) on it (crates::putCrowbar).
void PF_cratecrowbar()
{
    const int i = static_cast<int>(G_FLOAT(OFS_PARM0));
    G_FLOAT(OFS_RETURN) = crates::hasCrowbar(i) ? 1.f : crates::hasFlareGun(i) ? 2.f : 0.f; // (2: a flare gun, vr_crate_flaregun)
}

void PF_cratecrowbarput()
{
    G_FLOAT(OFS_RETURN) = crates::putCrowbar(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1))) ? 1.f : 0.f;
}

void PF_crateplace()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(crates::putPlaced(G_EDICT(OFS_PARM0)));
}

// entity sightblocked(vector start, vector end, entity ignore, entity ignore2): the solid prop that blocks sight
// (.vr_blocksight: a crate, an explosive box; not one in a hand) between `start` and `end`, met by its shape as it lies;
// world if none (crates::sightBlocked). QC's visible() (ai.qc): monsters don't see you behind one.
void PF_sightblocked()
{
    const float* a = G_VECTOR(OFS_PARM0);
    const float* b = G_VECTOR(OFS_PARM1);
    const int ignoreA = qcvm->argc > 2 ? NUM_FOR_EDICT(G_EDICT(OFS_PARM2)) : 0;
    const int ignoreB = qcvm->argc > 3 ? NUM_FOR_EDICT(G_EDICT(OFS_PARM3)) : 0;
    const int num = crates::sightBlocked(geom::vec(a), geom::vec(b), ignoreA, ignoreB);
    G_INT(OFS_RETURN) = EDICT_TO_PROG(EDICT_NUM(num));
}

// float heldshape(entity prop, vector start, vector end): where the line from `start` to `end` first meets the shape of
// `prop` held in a hand (its held body, as the hand holds it), as a fraction of it; 1 if it doesn't or `prop` isn't held
// (box3d::heldRay). A crate held up stops monsters' shots (weapons.qc VR_Crate_ShieldShot).
void PF_heldshape()
{
    const float* a = G_VECTOR(OFS_PARM1);
    const float* b = G_VECTOR(OFS_PARM2);
    G_FLOAT(OFS_RETURN) = box3d::heldRay(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), geom::vec(a),
        geom::vec(b));
}

// float heldbox(entity prop, vector mins, vector maxs, float reach): whether the shape of `prop` held in a hand meets the
// box mins..maxs (world) grown by `reach` units: 1 yes, 0 no, -1 `prop` has no held body (box3d::heldBox). A held prop
// presses a wall button it touches (buttons.qc VR_Buttons_PropFrame).
void PF_heldbox()
{
    const float* lo = G_VECTOR(OFS_PARM1);
    const float* hi = G_VECTOR(OFS_PARM2);
    G_FLOAT(OFS_RETURN) = static_cast<float>(box3d::heldBox(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), geom::vec(lo),
        geom::vec(hi), G_FLOAT(OFS_PARM3)));
}
