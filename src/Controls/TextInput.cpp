#include "TextInput.h"
#include "Padding.h"
#include "ControlView.h"

#include "lua.h"

#include <cstdlib>

namespace Luwow::Udc {
    TextInput::TextInput() : TextObject(ControlKind::TextInput) {
        backgroundColor = 0xFFFFFF;
        borderColor = 0x7A7A7A;
    }

    void TextInput::setText(const std::string& value) {
        text = value;
        caret = text.size();
        anchor = caret;
        invalidate();
    }

    int TextInput::measure(const std::string& value) const {
        return view ? view->measureText(value, font) : 0;
    }

    // U+2022, the bullet password boxes show
    static constexpr const char* PasswordDot = "\xE2\x80\xA2";

    std::string TextInput::shown(size_t end) const {
        if (inputType != TextInputType::Password) return text.substr(0, end);

        std::string dots;
        for (size_t index = 0; index < end && index < text.size(); index = nextCharacter(text, index)) dots += PasswordDot;
        return dots;
    }

    int TextInput::measurePrefix(size_t end) const {
        return measure(shown(end));
    }

    Rect TextInput::textBounds(const Rect& area) const {
        if (const ControlObject* padding = findHelper(ControlKind::Padding)) return static_cast<const Udc::Padding*>(padding)->inset(area);
        if (!usesLines()) return Rect{ area.x + Padding, area.y, std::max(0, area.width - Padding * 2), area.height };
        return Rect{ area.x + Padding, area.y + Padding, std::max(0, area.width - Padding * 2), std::max(0, area.height - Padding * 2) };
    }

    Rect TextInput::textArea() const {
        return textBounds(frame());
    }

    std::vector<TextLine> TextInput::lines() const {
        return layoutLines(text, textArea().width);
    }

    // A caret at the end of a line that wraps is on that line, the next one starts after the space
    size_t TextInput::lineOf(const std::vector<TextLine>& lines, size_t index) const {
        for (size_t line = 0; line < lines.size(); ++line) {
            if (index <= lines[line].end) return line;
        }
        return lines.empty() ? 0 : lines.size() - 1;
    }

    size_t TextInput::indexInLine(const TextLine& line, int x) const {
        size_t best = line.start;
        int bestDistance = std::abs(x);
        for (size_t index = nextCharacter(text, line.start); index <= line.end && index > line.start; index = nextCharacter(text, index)) {
            int distance = std::abs(measure(text.substr(line.start, index - line.start)) - x);
            if (distance < bestDistance) {
                best = index;
                bestDistance = distance;
            }
            if (index >= line.end) break;
        }
        return best;
    }

    void TextInput::paint(ICanvas& canvas, const Rect& frame) {
        paintBox(canvas, frame, backgroundColor, focused ? focusedBorderColor : borderColor);
        if (usesLines()) {
            paintLines(canvas, frame);
            return;
        }

        Rect inner = textBounds(frame);

        // Scrolls the text just enough to keep the caret inside the box
        int caretX = measurePrefix(caret);
        if (caretX - textScroll > inner.width) textScroll = caretX - inner.width;
        if (caretX - textScroll < 0) textScroll = caretX;
        int fullWidth = measurePrefix(text.size());
        textScroll = std::max(0, std::min(textScroll, std::max(0, fullWidth - inner.width)));

        bool showFocus = focused && view && view->isWindowFocused();
        int lineHeight = font.size + 4;
        int lineTop = inner.y + (inner.height - lineHeight) / 2;

        canvas.pushClip(inner);
        if (focused && hasSelection()) {
            int startX = measurePrefix(selectionStart());
            int endX = measurePrefix(selectionEnd());
            canvas.fillRect(Rect{ inner.x + startX - textScroll, lineTop, endX - startX, lineHeight }, selectionColor, 0);
        }

        Rect textRect{ inner.x - textScroll, inner.y, std::max(inner.width, fullWidth) + 1, inner.height };
        if (text.empty() && !placeholderText.empty()) {
            canvas.drawText(placeholderText, textRect, placeholderColor, font, TextAlignment::Left);
        } else {
            canvas.drawText(shown(text.size()), textRect, textColor, font, TextAlignment::Left);
        }

        if (showFocus) canvas.fillRect(Rect{ inner.x + caretX - textScroll, lineTop, 1, lineHeight }, textColor, 0);
        canvas.popClip();
    }

