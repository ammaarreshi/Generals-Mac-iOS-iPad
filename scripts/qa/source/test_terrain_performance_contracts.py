import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HEIGHT_MAP_CPP = ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp"
BASE_HEIGHT_MAP_CPP = ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp"
BASE_HEIGHT_MAP_HEADER = ROOT / "Core/GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h"
W3D_TERRAIN_VISUAL_CPP = (
    ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainVisual.cpp"
)


def function_body(source: str, signature: str) -> str:
    """Return a C++ function body, accounting for comments and literals."""
    signature_start = source.index(signature)
    body_start = source.index("{", signature_start)
    depth = 0
    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_char = False
    escaped = False

    index = body_start
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""

        if in_line_comment:
            if char == "\n":
                in_line_comment = False
        elif in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
                index += 1
        elif in_string or in_char:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif (in_string and char == '"') or (in_char and char == "'"):
                in_string = False
                in_char = False
        elif char == "/" and next_char == "/":
            in_line_comment = True
            index += 1
        elif char == "/" and next_char == "*":
            in_block_comment = True
            index += 1
        elif char == '"':
            in_string = True
        elif char == "'":
            in_char = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[body_start : index + 1]
        index += 1

    raise AssertionError(f"Unterminated function body: {signature}")


class TerrainPerformanceContractsTest(unittest.TestCase):
    def test_global_light_rays_are_built_before_vertex_loops(self) -> None:
        source = HEIGHT_MAP_CPP.read_text(encoding="utf-8")
        body = function_body(
            source,
            "Int HeightMapRenderObjClass::updateVB(",
        )

        light_loop = "for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)"
        vertex_loop = "for (j=y0;"
        self.assertIn(light_loop, body)
        self.assertLess(body.index(light_loop), body.index(vertex_loop))
        self.assertEqual(body.count("m_terrainLightPos[lightIndex]"), 1)

    def test_terrain_height_changes_accumulate_a_bounded_consumable_region(self) -> None:
        header = BASE_HEIGHT_MAP_HEADER.read_text(encoding="utf-8")
        source = BASE_HEIGHT_MAP_CPP.read_text(encoding="utf-8")

        static_lighting_declaration = "virtual void staticLightingChanged();"
        self.assertIn(static_lighting_declaration, header)
        declaration_start = header.index(static_lighting_declaration)
        self.assertIn("void terrainHeightChanged(Int x, Int y);", header[declaration_start:])
        self.assertIn("Bool takeDirtyRegion(IRegion2D &region);", header[declaration_start:])
        self.assertIn("IRegion2D m_dirtyRegion;", header)
        self.assertIn("Bool m_hasDirtyRegion;", header)

        redirect_body = function_body(header, "void redirectToHeightmap(")
        self.assertIn("m_hasDirtyRegion = false;", redirect_body)
        self.assertLess(
            redirect_body.index("m_hasDirtyRegion = false;"),
            redirect_body.index("REF_PTR_SET(m_map, pMap);"),
        )

        reset_body = function_body(source, "void BaseHeightMapRenderObjClass::reset()")
        self.assertIn("m_hasDirtyRegion = false;", reset_body)

        init_body = function_body(
            source,
            "Int BaseHeightMapRenderObjClass::initHeightData(",
        )
        self.assertIn("m_hasDirtyRegion = false;", init_body)
        self.assertIn(
            "if (m_map != pMap)\n\t{\n\t\tm_hasDirtyRegion = false;\n\t}",
            init_body,
        )
        self.assertLess(
            init_body.index("m_hasDirtyRegion = false;"),
            init_body.index("REF_PTR_SET(m_map, pMap);"),
        )

        changed_body = function_body(
            source,
            "void BaseHeightMapRenderObjClass::terrainHeightChanged(Int x, Int y)",
        )
        self.assertIn("const Int PAD = 2;", changed_body)
        self.assertIn("m_map->getXExtent() - 1", changed_body)
        self.assertIn("m_map->getYExtent() - 1", changed_body)
        self.assertIn("m_hasDirtyRegion = false;", changed_body)

        changed_fragments = (
            "m_dirtyRegion.lo.x = 0;",
            "m_dirtyRegion.lo.y = 0;",
            "m_dirtyRegion.hi.x = maxX;",
            "m_dirtyRegion.hi.y = maxY;",
            "m_scorchesInBuffer = 0;",
            "m_curNumScorchVertices = 0;",
            "m_curNumScorchIndices = 0;",
        )
        for fragment in changed_fragments:
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, changed_body)

        self.assertNotIn("m_roadBuffer->updateLighting();", changed_body)

        take_body = function_body(
            source,
            "Bool BaseHeightMapRenderObjClass::takeDirtyRegion(IRegion2D &region)",
        )
        self.assertIn("if (!m_hasDirtyRegion)", take_body)
        self.assertIn("region = m_dirtyRegion;", take_body)
        self.assertIn("m_hasDirtyRegion = false;", take_body)
        for fragment in ("return false;", "return true;"):
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, take_body)

        road_update = "m_roadBuffer->updateLighting();"
        self.assertEqual(take_body.count(road_update), 1)
        self.assertIn("if (m_roadBuffer)", take_body)
        self.assertLess(take_body.index("region = m_dirtyRegion;"), take_body.index(road_update))
        self.assertLess(take_body.index("if (m_roadBuffer)"), take_body.index(road_update))
        self.assertLess(take_body.index(road_update), take_body.index("return true;"))
        self.assertLess(
            take_body.index("region = m_dirtyRegion;"),
            take_body.index("m_hasDirtyRegion = false;"),
        )

        static_lighting_body = function_body(
            source,
            "void BaseHeightMapRenderObjClass::staticLightingChanged()",
        )
        self.assertIn("m_scorchesInBuffer = 0;", static_lighting_body)
        self.assertIn("if (m_roadBuffer)", static_lighting_body)
        self.assertIn("m_roadBuffer->updateLighting();", static_lighting_body)

    def test_foundation_lowering_records_a_partial_terrain_update(self) -> None:
        source = W3D_TERRAIN_VISUAL_CPP.read_text(encoding="utf-8")
        body = function_body(source, "void W3DTerrainVisual::setRawMapHeight(")

        self.assertIn("terrainHeightChanged(x, y);", body)
        self.assertNotIn("staticLightingChanged()", body)


if __name__ == "__main__":
    unittest.main()
