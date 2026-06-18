export module GW2Viewer.Services.Graphics;
import GW2Viewer.UI.ImGui;
import std;
import <d3d11.h>;

export namespace GW2Viewer::Services
{

struct Graphics
{
    ImVec4 BackgroundColor { };
    bool VSync = true;

    ID3D11Device* Device = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    ID3D11RenderTargetView* RenderTargetView = nullptr;

    bool Start(HWND outputWindow);
    void Stop();

    bool Resize(ImVec2i size);
    void Clear() const;
    void Present() const;

private:
    bool CreateRenderTargetView();

    void Release(auto& field)
    {
        if (auto const obj = std::exchange(field, nullptr))
        {
            try { obj->Release(); }
            catch (...) { }
        }
    }
};
}

export namespace GW2Viewer::G::Services { GW2Viewer::Services::Graphics Graphics; }

module :private;

namespace GW2Viewer::Services
{

bool Graphics::Start(HWND outputWindow)
{
    // Setup swap chain
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
    sd.OutputWindow = outputWindow;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    constexpr D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &SwapChain, &Device, &featureLevel, &DeviceContext)))
        return false;

    return CreateRenderTargetView();
}

void Graphics::Stop()
{
    Release(RenderTargetView);
    Release(SwapChain);
    Release(DeviceContext);
    Release(Device);
}

bool Graphics::Resize(ImVec2i size)
{
    if (!Device)
        return true;

    Release(RenderTargetView);
    if (FAILED(SwapChain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0)))
        return false;
    return CreateRenderTargetView();
}

void Graphics::Clear() const
{
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, nullptr);
    DeviceContext->ClearRenderTargetView(RenderTargetView, &BackgroundColor.x);
}

void Graphics::Present() const
{
    SwapChain->Present(VSync, 0);
}

bool Graphics::CreateRenderTargetView()
{
    ID3D11Texture2D* backBuffer;
    if (FAILED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        return false;
    if (FAILED(Device->CreateRenderTargetView(backBuffer, nullptr, &RenderTargetView)))
        return false;
    Release(backBuffer);
    return true;
}

}
