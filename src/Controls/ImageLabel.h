#pragma once

#include "ObjectUserdata.h"
#include "ImageObject.h"

namespace Luwow::Udc {
    // An image
    class ImageLabel : public ImageObject, public Luwow::Objects::ObjectUserdata<ImageLabel> {
    public:
        static constexpr const char* Name = "ImageLabel";
        using Super = ImageObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        ImageLabel() : ImageObject(ControlKind::ImageLabel) {}
    };
} // namespace Luwow::Udc
