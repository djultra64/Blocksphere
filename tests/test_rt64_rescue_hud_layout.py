"""Rescue HUD widening must match only the observed board layout."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.build.rt64_rescue_hud_layout import RESCUE_HUD_HELPER
from tools.build.patch_rt64_probe import SOURCES, patch_source
from tools.build.rt64_pause_snapshot import PAUSE_SNAPSHOT_HELPER


class RescueHudLayoutTests(unittest.TestCase):
    def test_signature_and_sides_are_specific_to_rescue(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        source = r'''#include <array>
#include <cstdio>
struct Rect { int ulx,uly,lrx,lry; };
struct Call { struct { Rect rect; } callDesc; };
struct Projection { unsigned type=0, transformsIndex=0, gameCallCount=0;
    Rect scissorRect{0,0,1280,960}; std::array<Call,40> gameCalls; };
struct Pair { Rect scissorRect{0,0,1280,960}; unsigned projectionCount=8;
    std::array<Projection,12> projections; };
struct Viewport { struct { float x,y; } scale,translate; };
struct Data { std::array<Viewport,12> rspViewports; };
namespace RT64 {
''' + RESCUE_HUD_HELPER + r'''
int run() {
    Pair pair; Data data{};
    pair.projections[2].type=1; pair.projections[2].gameCallCount=229;
    pair.projections[3].type=2; pair.projections[3].gameCallCount=3;
    pair.projections[3].transformsIndex=3;
    pair.projections[4].type=3; pair.projections[4].gameCallCount=39;
    pair.projections[7].type=2; pair.projections[7].gameCallCount=2;
    pair.projections[7].transformsIndex=7;
    pair.projections[7].gameCalls[0].callDesc.rect={932,168,1026,230};
    pair.projections[4].gameCalls[0].callDesc.rect={108,312,347,411};
    pair.projections[4].gameCalls[1].callDesc.rect={108,72,387,183};
    pair.projections[4].gameCalls[12].callDesc.rect={936,616,1195,735};
    data.rspViewports[3].scale={160,120};
    data.rspViewports[3].translate={160,120};
    data.rspViewports[7].scale={160,120};
    data.rspViewports[7].translate={160,120};
    auto good=tetrisphere_rescue_hud_layout(pair,data,1.333333f);
    auto sideLeft=tetrisphere_rescue_hud_rect_side(Rect{108,72,387,183});
    auto sideRight=tetrisphere_rescue_hud_rect_side(Rect{936,616,1195,735});
    auto sideCenter=tetrisphere_rescue_hud_rect_side(Rect{500,0,800,100});
    auto standard=tetrisphere_rescue_hud_layout(pair,data,1.0f);
    pair.projections[4].gameCalls[1].callDesc.rect.lrx=386;
    auto wrongAnchor=tetrisphere_rescue_hud_layout(pair,data,1.333333f);
    pair.projections[4].gameCalls[1].callDesc.rect.lrx=387;
    pair.projections[3].type=1;
    auto wrongPieces=tetrisphere_rescue_hud_layout(pair,data,1.333333f);
    pair.projections[3].type=2;
    pair.scissorRect.lrx=640;
    auto wrongScissor=tetrisphere_rescue_hud_layout(pair,data,1.333333f);
    pair.scissorRect.lrx=1280;
    pair.projectionCount=9;
    pair.projections[7].gameCallCount=0;
    pair.projections[8].type=2;
    pair.projections[8].gameCallCount=2;
    pair.projections[8].transformsIndex=8;
    pair.projections[8].gameCalls[0].callDesc.rect={932,168,1026,230};
    // The heart-loss animation can omit this rectangle while retaining both
    // left HUD anchors and the final orthographic right indicator.
    pair.projections[4].gameCalls[12].callDesc.rect={0,0,0,0};
    data.rspViewports[8].scale={160,120};
    data.rspViewports[8].translate={160,120};
    auto shifted8=tetrisphere_rescue_hud_layout(pair,data,1.333333f);
    pair.projectionCount=10;
    pair.projections[8].type=3;
    pair.projections[8].gameCallCount=1;
    pair.projections[9].type=2;
    pair.projections[9].gameCallCount=2;
    pair.projections[9].transformsIndex=9;
    pair.projections[9].gameCalls[0].callDesc.rect={1092,128,1131,191};
    data.rspViewports[9].scale={160,120};
    data.rspViewports[9].translate={160,120};
    auto shifted9=tetrisphere_rescue_hud_layout(pair,data,1.333333f);
    pair.projections[9].gameCalls[0].callDesc.rect.ulx=500;
    auto wrongRight=tetrisphere_rescue_hud_layout(pair,data,1.333333f);
    std::printf("%d %d %d %d %d %d %d %d %d %d %d",good,sideLeft,sideRight,sideCenter,
                standard,wrongAnchor,wrongPieces,wrongScissor,shifted8,shifted9,wrongRight);
    return 0;
}}
int main(){return RT64::run();}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "layout.cpp"
            exe = Path(directory) / "layout"
            path.write_text(source)
            compiled = subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                                      capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        self.assertEqual(result.stdout, "7 -1 1 0 -1 -1 -1 -1 8 9 -1")

    def test_normal_and_probe_renderer_anchor_each_tagged_draw(self):
        root = Path(__file__).resolve().parents[1]
        path = root / ".local/deps/rt64/src/render/rt64_framebuffer_renderer.cpp"
        raw = path.read_bytes()
        for probe in (False, True):
            source = patch_source(path.name, raw, SOURCES[path.name], probe=probe).decode()
            self.assertIn("tetrisphere_ui_anchor_ortho_for_call", source)
            self.assertIn("tetrisphere_ui_anchor_rect_for_call", source)
            self.assertIn("tetrisphere_ui_anchor_scissor_for_call", source)
            self.assertIn("call.callDesc.uid", source)
            self.assertNotIn("tetrisphere_rescue_hud_layout", source)
            if not probe:
                self.assertNotIn("rt64_probe_rescue_draw", source)

    def test_pause_snapshot_grid_is_scoped_to_guest_copy_and_wide_renderer(self):
        root = Path(__file__).resolve().parents[1]
        path = root / ".local/deps/rt64/src/hle/rt64_workload_queue.cpp"
        raw = path.read_bytes()
        for probe in (False, True):
            source = patch_source(path.name, raw, SOURCES[path.name], probe=probe).decode()
            self.assertIn("tetrisphere_rescue_pause_grid", source)
            self.assertIn("tetrisphere_rt64_pause_snapshot_peek", source)
            self.assertIn("tetrisphere_capture_rescue_pause", source)
            self.assertIn("sourceLastWriter", source)
            self.assertIn("fbManager.recordOperations", source)
            self.assertIn("callTile.tileCopyUsed = true", source)
        config = (root / "config/recomp/tetrisphere.us.rev0.toml").read_text()
        self.assertIn("before_vram = 0x8006F914", config)
        self.assertIn("tetrisphere_rescue_pause_snapshot_hook", config)

    def test_pause_snapshot_grid_rejects_other_tiles(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        grid = PAUSE_SNAPSHOT_HELPER.split("    // The guest draws", 1)[1].split(
            "    // This runs on the wide render thread", 1)[0]
        source = r'''#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>
#define G_IM_FMT_RGBA 0
#define G_IM_SIZ_16b 2
struct Rect { int32_t ulx=0,uly=0,lrx=1280,lry=960; };
struct Desc { Rect rect; uint32_t tileCount=1,loadCount=1,tileIndex=0,loadIndex=0; };
struct Call { Desc callDesc; };
struct Projection { enum class Type { Rectangle }; Type type=Type::Rectangle;
    uint32_t gameCallCount=40; std::array<Call,40> gameCalls; };
struct Pair { uint32_t projectionCount=1; Rect scissorRect;
    std::array<Projection,1> projections; };
struct LoadTexture { uint32_t address=0x166f20; uint32_t width=320,fmt=0,siz=2; };
struct Coords { uint16_t uls=0,ult=0,lrs=0,lrt=0; };
struct LoadOperation { enum class Type { Tile, Block }; Type type=Type::Tile;
    LoadTexture texture; Coords operationTile,tile; };
struct Int2 { int x=0,y=0; };
struct DrawCallTile { Int2 minTexcoord,maxTexcoord; uint16_t sampleWidth=505,sampleHeight=233; };
struct Data { std::array<DrawCallTile,40> callTiles;
    std::array<LoadOperation,40> loadOperations; };
struct Workload { uint32_t fbPairCount=2; std::array<Pair,2> fbPairs; Data drawData; };
namespace RT64 {
''' + "    // The guest draws" + grid + r'''
int run() {
    Workload w;
    for (uint32_t i=0;i<40;i++) {
        auto &d=w.fbPairs[1].projections[0].gameCalls[i].callDesc;
        d.rect={int32_t(i%5*256),int32_t(i/5*120),
                int32_t(i%5==4?1280:i%5*256+255),
                int32_t(i/5==7?960:i/5*120+119)};
        d.tileIndex=d.loadIndex=i;
        auto &load=w.drawData.loadOperations[i];
        load.operationTile=load.tile={uint16_t(i%5*256),uint16_t(i/5*120),
            uint16_t(i%5*256+252),uint16_t(i/5*120+116)};
        w.drawData.callTiles[i].maxTexcoord={64,30};
    }
    std::array<uint32_t,40> indices{};
    const bool valid=tetrisphere_rescue_pause_grid(w,0x166f20,indices);
    w.drawData.loadOperations[17].texture.address=0x18c720;
    const bool wrongAsset=tetrisphere_rescue_pause_grid(w,0x166f20,indices);
    w.drawData.loadOperations[17].texture.address=0x166f20;
    w.fbPairs[1].projections[0].gameCalls[39].callDesc.rect.lrx=1279;
    const bool wrongGrid=tetrisphere_rescue_pause_grid(w,0x166f20,indices);
    w.fbPairs[1].projections[0].gameCalls[39].callDesc.rect.lrx=1280;
    w.fbPairs[1].projections[0].gameCalls[7].callDesc.tileCount=2;
    const bool wrongTile=tetrisphere_rescue_pause_grid(w,0x166f20,indices);
    w.fbPairs[1].projections[0].gameCalls[7].callDesc.tileCount=1;
    w.drawData.loadOperations[17].operationTile.uls++;
    const bool wrongSourceOffset=tetrisphere_rescue_pause_grid(w,0x166f20,indices);
    w.drawData.loadOperations[17].operationTile.uls--;
    w.drawData.callTiles[17].maxTexcoord.x++;
    const bool wrongTexcoord=tetrisphere_rescue_pause_grid(w,0x166f20,indices);
    w.drawData.callTiles[17].maxTexcoord.x--;
    w.fbPairs[1].projections[0].gameCalls[17].callDesc.tileIndex=16;
    const bool duplicateTile=tetrisphere_rescue_pause_grid(w,0x166f20,indices);
    std::printf("%d %d %d %d %d %d %d %u %u",valid,wrongAsset,wrongGrid,wrongTile,
                wrongSourceOffset,wrongTexcoord,duplicateTile,
                indices[0],indices[39]);
    return 0;
}}
int main(){return RT64::run();}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "grid.cpp"
            exe = Path(directory) / "grid"
            path.write_text(source)
            compiled = subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                                      capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        self.assertEqual(result.stdout, "1 0 0 0 0 0 0 0 39")

    def test_pause_snapshot_fractional_tiles_share_all_edges(self):
        path = (Path(__file__).resolve().parents[1] /
                ".local/deps/rt64/src/hle/rt64_framebuffer_manager.cpp")
        source = patch_source(path.name, path.read_bytes(), SOURCES[path.name],
                              probe=False).decode()
        self.assertIn("lround(fbTile.right * resolutionScale.x) - lround(fbTile.left * resolutionScale.x)", source)
        self.assertIn("if ((it.first & (uint64_t(1) << 63)) != 0) continue", source)
        for scale in (1.0, 4 / 3, 2.5, 3.75):
            widths = [round((column + 1) * 64 * scale) - round(column * 64 * scale)
                      for column in range(5)]
            self.assertEqual(sum(widths), round(320 * scale))
            heights = [round((row + 1) * 30 * scale) - round(row * 30 * scale)
                       for row in range(8)]
            self.assertEqual(sum(heights), round(240 * scale))

    def test_pause_snapshot_events_keep_oldest_until_exact_ack(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        context = (Path(__file__).resolve().parents[1] /
                   "src/graphics/rt64_context.cpp").read_text()
        queue = context.split("namespace {\n// The guest copy", 1)[1].split(
            "std::mutex ui_tag_mutex;", 1)[0]
        readers = context.split(
            'extern "C" bool tetrisphere_rt64_pause_snapshot_peek', 1)[1].split(
                "\nnamespace tetrisphere {", 1)[0]
        source = r'''#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <cstdio>
namespace {
// The guest copy''' + queue + '\n}\nextern "C" bool tetrisphere_rt64_pause_snapshot_peek' + readers + r'''
int main() {
    rescue_snapshot_events.push_back({1, 11, 101, 1});
    rescue_snapshot_events.push_back({2, 22, 102, 2});
    uint32_t source=0,destination=0;
    uint64_t workload=0,generation=0;
    const bool first=tetrisphere_rt64_pause_snapshot_peek(
        &source,&destination,&workload,&generation);
    const bool oldest=first && source==1 && destination==11 && workload==101 && generation==1;
    tetrisphere_rt64_pause_snapshot_ack(2);
    const bool outOfOrderRetained=rescue_snapshot_events.size()==2;
    tetrisphere_rt64_pause_snapshot_ack(1);
    const bool second=tetrisphere_rt64_pause_snapshot_peek(
        &source,&destination,&workload,&generation);
    const bool next=second && source==2 && destination==22 && workload==102 && generation==2;
    tetrisphere_rt64_pause_snapshot_ack(2);
    std::printf("%d %d %d %zu",oldest,outOfOrderRetained,next,
        rescue_snapshot_events.size());
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.cpp"
            exe = Path(directory) / "events"
            path.write_text(source)
            compiled = subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                                      capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        self.assertEqual(result.stdout, "1 1 1 0")
        self.assertIn('"action\\":\\"reject_newest', context)
        self.assertIn("while (true) {", (Path(__file__).resolve().parents[1] /
                                       "tools/build/patch_rt64_probe.py").read_text())


if __name__ == "__main__":
    unittest.main()
