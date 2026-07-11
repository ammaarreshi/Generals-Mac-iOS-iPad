# Future: SDL3 Render as Complete Graphics Stack

**Document Status**: Strategic Analysis (Distant Future — 2028+)  
**Author**: GeneralsX Development Team  
**Date**: February 13, 2026  
**Related Phases**: Phase 4+ (Post-Linux stabilization)

---

## Executive Summary

**Question**: Can we replace DXVK with native SDL3 Render in a distant future?

**Answer**: **Technically yes, but strategically no** for the foreseeable future (2026-2028).

**Key Finding**: DXVK is the pragmatic solution. SDL3 Render migration would require 18-24+ months of development, risk Windows VC6 compatibility, and introduce 30-50% performance loss. Not recommended unless:
- macOS native builds become mandatory business requirement
- Web port via Emscripten becomes priority
- Game engine modernization is desired (separate effort)

---

## Current Architecture (DXVK Strategy)

### Why DXVK, Not SDL Render?

**Generals/Zero Hour Graphics Pipeline** (2003 era):

```
DirectX 8 Native Code
├─ Vertex Buffers (D3D VB/IB objects)
├─ Shader Model 1.0-2.0 HLSL shaders
├─ Z-buffer + depth testing
├─ Blend modes (D3D8-specific enums)
├─ Texture formats (TEXELFORMAT enum)
└─ 3D Meshes (LOD, particles, terrain)

 ↓ (DXVK intercepts at DLL boundary)

Vulkan Backend (Linux)
├─ Converts VB → Vulkan buffer
├─ Converts HLSL → SPIR-V (shader compilation)
├─ Translates blend modes → Vulkan equivalents
└─ Outputs native Vulkan commands

Result: Zero changes to game code, native Linux performance
```

### Why NOT SDL Render Today?

SDL3 Render is a **2D high-level abstraction**:

```c
// SDL3 Render (simplified 2D API)
SDL_RenderGeometry(renderer, NULL, vertices, vertex_count, indices, index_count);
SDL_RenderTexture(renderer, texture, src_rect, dst_rect);  // sprites only
SDL_RenderLine(renderer, x1, y1, x2, y2);

// Does NOT support:
// ❌ Vertex buffers with custom strides
// ❌ Index buffers for efficient mesh rendering
// ❌ Shader customization (limited to built-in shaders)
// ❌ Advanced blend modes (limited set)
// ❌ Texture coordinates manipulation (limited UVs)
```

**Performance Impact of SDL Render**:
- Generals uses **22-45 million vertices/frame** (terrain LOD, units, effects)
- SDL Render: Immediate-mode API (no batching optimization)
- Expected performance loss: **30-50%** (~40 FPS → ~20 FPS on average hardware)
- CPU bottleneck moves to immediate-mode submission

---

## Comparison Matrix

| Feature | DXVK (Current) | SDL3 Render (Hypothetical) | SDL3 Full Stack |
|---------|---|---|---|
| **Graphics Device** | DirectX 8 wrapper → Vulkan | SDL high-level API | Custom deferred renderer |
| **Window/Input** | SDL3 ✓ | SDL3 ✓ | SDL3 ✓ |
| **Audio** | OpenAL (Phase 2 target) | SDL AudioStream | Multiple backends |
| **Video Playback** | FFmpeg | FFmpeg | FFmpeg |
| **Linux Support** | ✓ Native (Vulkan) | ✓ But slow | ✓ Requires redesign |
| **Windows VC6 Build** | ✓ Unchanged | ✓ Unchanged | ❌ BREAKS (shader changes) |
| **Windows MSVC Win32** | ✓ DirectX8 native | ✓ Unchanged | ⚠️ Needs DirectX shim |
| **Performance vs DirectX8** | 95-100% | 50-70% | 90-110% (with optimization) |
| **Replay Determinism** | ✓ Preserved | ⚠️ Renderer precision differs | ⚠️ Precision loss likely |
| **Development Effort** | Complete (Phase 1) | 12-18 months | 24-36 months |
| **Risk Level** | LOW | MEDIUM-HIGH | VERY HIGH |
| **Code Maintainability** | Good (wrapper layer) | Worse (abstraction loss) | Best (modern architecture) |

---

## Why DXVK Wins Today

### 1. **Preservation by Translation**

```cpp
// Game code: Unchanged
IDirect3DDevice8* pDevice = ...;
pDevice->SetVertexBuffer(0, vertexBuffer, 0, sizeof(Vertex));
pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, nVertices, 0, nPrimitives);

// Under the hood:
// DXVK DLL intercepts COM calls
// → Converts to Vulkan API
// → Compiles HLSL shaders to SPIR-V
// → Dispatches to GPU driver
// Result: Game logic unchanged, Linux works perfectly
```

### 2. **Determinism Preserved**

- Replays work (`GeneralsReplays/*.rep` compatibility)
- No floating-point arithmetic changes
- Rendering path === original DirectX8
- ✓ Phase 1 acceptance tests pass

### 3. **Minimal Diff Against Upstream**

