"""Bounded streaming interpolation for RT64's pinned source copies.

The runtime switch is fixed before RT64 starts. Guest VI/audio pacing remains
owned by the runtime; only the renderer and presentation queues are changed.
"""
from __future__ import annotations


def _replace(source: str, old: str, new: str) -> str:
    if source.count(old) != 1:
        raise ValueError(f"RT64 frame-rate patch expected one anchor: {old[:100]!r}")
    return source.replace(old, new, 1)


def patch_frame_rate_source(name: str, source: str) -> str:
    if name == "rt64_application.cpp":
        return _patch_application(source)
    if name not in ("rt64_workload_queue.cpp", "rt64_present_queue.cpp"):
        return source
    source = _replace(source, '#include "common/rt64_thread.h"',
                      '#include "common/rt64_thread.h"\n#include "tetrisphere/unlimited_frame_stream.h"')
    if name == "rt64_workload_queue.cpp":
        return _patch_workload(source)
    return _patch_present(source)


def _patch_application(source: str) -> str:
    source = _replace(source, '#include "rt64_application.h"',
                      '#include "rt64_application.h"\n#include "tetrisphere/unlimited_frame_stream.h"')
    return _replace(source, "        state.reset();\n        workloadQueue.reset();", """        state.reset();
        if (tetrisphere_rt64_unlimited_frame_rate() && workloadQueue && presentQueue) {
            // Keep both queues alive while waking either side of the first-frame handshake.
            workloadQueue->threadsRunning = false;
            presentQueue->presentThreadRunning = false;
            for (auto &counter : sharedQueueResources->interpolatedFrames)
                tetrisphere::unlimited_frame_stream(&counter)->cancel();
            workloadQueue->cursorCondition.notify_all();
            workloadQueue->workloadIdCondition.notify_all();
            workloadQueue->idleCondition.notify_all();
            presentQueue->cursorCondition.notify_all();
            presentQueue->presentIdCondition.notify_all();
            if (presentQueue->presentThread && presentQueue->presentThread->joinable())
                presentQueue->presentThread->join();
        }
        workloadQueue.reset();""")


