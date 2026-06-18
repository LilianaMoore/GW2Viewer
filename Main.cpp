#include "Resource.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <d3d11.h>
//#define DIRECTINPUT_VERSION 0x0800
//#include <dinput.h>
#include <tchar.h>
#include <atomic>
#include <csignal>
#include <exception>
#include <format>
#include <stacktrace>

import GW2Viewer.Common.Time;
import GW2Viewer.Data.Game;
import GW2Viewer.Services.Graphics;
import GW2Viewer.UI.Manager;
import GW2Viewer.User.Config;

void Render()
{
    using namespace GW2Viewer;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    G::UI.Update();

    // Rendering
    ImGui::Render();
    G::Services::Graphics.Clear();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    G::Services::Graphics.Present();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    using namespace GW2Viewer;

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    static bool windowResizing = false;
    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            G::Services::Graphics.Resize({ LOWORD(lParam), HIWORD(lParam) });
        return 0;
    case WM_ENTERSIZEMOVE:
        InvalidateRect(hWnd, nullptr, true);
        windowResizing = true;
        break;
    case WM_EXITSIZEMOVE:
        windowResizing = false;
        break;
    case WM_PAINT:
        if (G::UI.IsLoaded())
        {
            Render();
            if (windowResizing)
                return 0;
        }
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_CLOSE:
        if (!G::Config.Save())
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    using namespace GW2Viewer;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Create application window
    WNDCLASSEX wc =
    {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_CLASSDC,
        .lpfnWndProc = WndProc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = hInstance,
        .hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON)),
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .hbrBackground = nullptr,
        .lpszMenuName = nullptr,
        .lpszClassName = _T("GW2Viewer Window"),
        .hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON)),
    };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(_T("GW2Viewer Window"), _T("GW2Viewer"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    if (!G::Services::Graphics.Start(hwnd))
        return false;

    // Show the window
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(G::Services::Graphics.Device, G::Services::Graphics.DeviceContext);

    G::Config.Load();
    G::UI.Load();

    if (!IsDebuggerPresent())
    {
        static auto crashHandler = []
        {
            if (static std::atomic_flag crashed { }; !crashed.test_and_set())
            {
                try { G::Config.Save(true); }
                catch (...) { }

                try { std::ofstream(std::format("crash-{:%F_%H-%M-%S}.log", Time::Now())) << std::stacktrace::current(); }
                catch (...) { }

                MessageBox(nullptr, L"GW2Viewer has crashed. Config and stacktrace were saved.", L"GW2Viewer", MB_OK | MB_ICONERROR);
            }
        };
        SetUnhandledExceptionFilter([](_EXCEPTION_POINTERS* pExceptionInfo) -> long __stdcall
        {
            crashHandler();
            return EXCEPTION_CONTINUE_SEARCH;
        });
        std::set_terminate([]
        {
            crashHandler();
            std::set_terminate(nullptr);
            std::terminate();
        });
        auto signalHandler = [](int signal)
        {
            crashHandler();
            std::signal(signal, SIG_DFL);
            raise(signal);
        };
        std::signal(SIGABRT, signalHandler);
        std::signal(SIGFPE, signalHandler);
        std::signal(SIGILL, signalHandler);
        std::signal(SIGSEGV, signalHandler);
        std::signal(SIGTERM, signalHandler);
    }

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        Render();

        if (GetForegroundWindow() != hwnd && !ImGui::GetIO().WantCaptureMouse)
            Sleep(100);
    }

    G::UI.Unload();
    //G::Config.Save();

    G::Game.Texture.StopLoading();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    G::Services::Graphics.Stop();
    DestroyWindow(hwnd);
    UnregisterClass(_T("GW2Viewer Window"), wc.hInstance);

    CoUninitialize();

    return 0;
}
