
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
        slider("Field of View", "vr_spectator_fov", 60.f, 120.f, 1.f, "%.0f deg").extend(40.f, 130.f)
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
                  "they're right again). Off: at their normal speed and pitch. Music is never slowed."),
        slider("Hand Speed Limit", "vr_timescale_hand_speed", 0.f, 20.f, 0.5f, "%.1f m/s")
            .help("In slow motion, your hands follow your controllers as fast as the slowed world allows: at most this "
                  "fast in the game's time (a quarter of it in real time at 0.25x). A slow move is followed exactly; "
                  "a real-speed swing lands as a hard blow, not a supersonic one. 0: no limit (the hands keep up)."),
        slider("Hand Turn Limit", "vr_timescale_hand_spin", 0.f, 40.f, 1.f, "%.0f rad/s")
            .help("Likewise, the most your hands turn per second of the game's time in slow motion. 0: no limit."),
    };
}
