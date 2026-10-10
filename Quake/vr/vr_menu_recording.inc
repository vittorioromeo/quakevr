
#include "Zancle/Container/Vector.hpp"

// vr_menu_recording.inc -- the Recording page (Graphics > Recording): what the desktop window shows while in VR, for
// recording it with OBS (vr_window.cpp; ROUND21.md, "Recording: smoothed mirror and spectator camera"); and slow motion
// (vr_timescale.cpp; ROUND21.md, "Slow motion"). Included by
// vr_menu.cpp; self-contained, so it can move with a reorganized menu.

[[nodiscard]] za::Vector<Item> pageRecording()
{
    return {
        cycle("Window View", "vr_window_view",
            {{3.f, "Both Eyes (raw)"}, {0.f, "Left Eye (raw)"}, {1.f, "Left Eye (smoothed)"}, {4.f, "Right Eye (raw)"},
                {5.f, "Right Eye (smoothed)"}, {2.f, "Spectator Camera"}})
            .help("What the desktop window shows, to record it (OBS: a Window Capture of it). Both Eyes: the two lenses' "
                  "images side by side, as they are. Left or Right Eye (raw): that eye's image as it is, shaking with your "
                  "head. Smoothed: the same image, turned to follow a steadied head and cropped to the window: free. "
                  "Spectator Camera (Recording Mode): the game drawn a third time for the window, from your head, "
                  "steadied, with its own field of view: sharpest and widest, and it costs about one more eye's "
                  "rendering. The headset's view is the same in all of them."),
        open("Desktop Mirror: Body and Display", pageIndex(pageBodyDisplay)).help("The desktop mirror is on Body and Display."),
        toggle("Flat HUD on the Mirror", "vr_window_hud_mirror")
            .help("The flat status bar over the window's raw and smoothed eye views (on by default)."),
        toggle("Flat HUD on the Spectator Camera", "vr_window_hud_spectator")
            .help("The flat status bar over the Spectator Camera (off by default: the recording shows the game as the "
                  "headset does, with the wrist gadget as its HUD)."),
        toggle("Hide Head Text on the Mirror", "vr_mirror_hide_hud_text")
            .help("Hides the text that floats in front of your head (pickup messages, centre prints, notify lines) in the "
                  "window's raw and smoothed eye views (off by default; in the raw views also the window's own copy of "
                  "that text, at its top). The headset still shows it."),
        toggle("Hide Head Text on the Spectator Camera", "vr_spectator_hide_hud_text")
            .help("Hides the text that floats in front of your head (pickup messages, centre prints, notify lines) in the "
                  "Spectator Camera (on by default: clean footage for trailers). The headset still shows it; menus "
                  "and the wrist gadget stay."),
        toggle("Recording Mode (No Tips or Messages)", "vr_recording_clean")
            .help("No tips, no messages on the wrist gadget and no text in front of your head (pickups, centre prints, "
                  "notify lines), in the headset too: clean takes. The trailer scene (vrtrailer) turns it on by "
                  "itself while it is loaded; menus and the wrist gadget's own display stay."),
        header("OBS"),
        toggle("OBS Recording Control", "vr_obs")
            .help("While a menu is open, the game looks for OBS Studio on this PC: when it finds it, a row above the "
                  "Spectator Camera's switch (bottom left of the headset's menus) shows whether OBS is recording, and "
                  "for how long; point at it and press to start or stop. In OBS (28 or later): Tools > WebSocket Server "
                  "Settings > Enable WebSocket server (port 4455). With Enable Authentication on, its password (Show "
                  "Connect Info) goes in the console: vr_obs_password \"...\" (kept in the config in plain text); or "
                  "turn authentication off. OBS on another PC: vr_obs_host and vr_obs_port. Off: never looks."),
        info(qvr::obs::statusLine),
        command("Start or Stop Recording", "vr_obs_toggle")
            .help("vr_obs_toggle (bindable): OBS's recording started or stopped, as a press on the menus' OBS row."),
        command("Connect to OBS Now", "vr_obs_connect")
            .help("vr_obs_connect: asks OBS now rather than at the next try (every 5 s while a menu is open); after "
                  "turning on OBS's WebSocket server or setting vr_obs_password."),
        header("Steadying"),
        slider("Smoothing", "vr_window_smooth", 0.f, 0.5f, 0.01f, "%.2f s")
            .help("How much the head's shake is smoothed out: the view's lag behind your head. 0.1-0.2 s takes out the "
                  "shake and keeps turns lively; more glides like a camera operator's."),
        slider("Level Horizon", "vr_window_level", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of your head's tilt to the side is taken out of the window's view: 1 keeps the horizon level."),
        header("Smoothed Mirror"),
        slider("Zoom", "vr_window_zoom", 1.f, 2.f, 0.05f, "%.2fx")
            .help("How much of the eye's image the window shows: 1 the widest crop of the window's shape; higher crops "
                  "closer and leaves a margin for the steadied view to turn in. Past the margin, the view follows your "
                  "head: raise it if quick turns show the view catching up."),
        header("Spectator Camera"),
        toggle("Preview in the Menus", "vr_spectator_preview")
            .help("While the Spectator Camera is on, a small copy of what the window shows, in the bottom left corner "
                  "of the headset's menus, above its switch: what the recording looks like (on by default). Only in the "
                  "headset: the recording itself shows the menu without it."),
        toggle("Bullet Time Effects", "vr_spectator_bullettime_fx")
            .help("Shows bullet time's drained colour, tint and darkened edges in the Spectator Camera (on by default). "
                  "Off: the camera keeps its normal look. The headset is unchanged. Strength and colour: Combat > "
                  "Bullet Time. The recording's Time Scale alone never applies this look."),
        slider("Field of View", "vr_spectator_fov", 60.f, 150.f, 1.f, "%.0f deg").extend(40.f, 160.f)
            .help("The window's horizontal field of view (90 as a flat game; the vertical one follows the window's shape)."),
        cycle("Frame Rate", "vr_spectator_rate", {{1.f, "Every Frame"}, {2.f, "Every 2nd Frame"}, {3.f, "Every 3rd Frame"},
                                                 {60.f, "60 fps"}})
            .help("How often the camera is drawn; the window shows its last image between. 60 fps (the default): as "
                  "often as a 60 fps recording takes (OBS), a third of the cost saved at 90 Hz, half at 120 Hz. Every "
                  "Frame: the headset's rate (smoother only if you record at more than 60 fps). Every 2nd or 3rd Frame: "
                  "half or a third of the headset's rate (45 or 30 fps at 90 Hz), for a slower PC."),
        slider("Resolution Scale", "vr_spectator_scale", 0.5f, 2.f, 0.25f, "%.2fx").extend(0.25f, 2.f)
            .help("Its resolution, times the window's: 1 sharp at the window's size; 0.75 (the default) draws 56% of "
                  "the pixels, drawn up to the window smoothly (a recording's compression softens more than that); lower "
                  "costs less (the pixels go with its square); 2 supersamples (four times the pixels)."),
        cycle("Anti-Aliasing", "vr_spectator_aa", {{0.f, "Off"}, {1.f, "Window's MSAA"}})
            .help("Window's MSAA (the default): the window's multisampling (Anti-Aliasing, vid_fsaa), as the eyes: "
                  "smooth edges, which a recording shows (4x costs the camera about a tenth more; nothing without "
                  "vid_fsaa). Off: none, the cheapest."),
        slider("Position Smoothing", "vr_spectator_pos_smooth", 0.f, 0.3f, 0.01f, "%.2f s")
            .help("How much your head's small movements are smoothed out (its lag). Walking, turning and teleporting are "
                  "never smoothed."),
        header("Slow Motion"),
        slider("Time Scale", "vr_timescale", 0.1f, 1.f, 0.05f, "%.2fx").extend(0.05f, 4.f)
            .help("The game's speed, for footage to speed up in editing (0.25: record at a quarter speed, play it back 4x "
                  "faster): monsters, missiles, physics, animations, effects and timers all slowed alike. Your view "
                  "follows your head in real time; your hands move with the slowed world. Single player. Not saved. "
                  "Bindable: bind a key to vr_slowmo (toggles to the scale below) or vr_slowmo 0.5."),
        slider("Toggle's Time Scale", "vr_slowmo_scale", 0.1f, 1.f, 0.05f, "%.2fx").extend(0.05f, 4.f)
            .help("The time scale the vr_slowmo command toggles to (and back to 1)."),
        slider("Ease In and Out", "vr_timescale_ramp", 0.f, 2.f, 0.1f, "%.1f s")
            .help("Seconds the time scale takes to slide all the way from 1 to 0: a gradual slow-down and speed-up; 0 "
                  "switches at once."),
        toggle("Slow Sounds", "vr_timescale_sound")
            .help("On: sounds play slowed and lower with the game, as a slowed recording sounds (sped up in editing, "
                  "they're right again). Off: at their normal speed and pitch. Music is never slowed. For the cleanest "
                  "sound sped up, use the Game-Time Sound File below."),
        toggle("Game-Time Sound File", "vr_timescale_wav")
            .help("For trailers: while Log Highlights runs, the game's sound is also mixed a second time as it sounds at "
                  "normal speed, in the game's time (no music; what you hear doesn't change), to "
                  "highlights/<log>_gametime.wav (24-bit), from the sync mark's beep (a new file at each Sync Mark Now). "
                  "It lines up with a recording sped up by the time scale; use it instead of the slowed sound "
                  "(docs/vr-port/TRAILER.md). About 16 MB a minute; a second spatial mix's CPU while it runs."),
        slider("Hand Speed Limit", "vr_timescale_hand_speed", 0.f, 20.f, 0.5f, "%.1f m/s")
            .help("In slow motion, your hands follow your controllers as fast as the slowed world allows: at most this "
                  "fast in the game's time (a quarter of it in real time at 0.25x). A slow move is followed exactly; "
                  "a real-speed swing lands as a hard blow, not a supersonic one. 0: no limit (the hands keep up)."),
        slider("Hand Turn Limit", "vr_timescale_hand_spin", 0.f, 40.f, 1.f, "%.0f rad/s")
            .help("Likewise, the most your hands turn per second of the game's time in slow motion. 0: no limit."),
        toggle("Turn in Real Time", "vr_timescale_turn_realtime")
            .help("On: the stick's smooth turn keeps its real-time speed in slow motion (sped up in editing, it looks "
                  "faster). Off: slowed with the world."),
        toggle("Move in Real Time", "vr_timescale_move_realtime")
            .help("On: your own movement (the stick, jumps, falls) and your weapons' timing keep their real-time speed "
                  "in slow motion; your hands are still slowed. Off: slowed with the world."),
        toggle("Sandevistan: You at Full Speed", "vr_sandevistan")
            .help("On: in slow motion only the world slows: you move, turn, swing, shoot and reload at full speed, your "
                  "hands follow your controllers as they are (Your Missiles at Full Speed Too: Combat > Bullet Time)."),
        open("Bullet Time's Look: Bullet Time", pageIndex(pageBulletTime)).help("Bullet time's look is on Combat > Bullet Time."),
        header("Highlight Markers"),
        toggle("Log Highlights", "vr_highlights")
            .help("For trailers (docs/vr-port/TRAILER.md): logs the cool moments as they happen (gibs, multi-kills, parries, "
                  "counters, bullet time, explosions and chains, shot grenades, stuck axes, hook swings...), each scored, "
                  "in real time (right in slow motion). Turned on: a sync mark (a white flash over the window, never the "
                  "headset, and a beep) to line the log up with the recording: start recording first. Turned off: the "
                  "log's DaVinci Resolve markers are written. Files: quakevr/highlights/. Bindable: vr_highlights_start, "
                  "vr_highlights_stop. Not saved."),
        command("Sync Mark Now", "vr_highlights_sync")
            .help("Another flash and beep in the running log (vr_highlights_sync): a recording started after the log."),
        command("Mark This Moment", "vr_highlight_mark")
            .help("A moment of your own in the running log (score 5). Bindable: vr_highlight_mark [score] [words]."),
        slider("Sync Flash", "vr_highlights_flash", 0.f, 1.f, 0.05f, "%.2f s")
            .help("How long the sync mark's flash lasts over the window (0: none)."),
        slider("Sync Beep", "vr_highlights_beep", 0.f, 1.f, 0.1f, "%.1f")
            .help("The sync mark's beep's volume (0: none): a 1 kHz tone the trailer script can find in the recording."),
        cycle("Markers' Frame Rate", "vr_highlights_fps", {{24.f, "24 fps"}, {25.f, "25 fps"}, {30.f, "30 fps"},
                                                         {50.f, "50 fps"}, {60.f, "60 fps"}, {120.f, "120 fps"}})
            .help("The marker file's (EDL) frame rate: the recording's and the Resolve timeline's."),
        slider("Least Score for a Marker", "vr_highlights_min_score", 0.f, 10.f, 0.5f, "%.1f")
            .help("Moments scoring less are left out of the marker file (the CSV and JSON keep them all): 2 leaves out "
                  "plain kills."),
        slider("Multi-Kill Window", "vr_highlights_multikill", 0.5f, 10.f, 0.5f, "%.1f s")
            .help("Kills at most this far apart (game seconds) make one multi-kill."),
    };
}

// Combat > Bullet Time (vr_bullettime.cpp; ROUND21.md, "Slow motion: bullet time and Sandevistan").
[[nodiscard]] za::Vector<Item> pageBulletTime()
{
    return {
        toggle("Bullet Time", "vr_bullettime_enabled")
            .help("Tap your gadget's screen hard with your other hand (or the butt of the gun it holds): the world "
                  "slows for as long as the meter on the gadget's screen (TIME) lasts; tap again to stop. Then a "
                  "cooldown, and the meter fills again. Bindable: vr_bullettime. Single player."),
        cycle("Trigger", "vr_bullettime_trigger",
            {{0.f, "Gadget (Screen Tap)"}, {1.f, "Left Stick Press"}, {2.f, "Right Stick Press"}})
            .help("What starts and stops it. Gadget: a hard tap on its screen (its settings below). A stick press: that "
                  "press does only this in the game (never its bound key: by default the left's is Run, the right's "
                  "Reload), and the screen tap does nothing."),
        command("Start or Stop It Now", "vr_bullettime").help("vr_bullettime: as the screen tap."),
        slider("Time Scale", "vr_bullettime_scale", 0.1f, 1.f, 0.05f, "%.2fx").extend(0.05f, 1.f)
            .help("How fast the world runs while it lasts (0.3: under a third of its speed)."),
        slider("Duration", "vr_bullettime_duration", 1.f, 20.f, 0.5f, "%.1f s").extend(0.5f, 120.f)
            .help("Real seconds a full meter lasts."),
        slider("Recharge Time", "vr_bullettime_recharge", 2.f, 120.f, 1.f, "%.0f s").extend(0.5f, 600.f)
            .help("Real seconds the meter takes to fill from empty."),
        slider("Cooldown", "vr_bullettime_cooldown", 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("Real seconds after it ends (or is stopped) before it can start again; the meter waits too."),
        slider("Least Meter to Start", "vr_bullettime_min", 0.05f, 1.f, 0.05f, "%.2f")
            .help("How full the meter must be to start (it then runs until empty or stopped)."),
        slider("End Sound Plays At Most", "vr_bullettime_sound_off_max", 0.f, 3.f, 0.25f, "%.2f s").extend(0.f, 10.f)
            .help("The sound as it ends plays this long at most, then fades out (and stops at once if you start it "
                  "again). 0: the whole sound."),
        toggle("Sandevistan: You at Full Speed", "vr_bullettime_sandevistan")
            .help("On: only the world slows; you move, turn, swing, throw, shoot and reload at full speed. Off: you are "
                  "slowed with the world but your view (your hands follow your controllers as the slowed world allows)."),
        toggle("Your Missiles at Full Speed Too", "vr_sandevistan_missiles")
            .help("In Sandevistan: your rockets, grenades, nails, thrown weapons and the hook fly at full speed. Off: they "
                  "join the slowed world as they leave you. Props you throw always join it."),
        header("Look"),
        slider("Look's Strength", "vr_bullettime_fx", 0.f, 1.f, 0.05f, "%.2f")
            .help("While it runs, the view drained of colour, tinted and darkened at its edges: this much (0 off). Never "
                  "with the recording's Time Scale (Graphics > Recording)."),
        slider("Colour Drained", "vr_bullettime_fx_desat", 0.f, 1.f, 0.05f, "%.2f"),
        slider("Darkened Edges", "vr_bullettime_fx_vignette", 0.f, 1.f, 0.05f, "%.2f"),
        header("Screen Tap"),
        toggle("Tap the Screen", "vr_bullettime_tap")
            .help("Strike the gadget's screen hard and straight with your other hand, or with the butt of the gun it "
                  "holds. It must come straight into the screen fast enough and stop on it: a swing across it, a "
                  "soft touch or a hand brushing past does nothing."),
        cycle("Gesture", "vr_bullettime_tap_gesture", {{0.f, "Single Tap"}, {1.f, "Double Tap"}})
            .help("Single Tap: one hard tap starts or stops it. Double Tap: two taps in quick succession (each may be "
                  "lighter: Double Tap Force), lifting off the screen between them; a single tap does nothing."),
        slider("Double Tap Window", "vr_bullettime_tap_double_window", 0.2f, 1.f, 0.05f, "%.2f s").extend(0.1f, 2.f)
            .help("Double Tap: how soon after the first tap the second must come."),
        slider("Double Tap Least Gap", "vr_bullettime_tap_min_gap", 0.f, 0.2f, 0.01f, "%.2f s").extend(0.f, 0.5f)
            .help("Double Tap: the least time between the two taps. A second tap sooner (an arm wiggled or flicked fast) "
                  "counts as a new first tap, never as the double tap."),
        slider("Double Tap Force", "vr_bullettime_tap_double_speed", 0.1f, 3.f, 0.1f, "%.1f m/s").extend(0.05f, 6.f)
            .help("Double Tap: how fast each tap must come into the screen (Tap Force is the single tap's)."),
        slider("Tap Force", "vr_bullettime_tap_speed", 0.5f, 3.f, 0.1f, "%.1f m/s").extend(0.05f, 6.f)
            .help("How fast your hand (or the gun's butt, or what it holds) must come into the screen (either arm may "
                  "move). Higher: harder taps only. Single Tap's (Double Tap Force: the double tap's)."),
        slider("Tap Straightness", "vr_bullettime_tap_angle", 10.f, 80.f, 5.f, "%.0f deg").extend(1.f, 89.f)
            .help("How far off straight into the screen the strike may come. Lower: only straight strikes (a melee "
                  "swing across the screen comes from the side)."),
        slider("Tap Zone Margin", "vr_bullettime_tap_margin", 0.f, 8.f, 0.5f, "%.1f cm").extend(0.f, 20.f)
            .help("How far past the screen's edges the strike may land too."),
        slider("Tap Zone Depth", "vr_bullettime_tap_depth", 0.5f, 15.f, 0.5f, "%.1f cm").extend(0.5f, 30.f)
            .help("How close over the screen your hand (any of it: palm, fingers, knuckles, its back) or the gun's "
                  "butt must stop. Debug > Show Gadget Button: And the Screen Tap shows this zone."),
        slider("Tap Zone Width", "vr_bullettime_tap_width", 0.5f, 2.f, 0.05f, "%.2fx").extend(0.1f, 5.f)
            .help("The zone's width as a share of the screen's (before the margin). Debug > Show Gadget Button: And the "
                  "Screen Tap shows the zone and what strikes (your hand's surface blue, the gun's butt orange, white on the zone)."),
        slider("Tap Zone Height", "vr_bullettime_tap_height", 0.5f, 2.f, 0.05f, "%.2fx").extend(0.1f, 5.f)
            .help("... and its height."),
        slider("Tap Zone Across", "vr_bullettime_tap_x", -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("The zone moved along the screen's width (positive: to its right)."),
        slider("Tap Zone Up", "vr_bullettime_tap_y", -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("... along its height (positive: up the screen)."),
        slider("Tap Zone Out", "vr_bullettime_tap_z", -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("... and out of its face (negative: into your arm): Tap Zone Depth is measured from there."),
        toggle("The Gun's Butt Taps Too", "vr_bullettime_tap_butt")
            .help("On: the butt of the gun in your other hand taps the screen as your hand does. Off: only your hand."),
        slider("Gun Butt Reach", "vr_bullettime_tap_butt_depth", 1.f, 10.f, 0.5f, "%.1f cm").extend(0.f, 30.f)
            .help("How far from the gun's rearmost end its butt reaches: all of it within that strikes."),
        slider("Tap Must Stop", "vr_bullettime_tap_stop", 0.1f, 1.f, 0.05f, "%.2f")
            .help("The impact: the speed into the screen must drop to this much of its peak on it (a hand passing by "
                  "keeps its speed). 1: counts as it arrives, stopped or not."),
        slider("Tap Window", "vr_bullettime_tap_window", 0.05f, 1.f, 0.05f, "%.2f s")
            .help("How soon after its fastest the strike must stop on the screen."),
        toggle("Tap While Holding", "vr_bullettime_tap_holding")
            .help("On: the tapping hand may hold a weapon or a prop. Off: only an empty hand taps."),
        toggle("Tap During Two-Handed Holds", "vr_bullettime_tap_twohanded")
            .help("Off: no taps while both hands hold one weapon (your gadget hand on its foregrip), against accidents. "
                  "Moving together, the hands never tap anyway."),
        slider("Ignore Repeats For", "vr_bullettime_trigger_cooldown", 0.f, 2.f, 0.1f, "%.1f s")
            .help("After a tap counts, how long before the next one does (a bounce is one)."),
        slider("Haptic Tick", "vr_bullettime_haptic", 0.f, 2.f, 0.25f, "%.2f")
            .help("The tick in both hands when a tap counts (0: none)."),
        slider("Tap Click Volume", "vr_bullettime_tap_sound", 0.f, 1.f, 0.05f, "%.2f")
            .help("The gadget clicks from its screen as a tap registers: a short tick for a double tap's first, two "
                  "rising blips for the tap that starts or stops bullet time (0: silent)."),
        slider("Tap Glitch", "vr_bullettime_tap_glitch", 0.f, 2.f, 0.1f, "%.1f")
            .help("... and its screen glitches for a moment: bands torn sideways, the colours split (0: none)."),
        slider("Tap Glitch Time", "vr_bullettime_tap_glitch_time", 0.05f, 0.5f, 0.01f, "%.2f s")
            .help("How long the glitch lasts (the tap that starts or stops it a quarter longer)."),
        command("Try: First Tap", "vr_bullettime_tap_feedback_test")
            .help("vr_bullettime_tap_feedback_test: a double tap's first tap's click and glitch, without tapping."),
        command("Try: Activation Tap", "vr_bullettime_tap_feedback_test 1")
            .help("vr_bullettime_tap_feedback_test 1: the click and glitch of the tap that starts or stops it."),
        open("Gadget Side Button: Gear Lights", pageIndex(pageScreens))
            .help("The button on the gadget's lower edge now dims your gear's lights (HUD and Menus > Screens)."),
        header("Distortion Trails"),
        cycle("Distortion Trails", "vr_bullettime_trails", {{0.f, "Off"}, {1.f, "In Bullet Time"}, {2.f, "Always"}})
            .help("As in F.E.A.R.: a wake behind each bullet, nail, rocket, grenade and monster's shot bends the view "
                  "behind it like a rippling glass rod, fading along its length. In Bullet Time: while it runs (and "
                  "fading out after). Always: in normal time too. Off: no cost."),
        slider("Bend", "vr_bullettime_trails_strength", 0.f, 3.f, 0.05f, "%.2f")
            .help("How far they bend what is behind them (1: about half the trail's width)."),
        slider("Trail Life", "vr_bullettime_trails_life", 0.1f, 3.f, 0.05f, "%.2f s").extend(0.05f, 10.f)
            .help("Real seconds each part of a trail lasts, fading."),
        slider("Longest Trail", "vr_bullettime_trails_length", 0.5f, 15.f, 0.5f, "%.1f m").extend(0.1f, 50.f)
            .help("The most of a trail behind its projectile."),
        slider("Trail Width", "vr_bullettime_trails_width", 2.f, 80.f, 1.f, "%.0f cm").extend(0.5f, 200.f)
            .help("A bullet's and a nail's; rockets and grenades twice as wide, monsters' balls one and a half times."),
        slider("Fade After It Ends", "vr_bullettime_trails_fade", 0.f, 3.f, 0.1f, "%.1f s")
            .help("Real seconds the trails take to fade out as bullet time ends."),
        toggle("Behind Your Bullets", "vr_bullettime_trails_hitscan")
            .help("The shotguns' pellets and the rifles' bullets (whether their tracer is drawn or not)."),
        toggle("Behind Nails", "vr_bullettime_trails_nails"),
        toggle("Behind Rockets and Grenades", "vr_bullettime_trails_explosives"),
        toggle("Behind Monsters' Shots", "vr_bullettime_trails_enemy")
            .help("Enforcers' lasers, scrags' spit, knights' flames, vores' balls, zombies' flesh and grunts' bullets."),
    };
}
