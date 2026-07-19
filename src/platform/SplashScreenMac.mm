#include "SplashScreen.h"

#import <Cocoa/Cocoa.h>

#include <QCoreApplication>
#include <QTimer>

#include <chrono>

namespace
{
    constexpr long long      kMinDisplayMs         = 1500;
    // ~3 refresh cycles at 60 Hz — enough for AppKit to commit the CATransaction
    // and for the compositor to present the first frame before we return.
    constexpr NSTimeInterval kInitialPresentPumpSec = 0.05;

    NSWindow                             *g_window        = nil;
    std::chrono::steady_clock::time_point g_showTime      = {};
    bool                                  g_hideRequested = false;

    void destroy()
    {
        if (g_window == nil)
        {
            return;
        }
        [g_window orderOut:nil];
        g_window = nil;
    }
} // namespace

namespace SplashScreen
{
    void show()
    {
        if (g_window != nil)
        {
            return;
        }

        [NSApplication sharedApplication];
        // Ensure the process is a foreground GUI app before we create a window.
        // In a bundled build this is already the case, but setting it explicitly
        // makes the splash appear on top when the binary is launched directly
        // (e.g. from a terminal for development).
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSString *path = [[NSBundle mainBundle] pathForResource:@"GarmentStyleMatchSplash" ofType:@"png"];
        if (path == nil)
        {
            return;
        }
        NSImage *img = [[NSImage alloc] initWithContentsOfFile:path];
        if (img == nil)
        {
            return;
        }

        const NSSize imgSize = [img size];
        if (imgSize.width <= 0 || imgSize.height <= 0)
        {
            return;
        }

        NSScreen    *screen      = [NSScreen mainScreen];
        const NSRect screenFrame = [screen frame];
        const NSRect winRect     = NSMakeRect(NSMidX(screenFrame) - imgSize.width / 2.0,
                                              NSMidY(screenFrame) - imgSize.height / 2.0,
                                              imgSize.width, imgSize.height);

        g_window = [[NSWindow alloc] initWithContentRect:winRect
                                               styleMask:NSWindowStyleMaskBorderless
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        [g_window setLevel:NSFloatingWindowLevel];
        [g_window setOpaque:NO];
        [g_window setBackgroundColor:[NSColor clearColor]];
        [g_window setIgnoresMouseEvents:YES];
        [g_window setHasShadow:NO];

        NSImageView *iv = [[NSImageView alloc] initWithFrame:NSMakeRect(0, 0, imgSize.width, imgSize.height)];
        [iv setImage:img];
        [iv setImageScaling:NSImageScaleProportionallyUpOrDown];
        [[g_window contentView] addSubview:iv];

        [g_window orderFrontRegardless];

        // AppKit only paints on run-loop turns. show() is called before
        // QGuiApplication is constructed, so no run loop is spinning yet — the
        // splash pixels would otherwise stay in the backing store until
        // QGuiApplication::exec() starts, by which time the main window is
        // about to appear too and both would pop up together, defeating the
        // splash's purpose. Force a synchronous draw and pump the run loop
        // long enough for the compositor to present at least one frame.
        [[g_window contentView] setNeedsDisplay:YES];
        [g_window displayIfNeeded];
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:kInitialPresentPumpSec]];

        // Start the min-display clock after the splash is actually on screen,
        // not from when we merely asked AppKit to show it.
        g_showTime = std::chrono::steady_clock::now();
    }

    void hide()
    {
        if (g_window == nil || g_hideRequested)
        {
            return;
        }
        g_hideRequested = true;

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - g_showTime)
                                 .count();
        const long long remaining = kMinDisplayMs - elapsed;

        if (remaining > 0 && QCoreApplication::instance() != nullptr)
        {
            QTimer::singleShot(static_cast<int>(remaining), QCoreApplication::instance(), [] { destroy(); });
        }
        else
        {
            destroy();
        }
    }
} // namespace SplashScreen
