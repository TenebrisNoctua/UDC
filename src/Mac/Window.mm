#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "Input.h"
#include "Window.h"
#include "CocoaCanvas.h"
#include "Controls/ControlView.h"
#include <stdexcept>
#include <string>
#include "lua.h"
#include "lualib.h"

using Luwow::Udc::ControlView;
using Luwow::Udc::PointerButton;
namespace MacInput = Luwow::Udc::MacInput;
using Luwow::Udc::Key;
using Luwow::Udc::KeyModifiers;

// The window's content view. Flipped, so coordinates start at the top left.
// Once the window has drawn controls, it paints them and sends them its mouse and keyboard input.
@interface LuwowUdcContentView : NSView
@property (nonatomic, assign) ControlView* controlView;
@property (nonatomic, assign) Luwow::Udc::CocoaSurface* surface;
@property (nonatomic, strong) NSTrackingArea* trackingArea;
@end

@implementation LuwowUdcContentView
- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return self.controlView != nullptr;
}

- (BOOL)becomeFirstResponder {
    if (self.controlView) self.controlView->setWindowFocused(true);
    return YES;
}

- (BOOL)resignFirstResponder {
    if (self.controlView) self.controlView->setWindowFocused(false);
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    if (self.controlView && self.surface) {
        self.surface->paint(*self.controlView);
    } else {
        [super drawRect:dirtyRect];
    }
}

// Mouse moves only arrive through a tracking area
- (void)updateTrackingAreas {
    if (self.trackingArea) [self removeTrackingArea:self.trackingArea];
    self.trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
        options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
        owner:self userInfo:nil];
    [self addTrackingArea:self.trackingArea];
    [super updateTrackingAreas];
}

- (NSPoint)pointFor:(NSEvent*)event {
    return [self convertPoint:[event locationInWindow] fromView:nil];
}

- (void)mouseDown:(NSEvent*)event {
    if (!self.controlView) return [super mouseDown:event];
    [[self window] makeFirstResponder:self];
    self.controlView->pointerDown(MacInput::pointerInput(event, [self pointFor:event], PointerButton::Primary));
}

- (void)mouseUp:(NSEvent*)event {
    if (!self.controlView) return [super mouseUp:event];
    self.controlView->pointerUp(MacInput::pointerInput(event, [self pointFor:event], PointerButton::Primary));
}

- (void)rightMouseDown:(NSEvent*)event {
    if (!self.controlView) return [super rightMouseDown:event];
    self.controlView->pointerDown(MacInput::pointerInput(event, [self pointFor:event], PointerButton::Secondary));
}

- (void)rightMouseUp:(NSEvent*)event {
    if (!self.controlView) return [super rightMouseUp:event];
    self.controlView->pointerUp(MacInput::pointerInput(event, [self pointFor:event], PointerButton::Secondary));
}

- (void)otherMouseDown:(NSEvent*)event {
    if (!self.controlView) return [super otherMouseDown:event];
    self.controlView->pointerDown(MacInput::pointerInput(event, [self pointFor:event], PointerButton::Middle));
}

- (void)otherMouseUp:(NSEvent*)event {
    if (!self.controlView) return [super otherMouseUp:event];
    self.controlView->pointerUp(MacInput::pointerInput(event, [self pointFor:event], PointerButton::Middle));
}

- (void)mouseDragged:(NSEvent*)event {
    if (!self.controlView) return [super mouseDragged:event];
    self.controlView->pointerMove(MacInput::pointerInput(event, [self pointFor:event], PointerButton::None));
}

- (void)mouseMoved:(NSEvent*)event {
    if (!self.controlView) return [super mouseMoved:event];
    self.controlView->pointerMove(MacInput::pointerInput(event, [self pointFor:event], PointerButton::None));
}

- (void)mouseExited:(NSEvent*)event {
    if (!self.controlView) return [super mouseExited:event];
    self.controlView->pointerLeave();
}

