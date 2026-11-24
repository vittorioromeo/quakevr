# Round 26: from 2026-11-24, towards 1.2

A per-feature reference: current behaviour only. Round 25 (2026-11-12 to 11-24, 12,400 lines) is history: `ROUND25.md`;
earlier: `ROUND24.md` to `ROUND22.md`. Grep them; never read one whole.

Open work and bugs: BACKLOG.md (section 1, headed "Open on 2026-11-24", names the ROUND25.md sections that hold the notes).

## Adding a section

One short section per feature, appended at the end (never rewrite others' sections; the file merges as `union`):
a heading "## <what it does> (branch agent/<name>, <date>)", then what it does, settings and defaults, files, tests with their
numbers, open issues. Fold a follow-up into its feature's section instead of adding a new one. Start ROUND27.md when
this file passes about 2,000 lines.

## menupaths

`vr_menu_dump_paths` built the Held Weapon pages (Weapon Offsets, Weights, the Offsets parts) with no weapon in the hand, so they were only a "hold a weapon" line and a full regeneration of menu_paths.txt dropped their rows (they were hand-added). The dump now pins weapon slot 16 (cvars `_17`) while it builds the index (`pinSlotPages`, vr_menu_paths.inc) and unpins after. A full regeneration now keeps every row. Diff vs before: ~66 real additions (Headset Taken Off, Centre Message Size, Spin Up/Motor Volume, flat spear/thrust tunables, eye rect/dither, debug rows), 1 rename (Wheel Hold / Quick Tap Time), 2 removed duplicates (Mark a Gap I Got Through). `vr_menu_path_check maps/vrcalibration.map`: 6 found, 0 missing; menu_shortcuts_test 14/0.
