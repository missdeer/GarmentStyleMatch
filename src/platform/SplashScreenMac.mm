#include "SplashScreen.h"

#import <Cocoa/Cocoa.h>

#include <QCoreApplication>
#include <QTimer>

#include <chrono>

namespace
{
    constexpr long long kMinDisplayMs = 3000;

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
}

namespace SplashScreen
{
    void show()
    {
        if (g_window != nil)
        {
            return;
        }

        [NSApplication sharedApplication];

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
}
