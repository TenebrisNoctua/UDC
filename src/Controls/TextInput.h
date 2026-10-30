#pragma once

#include "ObjectUserdata.h"
#include "TextObject.h"

#include <algorithm>

namespace Luwow::Udc {
    // How a TextInput shows its text.
    enum class TextInputType { Default, Password };

    // A text box with a selection. Fires OnChanged(text) on edits, OnSubmitted(text) on Enter, and
    // OnFocusLost(text) when it stops being focused.
    class TextInput : public TextObject, public Luwow::Objects::ObjectUserdata<TextInput> {
    public:
        static constexpr const char* Name = "TextInput";
        using Super = TextObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        TextInput();

        std::string placeholderText;
        uint32_t placeholderColor = 0x8A8A8A;
        // The border while focused, none when cleared
        std::optional<uint32_t> focusedBorderColor = 0x0078D7;
        uint32_t selectionColor = 0xACD4F6;
        TextInputType inputType = TextInputType::Default;

        // Byte offsets of the caret and the other end of the selection, always on character boundaries.
        // The selection is the text between them, empty when they're equal.
        size_t caret = 0;
        size_t anchor = 0;

        // Replaces the text and moves the caret to its end, without firing OnChanged
        void setText(const std::string& value) override;

        bool focusable() const override { return true; }
        bool activeByDefault() const override { return true; }
        void paint(ICanvas& canvas, const Rect& frame) override;
        void onMouseDown(int x, int y) override;
        void onMouseMove(int x, int y) override;
        void onMouseUp(int x, int y, bool inside) override;
        void onKey(Key key, KeyModifiers modifiers) override;
        void onText(const std::string& text) override;
        void onFocusChanged() override;

        // Luau
        void readDescriptor(lua_State* L) override;

    private:
        static constexpr int Padding = 6;

        // Where the text goes in the frame, within a Padding child's insets, or the built-in padding without
        // one, along X only for a single line
        Rect textBounds(const Rect& frame) const;

        // How far the text is scrolled left, or up for several lines, to keep the caret visible
        int textScroll = 0;
        int textScrollY = 0;
        // Where Up and Down keep the caret across lines, -1 until one of them is pressed
        int caretGoalX = -1;
        bool selecting = false;

        bool hasSelection() const { return caret != anchor; }
        size_t selectionStart() const { return std::min(caret, anchor); }
        size_t selectionEnd() const { return std::max(caret, anchor); }

        // Whether the text is laid out on lines, rather than as the one scrolled sideways
        bool usesLines() const { return (multiLine || textWrapped) && inputType != TextInputType::Password; }
        // The area the text is drawn in, the frame without the padding: the sides for one line, every side for several
        Rect textArea() const;
        std::vector<TextLine> lines() const;
        // The line the caret at index is on
        size_t lineOf(const std::vector<TextLine>& lines, size_t index) const;
        // The character boundary on a line nearest to x, measured from the start of the line
        size_t indexInLine(const TextLine& line, int x) const;

        int measure(const std::string& value) const;
        // The text as shown up to a byte offset: itself, or a dot per character for passwords
        std::string shown(size_t end) const;
        // The width of the shown text up to a byte offset
        int measurePrefix(size_t end) const;
        // The character boundary nearest to a window position
        size_t indexAt(int x, int y) const;
        // Moves the caret, extending the selection with shift
        void moveCaret(size_t index, bool extend);
        // Removes the selected text, returns whether there was any
        bool deleteSelection();
        void insert(const std::string& value);
        void changed();
        void paintLines(ICanvas& canvas, const Rect& frame);
    };
} // namespace Luwow::Udc