```bash
git diff TheSuperHackers/main -- Core/GameEngineDevice/
  # ~500 LOC additions (SDL3 window integration)
  # ~50 LOC changes (DXVK device wrapper)
  # DirectX8 path: 100% unchanged
```

### 4. **Fighter19's Proven Solution**

Reference implementation (`references/old-refs/fighter19-dxvk-port/`):
- ✓ Full Linux build (native ELF)
- ✓ Both Generals + Zero Hour
- ✓ Gameplay tested (skirmish, campaign intros)
- ✓ No determinism breaks observed

---

## Theoretical SDL3 Render Path (2028+)

### Phase 4a: Renderer Abstraction Layer (Months 1-6)

```cpp
// New abstraction (game logic untouched)
class IVertexBuffer {
  virtual void Lock(void** ppData, uint size) = 0;
  virtual void Unlock() = 0;
  virtual void DrawIndexed(uint index_count) = 0;
};

class IGraphicsDevice {
  virtual IVertexBuffer* CreateVertexBuffer(uint size) = 0;
  virtual void SetBlendMode(EBlendMode mode) = 0;
  virtual void DrawPrimitive(...) = 0;
};

// Implementations:
class DX8Device : public IGraphicsDevice { /* COM interop */ };
class SDLRenderDevice : public IGraphicsDevice { /* SDL high-level */ };
class VulkanDevice : public IGraphicsDevice { /* Direct Vulkan, future */ };
```

**Effort**: ~80-100k LOC refactor (locating all D3D8 calls, wrapping, testing)

### Phase 4b: Shader Modernization (Months 7-12)

```cpp
// Today: HLSL 1.0-2.0 shaders compiled by DirectX 8 runtime
// Problem: SDL Render doesn't compile HLSL

// Solution: Convert to SPIR-V or GLSL
//   (But requires understanding each shader's intent)

// Example: Terrain shader rewrite
// Original (HLSL):
float4 main(VS_INPUT input) : COLOR {
  float3 normal = normalize(input.normal);
  float4 diffuse = tex2D(textureSampler, input.uv);
  return diffuse * max(0, dot(normal, lightDir));
}

// New (GLSL with binary compatibility):
#version 450
layout(location = 0) in vec3 in_normal;
layout(location = 0) out vec4 out_color;
uniform sampler2D textureSampler;
void main() {
  vec3 normal = normalize(in_normal);
  vec4 diffuse = texture(textureSampler, ...);
  out_color = diffuse * max(0.0, dot(normal, lightDir));
}
```

**Effort**: ~40-60k LOC shader work (200+ shaders, each tested individually)

### Phase 4c: SDL Render Integration (Months 13-18)

```cpp
// Replace immediate-mode:
class SDLRenderDevice : public IGraphicsDevice {
  void DrawIndexed(uint indexCount) override {
    // Batch submission to SDL_RenderGeometry
    SDL_RenderGeometry(
      renderer,
      texture,
      (SDL_FPoint*)vertexData,  // Converted to SDL format
      vertexCount,
      (int*)indexData,
      indexCount
    );
    // Performance loss: 40-50% expected
  }
};
```

**Effort**: ~20-30k LOC integration + optimization attempts

### Phase 4d: Testing & Validation (Months 19-24)

- ✓ Replay compatibility tests (determinism check)
- ✓ Visual regression tests (screenshot comparison)
- ✓ Performance benchmarks (compare with DXVK baseline)
- ✓ Multi-platform testing (Windows MSVC, VC6, Linux)

**Effort**: ~10-15k LOC test infrastructure

---

## Risk Assessment

### HIGH RISKS

1. **Determinism Break** (CRITICAL for replays)
   - Floating-point precision differs between backends
   - SDL Render may batch commands differently
   - Replay files become incompatible
   - Mitigation: Extensive testing, but can't guarantee

2. **Windows VC6 Breakage** (CRITICAL for retail compatibility)
   - Modern abstraction layer may require C++11+ features
   - VC6 doesn't support many modern patterns
   - Mitigation: Use C++98-compatible abstractions (harder)

3. **Performance Regression** (HIGH)
   - 30-50% loss from immediate-mode rendering
   - Could make game unplayable on older hardware
   - Mitigation: Implement batching layer (doubles effort)

4. **Shader Maintenance Burden** (MEDIUM-HIGH)
   - 200+ shaders need rewrite + testing
   - Each shader is a potential bug
   - Mitigation: Automated HLSL→GLSL converter (not reliable)

### MEDIUM RISKS

5. **SDL3 API Stability** (MEDIUM)
   - SDL3 still evolving (1.0 release July 2024)
   - Future API changes possible
   - Mitigation: Lock to specific SDL3.x version

6. **Audio/Video Integration Complexity** (MEDIUM)
   - Mixing SDL Render + OpenAL + FFmpeg
   - Sync challenges
   - Mitigation: Use SDL AudioStream (already planned for Phase 2)

---

## Cost-Benefit Analysis

### Investment Required (24 months, 2-3 devs)