def _patch_workload(source: str) -> str:
    source = _replace(source, "        threadsRunning = false;", """        threadsRunning = false;
        if (tetrisphere_rt64_unlimited_frame_rate() && ext.sharedResources != nullptr) {
            for (auto &counter : ext.sharedResources->interpolatedFrames)
                tetrisphere::unlimited_frame_stream(&counter)->cancel();
        }""")
    source = _replace(source,
        "                const bool requiresFrameMatching = (workloadConfig.targetRate > 0) || workloadConfig.raytracingEnabled;",
        """                const bool unlimitedFrameRate = tetrisphere_rt64_unlimited_frame_rate();
                const bool requiresFrameMatching = unlimitedFrameRate || (workloadConfig.targetRate > 0) || workloadConfig.raytracingEnabled;""")
    source = _replace(source,
        "const bool displayRateAboveOriginal = (workload.viOriginalRate > 0) && (workloadConfig.targetRate > workload.viOriginalRate);",
        "const bool displayRateAboveOriginal = (workload.viOriginalRate > 0) && (unlimitedFrameRate || (workloadConfig.targetRate > workload.viOriginalRate));")
    source = _replace(source,
        "                if (generateInterpolatedFrames) {\n                    logicalTicks += workloadConfig.targetRate;",
        "                if (generateInterpolatedFrames && !unlimitedFrameRate) {\n                    logicalTicks += workloadConfig.targetRate;")
    source = _replace(source,
        "                else if (generateInterpolatedFrames) {\n                    std::unique_lock<std::mutex> interpolatedLock",
        "                else if (generateInterpolatedFrames && !unlimitedFrameRate) {\n                    std::unique_lock<std::mutex> interpolatedLock")
    resize_anchor = "        if (ext.sharedResources->fbConfigChanged || sizeChanged) {"
    source = _replace(source, resize_anchor, resize_anchor + """
            if (tetrisphere_rt64_unlimited_frame_rate()) {
                for (auto &counter : ext.sharedResources->interpolatedFrames) {
                    if (!tetrisphere::unlimited_frame_stream(&counter)->wait_until_drained([&]() {
                        return !threadsRunning || !ext.presentQueue->presentThreadRunning;
                    })) return;
                }
            }""")
    config_anchor = "                threadConfigurationUpdate(workload.viFbSize, workloadConfig);"
    source = _replace(source, config_anchor, config_anchor + "\n                if (!threadsRunning) break;")
    anchor = "                InterpolatedFrameCounters &curFrameCounters = ext.sharedResources->interpolatedFrames[ext.sharedResources->interpolatedFramesIndex];\n                curFrameCounters.skipped = false;"
    source = _replace(source, anchor, anchor + """
                const bool streamingFrames = unlimitedFrameRate && generateInterpolatedFrames;
                auto frameStream = unlimitedFrameRate ?
                    tetrisphere::unlimited_frame_stream(&curFrameCounters) : nullptr;
                const auto streamStopped = [&]() {
                    return !threadsRunning || !ext.presentQueue->presentThreadRunning;
                };
                const uint64_t streamGeneration = unlimitedFrameRate ?
                    frameStream->begin(streamingFrames, streamStopped) : 0;
                const uint32_t streamTargetBase = ext.sharedResources->interpolatedFramesIndex *
                    uint32_t(tetrisphere::UnlimitedFrameStream::capacity);""")
    source = _replace(source,
        "uint32_t requiredFrames = (usingMSAA && generateInterpolatedFrames) ? displayFrames : (displayFrames - 1);",
        "uint32_t requiredFrames = streamingFrames ? uint32_t(2 * tetrisphere::UnlimitedFrameStream::capacity) :\n                    ((usingMSAA && generateInterpolatedFrames) ? displayFrames : (displayFrames - 1));")
    source = _replace(source,
        "                for (uint32_t frame = 0; (frame < displayFrames) && !skipWorkloadNow; frame++) {",
        """                const auto streamDeadline = tetrisphere::UnlimitedFrameStream::Clock::now() +
                    std::chrono::microseconds(std::max<int64_t>(0, originalTimeMicro - workloadTimer.elapsedMicroseconds()));
                for (uint64_t frame = 0; ((frame < displayFrames) || streamingFrames) && !skipWorkloadNow; frame++) {
                    if (streamingFrames && frame > 0) {
                        const auto obsoleteOrStopped = [&]() {
                            if (streamStopped()) return true;
                            std::scoped_lock<std::mutex> cursorLock(cursorMutex);
                            return writeCursor != threadCursor;
                        };
                        if (!frameStream->wait_for_slot(streamGeneration, frame,
                                                       streamDeadline, obsoleteOrStopped)) break;
                    }""")
    source = _replace(source,
        "                    if ((frame > 0) && (originalTimeMicro > 0)) {",
        "                    if (!streamingFrames && (frame > 0) && (originalTimeMicro > 0)) {")
    source = _replace(source,
        "                    if (generateInterpolatedFrames) {\n                        prevFrameWeight = std::clamp",
        """                    if (streamingFrames) {
                        prevFrameWeight = (frame == 0) ? 0.0f : curFrameWeight;
                        const int64_t estimatedRenderMicro = std::max<int64_t>(1,
                            framesRendered > 0 ? renderTimeTotalMicro / framesRendered : 1);
                        curFrameWeight = std::max(prevFrameWeight, tetrisphere::unlimited_frame_weight(
                            workloadTimer.elapsedMicroseconds(), estimatedRenderMicro, originalTimeMicro));
                        deltaTimeMs = (curFrameWeight - prevFrameWeight) / float(workload.viOriginalRate);
                        if (usingMSAA || frame > 0) {
                            const uint32_t slot = uint32_t((frame - (usingMSAA ? 0 : 1)) %
                                tetrisphere::UnlimitedFrameStream::capacity) + streamTargetBase;
                            overrideTarget = interpolatedTargets[slot].get();
                            overrideModifier = slot + 1;
                        }
                    }
                    else if (generateInterpolatedFrames) {
                        prevFrameWeight = std::clamp""")
    anchor = "                    // For every additional frame, we increase the frames available and notify the present queue."
    source = _replace(source, anchor, """                    if (streamingFrames && !frameStream->publish(streamGeneration)) {
                        skipWorkloadNow = true;
                    }

""" + anchor)
    source = _replace(source,
        "                    if (generateInterpolatedFrames && (usingMSAA || (frame > 0))) {",
        "                    if (!streamingFrames && generateInterpolatedFrames && (usingMSAA || (frame > 0))) {")
    anchor = "                // Set the skipped parameter on the frame counter if the workload wasn't skipped but some of its frames were."
    source = _replace(source, anchor, """                if (streamingFrames) {
                    frameStream->finish(streamGeneration);
                    displayFrames = framesRendered;
                }

""" + anchor)
    return source


