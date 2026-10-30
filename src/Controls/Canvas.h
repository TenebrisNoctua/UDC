#pragma once

#include "RectCore.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Luwow::Udc {
    // A rectangle in window pixels
    using Rect = Luwow::Rects::Rect;

    struct Font {
        std::string family;
        int size = 14;
        bool bold = false;
        bool italic = false;
    };

    // Colors are 0xTTRRGGBB: the top byte is the transparency, so a plain 0xRRGGBB is opaque and
    // 0xFF000000 is fully transparent. Luau Colors are turned into these by ControlLuau.
    inline uint32_t colorAlpha(uint32_t color) { return 255 - (color >> 24); }
    inline uint32_t colorRgb(uint32_t color) { return color & 0xFFFFFF; }
    inline uint32_t makeColor(uint32_t rgb, uint32_t alpha) { return ((255 - (alpha & 0xFF)) << 24) | (rgb & 0xFFFFFF); }

    // The radius of each corner in pixels.
    struct CornerRadii {
        int topLeft = 0;
        int topRight = 0;
        int bottomRight = 0;
        int bottomLeft = 0;

        // The same radius for every corner
        CornerRadii(int all = 0) : topLeft(all), topRight(all), bottomRight(all), bottomLeft(all) {}
        CornerRadii(int topLeft, int topRight, int bottomRight, int bottomLeft)
            : topLeft(topLeft), topRight(topRight), bottomRight(bottomRight), bottomLeft(bottomLeft) {}

        bool square() const { return topLeft <= 0 && topRight <= 0 && bottomRight <= 0 && bottomLeft <= 0; }

        // Each rounded corner grown by amount, or shrunk for a negative one down to 0. Square corners stay square.
        CornerRadii grown(int amount) const;

        // Scaled down together when two corners on a side would overlap
        CornerRadii fitted(const Rect& rect) const;
    };

    // A color along a gradient, at position 0 to 1 from its start to its end
    struct GradientStop {
        float position = 0;
        uint32_t color = 0;
    };

    enum class TextAlignment {
        Left,
        Center,
        Right
    };

    // RGBA, a byte a channel, not premultiplied, rows from the top
    struct PixelData {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> rgba;
        // Tells these pixels apart from any made before or after them, for the buffer Luau reads them as
        uint64_t serial = 0;
    };

    // An image file's path, or pixels. Neither shows nothing. Pixels are never
    // changed once made, so a control and the view's cache can share them.
    struct ImageSource {
        std::string path;
        std::shared_ptr<const PixelData> pixels;

        bool empty() const { return path.empty() && !pixels; }
        bool operator==(const ImageSource& other) const { return path == other.path && pixels == other.pixels; }
        bool operator!=(const ImageSource& other) const { return !(*this == other); }
    };

    // An image loaded by a surface, in the platform's own form
    class ICanvasImage {
    public:
        virtual ~ICanvasImage() = default;

        // The size in the image's own pixels
        virtual int width() const = 0;
        virtual int height() const = 0;
    };

    // Paints controls.
    class ICanvas {
    public:
        virtual ~ICanvas() = default;

        // Square radii are a plain rectangle, which platforms draw with their fast path when the color is
        // opaque. The radii are already fitted to the rectangle (ScaledCanvas fits them).
        virtual void fillRect(const Rect& rect, uint32_t color, const CornerRadii& radii) = 0;
        // The border is drawn inside the rectangle
        virtual void strokeRect(const Rect& rect, uint32_t color, int width, const CornerRadii& radii) = 0;
        // Fills with a linear gradient. Its line goes through the rectangle's center at rotation degrees,
        // clockwise from left to right, and spans the rectangle, the corners
        // are at its ends. The stops are in order, starting at 0 and ending at 1.
        virtual void fillGradient(const Rect& rect, const std::vector<GradientStop>& stops, float rotation, const CornerRadii& radii) = 0;
        // Draws one line of text, vertically centered in rect and aligned horizontally within it
        virtual void drawText(const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment) = 0;

        // Limits painting to rect, intersected with the current clip, until the matching popClip
        virtual void pushClip(const Rect& rect) = 0;
        virtual void popClip() = 0;

        // Draws the source part of the image (in the image's pixels) scaled into destination, smoothly. Its
        // colors are multiplied by tint's, and its opacity by tint's alpha.
        // rotation is in degrees, clockwise about the destination's center
        virtual void drawImage(const ICanvasImage& image, const Rect& destination, const Rect& source, uint32_t tint = 0xFFFFFF, float rotation = 0) = 0;
    };

    // Keys and edit commands controls understand. The platform turns its own shortcuts into the edit
    // commands (Copy, Cut, Paste, SelectAll), so system and user key bindings work the same everywhere.
    enum class Key {
        Left,
        Right,
        Up,
        Down,
        Home,
        End,
        Backspace,
        Delete,
        Enter,
        Escape,
        Tab,
        Copy,
        Cut,
        Paste,
        SelectAll,
        Other
    };

    struct KeyModifiers {
        bool shift = false;
        bool control = false;
    };

    // The native window controls are drawn in
    class IControlSurface {
    public:
        virtual ~IControlSurface() = default;

        // Asks for a repaint, the window paints again from its event loop
        virtual void invalidate() = 0;

        // The window's client area, controls are positioned within it
        virtual Rect bounds() const = 0;

        virtual int measureText(const std::string& text, const Font& font) = 0;

        // While captured, the window keeps getting mouse moves and releases outside of it
        virtual void captureMouse(bool capture) = 0;

        virtual std::string clipboardText() = 0;
        virtual void setClipboardText(const std::string& text) = 0;

        // Device pixels per logical pixel, such as 1.5 at 144 DPI on Windows. Surfaces whose platform
        // already scales (macOS works in points) return 1. bounds() and measureText() are in device pixels.
        virtual float scale() const { return 1.0f; }

        // Loads an image file, PNG, JPEG, BMP or GIF. nullptr if it can't be loaded.
        virtual std::shared_ptr<ICanvasImage> loadImage(const std::string& path) = 0;
        // The same from the file's bytes in memory
        virtual std::shared_ptr<ICanvasImage> decodeImage(const std::string& bytes) = 0;
        // An image of the pixels, which are already checked to fill their size
        virtual std::shared_ptr<ICanvasImage> createImage(const PixelData& pixels) = 0;
    };
} // namespace Luwow::Udc
