#include "Scaling.h"

#include <algorithm>
#include <cmath>

namespace Luwow::Udc {
    // Scaler

    int Scaler::toDevice(int value) const {
        return static_cast<int>(std::lround(value * factor));
    }

    int Scaler::toLogical(int value) const {
        return static_cast<int>(std::lround(value / factor));
    }

    Rect Scaler::toDevice(const Rect& rect) const {
        int left = toDevice(rect.x);
        int top = toDevice(rect.y);
        return Rect{ left, top, toDevice(rect.right()) - left, toDevice(rect.bottom()) - top };
    }

    Rect Scaler::toLogical(const Rect& rect) const {
        int left = toLogical(rect.x);
        int top = toLogical(rect.y);
        return Rect{ left, top, toLogical(rect.right()) - left, toLogical(rect.bottom()) - top };
    }

    int Scaler::toDeviceWidth(int width) const {
        if (width <= 0) return 0;
        return std::max(1, toDevice(width));
    }

    Font Scaler::toDevice(const Font& font) const {
        return Font{ font.family, std::max(1, toDevice(font.size)), font.bold, font.italic };
    }

    // CornerRadii

    CornerRadii CornerRadii::grown(int amount) const {
        auto grow = [amount](int radius) { return radius > 0 ? std::max(0, radius + amount) : 0; };
        return CornerRadii(grow(topLeft), grow(topRight), grow(bottomRight), grow(bottomLeft));
    }

    // When the radii along a side add up to more than the side, every radius is scaled down by
    // the same factor, so the shape keeps its proportions. Negative radii are 0.
    CornerRadii CornerRadii::fitted(const Rect& rect) const {
        CornerRadii fit(std::max(0, topLeft), std::max(0, topRight), std::max(0, bottomRight), std::max(0, bottomLeft));
        if (fit.square()) return fit;

        double factor = 1;
        auto limit = [&factor](int side, int a, int b) {
            if (a + b > 0) factor = std::min(factor, static_cast<double>(std::max(0, side)) / (a + b));
        };
        limit(rect.width, fit.topLeft, fit.topRight);
        limit(rect.width, fit.bottomLeft, fit.bottomRight);
        limit(rect.height, fit.topLeft, fit.bottomLeft);
        limit(rect.height, fit.topRight, fit.bottomRight);
        if (factor >= 1) return fit;

        auto scale = [factor](int radius) { return static_cast<int>(radius * factor); };
        return CornerRadii(scale(fit.topLeft), scale(fit.topRight), scale(fit.bottomRight), scale(fit.bottomLeft));
    }

    // ScaledCanvas

    static CornerRadii toDevice(const Scaler& scaler, const CornerRadii& radii, const Rect& deviceRect) {
        return CornerRadii(scaler.toDevice(radii.topLeft), scaler.toDevice(radii.topRight), scaler.toDevice(radii.bottomRight),
            scaler.toDevice(radii.bottomLeft)).fitted(deviceRect);
    }

    void ScaledCanvas::fillRect(const Rect& rect, uint32_t color, const CornerRadii& radii) {
        Rect deviceRect = scaler.toDevice(rect);
        device.fillRect(deviceRect, color, toDevice(scaler, radii, deviceRect));
    }

    void ScaledCanvas::strokeRect(const Rect& rect, uint32_t color, int width, const CornerRadii& radii) {
        Rect deviceRect = scaler.toDevice(rect);
        device.strokeRect(deviceRect, color, scaler.toDeviceWidth(width), toDevice(scaler, radii, deviceRect));
    }

    void ScaledCanvas::fillGradient(const Rect& rect, const std::vector<GradientStop>& stops, float rotation, const CornerRadii& radii) {
        Rect deviceRect = scaler.toDevice(rect);
        device.fillGradient(deviceRect, stops, rotation, toDevice(scaler, radii, deviceRect));
    }

    void ScaledCanvas::drawText(const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment) {
        device.drawText(text, scaler.toDevice(rect), color, scaler.toDevice(font), alignment);
    }

    void ScaledCanvas::pushClip(const Rect& rect) {
        device.pushClip(scaler.toDevice(rect));
    }

    void ScaledCanvas::popClip() {
        device.popClip();
    }

    void ScaledCanvas::drawImage(const ICanvasImage& image, const Rect& destination, const Rect& source, uint32_t tint, float rotation) {
        device.drawImage(image, scaler.toDevice(destination), source, tint, rotation);
    }
} // namespace Luwow::Udc
