#pragma once

#include "Controls/Canvas.h"

#include <atomic>
#include <map>
#include <tuple>
#include <string>
#include <utility>
#include <windows.h>

namespace Luwow::Udc {
    class ControlView;

    // A window's surface for drawn controls. Keeps the fonts controls use for the window's lifetime.
    // Posted by a surface that can't paint, so the window settles its layout
    constexpr UINT WM_LUWOW_SETTLE = WM_APP + 0x4C;

    class GdiSurface : public IControlSurface {
    public:
        explicit GdiSurface(HWND hwnd) : hwnd(hwnd) {}
        ~GdiSurface() override;

        void invalidate() override;
        // Called as the settle message is handled, so later changes post another
        void settled() { settlePosted = false; }
        Rect bounds() const override;
        int measureText(const std::string& text, const Font& font) override;
        void captureMouse(bool capture) override;
        std::string clipboardText() override;
        void setClipboardText(const std::string& text) override;
        float scale() const override;
        std::shared_ptr<ICanvasImage> loadImage(const std::string& path) override;
        std::shared_ptr<ICanvasImage> decodeImage(const std::string& bytes) override;
        std::shared_ptr<ICanvasImage> createImage(const PixelData& pixels) override;

        // Paints the controls into an offscreen bitmap first, so repaints don't flicker
        void paint(ControlView& host);

        HFONT font(const Font& font);

        // Makes a font file's families usable by name in this process, returns false if it can't be loaded
        static bool loadFontFile(const std::string& path);

    private:
        HWND hwnd;
        // The paint's back buffer, a 32-bit DIB section text is blended into directly. Kept between paints
        // and grown to the client area.
        HDC backDc = nullptr;
        HBITMAP backBitmap = nullptr;
        HGDIOBJ backOld = nullptr;
        int backWidth = 0;
        int backHeight = 0;
        bool reserveBackBuffer(HDC windowDc, int width, int height);
        // Whether a settle message is waiting, so invalidating repeatedly posts one
        std::atomic<bool> settlePosted = false;
        // By family, size, bold and italic
        std::map<std::tuple<std::string, int, bool, bool>, HFONT> fonts;
    };

    class GdiCanvas : public ICanvas {
    public:
        GdiCanvas(HDC dc, GdiSurface& surface) : dc(dc), surface(surface) {}

        void fillRect(const Rect& rect, uint32_t color, const CornerRadii& radii) override;
        void strokeRect(const Rect& rect, uint32_t color, int width, const CornerRadii& radii) override;
        void fillGradient(const Rect& rect, const std::vector<GradientStop>& stops, float rotation, const CornerRadii& radii) override;
        void drawText(const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment) override;
        void pushClip(const Rect& rect) override;
        void popClip() override;
        void drawImage(const ICanvasImage& image, const Rect& destination, const Rect& source, uint32_t tint, float rotation) override;

    private:
        HDC dc;
        GdiSurface& surface;
    };

    // Loads an .ico, or an image, as an icon of size by size pixels.
    HICON loadIconFile(const std::string& path, int size);

    // UTF-8 and UTF-16 conversions for the wide Win32 APIs
    std::wstring toWide(const std::string& text);
    std::string toUtf8(const std::wstring& text);
} // namespace Luwow::Udc
