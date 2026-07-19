#pragma once

namespace SplashScreen
{
    // Call at the very top of main(), before QGuiApplication is constructed.
    // Silently no-ops if the platform image resource is missing.
    void show();

    // Enforces a minimum on-screen duration of 3 s from show().
    // Requires a QCoreApplication instance when called with time remaining.
    // Safe to call multiple times.
    void hide();
} // namespace SplashScreen
