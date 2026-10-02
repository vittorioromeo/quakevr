
#include "Zancle/Container/Vector.hpp"

// vr_menu_recording.inc -- the Recording page (Graphics > Recording): what the desktop window shows while in VR, for
// recording it with OBS (vr_window.cpp; ROUND21.md, "Recording: smoothed mirror and spectator camera"); and slow motion
// (vr_timescale.cpp; ROUND21.md, "Slow motion"). Included by
// vr_menu.cpp; self-contained, so it can move with a reorganized menu.

[[nodiscard]] za::Vector<Item> pageRecording()
{
    return {
        cycle("Window View", "vr_window_view", {{0.f, "Left Eye (raw)"}, {1.f, "Smoothed Mirror"}, {2.f, "Spectator Camera"}})
            .help("What the desktop window shows, to record it (OBS: a Window Capture of it). Left Eye: the eye's image as it "
                  "is, shaking with your head. Smoothed Mirror: the same image, turned to follow a steadied head and cropped "
                  "to the window: free. Spectator Camera (Recording Mode): the game drawn a third time for the window, from "
                  "your head, steadied, with its own field of view: sharpest and widest, and it costs about one more eye's "
                  "rendering. The headset's view is the same in all three."),
        cycle("Desktop Mirror", "vr_mirror", {{0.f, "Off"}, {1.f, "On"}, {2.f, "Both Eyes (Left Eye view)"}})
            .help("Off: the window shows nothing of the game in VR (in any Window View), which saves its small cost."),
        toggle("Flat HUD on the Mirror", "vr_window_hud_mirror")
            .help("The flat status bar over the window's Left Eye and Smoothed Mirror views (on by default)."),
        toggle("Flat HUD on the Spectator Camera", "vr_window_hud_spectator")
            .help("The flat status bar over the Spectator Camera (off by default: the recording shows the game as the "
                  "headset does, with the wrist gadget as its HUD)."),
        toggle("Hide Head Text on the Mirror", "vr_mirror_hide_hud_text")
            .help("Hides the text that floats in front of your head (pickup messages, centre prints, notify lines) in the "
                  "window's Left Eye and Smoothed Mirror views (off by default; in the Left Eye view also the window's own "
                  "copy of that text, at its top). The headset still shows it."),
        toggle("Hide Head Text on the Spectator Camera", "vr_spectator_hide_hud_text")
            .help("Hides the text that floats in front of your head (pickup messages, centre prints, notify lines) in the "
                  "Spectator Camera (on by default: clean footage for trailers). The headset still shows it; menus "
                  "and the wrist gadget stay."),
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
            .help("On: sounds play slowed and lower with the game: the whole mix is made at normal speed in the game's "
                  "time, then played slower, so a slowed recording sped up in editing (as varispeed: no pitch "
                  "correction) sounds exactly as the game at normal speed. Off: at their normal speed and pitch. Music "
                  "is never slowed."),
        toggle("Game-Time Sound File", "vr_timescale_wav")
            .help("On: while Log Highlights runs, the game's sound (no music) is also written as it sounds at normal "
                  "speed, in the game's time, 24-bit: highlights/<log>_gametime.wav, from the sync mark's beep (a new file "
                  "at each Sync Mark Now). It lines up with a recording sped up by the time scale "
                  "(docs/vr-port/TRAILER.md). About 16 MB a minute."),
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
        slider("Bullet Time's Look", "vr_bullettime_fx", 0.f, 1.f, 0.05f, "%.2f")
            .help("Bullet time (Combat > Bullet Time) drains, tints and darkens the eyes' view while it runs; the window's "
                  "Left Eye and Smoothed Mirror show it, the Spectator Camera doesn't. Set 0 to record bullet time "
                  "without it. The Time Scale above never has it."),
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
            .help("Tap your gadget's wrist hard with your other hand (or press the inner button on the gadget's lower "
                  "edge with its fingertip): the world slows for as long as the meter on the gadget's screen (TIME) "
                  "lasts; tap again to stop. Then a cooldown, and the meter fills again. Bindable: vr_bullettime. "
                  "Single player."),
        command("Start or Stop It Now", "vr_bullettime").help("vr_bullettime: as the gadget's button."),
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
        header("Wrist Tap"),
        toggle("Tap the Wrist", "vr_bullettime_tap")
            .help("Strike your other hand (the hand itself, not what it holds) against the wrist wearing the gadget, "
                  "anywhere near it: no button to find. It must come at it fast enough and stop there, so resting or "
                  "brushing hands do nothing."),
        slider("Tap Zone Size", "vr_bullettime_tap_radius", 5.f, 30.f, 1.f, "%.0f cm").extend(1.f, 60.f)
            .help("How near the gadget's middle your other hand's middle must come."),
        slider("Tap Force", "vr_bullettime_tap_speed", 0.3f, 3.f, 0.1f, "%.1f m/s").extend(0.05f, 6.f)
            .help("How fast your hands must come together (either may move). Higher: harder taps only."),
        slider("Tap Must Stop", "vr_bullettime_tap_stop", 0.1f, 1.f, 0.05f, "%.2f")
            .help("The impact: the hands' speed must drop to this much of its peak in the zone (a hand swinging past "
                  "keeps its speed). 1: counts as it arrives, stopped or not."),
        slider("Tap Window", "vr_bullettime_tap_window", 0.05f, 1.f, 0.05f, "%.2f s")
            .help("How soon after its fastest the hand must stop on the wrist."),
        toggle("Tap While Holding", "vr_bullettime_tap_holding")
            .help("On: the tapping hand may hold a weapon or a prop (its hand taps, never what it holds). Off: only "
                  "an empty hand taps."),
        toggle("Tap During Two-Handed Holds", "vr_bullettime_tap_twohanded")
            .help("Off: no taps while both hands hold one weapon (your gadget hand on its foregrip), against accidents. "
                  "Moving together, the hands never tap anyway."),
        header("Button"),
        toggle("Gadget Button", "vr_bullettime_button")
            .help("The inner button on the gadget's lower edge, pressed with your other hand's fingertip."),
        slider("Button Size", "vr_bullettime_button_radius", 0.5f, 8.f, 0.25f, "%.2f units")
            .help("How near your fingertip must come to the button's middle to press it (a unit is about 3.8 cm)."),
        slider("Fingertip Reach", "vr_bullettime_button_reach", -2.f, 6.f, 0.25f, "%.2f units")
            .help("Where your fingertip is taken to be, ahead of your hand's point."),
        header("Both"),
        slider("Ignore Repeats For", "vr_bullettime_trigger_cooldown", 0.f, 2.f, 0.1f, "%.1f s")
            .help("After a tap or press counts, how long before the next one does (a tap and a press together, or a "
                  "bounce, are one)."),
        slider("Haptic Tick", "vr_bullettime_haptic", 0.f, 2.f, 0.25f, "%.2f")
            .help("The tick in both hands when a tap or press counts (0: none)."),
    };
}
