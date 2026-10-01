"""Pinned RT64 source fragments for Rescue HUD placement at wide aspect."""

RESCUE_HUD_HELPER = '''    // Match the Rescue gameplay draw order and fixed UI anchors observed at
    // 320x240. All checks must pass before a wide HUD element is moved.
    template <typename RectT>
    static bool tetrisphere_rescue_rect_equals(const RectT &r,
                                               int x0, int y0, int x1, int y1) {
        return r.ulx == x0 && r.uly == y0 && r.lrx == x1 && r.lry == y1;
    }

    template <typename RectT>
    static int tetrisphere_rescue_hud_rect_side(const RectT &r) {
        if (r.lrx <= 640) return -1;
        if (r.ulx >= 640) return 1;
        return 0;
    }

    template <typename PairT, typename DataT>
    // Return the right indicator's projection. The left HUD and skull outputs
    // remain usable when an animation temporarily omits the right indicator.
    // Scoring can insert a projection before the world; heart loss can insert
    // projections before the right indicator.
    static int tetrisphere_rescue_hud_layout(const PairT &pair, const DataT &data,
                                             float aspectScale,
                                             int *nextIndex = nullptr,
                                             int *hudIndex = nullptr,
                                             int *skullIndex = nullptr) {
        if (aspectScale <= 1.01f || pair.projectionCount < 5 ||
            pair.projectionCount > pair.projections.size() ||
            !tetrisphere_rescue_rect_equals(pair.scissorRect, 0, 0, 1280, 960)) return -1;
        const auto centered = [](const auto &v) {
            return v.scale.x > 159.5f && v.scale.x < 160.5f &&
                   v.scale.y > 119.5f && v.scale.y < 120.5f &&
                   v.translate.x > 159.5f && v.translate.x < 160.5f &&
                   v.translate.y > 119.5f && v.translate.y < 120.5f;
        };
        int matchedRight = -1, matchedNext = -1, matchedHud = -1;
        int matchedSkull = -1;
        for (unsigned worldIndex = 2; worldIndex + 2 < pair.projectionCount; worldIndex++) {
            const unsigned nextCandidate = worldIndex + 1;
            const unsigned hudCandidate = worldIndex + 2;
            const auto &world = pair.projections[worldIndex];
            const auto &next = pair.projections[nextCandidate];
            const auto &hud = pair.projections[hudCandidate];
            if (static_cast<unsigned>(world.type) != 1u ||
                static_cast<unsigned>(next.type) != 2u || next.gameCallCount == 0 ||
                static_cast<unsigned>(hud.type) != 3u || hud.gameCallCount < 38 ||
                hud.gameCalls.size() < hud.gameCallCount ||
                !tetrisphere_rescue_rect_equals(next.scissorRect, 0, 0, 1280, 960) ||
                !tetrisphere_rescue_rect_equals(hud.scissorRect, 0, 0, 1280, 960) ||
                next.transformsIndex >= data.rspViewports.size() ||
                !centered(data.rspViewports[next.transformsIndex]) ||
                !tetrisphere_rescue_rect_equals(hud.gameCalls[0].callDesc.rect,
                                                108, 312, 347, 411) ||
                !tetrisphere_rescue_rect_equals(hud.gameCalls[1].callDesc.rect,
                                                108, 72, 387, 183)) continue;
            int rightIndex = -1, skullCandidate = -1;
            for (unsigned i = hudCandidate + 3; i < pair.projectionCount; i++) {
                const auto &right = pair.projections[i];
                if (static_cast<unsigned>(right.type) == 3u &&
                    right.gameCallCount > 0 && right.gameCallCount <= 2 &&
                    right.gameCalls.size() >= right.gameCallCount &&
                    tetrisphere_rescue_rect_equals(right.scissorRect, 0, 0, 1280, 960) &&
                    tetrisphere_rescue_rect_equals(right.gameCalls[0].callDesc.rect,
                                                   1092, 128, 1131, 191)) {
                    bool same = true;
                    for (unsigned d = 1; d < right.gameCallCount; d++) {
                        same &= tetrisphere_rescue_rect_equals(
                            right.gameCalls[d].callDesc.rect, 1092, 128, 1131, 191);
                    }
                    if (same) {
                        if (skullCandidate >= 0) return -1;
                        skullCandidate = int(i);
                    }
                }
                if (static_cast<unsigned>(right.type) != 2u || right.gameCallCount < 2 ||
                    right.gameCalls.size() < right.gameCallCount ||
                    !tetrisphere_rescue_rect_equals(right.scissorRect, 0, 0, 1280, 960) ||
                    right.transformsIndex >= data.rspViewports.size()) continue;
                const auto &first = right.gameCalls[0].callDesc.rect;
                if (first.ulx < 900 || first.lrx > 1280 || first.uly < 100 ||
                    first.lry > 300 || !centered(data.rspViewports[right.transformsIndex])) continue;
                if (rightIndex >= 0) return -1;
                rightIndex = int(i);
            }
            if (matchedHud >= 0) return -1;
            matchedRight = rightIndex;
            matchedNext = int(nextCandidate);
            matchedHud = int(hudCandidate);
            matchedSkull = skullCandidate;
        }
        if (matchedHud >= 0) {
            if (nextIndex != nullptr) *nextIndex = matchedNext;
            if (hudIndex != nullptr) *hudIndex = matchedHud;
            if (skullIndex != nullptr) *skullIndex = matchedSkull;
        }
        return matchedRight;
    }

    static float tetrisphere_rescue_hud_shift(float wideWidth, float originalWidth) {
        return (wideWidth - originalWidth) * 0.4f;
    }

'''