def _patch_present(source: str) -> str:
    source = _replace(source, '#include "tetrisphere/unlimited_frame_stream.h"',
        '#include "tetrisphere/unlimited_frame_stream.h"\n#include "plume_vulkan.h"\n#include <cstdio>\n#include <cstdlib>\n#include <cstring>')
    source = _replace(source, "            presentThread->join();",
                      "            if (presentThread->joinable()) presentThread->join();")
    source = _replace(source, "        presentThreadRunning = false;", """        presentThreadRunning = false;
        if (tetrisphere_rt64_unlimited_frame_rate() && ext.sharedResources != nullptr) {
            for (auto &counter : ext.sharedResources->interpolatedFrames)
                tetrisphere::unlimited_frame_stream(&counter)->cancel();
        }""")
    anchor = "        InterpolatedFrameCounters &frameCounters = ext.sharedResources->interpolatedFrames[ext.sharedResources->interpolatedFramesIndex];"
    # This anchor also occurs in skipInterpolation(), with different indentation.
    start = source.index("    void PresentQueue::threadPresent(")
    end = source.index("    void PresentQueue::skipInterpolation()")
    section = source[start:end]
    section = _replace(section, anchor, anchor + """
        const bool unlimitedFrameRate = tetrisphere_rt64_unlimited_frame_rate();
        auto frameStream = unlimitedFrameRate ?
            tetrisphere::unlimited_frame_stream(&frameCounters) : nullptr;
        const auto streamSnapshot = frameStream ? frameStream->claim() :
            tetrisphere::UnlimitedFrameStream::Snapshot{};
        bool streamingFrames = unlimitedFrameRate && streamSnapshot.active;
        bool streamingFramebuffer = false;
        const uint32_t streamTargetBase = ext.sharedResources->interpolatedFramesIndex *
            uint32_t(tetrisphere::UnlimitedFrameStream::capacity);""")
    section = _replace(section,
        "                if (presentFb->interpolationEnabled) {\n                    framesToPresent = frameCounters.count;",
        """                if (presentFb->interpolationEnabled) {
                    streamingFramebuffer = true;
                    framesToPresent = unlimitedFrameRate ? 1 : frameCounters.count;""")
    section = _replace(section,
        "        for (int32_t i = 0; i < framesToPresent; i++) {",
        """        if (streamingFrames && (!streamingFramebuffer || colorTarget == nullptr)) {
            frameStream->cancel();
            streamingFrames = false;
        }
        for (uint64_t i = 0; streamingFrames || (i < uint64_t(framesToPresent)); i++) {""")
    section = _replace(section,
        "            if ((framesToPresent > 1) && (usingMSAA || (i > 0))) {",
        """            if (streamingFrames) {
                if (!frameStream->wait_for_frame(streamSnapshot.generation, i, [&]() {
                        return !presentThreadRunning || !ext.workloadQueue->threadsRunning;
                    })) {
                    if (i == 0) notifyPresentId(present);
                    break;
                }
                if (usingMSAA || i > 0) {
                    const uint32_t slot = uint32_t((i - (usingMSAA ? 0 : 1)) %
                        tetrisphere::UnlimitedFrameStream::capacity) + streamTargetBase;
                    colorTarget = ext.sharedResources->interpolatedColorTargets[slot].get();
                }
            }
            else if ((framesToPresent > 1) && (usingMSAA || (i > 0))) {""")
    # Preserve ordinary-mode signed index arithmetic; it was int32_t before the
    # streaming sequence counter changed to uint64_t.
    section = _replace(section,
        "framesToPresent = std::min(int(frameCounters.available), i + 1);",
        "framesToPresent = std::min(int(frameCounters.available), int(i + 1));")
    section = _replace(section,
        "            const bool presentFrame = (i < framesToPresent) && swapChainValid;",
        "            const bool presentFrame = (streamingFrames || (i < uint64_t(framesToPresent))) && swapChainValid;")
    anchor = "            if (frameCountersNextPresented > 0) {"
    section = _replace(section, anchor, """            if (streamingFrames) {
                // The GPU has finished reading this source target. It can be
                // reused even while the swapchain image waits for presentation.
                frameStream->consume(streamSnapshot.generation, i);
            }

""" + anchor)
    section = _replace(section,
        "if ((presentTimestamp != Timestamp()) && (targetRate > 0) && (targetRate > viOriginalRate)) {",
        "if (!unlimitedFrameRate && (presentTimestamp != Timestamp()) && (targetRate > 0) && (targetRate > viOriginalRate)) {")
    section = _replace(section,
        "                if (presentWaitEnabled) {",
        "                if (!unlimitedFrameRate && presentWaitEnabled) {")
    anchor = "                presentProfiler.logAndRestart();"
    section = _replace(section, anchor, anchor + r"""
                static const bool measureFrames = [] {
                    const char *value = std::getenv("TETRISPHERE_FRAME_METRICS");
                    return value != nullptr && std::strcmp(value, "1") == 0;
                }();
                if (measureFrames && swapChainValid) {
                    using Clock = std::chrono::steady_clock;
                    static thread_local auto sampleStart = Clock::now();
                    static thread_local unsigned presented = 0;
                    ++presented;
                    const auto now = Clock::now();
                    const double elapsed = std::chrono::duration<double>(now - sampleStart).count();
                    if (elapsed >= 1.0) {
                        const int presentMode = ext.createdGraphicsAPI == UserConfiguration::GraphicsAPI::Vulkan ?
                            int(static_cast<plume::VulkanSwapChain *>(ext.swapChain)->createdPresentMode) : -1;
                        std::fprintf(stdout,
                            "{\"event\":\"rt64_frame_metrics\",\"fps\":%.2f,\"frames\":%u,"
                            "\"width\":%u,\"height\":%u,\"unlimited\":%s,\"vsync\":%s,\"present_mode\":%d}\n",
                            presented / elapsed, presented, ext.swapChain->getWidth(),
                            ext.swapChain->getHeight(), unlimitedFrameRate ? "true" : "false",
                            ext.swapChain->isVsyncEnabled() ? "true" : "false", presentMode);
                        std::fflush(stdout);
                        sampleStart = now;
                        presented = 0;
                    }
                }""")
    # Optional diagnostic format predates the 64-bit streaming sequence.
    section = section.replace(
        '                            i, framesToPresent, swapChainValid ? "true" : "false");',
        '                            static_cast<int>(i), framesToPresent, swapChainValid ? "true" : "false");')
    source = source[:start] + section + source[end:]
    anchor = "    void PresentQueue::skipInterpolation() {"
    source = _replace(source, anchor, anchor + """
        if (tetrisphere_rt64_unlimited_frame_rate()) {
            auto &counter = ext.sharedResources->interpolatedFrames[ext.sharedResources->interpolatedFramesIndex];
            tetrisphere::unlimited_frame_stream(&counter)->cancel();
        }""")
    anchor = '        Thread::setCurrentThreadName("RT64 Present");'
    source = _replace(source, anchor, anchor + """
        // Vulkan marks the swapchain for recreation when this mode changes.
        // IMMEDIATE is requested when supported; drivers/compositors can still
        // impose synchronization outside the game's frame scheduler.
        ext.swapChain->setVsyncEnabled(!tetrisphere_rt64_unlimited_frame_rate());""")
    anchor = "        ext.swapChain->setVsyncEnabled(!tetrisphere_rt64_unlimited_frame_rate());"
    source = _replace(source, anchor, anchor + """
        if (tetrisphere_rt64_unlimited_frame_rate() &&
            ext.createdGraphicsAPI == UserConfiguration::GraphicsAPI::Vulkan) {
            auto *swapChain = static_cast<plume::VulkanSwapChain *>(ext.swapChain);
            if (!swapChain->immediatePresentModeSupported && swapChain->mailboxPresentModeSupported)
                swapChain->requiredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        }""")
    return source
