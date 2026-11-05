// vr_arena_banners.qc -- Survival Arena's banners: the run's numbers on boards high up in the arena, live (the author's
// ask, 2026-11: "real-time text banners in the arena showing enemies left and information about the current wave, time,
// score and so on").
//
// Placed by the map: each info_arena_banner is a board, a world text whose text is not shown as text: it says the numbers,
// and the engine lays them out with the wrist gadget's screen code (a panel board: worldtext::isPanel,
// vr_gadget_panel.inc), one big number per thing, in the gadget's colours. A new arena map gets them by placing these;
// nothing here knows a map or a coordinate. With no arena running in the map (ar_mgr) the entity goes and no board is
// made. vr_arena_banners 0 empties the boards.
//
// The text: {arena:wave boss show value total seconds score}. `show` is what the middle shows (AR_BANNER_SHOW_*): the
// wave's foes left, of `total`; the seconds to the next wave; CHOOSE A PERK while the cards hold the rest; at the end of a
// run YOU FELL, or CLEARED. The time starts with the run's first wave and stops with its end.

#define AR_BANNER_SHOW_FOES   0
#define AR_BANNER_SHOW_NEXT   1
#define AR_BANNER_SHOW_PERK   2
#define AR_BANNER_SHOW_FELL   3
#define AR_BANNER_SHOW_CLEAR  4

#define AR_BANNER_SCALE 2.2 // (units a pixel of the board's 250 x 40: a digit 53 units, a label's letter 18)

.float ar_banner_h;     // an info_arena_banner: its world text's handle

float ar_banner_n;      // the map's boards

// What the boards show now, so a board is only sent a new text when a number has moved. Not saved: a load starts every
// board from its spawn (empty), and the first frame after it writes them again.
nosave float ar_bn_on;  // 0 nothing written yet, 1 off, 2 on
nosave float ar_bn_wave;
nosave float ar_bn_show;
nosave float ar_bn_total;
nosave float ar_bn_value;
nosave float ar_bn_secs;
nosave float ar_bn_score;

// The run's seconds so far: from its first wave to its end.
float() VR_ARENA_BannerSeconds =
{
    if(ar_run_at <= 0) { return 0; }
    return floor((ar_run_end > 0 ? ar_run_end : time) - ar_run_at);
}

// What the middle shows.
float() VR_ARENA_BannerShow =
{
    if(ar_down) { return AR_BANNER_SHOW_FELL; }
    if(ar_preset_over) { return AR_BANNER_SHOW_CLEAR; }
    if(ar_mgr.think == VR_ARK_HoldThink) { return AR_BANNER_SHOW_PERK; }
    return VR_ARENA_Between() ? AR_BANNER_SHOW_NEXT : AR_BANNER_SHOW_FOES;
}

// Its number: the wave's foes left (up, and still to come), or the whole seconds to the next wave (the rest and the
// countdown still to go; VR_ARENA_CountThink has counted the second it is in).
float(float xShow) VR_ARENA_BannerValue =
{
    if(xShow == AR_BANNER_SHOW_FOES) { return ar_foes_left; }
    if(xShow != AR_BANNER_SHOW_NEXT) { return 0; }

    float t = max(ar_mgr.nextthink - time, 0);
    if(ar_mgr.think == VR_ARENA_CountStart) { t = t + VR_ARENA_Opt_Countdown(); }
    if(ar_mgr.think == VR_ARENA_CountThink) { t = t + ar_count; }
    return ceil(t);
}

string() VR_ARENA_BannerText =
{
    float show = VR_ARENA_BannerShow();
    return sprintf("{arena:%.0f %.0f %.0f %.0f %.0f %.0f %.0f}", ar_wave, (ar_wave > 0 && VR_ARENA_IsBossWave(ar_wave)) ? 1 : 0, show,
                   VR_ARENA_BannerValue(show), ar_plan_n, VR_ARENA_BannerSeconds(), ar_score);
}

// Every frame (VR_ARENA_Frame): the boards written when a number has moved.
void() VR_ARENA_BannerFrame =
{
    if(ar_banner_n <= 0) { return; }

    // (the clock's end: it runs again if a jump takes the arena on from a cleared preset)
    if(ar_down || ar_preset_over) { if(ar_run_end <= 0) { ar_run_end = time; } }
    else { ar_run_end = 0; }

    float on = cvar_hget(cvarh_vr_arena_banners) != 0 ? 2 : 1;
    float show = VR_ARENA_BannerShow();
    float value = VR_ARENA_BannerValue(show);
    float secs = VR_ARENA_BannerSeconds();
    if(on == ar_bn_on && (on == 1 || (ar_wave == ar_bn_wave && show == ar_bn_show && value == ar_bn_value &&
                                      ar_plan_n == ar_bn_total && secs == ar_bn_secs && ar_score == ar_bn_score)))
    {
        return;
    }
    ar_bn_on = on;
    ar_bn_wave = ar_wave;
    ar_bn_show = show;
    ar_bn_total = ar_plan_n;
    ar_bn_value = value;
    ar_bn_secs = secs;
    ar_bn_score = ar_score;

    string text = on == 2 ? VR_ARENA_BannerText() : "";
    entity e = find(world, classname, "info_arena_banner");
    while(e)
    {
        worldtext_hsettext(e.ar_banner_h, text);
        e = find(e, classname, "info_arena_banner");
    }
}

/*QUAKED info_arena_banner (0 .8 .8) (-16 -16 -16) (16 16 16)
A Survival Arena board: the run's wave, foes left, time and score, live, while the arena runs in this map. The origin is
the middle of the board's face (its depth goes behind it: keep 5.6 x its scale clear). "angle" the way it faces.
"worldtext_scale": units a pixel of its 250 x 40 face (default 2.2: 550 by 88, with its bezel 566 by 104). With no arena
running here it does nothing.
*/
void() info_arena_banner =
{
    if(!ar_mgr) { remove(self); return; }

    self.ar_banner_h = worldtext_hmake();
    worldtext_hsetpos(self.ar_banner_h, self.origin);
    worldtext_hsetangles(self.ar_banner_h, self.angles + '0 180 0');
    worldtext_hsethalign(self.ar_banner_h, 1);
    worldtext_hsetscale(self.ar_banner_h, self.worldtext_scale != 0 ? self.worldtext_scale : AR_BANNER_SCALE);
    ar_banner_n = ar_banner_n + 1;
}