    // Several lines start at the top, scrolled down just enough to keep the caret's line inside the box
    void TextInput::paintLines(ICanvas& canvas, const Rect& frame) {
        Rect inner = textArea();
        std::vector<TextLine> laidOut = lines();
        int height = lineHeight();

        size_t caretLine = lineOf(laidOut, caret);
        int caretTop = static_cast<int>(caretLine) * height;
        if (caretTop - textScrollY < 0) textScrollY = caretTop;
        if (caretTop + height - textScrollY > inner.height) textScrollY = caretTop + height - inner.height;
        int fullHeight = height * static_cast<int>(laidOut.size());
        textScrollY = std::max(0, std::min(textScrollY, std::max(0, fullHeight - inner.height)));

        auto lineX = [&](const TextLine& line, size_t index) {
            return measure(text.substr(line.start, std::min(index, line.end) - line.start));
        };

        canvas.pushClip(inner);
        if (focused && hasSelection()) {
            size_t start = selectionStart();
            size_t end = selectionEnd();
            for (size_t index = 0; index < laidOut.size(); ++index) {
                const TextLine& line = laidOut[index];
                if (end < line.start || start > line.end) continue;
                int left = lineX(line, std::max(start, line.start));
                int right = lineX(line, std::min(end, line.end));
                // A selection going on past the line covers its break too
                if (end > line.end) right += font.size / 3;
                int top = inner.y + static_cast<int>(index) * height - textScrollY;
                if (right > left) canvas.fillRect(Rect{ inner.x + left, top, right - left, height }, selectionColor, 0);
            }
        }

        if (text.empty() && !placeholderText.empty()) {
            canvas.drawText(placeholderText, Rect{ inner.x, inner.y - textScrollY, inner.width, height }, placeholderColor, font, TextAlignment::Left);
        } else {
            for (size_t index = 0; index < laidOut.size(); ++index) {
                const TextLine& line = laidOut[index];
                Rect area{ inner.x, inner.y + static_cast<int>(index) * height - textScrollY, inner.width, height };
                canvas.drawText(text.substr(line.start, line.end - line.start), area, textColor, font, TextAlignment::Left);
            }
        }

        if (focused && view && view->isWindowFocused() && !laidOut.empty()) {
            int caretX = lineX(laidOut[caretLine], caret);
            canvas.fillRect(Rect{ inner.x + caretX, inner.y + caretTop - textScrollY, 1, height }, textColor, 0);
        }
        canvas.popClip();
    }

    size_t TextInput::indexAt(int x, int y) const {
        if (usesLines()) {
            Rect inner = textArea();
            std::vector<TextLine> laidOut = lines();
            if (laidOut.empty()) return 0;
            int line = (y - inner.y + textScrollY) / lineHeight();
            line = std::clamp(line, 0, static_cast<int>(laidOut.size()) - 1);
            return indexInLine(laidOut[line], x - inner.x);
        }

        int target = x - textArea().x + textScroll;

        size_t best = 0;
        int bestDistance = std::abs(target);
        for (size_t index = nextCharacter(text, 0); index <= text.size(); index = nextCharacter(text, index)) {
            int distance = std::abs(measurePrefix(index) - target);
            if (distance < bestDistance) {
                best = index;
                bestDistance = distance;
            }
            if (index == text.size()) break;
        }
        return best;
    }

    void TextInput::moveCaret(size_t index, bool extend) {
        caretGoalX = -1;
        caret = std::min(index, text.size());
        if (!extend) anchor = caret;
        invalidate();
    }

    // Clicking places the caret, dragging selects. Shift+click extends the selection.
    void TextInput::onMouseDown(int x, int y) {
        moveCaret(indexAt(x, y), view && view->modifiers().shift);
        selecting = true;
    }

    void TextInput::onMouseMove(int x, int y) {
        if (selecting) moveCaret(indexAt(x, y), true);
    }

    void TextInput::onMouseUp(int x, int y, bool inside) {
        selecting = false;
    }

    bool TextInput::deleteSelection() {
        if (!hasSelection()) return false;
        size_t start = selectionStart();
        text.erase(start, selectionEnd() - start);
        caret = anchor = start;
        return true;
    }

    // Replaces the selection, if any, with value
    void TextInput::insert(const std::string& value) {
        bool removed = deleteSelection();
        if (value.empty()) {
            if (removed) changed();
            return;
        }

        text.insert(caret, value);
        caret += value.size();
        anchor = caret;
        changed();
    }

    void TextInput::changed() {
        invalidate();
        std::string current = text;
        view->fireEvent(*this, "OnChanged", [current](lua_State* L) {
            lua_pushlstring(L, current.data(), current.size());
            return 1;
        });
    }

