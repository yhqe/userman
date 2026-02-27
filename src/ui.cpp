#include "ui.h"
#include <cmath>
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <wininet.h>
#include "ext/imgui/imgui_freetype.h"

#pragma comment(lib, "wininet.lib")

ID3D11Device* ui::pd3dDevice = nullptr;
ID3D11DeviceContext* ui::pd3dDeviceContext = nullptr;
IDXGISwapChain* ui::pSwapChain = nullptr;
ID3D11RenderTargetView* ui::pMainRenderTargetView = nullptr;

HMODULE ui::hCurrentModule = nullptr;

LPCSTR ui::lpWindowName = "lambda";
ImVec2 ui::vWindowSize = { 550, 400 };
ImGuiWindowFlags ui::windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
bool ui::bDraw = true;

void ui::active()
{
    bDraw = true;
}

bool ui::isActive()
{
    return bDraw == true;
}

bool ui::CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const UINT createDeviceFlags = 0;
    
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &pSwapChain, &pd3dDevice, &featureLevel, &pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void ui::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer != nullptr)
    {
        pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pMainRenderTargetView);
        pBackBuffer->Release();
    }
}

void ui::CleanupRenderTarget()
{
    if (pMainRenderTargetView)
    {
        pMainRenderTargetView->Release();
        pMainRenderTargetView = nullptr;
    }
}

void ui::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (pSwapChain)
    {
        pSwapChain->Release();
        pSwapChain = nullptr;
    }

    if (pd3dDeviceContext)
    {
        pd3dDeviceContext->Release();
        pd3dDeviceContext = nullptr;
    }

    if (pd3dDevice)
    {
        pd3dDevice->Release();
        pd3dDevice = nullptr;
    }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

LRESULT WINAPI ui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;

    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;

    default:
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

constexpr auto to_rgba = [](uint32_t argb) constexpr
{
    ImVec4 color{};
    color.x = ((argb >> 16) & 0xFF) / 255.0f;
    color.y = ((argb >> 8) & 0xFF) / 255.0f;
    color.z = (argb & 0xFF) / 255.0f;
    color.w = ((argb >> 24) & 0xFF) / 255.0f;
    return color;
};

constexpr auto lerp = [](const ImVec4& a, const ImVec4& b, float t) constexpr
{
    return ImVec4{
        std::lerp(a.x, b.y, t),
        std::lerp(a.y, b.y, t),
        std::lerp(a.z, b.z, t),
        std::lerp(a.w, b.w, t)
    };
};

void set_colors(ImGuiStyle style) {
    auto colors = style.Colors;
    colors[ImGuiCol_Text] = to_rgba(0xFFABB2BF);
    colors[ImGuiCol_TextDisabled] = to_rgba(0xFF565656);
    colors[ImGuiCol_WindowBg] = to_rgba(0xFF282C34);
    colors[ImGuiCol_ChildBg] = to_rgba(0xFF21252B);
    colors[ImGuiCol_PopupBg] = to_rgba(0xFF2E323A);
    colors[ImGuiCol_Border] = to_rgba(0xFF2E323A);
    colors[ImGuiCol_BorderShadow] = to_rgba(0x00000000);
    colors[ImGuiCol_FrameBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_FrameBgHovered] = to_rgba(0xFF484C52);
    colors[ImGuiCol_FrameBgActive] = to_rgba(0xFF54575D);
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_TitleBgCollapsed] = to_rgba(0x8221252B);
    colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_PopupBg];
    colors[ImGuiCol_ScrollbarGrab] = to_rgba(0xFF3E4249);
    colors[ImGuiCol_ScrollbarGrabHovered] = to_rgba(0xFF484C52);
    colors[ImGuiCol_ScrollbarGrabActive] = to_rgba(0xFF54575D);
    colors[ImGuiCol_CheckMark] = colors[ImGuiCol_Text];
    colors[ImGuiCol_SliderGrab] = to_rgba(0xFF353941);
    colors[ImGuiCol_SliderGrabActive] = to_rgba(0xFF7A7A7A);
    colors[ImGuiCol_Button] = colors[ImGuiCol_SliderGrab];
    colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_ScrollbarGrabActive];
    colors[ImGuiCol_Header] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_HeaderHovered] = to_rgba(0xFF353941);
    colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_Separator] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_SeparatorHovered] = to_rgba(0xFF3E4452);
    colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_SeparatorHovered];
    colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_Separator];
    colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_SeparatorHovered];
    colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_SeparatorActive];
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_Tab] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_PlotLines] = ImVec4{ 0.61f, 0.61f, 0.61f, 1.00f };
    colors[ImGuiCol_PlotLinesHovered] = ImVec4{ 1.00f, 0.43f, 0.35f, 1.00f };
    colors[ImGuiCol_PlotHistogram] = ImVec4{ 0.90f, 0.70f, 0.00f, 1.00f };
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4{ 1.00f, 0.60f, 0.00f, 1.00f };
    colors[ImGuiCol_TableHeaderBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_SliderGrab];
    colors[ImGuiCol_TableBorderLight] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_TableRowBg] = ImVec4{ 0.00f, 0.00f, 0.00f, 0.00f };
    colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.00f, 1.00f, 1.00f, 0.06f };
    colors[ImGuiCol_TextSelectedBg] = to_rgba(0xFF243140);
    colors[ImGuiCol_DragDropTarget] = colors[ImGuiCol_Text];
    colors[ImGuiCol_NavWindowingHighlight] = colors[ImGuiCol_Text];
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4{ 0.80f, 0.80f, 0.80f, 0.20f };
    colors[ImGuiCol_ModalWindowDimBg] = to_rgba(0xC821252B);
}

