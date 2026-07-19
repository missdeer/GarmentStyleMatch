#include <cstring>

#include "SplashScreen.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <objidl.h>
// clang-format off
#include <gdiplus.h>
// clang-format on

#include <QCoreApplication>
#include <QTimer>

namespace
{
    constexpr unsigned long kMinDisplayMs = 3000;
    constexpr wchar_t       kClassName[]  = L"GarmentStyleMatchSplash";

    HWND      g_hwnd          = nullptr;
    HBITMAP   g_hbm           = nullptr;
    ULONG_PTR g_gdiplusToken  = 0;
    ULONGLONG g_showTicks     = 0;
    bool      g_hideRequested = false;

    void destroy()
    {
        if (g_hwnd != nullptr)
        {
            DestroyWindow(g_hwnd);
            g_hwnd = nullptr;
        }
        if (g_hbm != nullptr)
        {
            DeleteObject(g_hbm);
            g_hbm = nullptr;
        }
        if (g_gdiplusToken != 0)
        {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            g_gdiplusToken = 0;
        }
    }

    IStream *loadResourceStream()
    {
        HRSRC resInfo = FindResourceW(nullptr, L"SPLASH_PNG", RT_RCDATA);
        if (resInfo == nullptr)
        {
            return nullptr;
        }
        HGLOBAL resData = LoadResource(nullptr, resInfo);
        if (resData == nullptr)
        {
            return nullptr;
        }
        void       *bytes = LockResource(resData);
        const DWORD size  = SizeofResource(nullptr, resInfo);
        if (bytes == nullptr || size == 0)
        {
            return nullptr;
        }

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hMem == nullptr)
        {
            return nullptr;
        }
        void *dst = GlobalLock(hMem);
        if (dst == nullptr)
        {
            GlobalFree(hMem);
            return nullptr;
        }
        std::memcpy(dst, bytes, size);
        GlobalUnlock(hMem);

        IStream *stream = nullptr;
        if (CreateStreamOnHGlobal(hMem, TRUE, &stream) != S_OK)
        {
            GlobalFree(hMem);
            return nullptr;
        }
        return stream;
    }
} // namespace

namespace SplashScreen
{
    void show()
    {
        if (g_hwnd != nullptr)
        {
            return;
        }

        // Match Qt 6's default DPI awareness so the splash is created in physical
        // pixels. Otherwise the process starts DPI-unaware, Qt flips it to
        // per-monitor-v2 later, and our legacy HWND ends up mis-positioned.
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        Gdiplus::GdiplusStartupInput startupInput;
        if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
        {
            g_gdiplusToken = 0;
            return;
        }

        IStream *stream = loadResourceStream();
        if (stream == nullptr)
        {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            g_gdiplusToken = 0;
            return;
        }

        Gdiplus::Bitmap bmp(stream, FALSE);
        stream->Release();
        if (bmp.GetLastStatus() != Gdiplus::Ok)
        {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            g_gdiplusToken = 0;
            return;
        }
        const UINT srcW = bmp.GetWidth();
        const UINT srcH = bmp.GetHeight();
        if (srcW == 0 || srcH == 0)
        {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            g_gdiplusToken = 0;
            return;
        }

        // Scale the splash to the primary monitor's DPI so it appears at the
        // intended physical size on any display scale factor. Requires that we
        // already flipped the process to a DPI-aware context above; otherwise
        // GetDpiForSystem always returns 96.
        HMONITOR    mon   = MonitorFromPoint(POINT {0, 0}, MONITOR_DEFAULTTOPRIMARY);
        const float scale = static_cast<float>(GetDpiForSystem()) / 96.0F;
        const UINT  w     = static_cast<UINT>(static_cast<float>(srcW) * scale + 0.5F);
        const UINT  h     = static_cast<UINT>(static_cast<float>(srcH) * scale + 0.5F);

        BITMAPINFO bi {};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = static_cast<LONG>(w);
        bi.bmiHeader.biHeight      = -static_cast<LONG>(h);
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        void *dibPixels = nullptr;
        g_hbm           = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &dibPixels, nullptr, 0);
        if (g_hbm == nullptr || dibPixels == nullptr)
        {
            if (g_hbm != nullptr)
            {
                DeleteObject(g_hbm);
                g_hbm = nullptr;
            }
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            g_gdiplusToken = 0;
            return;
        }

        // Render source PNG into an intermediate PARGB Gdiplus::Bitmap with
        // high-quality bicubic scaling, then copy its pixels into the DIB.
        Gdiplus::Bitmap   scaled(static_cast<INT>(w), static_cast<INT>(h), PixelFormat32bppPARGB);
        Gdiplus::Graphics gfx(&scaled);
        gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        gfx.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        gfx.DrawImage(&bmp, 0, 0, static_cast<INT>(w), static_cast<INT>(h));

        Gdiplus::BitmapData bd {};
        Gdiplus::Rect       rect(0, 0, static_cast<INT>(w), static_cast<INT>(h));
        if (scaled.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bd) != Gdiplus::Ok)
        {
            DeleteObject(g_hbm);
            g_hbm = nullptr;
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            g_gdiplusToken = 0;
            return;
        }
        std::memcpy(dibPixels, bd.Scan0, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
        scaled.UnlockBits(&bd);

        HINSTANCE hInst = GetModuleHandleW(nullptr);

        WNDCLASSW wc {};
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClassName;
        RegisterClassW(&wc);

        MONITORINFO mi {};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(mon, &mi);
        const LONG cx = mi.rcMonitor.left + (mi.rcMonitor.right - mi.rcMonitor.left - static_cast<LONG>(w)) / 2;
        const LONG cy = mi.rcMonitor.top + (mi.rcMonitor.bottom - mi.rcMonitor.top - static_cast<LONG>(h)) / 2;

        g_hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                                 kClassName,
                                 L"GarmentStyleMatch",
                                 WS_POPUP,
                                 cx,
                                 cy,
                                 static_cast<int>(w),
                                 static_cast<int>(h),
                                 nullptr,
                                 nullptr,
                                 hInst,
                                 nullptr);
        if (g_hwnd == nullptr)
        {
            DeleteObject(g_hbm);
            g_hbm = nullptr;
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            g_gdiplusToken = 0;
            return;
        }

        HDC     hdcScreen = GetDC(nullptr);
        HDC     hdcMem    = CreateCompatibleDC(hdcScreen);
        HGDIOBJ old       = SelectObject(hdcMem, g_hbm);

        POINT         ptSrc {0, 0};
        POINT         ptDst {cx, cy};
        SIZE          sz {static_cast<LONG>(w), static_cast<LONG>(h)};
        BLENDFUNCTION blend {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(g_hwnd, hdcScreen, &ptDst, &sz, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

        SelectObject(hdcMem, old);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);

        ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
        g_showTicks = GetTickCount64();
    }

    void hide()
    {
        if (g_hwnd == nullptr || g_hideRequested)
        {
            return;
        }
        g_hideRequested = true;

        const ULONGLONG elapsed   = GetTickCount64() - g_showTicks;
        const long      remaining = static_cast<long>(kMinDisplayMs) - static_cast<long>(elapsed);
        if (remaining > 0 && QCoreApplication::instance() != nullptr)
        {
            QTimer::singleShot(remaining, QCoreApplication::instance(), [] { destroy(); });
        }
        else
        {
            destroy();
        }
    }
} // namespace SplashScreen