// Trackpads report pixels, mouse wheels report lines
- (void)scrollWheel:(NSEvent*)event {
    if (!self.controlView) return [super scrollWheel:event];
    CGFloat scale = [event hasPreciseScrollingDeltas] ? 1.0 : 16.0;
    NSPoint p = [self pointFor:event];
    self.controlView->wheel(static_cast<int>(p.x), static_cast<int>(p.y),
        static_cast<int>([event scrollingDeltaX] * scale), static_cast<int>([event scrollingDeltaY] * scale));
}

- (void)sendKey:(Key)key shift:(BOOL)shift {
    KeyModifiers modifiers;
    modifiers.shift = shift;
    self.controlView->key(key, modifiers);
}

- (void)keyDown:(NSEvent*)event {
    if (!self.controlView) return [super keyDown:event];

    NSEventModifierFlags flags = [event modifierFlags];
    if (flags & NSEventModifierFlagCommand) {
        NSString* character = [[event charactersIgnoringModifiers] lowercaseString];
        if ([character isEqualToString:@"c"]) [self sendKey:Key::Copy shift:NO];
        else if ([character isEqualToString:@"x"]) [self sendKey:Key::Cut shift:NO];
        else if ([character isEqualToString:@"v"]) [self sendKey:Key::Paste shift:NO];
        else if ([character isEqualToString:@"a"]) [self sendKey:Key::SelectAll shift:NO];
        else [super keyDown:event];
        return;
    }

    [self interpretKeyEvents:@[event]];
}

// Keeps printable characters, dropping control characters and the function key range
- (void)insertText:(id)value {
    if (!self.controlView) return;
    NSString* characters = [value isKindOfClass:[NSAttributedString class]] ? [value string] : value;

    NSMutableString* printable = [NSMutableString string];
    for (NSUInteger i = 0; i < [characters length]; ++i) {
        unichar c = [characters characterAtIndex:i];
        if (c < 0x20 || c == 0x7F || (c >= 0xF700 && c <= 0xF8FF)) continue;
        [printable appendFormat:@"%C", c];
    }
    if ([printable length] > 0) self.controlView->text([printable UTF8String]);
}

// The editing commands key bindings name, unknown ones are ignored rather than beeping
- (void)doCommandBySelector:(SEL)selector {
    if (!self.controlView) return;

    struct Command {
        SEL selector;
        Key key;
        BOOL shift;
    };
    static const Command commands[] = {
        { @selector(moveLeft:), Key::Left, NO },
        { @selector(moveRight:), Key::Right, NO },
        { @selector(moveBackward:), Key::Left, NO },
        { @selector(moveForward:), Key::Right, NO },
        { @selector(moveLeftAndModifySelection:), Key::Left, YES },
        { @selector(moveUp:), Key::Up, NO },
        { @selector(moveDown:), Key::Down, NO },
        { @selector(moveUpAndModifySelection:), Key::Up, YES },
        { @selector(moveDownAndModifySelection:), Key::Down, YES },
        { @selector(moveRightAndModifySelection:), Key::Right, YES },
        { @selector(moveBackwardAndModifySelection:), Key::Left, YES },
        { @selector(moveForwardAndModifySelection:), Key::Right, YES },
        { @selector(moveToBeginningOfLine:), Key::Home, NO },
        { @selector(moveToEndOfLine:), Key::End, NO },
        { @selector(moveToLeftEndOfLine:), Key::Home, NO },
        { @selector(moveToRightEndOfLine:), Key::End, NO },
        { @selector(moveToBeginningOfDocument:), Key::Home, NO },
        { @selector(moveToEndOfDocument:), Key::End, NO },
        { @selector(scrollToBeginningOfDocument:), Key::Home, NO },
        { @selector(scrollToEndOfDocument:), Key::End, NO },
        { @selector(moveToBeginningOfLineAndModifySelection:), Key::Home, YES },
        { @selector(moveToEndOfLineAndModifySelection:), Key::End, YES },
        { @selector(moveToLeftEndOfLineAndModifySelection:), Key::Home, YES },
        { @selector(moveToRightEndOfLineAndModifySelection:), Key::End, YES },
        { @selector(moveToBeginningOfDocumentAndModifySelection:), Key::Home, YES },
        { @selector(moveToEndOfDocumentAndModifySelection:), Key::End, YES },
        { @selector(deleteBackward:), Key::Backspace, NO },
        { @selector(deleteForward:), Key::Delete, NO },
        { @selector(insertNewline:), Key::Enter, NO },
        { @selector(cancelOperation:), Key::Escape, NO },
        { @selector(insertTab:), Key::Tab, NO },
        { @selector(selectAll:), Key::SelectAll, NO },
    };

    for (const Command& command : commands) {
        if (sel_isEqual(selector, command.selector)) {
            [self sendKey:command.key shift:command.shift];
            return;
        }
    }
}

