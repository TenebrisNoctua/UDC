#pragma once

#include "ObjectUserdata.h"
#include "ImageObject.h"

namespace Luwow::Udc {
    // A button showing an image, firing OnActivated when released over it. While
    // the pointer is over it or pressing it, it shows the hover or pressed image and background color
    // instead, when it has them.
    class ImageButton : public ImageObject, public Luwow::Objects::ObjectUserdata<ImageButton> {
    public:
        static constexpr const char* Name = "ImageButton";
        using Super = ImageObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        ImageButton() : ImageObject(ControlKind::ImageButton) {}

        // Empty keeps the image.
        ImageSource hoverImage;
        ImageSource pressedImage;
        // nullopt keeps the background color
        std::optional<uint32_t> hoverColor;
        std::optional<uint32_t> pressedColor;

        void paint(ICanvas& canvas, const Rect& frame) override;
        bool activeByDefault() const override { return true; }
        void onMouseDown(int x, int y) override;
        void onMouseUp(int x, int y, bool inside) override;

        // Luau
        void readDescriptor(lua_State* L) override;
    };
} // namespace Luwow::Udc
