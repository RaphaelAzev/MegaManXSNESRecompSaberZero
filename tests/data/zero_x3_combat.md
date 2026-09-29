# Original X3 Zero combat reference

`zero_x3_combat.csv` contains measured frame results, not ROM bytes or graphics.
Recorded using the original USA X3 ROM, SHA-256
`65b03268afac296330e8ff8d60dd0825879e13ed658b37713c034a3bd074f1d7`,
through the X3 project's interpreter with execution mode off. Zero was selected
through the original game's character swap. The private fixture is on flat
ground at the beginning of Neon Tiger.

Each case restores the same stationary fixture, advances two neutral frames,
and holds Y for 205 frames. The air case additionally holds B+Y for five frames.
Then: release Y for 25 frames, press Y for one frame, release for 60 frames,
press Y for one frame, release for 50 frames. B stays held through the airborne
first and second busters. Each row is the state after that input frame.

Pose indexes are group $4A indexes, or 117 plus the group $4B saber index.
DY is relative to the starting position in 1/256 pixels. VY is signed 8.8.
The fixture's fractional Y is $ED; a landing snaps it to zero. `emitted` marks
an increase in the original charged-projectile count during a buster action.

Additional charge measurements give tiers at held frames 21, 81, 141, and 201:
half charge, full single buster, two busters, and two busters plus saber.
The first grounded shot emits on frame 8 and returns to idle on frame 18.
The second emits on frame 10 and returns to idle on frame 29. Airborne firing
uses separate original animation records and resumes the equivalent phase on
landing. Saber action returns on frame 44 and shows idle on frame 45.

The backport reads the original body-animation records from the extracted
asset cache. It retains X1's charged projectile and damage/collision machinery.
Saber readiness follows X3's dependency rule: preceding charged attacks must
finish. X1 includes its impact/disappearance animation in the projectile slot
($81:A3CE..A40D), whereas X3 also counts separate effect actors ($0A82).
Consequently, beam travel/effect lifetimes are X1's, not a claimed frame-exact
port of X3's projectile engine. There is no fixed inter-attack cooldown.

The ROM-backed test compares these stationary ground and air sequences with
X1's Highway fixture. The existing movement reference covers ordinary motion;
these checks do not claim exhaustive terrain, enemy, or campaign coverage.
