
#include "Zancle/Container/Vector.hpp"

// vr_menu_recording.inc -- the Recording page (Graphics > Recording): what the desktop window shows while in VR, for
// recording it with OBS (vr_window.cpp; ROUND21.md, "Recording: smoothed mirror and spectator camera"); and slow motion
// (vr_timescale.cpp; ROUND21.md, "Slow motion"). Included by
// vr_menu.cpp; self-contained, so it can move with a reorganized menu.

[[nodiscard]] za::Vector<Item> pageRecording()
{
    return {
        row(opt::vr_window_view),
        open("Desktop Mirror: Body and Display", pageIndex(pageBodyDisplay)).help("The desktop mirror is on Body and Display."),
        row(opt::vr_window_hud_mirror),
        row(opt::vr_window_hud_spectator),
        row(opt::vr_mirror_hide_hud_text),
        row(opt::vr_spectator_hide_hud_text),
        row(opt::vr_recording_clean),
        row(opt::vr_mirror_hz),
        header("OBS"),
        row(opt::vr_obs),
        info(qvr::obs::statusLine),
        command("Start or Stop Recording", "vr_obs_toggle")
            .help("vr_obs_toggle (bindable): OBS's recording started or stopped, as a press on the menus' OBS row."),
        command("Connect to OBS Now", "vr_obs_connect")
            .help("vr_obs_connect: asks OBS now rather than at the next try (every 5 s while a menu is open); after "
                  "turning on OBS's WebSocket server or setting vr_obs_password."),
        header("Takes and Clips"),
        toggle("Takes", "vr_takes")
            .help("vr_takes: each OBS recording is a take, renamed <date>_<scene>_t<NN>_<rating> in OBS's folder and rated "
                  "Keep, Maybe or Reject after it (Reject moves it to rejected/: nothing is deleted). TRAILER.md, \"Takes and "
                  "clips\"."),
        info(qvr::takes::statusLine),
        command("Start or Stop a Take", "vr_take").help("vr_take (F7): OBS's recording started, or stopped and the rating prompt shown."),
        command("Clip That", "vr_clip")
            .help("vr_clip (F8): OBS's replay buffer saved: its last seconds (OBS: Settings > Output > Replay Buffer, Maximum "
                  "Replay Time), named as a take, rated clip."),
        command("Keep", "vr_take_keep").help("vr_take_keep (keypad 1): the last take or clip kept."),
        command("Maybe", "vr_take_maybe").help("vr_take_maybe (keypad 2): the last take or clip, maybe."),
        command("Reject", "vr_take_reject").help("vr_take_reject (keypad 3): the last take or clip moved to rejected/."),
        command("In Mark", "vr_take_in").help("vr_take_in (Home): an in mark at this moment of the take (and an OBS chapter)."),
        command("Out Mark", "vr_take_out").help("vr_take_out (PgUp): an out mark at this moment of the take."),
        command("Start Take Session", "vr_takes_session start")
            .help("vr_takes_session start: OBS's connection kept, its replay buffer started (vr_clip), the wrist gadget's "
                  "take buttons shown. The first take or clip starts one too."),
        command("End Take Session", "vr_takes_session end").help("vr_takes_session end: the session's takes listed; the replay buffer stopped."),
        toggle("Replay Buffer in a Session", "vr_takes_replay")
            .help("vr_takes_replay: OBS's replay buffer kept running while a take session is open, for Clip That."),
        toggle("Wrist Gadget Buttons", "vr_takes_gadget")
            .help("vr_takes_gadget: while a take session is open the wrist gadget's screen shows TAKE, CLIP, IN, OUT, KEEP, "
                  "MAYBE, REJECT (touch them), instead of the HUD. HIDE: the HUD again until the next take ends. The gadget "
                  "is in the spectator camera's view: HIDE it for a clean shot."),
        cycle("Mark Chord", "vr_takes_chord",
            {{0.f, "Off"}, {1.f, "Left Grip + X (In) / Y (Out)"}, {2.f, "Right Grip + A (In) / B (Out)"}})
            .help("vr_takes_chord: during a take, an in or out mark by controller: hold the grip, then press the lower (In) or "
                  "upper (Out) button of that hand. Those presses don't reach the game. Only while a take runs."),
        toggle("Recording Light and Prompt", "vr_takes_light")
            .help("vr_takes_light: a small red light while a take runs, and the rating prompt after it. In the headset only "
                  "(never the mirror or spectator camera, so OBS doesn't record it); flat: a small top-right corner line "
                  "(which OBS does record: turn it off for clean shots)."),
        slider("Light Size", "vr_takes_light_size", 0.5f, 3.f, 0.1f, "%.1fx"),
        slider("Light Position: Right", "vr_takes_light_right", -0.6f, 0.6f, 0.01f, "%.2f").help("vr_takes_light_right: headset only."),
        slider("Light Position: Up", "vr_takes_light_up", -0.5f, 0.5f, 0.01f, "%.2f").help("vr_takes_light_up: headset only."),
        slider("Light Distance", "vr_takes_light_dist", 0.3f, 4.f, 0.1f, "%.1f m").help("vr_takes_light_dist: headset only."),
        slider("Rating Prompt Time", "vr_takes_prompt_time", 0.f, 60.f, 1.f, "%.0f s"),
        slider("Clip Length if Unknown", "vr_takes_clip_seconds", 5.f, 300.f, 5.f, "%.0f s")
            .help("vr_takes_clip_seconds: a clip's length on the session's timeline when OBS doesn't say."),
        slider("Wait for OBS", "vr_takes_connect_wait", 1.f, 30.f, 1.f, "%.0f s")
            .help("vr_takes_connect_wait: a take or clip asked for before OBS is connected waits this long for it."),
        header("Steadying"),
        row(opt::vr_window_smooth),
        row(opt::vr_window_level),
        header("Smoothed Mirror"),
        row(opt::vr_window_zoom),
        header("Spectator Camera"),
        row(opt::vr_spectator_preview),
        row(opt::vr_spectator_bullettime_fx),
        row(opt::vr_spectator_fov),
        row(opt::vr_spectator_rate),
        row(opt::vr_spectator_scale),
        row(opt::vr_spectator_skip_hidden),
        row(opt::vr_spectator_aa),
        row(opt::vr_spectator_pos_smooth),
        header("Slow Motion"),
        row(opt::vr_timescale),
        row(opt::vr_slowmo_scale),
        row(opt::vr_timescale_ramp),
        row(opt::vr_timescale_sound),
        row(opt::vr_timescale_wav),
        row(opt::vr_timescale_hand_speed),
        row(opt::vr_timescale_hand_spin),
        row(opt::vr_timescale_turn_realtime),
        row(opt::vr_timescale_move_realtime),
        row(opt::vr_sandevistan),
        open("Bullet Time's Look: Bullet Time", pageIndex(pageBulletTime)).help("Bullet time's look is on Combat > Bullet Time."),
        header("Highlight Markers"),
        row(opt::vr_highlights),
        command("Sync Mark Now", "vr_highlights_sync")
            .help("Another flash and beep in the running log (vr_highlights_sync): a recording started after the log."),
        command("Mark This Moment", "vr_highlight_mark")
            .help("A moment of your own in the running log (score 5). Bindable: vr_highlight_mark [score] [words]."),
        row(opt::vr_highlights_flash),
        row(opt::vr_highlights_beep),
        row(opt::vr_highlights_fps),
        row(opt::vr_highlights_min_score),
        row(opt::vr_highlights_multikill),
    };
}

