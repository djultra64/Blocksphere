#!/usr/bin/env python3
"""Copy pinned RT64 sources with VS aspect handling and optional diagnostics."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

if __package__:
    from .rt64_rescue_hud_probe import instrument_renderer
    from .rt64_rescue_hud_layout import RESCUE_HUD_HELPER
    from .rt64_pause_snapshot import PAUSE_SNAPSHOT_INCLUDE, PAUSE_SNAPSHOT_HELPER
    from .rt64_ui_tag_patch import patch_ui_tag_source
    from .rt64_ui_anchor_policy import UI_ANCHOR_HELPER
    from .rt64_m6_inspector import (APPLICATION_SHA256, STATE_SHA256,
                                   patch_application, patch_state)
else:
    from rt64_rescue_hud_probe import instrument_renderer
    from rt64_rescue_hud_layout import RESCUE_HUD_HELPER
    from rt64_pause_snapshot import PAUSE_SNAPSHOT_INCLUDE, PAUSE_SNAPSHOT_HELPER
    from rt64_ui_tag_patch import patch_ui_tag_source
    from rt64_ui_anchor_policy import UI_ANCHOR_HELPER
    from rt64_m6_inspector import (APPLICATION_SHA256, STATE_SHA256,
                                  patch_application, patch_state)


SOURCES = {
    "rt64_dynamic_libraries.cpp": "d7c56460ef2c9188fa9c1816d71b792b5e25f88e32f2e708fa31bcf5370eaabc",
    "rt64_application.cpp": APPLICATION_SHA256,
    "rt64_state.cpp": STATE_SHA256,
    "rt64_present_queue.cpp": "90df4e9c97136fa9662ee003393ad657fcb3e58cade6a24052012cf54a124c90",
    "rt64_workload_queue.cpp": "6e53030e8899c42347b51e67879fcb9679ad487ac067c192612c432d85bca4a4",
    "rt64_projection_processor.cpp": "cf04cb4da1bc39ab60cb24c92c0668385632ab7c6894157e6857c93252a39de9",
    "rt64_framebuffer_renderer.cpp": "73870c23b2340611b930b668d52aec9e687fd74b7da310962b7b1ef35dee015d",
    "rt64_framebuffer_manager.cpp": "1a97e98b34dc4707d4a9514ef6992bd751e5a0d6fe2c5bcefd50234b41686fd5",
    "rt64_interpreter.cpp": "cd91e76019205296b0ff9a52d4b477d45959243302c218c3c047d9328e66a7d2",
    "rt64_rdp.cpp": "288e186aa741a0f2ce8ff89a17d22b525fd017da81404ab709f5681c0666194c",
    "rt64_rsp.cpp": "7dfdf40254d44d92c247d9c876bb8ca55995927ad534981bd48868bb44f1f695",
}

VS_WIDE_HELPER = '''    // Tetrisphere VS uses two clipped 320x240 viewports, each centered on a player.
    // Both the projection and raster path must recognize the same exact signature.
    // Rect coordinates are quarter pixels; projection type 1 is Perspective and origin 2048 is None.
    static bool tetrisphere_vs_half_viewport(int fbLeft, int fbTop, int fbRight, int fbBottom,
                                              int clipLeft, int clipTop, int clipRight, int clipBottom,
                                              float viewportScaleX, float viewportTranslateX,
                                              unsigned viewportOrigin, unsigned projectionType,
                                              float aspectScale) {
        if ((projectionType != 1u) || (viewportOrigin != 2048u) ||
            (aspectScale <= 1.01f)) return false;
        if ((fbLeft != 0) || (fbTop != 0) || (fbRight != 1280) || (fbBottom != 960) ||
            (clipTop != 0) || (clipBottom != 960) ||
            (viewportScaleX < 159.5f) || (viewportScaleX > 160.5f)) return false;
        const bool left = (clipLeft == 0) && (clipRight == 640) &&
                          (viewportTranslateX > 79.5f) && (viewportTranslateX < 80.5f);
        const bool right = (clipLeft == 640) && (clipRight == 1280) &&
                           (viewportTranslateX > 239.5f) && (viewportTranslateX < 240.5f);
        return left || right;
    }

'''

VS_COLOR_FILL_HELPER = '''    // VS clears each player's color half separately. The depth clear is already full width.
    // The left clear ends at 639 quarter pixels, which rounds up to native pixel 160.
    static bool tetrisphere_vs_color_half_fill(int fbLeft, int fbTop, int fbRight, int fbBottom,
                                                int scissorLeft, int scissorTop,
                                                int scissorRight, int scissorBottom,
                                                int rectLeft, int rectTop, int rectRight, int rectBottom,
                                                unsigned leftOrigin, unsigned rightOrigin,
                                                bool colorTarget, float aspectScale) {
        if (!colorTarget || (aspectScale <= 1.01f) ||
            (leftOrigin != 2048u) || (rightOrigin != 2048u)) return false;
        if ((fbLeft != 0) || (fbTop != 0) || (fbRight != 1280) || (fbBottom != 960) ||
            (scissorLeft != 0) || (scissorTop != 0) ||
            (scissorRight != 1280) || (scissorBottom != 960) ||
            (rectTop != 0) || (rectBottom != 960)) return false;
        return ((rectLeft == 0) && (rectRight == 639)) ||
               ((rectLeft == 640) && (rectRight == 1280));
    }

'''

BACKGROUND_GRID_HELPER = '''    // The native background is forty 64x30 tiles, ordered by row. Verify all
    // forty before extending its edge columns; later calls belong to the UI.
    template <typename ProjectionT, typename RectT>
    static int tetrisphere_background_grid(const ProjectionT &proj, const RectT &fbScissor,
                                            float aspectScale) {
        if ((aspectScale <= 1.01f) || (static_cast<unsigned>(proj.type) != 3u) ||
            (proj.gameCallCount < 40) ||
            (fbScissor.ulx != 0) || (fbScissor.uly != 0) ||
            (fbScissor.lrx != 1280) || (fbScissor.lry != 960) ||
            (proj.scissorRect.ulx != 0) || (proj.scissorRect.uly != 0) ||
            (proj.scissorRect.lrx != 1280) || (proj.scissorRect.lry != 960)) return -1;
        int start = 0;
        if (proj.gameCalls[0].callDesc.otherMode.cycleType() == G_CYC_FILL) {
            const auto &clear = proj.gameCalls[0].callDesc;
            if ((clear.rect.ulx != 0) || (clear.rect.uly != 0) ||
                (clear.rect.lrx != 1280) || (clear.rect.lry != 960) ||
                (proj.gameCallCount < 41)) return -1;
            start = 1;
        }
        for (unsigned i = 0; i < 40; i++) {
            const auto &desc = proj.gameCalls[start + i].callDesc;
            const unsigned col = i % 5, row = i / 5;
            if ((desc.otherMode.cycleType() == G_CYC_FILL) ||
                (static_cast<unsigned>(desc.extendedType) != 0u) ||
                (desc.rectAspect != G_EX_ASPECT_AUTO) ||
                (desc.rectLeftOrigin != G_EX_ORIGIN_NONE) ||
                (desc.rectRightOrigin != G_EX_ORIGIN_NONE) ||
                (desc.scissorLeftOrigin != G_EX_ORIGIN_NONE) ||
                (desc.scissorRightOrigin != G_EX_ORIGIN_NONE) ||
                (desc.scissorRect.ulx != 0) || (desc.scissorRect.uly != 0) ||
                (desc.scissorRect.lrx != 1280) || (desc.scissorRect.lry != 960) ||
                (desc.rect.ulx != static_cast<int>(col * 256)) ||
                (desc.rect.uly != static_cast<int>(row * 120)) ||
                (desc.rect.lrx != static_cast<int>(col == 4 ? 1280 : col * 256 + 255)) ||
                (desc.rect.lry != static_cast<int>(row == 7 ? 960 : row * 120 + 119))) return -1;
        }
        return start;
    }

    // The title's backdrop uses a separate five-by-two image grid. The logo
    // and medallions are later calls/projections and must retain their scale.
    template <typename ProjectionT, typename RectT>
    static bool tetrisphere_title_grid(const ProjectionT &proj, const RectT &fbScissor,
                                       float aspectScale) {
        if ((aspectScale <= 1.01f) || (static_cast<unsigned>(proj.type) != 3u) ||
            (proj.gameCallCount != 10) ||
            (fbScissor.ulx != 0) || (fbScissor.uly != 0) ||
            (fbScissor.lrx != 1280) || (fbScissor.lry != 960) ||
            (proj.scissorRect.ulx != 0) || (proj.scissorRect.uly != 0) ||
            (proj.scissorRect.lrx != 1280) || (proj.scissorRect.lry != 960)) return false;
        for (unsigned i = 0; i < 10; i++) {
            const auto &desc = proj.gameCalls[i].callDesc;
            const unsigned col = i % 5, row = i / 5;
            if ((desc.otherMode.cycleType() == G_CYC_FILL) ||
                (static_cast<unsigned>(desc.extendedType) != 0u) ||
                (desc.rectAspect != G_EX_ASPECT_AUTO) ||
                (desc.rectLeftOrigin != G_EX_ORIGIN_NONE) ||
                (desc.rectRightOrigin != G_EX_ORIGIN_NONE) ||
                (desc.scissorLeftOrigin != G_EX_ORIGIN_NONE) ||
                (desc.scissorRightOrigin != G_EX_ORIGIN_NONE) ||
                (desc.scissorRect.ulx != 0) || (desc.scissorRect.uly != 0) ||
                (desc.scissorRect.lrx != 1280) || (desc.scissorRect.lry != 960) ||
                (desc.rect.ulx != static_cast<int>(col * 256)) ||
                (desc.rect.uly != static_cast<int>(row * 480)) ||
                (desc.rect.lrx != static_cast<int>(col == 4 ? 1280 : col * 256 + 255)) ||
                (desc.rect.lry != static_cast<int>(row == 1 ? 960 : 479))) return false;
        }
        return true;
    }

    static void tetrisphere_extend_background_tile(unsigned callIndex, float targetWidth,
                                                    float &left, float &right) {
        if ((callIndex % 5) == 0) left = 0.0f;
        if ((callIndex % 5) == 4) right = targetWidth;
    }

'''

PROBE_INCLUDE = '#include <chrono>\n#include <cstdio>\n#include <cstdlib>\n\n'
ASPECT_PROBE_INCLUDE = '#include <atomic>\n#include <cstdio>\n#include <cstdlib>\n\n'
ASPECT_PROBE_HELPER = '''    // ID zero is RT64's synchronous fullSync/readback path before advanceWorkload assigns an ID.
    // Select presented workloads so every projection and viewport decision in one sample is retained.
    static bool tetrisphere_aspect_probe_enabled() {
        static const bool enabled = []() {
            const char *value = std::getenv("TETRISPHERE_RT64_PROBE");
            return (value != nullptr) && (value[0] == '1') && (value[1] == '\\0');
        }();
        return enabled;
    }

    static bool tetrisphere_aspect_probe_sample(uint64_t workloadId,
                                                std::atomic<uint64_t> &emitted,
                                                const char *kind) {
        if ((workloadId == 0) || ((workloadId % 60) != 0)) return false;
        const uint64_t slot = emitted.fetch_add(1, std::memory_order_relaxed);
        if (slot < 16384) return true;
        if (slot == 16384) {
            std::fprintf(stderr,
                "{\\"event\\":\\"rt64_probe_aspect_%s_truncated\\",\\"workload_id\\":%llu,\\"limit\\":16384}\\n",
                kind, static_cast<unsigned long long>(workloadId));
            std::fflush(stderr);
        }
        return false;
    }

'''
PROBE_ENABLED = '''static const bool tetrisphere_probe_enabled = []() {
            const char *value = std::getenv("TETRISPHERE_RT64_PROBE");
            return (value != nullptr) && (value[0] == '1') && (value[1] == '\\0');
        }();'''

PRESENT_PROBE = '''
                // Diagnostic only: report the target that actually reached swapchain present.
                if (tetrisphere_probe_enabled) {
                    static unsigned long long probePresents = 0;
                    static const auto probeStart = std::chrono::steady_clock::now();
                    const auto probeSequence = ++probePresents;
                    if ((probeSequence <= 8) || (probeSequence % 60 == 0)) {
                        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - probeStart).count();
                        std::fprintf(stderr,
                            "{\\"event\\":\\"rt64_probe_present\\",\\"sequence\\":%llu,"
                            "\\"elapsed_ms\\":%lld,"
                            "\\"color_width\\":%u,\\"color_height\\":%u,"
                            "\\"texture_width\\":%u,\\"texture_height\\":%u,"
                            "\\"swapchain_width\\":%u,\\"swapchain_height\\":%u,"
                            "\\"frame_index\\":%d,\\"frames_to_present\\":%d,\\"present_ok\\":%s}\\n",
                            probeSequence, static_cast<long long>(elapsedMs),
                            probeColorWidth, probeColorHeight,
                            probeTextureWidth, probeTextureHeight,
                            probeSwapWidth, probeSwapHeight,
                            i, framesToPresent, swapChainValid ? "true" : "false");
                        std::fflush(stderr);
                    }
                }'''

WORKLOAD_PROBE = '''
                // Diagnostic only: count actual renders, including interpolated frames.
                if (tetrisphere_probe_enabled) {
                    static unsigned long long probeWorkloads = 0;
                    static unsigned long long probeRenderedTotal = 0;
                    static unsigned long long probeInterpolatedTotal = 0;
                    static const auto probeStart = std::chrono::steady_clock::now();
                    const auto probeSequence = ++probeWorkloads;
                    probeRenderedTotal += framesRendered;
                    probeInterpolatedTotal += interpolatedRendered;
                    if ((probeSequence <= 8) || (probeSequence % 60 == 0)) {
                        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - probeStart).count();
                        std::fprintf(stderr,
                            "{\\"event\\":\\"rt64_probe_workload\\",\\"sequence\\":%llu,"
                            "\\"elapsed_ms\\":%lld,\\"rendered_total\\":%llu,"
                            "\\"interpolated_total\\":%llu,"
                            "\\"vi_original_rate\\":%u,\\"target_rate\\":%u,"
                            "\\"display_frames\\":%u,\\"frames_rendered\\":%u,"
                            "\\"interpolated_rendered\\":%u,\\"interpolation_enabled\\":%s,"
                            "\\"interpolation_target_available\\":%s,\\"paused\\":%s,"
                            "\\"skipped_frames\\":%s}\\n",
                            probeSequence, static_cast<long long>(elapsedMs),
                            probeRenderedTotal, probeInterpolatedTotal,
                            workload.viOriginalRate, workloadConfig.targetRate,
                            displayFrames, framesRendered,
                            interpolatedRendered,
                            generateInterpolatedFrames ? "true" : "false",
                            interpolationTargetKey.isEmpty() ? "false" : "true",
                            workload.paused ? "true" : "false",
                            skippedFrames ? "true" : "false");
                        std::fflush(stderr);
                    }
                }
'''

PROJECTION_PROBE = '''
            // Observe RT64's actual projection choice; do not alter its heuristic.
            if (tetrisphere_aspect_probe_enabled()) {
                static std::atomic<uint64_t> probeEmitted{0};
                if (tetrisphere_aspect_probe_sample(workload.workloadId, probeEmitted, "projection")) {
                    FixedRect probeIntersection = proj.scissorRect;
                    FixedRect probeViewportRect(0, 0, 0, 0);
                    const interop::RSPViewport *probeViewport = nullptr;
                    const int16_t *probeClip = nullptr;
                    if (proj.usesViewport()) {
                        probeViewport = &drawData.rspViewports[proj.transformsIndex];
                        probeClip = &drawData.viewportClipRatios[proj.transformsIndex * 4];
                        probeViewportRect = probeViewport->rect(probeClip);
                        probeIntersection = probeIntersection.intersection(probeViewportRect);
                    }
                    const bool probeCoversWholeWidth = !probeIntersection.isEmpty() &&
                        (probeIntersection.ulx <= fbPair.scissorRect.ulx) &&
                        (probeIntersection.lrx >= fbPair.scissorRect.lrx);
                    const bool probeHorizontalRatio = !probeIntersection.isEmpty() &&
                        (probeIntersection.width(true, true) > probeIntersection.height(true, true));
                    std::fprintf(stderr,
                        "{\\"event\\":\\"rt64_probe_aspect_projection\\","
                        "\\"workload_id\\":%llu,\\"submission_frame\\":%llu,"
                        "\\"workload_index\\":%u,\\"fb_pair_index\\":%u,"
                        "\\"projection_index\\":%u,\\"transforms_index\\":%u,"
                        "\\"scene_index\\":%llu,\\"projection_type\\":%u,"
                        "\\"game_call_count\\":%u,"
                        "\\"aspect_mode\\":%u,\\"viewport_origin\\":%u,"
                        "\\"uses_viewport\\":%s,\\"covers_whole_width\\":%s,"
                        "\\"horizontal_ratio\\":%s,\\"adjust_aspect_ratio\\":%s,"
                        "\\"aspect_ratio_scale\\":%.6f,\\"projection_ratio_scale\\":%.6f,"
                        "\\"scissor_ulx\\":%d,\\"scissor_uly\\":%d,"
                        "\\"scissor_lrx\\":%d,\\"scissor_lry\\":%d,"
                        "\\"fb_scissor_ulx\\":%d,\\"fb_scissor_uly\\":%d,"
                        "\\"fb_scissor_lrx\\":%d,\\"fb_scissor_lry\\":%d,"
                        "\\"viewport_ulx\\":%d,\\"viewport_uly\\":%d,"
                        "\\"viewport_lrx\\":%d,\\"viewport_lry\\":%d,"
                        "\\"viewport_scale_x\\":%.6f,\\"viewport_scale_y\\":%.6f,\\"viewport_scale_z\\":%.6f,"
                        "\\"viewport_translate_x\\":%.6f,\\"viewport_translate_y\\":%.6f,\\"viewport_translate_z\\":%.6f,"
                        "\\"viewport_clip_0\\":%d,\\"viewport_clip_1\\":%d,"
                        "\\"viewport_clip_2\\":%d,\\"viewport_clip_3\\":%d,"
                        "\\"intersection_ulx\\":%d,\\"intersection_uly\\":%d,"
                        "\\"intersection_lrx\\":%d,\\"intersection_lry\\":%d}\\n",
                        static_cast<unsigned long long>(workload.workloadId),
                        static_cast<unsigned long long>(workload.submissionFrame),
                        sceneProj.workloadIndex, sceneProj.fbPairIndex,
                        sceneProj.projectionIndex, proj.transformsIndex,
                        static_cast<unsigned long long>(sceneIndex),
                        static_cast<unsigned>(proj.type),
                        proj.gameCallCount,
                        static_cast<unsigned>(curProjGroup.aspectMode),
                        static_cast<unsigned>(viewportOrigin),
                        proj.usesViewport() ? "true" : "false",
                        probeCoversWholeWidth ? "true" : "false",
                        probeHorizontalRatio ? "true" : "false",
                        adjustAspectRatio ? "true" : "false",
                        p.aspectRatioScale, projRatioScale,
                        proj.scissorRect.ulx, proj.scissorRect.uly,
                        proj.scissorRect.lrx, proj.scissorRect.lry,
                        fbPair.scissorRect.ulx, fbPair.scissorRect.uly,
                        fbPair.scissorRect.lrx, fbPair.scissorRect.lry,
                        probeViewportRect.ulx, probeViewportRect.uly,
                        probeViewportRect.lrx, probeViewportRect.lry,
                        probeViewport != nullptr ? probeViewport->scale.x : 0.0f,
                        probeViewport != nullptr ? probeViewport->scale.y : 0.0f,
                        probeViewport != nullptr ? probeViewport->scale.z : 0.0f,
                        probeViewport != nullptr ? probeViewport->translate.x : 0.0f,
                        probeViewport != nullptr ? probeViewport->translate.y : 0.0f,
                        probeViewport != nullptr ? probeViewport->translate.z : 0.0f,
                        probeClip != nullptr ? probeClip[0] : 0,
                        probeClip != nullptr ? probeClip[1] : 0,
                        probeClip != nullptr ? probeClip[2] : 0,
                        probeClip != nullptr ? probeClip[3] : 0,
                        probeIntersection.ulx, probeIntersection.uly,
                        probeIntersection.lrx, probeIntersection.lry);
                    std::fflush(stderr);
                }
            }
'''

DRAW_PROBE = '''
                // This records the viewport decision before the draw loop; projections without viewports do not reach here.
                if (tetrisphere_aspect_probe_enabled()) {
                    static std::atomic<uint64_t> probeEmitted{0};
                    if (tetrisphere_aspect_probe_sample(p.curWorkload->workloadId, probeEmitted, "viewport_decision")) {
                        const FixedRect probeViewportRect = viewport.rect(viewportClipRatios);
                        std::fprintf(stderr,
                            "{\\"event\\":\\"rt64_probe_aspect_viewport_decision\\","
                            "\\"workload_id\\":%llu,\\"submission_frame\\":%llu,"
                            "\\"draw_submission_frame\\":%llu,\\"fb_pair_index\\":%u,"
                            "\\"projection_index\\":%u,\\"transforms_index\\":%u,"
                            "\\"projection_type\\":%u,\\"game_call_count\\":%u,"
                            "\\"viewport_origin\\":%u,\\"covers_whole_width\\":%s,"
                            "\\"horizontal_ratio\\":%s,\\"use_wide_viewport\\":%s,"
                            "\\"adjust_ratio\\":%s,\\"aspect_ratio_scale\\":%.6f,"
                            "\\"aspect_source\\":%.6f,\\"aspect_target\\":%.6f,"
                            "\\"scissor_ratio\\":%.6f,\\"projection_inverse_scale\\":%.6f,"
                            "\\"screen_scale_x\\":%.6f,\\"wide_width\\":%.2f,"
                            "\\"original_width\\":%.2f,\\"target_width\\":%u,"
                            "\\"target_height\\":%u,"
                            "\\"scissor_ulx\\":%d,\\"scissor_uly\\":%d,"
                            "\\"scissor_lrx\\":%d,\\"scissor_lry\\":%d,"
                            "\\"fb_scissor_ulx\\":%d,\\"fb_scissor_uly\\":%d,"
                            "\\"fb_scissor_lrx\\":%d,\\"fb_scissor_lry\\":%d,"
                            "\\"viewport_ulx\\":%d,\\"viewport_uly\\":%d,"
                            "\\"viewport_lrx\\":%d,\\"viewport_lry\\":%d,"
                            "\\"viewport_scale_x\\":%.6f,\\"viewport_scale_y\\":%.6f,\\"viewport_scale_z\\":%.6f,"
                            "\\"viewport_translate_x\\":%.6f,\\"viewport_translate_y\\":%.6f,\\"viewport_translate_z\\":%.6f,"
                            "\\"viewport_clip_0\\":%d,\\"viewport_clip_1\\":%d,"
                            "\\"viewport_clip_2\\":%d,\\"viewport_clip_3\\":%d,"
                            "\\"intersection_ulx\\":%d,\\"intersection_uly\\":%d,"
                            "\\"intersection_lrx\\":%d,\\"intersection_lry\\":%d}\\n",
                            static_cast<unsigned long long>(p.curWorkload->workloadId),
                            static_cast<unsigned long long>(p.curWorkload->submissionFrame),
                            static_cast<unsigned long long>(p.submissionFrame),
                            p.fbPairIndex, pr, proj.transformsIndex,
                            static_cast<unsigned>(proj.type), proj.gameCallCount,
                            static_cast<unsigned>(viewportOrigin),
                            coversWholeWidth ? "true" : "false",
                            horizontalRatio ? "true" : "false",
                            useWideViewport ? "true" : "false",
                            adjustRatio ? "true" : "false",
                            aspectRatioScale, p.aspectRatioSource, p.aspectRatioTarget,
                            scissorRatio, projInvRatioScale, triangles.screenScale.x,
                            wideWidth, originalWidth, p.targetWidth, p.targetHeight,
                            proj.scissorRect.ulx, proj.scissorRect.uly,
                            proj.scissorRect.lrx, proj.scissorRect.lry,
                            fbPair.scissorRect.ulx, fbPair.scissorRect.uly,
                            fbPair.scissorRect.lrx, fbPair.scissorRect.lry,
                            probeViewportRect.ulx, probeViewportRect.uly,
                            probeViewportRect.lrx, probeViewportRect.lry,
                            viewport.scale.x, viewport.scale.y, viewport.scale.z,
                            viewport.translate.x, viewport.translate.y, viewport.translate.z,
                            viewportClipRatios[0], viewportClipRatios[1],
                            viewportClipRatios[2], viewportClipRatios[3],
                            intersectionRect.ulx, intersectionRect.uly,
                            intersectionRect.lrx, intersectionRect.lry);
                        std::fflush(stderr);
                    }
                }
'''

FILL_RECT_PROBE = '''
                    // Observe each sampled clear after RT64 chooses its final physical rectangle.
                    if (tetrisphere_aspect_probe_enabled()) {
                        static std::atomic<uint64_t> probeRectEmitted{0};
                        if (tetrisphere_aspect_probe_sample(p.curWorkload->workloadId, probeRectEmitted, "rect_call")) {
                            std::fprintf(stderr,
                                "{\\"event\\":\\"rt64_probe_aspect_rect_call\\",\\"kind\\":\\"fill\\","
                                "\\"workload_id\\":%llu,\\"fb_pair_index\\":%u,\\"projection_index\\":%u,"
                                "\\"projection_type\\":%u,\\"call_index\\":%u,\\"fill_color_target\\":%s,"
                                "\\"color_image_address\\":%u,\\"depth_image_address\\":%u,"
                                "\\"fill_color\\":%u,\\"inv_ratio_scale\\":%.6f,"
                                "\\"rect_ulx\\":%d,\\"rect_uly\\":%d,\\"rect_lrx\\":%d,\\"rect_lry\\":%d,"
                                "\\"scissor_ulx\\":%d,\\"scissor_uly\\":%d,\\"scissor_lrx\\":%d,\\"scissor_lry\\":%d,"
                                "\\"fb_scissor_ulx\\":%d,\\"fb_scissor_uly\\":%d,"
                                "\\"fb_scissor_lrx\\":%d,\\"fb_scissor_lry\\":%d,"
                                "\\"rect_aspect\\":%u,\\"left_origin\\":%u,\\"right_origin\\":%u,"
                                "\\"render_left\\":%d,\\"render_top\\":%d,"
                                "\\"render_right\\":%d,\\"render_bottom\\":%d}\\n",
                                static_cast<unsigned long long>(p.curWorkload->workloadId),
                                p.fbPairIndex, pr, static_cast<unsigned>(proj.type), d,
                                p.fbStorage->colorTarget != nullptr ? "true" : "false",
                                fbPair.colorImage.address, fbPair.depthImage.address,
                                call.callDesc.fillColor, invRatioScale,
                                call.callDesc.rect.ulx, call.callDesc.rect.uly,
                                call.callDesc.rect.lrx, call.callDesc.rect.lry,
                                call.callDesc.scissorRect.ulx, call.callDesc.scissorRect.uly,
                                call.callDesc.scissorRect.lrx, call.callDesc.scissorRect.lry,
                                fbPair.scissorRect.ulx, fbPair.scissorRect.uly,
                                fbPair.scissorRect.lrx, fbPair.scissorRect.lry,
                                static_cast<unsigned>(call.callDesc.rectAspect),
                                static_cast<unsigned>(call.callDesc.rectLeftOrigin),
                                static_cast<unsigned>(call.callDesc.rectRightOrigin),
                                clearRect.rect.left, clearRect.rect.top,
                                clearRect.rect.right, clearRect.rect.bottom);
                            std::fflush(stderr);
                        }
                    }
'''

REGULAR_RECT_PROBE = '''
                            // Rectangles may be backgrounds or HUD; record their actual placement.
                            if (tetrisphere_aspect_probe_enabled()) {
                                static std::atomic<uint64_t> probeRectEmitted{0};
                                if (tetrisphere_aspect_probe_sample(p.curWorkload->workloadId, probeRectEmitted, "rect_call")) {
                                    std::fprintf(stderr,
                                        "{\\"event\\":\\"rt64_probe_aspect_rect_call\\",\\"kind\\":\\"regular\\","
                                        "\\"workload_id\\":%llu,\\"fb_pair_index\\":%u,\\"projection_index\\":%u,"
                                        "\\"projection_type\\":%u,\\"call_index\\":%u,\\"fill_color_target\\":%s,"
                                        "\\"color_image_address\\":%u,\\"depth_image_address\\":%u,"
                                        "\\"inv_ratio_scale\\":%.6f,\\"rect_ulx\\":%d,\\"rect_uly\\":%d,"
                                        "\\"rect_lrx\\":%d,\\"rect_lry\\":%d,"
                                        "\\"scissor_ulx\\":%d,\\"scissor_uly\\":%d,"
                                        "\\"scissor_lrx\\":%d,\\"scissor_lry\\":%d,"
                                        "\\"fb_scissor_ulx\\":%d,\\"fb_scissor_uly\\":%d,"
                                        "\\"fb_scissor_lrx\\":%d,\\"fb_scissor_lry\\":%d,"
                                        "\\"rect_aspect\\":%u,\\"left_origin\\":%u,\\"right_origin\\":%u,"
                                        "\\"render_left\\":%.2f,\\"render_top\\":%.2f,"
                                        "\\"render_right\\":%.2f,\\"render_bottom\\":%.2f}\\n",
                                        static_cast<unsigned long long>(p.curWorkload->workloadId),
                                        p.fbPairIndex, pr, static_cast<unsigned>(proj.type), d,
                                        p.fbStorage->colorTarget != nullptr ? "true" : "false",
                                        fbPair.colorImage.address, fbPair.depthImage.address,
                                        invRatioScale,
                                        call.callDesc.rect.ulx, call.callDesc.rect.uly,
                                        call.callDesc.rect.lrx, call.callDesc.rect.lry,
                                        call.callDesc.scissorRect.ulx, call.callDesc.scissorRect.uly,
                                        call.callDesc.scissorRect.lrx, call.callDesc.scissorRect.lry,
                                        fbPair.scissorRect.ulx, fbPair.scissorRect.uly,
                                        fbPair.scissorRect.lrx, fbPair.scissorRect.lry,
                                        static_cast<unsigned>(call.callDesc.rectAspect),
                                        static_cast<unsigned>(call.callDesc.rectLeftOrigin),
                                        static_cast<unsigned>(call.callDesc.rectRightOrigin),
                                        viewportRect.x, viewportRect.y,
                                        viewportRect.x + viewportRect.width,
                                        viewportRect.y + viewportRect.height);
                                    std::fflush(stderr);
                                }
                            }
'''


def replace_once(source: str, anchor: str, replacement: str) -> str:
    count = source.count(anchor)
    if count != 1:
        raise ValueError(f"RT64 probe anchor count is {count}, expected 1: {anchor[:80]!r}")
    return source.replace(anchor, replacement, 1)


def patch_source(name: str, raw: bytes, expected_sha256: str, probe: bool = True) -> bytes:
    actual = hashlib.sha256(raw).hexdigest()
    if actual != expected_sha256:
        raise ValueError(f"RT64 {name} SHA256 mismatch: {actual}")
    source = raw.decode("utf-8")
    if name == "rt64_dynamic_libraries.cpp":
        source = replace_once(source, 'WCHAR modulePath[FILENAME_MAX];',
            'WCHAR modulePath[32768];')
        source = replace_once(source,
            'GetModuleFileNameW(moduleHandle, modulePath, sizeof(modulePath))',
            'GetModuleFileNameW(moduleHandle, modulePath, 32768)')
        source = replace_once(source, 'modulePathSz >= sizeof(modulePath)',
            'modulePathSz >= 32768')
        source = replace_once(source, 'fullPath.remove_filename();',
            'fullPath.remove_filename();\n        fullPath /= L"lib";')
        source = replace_once(source,
            'fullPathStr + win32::Utf8ToUtf16(pair.first)',
            '(fullPath / win32::Utf8ToUtf16(pair.first)).wstring()')
        source = replace_once(source, 'LoadLibraryW(libraryPath.c_str())',
            'LoadLibraryExW(libraryPath.c_str(), nullptr, '
            'LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)')
        return source.encode("utf-8")
    if name == "rt64_application.cpp":
        return patch_application(raw)
    if name == "rt64_state.cpp":
        return patch_state(raw)
    if name in ("rt64_interpreter.cpp", "rt64_rdp.cpp", "rt64_rsp.cpp"):
        return patch_ui_tag_source(name, source).encode("utf-8")
    if name in ("rt64_projection_processor.cpp", "rt64_framebuffer_renderer.cpp"):
        vs_namespace_anchor = ("namespace RT64 {\n    // Helper functions.\n"
                               if name == "rt64_framebuffer_renderer.cpp" else "namespace RT64 {\n")
        vs_helpers = VS_WIDE_HELPER + (VS_COLOR_FILL_HELPER + BACKGROUND_GRID_HELPER + UI_ANCHOR_HELPER if name == "rt64_framebuffer_renderer.cpp" else "")
        source = replace_once(source, vs_namespace_anchor, vs_namespace_anchor + vs_helpers)
        if name == "rt64_projection_processor.cpp":
            anchor = "            float projRatioScale = adjustAspectRatio ? (1.0f / p.aspectRatioScale) : 1.0f;"
            source = replace_once(source, anchor, '''            if (proj.usesViewport()) {
                const auto &vsViewport = drawData.rspViewports[proj.transformsIndex];
                if (tetrisphere_vs_half_viewport(
                    fbPair.scissorRect.ulx, fbPair.scissorRect.uly,
                    fbPair.scissorRect.lrx, fbPair.scissorRect.lry,
                    proj.scissorRect.ulx, proj.scissorRect.uly,
                    proj.scissorRect.lrx, proj.scissorRect.lry,
                    vsViewport.scale.x, vsViewport.translate.x, viewportOrigin,
                    static_cast<unsigned>(proj.type), p.aspectRatioScale)) {
                    adjustAspectRatio = true;
                }
            }
''' + anchor)
        else:
            anchor = "        const float originalWidth = p.fbWidth * p.resolutionScale.y;"
            anchor = "            const Projection &proj = fbPair.projections[pr];"
            source = replace_once(source, anchor, anchor + '''
            const int backgroundGridStart = tetrisphere_background_grid(
                proj, fbPair.scissorRect, aspectRatioScale);
            const bool backgroundGrid = (backgroundGridStart >= 0) &&
                (globalCallIndex + backgroundGridStart + 40 <= p.maxGameCall);
            const bool titleGrid = (globalCallIndex + 10 <= p.maxGameCall) &&
                tetrisphere_title_grid(proj, fbPair.scissorRect, aspectRatioScale);''')
            anchor = "                if (useWideViewport) {"
            source = replace_once(source, anchor, '''                if (tetrisphere_vs_half_viewport(
                    fbPair.scissorRect.ulx, fbPair.scissorRect.uly,
                    fbPair.scissorRect.lrx, fbPair.scissorRect.lry,
                    proj.scissorRect.ulx, proj.scissorRect.uly,
                    proj.scissorRect.lrx, proj.scissorRect.lry,
                    viewport.scale.x, viewport.translate.x, viewportOrigin,
                    static_cast<unsigned>(proj.type), aspectRatioScale)) {
                    useWideViewport = true;
                }
''' + anchor)
            anchor = "            for (uint32_t d = 0; (d < proj.gameCallCount) && (globalCallIndex < p.maxGameCall); d++) {"
            source = replace_once(source, anchor, '''            const float baseUiScreenOffsetX = triangles.screenOffset.x;
''' + anchor)
            anchor = "                const GameCall &call = proj.gameCalls[d];"
            source = replace_once(source, anchor, anchor + '''
                if (proj.type == Projection::Type::Orthographic ||
                    proj.type == Projection::Type::Perspective) {
                    tetrisphere_ui_anchor_ortho_for_call(
                        triangles, baseUiScreenOffsetX, call.callDesc.uid,
                        wideWidth, originalWidth, aspectRatioScale, halfViewportSize.x);
                }''')
            anchor = "                    clearRect.rect = convertFixedRect(call.callDesc.rect, p.resolutionScale, p.fbWidth, invRatioScale, extOriginPercentage, horizontalMisalignment, call.callDesc.rectLeftOrigin, call.callDesc.rectRightOrigin);"
            source = replace_once(source, anchor, '''                    if (tetrisphere_vs_color_half_fill(
                        fbPair.scissorRect.ulx, fbPair.scissorRect.uly,
                        fbPair.scissorRect.lrx, fbPair.scissorRect.lry,
                        call.callDesc.scissorRect.ulx, call.callDesc.scissorRect.uly,
                        call.callDesc.scissorRect.lrx, call.callDesc.scissorRect.lry,
                        call.callDesc.rect.ulx, call.callDesc.rect.uly,
                        call.callDesc.rect.lrx, call.callDesc.rect.lry,
                        call.callDesc.rectLeftOrigin, call.callDesc.rectRightOrigin,
                        p.fbStorage->colorTarget != nullptr, aspectRatioScale)) {
                        invRatioScale = 1.0f;
                        horizontalMisalignment = 0;
                    }
''' + anchor)
            source = replace_once(source, anchor, anchor + '''
                    const float uiFillShift = tetrisphere_ui_anchor_shift_pixels(
                        call.callDesc.uid, wideWidth, originalWidth, aspectRatioScale);
                    clearRect.rect.left += uiFillShift;
                    clearRect.rect.right += uiFillShift;''')
            anchor = "                            triangles.screenScale = { viewportRect.width / framebuffer.viewport.width, viewportRect.height / framebuffer.viewport.height };"
            source = replace_once(source, anchor, '''                            const bool extendGrid =
                                (backgroundGrid && (d >= unsigned(backgroundGridStart)) &&
                                 (d < unsigned(backgroundGridStart + 40))) ||
                                (titleGrid && (d < 10));
                            if (extendGrid) {
                                float left = viewportRect.x;
                                float right = viewportRect.x + viewportRect.width;
                                const unsigned gridIndex = titleGrid ? d : d - backgroundGridStart;
                                tetrisphere_extend_background_tile(gridIndex, float(p.targetWidth), left, right);
                                viewportRect.x = left;
                                viewportRect.width = right - left;
                            }
                            if (proj.type == Projection::Type::Rectangle) {
                                tetrisphere_ui_anchor_rect_for_call(
                                    viewportRect, viewportRect.x, call.callDesc.uid,
                                    wideWidth, originalWidth, aspectRatioScale);
                            }
''' + anchor)
            anchor = "                            triangles.scissor = viewportScissorIntersection(viewportClip, triangles.scissor);\n                        }"
            source = replace_once(source, anchor, anchor + '''
                        if (proj.type == Projection::Type::Perspective ||
                            proj.type == Projection::Type::Orthographic ||
                            proj.type == Projection::Type::Rectangle) {
                            const auto baseUiScissor = triangles.scissor;
                            tetrisphere_ui_anchor_scissor_for_call(
                                triangles.scissor, baseUiScissor, call.callDesc.uid,
                                wideWidth, originalWidth, aspectRatioScale,
                                float(p.targetWidth));
                        }''')
            anchor = "                        bool usesViewport = (proj.type == Projection::Type::Perspective) || (proj.type == Projection::Type::Orthographic);"
            source = replace_once(source, anchor, '''                        if ((backgroundGrid && (d >= unsigned(backgroundGridStart)) &&
                             (d < unsigned(backgroundGridStart + 40))) ||
                            (titleGrid && (d < 10))) {
                            triangles.scissor.left = 0;
                            triangles.scissor.right = p.targetWidth;
                        }
''' + anchor)
    elif name == "rt64_framebuffer_manager.cpp":
        source = replace_once(source,
            "            newId = std::max(it.first, newId);\n\n            TileCopy &tileCopy = it.second;",
            "            // High-bit IDs belong to the frozen Rescue pause snapshot.\n"
            "            // Never recycle them or include them in ordinary allocation.\n"
            "            if ((it.first & (uint64_t(1) << 63)) != 0) continue;\n"
            "            newId = std::max(it.first, newId);\n\n            TileCopy &tileCopy = it.second;")
        anchor = "        const uint32_t tileWidth = std::clamp<long>(lround((fbTile.right - fbTile.left) * resolutionScale.x), 1L, RenderTarget::MaxDimension);"
        source = replace_once(source, anchor, '''        const bool rescueSnapshot = (op.createTileCopy.id & (uint64_t(1) << 63)) != 0;
        const uint32_t tileWidth = rescueSnapshot ?
            std::clamp<long>(lround(fbTile.right * resolutionScale.x) - lround(fbTile.left * resolutionScale.x), 1L, RenderTarget::MaxDimension) :
            std::clamp<long>(lround((fbTile.right - fbTile.left) * resolutionScale.x), 1L, RenderTarget::MaxDimension);''')
        anchor = "        const uint32_t tileHeight = std::clamp<long>(lround((fbTile.bottom - fbTile.top) * resolutionScale.y), 1L, RenderTarget::MaxDimension);"
        source = replace_once(source, anchor, '''        const uint32_t tileHeight = rescueSnapshot ?
            std::clamp<long>(lround(fbTile.bottom * resolutionScale.y) - lround(fbTile.top * resolutionScale.y), 1L, RenderTarget::MaxDimension) :
            std::clamp<long>(lround((fbTile.bottom - fbTile.top) * resolutionScale.y), 1L, RenderTarget::MaxDimension);''')
    elif name == "rt64_workload_queue.cpp":
        source = replace_once(source, "#include \"rt64_workload_queue.h\"\n",
                              "#include \"rt64_workload_queue.h\"\n"
                              "#include \"tetrisphere/full_frame_target_height.h\"\n\n"
                              + PAUSE_SNAPSHOT_INCLUDE)
        source = replace_once(source, "namespace RT64 {\n", "namespace RT64 {\n" + PAUSE_SNAPSHOT_HELPER)
        source = replace_once(source,
            "                    nativeColorHeight = fbPair.drawColorRect.bottom(true);",
            '''                    nativeColorHeight = fbPair.drawColorRect.bottom(true);
                    bool fullPerspectiveScissor = false;
                    for (uint32_t p = 0; p < fbPair.projectionCount; p++) {
                        const Projection &projection = fbPair.projections[p];
                        if (projection.type != Projection::Type::Perspective) continue;
                        const FixedRect &rect = projection.scissorRect;
                        if (rect.ulx == 0 && rect.uly == 0 &&
                            rect.lrx == int32_t(workload.viFbSize[0] * 4) &&
                            rect.lry == int32_t(workload.viFbSize[1] * 4)) {
                            fullPerspectiveScissor = true;
                            break;
                        }
                    }
                    nativeColorHeight = tetrisphere::full_frame_target_height(
                        nativeColorHeight, workload.viFbSize[0], workload.viFbSize[1],
                        colorImg.width, fullPerspectiveScissor);''')
        anchor = "            Workload &workload = workloads[curFrame.workloads[w]];"
        source = replace_once(source, anchor, anchor + r'''
            auto &pauseSnapshot = tetrisphere_pause_snapshot();
            auto tryRescueSnapshot = [&](bool afterSourceDraw) {
                while (true) {
                uint32_t source = 0, destination = 0;
                uint64_t sourceWorkload = 0, generation = 0;
                if (!tetrisphere_rt64_pause_snapshot_peek(&source, &destination,
                                                          &sourceWorkload, &generation)) return;
                if (sourceWorkload == 0) {
                    tetrisphere_rt64_pause_snapshot_ack(generation);
                    continue;
                }
                if (afterSourceDraw ? (workload.workloadId != sourceWorkload) :
                                      (workload.workloadId <= sourceWorkload)) return;
                if (const char *disabled = std::getenv("TETRISPHERE_DISABLE_RESCUE_SNAPSHOT");
                    disabled != nullptr && disabled[0] == '1' && disabled[1] == '\0') {
                    tetrisphere_rt64_pause_snapshot_ack(generation);
                    continue;
                }
                const auto writer = pauseSnapshot.sourceLastWriter.find(source);
                const bool sourceMatches = writer != pauseSnapshot.sourceLastWriter.end() &&
                                           writer->second == sourceWorkload;
                const auto rescue = pauseSnapshot.sourceLastRescue.find(source);
                const bool sourceIsRescue = rescue != pauseSnapshot.sourceLastRescue.end() && rescue->second;
                bool captured = false;
                if (sourceMatches && workloadConfig.aspectRatioScale > 1.01f) {
                    workerMutex.lock();
                    captured = tetrisphere_capture_rescue_pause(*this, fbManager,
                        targetManager, ext.workloadGraphicsWorker,
                        ext.shaderLibrary, ext.textureCache, workload,
                        source, destination, generation);
                    workerMutex.unlock();
                }
                if (!captured) tetrisphere_invalidate_pause_snapshot();
                tetrisphere_rt64_pause_snapshot_ack(generation);
                if (const char *probe = std::getenv("TETRISPHERE_RT64_PROBE");
                    probe != nullptr && probe[0] == '1' && probe[1] == '\0') {
                    std::fprintf(stderr,
                        "{\"event\":\"rt64_probe_rescue_snapshot_capture\","
                        "\"source\":%u,\"destination\":%u,"
                        "\"source_workload\":%llu,\"render_workload\":%llu,"
                        "\"generation\":%llu,\"writer_match\":%s,\"source_rescue\":%s,\"captured\":%s}\n",
                        source, destination,
                        static_cast<unsigned long long>(sourceWorkload),
                        static_cast<unsigned long long>(workload.workloadId),
                        static_cast<unsigned long long>(generation),
                        sourceMatches ? "true" : "false", sourceIsRescue ? "true" : "false",
                        captured ? "true" : "false");
                }
                }
            };
            tryRescueSnapshot(false);''')
        anchor = "            // Add all framebuffer pairs to the framebuffer renderer and setup the operations."
        source = replace_once(source, anchor, '''            // Rectangle placement and GPU tiles must use the same snapshot.
            // Native State has already consumed the original RAM tiles.
            tetrisphere_apply_pause_snapshot(workload, fbManager,
                                              workloadConfig.aspectRatioScale);

''' + anchor)
        anchor = "            workerMutex.unlock();\n\n            // Update the GPU profiler"
        source = replace_once(source, anchor, '''            workerMutex.unlock();

            // Record which wide target this completed workload actually wrote.
            for (uint32_t f = 0; f < workload.fbPairCount; f++) {
                const auto &pair = workload.fbPairs[f];
                if (!pair.drawColorRect.isEmpty()) {
                    const uint32_t address = pair.colorImage.address;
                    auto &lastWriter = pauseSnapshot.sourceLastWriter[address];
                    if (lastWriter != workload.workloadId) {
                        lastWriter = workload.workloadId;
                        pauseSnapshot.sourceLastRescue[address] = false;
                    }
                    pauseSnapshot.sourceLastRescue[address] |=
                        tetrisphere_rescue_tagged_title(pair);
                }
            }
            tryRescueSnapshot(true);
            tetrisphere_retire_pause_tiles(fbManager, workload.workloadId);

            // Update the GPU profiler''')
    elif not probe:
        raise ValueError(f"normal RT64 copy does not need {name}")
    if not probe:
        return source.encode("utf-8")
    if name in ("rt64_projection_processor.cpp", "rt64_framebuffer_renderer.cpp"):
        namespace_anchor = ("namespace RT64 {\n    // Helper functions.\n"
                            if name == "rt64_framebuffer_renderer.cpp" else "namespace RT64 {\n")
        source = replace_once(source, namespace_anchor,
                              ASPECT_PROBE_INCLUDE + namespace_anchor + ASPECT_PROBE_HELPER)
    else:
        source = replace_once(source, "namespace RT64 {\n", PROBE_INCLUDE + "namespace RT64 {\n")
    if name == "rt64_present_queue.cpp":
        source = replace_once(
            source,
            "    void PresentQueue::threadPresent(const Present &present, bool &swapChainValid) {\n",
            "    void PresentQueue::threadPresent(const Present &present, bool &swapChainValid) {\n"
            "        " + PROBE_ENABLED + "\n",
        )
        anchor = "            uint32_t frameCountersNextPresented = 0;"
        source = replace_once(source, anchor, anchor + '''
            uint32_t probeColorWidth = 0, probeColorHeight = 0;
            uint32_t probeTextureWidth = 0, probeTextureHeight = 0;
            uint32_t probeSwapWidth = 0, probeSwapHeight = 0;''')
        anchor = "                commandList->setFramebuffer(swapChainFramebuffer);"
        source = replace_once(source, anchor, '''                // Snapshot while the render target is still protected by workloadMutex.
                if (tetrisphere_probe_enabled) {
                    probeColorWidth = colorTarget != nullptr ? colorTarget->width : 0u;
                    probeColorHeight = colorTarget != nullptr ? colorTarget->height : 0u;
                    probeTextureWidth = renderParams.textureWidth;
                    probeTextureHeight = renderParams.textureHeight;
                    probeSwapWidth = ext.swapChain->getWidth();
                    probeSwapHeight = ext.swapChain->getHeight();
                }
''' + anchor)
        anchor = "                swapChainValid = ext.swapChain->present(swapChainIndex, &waitSemaphore, 1);"
        source = replace_once(source, anchor, anchor + PRESENT_PROBE)
    elif name == "rt64_workload_queue.cpp":
        source = replace_once(
            source,
            "    void WorkloadQueue::renderThreadLoop() {\n",
            "    void WorkloadQueue::renderThreadLoop() {\n" + "        " + PROBE_ENABLED + "\n",
        )
        anchor = "                uint32_t framesRendered = 0;"
        source = replace_once(source, anchor, anchor + "\n                uint32_t interpolatedRendered = 0;")
        anchor = "                        interpolationTargetKey, interpolationTargetFbPairIndex, overrideTarget, overrideModifier, velocityUploaderUsed, uploadExtras, tileInterpolationUsed, lookAtInterpolationUsed);"
        source = replace_once(source, anchor, anchor + '''
                    if (generateInterpolatedFrames && (curFrameWeight > 0.0f) && (curFrameWeight < 1.0f)) {
                        interpolatedRendered++;
                    }''')
        anchor = "                threadConfigurationValidate();"
        source = replace_once(source, anchor, WORKLOAD_PROBE + anchor)
    elif name == "rt64_projection_processor.cpp":
        anchor = "            float projRatioScale = adjustAspectRatio ? (1.0f / p.aspectRatioScale) : 1.0f;"
        source = replace_once(source, anchor, anchor + PROJECTION_PROBE)
    elif name == "rt64_framebuffer_renderer.cpp":
        anchor = "                const GameCall &call = proj.gameCalls[d];"
        source = replace_once(source, anchor, anchor + r'''
                if (call.callDesc.uid == 1 &&
                    proj.type == Projection::Type::Perspective) {
                    static std::atomic<unsigned> nonRectUiLogged{0};
                    if (nonRectUiLogged.fetch_add(1, std::memory_order_relaxed) < 32)
                        std::fprintf(stderr,
                            "{\"event\":\"rt64_probe_ui_nonrect\",\"workload_id\":%llu,\"pair\":%u,\"projection\":%u,\"type\":%u,\"uid\":%u}\n",
                            static_cast<unsigned long long>(p.curWorkload ?
                                p.curWorkload->workloadId : 0), p.fbPairIndex, pr,
                            static_cast<unsigned>(proj.type), call.callDesc.uid);
                }
                if (call.callDesc.uid != 0) {
                    static std::atomic<unsigned> uiTagLogged{0};
                    if (uiTagLogged.fetch_add(1, std::memory_order_relaxed) < 64) {
                        std::fprintf(stderr,
                            "{\"event\":\"rt64_probe_ui_tag_draw\","
                            "\"workload_id\":%llu,\"pair\":%u,"
                            "\"projection\":%u,\"call\":%u,\"uid\":%u,"
                            "\"rect\":[%d,%d,%d,%d]}\n",
                            static_cast<unsigned long long>(p.curWorkload ?
                                p.curWorkload->workloadId : 0), p.fbPairIndex, pr, d,
                            call.callDesc.uid,
                            call.callDesc.rect.ulx, call.callDesc.rect.uly,
                            call.callDesc.rect.lrx, call.callDesc.rect.lry);
                    }
                }
''')
        anchor = "                viewportClip = convertViewportRect(viewport.rect(viewportClipRatios), p.resolutionScale, p.fbWidth, projInvRatioScale, extOriginPercentage, 0.0f, viewportOrigin, viewportOrigin);"
        source = replace_once(source, anchor, anchor + DRAW_PROBE)
        anchor = "                    clearRect.rect = convertFixedRect(call.callDesc.rect, p.resolutionScale, p.fbWidth, invRatioScale, extOriginPercentage, horizontalMisalignment, call.callDesc.rectLeftOrigin, call.callDesc.rectRightOrigin);"
        source = replace_once(source, anchor, anchor + FILL_RECT_PROBE)
        anchor = "                            triangles.screenOffset.y = halfPixelOffset.y + (halfViewportSize.y - (viewportRect.y + viewportRect.height / 2.0f)) / halfViewportSize.y;"
        source = replace_once(source, anchor, anchor + REGULAR_RECT_PROBE)
        source = instrument_renderer(source)
    elif name == "rt64_framebuffer_manager.cpp":
        pass
    else:
        raise ValueError(f"unsupported RT64 probe source: {name}")
    return source.encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rt64-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--probe", action="store_true", help="include opt-in diagnostic events")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    names = SOURCES if args.probe else {name: SOURCES[name] for name in
                                      ("rt64_application.cpp", "rt64_state.cpp",
                                       "rt64_projection_processor.cpp", "rt64_framebuffer_renderer.cpp",
                                       "rt64_framebuffer_manager.cpp", "rt64_workload_queue.cpp",
                                       "rt64_interpreter.cpp", "rt64_rdp.cpp", "rt64_rsp.cpp", "rt64_dynamic_libraries.cpp")}
    for name, expected in names.items():
        directory = "common" if name == "rt64_dynamic_libraries.cpp" else ("render" if name in ("rt64_projection_processor.cpp", "rt64_framebuffer_renderer.cpp") else "hle")
        source = args.rt64_root / "src" / directory / name
        patched = patch_source(name, source.read_bytes(), expected, probe=args.probe)
        destination = args.output / name
        if not destination.exists() or destination.read_bytes() != patched:
            destination.write_bytes(patched)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
