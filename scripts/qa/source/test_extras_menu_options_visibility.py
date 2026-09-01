import re
import unittest
from pathlib import Path


EXTRAS_MENU_CPP = (
    Path(__file__).resolve().parents[3]
    / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/ExtrasMenu.cpp"
)
EXTRAS_MENU_WND = (
    Path(__file__).resolve().parents[3]
    / "GeneralsZH/Data/Window/Menus/ExtrasMenu.wnd"
)


def function_body(source: str, name: str) -> str:
    match = re.search(
        rf"void\s+{name}\s*\([^)]*\)\s*\{{(?P<body>.*?)\n\}}",
        source,
        flags=re.DOTALL,
    )
    if not match:
        raise AssertionError(f"function not found: {name}")
    return match.group("body")


class ExtrasMenuOptionsVisibilityTest(unittest.TestCase):
    def test_extras_root_is_opaque(self):
        source = EXTRAS_MENU_WND.read_text(encoding="utf-8")
        root = source.split("  CHILD", 1)[0]

        self.assertRegex(
            root,
            r"ENABLEDDRAWDATA\s*=\s*IMAGE:\s*NoImage,\s*COLOR:\s*2\s+2\s+2\s+255",
        )

    def test_shutdown_completes_shell_pop(self):
        source = EXTRAS_MENU_CPP.read_text(encoding="utf-8")
        shutdown = function_body(source, "ExtrasMenuShutdown")

        self.assertRegex(shutdown, r"layout->hide\s*\(\s*TRUE\s*\)")
        self.assertRegex(
            shutdown,
            r"TheShell->shutdownComplete\s*\(\s*layout\s*\)",
        )


if __name__ == "__main__":
    unittest.main()
