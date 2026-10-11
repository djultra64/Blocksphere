"""Captured framebuffer provenance follows tiles through every transition layout."""
import re
import subprocess
import tempfile
import unittest
from pathlib import Path
from tools.build.rt64_pause_snapshot import PAUSE_SNAPSHOT_HELPER
ROOT=Path(__file__).resolve().parents[1]
class TransitionSnapshotTilesTests(unittest.TestCase):
 def test_transformed_and_partial_grids_preserve_source_regions(self):
  helper=PAUSE_SNAPSHOT_HELPER.split("    // A verified",1)[1].split("    // This runs on the wide render thread",1)[0]
  helper="    // A verified"+helper
  source=r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
constexpr int G_IM_FMT_RGBA=0,G_IM_SIZ_16b=2;
struct Rect {int ulx=0,uly=0,lrx=1280,lry=960;};
struct Vec {float x=0,y=0;};
struct DrawCallTile {Vec minTexcoord,maxTexcoord{64,30};int sampleWidth=505,sampleHeight=233;};
struct LoadOperation {
 enum class Type {Tile,Block};Type type=Type::Tile;
 struct Texture {uint32_t address=0x166f20;int width=320,fmt=0,siz=2;} texture;
 struct Tile {uint16_t uls=0,ult=0,lrs=252,lrt=116;} operationTile,tile;
};
struct Call {Rect rect;uint32_t tileCount=1,loadCount=1,tileIndex=0,loadIndex=0;};
struct GameCall {Call callDesc;};
struct Projection {enum class Type {Rectangle,Orthographic};Type type=Type::Rectangle;uint32_t gameCallCount=0;std::vector<GameCall> gameCalls;};
struct FramebufferPair {uint32_t projectionCount=1;Rect scissorRect;std::vector<Projection> projections{Projection{}};};
struct Workload {
 uint32_t fbPairCount=2;std::vector<FramebufferPair> fbPairs{FramebufferPair{},FramebufferPair{}};
 struct DrawData {std::vector<DrawCallTile> callTiles;std::vector<LoadOperation> loadOperations;} drawData;
};
''' + helper + r'''
    // A compact region index is useful to diagnostics and the original grid
    // regression. The actual renderer consumes every (tile, region) mapping.
    static bool tetrisphere_rescue_pause_grid(const Workload &workload,
                                              uint32_t destination,
                                              std::array<uint32_t, 40> &indices) {
        indices.fill(UINT32_MAX);
        std::vector<std::pair<uint32_t, uint32_t>> mappings;
        if (!tetrisphere_transition_tiles(workload, destination, mappings)) return false;
        for (const auto &entry : mappings) indices[entry.second] = entry.first;
        return true;
    }

int main() {
 Workload w;
 auto& proj=w.fbPairs[1].projections[0];proj.gameCallCount=40;
 for(unsigned i=0;i<40;++i) {
  Call c;c.tileIndex=c.loadIndex=i;
  unsigned col=i%5,row=i/5;
  c.rect={int(col*256),int(row*120),int(col==4 ? 1280:col*256+255),int(row==7 ? 960:row*120+119)};
  proj.gameCalls.push_back({c});
  LoadOperation load;load.operationTile={uint16_t(col*256),uint16_t(row*120),uint16_t(col*256+252),uint16_t(row*120+116)};load.tile=load.operationTile;
  w.drawData.loadOperations.push_back(load);w.drawData.callTiles.push_back({});
 }
 std::array<uint32_t,40> indices{};
 assert(tetrisphere_rescue_pause_grid(w,0x166f20,indices));
 for(unsigned i=0;i<40;++i)assert(indices[i]==i);
 // A slide/zoom/rotation changes geometry, not the captured source regions.
 for(auto& c:proj.gameCalls)c.callDesc.rect.ulx+=32;
 assert(tetrisphere_rescue_pause_grid(w,0x166f20,indices));
 // An effect may draw one captured region twice with separate tile records.
 w.drawData.callTiles.push_back({});w.drawData.loadOperations.push_back(w.drawData.loadOperations[0]);
 Call repeated=proj.gameCalls[0].callDesc;repeated.tileIndex=repeated.loadIndex=40;
 proj.gameCalls.push_back({repeated});proj.gameCallCount=41;
 assert(tetrisphere_rescue_pause_grid(w,0x166f20,indices));
 std::vector<std::pair<uint32_t,uint32_t>> mappings;
 assert(tetrisphere_transition_tiles(w,0x166f20,mappings) && mappings.size()==41);
 // Effects can place the image in the first framebuffer and an ortho projection.
 w.fbPairs[0]=w.fbPairs[1];w.fbPairCount=1;
 w.fbPairs[0].projections[0].type=Projection::Type::Orthographic;
 assert(tetrisphere_rescue_pause_grid(w,0x166f20,indices));
 // Only surviving tiles are sampled near the end of a transition.
 auto& partial=w.fbPairs[0].projections[0];partial.gameCallCount=5;partial.gameCalls.resize(5);
 assert(tetrisphere_rescue_pause_grid(w,0x166f20,indices));
 for(unsigned i=0;i<5;++i)assert(indices[i]==i);
 for(unsigned i=5;i<40;++i)assert(indices[i]==UINT32_MAX);
 for(auto& load:w.drawData.loadOperations)load.texture.address=0x222222;
 assert(!tetrisphere_rescue_pause_grid(w,0x166f20,indices));
}
'''
  with tempfile.TemporaryDirectory() as d:
   path=Path(d)/'tiles.cpp';exe=Path(d)/'tiles';path.write_text(source)
   subprocess.run(['c++','-std=c++17',str(path),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