- (void)copy:(id)sender {
    if (self.controlView) [self sendKey:Key::Copy shift:NO];
}

- (void)cut:(id)sender {
    if (self.controlView) [self sendKey:Key::Cut shift:NO];
}

- (void)paste:(id)sender {
    if (self.controlView) [self sendKey:Key::Paste shift:NO];
}

- (void)selectAll:(id)sender {
    if (self.controlView) [self sendKey:Key::SelectAll shift:NO];
}
@end

// Windows not closed yet, the application ends with the last one
static int openWindows = 0;

@interface LuwowUdcWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) Luwow::Udc::Window* window;
@end

@implementation LuwowUdcWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    return self.window ? self.window->closeRequested() : YES;
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    (void)notification;
    if (self.window && self.window->onFocused) self.window->onFocused();
}

// The application ends with the last window, unless another library's windows are still shown
- (void)windowWillClose:(NSNotification*)notification {
    if (self.window) self.window->destroyed();
    if (--openWindows > 0) return;
    for (NSWindow* other in [NSApp windows]) {
        if (other != [notification object] && [other isVisible] && ![[other delegate] isKindOfClass:[LuwowUdcWindowDelegate class]]) return;
    }
    [[NSApplication sharedApplication] terminate:nil];
}
@end

namespace Luwow::Udc {

Window::Window(const WindowDescriptor& descriptor) : descriptor(descriptor), title(descriptor.Title), topMost(descriptor.TopMost) {
    ++openWindows;
    @autoreleasepool {
        NSScreen* screen = [NSScreen mainScreen];
        NSRect screenFrame = [screen frame];
        CGFloat y = NSHeight(screenFrame) - descriptor.Top - descriptor.Height;
        NSRect frame = NSMakeRect(descriptor.Left, y, descriptor.Width, descriptor.Height);

        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;
        if (descriptor.Minimizable) style |= NSWindowStyleMaskMiniaturizable;
        if (descriptor.Resizable) style |= NSWindowStyleMaskResizable;
        // The descriptor's size is the whole window's, title bar included. ClientSize is the size inside it.
        NSRect content = [NSWindow contentRectForFrameRect:frame styleMask:style];
        NSWindow* window = [[NSWindow alloc] initWithContentRect:content styleMask:style backing:NSBackingStoreBuffered defer:NO];
        [window setTitle:[NSString stringWithUTF8String:descriptor.Title.c_str()]];
        if (!descriptor.Icon.empty()) {
            NSImage* icon = [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:descriptor.Icon.c_str()]];
            if (icon) [NSApp setApplicationIconImage:icon];
        }
        [window setReleasedWhenClosed:NO];

        NSRect contentBounds = [[window contentView] bounds];
        LuwowUdcContentView* cv = [[LuwowUdcContentView alloc] initWithFrame:contentBounds];
        [window setContentView:cv];
        drawnView = (__bridge void*)cv;

        LuwowUdcWindowDelegate* delegate = [[LuwowUdcWindowDelegate alloc] init];
        delegate.window = this;
        [window setDelegate:delegate];
        if (descriptor.TopMost) [window setLevel:NSFloatingWindowLevel];
        if (descriptor.ClientWidth > 0 && descriptor.ClientHeight > 0) {
            [window setContentSize:NSMakeSize(descriptor.ClientWidth, descriptor.ClientHeight)];
        }

        nativeWindow = (__bridge_retained void*)window;
        nativeDelegate = (__bridge_retained void*)delegate;
    }
}

