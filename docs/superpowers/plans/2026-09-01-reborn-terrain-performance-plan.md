# Reborn Terrain Performance Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove terrain-deformation frame spikes on the M1 Mac without changing graphics or gameplay.

**Architecture:** Port two renderer-independent Reborn optimizations into GeneralsX's `Core/GameEngineDevice`: compute invariant terrain light rays once per vertex-buffer update, and aggregate lowered terrain cells into one bounded dirty rectangle flushed once per render update. Keep the existing full-update path for global lighting changes and use the existing `doPartialUpdate` path for the dirty rectangle.

**Tech Stack:** C++17, GeneralsX W3D renderer, SDL3/DXVK/Vulkan/MoltenVK, Python `unittest` source-contract tests, CMake/Ninja ARM64 build.

---

## File structure

- `Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp` — terrain vertex lighting and existing partial-update implementation.
- `Core/GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h` — dirty-region API and state.
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp` — dirty-region aggregation, clipping, and consume-once behavior.
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainVisual.cpp` — records lowered foundation cells instead of requesting a full relight.
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp` — flushes one dirty rectangle per render update.
- `scripts/qa/source/test_terrain_performance_contracts.py` — focused regression contracts for both optimizations and their wiring.

This plan covers only Phase 1 of the design. AI work spreading and pathfinding
are independent, higher-risk subsystems and receive separate plans only after
this phase passes its Mac runtime gate.

### Task 1: Hoist invariant terrain light rays

**Files:**
- Create: `scripts/qa/source/test_terrain_performance_contracts.py`
- Modify: `Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp:305-390`

- [ ] **Step 1: Write the failing light-ray placement test**

```python
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HEIGHT_MAP = ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unterminated function: {signature}")


class TerrainPerformanceContractsTest(unittest.TestCase):
    def test_global_light_rays_are_built_before_vertex_loops(self):
        source = HEIGHT_MAP.read_text(encoding="utf-8")
        body = function_body(source, "Int HeightMapRenderObjClass::updateVB(")
        light_loop = body.index("for (Int lightIndex=0;")
        row_loop = re.search(r"for\s*\(j\s*=\s*y0;", body)

        self.assertIsNotNone(row_loop)
        self.assertLess(light_loop, row_loop.start())
        self.assertEqual(1, body.count("m_terrainLightPos[lightIndex]"))
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
python3 scripts/qa/source/test_terrain_performance_contracts.py -v
```

Expected: FAIL because the light loop is still inside the per-cell `i/j` loops.

- [ ] **Step 3: Move the light-ray setup before the vertex loops**

In `HeightMapRenderObjClass::updateVB`, remove `const Coord3D *lightPos;`, add this block after the debug bounds assertion and before locking the vertex buffer, then remove the identical block from inside the `i` loop:

```cpp
for (Int lightIndex = 0; lightIndex < TheGlobalData->m_numGlobalLights; ++lightIndex)
{
	const Coord3D *lightPos = &TheGlobalData->m_terrainLightPos[lightIndex];
	lightRay[lightIndex].Set(-lightPos->x, -lightPos->y, -lightPos->z);
}
```

- [ ] **Step 4: Run the focused test and build**

```bash
python3 scripts/qa/source/test_terrain_performance_contracts.py -v
source "$HOME/VulkanSDK/1.4.357.0/setup-env.sh"
cmake --build build/macos-vulkan --target z_generals -j4
```

Expected: the focused test passes and the ARM64 target exits 0.

- [ ] **Step 5: Commit the isolated optimization**

```bash
git add scripts/qa/source/test_terrain_performance_contracts.py \
  Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp
