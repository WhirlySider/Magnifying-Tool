#include <windows.h>
#include <magnification.h>

#pragma comment(lib, "Magnification.lib")
#pragma comment(lib, "User32.lib")


// ==================================================
// GLOBAL VARIABLES
// ==================================================

HWND g_hMagnifier = NULL;

// Current zoom level
double g_zoom = 2.0;

// Current focus point on the real screen
POINT g_focusPoint = { 0, 0 };

// Mouse hook handle
HHOOK g_mouseHook = NULL;


// Hotkey IDs
#define HOTKEY_ZOOM_IN   1
#define HOTKEY_ZOOM_OUT  2
#define HOTKEY_RESET     3


// ==================================================
// SET ZOOM
// ==================================================

void SetZoom(double zoom)
{
    MAGTRANSFORM transform = {};

    transform.v[0][0] = (float)zoom;
    transform.v[1][1] = (float)zoom;
    transform.v[2][2] = 1.0f;

    MagSetWindowTransform(
        g_hMagnifier,
        &transform
    );
}


// ==================================================
// SET FOCUS
// ==================================================

void SetFocus(POINT point)
{
    g_focusPoint = point;

    // Size of the real screen area we capture
    const int sourceWidth = 400;
    const int sourceHeight = 300;

    RECT sourceRect;

    sourceRect.left =
        point.x - sourceWidth / 2;

    sourceRect.top =
        point.y - sourceHeight / 2;

    sourceRect.right =
        sourceRect.left + sourceWidth;

    sourceRect.bottom =
        sourceRect.top + sourceHeight;


    // Get screen dimensions

    int screenWidth =
        GetSystemMetrics(SM_CXSCREEN);

    int screenHeight =
        GetSystemMetrics(SM_CYSCREEN);


    // Keep source rectangle inside screen

    if (sourceRect.left < 0)
    {
        sourceRect.left = 0;
        sourceRect.right = sourceWidth;
    }

    if (sourceRect.top < 0)
    {
        sourceRect.top = 0;
        sourceRect.bottom = sourceHeight;
    }

    if (sourceRect.right > screenWidth)
    {
        sourceRect.right = screenWidth;
        sourceRect.left = screenWidth - sourceWidth;
    }

    if (sourceRect.bottom > screenHeight)
    {
        sourceRect.bottom = screenHeight;
        sourceRect.top = screenHeight - sourceHeight;
    }


    // Tell Magnification API which part of
    // the real screen to display

    MagSetWindowSource(
        g_hMagnifier,
        sourceRect
    );
}


// ==================================================
// CHANGE ZOOM AT CURSOR
// ==================================================

void ChangeZoom(double amount)
{
    POINT cursor;

    // Capture cursor position NOW
    GetCursorPos(&cursor);


    // Change zoom

    g_zoom += amount;


    // Minimum zoom

    if (g_zoom < 0.25)
    {
        g_zoom = 0.25;
    }


    // Maximum zoom
    //
    // Prevent ridiculously large values.

    if (g_zoom > 10.0)
    {
        g_zoom = 10.0;
    }


    // Apply new zoom

    SetZoom(g_zoom);


    // Focus on cursor position
    // from the moment the action happened

    SetFocus(cursor);
}


// ==================================================
// MOUSE HOOK
// ==================================================

LRESULT CALLBACK LowLevelMouseProc(
    int nCode,
    WPARAM wParam,
    LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        MSLLHOOKSTRUCT* mouse =
            (MSLLHOOKSTRUCT*)lParam;


        // ------------------------------------------
        // Mouse wheel
        // ------------------------------------------

        if (wParam == WM_MOUSEWHEEL)
        {
            // mouseData contains wheel movement

            SHORT wheelDelta =
                GET_WHEEL_DELTA_WPARAM(
                    mouse->mouseData
                );


            if (wheelDelta > 0)
            {
                // Wheel UP
                ChangeZoom(0.05);
            }
            else if (wheelDelta < 0)
            {
                // Wheel DOWN
                ChangeZoom(-0.05);
            }
        }
    }


    // Pass the mouse event to the next hook

    return CallNextHookEx(
        g_mouseHook,
        nCode,
        wParam,
        lParam
    );
}


// ==================================================
// HOST WINDOW PROCEDURE
// ==================================================

LRESULT CALLBACK HostWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
        // ------------------------------------------
        // Window resized
        // ------------------------------------------

        case WM_SIZE:
        {
            if (g_hMagnifier)
            {
                MoveWindow(
                    g_hMagnifier,
                    0,
                    0,
                    LOWORD(lParam),
                    HIWORD(lParam),
                    TRUE
                );
            }

            return 0;
        }


        // ------------------------------------------
        // GLOBAL HOTKEY
        // ------------------------------------------

        case WM_HOTKEY:
        {
            if (wParam == HOTKEY_ZOOM_IN)
            {
                ChangeZoom(0.25);
            }


            else if (wParam == HOTKEY_ZOOM_OUT)
            {
                ChangeZoom(-0.25);
            }


            else if (wParam == HOTKEY_RESET)
            {
                POINT cursor;

                GetCursorPos(&cursor);

                g_zoom = 1.0;

                SetZoom(g_zoom);

                SetFocus(cursor);
            }

            return 0;
        }


        // ------------------------------------------
        // Window destroyed
        // ------------------------------------------

        case WM_DESTROY:
        {
            // Unregister global hotkeys

            UnregisterHotKey(
                hwnd,
                HOTKEY_ZOOM_IN
            );

            UnregisterHotKey(
                hwnd,
                HOTKEY_ZOOM_OUT
            );

            UnregisterHotKey(
                hwnd,
                HOTKEY_RESET
            );


            // Remove mouse hook

            if (g_mouseHook)
            {
                UnhookWindowsHookEx(
                    g_mouseHook
                );

                g_mouseHook = NULL;
            }


            PostQuitMessage(0);

            return 0;
        }
    }


    return DefWindowProc(
        hwnd,
        msg,
        wParam,
        lParam
    );
}


