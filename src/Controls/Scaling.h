#pragma once

#include "Canvas.h"

// High-DPI support, kept apart from the controls.
namespace Luwow::Udc {
    class Scaler {
    public:
        explicit Scaler(float scale) : factor(scale > 0 ? scale : 1.0f) {}

        float scale() const { return factor; }

        int toDevice(int value) const;
        int toLogical(int value) const;

        // Edges are rounded separately, so neighbouring rectangles still meet without gaps
        Rect toDevice(const Rect& rect) const;
        Rect toLogical(const Rect& rect) const;

        // A line or border width, never thinner than one device pixel
        int toDeviceWidth(int width) const;

        Font toDevice(const Font& font) const;

    private:
        float factor;
    };

    // Lets controls paint in logical pixels on a canvas drawing in device pixels
    class ScaledCanvas : public ICanvas {
    public:
        ScaledCanvas(ICanvas& device, float scale) : device(device), scaler(scale) {}

        void fillRect(const Rect& rect, uint32_t color, const CornerRadii& radii) override;
        void strokeRect(const Rect& rect, uint32_t color, int width, const CornerRadii& radii) override;
        void fillGradient(const Rect& rect, const std::vector<GradientStop>& stops, float rotation, const CornerRadii& radii) override;
        void drawText(const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment) override;
        void pushClip(const Rect& rect) override;
        void popClip() override;
        // Only the destination scales, the source is in the image's own pixels
        void drawImage(const ICanvasImage& image, const Rect& destination, const Rect& source, uint32_t tint, float rotation) override;

    private:
        ICanvas& device;
        Scaler scaler;
    };
} // namespace Luwow::Udc
