#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dwmapi.h>

#include <string>
#include <wrl.h>
#include <wil/com.h>
#include <filesystem>
#include <iostream>

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include "Utility/DirectoryHelper.h"

using namespace Microsoft::WRL;

static wil::com_ptr<ICoreWebView2> webviewWindow;
static wil::com_ptr<ICoreWebView2Controller> webviewController;

LPCWSTR StaticFileDirectory = L"static_files";
LPCWSTR g_szAppName = L"Win32Webview2Cpp";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;
    static bool s_fullscreen = false;

    switch (message)
    {
    case WM_PAINT:
        PAINTSTRUCT ps;
        (void)BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            if (!s_minimized)
            {
                s_minimized = true;
                s_in_suspend = true;
            }
        }
        else if (s_minimized)
        {
            s_minimized = false;
            s_in_suspend = false;
        }

        // Update webview size
        if (webviewController)
        {
            RECT bounds;
            GetClientRect(hWnd, &bounds);
            webviewController->put_Bounds(bounds);
        }
        break;
    case WM_ENTERSIZEMOVE:
        s_in_sizemove = true;
        break;
    case WM_EXITSIZEMOVE:
        s_in_sizemove = false;
        break;
    case WM_GETMINMAXINFO:
        if (lParam)
        {
            auto info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 320;
            info->ptMinTrackSize.y = 200;
        }
        break;
    case WM_POWERBROADCAST:
        switch (wParam)
        {
        case PBT_APMQUERYSUSPEND:
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if (!s_minimized)
            {
                s_in_suspend = false;
            }
            return TRUE;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
        {
            // Implements the classic ALT+ENTER fullscreen toggle
            if (s_fullscreen)
            {
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);

                int width = 800;
                int height = 600;

                ShowWindow(hWnd, SW_SHOWNORMAL);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
            else
            {
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                ShowWindow(hWnd, SW_SHOWMAXIMIZED);
            }

            s_fullscreen = !s_fullscreen;
        }
        break;
    case WM_MENUCHAR:
        // A menu is active and the user presses a key that does not correspond
        // to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
        return MAKELRESULT(0, MNC_CLOSE);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HWND hwnd;

    // Register class and create window
    {
        // Register class
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON");
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszClassName = L"Direct3DWin32Webview2WindowClass";
        wcex.hIconSm = LoadIconW(wcex.hInstance, L"IDI_ICON");
        if (!RegisterClassExW(&wcex))
            return 1;

        // Create window
        int w, h;
        w = 1080;
        h = 720;

        RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };

        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        hwnd = CreateWindowExW(0, L"Direct3DWin32Webview2WindowClass", g_szAppName, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
            nullptr);

        if (!hwnd)
            return 1;

        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);

        GetClientRect(hwnd, &rc);
        
        // Set caption color (Windows 11 only)
        enum DWMWINDOWATTRIBUTE
        {
            DWMWA_CAPTION_COLOR = 35
        };

        COLORREF color = RGB(0x00, 0x2b, 0x36);
        DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE::DWMWA_CAPTION_COLOR, &color, sizeof(color));

        // Setup WebView2
        CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {

                    // Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
                    env->CreateCoreWebView2Controller(hwnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (controller != nullptr) {
                                webviewController = controller;
                                webviewController->get_CoreWebView2(&webviewWindow);
                            }

                            // Add a few settings for the webview
                            // The demo step is redundant since the values are the default settings
                            ICoreWebView2Settings* Settings;
                            webviewWindow->get_Settings(&Settings);
                            Settings->put_IsScriptEnabled(TRUE);
                            Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                            Settings->put_IsWebMessageEnabled(TRUE);

                            // Resize the WebView2 control to fit the bounds of the parent window
                            RECT bounds;
                            GetClientRect(hwnd, &bounds);
                            webviewController->put_Bounds(bounds);

                            // Schedule an async task to navigate
                            wil::com_ptr<ICoreWebView2_3> webView3;
                            webView3 = webviewWindow.try_query<ICoreWebView2_3>();

                            // Get path
                            std::wstring staticFileDirectoryPath = DirectoryHelper::GetExecutableDirectory() + std::wstring(StaticFileDirectory);

                            // Set virtual host and navigate to
                            webView3->SetVirtualHostNameToFolderMapping(
                                L"maximalwebview.example", staticFileDirectoryPath.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                            webView3->Navigate(L"https://maximalwebview.example/index.html");
                            
                            // 4 - Navigation events

                            // 5 - Scripting

                            // 6 - Communication between host and web content
                            EventRegistrationToken token;
                            webviewWindow->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                [](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                    PWSTR message;
                                    args->TryGetWebMessageAsString(&message);

                                    if (lstrcmpW(message, L"closeApp") == 0)
                                    {
                                        PostQuitMessage(0);
                                    }

                                    // processMessage(&message);
                                    // webview->PostWebMessageAsString(message);
                                    CoTaskMemFree(message);
                                    return S_OK;
                                }).Get(), &token);

                            return S_OK;
                        }).Get());
                    return S_OK;
                }).Get());
    }

    // Main message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CoUninitialize();

    return static_cast<int>(msg.wParam);
}