// The window stays the other library's, its delegate, its content view and the native controls in it. The
// drawn view fills the content view beneath them, and follows its size.
Window::Window(void* attachedWindow) : attached(true) {
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)attachedWindow;
        title = [[window title] UTF8String] ?: "";
        topMost = [window level] >= NSFloatingWindowLevel;

        NSView* content = [window contentView];
        LuwowUdcContentView* view = [[LuwowUdcContentView alloc] initWithFrame:[content bounds]];
        [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [content addSubview:view positioned:NSWindowBelow relativeTo:nil];
        drawnView = (__bridge_retained void*)view;
        nativeWindow = (__bridge_retained void*)window;

        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
        Window* target = this;
        id closing = [center addObserverForName:NSWindowWillCloseNotification object:window queue:nil
            usingBlock:^(NSNotification*) { target->destroyed(); }];
        id focused = [center addObserverForName:NSWindowDidBecomeKeyNotification object:window queue:nil
            usingBlock:^(NSNotification*) { if (target->onFocused) target->onFocused(); }];
        closeObserver = (__bridge_retained void*)closing;
        focusObserver = (__bridge_retained void*)focused;
    }
}

void Window::detach() {
    dispatch(^{
        if (!attached || !drawnView) return;
        LuwowUdcContentView* view = (__bridge LuwowUdcContentView*)drawnView;
        view.controlView = nullptr;
        view.surface = nullptr;
        [view removeFromSuperview];

        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
        for (void** observer : { &closeObserver, &focusObserver }) {
            if (!*observer) continue;
            id token = (__bridge_transfer id)*observer;
            *observer = nullptr;
            [center removeObserver:token];
        }
    });
}

Window::~Window() {
    @autoreleasepool {
        // The view may outlive the window object, so it stops using the controls first
        if (controlView && drawnView) {
            LuwowUdcContentView* view = (__bridge LuwowUdcContentView*)drawnView;
            view.controlView = nullptr;
            view.surface = nullptr;
        }
        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
        for (void** observer : { &closeObserver, &focusObserver }) {
            if (!*observer) continue;
            id token = (__bridge_transfer id)*observer;
            *observer = nullptr;
            [center removeObserver:token];
        }
        if (attached && drawnView) {
            id view = (__bridge_transfer id)drawnView;
            drawnView = nullptr;
            (void)view;
        }
        if (nativeDelegate) {
            id delegate = (__bridge_transfer id)nativeDelegate;
            nativeDelegate = nullptr;
            (void)delegate;
        }
        if (nativeWindow) {
            id window = (__bridge_transfer id)nativeWindow;
            nativeWindow = nullptr;
            (void)window;
        }
    }
}

ControlView* Window::getControlView(Luwow::Engine::ILuauHost* luauHost) {
    if (!controlView) {
        LuwowUdcContentView* view = (__bridge LuwowUdcContentView*)drawnView;
        surface = std::make_unique<CocoaSurface>((__bridge void*)view);
        controlView = std::make_unique<ControlView>(surface.get(), luauHost);

        view.surface = surface.get();
        view.controlView = controlView.get();
        [[view window] makeFirstResponder:view];
        [view setNeedsDisplay:YES];
    }
    return controlView.get();
}

void Window::show() {
    NSWindow* w = (__bridge NSWindow*)nativeWindow;
    if (!w) {
        throw std::runtime_error("Window not found");
    }
    dispatch(^{
        if (owner) {
            if (modal) owner->setEnabled(false);
            [(__bridge NSWindow*)owner->nativeWindow addChildWindow:w ordered:NSWindowAbove];
        }
        [w makeKeyAndOrderFront:nil];
    });
}

void Window::close() {
    NSWindow* w = (__bridge NSWindow*)nativeWindow;
    if (!w) {
        throw std::runtime_error("Window not found");
    }
    dispatch(^{
        if (closed) return;
        releaseOwner();
        [w close];
    });
}

