import re
import unittest
from pathlib import Path


EXTRAS_MENU = (
    Path(__file__).resolve().parents[3]
    / "GeneralsZH/Data/Window/Menus/ExtrasMenu.wnd"
)


class ExtrasMenuSliderImagesTest(unittest.TestCase):
    def test_every_slider_has_drawable_disabled_track_images(self):
        text = EXTRAS_MENU.read_text(encoding="utf-8")
        slider_blocks = re.findall(
            r"\bWINDOW\s*\n\s*WINDOWTYPE = HORZSLIDER;(?P<body>.*?)\n\s*END",
            text,
            flags=re.DOTALL,
        )

        self.assertEqual(5, len(slider_blocks))
        broken = []
        for block in slider_blocks:
            name = re.search(r'NAME = "([^"]+)";', block).group(1)
            disabled = re.search(
                r"DISABLEDDRAWDATA = IMAGE: ([^,]+),.*?\n\s*IMAGE: ([^,]+),",
                block,
                flags=re.DOTALL,
            )
            if not disabled or "NoImage" in disabled.groups():
                broken.append(name)

        self.assertEqual([], broken, f"sliders with null track images: {broken}")


if __name__ == "__main__":
    unittest.main()
