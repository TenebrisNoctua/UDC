#include "ImageObject.h"
#include "ControlView.h"

#include "RectCore.h"
#include "UDimCore.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

namespace Luwow::Udc {
    static const char* const scaleTypeNames[] = { "Stretch", "Fit", "Crop", "Slice", "Tile", nullptr };

    // The registry's weak table of the buffers pixels were last read as, by their serial
    static const char* const pixelBuffersKey = "Luwow.Udc.PixelBuffers";
    static std::atomic<uint64_t> nextPixelSerial{ 1 };

    static void pushPixelBuffers(lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, pixelBuffersKey);
        if (lua_istable(L, -1)) return;
        lua_pop(L, 1);
        lua_newtable(L);
        lua_createtable(L, 0, 1);
        lua_pushstring(L, "v");
        lua_setfield(L, -2, "__mode");
        lua_setmetatable(L, -2);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, pixelBuffersKey);
    }

    ICanvasImage* ImageObject::currentImage(const ImageSource& source) {
        if (source != loadedSource) {
            loadedSource = source;
            loaded = (view && !source.empty()) ? view->image(source) : nullptr;
        }
        return loaded.get();
    }

    Rect ImageObject::sourceRect(const ICanvasImage& picture) const {
        Rect whole{ 0, 0, picture.width(), picture.height() };
        if (rectWidth <= 0 || rectHeight <= 0) return whole;
        return whole.intersect(Rect{ rectX, rectY, rectWidth, rectHeight });
    }

    void ImageObject::paint(ICanvas& canvas, const Rect& frame) {
        ControlObject::paint(canvas, frame);
        paintImage(canvas, image);
    }

    // The scale type decides which part of the image goes where, Fit shrinks the destination, Crop the source
    void ImageObject::paintImage(ICanvas& canvas, const ImageSource& shown) {
        if (colorAlpha(imageColor) == 0) return;
        ICanvasImage* picture = currentImage(shown);
        if (!picture || picture->width() <= 0 || picture->height() <= 0) return;

        Rect inner = innerFrame();
        if (inner.width <= 0 || inner.height <= 0) return;

        Rect source = sourceRect(*picture);
        if (source.width <= 0 || source.height <= 0) return;

        canvas.pushClip(inner);
        if (scaleType == ScaleType::Slice) {
            paintSliced(canvas, *picture, source, inner);
        } else if (scaleType == ScaleType::Tile) {
            paintTiled(canvas, *picture, source, inner);
        } else {
            Rect destination = inner;
            float scaleX = static_cast<float>(inner.width) / source.width;
            float scaleY = static_cast<float>(inner.height) / source.height;

            if (scaleType == ScaleType::Fit) {
                float scale = std::min(scaleX, scaleY);
                int width = static_cast<int>(std::lround(source.width * scale));
                int height = static_cast<int>(std::lround(source.height * scale));
                destination = Rect{ inner.x + (inner.width - width) / 2, inner.y + (inner.height - height) / 2, width, height };
            } else if (scaleType == ScaleType::Crop) {
                float scale = std::max(scaleX, scaleY);
                int width = std::min(source.width, static_cast<int>(std::lround(inner.width / scale)));
                int height = std::min(source.height, static_cast<int>(std::lround(inner.height / scale)));
                source = Rect{ source.x + (source.width - width) / 2, source.y + (source.height - height) / 2, width, height };
            }
            canvas.drawImage(*picture, destination, source, imageColor, rotation);
        }
        canvas.popClip();
    }

    // Rendering nine sliced parts.
    void ImageObject::paintSliced(ICanvas& canvas, const ICanvasImage& picture, const Rect& source, const Rect& inner) {
        Rect center = sliceCenter;
        if (center.width <= 0 && center.height <= 0 && center.x <= 0 && center.y <= 0) {
            canvas.drawImage(picture, inner, source, imageColor);
            return;
        }
        center = Rect{ 0, 0, source.width, source.height }.intersect(center);

        int sourceLeft = center.x;
        int sourceTop = center.y;
        int sourceRight = source.width - center.right();
        int sourceBottom = source.height - center.bottom();

        float scale = std::max(0.0f, sliceScale);
        float left = sourceLeft * scale;
        float right = sourceRight * scale;
        float top = sourceTop * scale;
        float bottom = sourceBottom * scale;
        float fit = 1;
        if (left + right > inner.width && left + right > 0) fit = std::min(fit, inner.width / (left + right));
        if (top + bottom > inner.height && top + bottom > 0) fit = std::min(fit, inner.height / (top + bottom));

        int destinationLeft = static_cast<int>(std::lround(left * fit));
        int destinationRight = static_cast<int>(std::lround(right * fit));
        int destinationTop = static_cast<int>(std::lround(top * fit));
        int destinationBottom = static_cast<int>(std::lround(bottom * fit));

        const int sourceColumns[4] = { source.x, source.x + sourceLeft, source.x + source.width - sourceRight, source.right() };
        const int sourceRows[4] = { source.y, source.y + sourceTop, source.y + source.height - sourceBottom, source.bottom() };
        const int destinationColumns[4] = { inner.x, inner.x + destinationLeft, inner.right() - destinationRight, inner.right() };
        const int destinationRows[4] = { inner.y, inner.y + destinationTop, inner.bottom() - destinationBottom, inner.bottom() };

        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                Rect from{ sourceColumns[column], sourceRows[row], sourceColumns[column + 1] - sourceColumns[column], sourceRows[row + 1] - sourceRows[row] };
                Rect to{ destinationColumns[column], destinationRows[row], destinationColumns[column + 1] - destinationColumns[column],
                    destinationRows[row + 1] - destinationRows[row] };
                if (from.width <= 0 || from.height <= 0 || to.width <= 0 || to.height <= 0) continue;
                canvas.drawImage(picture, to, from, imageColor);
            }
        }
    }

    // From the top left, the last row and column cut off by the clip
    void ImageObject::paintTiled(ICanvas& canvas, const ICanvasImage& picture, const Rect& source, const Rect& inner) {
        int tileWidth = static_cast<int>(std::lround(tileSize.x.resolve(static_cast<float>(inner.width))));
        int tileHeight = static_cast<int>(std::lround(tileSize.y.resolve(static_cast<float>(inner.height))));
        if (tileWidth <= 0 || tileHeight <= 0) return;

        for (int y = inner.y; y < inner.bottom(); y += tileHeight) {
            for (int x = inner.x; x < inner.right(); x += tileWidth) {
                canvas.drawImage(picture, Rect{ x, y, tileWidth, tileHeight }, source, imageColor);
            }
        }
    }

    // Luau

    // A buffer's pixels have no size of their own, so the Size property is used.
    ImageSource ImageObject::checkImage(lua_State* L, int index) const {
        ImageSource source;
        if (lua_isbuffer(L, index)) {
            size_t length = 0;
            const void* data = lua_tobuffer(L, index, &length);
            if (size.x.scale != 0 || size.y.scale != 0) {
                luaL_error(L, "a buffer image's size is the %s's Size, which must be pixels without scale, got scale %g, %g",
                    getClassName(), size.x.scale, size.y.scale);
            }
            if (size.x.offset != std::floor(size.x.offset) || size.y.offset != std::floor(size.y.offset)) {
                luaL_error(L, "a buffer image's size is the %s's Size, which must be whole pixels, got %g x %g", getClassName(),
                    size.x.offset, size.y.offset);
            }
            if (size.x.offset < 1 || size.y.offset < 1 || size.x.offset > 32768 || size.y.offset > 32768) {
                luaL_error(L, "a buffer image's size is the %s's Size, which must be 1 to 32768 pixels each way, got %g x %g",
                    getClassName(), size.x.offset, size.y.offset);
            }
            int width = static_cast<int>(size.x.offset);
            int height = static_cast<int>(size.y.offset);
            size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
            if (length != expected) {
                luaL_error(L, "a %d x %d image takes a buffer of %.0f bytes, 4 a pixel (RGBA), got %.0f bytes", width, height,
                    static_cast<double>(expected), static_cast<double>(length));
            }
            auto pixels = std::make_shared<PixelData>();
            pixels->width = width;
            pixels->height = height;
            pixels->rgba.resize(length);
            std::memcpy(pixels->rgba.data(), data, length);
            pixels->serial = nextPixelSerial++;
            source.pixels = std::move(pixels);
        } else if (lua_type(L, index) == LUA_TSTRING) {
            size_t length = 0;
            const char* path = lua_tolstring(L, index, &length);
            source.path.assign(path, length);
        } else {
            luaL_typeerror(L, index, "string or buffer");
        }
        return source;
    }

    void ImageObject::pushImage(lua_State* L, const ImageSource& source) {
        if (source.pixels) {
            pushPixelBuffers(L);
            lua_pushnumber(L, static_cast<double>(source.pixels->serial));
            lua_rawget(L, -2);
            if (!lua_isbuffer(L, -1)) {
                lua_pop(L, 1);
                const std::vector<uint8_t>& rgba = source.pixels->rgba;
                void* data = lua_newbuffer(L, rgba.size());
                if (!rgba.empty()) std::memcpy(data, rgba.data(), rgba.size());
                lua_pushnumber(L, static_cast<double>(source.pixels->serial));
                lua_pushvalue(L, -2);
                lua_rawset(L, -4);
            }
            lua_remove(L, -2);
        } else {
            lua_pushlstring(L, source.path.data(), source.path.size());
        }
    }

    void ImageObject::readImage(lua_State* L, const char* name, ImageSource& out) const {
        if (!ControlLuau::field(L, name)) return;
        out = checkImage(L, -1);
        lua_pop(L, 1);
    }

    // Size is required, since buffers are read with it. ControlObject reads it before the images are.
    void ImageObject::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        if (!field(L, "Size")) luaL_error(L, "%s descriptors need a Size", getClassName());
        lua_pop(L, 1);
        ControlObject::readDescriptor(L);
        readImage(L, "Image", image);
        readEnum(L, "ScaleType", scaleType, scaleTypeNames);
        if (field(L, "ImageRectOffset")) {
            const float* offset = luaL_checkvector(L, -1);
            rectX = static_cast<int>(std::lround(offset[0]));
            rectY = static_cast<int>(std::lround(offset[1]));
            lua_pop(L, 1);
        }
        if (field(L, "ImageRectSize")) {
            checkSizeVector(L, -1, rectWidth, rectHeight);
            lua_pop(L, 1);
        }
        readColor(L, "ImageColor", imageColor);
        if (field(L, "Rotation")) {
            rotation = static_cast<float>(luaL_checknumber(L, -1));
            lua_pop(L, 1);
        }
        if (field(L, "SliceCenter")) {
            sliceCenter = Luwow::Rects::checkRect(L, -1);
            lua_pop(L, 1);
        }
        if (field(L, "SliceScale")) {
            sliceScale = static_cast<float>(luaL_checknumber(L, -1));
            lua_pop(L, 1);
        }
        readUDim2(L, "TileSize", tileSize);
    }

    int ImageObject::Luau::getScaleType(lua_State* L, ImageObject& object) {
        ControlLuau::pushEnum(L, object.scaleType, scaleTypeNames);
        return 1;
    }

    int ImageObject::Luau::setScaleType(lua_State* L, ImageObject& object) {
        object.scaleType = ControlLuau::checkEnum<ScaleType>(L, 3, scaleTypeNames);
        object.invalidate();
        return 0;
    }

    // The image's size in pixels, 0, 0 when there's none or it couldn't be loaded
    int ImageObject::Luau::getImageSize(lua_State* L, ImageObject& object) {
        ICanvasImage* picture = object.currentImage(object.image);
        ControlLuau::pushVector2(L, static_cast<float>(picture ? picture->width() : 0), static_cast<float>(picture ? picture->height() : 0));
        return 1;
    }

    int ImageObject::Luau::getImageRectOffset(lua_State* L, ImageObject& object) {
        ControlLuau::pushVector2(L, static_cast<float>(object.rectX), static_cast<float>(object.rectY));
        return 1;
    }

    int ImageObject::Luau::setImageRectOffset(lua_State* L, ImageObject& object) {
        const float* offset = luaL_checkvector(L, 3);
        object.rectX = static_cast<int>(std::lround(offset[0]));
        object.rectY = static_cast<int>(std::lround(offset[1]));
        object.invalidate();
        return 0;
    }

    // 0, 0 shows the whole image
    int ImageObject::Luau::getImageRectSize(lua_State* L, ImageObject& object) {
        ControlLuau::pushVector2(L, static_cast<float>(object.rectWidth), static_cast<float>(object.rectHeight));
        return 1;
    }

    int ImageObject::Luau::setImageRectSize(lua_State* L, ImageObject& object) {
        ControlLuau::checkSizeVector(L, 3, object.rectWidth, object.rectHeight);
        object.invalidate();
        return 0;
    }

    int ImageObject::Luau::getRotation(lua_State* L, ImageObject& object) {
        lua_pushnumber(L, object.rotation);
        return 1;
    }

    int ImageObject::Luau::setRotation(lua_State* L, ImageObject& object) {
        object.rotation = static_cast<float>(luaL_checknumber(L, 3));
        object.invalidate();
        return 0;
    }

    int ImageObject::Luau::getImageColor(lua_State* L, ImageObject& object) {
        ControlLuau::pushColor(L, object.imageColor);
        return 1;
    }

    // nil goes back to white, the image as it is
    int ImageObject::Luau::setImageColor(lua_State* L, ImageObject& object) {
        object.imageColor = ControlLuau::checkColor(L, 3).value_or(0xFFFFFF);
        object.invalidate();
        return 0;
    }

    int ImageObject::Luau::getSliceCenter(lua_State* L, ImageObject& object) {
        Luwow::Rects::pushRect(L, object.sliceCenter);
        return 1;
    }

    int ImageObject::Luau::setSliceCenter(lua_State* L, ImageObject& object) {
        object.sliceCenter = Luwow::Rects::checkRect(L, 3);
        object.invalidate();
        return 0;
    }

    int ImageObject::Luau::getSliceScale(lua_State* L, ImageObject& object) {
        lua_pushnumber(L, object.sliceScale);
        return 1;
    }

    int ImageObject::Luau::setSliceScale(lua_State* L, ImageObject& object) {
        object.sliceScale = static_cast<float>(luaL_checknumber(L, 3));
        object.invalidate();
        return 0;
    }

    int ImageObject::Luau::getTileSize(lua_State* L, ImageObject& object) {
        Luwow::UDims::pushUDim2(L, object.tileSize);
        return 1;
    }

    int ImageObject::Luau::setTileSize(lua_State* L, ImageObject& object) {
        object.tileSize = Luwow::UDims::checkUDim2Like(L, 3);
        object.invalidate();
        return 0;
    }
} // namespace Luwow::Udc
