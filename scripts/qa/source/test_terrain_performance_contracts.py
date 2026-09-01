import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HEIGHT_MAP_CPP = ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp"


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


if __name__ == "__main__":
    unittest.main()
