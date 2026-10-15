# Blood feedback

VR Settings > Blood Feedback exposes two independent, archived effects. They use the local player's
STAT_HEALTH and the original svc_damage payload. They work in VR and flat-screen mode. They never change
damage, aim, legacy view kicks, hand knocks, or haptics.

| Setting | Default | Meaning / runtime range |
|---|---:|---|
| vr_blood_hit | 1 | Hit vignette on/off |
| vr_blood_hit_duration | 0.45 | Seconds to fade out, 0–5; 0 disables |
| vr_blood_hit_intensity | 0.22 | Maximum single-hit edge opacity, 0–0.8 |
| vr_blood_hit_size | 0.65 | Hit coverage, 0-1; 0 hides it, 1 widest |
| vr_blood_hit_bias | 0.75 | Directional preference, 0–1; 0 is even |
| vr_blood_health | 1 | Low-health vignette on/off |
| vr_blood_health_threshold | 35 | Health below which pulses appear, 1–200 |
| vr_blood_health_intensity | 0.25 | Peak opacity approaching zero health, 0–0.8 |
| vr_blood_health_size | 0.65 | Independent health coverage, 0-1; 0 hides it, 1 widest |
| vr_blood_health_duration | 0.25 | Individual beat width in seconds, 0.05–2 |
| vr_blood_health_rate | 0.75 | Frequency near threshold in Hz, 0.1–3 |
| vr_blood_health_rate_scale | 2 | Extra frequency as severity increases, 0–4 |
| vr_blood_health_curve | 1 | Severity exponent, 0.25–4 |

Severity = ((threshold − health) / threshold)^curve for living players below the threshold.
Peak opacity = intensity × severity. Frequency = rate × (1 + severity × rate_scale), bounded at 6 Hz.
Individual Gaussian beats have the configured full width at half maximum; periodic neighbours
overlap smoothly and the train is normalized to a peak of one. Increasing duration does not clip
the period or change frequency. Very wide beats overlap into a nearly steady tint.

A hit lasts the configured duration with a quadratic fade. Its strength counts both health and
armor damage, reaching maximum at 20 combined damage; smaller hits retain at least a quarter.
Eight overlapping hits retain separate world directions. Their lobes add within an opacity bound.
Hit and health alpha compose as 1 − (1 − hit) × (1 − health), limited to 0.8. The inner circle
stays untouched by these effects. Hit Coverage and Health Coverage independently control how far each
effect reaches inward, without changing peak intensity, direction, hit duration or pulse timing.
The wider defaults start at radius 0.4125 and reach full strength at 0.8475 in normalized screen
coordinates; previously the fixed radii were 0.6 and 1.162. At maximum coverage the inner clear
radius is still 0.15, with full strength from 0.55. At small positive sizes the effect retreats
to the outer edges/corners. Size zero hides only that effect; its existing toggle remains independent.
Values are clamped to 0-1; NaN is treated as zero.

The direction is stored in world space and projected using each rendered eye/view's current angles.
Left/right hits prefer that edge; front prefers the upper edge, rear the lower edge; above/below
prefer upper/lower. Near-player sources (within 24 units), missing/invalid sources and zero coordinates
are neutral. A listen server additionally recognizes world, self and owned-projectile sources
when its authoritative inflictor centre matches the protocol-decoded coordinates exactly.
Remote servers and old demos provide coordinates only: projectile ownership cannot be inferred,
so those hits retain the available direction. No packet format or existing feedback changes.

The existing scene resolve/warp shader applies blood before the existing colour shift; it therefore
respects full-black comfort/AD fades. The original damage colour shift remains enabled independently,
so turning off Screen Tints removes that tint while leaving blood feedback. Motion-comfort and
bullet-time effects retain their existing composition. Portal views do not render blood.
No extra GL blend, framebuffer or texture state is introduced, and the inactive path remains unchanged.

The effect clock follows client game time, once per host frame; pausing freezes it. Death, intermission,
disconnect, client state reset, map changes and save loading clear hit/pulse history. Respawn starts
fresh; loading reconstructs low-health feedback from authoritative health. Healing to the threshold
resets pulse phase. Rewinding demo time clears history.

Debug > Tools contains previews and vr_blood_info (edge alpha, health severity/peak/rate/pulse, hit
strength, clock/phase, pause state). vr_blood_test [packet] left|right|front|rear|up|down|neutral|armor
previews a hit; packet round-trips the real damage reader, also exercising the original tint/hand
feedback. vr_blood_test sample <health> <phase> evaluates production health equations without changing
player stats. clear clears preview history.

Local kit regression: python Misc/quakevr/test_blood.py --name <worktree> --images.
Evidence is written to the assigned worktree's ignored scratch directory. It checks protocol hit
directions, head turns, armor-only events, pause/expiry, timing/intensity controls, independent
toggles, threshold/healing, death/respawn, save/load, map/disconnect, scaling curves and visual
eye/flat composition. Real-headset review should confirm mild defaults, peripheral depth/comfort,
left/right versus front/rear cues, health pulsation, and overlapping movement-comfort/black fades.

Coverage GPU regression: run Misc/quakevr/test_blood_size.py with the bundled image Python and optional --tracy-smoke.
It measures actual left/right eye images and flat scene pixels for monotonic coverage, independent masks/toggles,
zero/out-of-range/invalid sizes and the minimum clear centre. Evidence lives in scratch/blood_size.
VR comparisons use captured eye images because uncapped mock runs can outpace the desktop mirror update rate.
