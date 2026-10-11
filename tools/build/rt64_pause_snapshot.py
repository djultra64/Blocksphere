"""Scoped RT64 wide-frame capture for verified 5x8 transition images."""

PAUSE_SNAPSHOT_INCLUDE = '''#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <unordered_map>
'''

PAUSE_SNAPSHOT_HELPER = r'''    extern "C" bool tetrisphere_rt64_pause_snapshot_peek(uint32_t *, uint32_t *, uint64_t *, uint64_t *);
    extern "C" void tetrisphere_rt64_pause_snapshot_ack(uint64_t);

    struct TetrispherePauseSnapshot {
        uint32_t destination = 0;
        uint64_t generation = 0;
        std::array<uint64_t, 40> tileIds{};
        std::unordered_map<uint64_t, std::vector<std::pair<uint32_t, DrawCallTile>>> originalTiles;
        std::unordered_map<uint32_t, uint64_t> sourceLastWriter;
        std::unordered_map<uint32_t, bool> sourceLastRescue;
        struct Retired {
            uint64_t lastUsedWorkload;
            std::array<uint64_t, 40> tileIds;
        };
        std::deque<Retired> retired;
        uint64_t lastUsedWorkload = 0;
        bool valid = false;
    };

    static TetrispherePauseSnapshot &tetrisphere_pause_snapshot() {
        static TetrispherePauseSnapshot snapshot;
        return snapshot;
    }

    static void tetrisphere_invalidate_pause_snapshot() {
        auto &snapshot = tetrisphere_pause_snapshot();
        if (snapshot.valid)
            snapshot.retired.push_back({snapshot.lastUsedWorkload, snapshot.tileIds});
        snapshot.valid = false;
    }

    // Retain Rescue tagging for diagnostics. Eligibility follows the recorded
    // source writer and framebuffer provenance, including menu captures.
    static bool tetrisphere_rescue_tagged_title(const FramebufferPair &pair) {
        for (uint32_t p = 0; p < pair.projectionCount; p++) {
            const auto &projection = pair.projections[p];
            if (projection.type != Projection::Type::Rectangle) continue;
            for (uint32_t c = 0; c < projection.gameCallCount; c++) {
                const auto &call = projection.gameCalls[c].callDesc;
                if (call.uid == 1 && call.rect.ulx == 108 &&
                    call.rect.uly == 72 && call.rect.lrx == 387 &&
                    call.rect.lry == 183) return true;
            }
        }
        return false;
    }

    // A verified 320x240 framebuffer capture is consumed as 64x30 RGBA16
    // regions. Transitions slide, zoom, rotate, reorder and hide those regions;
    // identify the source loads, never their changing screen geometry or mode.
    static bool tetrisphere_transition_tiles(const Workload &workload,
                                              uint32_t destination,
                                              std::vector<std::pair<uint32_t, uint32_t>> &indices) {
        indices.clear();
        bool matched = false;
        for (uint32_t f = 0; f < workload.fbPairCount; f++) {
            const auto &pair = workload.fbPairs[f];
            for (uint32_t p = 0; p < pair.projectionCount; p++) {
                const auto &proj = pair.projections[p];
                for (uint32_t c = 0; c < proj.gameCallCount; c++) {
                    const auto &call = proj.gameCalls[c].callDesc;
                    if (call.tileCount != 1 || call.loadCount != 1 ||
                        call.tileIndex >= workload.drawData.callTiles.size() ||
                        call.loadIndex >= workload.drawData.loadOperations.size()) continue;
                    const auto &load = workload.drawData.loadOperations[call.loadIndex];
                    if (load.texture.address != destination || load.texture.width != 320 ||
                        load.texture.fmt != G_IM_FMT_RGBA || load.texture.siz != G_IM_SIZ_16b ||
                        load.type != LoadOperation::Type::Tile) continue;
                    const uint16_t uls = load.operationTile.uls, ult = load.operationTile.ult;
                    if (uls % 256 != 0 || ult % 120 != 0 || uls >= 1280 || ult >= 960 ||
                        load.operationTile.lrs != uls + 252 || load.operationTile.lrt != ult + 116 ||
                        load.tile.uls != uls || load.tile.ult != ult ||
                        load.tile.lrs != uls + 252 || load.tile.lrt != ult + 116) continue;
                    const unsigned region = (ult / 120) * 5 + uls / 256;
                    const auto &tile = workload.drawData.callTiles[call.tileIndex];
                    if (tile.minTexcoord.x < 0 || tile.minTexcoord.y < 0 ||
                        tile.maxTexcoord.x > 64 || tile.maxTexcoord.y > 30) continue;
                    bool duplicate = false;
                    for (const auto &previous : indices) {
                        if (previous.first != call.tileIndex) continue;
                        if (previous.second != region) return false;
                        duplicate = true;
                    }
                    if (!duplicate) indices.emplace_back(call.tileIndex, region);
                    matched = true;
                }
            }
        }
        return matched;
    }

    // This runs on the wide render thread under workloadMutex, before any
    // subsequent workload can resize/discard the source render target.
    static bool tetrisphere_capture_rescue_pause(WorkloadQueue &queue,
                                                  FramebufferManager &fbManager,
                                                  RenderTargetManager &targetManager,
                                                  RenderWorker *worker,
                                                  const ShaderLibrary *shaderLibrary,
                                                  TextureCache *textureCache,
                                                  const Workload &workload,
                                                  uint32_t source,
                                                  uint32_t destination,
                                                  uint64_t generation) {
        auto *sourceFb = fbManager.find(source);
        if (sourceFb == nullptr || sourceFb->width != 320 || sourceFb->height < 240 ||
            sourceFb->siz != G_IM_SIZ_16b ||
            sourceFb->lastWriteType != Framebuffer::Type::Color) return false;
        RenderTargetKey sourceKey(source, sourceFb->width, sourceFb->siz,
                                  Framebuffer::Type::Color);
        auto &sourceTarget = targetManager.get(sourceKey);
        if (sourceTarget.isEmpty()) return false;
        const auto scale = sourceTarget.resolutionScale;
        if (scale.x <= scale.y * 1.01f || scale.y < 1.0f) return false;
        uint32_t expectedWidth, expectedHeight, misalignment;
        RenderTarget::computeScaledSize(sourceFb->width, sourceFb->height,
                                        scale, expectedWidth, expectedHeight,
                                        misalignment);
        if (sourceTarget.width < expectedWidth ||
            sourceTarget.height < expectedHeight) return false;

        std::vector<FramebufferOperation> operations;
        operations.reserve(40);
        std::array<uint64_t, 40> tileIds{};
        for (uint32_t i = 0; i < 40; i++) {
            const uint64_t id = (uint64_t(1) << 63) |
                                ((generation & 0x00FFFFFFFFFFFFFFull) << 6) | i;
            if (fbManager.tileCopies.find(id) != fbManager.tileCopies.end()) return false;
            tileIds[i] = id;
            FramebufferTile tile{};
            tile.address = source;
            tile.siz = sourceFb->siz;
            tile.fmt = G_IM_FMT_RGBA;
            tile.left = (i % 5) * 64;
            tile.top = (i / 5) * 30;
            tile.right = tile.left + 64;
            tile.bottom = tile.top + 30;
            tile.lineWidth = 320;
            tile.ditherPattern = sourceFb->bestDitherPattern();
            operations.push_back(fbManager.makeTileCopyTMEM(id, tile));
        }

        std::unordered_set<RenderTarget *> resized;
        fbManager.setupOperations(worker, operations, scale, targetManager, &resized);
        bool safe = resized.empty();
        for (const auto id : tileIds) {
            const auto &tile = fbManager.tileCopies.at(id);
            safe &= !tile.ignore && !tile.readColorFromStorage &&
                    !tile.readDepthFromStorage && tile.texture != nullptr;
        }
        if (!safe) {
            for (const auto id : tileIds) fbManager.tileCopies.erase(id);
            return false;
        }

        worker->commandList->begin();
        fbManager.recordOperations(worker, &workload.fbChangePool,
                                   &workload.fbStorage, shaderLibrary,
                                   textureCache, operations, targetManager,
                                   scale, 0, workload.submissionFrame);
        worker->commandList->end();
        worker->execute();
        worker->wait();

        auto &snapshot = tetrisphere_pause_snapshot();
        tetrisphere_invalidate_pause_snapshot();
        snapshot.destination = destination;
        snapshot.generation = generation;
        snapshot.tileIds = tileIds;
        snapshot.valid = true;
        snapshot.lastUsedWorkload = 0;
        (void)queue;
        return true;
    }

    static void tetrisphere_restore_pause_tiles(Workload &workload) {
        auto &snapshot = tetrisphere_pause_snapshot();
        const auto found = snapshot.originalTiles.find(workload.workloadId);
        if (found == snapshot.originalTiles.end()) return;
        for (const auto &entry : found->second) {
            if (entry.first < workload.drawData.callTiles.size())
                workload.drawData.callTiles[entry.first] = entry.second;
        }
    }

    static bool tetrisphere_apply_pause_snapshot(Workload &workload,
                                                  FramebufferManager &fbManager,
                                                  float aspectRatioScale) {
        auto &snapshot = tetrisphere_pause_snapshot();
        tetrisphere_restore_pause_tiles(workload);
        if (!snapshot.valid || aspectRatioScale <= 1.01f ||
            fbManager.tileCopies.find(snapshot.tileIds[0]) == fbManager.tileCopies.end()) {
            tetrisphere_invalidate_pause_snapshot();
            return false;
        }
        std::vector<std::pair<uint32_t, uint32_t>> indices;
        if (!tetrisphere_transition_tiles(workload, snapshot.destination, indices)) return false;
        auto &original = snapshot.originalTiles[workload.workloadId];
        if (original.empty()) {
            for (const auto &entry : indices)
                original.emplace_back(entry.first, workload.drawData.callTiles[entry.first]);
        }
        for (const auto &entry : indices) {
            const uint32_t i = entry.second;
            auto &callTile = workload.drawData.callTiles[entry.first];
            callTile.tileCopyUsed = true;
            callTile.tmemHashOrID = snapshot.tileIds[i];
            callTile.tileCopyWidth = 64;
            callTile.tileCopyHeight = 30;
            callTile.rawTMEM = false;
            callTile.syncRequired = false;
        }
        snapshot.lastUsedWorkload = workload.workloadId;
        if (const char *probe = std::getenv("TETRISPHERE_RT64_PROBE");
            probe != nullptr && probe[0] == '1' && probe[1] == '\0') {
            std::fprintf(stderr,
                "{\"event\":\"rt64_probe_rescue_snapshot_apply\","
                "\"workload\":%llu,\"generation\":%llu,\"tiles\":%zu}\n",
                static_cast<unsigned long long>(workload.workloadId),
                static_cast<unsigned long long>(snapshot.generation),indices.size());
        }
        // Workload IDs are monotonic; recycled ring slots retain their original
        // tile data and old override records can be dropped once retired.
        for (auto it = snapshot.originalTiles.begin(); it != snapshot.originalTiles.end();) {
            if (it->first + 16 < workload.workloadId) it = snapshot.originalTiles.erase(it);
            else ++it;
        }
        return true;
    }

    static void tetrisphere_retire_pause_tiles(FramebufferManager &fbManager,
                                               uint64_t completedWorkload) {
        auto &snapshot = tetrisphere_pause_snapshot();
        // The render thread waits for each workload's GPU execution. Four
        // additional workloads cover RT64's ring and interpolation repeats.
        while (!snapshot.retired.empty() &&
               snapshot.retired.front().lastUsedWorkload + 4 < completedWorkload) {
            for (const uint64_t id : snapshot.retired.front().tileIds)
                fbManager.tileCopies.erase(id);
            snapshot.retired.pop_front();
        }
    }
'''
