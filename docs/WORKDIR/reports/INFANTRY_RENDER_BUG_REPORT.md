# Bug Report: Skinned Mesh (Infantry) Rendering Black on Vulkan/DXVK in Base Game

## Context
In the macOS Vulkan port of the base game (C&C Generals), skinned meshes (such as infantry units) render completely black, even though the same units render perfectly in Zero Hour.

## Attempted Fixes (Already Committed)
1. **Stencil Volume Shadow Skip** (`W3DVolumetricShadow.cpp`): Skinned meshes cannot have CPU stencil shadow volumes projected accurately in real time. We skip shadow volume generation for any mesh or hierarchy (LOD levels) with skinned vertices (`MeshGeometryClass::SKIN`).
2. **Skinned Mesh Vertex Diffuse Color Fallback** (`dx8renderer.cpp`, `meshmdl.cpp`): Skinned meshes without explicit vertex colors had their diffuse fields set to `0` (black) instead of `0xFFFFFFFF` (white), multiplying the texture by zero under Vulkan/DXVK. We added a fallback to `0xFFFFFFFF`.
3. **Infantry Light Clamping** (`W3DScene.cpp`): Infantry lighting was scaled by `infantryLightScale` (1.5f) without clamping, which can cause shader overflows under Vulkan/DXVK. We clamped the ambient and diffuse light components to `(1.0f, 1.0f, 1.0f)` using `Vector3::Cap_Absolute_To`.
4. **Decal Batch Shadow Loop Fixes** (`W3DProjectedShadow.cpp`): Corrected initialization of `lastShadowDecalTexture` and `lastShadowType` to use the current processed active shadow's properties instead of list heads.

## Observed Behavior
Despite these fixes, building shadows and/or infantry units still render as black silhouettes or solid black shapes in the base game, whereas Zero Hour does not exhibit this behavior.

## Recommendations for Further Investigation
- **Shader / Material Passes**: Compare the shader selection, material pass installation (`vmat->Install_Materials()`), or Vertex Shader binding in the skinned rendering path between the base game and Zero Hour.
- **Render States**: Audit blend/rasterizer states in the base game compared to Zero Hour when rendering skinned FVF categories (`compose_deformed_vertex_buffer`).
- **Texture / Sampler State**: Investigate if skin textures are incorrectly bound, cleared, or if their sampler/addressing modes differ under Vulkan/DXVK in the base game.