git commit -m "perf: hoist invariant terrain light rays"
```

### Task 2: Add a bounded dirty-region accumulator

**Files:**
- Modify: `scripts/qa/source/test_terrain_performance_contracts.py`
- Modify: `Core/GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h:120-130,265-275`
- Modify: `Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp:239-250,2338-2360`

- [ ] **Step 1: Add failing contracts for aggregation, clipping, and consume-once behavior**

Append to `TerrainPerformanceContractsTest`:

```python
    def test_dirty_region_is_padded_clamped_and_consumed_once(self):
        header = (
            ROOT / "Core/GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h"
        ).read_text(encoding="utf-8")
        source = (
            ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp"
        ).read_text(encoding="utf-8")
        changed = function_body(
            source, "void BaseHeightMapRenderObjClass::terrainHeightChanged("
        )
        take = function_body(
            source, "Bool BaseHeightMapRenderObjClass::takeDirtyRegion("
        )

        self.assertIn("void terrainHeightChanged(Int x, Int y);", header)
        self.assertIn("Bool takeDirtyRegion(IRegion2D &region);", header)
        self.assertIn("const Int PAD = 2;", changed)
        self.assertIn("m_map->getXExtent() - 1", changed)
        self.assertIn("m_map->getYExtent() - 1", changed)
        self.assertIn("m_hasDirtyRegion = false;", take)
```

- [ ] **Step 2: Run the test and verify RED**

```bash
python3 scripts/qa/source/test_terrain_performance_contracts.py -v
```

Expected: ERROR/FAIL because `terrainHeightChanged` and `takeDirtyRegion` do not exist.

- [ ] **Step 3: Declare and initialize the accumulator**

Add to the public API after `staticLightingChanged()`:

```cpp
void terrainHeightChanged(Int x, Int y);
Bool takeDirtyRegion(IRegion2D &region);
```

Add beside `m_needFullUpdate`:

```cpp
IRegion2D m_dirtyRegion;
Bool m_hasDirtyRegion;
```

Initialize in the constructor:

```cpp
m_hasDirtyRegion = false;
m_dirtyRegion.lo.x = m_dirtyRegion.lo.y = 0;
m_dirtyRegion.hi.x = m_dirtyRegion.hi.y = 0;
```

- [ ] **Step 4: Implement padded aggregation and map clipping**

Add after `staticLightingChanged()`:

```cpp
void BaseHeightMapRenderObjClass::terrainHeightChanged(Int x, Int y)
{
	const Int PAD = 2;
	if (!m_hasDirtyRegion)
	{
		m_dirtyRegion.lo.x = x - PAD;
		m_dirtyRegion.lo.y = y - PAD;
		m_dirtyRegion.hi.x = x + PAD;
		m_dirtyRegion.hi.y = y + PAD;
		m_hasDirtyRegion = true;
	}
	else
	{
		m_dirtyRegion.lo.x = min(m_dirtyRegion.lo.x, x - PAD);
		m_dirtyRegion.lo.y = min(m_dirtyRegion.lo.y, y - PAD);
		m_dirtyRegion.hi.x = max(m_dirtyRegion.hi.x, x + PAD);
		m_dirtyRegion.hi.y = max(m_dirtyRegion.hi.y, y + PAD);
	}

	if (m_map)
	{
		const Int maxX = m_map->getXExtent() - 1;
		const Int maxY = m_map->getYExtent() - 1;
		m_dirtyRegion.lo.x = max(0, m_dirtyRegion.lo.x);
		m_dirtyRegion.lo.y = max(0, m_dirtyRegion.lo.y);
		m_dirtyRegion.hi.x = min(maxX, m_dirtyRegion.hi.x);
		m_dirtyRegion.hi.y = min(maxY, m_dirtyRegion.hi.y);
		if (m_dirtyRegion.hi.x < m_dirtyRegion.lo.x || m_dirtyRegion.hi.y < m_dirtyRegion.lo.y)
			m_hasDirtyRegion = false;
	}

	m_scorchesInBuffer = 0;
	m_curNumScorchVertices = 0;
	m_curNumScorchIndices = 0;
	if (m_roadBuffer)
		m_roadBuffer->updateLighting();
}

