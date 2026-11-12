# Round 25: from 2026-11-12, towards 1.1

A per-feature reference: current behaviour only. Earlier rounds: `ROUND24.md` (2026-11-05 to 11-12: 9,600 lines),
`ROUND23.md` (beta4 to 11-05), `ROUND22.md` (1.0.1 to beta4). Grep them; never read one whole.

## Where things stand

- Released: 1.1.0-beta4. Everything since is on `next`; 1.1.0 is cut only on his word (RELEASE_TODO.md).
- Round 24 brought: flat-screen play (poses per weapon, ADS, slide, lean, prone, holster menu, hand swap, flat melee
  and parry), armour on armed humanoids and the player (the Armour Studio), embedded blades on by default, the navmesh
  for walking monsters (drops, lift links off, props as obstacles, the arena's towers), the arena's budget director and
  weapon ladder, take and clip recording, ammo screen housings, the build checks in C++ (`Tools/qvr_checks`), the
  header split, the no-LTO quick build, and the cvar retirement (CVAR_AUDIT.md).
- Open work and bugs: BACKLOG.md (section 1 is the open bugs and failing tests).

## Carried over from round 24

- `vr_agrunt_test 210` fails; `vr_flatswap_test 4` (AD blades through the hand swap) to pass with AD mounted.
- The stance's torso twist and the free hand's punch (32 against 21 damage): his call; flatstance's hotspot check too.
- The triage's random cases (flatpose 21, `vr_systems_test 15`, flatswing's knight trials); visor_test's AD keys.
- Navmesh next steps: lifts finished, flyers and swimmers, flanking and cover, the tower dog fall (`vr_arena_test 41`).
- Arena geometry that changes between waves; flat keys bindable from the menu; Slipseer in the map library.

## Adding a section

One short section per feature, appended at the end (never rewrite others' sections; the file merges as `union`):
a heading "## <what it does> (branch agent/<name>, <date>)", then what it does, settings and defaults, files, tests with their
numbers, open issues. Fold a follow-up into its feature's section instead of adding a new one. Start ROUND26.md when
this file passes about 2,000 lines.

## Round start: BACKLOG and the agent prompts tidied (branch agent/round25, 2026-11-12)

BACKLOG.md rewritten from 369 to about 135 lines: done items out (design review P3/P6/P8/P9/P10, menus' per-slot
data, the embedded blades' "turn on"), open bugs first, one or two lines an item; the long entries moved into their
topic docs' "Open items moved from BACKLOG.md" sections (ARMED_MONSTERS, ARCANE_DIMENSIONS, FLINCH_RETROSPECTIVE,
MULTIPLAYER, PERF_DECISIONS, STEALTH). `.gitattributes` merges ROUND25.md as `union`; README.md points here.

## TESTING.md split into the guide and an archive of recipes (branch agent/testsplit, 2026-11-12)

TESTING.md (1,725 lines) is now the guide (about 320 lines: the mock headset, Writing a test, Test numbers, Checklist scenarios, Debug build assertions, Batched test cases, and an index). The per-feature recipes moved word for word to archive/TESTING_RECIPES.md (82 `##` sections, grep-only); a feature's scattered paragraphs are merged into its section. Retired or vanished cvars named in recipes are marked inline "(retired cvar: <name>)" (9). Code and script comments that quote a moved section ("Climbing", "Swimming", "OpenXR runtime choice", "Body calibration", ...) still say TESTING.md: the index at its end names the archive section.
