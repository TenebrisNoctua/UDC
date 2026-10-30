#include "ImageButton.h"
#include "ControlView.h"

namespace Luwow::Udc {
    // Pressed wins while the pointer is still over the button, like TextButton
    void ImageButton::paint(ICanvas& canvas, const Rect& frame) {
        bool showPressed = pressed && hovered;
        std::optional<uint32_t> background = backgroundColor;
        if (showPressed && pressedColor) background = pressedColor;
        else if (hovered && hoverColor) background = hoverColor;

        const ImageSource* shown = &image;
        if (showPressed && !pressedImage.empty()) shown = &pressedImage;
        else if (hovered && !hoverImage.empty()) shown = &hoverImage;

        paintBox(canvas, frame, background, borderColor);
        paintImage(canvas, *shown);
    }

    void ImageButton::onMouseDown(int x, int y) {
        invalidate();
    }

    void ImageButton::onMouseUp(int x, int y, bool inside) {
        invalidate();
        if (inside) view->fireEvent(*this, "OnActivated");
    }

    // Luau

    void ImageButton::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        ImageObject::readDescriptor(L);
        readImage(L, "HoverImage", hoverImage);
        readImage(L, "PressedImage", pressedImage);
        readColor(L, "HoverColor", hoverColor);
        readColor(L, "PressedColor", pressedColor);
        readHandler(L, "OnActivated", *this);
    }

    // nil keeps the background color
    static int getHoverColor(lua_State* L, ImageButton& object) {
        ControlLuau::pushColor(L, object.hoverColor);
        return 1;
    }

    static int setHoverColor(lua_State* L, ImageButton& object) {
        object.hoverColor = ControlLuau::checkColor(L, 3);
        object.invalidate();
        return 0;
    }

    static int getPressedColor(lua_State* L, ImageButton& object) {
        ControlLuau::pushColor(L, object.pressedColor);
        return 1;
    }

    static int setPressedColor(lua_State* L, ImageButton& object) {
        object.pressedColor = ControlLuau::checkColor(L, 3);
        object.invalidate();
        return 0;
    }

    void ImageButton::RegisterClass(lua_State* L) {
        RegisterGuiObject<ImageButton>(L, {}, ControlLuau::extend<ImageButton>(ImageObject::properties<ImageButton>(), {
            { "HoverImage", Bind<&getImageProperty<ImageButton, &ImageButton::hoverImage>>,
                Bind<&setImageProperty<ImageButton, &ImageButton::hoverImage>> },
            { "PressedImage", Bind<&getImageProperty<ImageButton, &ImageButton::pressedImage>>,
                Bind<&setImageProperty<ImageButton, &ImageButton::pressedImage>> },
            { "HoverColor", Bind<getHoverColor>, Bind<setHoverColor> },
            { "PressedColor", Bind<getPressedColor>, Bind<setPressedColor> },
        }));
    }
} // namespace Luwow::Udc
