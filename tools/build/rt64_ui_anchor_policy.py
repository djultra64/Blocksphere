"""RT64 source fragment for per-draw UI anchoring at wide aspect.

DrawCall.uid carries the semantic tag supplied by the guest display-list
producer. This helper supplies only placement policy; the caller chooses
which calls to tag and passes the unmodified per-call baseline geometry.
"""

UI_ANCHOR_HELPER = r'''    enum class TetrisphereUiAnchor : unsigned char {
        None = 0, Left = 1, Right = 2, Center = 3, PauseText = 7
    };

    // Player scope is reserved for future VS P1/P2 evidence. Current tag
    // values describe shared-screen anchors and make no VS geometry claim.
    enum class TetrisphereUiPlayer : unsigned char {
        Shared = 0, P1 = 1, P2 = 2
    };

    struct TetrisphereUiAnchorTag {
        TetrisphereUiAnchor anchor;
        TetrisphereUiPlayer player;
    };

    static TetrisphereUiAnchorTag tetrisphere_ui_anchor_tag(unsigned uid) {
        switch (uid) {
            case 1: return { TetrisphereUiAnchor::Left, TetrisphereUiPlayer::Shared };
            case 2: return { TetrisphereUiAnchor::Right, TetrisphereUiPlayer::Shared };
            case 3: return { TetrisphereUiAnchor::Center, TetrisphereUiPlayer::Shared };
            case 7: return { TetrisphereUiAnchor::PauseText, TetrisphereUiPlayer::Shared };
            default: return { TetrisphereUiAnchor::None, TetrisphereUiPlayer::Shared };
        }
    }

    static float tetrisphere_ui_anchor_shift_pixels(unsigned uid,
                                                    float wideWidth,
                                                    float originalWidth,
                                                    float aspectScale) {
        if (aspectScale <= 1.01f || originalWidth <= 0.0f ||
            wideWidth <= originalWidth) return 0.0f;
        const float shift = (wideWidth - originalWidth) * 0.4f;
        switch (tetrisphere_ui_anchor_tag(uid).anchor) {
            case TetrisphereUiAnchor::Left: return -shift;
            // Keep pause labels inside their frame as the background expands.
            case TetrisphereUiAnchor::PauseText: return -(wideWidth - originalWidth) * 0.1f;
            case TetrisphereUiAnchor::Right: return shift;
            case TetrisphereUiAnchor::None:
            case TetrisphereUiAnchor::Center: return 0.0f;
        }
        return 0.0f;
    }

    // Read baseScreenOffsetX before the projection's draw-call loop. Assign
    // from it for EVERY call: a Center call after Left must not inherit Left.
    template <typename TrianglesT>
    static void tetrisphere_ui_anchor_ortho_for_call(
        TrianglesT &triangles, float baseScreenOffsetX, unsigned uid,
        float wideWidth, float originalWidth, float aspectScale,
        float halfViewportWidth) {
        const float shift = tetrisphere_ui_anchor_shift_pixels(
            uid, wideWidth, originalWidth, aspectScale);
        triangles.screenOffset.x = baseScreenOffsetX +
            ((halfViewportWidth > 0.0f) ? shift / halfViewportWidth : 0.0f);
    }

    // Rectangle viewport positions are in pixels. Pass the current call's
    // unshifted viewport X; do not reuse a previously shifted rectangle.
    template <typename ViewportT>
    static void tetrisphere_ui_anchor_rect_for_call(
        ViewportT &viewportRect, float baseRectX, unsigned uid,
        float wideWidth, float originalWidth, float aspectScale) {
        viewportRect.x = baseRectX + tetrisphere_ui_anchor_shift_pixels(
            uid, wideWidth, originalWidth, aspectScale);
    }

    // Shifted UI may extend beyond the original 4:3 scissor. Restore the
    // baseline for Center/None and for 4:3, including after a shifted call.
    template <typename ScissorT>
    static void tetrisphere_ui_anchor_scissor_for_call(
        ScissorT &scissor, const ScissorT &baseScissor, unsigned uid,
        float wideWidth, float originalWidth, float aspectScale,
        float targetWidth) {
        scissor.left = baseScissor.left;
        scissor.right = baseScissor.right;
        if (targetWidth > 0.0f && tetrisphere_ui_anchor_shift_pixels(
                uid, wideWidth, originalWidth, aspectScale) != 0.0f) {
            scissor.left = 0.0f;
            scissor.right = targetWidth;
        }
    }

'''