Bool BaseHeightMapRenderObjClass::takeDirtyRegion(IRegion2D &region)
{
	if (!m_hasDirtyRegion)
		return false;
	region = m_dirtyRegion;
	m_hasDirtyRegion = false;
	return true;
}
```

- [ ] **Step 5: Run focused tests and build**

```bash
python3 scripts/qa/source/test_terrain_performance_contracts.py -v
source "$HOME/VulkanSDK/1.4.357.0/setup-env.sh"
cmake --build build/macos-vulkan --target z_generals -j4
```

Expected: all focused tests pass and the ARM64 target exits 0.

- [ ] **Step 6: Commit the accumulator**

```bash
git add scripts/qa/source/test_terrain_performance_contracts.py \
  Core/GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h \
  Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp
git commit -m "perf: accumulate bounded terrain dirty regions"
```

### Task 3: Record foundation terrain changes without full relighting

**Files:**
- Modify: `scripts/qa/source/test_terrain_performance_contracts.py`
- Modify: `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainVisual.cpp:886-910`

- [ ] **Step 1: Add the failing producer-wiring test**

Append:

```python
    def test_foundation_lowering_records_a_partial_terrain_update(self):
        source = (
            ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainVisual.cpp"
        ).read_text(encoding="utf-8")
        body = function_body(source, "void W3DTerrainVisual::setRawMapHeight(")

        self.assertIn("terrainHeightChanged(x, y);", body)
        self.assertNotIn("staticLightingChanged()", body)
```

- [ ] **Step 2: Run the test and verify RED**

```bash
python3 scripts/qa/source/test_terrain_performance_contracts.py -v
```

Expected: FAIL because `setRawMapHeight` still calls `staticLightingChanged()`.

- [ ] **Step 3: Route lowered cells to the accumulator**

Replace only the full-update call inside `W3DTerrainVisual::setRawMapHeight`:

```cpp
m_logicHeightMap->setRawHeight(x, y, height);
m_terrainRenderObject->terrainHeightChanged(x, y);
```

Leave the other `staticLightingChanged()` call in this file unchanged because it belongs to a different global-update path.

- [ ] **Step 4: Run tests and build**

```bash
python3 scripts/qa/source/test_terrain_performance_contracts.py -v
source "$HOME/VulkanSDK/1.4.357.0/setup-env.sh"
cmake --build build/macos-vulkan --target z_generals -j4
```

Expected: all focused tests pass and the ARM64 target exits 0.

- [ ] **Step 5: Commit producer wiring**

```bash
git add scripts/qa/source/test_terrain_performance_contracts.py \
  Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainVisual.cpp
git commit -m "perf: record foundation terrain dirty cells"
```

### Task 4: Flush one partial terrain update per render frame

**Files:**
- Modify: `scripts/qa/source/test_terrain_performance_contracts.py`
- Modify: `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp:1395-1402`

- [ ] **Step 1: Add the failing consumer-wiring test**

Append:

```python
    def test_view_prefers_full_update_then_flushes_one_dirty_region(self):
        source = (
            ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp"
        ).read_text(encoding="utf-8")
        body = function_body(source, "void W3DView::update()")

        full = body.index("doesNeedFullUpdate()")
        partial = body.index("takeDirtyRegion(dirtyTerrain)")
        self.assertLess(full, partial)
        self.assertIn("else if", body[full:partial])
        self.assertIn("doPartialUpdate(dirtyTerrain", body)
        self.assertIn("destroyLightsIterator", body[partial:])
