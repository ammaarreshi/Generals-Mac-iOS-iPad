import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
GLOBAL_DATA_SOURCE = (
    REPO_ROOT / "GeneralsMD/Code/GameEngine/Source/Common/GlobalData.cpp"
)


def function_body(source: str, signature: str) -> str:
    signature_start = source.index(signature)
    body_start = source.index("{", signature_start)
    depth = 0

    for index in range(body_start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[body_start : index + 1]

    raise AssertionError(f"Unterminated function body: {signature}")


class GlobalDataCameraPreferencesTest(unittest.TestCase):
    def test_parse_game_data_applies_all_camera_preferences(self) -> None:
        source = GLOBAL_DATA_SOURCE.read_text(encoding="utf-8")
        body = function_body(
            source, "void GlobalData::parseGameDataDefinition( INI* ini )"
        )
        expected_wiring = {
            "MaxCameraHeight": (
                r"TheWritableGlobalData\s*->\s*m_maxCameraHeight\s*=\s*"
                r"optionPref\.getMaxCameraHeight\(\)\s*;"
            ),
            "MinCameraHeight": (
                r"TheWritableGlobalData\s*->\s*m_minCameraHeight\s*=\s*"
                r"optionPref\.getMinCameraHeight\(\)\s*;"
            ),
            "CameraPitch": (
                r"TheWritableGlobalData\s*->\s*m_cameraPitch\s*=\s*"
                r"optionPref\.getCameraPitch\(\)\s*;"
            ),
            "TerrainDrawDistanceScale": (
                r"TheWritableGlobalData\s*->\s*m_terrainDrawDistanceScale\s*=\s*"
                r"optionPref\.getTerrainDrawDistanceScale\(\)\s*;"
            ),
        }

        missing = [
            preference
            for preference, pattern in expected_wiring.items()
            if re.search(pattern, body) is None
        ]

        self.assertEqual(
            [],
            missing,
            "GlobalData::parseGameDataDefinition does not apply these "
            f"OptionPreferences values: {', '.join(missing)}",
        )


if __name__ == "__main__":
    unittest.main()
