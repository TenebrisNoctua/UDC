#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>

#include "CocoaCanvas.h"
#include "Controls/ControlView.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Luwow::Udc {
    static NSColor* toColor(uint32_t color) {
        return [NSColor colorWithSRGBRed:((color >> 16) & 0xFF) / 255.0
                                   green:((color >> 8) & 0xFF) / 255.0
                                    blue:(color & 0xFF) / 255.0
                                   alpha:colorAlpha(color) / 255.0];
    }

    // An empty family, or one that isn't installed, uses the system font.
    static NSFont* toFont(const Font& font) {
        NSFont* result = nil;
        if (!font.family.empty()) {
            result = [NSFont fontWithName:[NSString stringWithUTF8String:font.family.c_str()] size:font.size];
        }
        if (!result) result = [NSFont systemFontOfSize:font.size];

        // Through font descriptors rather than NSFontManager, which isn't thread-safe. Text is measured on
        // whichever thread runs the script, while the main thread paints
        if (!font.bold && !font.italic) return result;
        NSFontDescriptorSymbolicTraits traits = result.fontDescriptor.symbolicTraits;
        if (font.bold) traits |= NSFontDescriptorTraitBold;
        if (font.italic) traits |= NSFontDescriptorTraitItalic;
        NSFont* styled = [NSFont fontWithDescriptor:[result.fontDescriptor fontDescriptorWithSymbolicTraits:traits] size:font.size];
        return styled ? styled : result;
    }

    // nil for text that isn't valid UTF-8
    static NSString* toString(const std::string& text) {
        return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
    }

    static NSRect toNSRect(const Rect& rect) {
        return NSMakeRect(rect.x, rect.y, rect.width, rect.height);
    }

    // An image NSImage decoded, its size in pixels from its first representation
    class CocoaImage : public ICanvasImage {
    public:
        CocoaImage(NSImage* image, int pixelWidth, int pixelHeight) : image(image), pixelWidth(pixelWidth), pixelHeight(pixelHeight) {}

        int width() const override { return pixelWidth; }
        int height() const override { return pixelHeight; }

        NSImage* image;
        int pixelWidth;
        int pixelHeight;
    };

    // CocoaSurface

    // Sized in pixels, nullptr if it couldn't be decoded
    static std::shared_ptr<ICanvasImage> fromDecoded(NSImage* image) {
        if (!image || image.representations.count == 0) return nullptr;

        NSImageRep* representation = image.representations.firstObject;
        int width = static_cast<int>(representation.pixelsWide);
        int height = static_cast<int>(representation.pixelsHigh);
        if (width <= 0 || height <= 0) return nullptr;

        // Points match pixels, so source rectangles in pixels work as they are
        image.size = NSMakeSize(width, height);
        return std::make_shared<CocoaImage>(image, width, height);
    }

    std::shared_ptr<ICanvasImage> CocoaSurface::loadImage(const std::string& path) {
        return fromDecoded([[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:path.c_str()]]);
    }

    std::shared_ptr<ICanvasImage> CocoaSurface::decodeImage(const std::string& bytes) {
        NSData* data = [NSData dataWithBytes:bytes.data() length:bytes.size()];
        return fromDecoded([[NSImage alloc] initWithData:data]);
    }

    // The bitmap keeps its own copy of the pixels, unpremultiplied as they came
    std::shared_ptr<ICanvasImage> CocoaSurface::createImage(const PixelData& pixels) {
        NSBitmapImageRep* bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                                           pixelsWide:pixels.width
                                                                           pixelsHigh:pixels.height
                                                                        bitsPerSample:8
                                                                      samplesPerPixel:4
                                                                             hasAlpha:YES
                                                                             isPlanar:NO
                                                                       colorSpaceName:NSDeviceRGBColorSpace
                                                                         bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                                                                          bytesPerRow:pixels.width * 4
                                                                         bitsPerPixel:32];
        if (!bitmap) return nullptr;
        std::memcpy(bitmap.bitmapData, pixels.rgba.data(), pixels.rgba.size());

        NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(pixels.width, pixels.height)];
        [image addRepresentation:bitmap];
        return std::make_shared<CocoaImage>(image, pixels.width, pixels.height);
    }

    CocoaSurface::CocoaSurface(void* view) : view(view) {
        bounds();
    }

    // Scripts may change controls from another thread, the repaint is asked for on the main one
    void CocoaSurface::invalidate() {
        NSView* nsView = (__bridge NSView*)view;
        if ([NSThread isMainThread]) {
            [nsView setNeedsDisplay:YES];
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{
                [nsView setNeedsDisplay:YES];
            });
        }
    }

    Rect CocoaSurface::bounds() const {
        if (![NSThread isMainThread]) return lastBounds;

        NSRect bounds = [(__bridge NSView*)view bounds];
        lastBounds = Rect{ 0, 0, static_cast<int>(NSWidth(bounds)), static_cast<int>(NSHeight(bounds)) };
        return lastBounds;
    }

    int CocoaSurface::measureText(const std::string& text, const Font& font) {
        NSString* string = toString(text);
        if (!string || text.empty()) return 0;

        NSDictionary* attributes = @{ NSFontAttributeName: toFont(font) };
        return static_cast<int>(std::ceil([string sizeWithAttributes:attributes].width));
    }

    std::string CocoaSurface::clipboardText() {
        NSString* text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        return text ? std::string([text UTF8String]) : std::string();
    }

    void CocoaSurface::setClipboardText(const std::string& text) {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        [pasteboard setString:[NSString stringWithUTF8String:text.c_str()] forType:NSPasteboardTypeString];
    }

    // Registered for this process only, it's gone when the process ends
    bool CocoaSurface::loadFontFile(const std::string& path) {
        NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
        CFErrorRef error = nullptr;
        bool loaded = CTFontManagerRegisterFontsForURL((__bridge CFURLRef)url, kCTFontManagerScopeProcess, &error);
        if (error) CFRelease(error);
        return loaded;
    }

    void CocoaSurface::paint(ControlView& host) {
        NSView* nsView = (__bridge NSView*)view;
        [[NSColor windowBackgroundColor] setFill];
        NSRectFill([nsView bounds]);

        CocoaCanvas canvas;
        host.paint(canvas);
    }

    // CocoaCanvas

    // A rectangle with each corner rounded by its own radius, 0 for a square corner. The view is flipped,
    // so minY is the top.
    static NSBezierPath* roundedPath(NSRect r, CGFloat topLeft, CGFloat topRight, CGFloat bottomRight, CGFloat bottomLeft) {
        NSBezierPath* path = [NSBezierPath bezierPath];
        [path moveToPoint:NSMakePoint(NSMinX(r) + topLeft, NSMinY(r))];
        [path appendBezierPathWithArcFromPoint:NSMakePoint(NSMaxX(r), NSMinY(r)) toPoint:NSMakePoint(NSMaxX(r), NSMaxY(r)) radius:topRight];
        [path appendBezierPathWithArcFromPoint:NSMakePoint(NSMaxX(r), NSMaxY(r)) toPoint:NSMakePoint(NSMinX(r), NSMaxY(r)) radius:bottomRight];
        [path appendBezierPathWithArcFromPoint:NSMakePoint(NSMinX(r), NSMaxY(r)) toPoint:NSMakePoint(NSMinX(r), NSMinY(r)) radius:bottomLeft];
        [path appendBezierPathWithArcFromPoint:NSMakePoint(NSMinX(r), NSMinY(r)) toPoint:NSMakePoint(NSMaxX(r), NSMinY(r)) radius:topLeft];
        [path closePath];
        return path;
    }

    void CocoaCanvas::fillRect(const Rect& rect, uint32_t color, const CornerRadii& radii) {
        if (colorAlpha(color) == 0) return;
        [toColor(color) setFill];
        if (radii.square()) {
            NSRectFillUsingOperation(toNSRect(rect), NSCompositingOperationSourceOver);
            return;
        }
        [roundedPath(toNSRect(rect), radii.topLeft, radii.topRight, radii.bottomRight, radii.bottomLeft) fill];
    }

    // The border is inside the rectangle. A path's stroke is centered on it, so it's inset by half the
    // width, and its corners follow the fill's curve
    void CocoaCanvas::strokeRect(const Rect& rect, uint32_t color, int width, const CornerRadii& radii) {
        if (colorAlpha(color) == 0 || width <= 0) return;
        [toColor(color) setStroke];
        if (radii.square()) {
            [toColor(color) setFill];
            NSFrameRectWithWidthUsingOperation(toNSRect(rect), width, NSCompositingOperationSourceOver);
            return;
        }

        CGFloat half = width / 2.0;
        auto inner = [half](int radius) -> CGFloat { return radius > 0 ? std::max<CGFloat>(0.5, radius - half) : 0; };
        NSBezierPath* path = roundedPath(NSInsetRect(toNSRect(rect), half, half), inner(radii.topLeft), inner(radii.topRight),
            inner(radii.bottomRight), inner(radii.bottomLeft));
        [path setLineWidth:width];
        [path stroke];
    }

    // NSGradient's angle is counterclockwise, which the flipped view turns clockwise
    void CocoaCanvas::fillGradient(const Rect& rect, const std::vector<GradientStop>& stops, float rotation, const CornerRadii& radii) {
        if (stops.empty() || rect.width <= 0 || rect.height <= 0) return;
        if (stops.size() == 1) {
            fillRect(rect, stops.front().color, radii);
            return;
        }

        NSMutableArray<NSColor*>* colors = [NSMutableArray arrayWithCapacity:stops.size()];
        std::vector<CGFloat> locations;
        for (const GradientStop& stop : stops) {
            [colors addObject:toColor(stop.color)];
            locations.push_back(std::clamp<CGFloat>(stop.position, 0, 1));
        }
        NSGradient* gradient = [[NSGradient alloc] initWithColors:colors atLocations:locations.data()
            colorSpace:[NSColorSpace sRGBColorSpace]];
        NSBezierPath* path = roundedPath(toNSRect(rect), radii.topLeft, radii.topRight, radii.bottomRight, radii.bottomLeft);
        [gradient drawInBezierPath:path angle:rotation];
    }

    // The source rectangle is in the image's own coordinates, whose origin is the bottom left. A tint's
    // color multiplies the image in a transparency layer, which the image then masks to its own shape.
    void CocoaCanvas::drawImage(const ICanvasImage& image, const Rect& destination, const Rect& source, uint32_t tint, float rotation) {
        const auto& picture = static_cast<const CocoaImage&>(image);
        NSRect from = NSMakeRect(source.x, picture.height() - source.y - source.height, source.width, source.height);
        NSRect to = toNSRect(destination);
        NSGraphicsContext* context = [NSGraphicsContext currentContext];
        [context setImageInterpolation:NSImageInterpolationHigh];
        CGFloat opacity = colorAlpha(tint) / 255.0;

        CGContextRef cg = context.CGContext;
        CGContextSaveGState(cg);
        // The view is flipped, so a positive angle turns clockwise
        if (rotation != 0) {
            CGFloat centerX = NSMidX(to);
            CGFloat centerY = NSMidY(to);
            CGContextTranslateCTM(cg, centerX, centerY);
            CGContextRotateCTM(cg, rotation * M_PI / 180.0);
            CGContextTranslateCTM(cg, -centerX, -centerY);
        }

        if ((tint & 0xFFFFFF) == 0xFFFFFF) {
            [picture.image drawInRect:to fromRect:from operation:NSCompositingOperationSourceOver fraction:opacity respectFlipped:YES hints:nil];
            CGContextRestoreGState(cg);
            return;
        }

        CGContextSetAlpha(cg, opacity);
        CGContextBeginTransparencyLayerWithRect(cg, NSRectToCGRect(to), nullptr);
        [picture.image drawInRect:to fromRect:from operation:NSCompositingOperationSourceOver fraction:1.0 respectFlipped:YES hints:nil];
        [toColor(colorRgb(tint)) setFill];
        NSRectFillUsingOperation(to, NSCompositingOperationMultiply);
        [picture.image drawInRect:to fromRect:from operation:NSCompositingOperationDestinationIn fraction:1.0 respectFlipped:YES hints:nil];
        CGContextEndTransparencyLayer(cg);
        CGContextRestoreGState(cg);
    }

    // The view is flipped, so drawing at a point puts the text's top left there
    void CocoaCanvas::drawText(const std::string& text, const Rect& rect, uint32_t color, const Font& font, TextAlignment alignment) {
        NSString* string = toString(text);
        if (!string || text.empty() || colorAlpha(color) == 0) return;

        NSDictionary* attributes = @{
            NSFontAttributeName: toFont(font),
            NSForegroundColorAttributeName: toColor(color)
        };
        NSSize size = [string sizeWithAttributes:attributes];

        CGFloat x = rect.x;
        if (alignment == TextAlignment::Center) x = rect.x + (rect.width - size.width) / 2;
        else if (alignment == TextAlignment::Right) x = rect.right() - size.width;
        CGFloat y = rect.y + (rect.height - size.height) / 2;

        [string drawAtPoint:NSMakePoint(x, y) withAttributes:attributes];
    }

    void CocoaCanvas::pushClip(const Rect& rect) {
        [NSGraphicsContext saveGraphicsState];
        NSRectClip(toNSRect(rect));
    }

    void CocoaCanvas::popClip() {
        [NSGraphicsContext restoreGraphicsState];
    }
} // namespace Luwow::Udc
