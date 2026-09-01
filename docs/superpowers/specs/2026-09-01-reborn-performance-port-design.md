# Reborn Performance Port Design

## Goal

Port only the measurable performance and hitch-reduction work from
`olcayseygan/CnCGeneralsZH-Reborn` into the Apple Silicon GeneralsX build.
Preserve gameplay, AI decisions, balance, saves, replays, and the existing
SDL3/DXVK/Vulkan/MoltenVK platform path.

## Non-goals

- No Reborn AI, difficulty, scouting, retreat, balance, UI, bloom, or shadow features.
- No Win32/x86, Direct3D 9/12, d3d8to9, SEH, DbgHelp, Miles, or Bink code.
- No duplicate ports of work already present in GeneralsX, including render/logic
  clock separation, radar batching, and row-major height-map traversal.
- No bulk cherry-picks from Reborn.

## Approach

Use a curated, benchmark-gated port. Each optimization is isolated, tested,
measured on the M1 Mac, and kept only when it improves the relevant tail latency
without changing deterministic game behavior. The installed working binary is
backed up before every deployment.

## Measurement

Establish a reproducible baseline before production changes:

- Same map, factions, seed, graphics settings, camera position, and match duration.
- Record average frame time plus p95, p99, and worst frame time.
- Record slow logic turns and pathfinder search work where instrumentation exists.
- Monitor CPU load and memory so improvements do not make other Mac work less responsive.
- Compare deterministic replay/save outcomes before and after each phase.

Instrumentation must be disabled or negligible in the normal release build.

## Phases

### Phase 1: Low-risk terrain work

1. Port Reborn `3a4e64b`: move invariant terrain light-ray calculations out of
   the per-vertex loop.
2. Port Reborn `db089e4`: rebuild only the terrain region that changed.
3. Adapt file locations to GeneralsX `Core/GameEngineDevice` and clamp dirty
   regions for the Mac renderer.

Acceptance: identical rendering, no new terrain corruption, improved terrain
update tail latency, and no CPU/RSS regression.

### Phase 2: Deterministic work spreading

1. Port only the scheduling portion of `1ab7b64`: at most one expensive AI
   building-placement calculation per logic frame.
2. Do not port AI decision or build-policy changes.

Acceptance: identical deterministic results for the same seed, no delayed or
missing construction, and lower worst-frame time in AI-heavy matches.

### Phase 3: Pathfinder optimization

1. Adapt `fc7ea02` hierarchical corridor fixes.
2. Surgically extract performance-only portions of `309ce2f` that prevent
   multiple full-map searches on one frame.
3. Adapt `261b85f` bucket-indexed A* open list only after verifying compatibility
   with GeneralsX's existing reverse-insertion optimization.

Acceptance: no stuck units or route regressions, deterministic replay behavior,
bounded search work, and a measurable reduction in p99/worst logic-frame time.

## Testing and rollback

- Add a failing regression or source/behavior contract before every ported change.
- Build and test the ARM64 target after every isolated patch.
- Run representative infantry, vehicle, bridge, narrow-route, large-group, and
  uneven-terrain scenarios.
- Deploy one phase at a time and retain a named backup binary for instant rollback.
- Stop a phase if performance does not improve or compatibility cannot be proved.

## Success criteria

- No visible gameplay, AI, balance, or graphics change.
- Existing saves load and normal replays remain deterministic.
- No new crashes or rendering artifacts.
- Lower p99 and worst-frame time on the M1 Mac; average FPS alone is not sufficient.
- Normal play does not materially increase CPU or memory pressure on other work.
