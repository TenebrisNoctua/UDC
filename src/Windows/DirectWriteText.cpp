#include "DirectWriteText.h"
#include "GdiCanvas.h"

#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

namespace Luwow::Udc::DirectWriteText {
    using Microsoft::WRL::ComPtr;

    // A text's glyph coverage, drawn once and blended at each paint. left and top place it from the text's
    // rectangle, which the glyphs may overhang.
    struct Mask {
        int left = 0;
        int top = 0;
        int width = 0;
        int height = 0;
        std::vector<uint8_t> coverage;
    };

    // The masks of the texts drawn lately, by text, font, box and alignment. The least recently drawn are
    // dropped past the limit, so a window's texts stay cached without the memory growing.
    class MaskCache {
    public:
        using Key = std::tuple<std::string, std::string, int, bool, bool, int, int, int>;

        Mask* find(const Key& key) {
            auto found = entries.find(key);
            if (found == entries.end()) return nullptr;
            order.splice(order.begin(), order, found->second.second);
            return &found->second.first;
        }

        Mask& insert(const Key& key, Mask&& mask) {
            bytes += mask.coverage.size();
            order.push_front(key);
            auto& entry = entries[key];
            entry = { std::move(mask), order.begin() };
            Mask& inserted = entry.first;
            while (bytes > Limit && order.size() > 1) {
                auto oldest = entries.find(order.back());
                bytes -= oldest->second.first.coverage.size();
                entries.erase(oldest);
                order.pop_back();
            }
            return inserted;
        }

        void clear() {
            entries.clear();
            order.clear();
            bytes = 0;
        }

    private:
        static constexpr size_t Limit = 4 * 1024 * 1024;
        std::list<Key> order;
        std::map<Key, std::pair<Mask, std::list<Key>::iterator>> entries;
        size_t bytes = 0;
    };

    struct GlyphMask {
        int left = 0;
        int top = 0;
        int width = 0;
        int height = 0;
        std::vector<uint8_t> coverage;
    };

    // How many positions within a pixel a glyph is drawn at, along each axis
    static constexpr int GlyphPhases = 32;

    // A glyph of a font face at a size and a vertical fraction, drawn at every horizontal fraction at once
    using GlyphKey = std::tuple<IDWriteFontFace*, float, UINT16, int>;
    using GlyphPositions = std::array<GlyphMask, GlyphPhases>;

    // The factories and the fonts, shared by every window. 
    struct State {
        std::mutex mutex;
        bool started = false;
        ComPtr<IDWriteFactory5> factory;
        ComPtr<ID2D1Factory> d2d;
        ComPtr<IWICImagingFactory> wic;
        ComPtr<IDWriteFontCollection1> collection;
        ComPtr<IDWriteRenderingParams> renderingParams;
        std::vector<std::wstring> files;
        std::map<std::tuple<std::wstring, bool, bool, int>, ComPtr<IDWriteTextFormat>> formats;
        MaskCache masks;
        // The glyphs texts are put together from. Their faces are held, so a key's address is never reused.
        std::map<GlyphKey, GlyphPositions> glyphs;
        std::map<IDWriteFontFace*, ComPtr<IDWriteFontFace>> glyphFaces;
        size_t glyphBytes = 0;
    };

    // At exit COM and the factories' DLLs may already be gone, and releasing into them
    // crashes. The process ending frees it all.
    static State& state() {
        static State* instance = new State();
        return *instance;
    }

