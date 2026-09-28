# Changelog

All notable changes to **SPIKE ELITE** (working title: a 3D indoor volleyball game built on UE5).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
for milestone tags once a first playable is tagged.

## [Unreleased]

### Milestone M3 — FIVB rotation, serving player, stadium lighting
_commit 8ec4531 → next_

#### Added
- **FIVB §7.4 positional rotation on side-out.** When the team that did NOT serve wins the rally,
  its roster rotates clockwise: position 2 → 1, 3 → 2, …, 1 → 6. The human player is teleported
  to their new home position; bots update their `HomePosition` and walk there.
- **Roster tracking.** GameMode keeps `TeamAPlayers[6]` / `TeamBPlayers[6]` in position order
  (index 0 = position 1 = the server).
- **Server-based serve.** `ServeNextBall()` now places the ball at the current position-1
  player's location and strikes it across the net, instead of spawning it at a fixed point.
- **New-set reset.** `RespawnPlayersToPositions()` teleports both rosters back to the starting
  1–6 layout when a new set begins.
- **Directional stadium light** (rotated -55° pitch, intensity 4.5) so the court and Mannequins
  are readable in the default OpenWorld map.

#### Fixed
- Removed leftover SkyLight call that failed to compile (`USkyLightComponent` not forward-included).
- Scoreboard no longer uses `%s` with a ternary (UE5.8 `UE_LOG` static_assert trap).

---

### Milestone M2 — 6v6 AI characters
_commit 93d2e8f → 8ec4531_

#### Added
- 12 real `ACharacter` actors on the court: 1 human (Team A, position 1) + 5 Team A bots +
  6 Team B bots. Mannequin skeleton mesh (`SK_Mannequin`) from the UE5 template resources.
- `ASpikeEliteCharacter::TickBot` simple AI:
  - if the ball is on the bot's side, between 1.2 m and 4.5 m high, and within 2.5 m,
    `Strike()` it toward a random spot on the opponent's back court (9–11 m/s);
  - otherwise walk toward the ball if it is dropping on the bot's side, else walk back to
    `HomePosition`.
- Jersey colours via `UMaterialInstanceDynamic`: Team A = blue, Team B = red.
- Bots are boundary-clamped so they never cross the net or run out of bounds.

#### Removed
- The old "placeholder cube" position markers (replaced by real characters).
- The old GameMode-side fake AI that teleported the ball across the net every 1.2 s —
  the ball is now physically hit by the characters themselves.

#### Engineering notes
- UE5.8 `UE_LOG(…, TEXT("… %s …"), cond ? TEXT("A") : TEXT("B"))` triggers a
  `static_assert "Formatting string must be a TCHAR array"` because the format-string
  checker does not see through the ternary. Avoid conditional strings in `UE_LOG` args;
  either precompute a `const TCHAR*` into a variable or drop the `%s`.

---

### Milestone M1 — Playable single-rally prototype
_earliest commits_

#### Added
- C++ project skeleton, `WASD` movement + mouse look, `Space` jump, `V` toggle between
  first-person and third-person spring-arm camera.
- Procedural court: 18 m × 9 m wooden floor (Poly Haven `wood_floor` CC0 diffuse+normal),
  7 FIVB white lines (end / sideline / center / attack), transparent cloth net at 2.43 m,
  two net posts, 5 m perimeter walls.
- Physics-driven orange `AVolleyballBall` (sphere, Chaos physics, `OnComponentHit` →
  floor contact calls `GameMode::OnBallLanded`).
- Ball-net collision bounce.
- FIVB rally-point rules: 25 / win-by-2 / best-of-5 / decider to 15, side-out serves.
- Debug on-screen scoreboard (set, score, sets won, server) and a first-person ball-direction
  arrow (`FRONT` / `LEFT` / `RIGHT` / `BEHIND` / `HERE!`).
- Player hit: `LMB` (8.5 m/s on ground, 12 m/s while jumping), `E` serve.
- Stands: three rows × 17 coloured cubes as a placeholder crowd.

---

## Build & run

```powershell
# Compile (bypasses the UBA recursion bug in Build.bat on this machine)
& "D:\Epic\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" `
  "D:\Epic\UE_5.8\Engine\Binaries\DotNet\UnrealBuildTool\UnrealBuildTool.dll" `
  SpikeEliteEditor Win64 Development -Project="D:\projects\spike-elite\SpikeElite.uproject"

# Play
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe `
  "D:\projects\spike-elite\SpikeElite.uproject" /Engine/Maps/Templates/OpenWorld `
  -game -windowed -ResX=1280 -ResY=720
```

Controls: `WASD` move, mouse look, `Space` jump, `V` toggle first/third person,
`LMB` hit, `E` serve.
