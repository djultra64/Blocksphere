"""Optional, bounded RT64 draw trace for locating Rescue HUD and sphere calls.

The caller must first validate the pinned upstream RT64 source SHA. This
transform only adds diagnostics and never changes a rendering decision.
"""

INCLUDE_ANCHOR = "namespace RT64 {\n    // Helper functions."
DRAW_ANCHOR = "                // Determine to use the draw call either in the RT scene or the raster scene."

INCLUDES = "#include <atomic>\n#include <cstdio>\n#include <cstdlib>\n#include <sstream>\n\n"

HELPER = r'''    // Trace one out of 60 presented workloads, only after the game switches
    // to a wide 320x240 board. Every draw in a selected workload is kept.
    template <typename ParamsT, typename PairT, typename ProjectionT,
              typename CallT, typename DrawT, typename DataT>
    static void tetrisphere_rescue_hud_probe_emit(
        const ParamsT &p, const PairT &pair, const ProjectionT &proj,
        const CallT &call, const DrawT &draw, const DataT &data,
        unsigned projectionIndex, unsigned localCallIndex, float aspectScale) {
        static const bool enabled = [] {
            const char *v = std::getenv("TETRISPHERE_RT64_RESCUE_PROBE");
            return v && (v[0] == '1') && (v[1] == '\0');
        }();
        if (!enabled || !p.curWorkload || !p.fbStorage ||
            (p.curWorkload->workloadId == 0) ||
            ((p.curWorkload->workloadId % 60) != 0) ||
            (aspectScale <= 1.01f) ||
            (pair.scissorRect.ulx != 0) || (pair.scissorRect.uly != 0) ||
            (pair.scissorRect.lrx != 1280) || (pair.scissorRect.lry != 960)) return;

        static std::atomic<unsigned> emitted{0};
        const unsigned slot = emitted.fetch_add(1, std::memory_order_relaxed);
        if (slot >= 8192) {
            if (slot == 8192) {
                std::fprintf(stderr,
                    "{\"event\":\"rt64_probe_rescue_draw_truncated\",\"limit\":8192}\n");
            }
            return;
        }

        const auto &desc = call.callDesc;
        const auto &viewport = data.rspViewports[proj.transformsIndex];
        const bool fill = (desc.otherMode.cycleType() == G_CYC_FILL);
        const uint64_t tileHash = (desc.tileCount &&
            (desc.tileIndex < data.callTiles.size())) ?
            data.callTiles[desc.tileIndex].tmemHashOrID : 0;
        const bool tileCopyUsed = (desc.tileCount &&
            (desc.tileIndex < data.callTiles.size())) ?
            data.callTiles[desc.tileIndex].tileCopyUsed : false;
        const bool tileSyncRequired = (desc.tileCount &&
            (desc.tileIndex < data.callTiles.size())) ?
            data.callTiles[desc.tileIndex].syncRequired : false;
        const bool hasLoad = desc.loadCount &&
            (desc.loadIndex < data.loadOperations.size());
        const uint32_t loadAddress = hasLoad ?
            data.loadOperations[desc.loadIndex].texture.address : 0;
        const uint32_t loadWidth = hasLoad ?
            data.loadOperations[desc.loadIndex].texture.width : 0;
        std::ostringstream line;
        line << "{\"event\":\"rt64_probe_rescue_draw\""
             << ",\"workload_id\":" << p.curWorkload->workloadId
             << ",\"submission_frame\":" << p.submissionFrame
             << ",\"fb_pair_index\":" << p.fbPairIndex
             << ",\"projection_index\":" << projectionIndex
             << ",\"projection_type\":" << static_cast<unsigned>(proj.type)
             << ",\"transforms_index\":" << proj.transformsIndex
             << ",\"local_call_index\":" << localCallIndex
             << ",\"uid\":" << desc.uid
             << ",\"call_index\":" << desc.callIndex
             << ",\"triangle_count\":" << desc.triangleCount
             << ",\"min_world_matrix\":" << desc.minWorldMatrix
             << ",\"max_world_matrix\":" << desc.maxWorldMatrix
             << ",\"draw_type\":" << static_cast<unsigned>(draw.type)
             << ",\"cycle_type\":" << desc.otherMode.cycleType()
             << ",\"texture_on\":" << static_cast<unsigned>(desc.textureOn)
             << ",\"tile_count\":" << desc.tileCount
             << ",\"tile_hash\":" << tileHash
             << ",\"tile_copy_used\":" << (tileCopyUsed ? "true" : "false")
             << ",\"tile_sync_required\":" << (tileSyncRequired ? "true" : "false")
             << ",\"load_count\":" << desc.loadCount
             << ",\"load_texture_address\":" << loadAddress
             << ",\"load_texture_width\":" << loadWidth
             << ",\"fb_color_address\":" << pair.colorImage.address
             << ",\"color_target\":" << (p.fbStorage->colorTarget ? "true" : "false")
             << ",\"rect\":[" << desc.rect.ulx << ',' << desc.rect.uly
             << ',' << desc.rect.lrx << ',' << desc.rect.lry << ']'
             << ",\"scissor\":[" << desc.scissorRect.ulx << ',' << desc.scissorRect.uly
             << ',' << desc.scissorRect.lrx << ',' << desc.scissorRect.lry << ']'
             << ",\"viewport_scale\":[" << viewport.scale.x << ',' << viewport.scale.y << ']'
             << ",\"viewport_translate\":[" << viewport.translate.x << ','
             << viewport.translate.y << ']';
        if (fill) {
            const auto &rect = draw.clearRect.rect;
            line << ",\"render_rect\":[" << rect.left << ',' << rect.top
                 << ',' << rect.right << ',' << rect.bottom << ']';
        }
        else {
            line << ",\"screen_scale\":[" << draw.triangles.screenScale.x << ','
                 << draw.triangles.screenScale.y << ']'
                 << ",\"screen_offset\":[" << draw.triangles.screenOffset.x << ','
                 << draw.triangles.screenOffset.y << ']'
                 << ",\"render_scissor\":[" << draw.triangles.scissor.left << ','
                 << draw.triangles.scissor.top << ',' << draw.triangles.scissor.right
                 << ',' << draw.triangles.scissor.bottom << ']';
        }
        line << '}';
        std::fprintf(stderr, "%s\n", line.str().c_str());
    }

'''

DRAW_CALL = '''                tetrisphere_rescue_hud_probe_emit(
                    p, fbPair, proj, call, instanceDrawCall, drawData,
                    pr, d, aspectRatioScale);

'''


def _replace_once(source: str, anchor: str, replacement: str) -> str:
    if source.count(anchor) != 1:
        raise ValueError(f"RT64 Rescue probe anchor count {source.count(anchor)}: {anchor}")
    return source.replace(anchor, replacement, 1)


def instrument_renderer(source: str) -> str:
    """Add opt-in Rescue draw records to an already SHA-validated renderer."""
    source = _replace_once(source, INCLUDE_ANCHOR, INCLUDES + INCLUDE_ANCHOR + "\n" + HELPER)
    return _replace_once(source, DRAW_ANCHOR, DRAW_CALL + DRAW_ANCHOR)