    // The system's fonts and the added files, in one collection
    static bool buildCollection(State& s) {
        ComPtr<IDWriteFontSetBuilder1> builder;
        ComPtr<IDWriteFontSet> system;
        if (FAILED(s.factory->CreateFontSetBuilder(&builder)) || FAILED(s.factory->GetSystemFontSet(&system))) return false;
        builder->AddFontSet(system.Get());
        for (const std::wstring& file : s.files) {
            ComPtr<IDWriteFontFile> fontFile;
            if (SUCCEEDED(s.factory->CreateFontFileReference(file.c_str(), nullptr, &fontFile))) builder->AddFontFile(fontFile.Get());
        }

        ComPtr<IDWriteFontSet> set;
        ComPtr<IDWriteFontCollection1> collection;
        if (FAILED(builder->CreateFontSet(&set)) || FAILED(s.factory->CreateFontCollectionFromFontSet(set.Get(), &collection))) return false;
        s.collection = collection;
        s.formats.clear();
        s.masks.clear();
        s.glyphs.clear();
        s.glyphFaces.clear();
        s.glyphBytes = 0;
        return true;
    }

    // Under the lock. Font sets need Windows 10, older versions keep GDI.
    static bool start(State& s) {
        if (s.started) return s.collection != nullptr;
        s.started = true;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5), reinterpret_cast<IUnknown**>(s.factory.GetAddressOf())))) return false;
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, s.d2d.GetAddressOf()))) return false;
        // WIC is COM. The thread may have COM in either model already, which is fine for WIC.
        HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        (void)com;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&s.wic)))) return false;

        ComPtr<IDWriteRenderingParams3> params;
        if (SUCCEEDED(s.factory->CreateCustomRenderingParams(1.0f, 0.0f, 0.0f, 0.0f, DWRITE_PIXEL_GEOMETRY_FLAT,
                DWRITE_RENDERING_MODE1_OUTLINE, DWRITE_GRID_FIT_MODE_DISABLED, &params))) {
            s.renderingParams = params;
        }
        return buildCollection(s);
    }

    bool addFontFile(const std::string& path) {
        State& s = state();
        std::lock_guard lock(s.mutex);
        if (!start(s)) return false;

        wchar_t full[MAX_PATH * 4];
        std::wstring wide = toWide(path);
        DWORD length = GetFullPathNameW(wide.c_str(), static_cast<DWORD>(std::size(full)), full, nullptr);
        if (length == 0 || length >= std::size(full) || GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) return false;
        std::wstring file(full, length);
        if (std::find(s.files.begin(), s.files.end(), file) != s.files.end()) return true;
        s.files.push_back(file);
        return buildCollection(s);
    }

    static bool hasFamily(State& s, const std::wstring& name) {
        UINT32 index = 0;
        BOOL exists = FALSE;
        return SUCCEEDED(s.collection->FindFamilyName(name.c_str(), &index, &exists)) && exists;
    }

    // A family name GDI style, such as "Builder Sans Medium", is DirectWrite's family with a weight
    static void resolveFamily(State& s, const std::string& family, bool bold, std::wstring& name, DWRITE_FONT_WEIGHT& weight) {
        name = family.empty() ? L"Segoe UI" : toWide(family);
        weight = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
        if (hasFamily(s, name)) return;

        static const std::pair<const wchar_t*, DWRITE_FONT_WEIGHT> weights[] = {
            { L"Thin", DWRITE_FONT_WEIGHT_THIN }, { L"ExtraLight", DWRITE_FONT_WEIGHT_EXTRA_LIGHT },
            { L"Light", DWRITE_FONT_WEIGHT_LIGHT }, { L"Regular", DWRITE_FONT_WEIGHT_NORMAL },
            { L"Medium", DWRITE_FONT_WEIGHT_MEDIUM }, { L"SemiBold", DWRITE_FONT_WEIGHT_SEMI_BOLD },
            { L"Bold", DWRITE_FONT_WEIGHT_BOLD }, { L"ExtraBold", DWRITE_FONT_WEIGHT_EXTRA_BOLD },
            { L"Black", DWRITE_FONT_WEIGHT_BLACK }, { L"Heavy", DWRITE_FONT_WEIGHT_HEAVY },
        };
        for (const auto& [word, value] : weights) {
            std::wstring suffix = std::wstring(L" ") + word;
            if (name.size() <= suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
            std::wstring base = name.substr(0, name.size() - suffix.size());
            if (!hasFamily(s, base)) continue;
            name = base;
            weight = bold ? std::max(value, DWRITE_FONT_WEIGHT_BOLD) : value;
            return;
        }
    }

    // Under the lock
    static IDWriteTextFormat* formatFor(State& s, const Font& font) {
        auto key = std::make_tuple(toWide(font.family), font.bold, font.italic, font.size);
        auto cached = s.formats.find(key);
        if (cached != s.formats.end()) return cached->second.Get();

        std::wstring name;
        DWRITE_FONT_WEIGHT weight;
        resolveFamily(s, font.family, font.bold, name, weight);
        ComPtr<IDWriteTextFormat> format;
        if (FAILED(s.factory->CreateTextFormat(name.c_str(), s.collection.Get(), weight,
                font.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                static_cast<float>(font.size), L"", &format))) {
            return nullptr;
        }
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        s.formats[key] = format;
        return format.Get();
    }

    static ComPtr<IDWriteTextLayout> layoutFor(State& s, const std::string& text, const Font& font, float width, float height) {
        IDWriteTextFormat* format = formatFor(s, font);
        if (!format) return nullptr;
        std::wstring wide = toWide(text);
        ComPtr<IDWriteTextLayout> layout;
        if (FAILED(s.factory->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()), format, width, height, &layout))) return nullptr;
        return layout;
    }

    bool measure(const std::string& text, const Font& font, int& width) {
        State& s = state();
        std::lock_guard lock(s.mutex);
        if (!start(s)) return false;
        ComPtr<IDWriteTextLayout> layout = layoutFor(s, text, font, 100000.0f, static_cast<float>(font.size * 2));
        DWRITE_TEXT_METRICS metrics;
        if (!layout || FAILED(layout->GetMetrics(&metrics))) return false;
        width = static_cast<int>(std::ceil(metrics.widthIncludingTrailingWhitespace));
        return true;
    }

    // A 32-bit DIB of at least a size, its pixels within reach
    struct Pixels {
        HDC dc = nullptr;
        HBITMAP bitmap = nullptr;
        uint32_t* bits = nullptr;
        int width = 0;
        int height = 0;

        ~Pixels() {
            if (dc) DeleteDC(dc);
            if (bitmap) DeleteObject(bitmap);
        }

        bool reserve(int w, int h) {
            if (bits && w <= width && h <= height) return true;
            w = std::max(w, width);
            h = std::max(h, height);
            if (!dc) dc = CreateCompatibleDC(nullptr);
            if (!dc) return false;
            BITMAPINFO info{};
            info.bmiHeader = { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB };
            void* memory = nullptr;
            HBITMAP created = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &memory, nullptr, 0);
            if (!created) return false;
            SelectObject(dc, created);
            if (bitmap) DeleteObject(bitmap);
            bitmap = created;
            bits = static_cast<uint32_t*>(memory);
            width = w;
            height = h;
            return true;
        }
    };

    // A software render target drawing the glyphs' coverage, white on transparent, into
    // a WIC bitmap, and a copy of the destination for a DC that isn't a DIB.
    struct Target {
        ComPtr<IWICBitmap> bitmap;
        ComPtr<ID2D1RenderTarget> renderTarget;
        ComPtr<ID2D1SolidColorBrush> white;
        int width = 0;
        int height = 0;
        Pixels destination;

        void reset() {
            white.Reset();
            renderTarget.Reset();
            bitmap.Reset();
            width = height = 0;
        }
    };

    // The target, at least the size, grown to the largest size asked for so far
    static Target* targetFor(State& s, int width, int height) {
        // Kept for the thread's life and never destroyed, for the same reason as the state
        thread_local Target* targetSlot = new Target();
        Target& target = *targetSlot;
        if (target.renderTarget && width <= target.width && height <= target.height) return &target;

        int w = std::max(width, target.width);
        int h = std::max(height, target.height);
        target.reset();
        if (FAILED(s.wic->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &target.bitmap))) return nullptr;

        D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
        if (FAILED(s.d2d->CreateWicBitmapRenderTarget(target.bitmap.Get(), &properties, &target.renderTarget)) ||
            FAILED(target.renderTarget->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &target.white))) {
            target.reset();
            return nullptr;
        }
        target.renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        if (s.renderingParams) target.renderTarget->SetTextRenderingParams(s.renderingParams.Get());
        target.width = w;
        target.height = h;
        return &target;
    }

    // The back buffers added, which are top-down 32-bit DIB sections. A DIB section doesn't say reliably
    // whether its rows run up or down, so only these are drawn into in place.
    static std::mutex backBuffersMutex;
    static std::vector<HBITMAP> backBuffers;

    void addBackBuffer(HBITMAP bitmap) {
        std::lock_guard lock(backBuffersMutex);
        backBuffers.push_back(bitmap);
    }

    void removeBackBuffer(HBITMAP bitmap) {
        std::lock_guard lock(backBuffersMutex);
        backBuffers.erase(std::remove(backBuffers.begin(), backBuffers.end(), bitmap), backBuffers.end());
    }

    // The 32-bit pixels a DC draws into, when it's a back buffer without a transform
    struct DirectPixels {
        uint8_t* bits = nullptr;
        int stride = 0;
        int width = 0;
        int height = 0;
        bool bottomUp = false;

        uint32_t* row(int y) const {
            int line = bottomUp ? height - 1 - y : y;
            return reinterpret_cast<uint32_t*>(bits + static_cast<ptrdiff_t>(line) * stride);
        }
    };

    static bool directPixelsOf(HDC dc, DirectPixels& pixels) {
        HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(dc, OBJ_BITMAP));
        {
            std::lock_guard lock(backBuffersMutex);
            if (!bitmap || std::find(backBuffers.begin(), backBuffers.end(), bitmap) == backBuffers.end()) return false;
        }
        DIBSECTION section{};
        if (GetObject(bitmap, sizeof(section), &section) != sizeof(DIBSECTION)) return false;
        if (section.dsBm.bmBitsPixel != 32 || !section.dsBm.bmBits) return false;

        POINT viewport{}, window{};
        GetViewportOrgEx(dc, &viewport);
        GetWindowOrgEx(dc, &window);
        if (viewport.x || viewport.y || window.x || window.y || GetGraphicsMode(dc) != GM_COMPATIBLE) return false;

        pixels.bits = static_cast<uint8_t*>(section.dsBm.bmBits);
        pixels.stride = section.dsBm.bmWidthBytes;
        pixels.width = section.dsBm.bmWidth;
        pixels.height = std::abs(section.dsBm.bmHeight);
        pixels.bottomUp = false;
        return true;
    }

    // sRGB to linear light, and back from linear light quantized to 4096 steps. Also the opacity each
    // level of glyph coverage blends with.
    struct Gamma {
        float linear[256];
        uint8_t srgb[4096];
        float coverage[256];

        Gamma() {
            // Round letters dip a fraction of a pixel under the baseline, as fonts draw them, which leaves
            // faint coverage in the row below. Coverage under the threshold is
            // dropped and the rest stretched to keep the strokes' weight.
            const float threshold = 0.18f;
            for (int i = 0; i < 256; ++i) coverage[i] = std::clamp((i / 255.0f - threshold) / (1 - threshold), 0.0f, 1.0f);
            for (int i = 0; i < 256; ++i) {
                float c = i / 255.0f;
                linear[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
            }
            for (int i = 0; i < 4096; ++i) {
                float l = i / 4095.0f;
                float c = l <= 0.0031308f ? l * 12.92f : 1.055f * std::pow(l, 1 / 2.4f) - 0.055f;
                srgb[i] = static_cast<uint8_t>(std::lround(std::clamp(c, 0.0f, 1.0f) * 255));
            }
        }
    };

    static const Gamma& gamma() {
        static const Gamma table;
        return table;
    }

    // A glyph drawn at each horizontal fraction of a pixel side by side in one pass, each cut to its ink
    static const GlyphPositions* glyphPositions(State& s, IDWriteFontFace* face, float size, UINT16 index, int phaseY) {
        GlyphKey key{ face, size, index, phaseY };
        auto found = s.glyphs.find(key);
        if (found != s.glyphs.end()) return &found->second;

        // Past the limit the glyphs are dropped together, and drawn again as they're needed
        if (s.glyphBytes > 16 * 1024 * 1024) {
            s.glyphs.clear();
            s.glyphFaces.clear();
            s.glyphBytes = 0;
        }

        int cell = static_cast<int>(std::ceil(size * 3)) + 4;
        float originX = std::ceil(size);
        float originY = std::ceil(size * 2);
        Target* target = targetFor(s, cell * GlyphPhases, cell);
        if (!target) return nullptr;

        FLOAT advance = 0;
        DWRITE_GLYPH_RUN run{ face, size, 1, &index, &advance, nullptr, FALSE, 0 };
        ID2D1RenderTarget* renderTarget = target->renderTarget.Get();
        renderTarget->BeginDraw();
        renderTarget->PushAxisAlignedClip(D2D1::RectF(0, 0, static_cast<float>(cell * GlyphPhases), static_cast<float>(cell)), D2D1_ANTIALIAS_MODE_ALIASED);
        renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        for (int phase = 0; phase < GlyphPhases; ++phase) {
            D2D1_POINT_2F origin = D2D1::Point2F(cell * phase + originX + static_cast<float>(phase) / GlyphPhases,
                originY + static_cast<float>(phaseY) / GlyphPhases);
            renderTarget->DrawGlyphRun(origin, &run, target->white.Get(), DWRITE_MEASURING_MODE_NATURAL);
        }
        renderTarget->PopAxisAlignedClip();
        HRESULT result = renderTarget->EndDraw();
        if (result == D2DERR_RECREATE_TARGET) target->reset();
        if (FAILED(result)) return nullptr;

        WICRect lockArea{ 0, 0, cell * GlyphPhases, cell };
        ComPtr<IWICBitmapLock> lock;
        UINT stride = 0, bytes = 0;
        BYTE* bits = nullptr;
        if (FAILED(target->bitmap->Lock(&lockArea, WICBitmapLockRead, &lock)) || FAILED(lock->GetStride(&stride)) ||
            FAILED(lock->GetDataPointer(&bytes, &bits))) {
            return nullptr;
        }
        auto alphaAt = [&](int x, int y) {
            return static_cast<uint8_t>(reinterpret_cast<const uint32_t*>(bits + static_cast<size_t>(y) * stride)[x] >> 24);
        };

        GlyphPositions positions;
        for (int phase = 0; phase < GlyphPhases; ++phase) {
            int minX = cell, minY = cell, maxX = -1, maxY = -1;
            for (int y = 0; y < cell; ++y) {
                for (int x = 0; x < cell; ++x) {
                    if (!alphaAt(cell * phase + x, y)) continue;
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                    minY = std::min(minY, y);
                    maxY = std::max(maxY, y);
                }
            }
            if (maxX < 0) continue;

            GlyphMask& glyph = positions[phase];
            glyph.left = minX - static_cast<int>(originX);
            glyph.top = minY - static_cast<int>(originY);
            glyph.width = maxX - minX + 1;
            glyph.height = maxY - minY + 1;
            glyph.coverage.resize(static_cast<size_t>(glyph.width) * glyph.height);
            for (int y = 0; y < glyph.height; ++y) {
                for (int x = 0; x < glyph.width; ++x) glyph.coverage[static_cast<size_t>(y) * glyph.width + x] = alphaAt(cell * phase + minX + x, minY + y);
            }
            s.glyphBytes += glyph.coverage.size();
        }

        s.glyphFaces.emplace(face, face);
        return &s.glyphs.emplace(key, std::move(positions)).first->second;
    }

    // Adds up the glyphs of a layout's runs into a mask, each from its drawing at the nearest fraction of a pixel
    class GlyphCompositor : public IDWriteTextRenderer {
    public:
        GlyphCompositor(State& s, Mask& mask) : s(s), mask(mask) {}

        bool failed = false;

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override {
            if (id == __uuidof(IUnknown) || id == __uuidof(IDWritePixelSnapping) || id == __uuidof(IDWriteTextRenderer)) {
                *object = this;
                return S_OK;
            }
            *object = nullptr;
            return E_NOINTERFACE;
        }
        // Kept on the stack while the layout draws
        ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
        ULONG STDMETHODCALLTYPE Release() override { return 1; }

        HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override {
            *disabled = FALSE;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override {
            *transform = { 1, 0, 0, 1, 0, 0 };
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixels) override {
            *pixels = 1;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return S_OK; }

        HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baselineX, FLOAT baselineY, DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN* run,
                const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) override {
            bool rightToLeft = (run->bidiLevel & 1) != 0;
            float pen = baselineX;
            for (UINT32 i = 0; i < run->glyphCount; ++i) {
                float advance = run->glyphAdvances ? run->glyphAdvances[i] : 0;
                if (rightToLeft) pen -= advance;
                float offsetX = run->glyphOffsets ? run->glyphOffsets[i].advanceOffset : 0;
                float x = pen + (rightToLeft ? -offsetX : offsetX);
                float y = baselineY - (run->glyphOffsets ? run->glyphOffsets[i].ascenderOffset : 0);
                if (!rightToLeft) pen += advance;

                int pixelX = static_cast<int>(std::floor(x));
                int pixelY = static_cast<int>(std::floor(y));
                int phaseX = std::min(GlyphPhases - 1, static_cast<int>(std::lround((x - pixelX) * GlyphPhases)));
                int phaseY = std::min(GlyphPhases - 1, static_cast<int>(std::lround((y - pixelY) * GlyphPhases)));
                const GlyphPositions* positions = glyphPositions(s, run->fontFace, run->fontEmSize, run->glyphIndices[i], phaseY);
                if (!positions) {
                    failed = true;
                    return E_FAIL;
                }
                add((*positions)[phaseX], pixelX, pixelY);
            }
            return S_OK;
        }

    private:
        State& s;
        Mask& mask;

        // Saturating, so glyphs that touch add up like their outlines filled together
        void add(const GlyphMask& glyph, int x, int y) {
            for (int row = 0; row < glyph.height; ++row) {
                int maskY = y + glyph.top + row;
                if (maskY < 0 || maskY >= mask.height) continue;
                const uint8_t* source = glyph.coverage.data() + static_cast<size_t>(row) * glyph.width;
                uint8_t* destination = mask.coverage.data() + static_cast<size_t>(maskY) * mask.width;
                for (int column = 0; column < glyph.width; ++column) {
                    int maskX = x + glyph.left + column;
                    if (maskX < 0 || maskX >= mask.width || source[column] == 0) continue;
                    destination[maskX] = static_cast<uint8_t>(std::min(255, destination[maskX] + source[column]));
                }
            }
        }
    };

    // Draws a text's coverage into a mask: its whole box with the glyphs' overhang, whatever is clipped
    static bool renderMask(State& s, const std::string& text, const Font& font, int boxWidth, int boxHeight, TextAlignment alignment, Mask& mask) {
        ComPtr<IDWriteTextLayout> layout = layoutFor(s, text, font, static_cast<float>(std::max(0, boxWidth)), static_cast<float>(std::max(0, boxHeight)));
        if (!layout) return false;
        layout->SetTextAlignment(alignment == TextAlignment::Center ? DWRITE_TEXT_ALIGNMENT_CENTER
            : alignment == TextAlignment::Right ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);

        DWRITE_OVERHANG_METRICS overhang;
        if (FAILED(layout->GetOverhangMetrics(&overhang))) return false;
        int left = static_cast<int>(std::ceil(std::max(0.0f, overhang.left))) + 1;
        int top = static_cast<int>(std::ceil(std::max(0.0f, overhang.top))) + 1;
        int right = static_cast<int>(std::ceil(std::max(0.0f, overhang.right))) + 1;
        int bottom = static_cast<int>(std::ceil(std::max(0.0f, overhang.bottom))) + 1;
        mask.left = -left;
        mask.top = -top;
        mask.width = std::max(1, std::max(0, boxWidth) + left + right);
        mask.height = std::max(1, std::max(0, boxHeight) + top + bottom);

        mask.coverage.assign(static_cast<size_t>(mask.width) * mask.height, 0);
        GlyphCompositor compositor(s, mask);
        if (FAILED(layout->Draw(nullptr, &compositor, static_cast<float>(left), static_cast<float>(top))) || compositor.failed) return false;
        return true;
    }

    // The glyphs' coverage, drawn once into a mask and cached, blends the color over the DC's pixels in linear light.
    bool draw(HDC dc, const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment) {
        State& s = state();
        std::lock_guard lock(s.mutex);
        if (!start(s)) return false;

        MaskCache::Key key{ text, font.family, font.size, font.bold, font.italic, rect.width, rect.height, static_cast<int>(alignment) };
        Mask* mask = s.masks.find(key);
        if (!mask) {
            Mask rendered;
            if (!renderMask(s, text, font, rect.width, rect.height, alignment, rendered)) return false;
            mask = &s.masks.insert(key, std::move(rendered));
        }

        RECT covered{ rect.x + mask->left, rect.y + mask->top, rect.x + mask->left + mask->width, rect.y + mask->top + mask->height };
        RECT clip;
        if (GetClipBox(dc, &clip) == ERROR) return false;
        RECT bound;
        if (!IntersectRect(&bound, &covered, &clip)) return true;
        int width = bound.right - bound.left;
        int height = bound.bottom - bound.top;
        int maskX = bound.left - covered.left;
        int maskY = bound.top - covered.top;

        // Blended straight into the DC's pixels when it's the back buffer, otherwise into a copy of them
        DirectPixels direct;
        bool isDirect = directPixelsOf(dc, direct);
        Target* target = isDirect ? nullptr : targetFor(s, 1, 1);
        if (!isDirect) {
            if (!target || !target->destination.reserve(width, height)) return false;
            GdiFlush();
            if (!BitBlt(target->destination.dc, 0, 0, width, height, dc, bound.left, bound.top, SRCCOPY)) return false;
        }
        GdiFlush();

        const Gamma& table = gamma();
        float opacity = colorAlpha(color) / 255.0f;
        const float shade[3] = { table.linear[color & 0xFF], table.linear[(color >> 8) & 0xFF], table.linear[(color >> 16) & 0xFF] };
        for (int y = 0; y < height; ++y) {
            const uint8_t* coverage = mask->coverage.data() + static_cast<size_t>(maskY + y) * mask->width + maskX;
            uint32_t* pixels = isDirect ? direct.row(bound.top + y) + bound.left : target->destination.bits + y * target->destination.width;
            for (int x = 0; x < width; ++x) {
                uint32_t amount = coverage[x];
                if (amount == 0) continue;
                float a = table.coverage[amount] * opacity;
                if (a <= 0) continue;
                uint32_t pixel = pixels[x];
                uint32_t blended = 0;
                for (int channel = 0; channel < 3; ++channel) {
                    float below = table.linear[(pixel >> (channel * 8)) & 0xFF];
                    float mixed = below + (shade[channel] - below) * a;
                    blended |= static_cast<uint32_t>(table.srgb[static_cast<int>(mixed * 4095.0f + 0.5f)]) << (channel * 8);
                }
                pixels[x] = blended | (pixel & 0xFF000000);
            }
        }

        if (isDirect) return true;
        return BitBlt(dc, bound.left, bound.top, width, height, target->destination.dc, 0, 0, SRCCOPY) != FALSE;
    }
} // namespace Luwow::Udc::DirectWriteText
