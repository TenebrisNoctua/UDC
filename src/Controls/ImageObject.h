#pragma once

#include "ControlObject.h"

#include <memory>

namespace Luwow::Udc {
    // How an image fills its control, stretched to the frame, fitted inside it
    // keeping its proportions, cropped to cover it keeping its proportions, nine-sliced so its borders keep
    // their size, or tiled
    enum class ScaleType { Stretch, Fit, Crop, Slice, Tile };

    // Shared by the controls showing an image. Draws its background and border,
    // then the image over them.
    class ImageObject : public ControlObject {
    public:
        static constexpr const char* Name = "ImageObject";
        using Super = ControlObject;

        using ControlObject::ControlObject;

        // A file path, read from the package being run when it has the file, otherwise from the disk relative
        // to the working directory. Or pixels a buffer gave, as many as Size's offsets when it was given.
        // Empty shows no image.
        ImageSource image;
        ScaleType scaleType = ScaleType::Stretch;
        // The part of the image shown, in its pixels, like a sprite sheet's. A size of 0 shows all of it.
        int rectX = 0;
        int rectY = 0;
        int rectWidth = 0;
        int rectHeight = 0;
        // Multiplies the image's colors, its alpha the image's opacity. White shows the image as it is.
        uint32_t imageColor = 0xFFFFFF;
        // Slice: the stretched middle of the shown part, in its pixels. What's around it keeps its size
        // times sliceScale. Empty stretches the whole image.
        Rect sliceCenter{ 0, 0, 0, 0 };
        float sliceScale = 1;
        // Degrees the image turns clockwise about its center, for the scale types other than Slice and Tile.
        // The control and its children aren't turned.
        float rotation = 0;
        // The size of each tile, its scale a fraction of the content's size
        UDim2 tileSize = UDim2::fromScale(1, 1);

        void paint(ICanvas& canvas, const Rect& frame) override;

        // Luau
        void readDescriptor(lua_State* L) override;

        // The Luau properties every image control has
        template <typename T>
        static ControlLuau::Properties<T> properties() {
            return ControlLuau::extend<T>(ControlObject::properties<T>(), {
                { "Image", T::template Bind<&getImageProperty<ImageObject, &ImageObject::image>>,
                    T::template Bind<&setImageProperty<ImageObject, &ImageObject::image>> },
                { "ScaleType", T::template Bind<&Luau::getScaleType>, T::template Bind<&Luau::setScaleType> },
                { "ImageSize", T::template Bind<&Luau::getImageSize> },
                { "ImageRectOffset", T::template Bind<&Luau::getImageRectOffset>, T::template Bind<&Luau::setImageRectOffset> },
                { "ImageRectSize", T::template Bind<&Luau::getImageRectSize>, T::template Bind<&Luau::setImageRectSize> },
                { "ImageColor", T::template Bind<&Luau::getImageColor>, T::template Bind<&Luau::setImageColor> },
                { "SliceCenter", T::template Bind<&Luau::getSliceCenter>, T::template Bind<&Luau::setSliceCenter> },
                { "SliceScale", T::template Bind<&Luau::getSliceScale>, T::template Bind<&Luau::setSliceScale> },
                { "TileSize", T::template Bind<&Luau::getTileSize>, T::template Bind<&Luau::setTileSize> },
                { "Rotation", T::template Bind<&Luau::getRotation>, T::template Bind<&Luau::setRotation> },
            });
        }

        // An image property's value at index, a path, or a buffer of RGBA pixels the size of Size's offsets,
        // copied so later writes to the buffer don't show. Raises for anything else.
        ImageSource checkImage(lua_State* L, int index) const;
        // A path as a string or pixels as a buffer of them. Writing to it doesn't change the image.
        static void pushImage(lua_State* L, const ImageSource& source);
        // Reads the descriptor's image property called name into out, when it has one
        void readImage(lua_State* L, const char* name, ImageSource& out) const;

        // Every image property's Luau getter and setter
        template <typename T, ImageSource T::*Member>
        static int getImageProperty(lua_State* L, T& object) {
            pushImage(L, object.*Member);
            return 1;
        }

        template <typename T, ImageSource T::*Member>
        static int setImageProperty(lua_State* L, T& object) {
            object.*Member = object.checkImage(L, 3);
            object.invalidate();
            return 0;
        }

    protected:
        // Draws the image over the background, as the scale type places it
        void paintImage(ICanvas& canvas, const ImageSource& source);

    private:
        // The loaded image, from the view's cache
        std::shared_ptr<ICanvasImage> loaded;
        ImageSource loadedSource;

        // Loads the image if it isn't the one loaded, nullptr when there's none or it can't be loaded
        ICanvasImage* currentImage(const ImageSource& source);

        // The part of the image shown, within its bounds
        Rect sourceRect(const ICanvasImage& picture) const;

        void paintSliced(ICanvas& canvas, const ICanvasImage& picture, const Rect& source, const Rect& inner);
        void paintTiled(ICanvas& canvas, const ICanvasImage& picture, const Rect& source, const Rect& inner);

        // The Luau properties
        struct Luau {
            static int getScaleType(lua_State* L, ImageObject& object);
            static int setScaleType(lua_State* L, ImageObject& object);
            static int getImageSize(lua_State* L, ImageObject& object);
            static int getImageRectOffset(lua_State* L, ImageObject& object);
            static int setImageRectOffset(lua_State* L, ImageObject& object);
            static int getImageRectSize(lua_State* L, ImageObject& object);
            static int setImageRectSize(lua_State* L, ImageObject& object);
            static int getRotation(lua_State* L, ImageObject& object);
            static int setRotation(lua_State* L, ImageObject& object);
            static int getImageColor(lua_State* L, ImageObject& object);
            static int setImageColor(lua_State* L, ImageObject& object);
            static int getSliceCenter(lua_State* L, ImageObject& object);
            static int setSliceCenter(lua_State* L, ImageObject& object);
            static int getSliceScale(lua_State* L, ImageObject& object);
            static int setSliceScale(lua_State* L, ImageObject& object);
            static int getTileSize(lua_State* L, ImageObject& object);
            static int setTileSize(lua_State* L, ImageObject& object);
        };
    };
} // namespace Luwow::Udc