    void TextInput::onKey(Key key, KeyModifiers modifiers) {
        switch (key) {
            case Key::Left:
                // Without shift, a selection collapses to its start
                if (hasSelection() && !modifiers.shift) moveCaret(selectionStart(), false);
                else moveCaret(previousCharacter(text, caret), modifiers.shift);
                break;
            case Key::Right:
                if (hasSelection() && !modifiers.shift) moveCaret(selectionEnd(), false);
                else moveCaret(nextCharacter(text, caret), modifiers.shift);
                break;
            // On lines, Home and End go to the caret's line's ends, and with Ctrl (Cmd) to the text's
            case Key::Home:
                if (usesLines() && !modifiers.control) {
                    std::vector<TextLine> laidOut = lines();
                    if (!laidOut.empty()) moveCaret(laidOut[lineOf(laidOut, caret)].start, modifiers.shift);
                } else {
                    moveCaret(0, modifiers.shift);
                }
                break;
            case Key::End:
                if (usesLines() && !modifiers.control) {
                    std::vector<TextLine> laidOut = lines();
                    if (!laidOut.empty()) moveCaret(laidOut[lineOf(laidOut, caret)].end, modifiers.shift);
                } else {
                    moveCaret(text.size(), modifiers.shift);
                }
                break;
            // The caret keeps its x across lines, going to the text's ends past the first and last
            case Key::Up:
            case Key::Down: {
                if (!usesLines()) break;
                std::vector<TextLine> laidOut = lines();
                if (laidOut.empty()) break;
                size_t line = lineOf(laidOut, caret);
                int goal = caretGoalX >= 0 ? caretGoalX : measure(text.substr(laidOut[line].start, caret - laidOut[line].start));
                size_t target;
                if (key == Key::Up) target = line == 0 ? 0 : indexInLine(laidOut[line - 1], goal);
                else target = line + 1 >= laidOut.size() ? text.size() : indexInLine(laidOut[line + 1], goal);
                moveCaret(target, modifiers.shift);
                caretGoalX = goal;
                break;
            }
            case Key::Backspace:
                if (deleteSelection()) {
                    changed();
                } else if (caret > 0) {
                    size_t start = previousCharacter(text, caret);
                    text.erase(start, caret - start);
                    caret = anchor = start;
                    changed();
                }
                break;
            case Key::Delete:
                if (deleteSelection()) {
                    changed();
                } else if (caret < text.size()) {
                    text.erase(caret, nextCharacter(text, caret) - caret);
                    changed();
                }
                break;
            case Key::SelectAll:
                anchor = 0;
                caret = text.size();
                invalidate();
                break;
            // A password can't leave the input
            case Key::Copy:
                if (inputType == TextInputType::Password) break;
                if (hasSelection()) view->surface->setClipboardText(text.substr(selectionStart(), selectionEnd() - selectionStart()));
                break;
            case Key::Cut:
                if (inputType == TextInputType::Password) break;
                if (hasSelection()) {
                    view->surface->setClipboardText(text.substr(selectionStart(), selectionEnd() - selectionStart()));
                    deleteSelection();
                    changed();
                }
                break;
            case Key::Paste: {
                // Every line with MultiLine, its newlines made plain ones, otherwise only the first
                std::string pasted = view->surface->clipboardText();
                if (multiLine && inputType != TextInputType::Password) {
                    std::string lines;
                    for (size_t index = 0; index < pasted.size(); ++index) {
                        if (pasted[index] == '\r') {
                            lines.push_back('\n');
                            if (index + 1 < pasted.size() && pasted[index + 1] == '\n') ++index;
                        } else {
                            lines.push_back(pasted[index]);
                        }
                    }
                    pasted = lines;
                } else {
                    size_t lineEnd = pasted.find_first_of("\r\n");
                    if (lineEnd != std::string::npos) pasted.resize(lineEnd);
                }
                if (!pasted.empty()) insert(pasted);
                break;
            }
            case Key::Enter: {
                if (multiLine && inputType != TextInputType::Password) {
                    insert("\n");
                    break;
                }
                std::string current = text;
                view->fireEvent(*this, "OnSubmitted", [current](lua_State* L) {
                    lua_pushlstring(L, current.data(), current.size());
                    return 1;
                });
                break;
            }
            case Key::Escape:
                view->setFocus(nullptr);
                break;
            default:
                break;
        }
    }

