#ifdef QVR_TESTS
// vr_gibprops_test.qc -- every monster's head and gibs are Quake VR's props (docs/vr-port/ROUND22.md, "Gib props"):
// `vr_gibprops_test <n>` (Debug > Gore Tests > Gib Props), on the newest monster (the debug spawner's: vr_test_spawn N
// with impulse 241). One "gibprops:" line each (bprint). After AD's files: it counts AD's own gibs left AD's.
//   1 gib it alive (damage past its gib health: id's ThrowHead, AD's monster_ThrowGib): its head and gibs, each a prop
//     (grabbable, destroyable) or not
//   2 kill it whole (a corpse to gib with 3)
//   3 gib the newest dead body (1000: Quake VR's corpse gibbing, ThrowHead on it)
//   4 the head of 1 or 3: a Box3D body (physicsdamp), taken by the main hand's touch at the grip's edge (the script
//     holds the mock's grip: vr_mock_button main grip 1, +grabright)
//   5 gripped: carried, in the main hand, its held body (heldbox); 7 let go (after the grip): a loose body again
//   6 shot (the player's 1000): armed, burst (no longer a gib), the small gibs made (its brain's too)
//   8 the gibs thrown since 1 or 3 (at a death's end too); 9 the head's state
// The run: Misc/quakevr/gibprops_test.sh.

entity vr_gpt_head;       // the head 1 or 3 found
string vr_gpt_class;      // whose
vector vr_gpt_at;         // where its monster was
float vr_gpt_time;        // when 1 or 3 hit it (8: the gibs since)

// The newest monster: live (xLive) or a dead body still drawn (a corpse, a dying one).
entity(float xLive) VR_GPT_Nearest =
{
    entity p = VR_Player();
    entity best = world;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_gib || !e.modelindex || e == p)
        {
            continue;
        }
        float ok = xLive ? ((e.flags & FL_MONSTER) && e.health > 0 && e.takedamage) :
                           (e.vr_corpse || ((e.flags & FL_MONSTER) && e.health <= 0 && e.takedamage));
        if(ok) // (the newest: the one the spawner just made, not a map's own nearer)
        {
            best = e;
        }
    }
    if(best == world)
    {
        bprint(sprintf("gibprops: no %s monster\n", xLive ? "live" : "dead"));
    }
    return best;
};

// After a gibbing this frame: its head (the one nearest where it was), its gibs, and AD's left AD's.
void(string xHow) VR_GPT_Count =
{
    float gibs = 0, destroy = 0, grab = 0, adLeft = 0, heads = 0;
    vr_gpt_head = world;
    vr_gpt_time = time;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_gib && e.vr_gib_made == time && !e.vr_sgib)
        {
            if(e.gorging)
            {
                heads++;
                if(vr_gpt_head == world || vlen(e.origin - vr_gpt_at) < vlen(vr_gpt_head.origin - vr_gpt_at))
                {
                    vr_gpt_head = e;
                }
                continue;
            }
            if(e.vr_limb_bone) // (its own limbs: vr_limbs.qc, counted apart)
            {
                continue;
            }
            gibs++;
            grab += (e.flags & FL_FORCEGRABBABLE) != 0;
            destroy += e.th_die == VR_Gib_Die;
            if(cvar("developer") >= 2)
            {
                bprint(sprintf("gibprops:   gib %s %s flags %g solid %g\n", e.classname, e.model, e.flags, e.solid));
            }
        }
        else if(e.classname == "item_gib" && !e.vr_gib && e.model != "" && e.ad_pain_finished > time + 9)
        {
            adLeft++; // (AD's own, just thrown: not ours)
        }
    }
    entity h = vr_gpt_head;
    bprint(sprintf("gibprops: %s %s\n", vr_gpt_class, xHow));
    bprint(sprintf("gibprops: heads %g head %s grabbable %g destroyable %g;\n", heads, h != world ? h.model : "-",
        h != world && (h.flags & FL_FORCEGRABBABLE) != 0, h != world && h.th_die == VR_Gib_Die));
    bprint(sprintf("gibprops: gibs %g grabbable %g destroyable %g; AD's left %g\n", gibs, grab, destroy, adLeft));
};