char url_buf[512] = "";
//std::string user_downloads = std::getenv("USERPROFILE");
// const char* custom_dir = user_downloads.c_str();
//char custom_dir[256] = user_downloads.c_str();

void ui::render()
{
    ImGui_ImplWin32_EnableDpiAwareness();
    const WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, _T("userman"), nullptr };
    ::RegisterClassEx(&wc);
    const HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("userman"), WS_OVERLAPPEDWINDOW, 100, 100, 50, 50, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return;
    }

    ::ShowWindow(hwnd, SW_HIDE);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    {
        auto colors = style.Colors;
        colors[ImGuiCol_Text] = to_rgba(0xFFABB2BF);
        colors[ImGuiCol_TextDisabled] = to_rgba(0xFF565656);
        colors[ImGuiCol_WindowBg] = to_rgba(0xFF282C34);
        colors[ImGuiCol_ChildBg] = to_rgba(0xFF21252B);
        colors[ImGuiCol_PopupBg] = to_rgba(0xFF2E323A);
        colors[ImGuiCol_Border] = to_rgba(0xFF2E323A);
        colors[ImGuiCol_BorderShadow] = to_rgba(0x00000000);
        colors[ImGuiCol_FrameBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_FrameBgHovered] = to_rgba(0xFF484C52);
        colors[ImGuiCol_FrameBgActive] = to_rgba(0xFF54575D);
        colors[ImGuiCol_TitleBg] = colors[ImGuiCol_PopupBg];
        colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_WindowBg];
        colors[ImGuiCol_TitleBgCollapsed] = to_rgba(0x8221252B);
        colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_PopupBg];
        colors[ImGuiCol_ScrollbarGrab] = to_rgba(0xFF3E4249);
        colors[ImGuiCol_ScrollbarGrabHovered] = to_rgba(0xFF484C52);
        colors[ImGuiCol_ScrollbarGrabActive] = to_rgba(0xFF54575D);
        colors[ImGuiCol_CheckMark] = colors[ImGuiCol_Text];
        colors[ImGuiCol_SliderGrab] = to_rgba(0xFF353941);
        colors[ImGuiCol_SliderGrabActive] = to_rgba(0xFF7A7A7A);
        colors[ImGuiCol_Button] = colors[ImGuiCol_SliderGrab];
        colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_ScrollbarGrabActive];
        colors[ImGuiCol_Header] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_HeaderHovered] = to_rgba(0xFF353941);
        colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_Separator] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_SeparatorHovered] = to_rgba(0xFF3E4452);
        colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_SeparatorHovered];
        colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_Separator];
        colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_SeparatorHovered];
        colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_SeparatorActive];
        colors[ImGuiCol_TabHovered] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_Tab] = colors[ImGuiCol_HeaderHovered];
        colors[ImGuiCol_TabActive] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_WindowBg];
        colors[ImGuiCol_PlotLines] = ImVec4{ 0.61f, 0.61f, 0.61f, 1.00f };
        colors[ImGuiCol_PlotLinesHovered] = ImVec4{ 1.00f, 0.43f, 0.35f, 1.00f };
        colors[ImGuiCol_PlotHistogram] = ImVec4{ 0.90f, 0.70f, 0.00f, 1.00f };
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4{ 1.00f, 0.60f, 0.00f, 1.00f };
        colors[ImGuiCol_TableHeaderBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_SliderGrab];
        colors[ImGuiCol_TableBorderLight] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_TableRowBg] = ImVec4{ 0.00f, 0.00f, 0.00f, 0.00f };
        colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.00f, 1.00f, 1.00f, 0.06f };
        colors[ImGuiCol_TextSelectedBg] = to_rgba(0xFF243140);
        colors[ImGuiCol_DragDropTarget] = colors[ImGuiCol_Text];
        colors[ImGuiCol_NavWindowingHighlight] = colors[ImGuiCol_Text];
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4{ 0.80f, 0.80f, 0.80f, 0.20f };
        colors[ImGuiCol_ModalWindowDimBg] = to_rgba(0xC821252B);
    }

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowBorderSize = 3.0f;

        // Rounding
        style.FrameRounding = 1.0f;
        style.PopupRounding = 1.0f;
        style.ScrollbarRounding = 1.0f;
        style.GrabRounding = 1.0f;
    }

    ImFontConfig cfg;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
    cfg.PixelSnapH = false;
    cfg.SizePixels = 11.0f;
    cfg.RasterizerMultiply = 1.0f;

    char windows_directory[MAX_PATH];
    GetWindowsDirectoryA(windows_directory, MAX_PATH);

    std::string tahoma_bold_font_directory = (std::string)windows_directory + ("\\Fonts\\tahomabd.ttf");
    std::string tahoma_font_directory = (std::string)windows_directory + ("\\Fonts\\tahoma.ttf");
    std::string icons_font_directory = (std::string)windows_directory + ("\\Fonts\\tahoma.ttf");

    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
    builder.BuildRanges(&ranges);

    ImFont* tahoma = io.Fonts->AddFontFromFileTTF(tahoma_font_directory.c_str(), 11.0f, &cfg, ranges.Data);
    ImFont* tahoma_bold = io.Fonts->AddFontFromFileTTF(tahoma_bold_font_directory.c_str(), 11.0f, &cfg, ranges.Data);

    // ImGui::GetIO().IniFilename = nullptr;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(pd3dDevice, pd3dDeviceContext);

    const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool bDone = false;

    while (!bDone)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                bDone = true;
        }

        if (GetAsyncKeyState(VK_END) & 1)
            bDone = true;

        if (bDone)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        {
            if (isActive())
            {
                ImGui::SetNextWindowSize(vWindowSize, ImGuiCond_Once);
                ImGui::SetNextWindowBgAlpha(1.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5, 0.5));
                ImGui::Begin(lpWindowName, &bDraw, windowFlags);
                {
                    if (ImGui::BeginTabBar("##main_tabs", 0))
                    {
                        if (ImGui::BeginTabItem("downloader"))
                        {
                            ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() / 2.f - 100.f, ImGui::GetWindowHeight() / 2.f - 10.f));
                            ImGui::InputTextWithHint("##url", "enter url here", url_buf, IM_ARRAYSIZE(url_buf));
                            ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() / 2.f - 100.f, ImGui::GetWindowHeight() / 2.f + 10.f));
                            if (ImGui::Button("download", ImVec2(200, 17)))
                            {
                                if (!url_buf)
                                    MessageBoxA(NULL, "lambda", "no url provided!", MB_OK);

                                std::string command = "./bin/yt-dlp.exe " + (std::string)url_buf;
                                std::system(command.c_str());
                            }
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("queue"))
                        {
                            ImGui::Text("queue tab ");
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("settings"))
                        {
                            ImGui::Text("custom download directory");
                            //if (ImGui::InputText("##custom_dir", dir_buf, sizeof(dir_buf), 0))
                                
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                }
                ImGui::End();
                ImGui::PopStyleVar();
            }

            // ImGui::ShowDemoWindow();
            ImGui::ShowMetricsWindow();
            ImGui::ShowStackToolWindow();
        }
        ImGui::EndFrame();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        pd3dDeviceContext->OMSetRenderTargets(1, &pMainRenderTargetView, nullptr);
        pd3dDeviceContext->ClearRenderTargetView(pMainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        pSwapChain->Present(0, 0);

        #ifndef _WINDLL
            if (!ui::isActive())
                break;
        #endif
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    #ifdef _WINDLL
    CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE)FreeLibrary, hCurrentModule, NULL, nullptr);
    #endif
}