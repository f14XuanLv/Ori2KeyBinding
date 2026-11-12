#include <Windows.h>
#include <Xinput.h>
#include <d3d11.h>
#include <dxgi.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

namespace {
void require(bool condition, const char* what) {
    std::ofstream("harness.log", std::ios::app) << (condition ? "PASS " : "FAIL ") << what << '\n';
    if (!condition) ExitProcess(1);
}
void capture(IDXGISwapChain* swap, ID3D11Device* device, ID3D11DeviceContext* context) {
    ID3D11Texture2D* back{}; require(SUCCEEDED(swap->GetBuffer(0, IID_PPV_ARGS(&back))), "capture buffer");
    D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    ID3D11Texture2D* staging{};
    require(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)), "capture staging");
    context->CopyResource(staging, back); back->Release();
    D3D11_MAPPED_SUBRESOURCE map{};
    require(SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &map)), "capture map");
    std::vector<unsigned char> pixels(static_cast<size_t>(desc.Width) * desc.Height * 4);
    for (UINT y = 0; y < desc.Height; ++y) for (UINT x = 0; x < desc.Width; ++x) {
        const auto* source = static_cast<BYTE*>(map.pData) + y * map.RowPitch + x * 4;
        auto* dst = pixels.data() + (static_cast<size_t>(y) * desc.Width + x) * 4;
        dst[0] = source[2]; dst[1] = source[1]; dst[2] = source[0]; dst[3] = 255;
    }
    context->Unmap(staging, 0); staging->Release();
    BITMAPFILEHEADER file{}; file.bfType = 0x4D42; file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    BITMAPINFOHEADER info{}; info.biSize = sizeof(info); info.biWidth = desc.Width; info.biHeight = -static_cast<LONG>(desc.Height);
    info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
    std::ofstream out("panel.bmp", std::ios::binary); out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    out.write(reinterpret_cast<const char*>(&info), sizeof(info)); out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    std::ofstream("harness.log", std::ios::trunc) << "Hidden D3D11 integration harness\n";
    wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr, path, MAX_PATH);
    auto dir = std::filesystem::path(path).parent_path();
    auto module = LoadLibraryW((dir / "version.dll").c_str());
    require(module != nullptr, "load Mod DLL");
    auto status = reinterpret_cast<unsigned long long(*)()>(GetProcAddress(module, "OriPad_GetStatus"));
    auto show = reinterpret_cast<void(*)(BOOL)>(GetProcAddress(module, "OriPad_SetPanelVisible"));
    auto reload = reinterpret_cast<BOOL(*)()>(GetProcAddress(module, "OriPad_ReloadConfig"));
    require(status && show && reload, "diagnostic exports");
    using VersionSize = DWORD(WINAPI*)(LPCWSTR, LPDWORD);
    auto size = reinterpret_cast<VersionSize>(GetProcAddress(module, "GetFileVersionInfoSizeW"));
    DWORD unused{};
    require(size && size((dir / "OriPadHarness.exe").c_str(), &unused) == 0, "proxy forwards no-version executable");
    wchar_t system[MAX_PATH]{}; GetSystemDirectoryW(system, MAX_PATH);
    require(size((std::filesystem::path(system) / "kernel32.dll").c_str(), &unused) > 0, "proxy forwards system version query");
    WNDCLASSW wc{}; wc.hInstance = instance; wc.lpfnWndProc = DefWindowProcW; wc.lpszClassName = L"OriPadHarness";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"OriPad hidden harness", WS_OVERLAPPEDWINDOW, 0, 0, 1000, 900, nullptr, nullptr, instance, nullptr);
    require(hwnd != nullptr, "create invisible helper window");
    DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferCount = 1; desc.BufferDesc.Width = 1000; desc.BufferDesc.Height = 900;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd; desc.SampleDesc.Count = 1; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    IDXGISwapChain* swap{}; ID3D11Device* device{}; ID3D11DeviceContext* context{};
    require(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &desc, &swap, &device, nullptr, &context)), "create D3D11 swap chain");
    using GetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    auto xinput = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto get = reinterpret_cast<GetState>(GetProcAddress(xinput, "XInputGetState"));
    require(get != nullptr, "system XInput function");
    for (int n = 0; n < 400 && !(status() & 1); ++n) {
        XINPUT_STATE s{}; get(0, &s); swap->Present(0, 0);
        MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
    require((status() & 1) != 0, "overlay attaches to actual D3D11 Present");
    XINPUT_STATE s{}; const auto before = status() >> 2; get(0, &s);
    require((status() >> 2) > before, "system XInput call passes through installed hook");
    show(TRUE);
    for (int n = 0; n < 10; ++n) {
        ID3D11Texture2D* back{}; ID3D11RenderTargetView* view{};
        swap->GetBuffer(0, IID_PPV_ARGS(&back)); device->CreateRenderTargetView(back, nullptr, &view); back->Release();
        const float bg[4]{.07f, .1f, .15f, 1}; context->ClearRenderTargetView(view, bg); view->Release();
        swap->Present(0, 0); Sleep(16);
    }
    require((status() & 2) != 0, "panel opens without foreground automation");
    capture(swap, device, context);
    // Isolated build-directory config: rejection must retain the previous mapping.
    auto cfg_path = dir / "OriPadMod" / "bindings.ini";
    std::ifstream original(cfg_path); const std::string text((std::istreambuf_iterator<char>(original)), {}); original.close();
    { std::ofstream cfg(cfg_path); cfg << "Enabled=True\nA=NotAButton\n"; }
    require(!reload(), "reject malformed mapping");
    { std::ofstream cfg(cfg_path); cfg << "Enabled=True\nTriggerThreshold=30\nA=X\nX=A\n"; }
    require(reload(), "accept valid swap configuration");
    { std::ofstream cfg(cfg_path); cfg << text; }
    require(reload(), "restore original harness config");
    context->ClearState();
    require(SUCCEEDED(swap->ResizeBuffers(1, 1100, 920, DXGI_FORMAT_R8G8B8A8_UNORM, 0)), "resize while panel visible");
    swap->Present(0, 0);
    show(FALSE); swap->Present(0, 0);
    require(!(status() & 2), "panel closes");
    require(!IsWindowVisible(hwnd), "harness window stayed hidden");
    // Process teardown owns hooks and resources; live DLL unloading is intentionally unsupported.
    std::ofstream("harness.log", std::ios::app) << "ALL INTEGRATION CHECKS PASSED\n";
    ExitProcess(0);
}