void() VR_GibProps_TestFrame =
{
    float request = cvar("vr_gibprops_test");
    if(!request)
    {
        return;
    }
    cvar_set("vr_gibprops_test", "0");
    entity p = VR_Player();
    entity os = self;
    entity m, h = vr_gpt_head;
    if(request <= 3)
    {
        vr_gpt_head = world; // (an old one's, burst and gone: its entity another's now)
    }
    if(request == 1 || request == 2)
    {
        m = VR_GPT_Nearest(TRUE);
        if(m != world)
        {
            vr_gpt_class = m.classname;
            vr_gpt_at = m.origin;
            // Past its gib health (AD's own; id's: 100 under 0, a heavy blow's gibs, not a rocket's fling), or just its
            // health (whole).
            float dmg = m.health;
            if(request == 1)
            {
                dmg = m.ad_classtype ? m.health - min(m.ad_gibhealth, -40) + 40 : m.health + 100;
            }
            self = p;
            T_Damage(m, p, p, dmg);
            self = os;
            if(request == 1)
            {
                VR_GPT_Count("gibbed alive");
            }
            else
            {
                bprint(sprintf("gibprops: %s killed whole: health %g corpse %g drawn %g\n", vr_gpt_class, m.health, m.vr_corpse,
                    m.modelindex != 0));
            }
        }
    }
    else if(request == 3)
    {
        m = VR_GPT_Nearest(FALSE);
        if(m != world)
        {
            vr_gpt_class = m.classname;
            vr_gpt_at = m.origin;
            self = p;
            T_Damage(m, p, p, 1000);
            self = os;
            VR_GPT_Count(sprintf("corpse gibbed (corpse %g)", m.vr_corpse));
        }
    }
    else if(request == 8)
    {
        // The gibs thrown since 1 or 3 (a wrath's, a dragon's: at its death's end, not at the blow), and AD's own.
        float n = 0, g = 0, d = 0, ad = 0;
        for(entity e8 = nextent(world); e8; e8 = nextent(e8))
        {
            if(e8.vr_gib && !e8.vr_sgib && !e8.gorging && !e8.vr_limb_bone && e8.vr_gib_made >= vr_gpt_time)
            {
                n++;
                g += (e8.flags & FL_FORCEGRABBABLE) != 0;
                d += e8.th_die == VR_Gib_Die;
            }
            ad += (e8.classname == "item_gib" && !e8.vr_gib && e8.model != "");
        }
        bprint(sprintf("gibprops: since: gibs %g grabbable %g destroyable %g; AD's %g\n", n, g, d, ad));
    }
    else if(h == world || !h.modelindex || (request == 4 && (!h.vr_gib || !h.handtouch)))
    {
        bprint("gibprops: no head\n");
    }
    else if(request == 4)
    {
        // Box3D's (physicsdamp keeping everything: only whether it has a body); the mock's main hand put in it, for the
        // script's +grabmain (the real grip: the carry's hand touch).
        float box3d = physicsdamp(h, '0 0 0', 1, 1, 0, '0 0 0');
        // The main hand touching it at the grip's edge (the script holds the mock's grip down from before: the frames
        // after keep it), as a hand reaching down to it: the carry's hand touch takes it (VR_Carry_Setup's).
        p.touchinghand = cVR_MainHand;
        VRSetMainHandGrabbingBit(p, TRUE);
        VRSetMainHandPrevGrabbingBit(p, FALSE);
        entity os2 = self, oo2 = other;
        self = h;
        other = p;
        h.handtouch();
        self = os2;
        other = oo2;
        bprint(sprintf("gibprops: %s head %s: rigid %g box3d %g taken %g\n", vr_gpt_class, h.model, h.vr_rigid, box3d,
            h.carry_player == p));
    }
    else if(request == 5 || request == 7)
    {
        // 5 the grip held: taken, in the hand, its held body (heldbox -1: none); 7 let go: a loose body again.
        float held = heldbox(h, h.absmin, h.absmax, 4);
        bprint(sprintf("gibprops: %s head %s: carried %g in hand %g held body %g box3d %g", vr_gpt_class,
            request == 5 ? "gripped" : "let go", h.carry_player == p, VR_Carry_Held(p, cVR_MainHand) == h, held != -1,
            physicsdamp(h, '0 0 0', 1, 1, 0, '0 0 0')));
        bprint(sprintf(" (hand %.0f from it)\n", vlen(VRGetEntHandPos(p, cVR_MainHand) - (h.absmin + h.absmax) * 0.5)));
    }
    else if(request == 6)
    {
        // (Armed: VR_Gib_Think2. The small gibs made now: older ones make room under vr_smallgibs' cap.)
        float armed = h.takedamage != DAMAGE_NO;
        self = p;
        T_Damage(h, p, p, 1000);
        self = os;
        float made = 0;
        for(entity e = nextent(world); e; e = nextent(e))
        {
            made += e.vr_sgib == time;
        }
        bprint(sprintf("gibprops: %s head shot: armed %g burst %g small gibs %g\n", vr_gpt_class, armed,
            !h.vr_gib && h.th_die != VR_Gib_Die, made));
    }
    else if(request == 9)
    {
        bprint(sprintf("gibprops: %s head %s gib %g rigid %g solid %g", vr_gpt_class, h.model, h.vr_gib, h.vr_rigid, h.solid));
        bprint(sprintf(" carry %g hp %g at %v\n", h.carry_player != world, h.vr_gib_hp, h.origin));
    }
};
#endif
