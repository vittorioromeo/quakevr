// vr_arena_banners.qc -- Survival Arena's banners: the run's numbers on boards high up in the arena, live (the author's
// ask, 2026-11: "real-time text banners in the arena showing enemies left and information about the current wave, time,
// score and so on").
//
// Placed by the map: each info_arena_banner is a board, the same world text board as func_worldtext_banner's (the engine's
// CRT boards, vr_text3d.cpp: Screens > World Text Font, vr_worldtext_hue). A new arena map gets them by placing these;
// nothing here knows a map or a coordinate. With no arena running in the map (ar_mgr) the entity goes and no board is
// made. vr_arena_banners 0 empties the boards.
//
// The text is two lines of one width, so a board keeps its size as the numbers change:
//   WAVE  12      FOES LEFT  14     (BOSS  12 on a boss wave)
//   TIME 03:21  SCORE    4500
// Between waves the top line's right says WAVE 13 IN   7, or CHOOSE A PERK while the cards hold the rest; at the end of a
// run YOU FELL, or CLEARED. The time starts with the run's first wave and stops with its end.

#define AR_BANNER_RIGHT_FOES   0
#define AR_BANNER_RIGHT_NEXT   1
#define AR_BANNER_RIGHT_PERK   2
#define AR_BANNER_RIGHT_FELL   3
#define AR_BANNER_RIGHT_CLEAR  4

.float ar_banner_h;     // an info_arena_banner: its world text's handle

float ar_banner_n;      // the map's boards

// What the boards show now, so a board is only sent a new text when a number has moved. Not saved: a load starts every
// board from its spawn (empty), and the first frame after it writes them again.
nosave float ar_bn_on;  // 0 nothing written yet, 1 off, 2 on
nosave float ar_bn_wave;
nosave float ar_bn_right;
nosave float ar_bn_value;
nosave float ar_bn_secs;
nosave float ar_bn_score;

// The run's seconds so far: from its first wave to its end.
float() VR_ARENA_BannerSeconds =
{
    if(ar_run_at <= 0) { return 0; }
    return floor((ar_run_end > 0 ? ar_run_end : time) - ar_run_at);
}

// What the top line's right says.
float() VR_ARENA_BannerRight =
{
    if(ar_down) { return AR_BANNER_RIGHT_FELL; }
    if(ar_preset_over) { return AR_BANNER_RIGHT_CLEAR; }
    if(ar_mgr.think == VR_ARK_HoldThink) { return AR_BANNER_RIGHT_PERK; }
    return VR_ARENA_Between() ? AR_BANNER_RIGHT_NEXT : AR_BANNER_RIGHT_FOES;
}

// Its number: the wave's foes left (up, and still to come), or the whole seconds to the next wave (the rest and the
// countdown still to go; VR_ARENA_CountThink has counted the second it is in).
float(float xRight) VR_ARENA_BannerValue =
{
    if(xRight == AR_BANNER_RIGHT_FOES) { return ar_foes_left; }
    if(xRight != AR_BANNER_RIGHT_NEXT) { return 0; }

    float t = max(ar_mgr.nextthink - time, 0);
    if(ar_mgr.think == VR_ARENA_CountStart) { t = t + VR_ARENA_Opt_Countdown(); }
    if(ar_mgr.think == VR_ARENA_CountThink) { t = t + ar_count; }
    return ceil(t);
}

string() VR_ARENA_BannerText =
{
    float right = VR_ARENA_BannerRight();
    float value = VR_ARENA_BannerValue(right);
    float secs = min(VR_ARENA_BannerSeconds(), 5999); // (99:59: the board's width)
    float mins = floor(secs / 60);

    string left;
    if(ar_wave <= 0) { left = "WAVE   -"; }
    else { left = sprintf("%s %3.0f", VR_ARENA_IsBossWave(ar_wave) ? "BOSS" : "WAVE", ar_wave); }

    string say = "";
    if(right == AR_BANNER_RIGHT_FOES) { say = sprintf("FOES LEFT %3.0f", value); }
    if(right == AR_BANNER_RIGHT_NEXT) { say = sprintf("WAVE %.0f IN %2.0f", ar_wave + 1, value); }
    if(right == AR_BANNER_RIGHT_PERK) { say = "CHOOSE A PERK"; }
    if(right == AR_BANNER_RIGHT_FELL) { say = "YOU FELL"; }
    if(right == AR_BANNER_RIGHT_CLEAR) { say = "CLEARED"; }

    return sprintf("%s  %13s\nTIME %02.0f:%02.0f %s", left, say, mins, secs - mins * 60, sprintf("SCORE %6.0f", ar_score));
}

// Every frame (VR_ARENA_Frame): the boards written when a number has moved.
void() VR_ARENA_BannerFrame =
{
    if(ar_banner_n <= 0) { return; }

    // (the clock's end: it runs again if a jump takes the arena on from a cleared preset)
    if(ar_down || ar_preset_over) { if(ar_run_end <= 0) { ar_run_end = time; } }
    else { ar_run_end = 0; }

    float on = cvar_hget(cvarh_vr_arena_banners) != 0 ? 2 : 1;
    float right = VR_ARENA_BannerRight();
    float value = VR_ARENA_BannerValue(right);
    float secs = VR_ARENA_BannerSeconds();
    if(on == ar_bn_on && (on == 1 || (ar_wave == ar_bn_wave && right == ar_bn_right && value == ar_bn_value &&
                                      secs == ar_bn_secs && ar_score == ar_bn_score)))
    {
        return;
    }
    ar_bn_on = on;
    ar_bn_wave = ar_wave;
    ar_bn_right = right;
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
the middle of the board's face (its depth goes behind it: keep 0.7 of a character clear). "angle" the way it faces.
"worldtext_scale": its size, 8 units a character at 1. It is 23 characters across and 2 down, with 1.2 characters round
them. With no arena running here it does nothing.
*/
void() info_arena_banner =
{
    if(!ar_mgr) { remove(self); return; }

    self.ar_banner_h = worldtext_hmake();
    worldtext_hsetpos(self.ar_banner_h, self.origin);
    worldtext_hsetangles(self.ar_banner_h, self.angles + '0 180 0');
    worldtext_hsethalign(self.ar_banner_h, 1);
    if(self.worldtext_scale != 0) { worldtext_hsetscale(self.ar_banner_h, self.worldtext_scale); }
    ar_banner_n = ar_banner_n + 1;
}
