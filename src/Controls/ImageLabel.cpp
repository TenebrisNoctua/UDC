#include "ImageLabel.h"

namespace Luwow::Udc {
    void ImageLabel::RegisterClass(lua_State* L) {
        RegisterGuiObject<ImageLabel>(L, {}, ImageObject::properties<ImageLabel>());
    }
} // namespace Luwow::Udc