// Combat > Bullet Time (vr_bullettime.cpp; ROUND21.md, "Slow motion: bullet time and Sandevistan").
[[nodiscard]] za::Vector<Item> pageBulletTime()
{
    return {
        row(opt::vr_bullettime_enabled),
        row(opt::vr_bullettime_trigger),
        command("Start or Stop It Now", "vr_bullettime").help("vr_bullettime: as the screen tap."),
        row(opt::vr_bullettime_scale),
        row(opt::vr_bullettime_duration),
        cycle("Meter Refill", "vr_bullettime_meter_mode", {{1.f, "Earned by Damage"}, {0.f, "Over Time"}})
            .help("Earned by Damage: the meter never fills by itself, only by the damage you deal living enemies (what "
                  "health they had at most: no overkill; never corpses, props or enemies hurting each other). Over Time: it "
                  "fills by itself after the cooldown (Recharge Time). Kept from level to level and in saves; full on a "
                  "new game."),
        row(opt::vr_bullettime_earn_damage),
        row(opt::vr_bullettime_earn_skill0),
        row(opt::vr_bullettime_earn_skill1),
        row(opt::vr_bullettime_earn_skill2),
        row(opt::vr_bullettime_earn_skill3),
        row(opt::vr_bullettime_earn_active),
        row(opt::vr_bullettime_recharge),
        row(opt::vr_bullettime_cooldown),
        row(opt::vr_bullettime_sandevistan),
        row(opt::vr_sandevistan_missiles),
        header("Look"),
        row(opt::vr_bullettime_fx),
        header("Screen Tap"),
        row(opt::vr_bullettime_tap),
        cycle("Gesture", "vr_bullettime_tap_gesture", {{0.f, "Single Tap"}, {1.f, "Double Tap"}})
            .help("Single Tap: one hard tap starts or stops it. Double Tap: two taps in quick succession (each may be "
                  "lighter: Double Tap Force), lifting off the screen between them; a single tap does nothing."),
        slider("Quiet After Blows", "vr_bullettime_tap_blow_quiet", 0.f, 1.5f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("Taps don't count for this long after a melee blow of yours lands, or you shove or bash (a double "
                  "tap's first is dropped too): no slow motion by accident in a melee's thick. 0: off."),
        row(opt::vr_bullettime_tap_calm_speed),
        row(opt::vr_bullettime_tap_both_held),
        row(opt::vr_bullettime_tap_butt),
        row(opt::vr_bullettime_tap_holding),
        row(opt::vr_bullettime_tap_twohanded),
        row(opt::vr_bullettime_trigger_cooldown),
        row(opt::vr_bullettime_tap_sound),
        command("Try: First Tap", "vr_bullettime_tap_feedback_test")
            .help("vr_bullettime_tap_feedback_test: a double tap's first tap's click and glitch, without tapping."),
        command("Try: Activation Tap", "vr_bullettime_tap_feedback_test 1")
            .help("vr_bullettime_tap_feedback_test 1: the click and glitch of the tap that starts or stops it."),
        open("Gadget Side Button: Gear Lights", pageIndex(pageScreens))
            .help("The button on the gadget's lower edge now dims your gear's lights (HUD and Menus > Screens)."),
        header("Distortion Trails"),
        row(opt::vr_bullettime_trails),
        row(opt::vr_bullettime_trails_hitscan),
        row(opt::vr_bullettime_trails_nails),
        row(opt::vr_bullettime_trails_explosives),
        row(opt::vr_bullettime_trails_enemy),
    };
}
