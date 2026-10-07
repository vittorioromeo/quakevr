# Official expansion QuakeC source provenance

Upstream: [id Software's Quake rerelease QuakeC](https://github.com/id-Software/quake-rerelease-qc).
Snapshot: `634eefab09a77eb7b5f5ca7078ba3d8784a91142`, downloaded with the author's approval on 2026-10-05.

The upstream repository declares GPLv2. Its original license text is retained unchanged in
[QC/COPYING-rerelease.txt](../../QC/COPYING-rerelease.txt). Retain the original copyright and license headers
in adapted files; individual headers permit GPLv2 or later. The manifests credit MachineGames 2021/2026.
Native port changes are recorded in the task commits and expansion implementation notes.

This source license covers the QuakeC code. Commercial PAKs, maps, models, sounds and localization tables
come from the player's owned Quake installation and are not included in the VR distribution.

Implementation status and validation are tracked in [EXPANSIONS.md](EXPANSIONS.md), with player setup in
[INSTALL.md](../INSTALL.md).

The native Machine Horde implementation in QC/vr_mg_horde.qc adapts the approved
quakec_mg1/horde.qc source, retaining its original license header. Narrow Hunger,
healing, death, key spending, item and intermission hooks adapt the same snapshot's
MG1 game code to the existing native VR inventory and monster constructors. Tests
and the contemporary seven-arena rotation are VR-port additions documented in
EXPANSIONS.md.

Dawn of the Machine (MG3) native port: QC/vr_mg3_upgrades.qc adapts the snapshot's quakec_mg3/mg3_upgrades.qc
(capacity upgrades), retaining its original license header; the capacity constants in QC/vr_mg3_defs.qc come
from quakec_mg3/defs.qc and client.qc. Upstream's parm10..15 state moved to parm51..56 (VR's hands own parm10..16).
QC/vr_mg3_triggers.qc adapts the map triggers of quakec_mg3/mg3_triggers.qc and triggers.qc (same header).
QC/vr_mg3_items.qc adapts quakec_mg3/mg3_items.qc and the lava suit of items.qc/client.qc (same header).
Plan and task list: [MG3_PLAN.md](MG3_PLAN.md).
