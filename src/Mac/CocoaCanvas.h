#pragma once

#include "Controls/Canvas.h"

namespace Luwow::Udc {
    class ControlView;

    // A window's surface for drawn controls, backed by its flipped content view
    class CocoaSurface : public IControlSurface {
    public:
        explicit CocoaSurface(void* view);

        void invalidate() override;
        Rect bounds() const override;
        int measureText(const std::string& text, const Font& font) override;
        void captureMouse(bool capture) override {}
        std::string clipboardText() override;
        void setClipboardText(const std::string& text) override;
        std::shared_ptr<ICanvasImage> loadImage(const std::string& path) override;
        std::shared_ptr<ICanvasImage> decodeImage(const std::string& bytes) override;
        std::shared_ptr<ICanvasImage> createImage(const PixelData& pixels) override;

        // Makes a font file's families usable by name in this process, returns false if it can't be loaded
        static bool loadFontFile(const std::string& path);

        // Paints the background and the controls
        void paint(ControlView& host);

    private:
        void* view;
        // AppKit views belong to the main thread, so other threads read the size last seen there
        mutable Rect lastBounds;
    };

    // Draws with AppKit into the current graphics context
    class CocoaCanvas : public ICanvas {
    public:
        void fillRect(const Rect& rect, uint32_t color, const CornerRadii& radii) override;
        void strokeRect(const Rect& rect, uint32_t color, int width, const CornerRadii& radii) override;
        void fillGradient(const Rect& rect, const std::vector<GradientStop>& stops, float rotation, const CornerRadii& radii) override;
        void drawText(const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment) override;
        void pushClip(const Rect& rect) override;
        void popClip() override;
        void drawImage(const ICanvasImage& image, const Rect& destination, const Rect& source, uint32_t tint, float rotation) override;
    };
} // namespace Luwow::Udc