| Phase | Months | FTE | Primary Risk |
|-------|--------|-----|--------------|
| 4a: Abstraction | 6 | 1.5 | Code fragility |
| 4b: Shaders | 6 | 2.0 | Correctness |
| 4c: SDL Integration | 6 | 1.0 | Performance |
| 4d: Testing | 6 | 2.0 | Determinism |
| **Total** | **24** | **6.5 FTE** | **VERY HIGH** |

### Return on Investment

✅ **Benefits**:
- macOS native port (no Wine needed)
- Web port via Emscripten (theoretical)
- Simpler build (no DXVK dependency)
- Modern shader pipeline

❌ **Downsides**:
- 30-50% performance loss (not recoverable)
- 18 months of stabilization for new bugs
- Windows VC6 compatibility likely broken
- Replay system becomes fragile

**Verdict**: **NOT RECOMMENDED** unless:
- Business requirement: "Generals must run natively on macOS"
- Business requirement: "Web port is priority"
- Available devs: 3+ full-time
- Timeline: 24+ months

---

## Alternative Paths

### Option A: Vulkan Native (Recommended Alternative)

Skip SDL Render, implement **direct Vulkan backend** (instead of DXVK):

```cpp
class VulkanDevice : public IGraphicsDevice {
  // Direct Vulkan API (not through D3D8 translation)
  // Better performance than SDL Render
  // Similar effort: 18-24 months
  // Benefit: 95-100% performance vs DirectX8
};
```

**Advantage over SDL Render**: Full control, better performance  
**Disadvantage**: Still requires abstraction layer refactor

### Option B: Wine + DXVK (Current, Recommended)

Keep DXVK for Linux, use Wine on non-native platforms (macOS, etc.):

```bash
# Today (recommended)
./GeneralsXZH -win          # Linux native (DXVK)
wine GeneralsXZH.exe        # Windows exe on Linux (compatibility)
wine GeneralsXZH.exe        # Windows exe on macOS (Wine runtime)

# No refactoring needed, works today
```

**Advantage**: Zero development cost, works now  
**Disadvantage**: Requires Wine on macOS (not native)

---

## Decision Timeline

| When | Decision |
|------|----------|
| **Now (2026)** | ✓ Stick with DXVK (Phase 1 complete) |
| **2027** | Evaluate: Is macOS native a requirement? |
| **2028** | IF yes: Plan Phase 4 architecture sprint |
| **2029+** | IF approved: Begin SDL Render migration |

---

## Lessons for Future Consideration

1. **Abstraction layers are expensive**: The cost compounds with each layer.
2. **Determinism is critical**: Replays are a stability anchor; don't break them.
3. **Three-way compatibility is hard**: Windows + Linux + legacy tools = constraint.
4. **DXVK is a force multiplier**: Let the driver handle graphics; focus on gameplay.
5. **Performance regressions are hard to recover from**: 30% loss is not acceptable for PvP.

---

## Recommendations

### For Next 24 Months (2026-2028)

✓ Continue DXVK-based Linux port (Phase 1-3)  
✓ Stabilize OpenAL audio (Phase 2)  
✓ Add FFmpeg video support (Phase 3)  
✓ Bug-fix and harden core systems  

### For 2028+ (If Requirements Change)

IF macOS native becomes mandatory:
1. **Research phase**: Evaluate pure Vulkan vs SDL Render trade-offs
2. **Design phase**: Create IGraphicsDevice abstraction
3. **Prototype phase**: Implement shader converter tooling
4. **Implementation phase**: Begin with non-critical game subsystems
5. **Validation phase**: Intensive replay/determinism testing

### Documentation & Tracking

- [ ] Keep this document updated as platform requirements evolve
- [ ] Document any new DXVK limitations discovered
- [ ] Track community interest in macOS native builds
- [ ] Monitor SDL3 stability (1.0.x versions)
- [ ] Archive shader compatibility decisions in `docs/WORKDIR/support/`

---

## Open Questions

- **Q**: Could we use Metal on macOS instead of Vulkan?  
  **A**: Technically yes, but adds third backend (3x complexity). DXVK already handles this via WSI layer.

- **Q**: What about web ports via Emscripten?  
  **A**: Requires WebGPU (not mature). Current: Use Wine + browser? Revisit 2027.

- **Q**: Can we do partial SDL migration (render only, keep D3D8 audio)?  
  **A**: Theoretically, but mixing rendering backends adds complexity. Not recommended.

- **Q**: How does this affect the Generals (base game) backport?  
  **A**: Same analysis applies. Base game is less complex, could migrate faster if needed.

---

## References

- **Current Implementation**: [DXVK Graphics Pipeline](Phase1_DXVK_Graphics.md) (when written)
- **SDL3 Documentation**: https://wiki.libsdl.org/
- **Fighter19 Reference**: `references/old-refs/fighter19-dxvk-port/`
- **DirectX8 API**: DXVK source code analysis
- **Replay System**: `docs/WORKDIR/support/REPLAY_DETERMINISM.md` (future)

---

**Document Owner**: GeneralsX Development Team  
**Last Updated**: February 13, 2026  
**Next Review**: February 2027 (annual strategy sync)