void Window::dispatch(void (^work)()) {
    if ([NSThread isMainThread]) work();
    else dispatch_async(dispatch_get_main_queue(), work);
}

void Window::hide() {
    dispatch(^{
        NSWindow* w = (__bridge NSWindow*)nativeWindow;
        releaseOwner();
        [w orderOut:nil];
    });
}

void Window::focus() {
    dispatch(^{
        NSWindow* w = (__bridge NSWindow*)nativeWindow;
        if ([w isMiniaturized]) [w deminiaturize:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [w makeKeyAndOrderFront:nil];
    });
}

// Over a shown owner, otherwise in the middle of the screen
void Window::center() {
    dispatch(^{
        NSWindow* w = (__bridge NSWindow*)nativeWindow;
        NSWindow* around = owner ? (__bridge NSWindow*)owner->nativeWindow : nil;
        if (!around || ![around isVisible]) {
            [w center];
            return;
        }
        NSRect target = [around frame];
        NSRect frame = [w frame];
        frame.origin.x = NSMidX(target) - NSWidth(frame) / 2;
        frame.origin.y = NSMidY(target) - NSHeight(frame) / 2;
        [w setFrameOrigin:frame.origin];
    });
}

bool Window::isVisible() const {
    return !closed && [(__bridge NSWindow*)nativeWindow isVisible];
}

void Window::setTitle(const std::string& newTitle) {
    title = newTitle;
    NSString* text = [NSString stringWithUTF8String:newTitle.c_str()];
    dispatch(^{
        [(__bridge NSWindow*)nativeWindow setTitle:text];
    });
}

void Window::setOwner(IWindow* newOwner) {
    Window* next = static_cast<Window*>(newOwner);
    dispatch(^{
        NSWindow* w = (__bridge NSWindow*)nativeWindow;
        if (owner) [(__bridge NSWindow*)owner->nativeWindow removeChildWindow:w];
        owner = next;
        if (owner && [w isVisible]) [(__bridge NSWindow*)owner->nativeWindow addChildWindow:w ordered:NSWindowAbove];
    });
}

void Window::setModal(bool isModal) {
    dispatch(^{
        bool shown = [(__bridge NSWindow*)nativeWindow isVisible];
        if (shown && modal) releaseOwner();
        modal = isModal;
        if (shown && modal && owner) owner->setEnabled(false);
    });
}

// A window that isn't enabled ignores the mouse, its controls get no input
void Window::setEnabled(bool isEnabled) {
    enabled = isEnabled;
    dispatch(^{
        [(__bridge NSWindow*)nativeWindow setIgnoresMouseEvents:!isEnabled];
    });
}

void Window::setTopMost(bool isTopMost) {
    topMost = isTopMost;
    dispatch(^{
        [(__bridge NSWindow*)nativeWindow setLevel:isTopMost ? NSFloatingWindowLevel : NSNormalWindowLevel];
    });
}

void Window::getClientSize(int& width, int& height) const {
    NSRect bounds = [[(__bridge NSWindow*)nativeWindow contentView] bounds];
    width = static_cast<int>(NSWidth(bounds));
    height = static_cast<int>(NSHeight(bounds));
}

void Window::setClientSize(int width, int height) {
    dispatch(^{
        [(__bridge NSWindow*)nativeWindow setContentSize:NSMakeSize(width, height)];
    });
}

void Window::releaseOwner() {
    if (!modal || !owner || ![(__bridge NSWindow*)nativeWindow isVisible]) return;
    owner->setEnabled(true);
}

bool Window::closeRequested() {
    if (onCloseRequested) onCloseRequested();
    if (preventClose) return false;
    releaseOwner();
    return true;
}

void Window::destroyed() {
    closed = true;
    if (onClosed) onClosed();
}

void* Window::getNativeContentView() const {
    NSWindow* w = (__bridge NSWindow*)nativeWindow;
    if (!w) {
        return nullptr;
    }
    return attached ? drawnView : (__bridge void*)[w contentView];
}

} // namespace Luwow::Udc
