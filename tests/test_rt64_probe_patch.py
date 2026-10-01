"""Fail-closed checks for the opt-in RT64 source-copy probe."""

import hashlib
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.build.patch_rt64_probe import (ASPECT_PROBE_HELPER, SOURCES,
                                         VS_COLOR_FILL_HELPER, VS_WIDE_HELPER,
                                         BACKGROUND_GRID_HELPER, patch_source)


class Rt64ProbePatchTests(unittest.TestCase):
    def test_windows_dynamic_libraries_load_from_absolute_lib_directory(self):
        root = Path(__file__).resolve().parents[1] / '.local/deps/rt64'
        source = root / 'src/common/rt64_dynamic_libraries.cpp'
        if not source.is_file():
            self.skipTest('Pinned RT64 source unavailable')
        raw = source.read_bytes()
        patched = patch_source(source.name, raw, hashlib.sha256(raw).hexdigest(), probe=False).decode()
        self.assertIn('fullPath /= L"lib";', patched)
        self.assertIn('GetModuleFileNameW(moduleHandle, modulePath, 32768)', patched)
        self.assertNotIn('sizeof(modulePath)', patched)
        self.assertIn('LoadLibraryExW(libraryPath.c_str()', patched)
        self.assertIn('LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS', patched)
        self.assertNotIn('LoadLibraryW(libraryPath.c_str())', patched)

    def test_background_grid_requires_complete_exact_5_by_8_projection(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        source = '''#include <cstdio>
struct Rect { int ulx, uly, lrx, lry; };
constexpr unsigned G_CYC_FILL=3, G_EX_ASPECT_AUTO=0, G_EX_ORIGIN_NONE=2048;
struct Mode { unsigned cycle=0; unsigned cycleType() const { return cycle; } };
struct Desc { Rect rect, scissorRect; Mode otherMode; unsigned rectLeftOrigin=2048, rectRightOrigin=2048;
    unsigned scissorLeftOrigin=2048, scissorRightOrigin=2048, rectAspect=0;
    unsigned extendedType=0; };
struct Call { Desc callDesc; };
struct Projection { unsigned type=3, gameCallCount=40; Rect scissorRect{0,0,1280,960};
    Call gameCalls[42]; };
namespace RT64 {
''' + BACKGROUND_GRID_HELPER + '''
int run() {
    Projection p;
    for (unsigned i=0;i<40;i++) {
        int col=i%5, row=i/5;
        p.gameCalls[i].callDesc.rect={col*256,row*120,col==4?1280:col*256+255,row==7?960:row*120+119};
        p.gameCalls[i].callDesc.scissorRect={0,0,1280,960};
    }
    Rect fb{0,0,1280,960};
    auto good=tetrisphere_background_grid(p,fb,1.33333f);
    p.gameCallCount=39; auto shortGrid=tetrisphere_background_grid(p,fb,1.33333f);
    p.gameCallCount=41; auto withHud=tetrisphere_background_grid(p,fb,1.33333f);
    p.gameCalls[20].callDesc.rect.lrx=254; auto changed=tetrisphere_background_grid(p,fb,1.33333f);
    p.gameCalls[20].callDesc.rect.lrx=255;
    auto standard=tetrisphere_background_grid(p,fb,1.0f);
    p.gameCalls[0].callDesc.scissorRect.lrx=640; auto clipped=tetrisphere_background_grid(p,fb,1.33333f);
    p.gameCalls[0].callDesc.scissorRect.lrx=1280;
    p.gameCalls[0].callDesc.rectLeftOrigin=0; auto moved=tetrisphere_background_grid(p,fb,1.33333f);
    p.gameCalls[0].callDesc.rectLeftOrigin=2048;
    for (int i=39;i>=0;i--) p.gameCalls[i+1]=p.gameCalls[i];
    p.gameCalls[0].callDesc.otherMode.cycle=G_CYC_FILL;
    p.gameCalls[0].callDesc.rect={0,0,1280,960};
    p.gameCallCount=41;
    auto prefixed=tetrisphere_background_grid(p,fb,1.33333f);
    p.gameCalls[0].callDesc.rect.lrx=1279;
    auto badClear=tetrisphere_background_grid(p,fb,1.33333f);
    Projection title;
    title.gameCallCount=10;
    for (unsigned i=0;i<10;i++) {
        int col=i%5, row=i/5;
        title.gameCalls[i].callDesc.rect={col*256,row*480,col==4?1280:col*256+255,row==1?960:479};
        title.gameCalls[i].callDesc.scissorRect={0,0,1280,960};
    }
    auto titleGood=tetrisphere_title_grid(title,fb,1.33333f);
    title.gameCalls[6].callDesc.rect.lry=959;
    auto titleChanged=tetrisphere_title_grid(title,fb,1.33333f);
    float left=480,right=1056;
    tetrisphere_extend_background_tile(0,3840,left,right);
    float firstLeft=left, firstRight=right;
    left=2784;right=3360;
    tetrisphere_extend_background_tile(4,3840,left,right);
    float centerLeft=1632,centerRight=2208;
    tetrisphere_extend_background_tile(2,3840,centerLeft,centerRight);
    std::printf("%d %d %d %d %d %d %d %d %d %d %d %.0f %.0f %.0f %.0f %.0f %.0f",good,shortGrid,withHud,changed,standard,clipped,moved,prefixed,badClear,titleGood,titleChanged,
        firstLeft,firstRight,left,right,centerLeft,centerRight);
    return 0;
}}
int main(){return RT64::run();}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "grid.cpp"
            exe = Path(directory) / "grid"
            path.write_text(source)
            subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                           check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        self.assertEqual(result.stdout, "0 -1 0 -1 -1 -1 -1 1 -1 1 0 0 1056 2784 3840 1632 2208")

    def test_vs_half_signature_accepts_only_player_perspective_viewports(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        source = ("#include <cstdio>\nnamespace RT64 {\n" + VS_WIDE_HELPER +
                  "int run() { "
                  "const auto f = [](int left, int right, float sx, float tx, unsigned type, float ratio) { "
                  "return tetrisphere_vs_half_viewport(0, 0, 1280, 960, left, 0, right, 960, "
                  "sx, tx, 2048, type, ratio); }; "
                  "std::printf(\"%d%d%d%d%d%d%d%d%d%d\", "
                  "f(0,640,160,80,1,1.33333f), f(640,1280,160,240,1,1.33333f), "
                  "f(0,640,160,80,2,1.33333f), f(0,640,160,80,3,1.33333f), "
                  "f(0,1280,160,160,1,1.33333f), f(0,640,160,240,1,1.33333f), "
                  "f(0,640,80,80,1,1.33333f), f(0,640,160,80,1,1.0f), "
                  "tetrisphere_vs_half_viewport(0,0,1280,960,0,0,639,960,160,80,2048,1,1.33333f), "
                  "tetrisphere_vs_half_viewport(0,0,1280,960,0,0,640,960,160,80,0,1,1.33333f)); "
                  "return 0; } }\nint main() { return RT64::run(); }\n")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "signature.cpp"
            exe = Path(directory) / "signature"
            path.write_text(source)
            subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                           check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        self.assertEqual(result.stdout, "1100000000")

    def test_vs_color_fill_signature_excludes_depth_and_hud(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        source = ("#include <cstdio>\nnamespace RT64 {\n" + VS_COLOR_FILL_HELPER +
                  "int run() { const auto f = [](int l, int r, bool color, int y, float ratio) { "
                  "return tetrisphere_vs_color_half_fill(0,0,1280,960,0,0,1280,960,l,y,r,960,"
                  "2048,2048,color,ratio); }; "
                  "std::printf(\"%d%d%d%d%d%d%d%d%d\","
                  "f(0,639,true,0,1.33333f), f(640,1280,true,0,1.33333f),"
                  "f(0,639,false,0,1.33333f), f(0,1280,true,0,1.33333f),"
                  "f(0,640,true,0,1.33333f), f(0,639,true,4,1.33333f),"
                  "f(0,639,true,0,1.0f),"
                  "tetrisphere_vs_color_half_fill(0,0,1280,960,0,0,1280,960,0,0,639,960,0,2048,true,1.33333f),"
                  "tetrisphere_vs_color_half_fill(0,0,1280,960,0,0,640,960,0,0,639,960,2048,2048,true,1.33333f));"
                  "return 0; } }\nint main() { return RT64::run(); }\n")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fill.cpp"
            exe = Path(directory) / "fill"
            path.write_text(source)
            subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                           check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        self.assertEqual(result.stdout, "110000000")

    def test_normal_and_probe_copies_apply_same_vs_decision(self):
        rt64 = Path(__file__).resolve().parents[1] / ".local/deps/rt64/src/render"
        for name in ("rt64_projection_processor.cpp", "rt64_framebuffer_renderer.cpp"):
            raw = (rt64 / name).read_bytes()
            normal = patch_source(name, raw, SOURCES[name], probe=False).decode()
            diagnostic = patch_source(name, raw, SOURCES[name], probe=True).decode()
            self.assertIn("tetrisphere_vs_half_viewport(", normal)
            self.assertIn("tetrisphere_vs_half_viewport(", diagnostic)
            if name == "rt64_framebuffer_renderer.cpp":
                self.assertIn("tetrisphere_vs_color_half_fill(", normal)
                self.assertIn("tetrisphere_vs_color_half_fill(", diagnostic)
                self.assertIn("p.fbStorage->colorTarget != nullptr", normal)
                for patched in (normal, diagnostic):
                    self.assertIn("tetrisphere_background_grid(\n                proj, fbPair.scissorRect, aspectRatioScale)", patched)
                    self.assertIn("tetrisphere_title_grid(proj, fbPair.scissorRect, aspectRatioScale)", patched)
                    self.assertIn("tetrisphere_extend_background_tile(gridIndex, float(p.targetWidth), left, right)", patched)
                    self.assertIn("triangles.scissor.right = p.targetWidth", patched)
            self.assertNotIn("rt64_probe_aspect_", normal)
            self.assertIn("rt64_probe_aspect_", diagnostic)

    def test_aspect_sources_are_pinned_and_emit_bounded_decision_records(self):
        rt64 = Path(__file__).resolve().parents[1] / ".local/deps/rt64/src/render"
        expected = {
            "rt64_projection_processor.cpp": (
                "rt64_probe_aspect_projection", "adjustAspectRatio",
                "workload.workloadId", "sceneProj.fbPairIndex",
                "sceneProj.projectionIndex", "proj.transformsIndex"),
            "rt64_framebuffer_renderer.cpp": (
                "rt64_probe_aspect_viewport_decision", "useWideViewport",
                "p.curWorkload->workloadId", "p.fbPairIndex",
                "proj.transformsIndex", "p.submissionFrame",
                "rt64_probe_aspect_rect_call", "fill_color_target",
                "rect_aspect", "render_left", "render_right"),
        }
        for name, terms in expected.items():
            with self.subTest(name=name):
                source = (rt64 / name).read_bytes()
                patched = patch_source(name, source, SOURCES[name]).decode()
                self.assertEqual(hashlib.sha256(source).hexdigest(), SOURCES[name])
                for term in terms:
                    self.assertIn(term, patched)
                self.assertIn("TETRISPHERE_RT64_PROBE", patched)
                self.assertIn("_truncated", patched)
                self.assertIn("workloadId", patched)
                self.assertIn("% 60", patched)
                self.assertIn("game_call_count", patched)
                for coord in ("scissor_ulx", "scissor_uly", "scissor_lrx", "scissor_lry",
                              "fb_scissor_ulx", "fb_scissor_uly", "fb_scissor_lrx", "fb_scissor_lry",
                              "viewport_ulx", "viewport_uly", "viewport_lrx", "viewport_lry",
                              "intersection_ulx", "intersection_uly", "intersection_lrx", "intersection_lry"):
                    self.assertIn(coord, patched)
                for field in ("viewport_scale_x", "viewport_scale_y", "viewport_scale_z",
                              "viewport_translate_x", "viewport_translate_y", "viewport_translate_z",
                              "viewport_clip_0", "viewport_clip_1", "viewport_clip_2", "viewport_clip_3"):
                    self.assertIn(field, patched)
                with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
                    patch_source(name, source, "0" * 64)

    def test_two_projections_in_one_selected_workload_are_both_sampled(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        source = ("#include <atomic>\n#include <cstdint>\n#include <cstdio>\n"
                  "#include <cstdlib>\nnamespace RT64 {\n" + ASPECT_PROBE_HELPER +
                  "int run() { std::atomic<uint64_t> emitted{0}; "
                  "const uint64_t ids[] = {0, 59, 60, 60, 61, 120}; "
                  "for (uint64_t id : ids) std::printf(\"%d\", "
                  "tetrisphere_aspect_probe_sample(id, emitted, \"projection\")); "
                  "std::atomic<uint64_t> viewportEmitted{0}; int menu = 0, board = 0; "
                  "for (uint64_t w = 1; w <= 670; ++w) for (int p = 0; p < 8; ++p) { "
                  "bool include = tetrisphere_aspect_probe_sample(w * 60, viewportEmitted, "
                  "\"viewport_decision\"); if (w <= 610) menu += include; else board += include; } "
                  "std::printf(\" %d %d\", menu, board); "
                  "std::atomic<uint64_t> bounded{0}; int accepted = 0; "
                  "for (uint64_t w = 1; w <= 2050; ++w) for (int p = 0; p < 8; ++p) "
                  "accepted += tetrisphere_aspect_probe_sample(w * 60, bounded, "
                  "\"viewport_decision\"); std::printf(\" %d\", accepted); "
                  "return 0; } }\nint main() { return RT64::run(); }\n")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "probe.cpp"
            exe = Path(directory) / "probe"
            path.write_text(source)
            subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                           check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
        self.assertEqual(result.stdout, "001101 4880 480 16384")
        self.assertEqual(result.stderr.count("rt64_probe_aspect_viewport_decision_truncated"), 1)

    def test_present_patch_requires_exact_source_and_single_anchor(self):
        raw = (b'namespace RT64 {\n'
               b'    void PresentQueue::threadPresent(const Present &present, bool &swapChainValid) {\n'
               b'            uint32_t frameCountersNextPresented = 0;\n'
               b'                commandList->setFramebuffer(swapChainFramebuffer);\n'
               b'                swapChainValid = ext.swapChain->present(swapChainIndex, &waitSemaphore, 1);\n')
        digest = hashlib.sha256(raw).hexdigest()
        patched = patch_source("rt64_present_queue.cpp", raw, digest)
        self.assertIn(b'rt64_probe_present', patched)
        self.assertIn(b'TETRISPHERE_RT64_PROBE', patched)
        with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
            patch_source("rt64_present_queue.cpp", raw, "0" * 64)
        duplicate = raw + b'                swapChainValid = ext.swapChain->present(swapChainIndex, &waitSemaphore, 1);\n'
        with self.assertRaisesRegex(ValueError, "anchor count is 2"):
            patch_source("rt64_present_queue.cpp", duplicate, hashlib.sha256(duplicate).hexdigest())

    def test_workload_patch_is_bounded_to_observed_function(self):
        path = (Path(__file__).resolve().parents[1] /
                ".local/deps/rt64/src/hle/rt64_workload_queue.cpp")
        raw = path.read_bytes()
        patched = patch_source(path.name, raw, SOURCES[path.name])
        self.assertIn(b'rt64_probe_workload', patched)
        self.assertIn(b'framesRendered', patched)
        self.assertIn(b'tetrisphere_rescue_pause_grid', patched)
        self.assertLess(patched.index(b'tetrisphere_apply_pause_snapshot(workload'),
                        patched.index(b'framebufferRenderer->addFramebuffer(drawParams)'),
                        'pause tiles must change before RT64 chooses rectangle geometry')
        with self.assertRaisesRegex(ValueError, "anchor count is 0"):
            patch_source("rt64_workload_queue.cpp", b'namespace RT64 {\n',
                         hashlib.sha256(b'namespace RT64 {\n').hexdigest())


if __name__ == "__main__":
    unittest.main()