```

- [ ] **Step 2: Run the test and verify RED**

```bash
python3 scripts/qa/source/test_terrain_performance_contracts.py -v
```

Expected: FAIL because `W3DView::update` has no dirty-region consumer.

- [ ] **Step 3: Add the partial-update branch**

Replace the current full-update block in `W3DView::update` with:

```cpp
IRegion2D dirtyTerrain;
if (TheTerrainRenderObject && TheTerrainRenderObject->doesNeedFullUpdate())
{
	updateTerrain();
}
else if (TheTerrainRenderObject && TheTerrainRenderObject->takeDirtyRegion(dirtyTerrain))
{
	RefRenderObjListIterator *it = W3DDisplay::m_3DScene->createLightsIterator();
	TheTerrainRenderObject->doPartialUpdate(
		dirtyTerrain,
		TheTerrainRenderObject->getMap(),
		it);
	if (it)
		W3DDisplay::m_3DScene->destroyLightsIterator(it);
}
```

The `else if` is required: a pending full update supersedes and safely defers the dirty rectangle.

- [ ] **Step 4: Run all source QA tests and build**

```bash
python3 -m unittest discover -s scripts/qa/source -p 'test_*.py' -v
source "$HOME/VulkanSDK/1.4.357.0/setup-env.sh"
cmake --build build/macos-vulkan --target z_generals -j4
```

Expected: all source QA tests pass and the ARM64 target exits 0.

- [ ] **Step 5: Commit consumer wiring**

```bash
git add scripts/qa/source/test_terrain_performance_contracts.py \
  Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp
git commit -m "perf: flush partial terrain updates once per frame"
```

### Task 5: Review, deploy, and verify on the M1 Mac

**Files:**
- Verify: all files from Tasks 1-4
- Deploy: `build/macos-vulkan/GeneralsMD/GeneralsXZH`
- Deploy: `/Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH`

- [ ] **Step 1: Run the final automated gate**

```bash
python3 -m unittest discover -s scripts/qa/source -p 'test_*.py' -v
git diff --check HEAD~4..HEAD
source "$HOME/VulkanSDK/1.4.357.0/setup-env.sh"
cmake --build build/macos-vulkan --target z_generals -j4
```

Expected: zero test failures, no whitespace errors, and build exit 0.

- [ ] **Step 2: Obtain code review**

Request review focused on dirty-rectangle bounds, full-update precedence, iterator lifetime, and whether any `staticLightingChanged` caller other than foundation lowering was changed. Resolve every blocker and rerun Step 1.

- [ ] **Step 3: Back up and deploy the signed ARM64 binary**

```bash
cp /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH \
  /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH.before-terrain-performance
cp build/macos-vulkan/GeneralsMD/GeneralsXZH \
  /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH
chmod +x /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH
codesign --force --sign - /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH
codesign --verify --verbose=2 /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH
file /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH
```

Expected: valid signature and `Mach-O 64-bit executable arm64`.

- [ ] **Step 4: Run the visual/runtime scenario**

Launch the desktop command, start a skirmish on a map with uneven ground, place several large buildings near the center and near each map edge, then confirm:

- Foundation ground remains visually level with no dark seams or stale lighting.
- Roads and scorch lighting remain correct.
- Camera movement stays smooth while foundations are placed.
- Saving, loading, and continuing the match works.
- No new `.ips` crash report appears.

- [ ] **Step 5: Keep or roll back based on evidence**

Keep the phase only if automated gates and the runtime scenario pass. Otherwise restore:

```bash
cp /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH.before-terrain-performance \
  /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH
chmod +x /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH
codesign --force --sign - /Users/mbat/GeneralsX/GeneralsZH/GeneralsXZH
```

- [ ] **Step 6: Record Phase 1 result**

Append this section to `docs/superpowers/specs/2026-09-01-reborn-performance-port-design.md`, replacing the bracketed values with the observed evidence:

```markdown
## Phase 1 result

- Source QA: [passed count] passed, 0 failed.
- ARM64 build: exit 0; deployed binary passed `codesign --verify`.
- Runtime terrain scenario: [pass or fail, with the observed artifact if failed].
- Crash reports: [none, or exact new report filename].
- Decision: [keep Phase 1 and plan Phase 2, or roll back Phase 1].
```

Commit only the performance-port files and result note.
