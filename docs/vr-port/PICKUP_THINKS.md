# Pickup thinks: making idle pickups cheaper (research, 2026-10-03)

Research only, nothing built. From the performance review of 2026-10-03 (item 12, "Dormant pickups still execute
frequent thinks"). To review later (`BACKLOG.md`, low priority).

## Why

In the 2026-09-30 e2m1 capture (`ROUND21.md`, "Where the frame goes") QuakeC is 0.065 ms a frame, and
`forcegrabbable_think_impl` is 17% of its instructions: the largest single share, but a small absolute cost. The other
big share, `anglemod` (7%), is already gone: it is now a constant-time engine builtin (`AngleMod360`, `mathlib.c`;
the QC loop took a step per 360 degrees, so `anglemod(100 * time)` grew with level time). What is left is the number
of thinks, not the work in each.

## What happens now

Pickups think through `forcegrabbable_item_think` (`QC/items.qc`, set at `items.qc:640` and `:905`) and thrown
weapons through `forcegrabbable_think` (`QC/weapons.qc`). Each reschedules itself at `time + 0.02`. A think only runs
on a server frame: with renderer/network isolation (`host.c` `Max_Fps_f`: `host_maxfps` 0 or over 72) the server runs
at about 72 Hz, 13.9 ms a frame, so 0.02 s means every second server frame: about every 28 ms, ~36 thinks a second
per pickup. Every think, even for a pickup hanging untouched:

| Step | Time-sensitive? |
|---|---|
| Carried (`carry_player`): return at once | No |
| `forcegrabbable_item_reposition`: moved more than a unit from its place, start a return timer (`rad_time`); expired, put it back | Yes: a deadline, and noticing it moved |
| A floating wearable back at its place hangs again; moved by physics, it becomes `vr_rigid` | Yes: noticing a change |
| Hanging: `angles_y = anglemod(100 * time)` | Partly: the drawn spin is the client's (`cl_main.c` `bobjrotate`), physics uses `spinYaw()` (`vr_box3d.cpp`); this only feeds QC's own reads of `.angles` |
| A weapon's sparkle every 0.25 s | A deadline |
| `forcegrabbable_think_impl` (`weapons.qc:5212`): water entry/exit counters, slowing in water, a dust puff with chance 0.2 per think on the ground or in water, settling after a throw | Only wet, on the ground, or just thrown: not the hanging idle state |

`VR_PickupObj_Handtouch` (`items.qc`) already sets the spin angle when a hand touches the pickup.

## Options, cheapest first

1. **Idle thinks less often, with exact deadlines.** Idle: not carried, `MOVETYPE_NONE`, not rigid,
   `origin == oldorigin`, dry, no return pending (`rad_time == -999`). Idle, think every 0.25 s (the sparkle's
   interval) instead of every 0.02 s, but schedule deadlines exactly:
   `nextthink = min(time + 0.25, rad_time, vr_pickup_sparkle)`. Every QC event that changes the state sets
   `nextthink = time`: hand touch, grab, carry start and end, damage, a knock that changes the movetype, a deathmatch
   respawn. About 5-12 times fewer thinks for idle pickups.
2. **No thinks while carried.** A carried pickup's think does nothing but reschedule. Stop it when carried
   (`nextthink = 0`) and resume it where `vr_carry.qc` clears `carry_player` (lines 87, 118, 809) with
   `nextthink = time`. No behaviour changes.
3. **The spin left to the engine.** Drop `angles_y = anglemod(100 * time)` from the think and compute it where
   `.angles` is read (the hand touch already does; the engine side has `spinYaw()`, as physics uses). This removes the
   only every-frame work of a hanging pickup, which is what makes option 1 safe.
4. **Staggered phases.** A random first delay (`nextthink = time + random() * interval`) so pickups don't all think on
   the same frame. Same average cost, smoother frames on pickup-heavy maps.
5. **An engine-side idle flag** (largest). The engine skips the QC think of flagged pickups and wakes them on a touch,
   a physics wake or a movetype change. Only if options 1-3 still show in profiles.

Options 1+2+3 together: about an hour, removing nearly all of an idle pickup's think cost.

## Is any thinking resolution lost? (option 1)

| What changes the state | Resolution with option 1 |
|---|---|
| Deadlines (return timer, sparkle) | None lost: scheduled at the deadline itself. Still on a server frame, as now (and possibly a frame earlier than today's every-other-frame grid) |
| Events QC sees (hand touch, grab, carry, damage, deathmatch respawn) | None lost: the event sets `nextthink = time`, so the next server frame runs the full think, as now |
| Changes the engine makes without telling QC (Box3D knocking the pickup loose or pushing it) | **Lost unless the engine wakes it**: "moved, start the return timer" or "now a rigid body" could be noticed up to the idle interval late, so a knocked pickup returns up to 0.25 s later. Fix: the engine sets `nextthink = time` when it moves a sleeping fixture; or a shorter idle interval (0.1 s: at most 0.1 s later) |
| Per-think random effects (the 0.2 dust puff) | Their rate would follow the think rate. They don't apply while hanging idle, so the idle state excludes them; if one ever does apply, make it time-based (a chance per second, not per think) |

With the engine's wake on physics changes: the same gameplay at about a ninth of the idle think cost. Without it: the
only difference is up to 0.1-0.25 s later return-to-place after a physics knock.

## To check, before and after

- Return to place (`vr_forcegrabbable_return_time_*`): knocked by a hand, by an explosion, by a thrown thing.
- Deathmatch respawn.
- A pickup knocked loose turns rigid at once; dropped into water it floats as now.
- Hand reach and grab angle on a spinning pickup (taken mid-spin starts from the drawn turn).
- Weapon sparkle rate; dust puffs of dropped weapons.
- Measure: thinks per server frame and QuakeC ms (`vr_profile_detail 2`, `profile 30`) on a pickup-heavy map.
