#pragma once

#include "ControlObject.h"

#include <vector>

namespace Luwow::Udc {
    // Text is UTF-8. These give the first byte of the next and of the previous character, stopping at the
    // ends of the text.
    size_t nextCharacter(const std::string& text, size_t index);
    size_t previousCharacter(const std::string& text, size_t index);

    // One line of laid out text: the bytes of the text it shows, and its width in pixels
    struct TextLine {
        size_t start = 0;
        size_t end = 0;
        int width = 0;
    };

    // Shared by the controls showing text
    class TextObject : public ControlObject {
    public:
        static constexpr const char* Name = "TextObject";
        using Super = ControlObject;

        using ControlObject::ControlObject;

        std::string text;
        uint32_t textColor = 0x000000;
        Font font;
        TextAlignment textAlignment = TextAlignment::Left;

        bool multiLine = false;
        bool textWrapped = false;
        bool truncated = false;

        // The height of one line of text, with a little room
        int lineHeight() const { return font.size + 4; }

        // Breaks value into lines, at its newlines with MultiLine, and to fit width with TextWrapped. A width
        // of 0 or less doesn't wrap.
        std::vector<TextLine> layoutLines(const std::string& value, int width) const;

        // Draws value's lines in area, the block of them centered vertically and each aligned by TextAlignment
        void drawLines(ICanvas& canvas, const Rect& area, const std::string& value, uint32_t color) const;
        // The line cut to fit width with an ellipsis, with Truncated, or as it is
        std::string fitLine(const std::string& line, int width) const;

        // Replaces the text
        virtual void setText(const std::string& value);

        // The text's size, or the children's when they take more. The text wraps to the width unless the
        // control grows along X.
        void measureContent(float width, float height, int& contentWidth, int& contentHeight) const override;

        // Luau
        void readDescriptor(lua_State* L) override;

        // The Luau properties every text control has
        template <typename T>
        static ControlLuau::Properties<T> properties() {
            return ControlLuau::extend<T>(ControlObject::properties<T>(), {
                { "Text", T::template Bind<&Luau::getText>, T::template Bind<&Luau::setText> },
                { "TextColor", T::template Bind<&Luau::getTextColor>, T::template Bind<&Luau::setTextColor> },
                { "TextSize", T::template Bind<&Luau::getTextSize>, T::template Bind<&Luau::setTextSize> },
                { "Font", T::template Bind<&Luau::getFont>, T::template Bind<&Luau::setFont> },
                { "TextAlignment", T::template Bind<&Luau::getTextAlignment>, T::template Bind<&Luau::setTextAlignment> },
                { "FontStyle", T::template Bind<&Luau::getFontStyle>, T::template Bind<&Luau::setFontStyle> },
                { "MultiLine", T::template Bind<&Luau::getMultiLine>, T::template Bind<&Luau::setMultiLine> },
                { "TextWrapped", T::template Bind<&Luau::getTextWrapped>, T::template Bind<&Luau::setTextWrapped> },
                { "Truncated", T::template Bind<&Luau::getTruncated>, T::template Bind<&Luau::setTruncated> },
            });
        }

    private:
        // The Luau properties
        struct Luau {
            static int getText(lua_State* L, TextObject& object);
            static int setText(lua_State* L, TextObject& object);
            static int getTextColor(lua_State* L, TextObject& object);
            static int setTextColor(lua_State* L, TextObject& object);
            static int getTextSize(lua_State* L, TextObject& object);
            static int setTextSize(lua_State* L, TextObject& object);
            static int getFont(lua_State* L, TextObject& object);
            static int setFont(lua_State* L, TextObject& object);
            static int getTextAlignment(lua_State* L, TextObject& object);
            static int setTextAlignment(lua_State* L, TextObject& object);
            static int getFontStyle(lua_State* L, TextObject& object);
            static int setFontStyle(lua_State* L, TextObject& object);
            static int getMultiLine(lua_State* L, TextObject& object);
            static int setMultiLine(lua_State* L, TextObject& object);
            static int getTruncated(lua_State* L, TextObject& object);
            static int setTruncated(lua_State* L, TextObject& object);
            static int getTextWrapped(lua_State* L, TextObject& object);
            static int setTextWrapped(lua_State* L, TextObject& object);
        };
    };
} // namespace Luwow::Udc
