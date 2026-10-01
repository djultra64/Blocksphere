"""The pause source gate follows a tagged title through projection changes."""

import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PauseTitleGateTests(unittest.TestCase):
    def test_tagged_title_is_required_regardless_of_projection_index(self):
        from tools.build.rt64_pause_snapshot import PAUSE_SNAPSHOT_HELPER

        match = re.search(
            r"    static bool tetrisphere_rescue_tagged_title\(.*?\n    }",
            PAUSE_SNAPSHOT_HELPER,
            re.S,
        )
        self.assertIsNotNone(match)
        source = r"""
#include <cstdint>
#include <vector>
struct Rect { int32_t ulx, uly, lrx, lry; };
struct DrawCall { uint32_t uid; Rect rect; };
struct GameCall { DrawCall callDesc; };
struct Projection {
    enum class Type { Rectangle, Orthographic };
    Type type;
    std::vector<GameCall> gameCalls;
    uint32_t gameCallCount;
};
struct FramebufferPair {
    std::vector<Projection> projections;
    uint32_t projectionCount;
};
""" + match.group(0) + r"""
int main() {
    FramebufferPair pair{{{Projection::Type::Orthographic, {}, 0},
                          {Projection::Type::Rectangle, {{{0, {108, 72, 387, 183}}}}, 1},
                          {Projection::Type::Rectangle, {{{1, {108, 72, 387, 183}}}}, 1}}, 3};
    if (!tetrisphere_rescue_tagged_title(pair)) return 1;
    pair.projections.insert(pair.projections.begin(), {Projection::Type::Orthographic, {}, 0});
    pair.projectionCount++;
    if (!tetrisphere_rescue_tagged_title(pair)) return 2;
    pair.projections[3].gameCalls[0].callDesc.uid = 0;
    if (tetrisphere_rescue_tagged_title(pair)) return 3;
    pair.projections[3].gameCalls[0].callDesc.uid = 1;
    pair.projections[3].gameCalls[0].callDesc.rect.ulx = 109;
    if (tetrisphere_rescue_tagged_title(pair)) return 4;
    pair.projections[3].gameCalls[0].callDesc.rect.ulx = 108;
    pair.projections[3].type = Projection::Type::Orthographic;
    if (tetrisphere_rescue_tagged_title(pair)) return 5;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            src = Path(directory) / "gate.cpp"
            exe = Path(directory) / "gate"
            src.write_text(source)
            subprocess.run([shutil.which("c++") or "c++", "-std=c++17", str(src), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()