// ==================================================
// PROGRAM ENTRY POINT
// ==================================================

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    // ------------------------------------------
    // 1. Initialize Magnification API
    // ------------------------------------------

    if (!MagInitialize())
    {
        MessageBoxA(
            NULL,
            "Failed to initialize Magnification API.",
            "Magnifier",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }


    // ------------------------------------------
    // 2. Register HOST window class
    // ------------------------------------------

    WNDCLASSA wc = {};

    wc.lpfnWndProc = HostWndProc;

    wc.hInstance = hInstance;

    wc.lpszClassName = "MagnifierHost";

    wc.hCursor =
        LoadCursor(NULL, IDC_ARROW);


    if (!RegisterClassA(&wc))
    {
        MessageBoxA(
            NULL,
            "Failed to register window class.",
            "Magnifier",
            MB_OK | MB_ICONERROR
        );

        MagUninitialize();

        return 1;
    }


    // ------------------------------------------
    // 3. Create HOST window
    // ------------------------------------------

    HWND hHost = CreateWindowExA(
        0,

        "MagnifierHost",

        "My Magnifier",

        WS_OVERLAPPEDWINDOW,

        100,
        100,

        800,
        600,

        NULL,
        NULL,

        hInstance,

        NULL
    );


    if (!hHost)
    {
        MessageBoxA(
            NULL,
            "Failed to create host window.",
            "Magnifier",
            MB_OK | MB_ICONERROR
        );

        MagUninitialize();

        return 1;
    }


    // ------------------------------------------
    // 4. Create MAGNIFIER control
    // ------------------------------------------

    g_hMagnifier = CreateWindowExA(
        0,

        WC_MAGNIFIER,

        "MagnifierControl",

        WS_CHILD | WS_VISIBLE,

        0,
        0,

        800,
        600,

        hHost,

        NULL,

        hInstance,

        NULL
    );


    if (!g_hMagnifier)
    {
        MessageBoxA(
            NULL,
            "Failed to create magnifier control.",
            "Magnifier",
            MB_OK | MB_ICONERROR
        );

        DestroyWindow(hHost);

        MagUninitialize();

        return 1;
    }


    // ------------------------------------------
    // 5. Set initial zoom
    // ------------------------------------------

    SetZoom(g_zoom);


    // ------------------------------------------
    // 6. Initial focus = center of screen
    // ------------------------------------------

    POINT center;

    center.x =
        GetSystemMetrics(SM_CXSCREEN) / 2;

    center.y =
        GetSystemMetrics(SM_CYSCREEN) / 2;


    SetFocus(center);


    // ------------------------------------------
    // 7. Register GLOBAL HOTKEYS
    // ------------------------------------------

    if (!RegisterHotKey(
        hHost,
        HOTKEY_ZOOM_IN,
        MOD_CONTROL,
        VK_OEM_PLUS))
    {
        MessageBoxA(
            NULL,
            "Could not register Ctrl + +",
            "Magnifier",
            MB_OK | MB_ICONWARNING
        );
    }


    if (!RegisterHotKey(
        hHost,
        HOTKEY_ZOOM_OUT,
        MOD_CONTROL,
        VK_OEM_MINUS))
    {
        MessageBoxA(
            NULL,
            "Could not register Ctrl + -",
            "Magnifier",
            MB_OK | MB_ICONWARNING
        );
    }


    if (!RegisterHotKey(
        hHost,
        HOTKEY_RESET,
        MOD_CONTROL,
        'R'))
    {
        MessageBoxA(
            NULL,
            "Could not register Ctrl + R",
            "Magnifier",
            MB_OK | MB_ICONWARNING
        );
    }


    // ------------------------------------------
    // 8. Install GLOBAL MOUSE HOOK
    // ------------------------------------------

    g_mouseHook = SetWindowsHookExA(
        WH_MOUSE_LL,
        LowLevelMouseProc,
        hInstance,
        0
    );


    if (!g_mouseHook)
    {
        MessageBoxA(
            NULL,
            "Could not install mouse hook.",
            "Magnifier",
            MB_OK | MB_ICONERROR
        );

        DestroyWindow(hHost);

        MagUninitialize();

        return 1;
    }


    // ------------------------------------------
    // 9. Show window
    // ------------------------------------------

    ShowWindow(
        hHost,
        nCmdShow
    );

    UpdateWindow(hHost);


    // ------------------------------------------
    // 10. Message loop
    // ------------------------------------------

    MSG msg;

    while (GetMessage(
        &msg,
        NULL,
        0,
        0))
    {
        TranslateMessage(&msg);

        DispatchMessage(&msg);
    }


    // ------------------------------------------
    // 11. Cleanup
    // ------------------------------------------

    if (g_mouseHook)
    {
        UnhookWindowsHookEx(
            g_mouseHook
        );

        g_mouseHook = NULL;
    }


    MagUninitialize();

    return 0;
}