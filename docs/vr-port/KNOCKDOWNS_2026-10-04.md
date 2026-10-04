# Quake VR: shove knockdowns (live ragdolls that get up)

Date: 2026-10-04. A shove (an open hand, or both) can knock a monster down as a ragdoll while it is still alive. It
lies there, can be hit, grabbed, dragged and thrown like a dead one, and after a few seconds it gets up near where it
lies and fights on. Menu: Combat > Knockdowns (`vr_knockdown`, default on). Only monsters with a ragdoll rig, with
Ragdolls on.

Code:

- QC: `QC/vr_knockdown.qc` (the rules), with hooks in `combat.qc` (`Killed`, `T_DamageImpl`, `VR_Push`),
  `vr_melee.qc` (`VR_Bash_Hit`), and each monster's spawn function (its chance and get-up).
- Engine: `vr_box3d.cpp` "Knockdowns" (`ragdollKnockdown`, `ragdollGetUp`, the recovery blend). Builtins:
  `canragdoll`, `ragdollknockdown`, `ragdollgetup`.

## The chance

A shove that lands (`VR_Bash_Hit`) rolls `VR_Knockdown_Try`. The chance is the product of:

| Factor | Cvar | Default |
|---|---|---|
| The monster's base chance | `vr_knockdown_chance_<monster>` | grunt .45, rottweiler .35, zombie .35, enforcer .3, knight .25, gremlin .25, mummy .2, ogre .15, death knight .12, fiend .1, shambler .03 |
| Everything | `vr_knockdown_chance` | 1 |
| Damage taken: 1 + this x the share of its health lost | `vr_knockdown_damage` | 2 (three times as likely at no health left) |
| One hand / both hands | `vr_knockdown_onehand`, `_twohand` | 1, 1.75 |
| A weapon's bash (on top) | `vr_knockdown_bash` | 0 (only open-hand shoves) |
| Stamina: 1 - this x the share spent (stamina pool on) | `vr_knockdown_stamina` | 1 |

`vr_knockdown_debug 1` (with `developer 1`) prints every roll and every get-up.

## Down

- Not solid, touchable (`SOLID_NOT_BUT_TOUCHABLE`), `.vr_knockdown` 1. The engine makes its ragdoll at once from the
  frame it is drawn in, keeping its motion; the shove's push is scaled by `vr_knockdown_push`. Its weapon is welded to
  its hand (`vr_knockdown_weld`).
- Down knockdowns don't count against `vr_ragdoll_max` in the engine. QC makes room first (`VR_Knockdown_MakeRoom`):
  the oldest dead ragdoll is removed; if none is left, the oldest knocked-down monster is killed and its body removed.
- Hits land on the ragdoll (the precise hits test the limbs as drawn). It reacts as usual: it gets angry, plays its
  pain sound and bleeds. Its pain frames don't play (`T_DamageImpl` keeps its frame and think). Each hit keeps it down
  `vr_knockdown_hit_time` longer, never past `vr_knockdown_time_max` from that hit.
- Grabbing, pushing and throwing work as on a dead one's ragdoll.

## Killed while down

- `Killed` runs its death code (`th_die`: the scream, the drops) and then plays its death frames' functions at once
  (`VR_Knockdown_FastForwardDeath`). The ragdoll is not remade: it is the same body, now a dead one's. Its welded
  weapon's part is removed, as QC drops the weapon as an item.
- Gibbed: it gibs as usual.
- Beheaded (a blade) or head popped (a shotgun or lightning headshot): the existing ragdoll loses its head
  (`ragdollDecap` on a live ragdoll) and stays where it lies.

## Getting up

After `vr_knockdown_time_min` to `_max` seconds (at random), `VR_Knockdown_Think` asks the engine
(`ragdollGetUp`) to get up:

1. **The pose.** The get-up's first frame is fitted to how the body lies: a 2D Procrustes fit of the frame's bone pivots
   onto the ragdoll's, plus the pelvis's turn. That gives the yaw and the place. With two candidate frames (two deaths
   played backwards), the closer fit wins.
2. **Room to stand.** The search starts where the fitted frame stands, then tries rings 8 units apart out to
   `vr_knockdown_search` (default 48), with 8 to 32 directions each. A spot counts only if:
   - its line from the pelvis is clear, so the search never crosses a wall, floor or ceiling;
   - its floor is within 24 units of the floor under the body;
   - the standing box is clear (`SV_Move`, `MOVE_NORMAL`).

   With no room, it stays down and tries again every `vr_knockdown_retry` seconds. This covers a body dragged into a
   tight corner or under something low.
3. **Hands let go.** The ragdoll's parts are destroyed, which drops every grab on it. The hands' QC frame sees
   `ragdollheld` 0 and lets go.
4. **The blend.** For `vr_knockdown_blend` seconds the drawn body blends from the ragdoll's last pose into the
   animation: rotations slerped, pivots lerped, frames lerped. There is no frame of the animated model in between.
5. **The animation.** Monsters with their own fall-and-rise play it from its lowest frame, at
   `vr_knockdown_getup_speed`:

   | Monster | Frames |
   |---|---|
   | grunt | painb7 on |
   | enforcer | paind7 on |
   | ogre | paind5 on |
   | zombie, mummy | paine12 on |
   | rottweiler | painb9 on |
   | knight | painb9 on |

   Monsters without one play a death backwards:

   | Monster | Deaths |
   |---|---|
   | death knight | death or deathb, whichever fits |
   | fiend | death |
   | shambler | death |
   | gremlin | death |

   Then it runs. Pain during the get-up plays its sound and the get-up goes on.

## Saved games

State 1 (down) remakes its ragdoll on load (the engine's `wantsRagdoll` takes knocked-down monsters). Without a
ragdoll (Ragdolls turned off meanwhile), it gets up where it is when it fits there.

## Tests (headless, `vr_knockdown_test <mode>`)

`vr_knockdown_test` runs QC's `VR_Knockdown_Test` as the first player. The modes:

| Mode | Does |
|---|---|
| 0 | Knocks the nearest monster down, regardless of the chance |
| 1 | Gets every knocked-down monster up now |
| 2 | Hits the nearest knocked-down monster for 5 |
| 3 | Kills it |
| 4 | Gibs it |
| 5 | Lists the monsters |

Results on the firing range, with grunts spawned by `vr_physics_spawn`:

| Case | Result |
|---|---|
| Knockdown | The ragdoll is made at once and the shove's push throws it (eyeshots) |
| A hit while down | Health 30 to 25; it stays a ragdoll; `vr_kd_until` is extended |
| Get-up | Fit to painb7, 0 units from where it lay. The body blends from lying, through kneeling, to standing over about 0.4 s, then runs (eyeshots every 50 ms) |
| Killed while down | The same ragdoll lies on (its weapon part removed, 12 to 11 parts); its death code ran to its last frame (frame 28); it is watched as a corpse |
| Gibbed while down | Gibbed |
| A sword's slash at the head while down (`vr_decap_test 1`) | Beheaded; the ragdoll stays, headless (10 parts) |
| A shotgun headshot while down (`vr_decap_test 12`) | Head popped; the ragdoll stays, headless |
| `vr_ragdoll_max 2`: a dead ragdoll and one down, then another knockdown | The dead one's body is removed |
| The same with two down and no dead ones | The oldest one down is killed and removed |
| No room (`vr_knockdown_search 0`, a monster standing where it would stand) | It stays down and retries; with the search at 64 it gets up beside the monster |
