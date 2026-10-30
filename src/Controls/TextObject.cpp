#include "TextObject.h"
#include "ControlView.h"

namespace Luwow::Udc {
    static const char* const alignmentNames[] = { "Left", "Center", "Right", nullptr };

    // FontStyle is bold and italic together, in this order: neither, bold, italic, both
    static const char* const fontStyleNames[] = { "Regular", "Bold", "Italic", "BoldItalic", nullptr };

    static int fontStyleOf(const Font& font) {
        return (font.bold ? 1 : 0) | (font.italic ? 2 : 0);
    }

    static void setFontStyleOf(Font& font, int style) {
        font.bold = (style & 1) != 0;
        font.italic = (style & 2) != 0;
    }

    size_t nextCharacter(const std::string& text, size_t index) {
        if (index >= text.size()) return text.size();
        ++index;
        while (index < text.size() && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) ++index;
        return index;
    }

    size_t previousCharacter(const std::string& text, size_t index) {
        if (index == 0) return 0;
        --index;
        while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) --index;
        return index;
    }

    // Each line takes as many words as fit. The space a line breaks at belongs to neither line.
    std::vector<TextLine> TextObject::layoutLines(const std::string& value, int width) const {
        auto measure = [&](size_t start, size_t end) {
            return (view && end > start) ? view->measureText(value.substr(start, end - start), font) : 0;
        };

        std::vector<TextLine> lines;
        auto wrap = [&](size_t start, size_t end) {
            if (!textWrapped || width <= 0) {
                lines.push_back({ start, end, measure(start, end) });
                return;
            }
            size_t lineStart = start;
            while (true) {
                int full = measure(lineStart, end);
                if (full <= width) {
                    lines.push_back({ lineStart, end, full });
                    return;
                }

                // The last space the line still fits before
                size_t breakAt = std::string::npos;
                for (size_t index = lineStart; index < end; ++index) {
                    if (value[index] != ' ') continue;
                    if (index > lineStart && measure(lineStart, index) > width) break;
                    breakAt = index;
                }
                if (breakAt != std::string::npos && breakAt > lineStart) {
                    lines.push_back({ lineStart, breakAt, measure(lineStart, breakAt) });
                    lineStart = breakAt + 1;
                    continue;
                }

                // No space fits, so the word breaks between characters, at least one per line
                size_t cut = nextCharacter(value, lineStart);
                while (cut < end) {
                    size_t next = nextCharacter(value, cut);
                    if (measure(lineStart, next) > width) break;
                    cut = next;
                }
                lines.push_back({ lineStart, cut, measure(lineStart, cut) });
                lineStart = cut;
            }
        };

        size_t start = 0;
        while (true) {
            size_t end = multiLine ? value.find('\n', start) : std::string::npos;
            if (end == std::string::npos) end = value.size();
            wrap(start, end);
            if (end >= value.size()) break;
            start = end + 1;
        }
        return lines;
    }

    // Whole characters are dropped from the end, and the spaces before the ellipsis with them
    std::string TextObject::fitLine(const std::string& line, int width) const {
        if (!truncated || !view || view->measureText(line, font) <= width) return line;
        static const std::string ellipsis = "\xE2\x80\xA6";
        size_t end = line.size();
        while (end > 0) {
            end = previousCharacter(line, end);
            size_t kept = end;
            while (kept > 0 && line[kept - 1] == ' ') --kept;
            std::string candidate = line.substr(0, kept) + ellipsis;
            if (view->measureText(candidate, font) <= width) return candidate;
        }
        return view->measureText(ellipsis, font) <= width ? ellipsis : std::string();
    }

    void TextObject::drawLines(ICanvas& canvas, const Rect& area, const std::string& value, uint32_t color) const {
        std::vector<TextLine> lines = layoutLines(value, area.width);
        if (lines.size() == 1 && area.height >= lineHeight()) {
            canvas.drawText(fitLine(value.substr(lines[0].start, lines[0].end - lines[0].start), area.width), area, color, font, textAlignment);
            return;
        }

        int height = lineHeight();
        int top = area.y + (area.height - height * static_cast<int>(lines.size())) / 2;
        for (const TextLine& line : lines) {
            Rect lineArea{ area.x, top, area.width, height };
            canvas.drawText(fitLine(value.substr(line.start, line.end - line.start), area.width), lineArea, color, font, textAlignment);
            top += height;
        }
    }

    // Each line of text is as tall as the font with a little room, like TextInput's line
    void TextObject::measureContent(float width, float height, int& contentWidth, int& contentHeight) const {
        ControlObject::measureContent(width, height, contentWidth, contentHeight);
        int wrapWidth = growsX(automaticSize) ? wrapLimit : static_cast<int>(width);
        std::vector<TextLine> lines = layoutLines(text, wrapWidth);
        int textWidth = 0;
        for (const TextLine& line : lines) textWidth = std::max(textWidth, line.width);
        contentWidth = std::max(contentWidth, textWidth);
        contentHeight = std::max(contentHeight, lineHeight() * static_cast<int>(lines.size()));
    }

    void TextObject::setText(const std::string& value) {
        text = value;
        invalidate();
    }

    // Luau

    void TextObject::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        ControlObject::readDescriptor(L);
        readString(L, "Text", text);
        readColor(L, "TextColor", textColor);
        readInt(L, "TextSize", font.size);
        readString(L, "Font", font.family);
        readEnum(L, "TextAlignment", textAlignment, alignmentNames);
        readBool(L, "MultiLine", multiLine);
        readBool(L, "Truncated", truncated);
        readBool(L, "TextWrapped", textWrapped);
        if (field(L, "FontStyle")) {
            setFontStyleOf(font, luaL_checkoption(L, -1, nullptr, fontStyleNames));
            lua_pop(L, 1);
        }
    }

    int TextObject::Luau::getText(lua_State* L, TextObject& object) {
        lua_pushlstring(L, object.text.data(), object.text.size());
        return 1;
    }

    int TextObject::Luau::setText(lua_State* L, TextObject& object) {
        size_t length = 0;
        const char* value = luaL_checklstring(L, 3, &length);
        object.setText(std::string(value, length));
        return 0;
    }

    int TextObject::Luau::getTextColor(lua_State* L, TextObject& object) {
        ControlLuau::pushColor(L, object.textColor);
        return 1;
    }

    int TextObject::Luau::setTextColor(lua_State* L, TextObject& object) {
        object.textColor = ControlLuau::checkColor(L, 3).value_or(0x000000);
        object.invalidate();
        return 0;
    }

    int TextObject::Luau::getTextSize(lua_State* L, TextObject& object) {
        lua_pushinteger(L, object.font.size);
        return 1;
    }

    int TextObject::Luau::setTextSize(lua_State* L, TextObject& object) {
        int size = luaL_checkinteger(L, 3);
        if (size <= 0) luaL_argerror(L, 3, "must be positive");
        object.font.size = size;
        object.invalidate();
        return 0;
    }

    int TextObject::Luau::getFont(lua_State* L, TextObject& object) {
        lua_pushlstring(L, object.font.family.data(), object.font.family.size());
        return 1;
    }

    // An empty string uses the platform's default UI font
    int TextObject::Luau::setFont(lua_State* L, TextObject& object) {
        object.font.family = luaL_checkstring(L, 3);
        object.invalidate();
        return 0;
    }

    int TextObject::Luau::getTextAlignment(lua_State* L, TextObject& object) {
        ControlLuau::pushEnum(L, object.textAlignment, alignmentNames);
        return 1;
    }

    int TextObject::Luau::setTextAlignment(lua_State* L, TextObject& object) {
        object.textAlignment = ControlLuau::checkEnum<TextAlignment>(L, 3, alignmentNames);
        object.invalidate();
        return 0;
    }

    int TextObject::Luau::getFontStyle(lua_State* L, TextObject& object) {
        lua_pushstring(L, fontStyleNames[fontStyleOf(object.font)]);
        return 1;
    }

    int TextObject::Luau::setFontStyle(lua_State* L, TextObject& object) {
        setFontStyleOf(object.font, luaL_checkoption(L, 3, nullptr, fontStyleNames));
        object.invalidate();
        return 0;
    }

    int TextObject::Luau::getMultiLine(lua_State* L, TextObject& object) {
        lua_pushboolean(L, object.multiLine);
        return 1;
    }

    int TextObject::Luau::setMultiLine(lua_State* L, TextObject& object) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        object.multiLine = lua_toboolean(L, 3);
        object.invalidate();
        return 0;
    }

    int TextObject::Luau::getTruncated(lua_State* L, TextObject& object) {
        lua_pushboolean(L, object.truncated);
        return 1;
    }

    int TextObject::Luau::setTruncated(lua_State* L, TextObject& object) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        object.truncated = lua_toboolean(L, 3);
        object.invalidate();
        return 0;
    }

    int TextObject::Luau::getTextWrapped(lua_State* L, TextObject& object) {
        lua_pushboolean(L, object.textWrapped);
        return 1;
    }

    int TextObject::Luau::setTextWrapped(lua_State* L, TextObject& object) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        object.textWrapped = lua_toboolean(L, 3);
        object.invalidate();
        return 0;
    }
} // namespace Luwow::Udc