    void TextInput::onText(const std::string& value) {
        std::string filtered;
        for (char c : value) {
            if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7F) filtered.push_back(c);
        }
        if (!filtered.empty()) insert(filtered);
    }

    void TextInput::onFocusChanged() {
        invalidate();
        selecting = false;
        if (!focused) {
            std::string current = text;
            view->fireEvent(*this, "OnFocusLost", [current](lua_State* L) {
                lua_pushlstring(L, current.data(), current.size());
                return 1;
            });
        }
    }

    // Luau

    static const char* const inputTypeNames[] = { "Default", "Password", nullptr };

    void TextInput::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        TextObject::readDescriptor(L);
        readString(L, "PlaceholderText", placeholderText);
        readColor(L, "PlaceholderColor", placeholderColor);
        readColor(L, "SelectionColor", selectionColor);
        readColor(L, "FocusedBorderColor", focusedBorderColor);
        readEnum(L, "TextInputType", inputType, inputTypeNames);
        caret = anchor = text.size();
        readHandler(L, "OnChanged", *this);
        readHandler(L, "OnSubmitted", *this);
        readHandler(L, "OnFocusLost", *this);
    }

    // input:Focus()
    static int focus(lua_State* L, TextInput& object) {
        object.view->setFocus(&object);
        return 0;
    }

    // input:ReleaseFocus()
    static int releaseFocus(lua_State* L, TextInput& object) {
        if (object.focused) object.view->setFocus(nullptr);
        return 0;
    }

    static int getFocused(lua_State* L, TextInput& object) {
        lua_pushboolean(L, object.focused);
        return 1;
    }

    static int getPlaceholderText(lua_State* L, TextInput& object) {
        lua_pushlstring(L, object.placeholderText.data(), object.placeholderText.size());
        return 1;
    }

    static int setPlaceholderText(lua_State* L, TextInput& object) {
        object.placeholderText = luaL_checkstring(L, 3);
        object.invalidate();
        return 0;
    }

    static int getPlaceholderColor(lua_State* L, TextInput& object) {
        ControlLuau::pushColor(L, object.placeholderColor);
        return 1;
    }

    static int setPlaceholderColor(lua_State* L, TextInput& object) {
        object.placeholderColor = ControlLuau::checkColor(L, 3).value_or(0x8A8A8A);
        object.invalidate();
        return 0;
    }

    static int getSelectionColor(lua_State* L, TextInput& object) {
        ControlLuau::pushColor(L, object.selectionColor);
        return 1;
    }

    static int setSelectionColor(lua_State* L, TextInput& object) {
        object.selectionColor = ControlLuau::checkColor(L, 3).value_or(0xACD4F6);
        object.invalidate();
        return 0;
    }

    static int getFocusedBorderColor(lua_State* L, TextInput& object) {
        if (object.focusedBorderColor) ControlLuau::pushColor(L, *object.focusedBorderColor);
        else lua_pushboolean(L, false);
        return 1;
    }

    // false draws no border while focused
    static int setFocusedBorderColor(lua_State* L, TextInput& object) {
        if (lua_isboolean(L, 3) && !lua_toboolean(L, 3)) object.focusedBorderColor.reset();
        else object.focusedBorderColor = ControlLuau::checkColor(L, 3).value_or(0x0078D7);
        object.invalidate();
        return 0;
    }

    // "Default" or "Password"
    static int getTextInputType(lua_State* L, TextInput& object) {
        ControlLuau::pushEnum(L, object.inputType, inputTypeNames);
        return 1;
    }

    static int setTextInputType(lua_State* L, TextInput& object) {
        object.inputType = ControlLuau::checkEnum<TextInputType>(L, 3, inputTypeNames);
        object.invalidate();
        return 0;
    }

    void TextInput::RegisterClass(lua_State* L) {
        RegisterGuiObject<TextInput>(L, {
            { "Focus", Bind<focus> },
            { "ReleaseFocus", Bind<releaseFocus> },
        }, ControlLuau::extend<TextInput>(TextObject::properties<TextInput>(), {
            { "Focused", Bind<getFocused> },
            { "PlaceholderText", Bind<getPlaceholderText>, Bind<setPlaceholderText> },
            { "PlaceholderColor", Bind<getPlaceholderColor>, Bind<setPlaceholderColor> },
            { "SelectionColor", Bind<getSelectionColor>, Bind<setSelectionColor> },
            { "FocusedBorderColor", Bind<getFocusedBorderColor>, Bind<setFocusedBorderColor> },
            { "TextInputType", Bind<getTextInputType>, Bind<setTextInputType> },
        }));
    }
} // namespace Luwow::Udc
