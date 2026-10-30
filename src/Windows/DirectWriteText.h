#pragma once

#include "Controls/Canvas.h"

#include <string>
#include <windows.h>

// Text drawn and measured with DirectWrite.
namespace Luwow::Udc::DirectWriteText {
    // Makes a font file's families usable by name
    bool addFontFile(const std::string& path);

    // The width of one line of text in device pixels, the font's size being in device pixels too
    bool measure(const std::string& text, const Font& font, int& width);

    // Draws one line of text on the DC, vertically centered in rect and aligned horizontally within it,
    // blended by the color's alpha. Like GDI's, it isn't clipped to rect, only to the DC's clip.
    bool draw(HDC dc, const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment);

    // A top-down 32-bit DIB section, such as a paint's back buffer, which text is blended into in place
    // while it's selected into the DC drawn on. Forgotten again before it's deleted.
    void addBackBuffer(HBITMAP bitmap);
    void removeBackBuffer(HBITMAP bitmap);
} // namespace Luwow::Udc::DirectWriteText
