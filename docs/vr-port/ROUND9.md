# Feedback round 9: plan and notes

| # | Request | Status |
|---|---|---|
| 1 | Bloom too strong by default; bloom and threshold sliders need finer steps | done |
| 2 | Sword handles off-centre, not in the model's style: bigger, lower-poly | done |
| 3 | Held boxes turn wrongly (hand yaw turns their pitch), wrong pivot, clip into the floor when nudged, bounce oddly, grab box bigger than the object | done |
| 4 | The shotgun is held by the middle | not reproduced: see below |
| 5 | Headbutt too hard to trigger | done |

## Notes

1. **Bloom:**
   - The default strength is 0.3 (was 0.8); the threshold default is 0.7 (was 0.6).
   - Configs still on the old defaults take the new ones once (`vr_cfg_version` 3). A value you set yourself is
     kept.
   - Slider steps: strength 0.02 (0 to 1.5), threshold 0.01, size 0.05, Light Contrast 0.02, Muzzle Flash and
     Explosion Light 0.05.
2. **Swords** (`Misc/quakevr/make_swords.py`):
   - The grip is centred on the blade: the middle of the blade's section above the hilt. It was on the model's
     principal axis, which the knight's lopsided guard pulls to one side.
   - The grip and pommel are thicker (grip radius 1.5, was 1.1) and square in section (4 sides, was 8), blocky
     like the knights' own models.
   - The hand anchors were found again (slot 18: 82, slot 19: 22), and the offsets were worked out so the hand
     holds the grip where it held it before.
   - Configs take the new sword settings once (`vr_wofs_version` 3).
3. **Boxes, gibs and thrown weapons:**
   - **Wrong turning.** The physics treated every model as the renderer turns alias models, but brush models
     (the ammo and health boxes) take pitch the other way. So a box's pitch and roll in the physics were the
     mirror of what was drawn. That is why bounces turned wrongly and a resting box sank a corner into the floor
     it seemed to lie on. The conversions now depend on the model's kind.
   - **Held objects.** A held box now keeps the turn it had in your hand when you gripped it, and turns exactly as
     the hand does (a new builtin, `carryangles`). Before, the box's angles were the hand's angles plus a fixed
     difference, which mixes the axes as soon as the hand is tilted: turning the hand's yaw turned the box's
     pitch.
   - **Pivot.** It turns about the point where you hold it.
   - **Tested:** turning the hand 45 degrees in yaw turns the box 45 degrees in yaw; pitching or rolling the hand
     turns the box about the hand's own axes.
   - **Grab box.** A hand takes hold of a carryable object (a box, gib, head or dropped weapon) only inside the
     object's drawn box, turned with it, plus 2 units. Thin things are treated as at least 6 units thick, so a
     dropped gun can still be found. Before, it was Quake's box, which never turns, plus the wide margin items
     get for walking over them. **Tested:** a hand 7 units above the shells box does not take it; a hand on it
     does.
   - **Settling.** When a thrown or nudged object comes to rest, a side within 8 degrees of the floor is laid
     flat on it, and the object is lifted out of the floor if an edge is still in it. **Tested:** a box nudged
     across e1m1's floor rests flat (it used to stop tilted 1.6 degrees, one edge in the floor).
4. **Shotgun:** I could not reproduce it.
   - In the mock, the shotgun (slot 2) is held at the rear grip, as with the build before this work.
   - Your config's values for that slot are the defaults, and its hand anchor resolves to a vertex at the rear.
   - A screenshot, or when it happens (after picking it up, a throw, two hands, the holster, a level start),
     would help.
5. **Headbutt:**
   - The threshold is 1 m/s (was 1.5; migrated once).
   - The lunge only needs to go roughly where you look.
   - Any monster within reach ahead counts, and one your head is already in is hit at once.
   - The mock backend now reports head velocity, so it could be tested: a headbutt gibbed the e1m1 grunt.
