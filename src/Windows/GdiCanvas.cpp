#include "GdiCanvas.h"
#include "DirectWriteText.h"
#include "Dpi.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include "Controls/ControlView.h"

#include <objidl.h>
namespace Gdiplus {
    using std::max;
    using std::min;
}
#include <gdiplus.h>
#include <shlwapi.h>

namespace Luwow::Udc {

    // GDI colors are 0x00BBGGRR, and have no alpha
    static COLORREF toColorRef(uint32_t color) {
        return RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    }

    std::wstring toWide(const std::string& text) {
        if (text.empty()) return std::wstring();
        int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
        return wide;
    }

    std::string toUtf8(const std::wstring& text) {
        if (text.empty()) return std::string();
        int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), length, nullptr, nullptr);
        return utf8;
    }

    // GdiSurface

    GdiSurface::~GdiSurface() {
        for (auto& [key, handle] : fonts) DeleteObject(handle);
        if (backDc) {
            SelectObject(backDc, backOld);
            DeleteDC(backDc);
        }
        if (backBitmap) {
            DirectWriteText::removeBackBuffer(backBitmap);
            DeleteObject(backBitmap);
        }
    }

    bool GdiSurface::reserveBackBuffer(HDC windowDc, int width, int height) {
        if (backBitmap && width == backWidth && height == backHeight) return true;
        if (!backDc) {
            backDc = CreateCompatibleDC(windowDc);
            if (!backDc) return false;
        }

        BITMAPINFO info{};
        info.bmiHeader = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB };
        void* bits = nullptr;
        HBITMAP created = CreateDIBSection(windowDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!created) return false;

        HGDIOBJ previous = SelectObject(backDc, created);
        if (!backOld) backOld = previous;
        if (backBitmap) {
            DirectWriteText::removeBackBuffer(backBitmap);
            DeleteObject(backBitmap);
        }
        backBitmap = created;
        DirectWriteText::addBackBuffer(created);
        backWidth = width;
        backHeight = height;
        return true;
    }

    // A window that isn't shown gets no WM_PAINT, so its layout is settled through a message instead
    void GdiSurface::invalidate() {
        InvalidateRect(hwnd, nullptr, FALSE);
        if ((!IsWindowVisible(hwnd) || IsIconic(hwnd)) && !settlePosted.exchange(true)) PostMessage(hwnd, WM_LUWOW_SETTLE, 0, 0);
    }

    Rect GdiSurface::bounds() const {
        RECT client;
        GetClientRect(hwnd, &client);
        return Rect{ 0, 0, client.right - client.left, client.bottom - client.top };
    }

    HFONT GdiSurface::font(const Font& font) {
        auto key = std::make_tuple(font.family, font.size, font.bold, font.italic);
        auto it = fonts.find(key);
        if (it != fonts.end()) return it->second;

        std::wstring family = font.family.empty() ? L"Segoe UI" : toWide(font.family);
        // A negative height asks for the character height in pixels, rather than the cell height
        HFONT handle = CreateFontW(-font.size, 0, 0, 0, font.bold ? FW_BOLD : FW_NORMAL, font.italic ? TRUE : FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family.c_str());

        fonts[key] = handle;
        return handle;
    }

    // FR_PRIVATE keeps the font to this process, it's gone when the process ends.
    bool GdiSurface::loadFontFile(const std::string& path) {
        bool gdi = AddFontResourceExW(toWide(path).c_str(), FR_PRIVATE, nullptr) > 0;
        bool directWrite = DirectWriteText::addFontFile(path);
        return gdi || directWrite;
    }

    int GdiSurface::measureText(const std::string& text, const Font& font) {
        if (text.empty()) return 0;
        int width = 0;
        if (DirectWriteText::measure(text, font, width)) return width;

        std::wstring wide = toWide(text);
        HDC dc = GetDC(hwnd);
        HGDIOBJ old = SelectObject(dc, this->font(font));
        SIZE size{};
        GetTextExtentPoint32W(dc, wide.c_str(), static_cast<int>(wide.size()), &size);
        SelectObject(dc, old);
        ReleaseDC(hwnd, dc);
        return size.cx;
    }

    void GdiSurface::captureMouse(bool capture) {
        if (capture) SetCapture(hwnd);
        else if (GetCapture() == hwnd) ReleaseCapture();
    }

    std::string GdiSurface::clipboardText() {
        if (!OpenClipboard(hwnd)) return std::string();

        std::string text;
        if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
            if (auto* wide = static_cast<const wchar_t*>(GlobalLock(data))) {
                text = toUtf8(wide);
                GlobalUnlock(data);
            }
        }
        CloseClipboard();
        return text;
    }

    void GdiSurface::setClipboardText(const std::string& text) {
        if (!OpenClipboard(hwnd)) return;
        EmptyClipboard();

        // The clipboard owns the memory once SetClipboardData succeeds
        std::wstring wide = toWide(text);
        size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* target = GlobalLock(data)) {
                memcpy(target, wide.c_str(), bytes);
                GlobalUnlock(data);
                if (!SetClipboardData(CF_UNICODETEXT, data)) GlobalFree(data);
            } else {
                GlobalFree(data);
            }
        }
        CloseClipboard();
    }

    float GdiSurface::scale() const {
        return static_cast<float>(Dpi::forWindow(hwnd)) / Dpi::Default;
    }

    void GdiSurface::paint(ControlView& host) {
        host.preparePaint();
        PAINTSTRUCT paint;
        HDC windowDc = BeginPaint(hwnd, &paint);

        Rect area = bounds();
        int width = std::max(1, area.width);
        int height = std::max(1, area.height);
        if (!reserveBackBuffer(windowDc, width, height)) {
            EndPaint(hwnd, &paint);
            return;
        }

        RECT client{ 0, 0, width, height };
        FillRect(backDc, &client, GetSysColorBrush(COLOR_WINDOW));

        {
            GdiCanvas canvas(backDc, *this);
            host.paint(canvas);
        }

        BitBlt(windowDc, 0, 0, width, height, backDc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &paint);
    }

    // GdiCanvas

    // GDI+ starts once for the process, the first time a rounded shape is drawn. It's left running until the
    // process ends, as windows can be painting until then.
    static void startGdiplus() {
        static std::once_flag started;
        std::call_once(started, [] {
            ULONG_PTR token = 0;
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&token, &input, nullptr);
        });
    }

    // A rectangle with its edges on x, y, x + width and y + height, each corner rounded by its own radius.
    // A corner of radius 0 is square, a point the path's lines meet at.
    static void addRoundRect(Gdiplus::GraphicsPath& path, float x, float y, float width, float height,
        float topLeft, float topRight, float bottomRight, float bottomLeft) {
        auto corner = [&path](float left, float top, float radius, float startAngle) {
            if (radius > 0) path.AddArc(left, top, radius * 2, radius * 2, startAngle, 90);
            else path.AddLine(left, top, left, top);
        };
        corner(x, y, topLeft, 180);
        corner(x + width - topRight * 2, y, topRight, 270);
        corner(x + width - bottomRight * 2, y + height - bottomRight * 2, bottomRight, 0);
        corner(x, y + height - bottomLeft * 2, bottomLeft, 90);
        path.CloseFigure();
    }

    static void prepare(Gdiplus::Graphics& graphics) {
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    }

    static Gdiplus::Color toGdiplusColor(uint32_t color) {
        return Gdiplus::Color(static_cast<BYTE>(colorAlpha(color)), (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    }

    // An image decoded by GDI+, copied into memory so the file isn't kept open, in the premultiplied format
    // GDI+ draws fastest
    class GdiImage : public ICanvasImage {
    public:
        explicit GdiImage(std::unique_ptr<Gdiplus::Bitmap> bitmap) : bitmap(std::move(bitmap)) {}

        int width() const override { return static_cast<int>(bitmap->GetWidth()); }
        int height() const override { return static_cast<int>(bitmap->GetHeight()); }

        std::unique_ptr<Gdiplus::Bitmap> bitmap;
    };

    // A decoded image copied out of its file or stream, nullptr if it couldn't be decoded
    static std::shared_ptr<ICanvasImage> copyDecoded(Gdiplus::Bitmap& file) {
        if (file.GetLastStatus() != Gdiplus::Ok || file.GetWidth() == 0 || file.GetHeight() == 0) return nullptr;

        auto copy = std::make_unique<Gdiplus::Bitmap>(static_cast<INT>(file.GetWidth()), static_cast<INT>(file.GetHeight()), PixelFormat32bppPARGB);
        {
            Gdiplus::Graphics graphics(copy.get());
            graphics.DrawImage(&file, 0, 0, static_cast<INT>(file.GetWidth()), static_cast<INT>(file.GetHeight()));
        }
        return std::make_shared<GdiImage>(std::move(copy));
    }

    std::shared_ptr<ICanvasImage> GdiSurface::loadImage(const std::string& path) {
        startGdiplus();
        Gdiplus::Bitmap file(toWide(path).c_str());
        return copyDecoded(file);
    }

    // GDI+ reads the stream while the bitmap lives, so the stream is released after the copy
    std::shared_ptr<ICanvasImage> GdiSurface::decodeImage(const std::string& bytes) {
        startGdiplus();
        IStream* stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<UINT>(bytes.size()));
        if (!stream) return nullptr;
        std::shared_ptr<ICanvasImage> decoded;
        {
            Gdiplus::Bitmap file(stream);
            decoded = copyDecoded(file);
        }
        stream->Release();
        return decoded;
    }

    // RGBA rows into GDI+'s premultiplied BGRA
    std::shared_ptr<ICanvasImage> GdiSurface::createImage(const PixelData& pixels) {
        startGdiplus();
        auto bitmap = std::make_unique<Gdiplus::Bitmap>(pixels.width, pixels.height, PixelFormat32bppPARGB);
        if (bitmap->GetLastStatus() != Gdiplus::Ok) return nullptr;

        Gdiplus::Rect whole(0, 0, pixels.width, pixels.height);
        Gdiplus::BitmapData data;
        if (bitmap->LockBits(&whole, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &data) != Gdiplus::Ok) return nullptr;
        for (int y = 0; y < pixels.height; ++y) {
            const uint8_t* from = pixels.rgba.data() + static_cast<size_t>(y) * pixels.width * 4;
            uint8_t* to = static_cast<uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride;
            for (int x = 0; x < pixels.width; ++x, from += 4, to += 4) {
                uint32_t alpha = from[3];
                to[0] = static_cast<uint8_t>((from[2] * alpha + 127) / 255);
                to[1] = static_cast<uint8_t>((from[1] * alpha + 127) / 255);
                to[2] = static_cast<uint8_t>((from[0] * alpha + 127) / 255);
                to[3] = static_cast<uint8_t>(alpha);
            }
        }
        bitmap->UnlockBits(&data);
        return std::make_shared<GdiImage>(std::move(bitmap));
    }

    // An image scaled to the icon's size, keeping its proportions
    static HICON iconFromImage(Gdiplus::Bitmap& file, int size) {
        if (file.GetLastStatus() != Gdiplus::Ok || file.GetWidth() == 0 || file.GetHeight() == 0) return nullptr;

        Gdiplus::Bitmap scaled(size, size, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics graphics(&scaled);
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
            float scale = std::min(static_cast<float>(size) / file.GetWidth(), static_cast<float>(size) / file.GetHeight());
            float width = file.GetWidth() * scale;
            float height = file.GetHeight() * scale;
            graphics.DrawImage(&file, (size - width) / 2, (size - height) / 2, width, height);
        }
        HICON icon = nullptr;
        scaled.GetHICON(&icon);
        return icon;
    }

    HICON loadIconFile(const std::string& path, int size) {
        std::wstring wide = toWide(path);
        size_t dot = path.find_last_of('.');
        std::string extension = dot == std::string::npos ? "" : path.substr(dot);
        for (char& c : extension) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        if (extension == ".ico") {
            return static_cast<HICON>(LoadImageW(nullptr, wide.c_str(), IMAGE_ICON, size, size, LR_LOADFROMFILE));
        }

        startGdiplus();
        Gdiplus::Bitmap file(wide.c_str());
        return iconFromImage(file, size);
    }

    static HICON iconFromIcoData(const std::string& bytes, int size) {
        auto read16 = [&](size_t at) { return static_cast<uint32_t>(static_cast<uint8_t>(bytes[at]) | (static_cast<uint8_t>(bytes[at + 1]) << 8)); };
        auto read32 = [&](size_t at) { return read16(at) | (read16(at + 2) << 16); };

        size_t count = read16(4);
        if (bytes.size() < 6 + count * 16) return nullptr;
        size_t best = count;
        uint32_t bestWidth = 0, bestDepth = 0;
        for (size_t i = 0; i < count; ++i) {
            size_t entry = 6 + i * 16;
            uint32_t width = static_cast<uint8_t>(bytes[entry]);
            if (width == 0) width = 256;
            uint32_t depth = read16(entry + 6);
            uint32_t length = read32(entry + 8);
            uint32_t offset = read32(entry + 12);
            if (length == 0 || offset > bytes.size() || length > bytes.size() - offset) continue;

            bool better;
            if (best == count) better = true;
            else if ((width >= static_cast<uint32_t>(size)) != (bestWidth >= static_cast<uint32_t>(size))) better = width >= static_cast<uint32_t>(size);
            else if (width != bestWidth) better = width >= static_cast<uint32_t>(size) ? width < bestWidth : width > bestWidth;
            else better = depth > bestDepth;
            if (better) {
                best = i;
                bestWidth = width;
                bestDepth = depth;
            }
        }
        if (best == count) return nullptr;

        size_t entry = 6 + best * 16;
        std::vector<BYTE> image(bytes.begin() + read32(entry + 12), bytes.begin() + read32(entry + 12) + read32(entry + 8));
        return CreateIconFromResourceEx(image.data(), static_cast<DWORD>(image.size()), TRUE, 0x00030000, size, size, LR_DEFAULTCOLOR);
    }

    HICON loadIconData(const std::string& bytes, int size) {
        bool isIco = bytes.size() >= 6 && bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 1 && bytes[3] == 0;
        if (isIco) return iconFromIcoData(bytes, size);

        startGdiplus();
        IStream* stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<UINT>(bytes.size()));
        if (!stream) return nullptr;
        HICON icon = nullptr;
        {
            Gdiplus::Bitmap file(stream);
            icon = iconFromImage(file, size);
        }
        stream->Release();
        return icon;
    }

    // Smoothly scaled, with coordinates on pixel edges like GDI's. It draws on the canvas's DC, so the clip
    // applies. A tint other than opaque white scales the channels with a color matrix.
    void GdiCanvas::drawImage(const ICanvasImage& image, const Rect& destination, const Rect& source, uint32_t tint, float rotation) {
        const auto& picture = static_cast<const GdiImage&>(image);
        Gdiplus::Graphics graphics(dc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        if (rotation != 0) {
            float centerX = destination.x + destination.width / 2.0f;
            float centerY = destination.y + destination.height / 2.0f;
            graphics.TranslateTransform(centerX, centerY);
            graphics.RotateTransform(rotation);
            graphics.TranslateTransform(-centerX, -centerY);
        }

        // Clamping the edge pixels keeps transparent fringes from the scaling out of the border
        Gdiplus::ImageAttributes attributes;
        attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        if (tint != 0xFFFFFF) {
            Gdiplus::ColorMatrix matrix{};
            matrix.m[0][0] = ((tint >> 16) & 0xFF) / 255.0f;
            matrix.m[1][1] = ((tint >> 8) & 0xFF) / 255.0f;
            matrix.m[2][2] = (tint & 0xFF) / 255.0f;
            matrix.m[3][3] = colorAlpha(tint) / 255.0f;
            matrix.m[4][4] = 1;
            attributes.SetColorMatrix(&matrix);
        }
        graphics.DrawImage(picture.bitmap.get(), Gdiplus::Rect(destination.x, destination.y, destination.width, destination.height),
            source.x, source.y, source.width, source.height, Gdiplus::UnitPixel, &attributes);
    }
    
    void GdiCanvas::fillRect(const Rect& rect, uint32_t color, const CornerRadii& radii) {
        uint32_t alpha = colorAlpha(color);
        if (alpha == 0) return;
        if (radii.square() && alpha == 255) { // Opaque square corners use the fast path.
            HBRUSH brush = CreateSolidBrush(toColorRef(color));
            RECT r{ rect.x, rect.y, rect.right(), rect.bottom() };
            FillRect(dc, &r, brush);
            DeleteObject(brush);
            return;
        }

        startGdiplus();
        Gdiplus::Graphics graphics(dc);
        prepare(graphics);
        Gdiplus::GraphicsPath path;
        addRoundRect(path, static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width),
            static_cast<float>(rect.height), static_cast<float>(radii.topLeft), static_cast<float>(radii.topRight),
            static_cast<float>(radii.bottomRight), static_cast<float>(radii.bottomLeft));
        Gdiplus::SolidBrush brush(toGdiplusColor(color));
        graphics.FillPath(&brush, &path);
    }

    // The border is inside the rectangle, the outline runs half the width in, and its corners follow the fill's curve
    void GdiCanvas::strokeRect(const Rect& rect, uint32_t color, int width, const CornerRadii& radii) {
        uint32_t alpha = colorAlpha(color);
        if (alpha == 0 || width <= 0) return;
        if (radii.square() && alpha == 255) {
            HPEN pen = CreatePen(PS_INSIDEFRAME, width, toColorRef(color));
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, rect.x, rect.y, rect.right(), rect.bottom());
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
            return;
        }

        startGdiplus();
        Gdiplus::Graphics graphics(dc);
        prepare(graphics);
        float half = width / 2.0f;
        auto inner = [half](int radius) { return radius > 0 ? std::max(0.5f, radius - half) : 0.0f; };
        Gdiplus::GraphicsPath path;
        addRoundRect(path, rect.x + half, rect.y + half, rect.width - static_cast<float>(width), rect.height - static_cast<float>(width),
            inner(radii.topLeft), inner(radii.topRight), inner(radii.bottomRight), inner(radii.bottomLeft));
        Gdiplus::Pen pen(toGdiplusColor(color), static_cast<float>(width));
        graphics.DrawPath(&pen, &path);
    }

    void GdiCanvas::fillGradient(const Rect& rect, const std::vector<GradientStop>& stops, float rotation, const CornerRadii& radii) {
        if (stops.empty() || rect.width <= 0 || rect.height <= 0) return;
        if (stops.size() == 1) {
            fillRect(rect, stops.front().color, radii);
            return;
        }

        startGdiplus();
        Gdiplus::Graphics graphics(dc);
        prepare(graphics);

        std::vector<Gdiplus::Color> colors;
        std::vector<Gdiplus::REAL> positions;
        for (const GradientStop& stop : stops) {
            colors.push_back(toGdiplusColor(stop.color));
            positions.push_back(std::clamp(stop.position, 0.0f, 1.0f));
        }
        positions.front() = 0;
        positions.back() = 1;

        Gdiplus::RectF area(static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height));
        Gdiplus::LinearGradientBrush brush(area, colors.front(), colors.back(), rotation, FALSE);
        brush.SetInterpolationColors(colors.data(), positions.data(), static_cast<INT>(colors.size()));
        // Past its ends the brush repeats mirrored, so the pixel it can be off by at an edge keeps the end color
        brush.SetWrapMode(Gdiplus::WrapModeTileFlipXY);

        Gdiplus::GraphicsPath path;
        addRoundRect(path, area.X, area.Y, area.Width, area.Height, static_cast<float>(radii.topLeft), static_cast<float>(radii.topRight),
            static_cast<float>(radii.bottomRight), static_cast<float>(radii.bottomLeft));
        graphics.FillPath(&brush, &path);
    }

    static void drawBlendedText(HDC dc, HFONT hfont, const std::wstring& text, const Rect& rect, uint32_t color, TextAlignment alignment) {
        startGdiplus();
        Gdiplus::Graphics graphics(dc);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        Gdiplus::Font font(dc, hfont);

        Gdiplus::StringFormat format(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        if (alignment == TextAlignment::Center) format.SetAlignment(Gdiplus::StringAlignmentCenter);
        else if (alignment == TextAlignment::Right) format.SetAlignment(Gdiplus::StringAlignmentFar);
        else format.SetAlignment(Gdiplus::StringAlignmentNear);

        Gdiplus::SolidBrush brush(toGdiplusColor(color));
        Gdiplus::RectF area(static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height));
        graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &font, area, &format, &brush);
    }

    // Not clipped to rect, the current clip limits the text.
    void GdiCanvas::drawText(const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment) {
        uint32_t alpha = colorAlpha(color);
        if (text.empty() || alpha == 0) return;
        if (DirectWriteText::draw(dc, text, rect, color, font, alignment)) return;

        std::wstring wide = toWide(text);
        if (alpha < 255) {
            drawBlendedText(dc, surface.font(font), wide, rect, color, alignment);
            return;
        }

        HGDIOBJ oldFont = SelectObject(dc, surface.font(font));
        SetTextColor(dc, toColorRef(color));
        SetBkMode(dc, TRANSPARENT);

        UINT format = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP;
        if (alignment == TextAlignment::Center) format |= DT_CENTER;
        else if (alignment == TextAlignment::Right) format |= DT_RIGHT;
        else format |= DT_LEFT;

        RECT r{ rect.x, rect.y, rect.right(), rect.bottom() };
        DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &r, format);
        SelectObject(dc, oldFont);
    }

    void GdiCanvas::pushClip(const Rect& rect) {
        SaveDC(dc);
        IntersectClipRect(dc, rect.x, rect.y, rect.right(), rect.bottom());
    }

    void GdiCanvas::popClip() {
        RestoreDC(dc, -1);
    }
} // namespace Luwow::Udc
