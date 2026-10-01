"""Scoring inserts a world projection before the Rescue HUD."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.build.rt64_rescue_hud_layout import RESCUE_HUD_HELPER


class RescueScoreLayoutTests(unittest.TestCase):
    def test_score_projection_shift_keeps_both_hud_anchors(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        source = r'''#include <array>
#include <cstdio>
struct Rect { int ulx=0,uly=0,lrx=1280,lry=960; };
struct Call { struct { Rect rect; } callDesc; };
struct Projection { unsigned type=0,transformsIndex=0,gameCallCount=0;
    Rect scissorRect; std::array<Call,40> gameCalls; };
struct Pair { Rect scissorRect; unsigned projectionCount=9;
    std::array<Projection,12> projections; };
struct Viewport { struct { float x=0,y=0; } scale,translate; };
struct Data { std::array<Viewport,12> rspViewports; };
namespace RT64 {
''' + RESCUE_HUD_HELPER + r'''
int run() {
    Pair pair; Data data;
    pair.projections[2].type=1;
    pair.projections[3].type=1;
    pair.projections[4].type=2; pair.projections[4].gameCallCount=3;
    pair.projections[4].transformsIndex=4;
    pair.projections[5].type=3; pair.projections[5].gameCallCount=39;
    pair.projections[5].gameCalls[0].callDesc.rect={108,312,347,411};
    pair.projections[5].gameCalls[1].callDesc.rect={108,72,387,183};
    pair.projections[8].type=2; pair.projections[8].gameCallCount=2;
    pair.projections[8].transformsIndex=8;
    pair.projections[8].gameCalls[0].callDesc.rect={932,168,1026,230};
    data.rspViewports[4].scale={160,120};
    data.rspViewports[4].translate={160,120};
    data.rspViewports[8].scale={160,120};
    data.rspViewports[8].translate={160,120};
    int next=-1,hud=-1,skull=-1;
    const int right=tetrisphere_rescue_hud_layout(pair,data,1.333333f,&next,&hud);
    pair.projectionCount=11;
    pair.projections[8].type=3;
    pair.projections[8].gameCallCount=1;
    pair.projections[10].type=2; pair.projections[10].gameCallCount=2;
    pair.projections[10].transformsIndex=10;
    pair.projections[10].gameCalls[0].callDesc.rect={1092,128,1131,191};
    data.rspViewports[10].scale={160,120};
    data.rspViewports[10].translate={160,120};
    pair.projections[8].gameCalls[0].callDesc.rect={1092,128,1131,191};
    const int right10=tetrisphere_rescue_hud_layout(pair,data,1.333333f,&next,&hud,&skull);
    const int skull10=skull;
    pair.projections[10].gameCallCount=0;
    next=hud=skull=-1;
    const int noRight=tetrisphere_rescue_hud_layout(pair,data,1.333333f,&next,&hud,&skull);
    const int noRightNext=next, noRightHud=hud, noRightSkull=skull;
    pair.projections[10].gameCallCount=2;
    pair.projections[5].gameCalls[1].callDesc.rect.lrx=386;
    const int wrong=tetrisphere_rescue_hud_layout(pair,data,1.333333f,&next,&hud);
    std::printf("%d %d %d %d %d %d %d %d %d %d",right,next,hud,right10,
        skull10,noRight,noRightNext,noRightHud,noRightSkull,wrong);
    return 0;
}}
int main(){return RT64::run();}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "score.cpp"
            exe = Path(directory) / "score"
            path.write_text(source)
            compiled = subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                                      capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        self.assertEqual(result.stdout, "8 4 5 10 8 -1 4 5 8 -1")


if __name__ == "__main__":
    unittest.main()
