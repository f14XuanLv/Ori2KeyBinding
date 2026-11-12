#include "runtime.h"
#include "control_descriptions.h"
#include <MinHook.h>
#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <mutex>
#include <vector>

namespace ori {
namespace {
using Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Resize = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
Present original_present{};
Resize original_resize{};
std::recursive_mutex render_mutex;
HWND window{};
WNDPROC previous_proc{};
IDXGISwapChain* active_swap{}; // Borrowed identity only, never AddRef: do not prevent destruction.
ID3D11Device* device{};
ID3D11DeviceContext* context{};
ID3D11RenderTargetView* rtv{};
ImGuiContext* imgui{};
bool last_f8 = false, last_esc = false, swap_conflicts = true;
int capture = -1, selected_pad = 0;
bool capture_armed = false;
ULONGLONG capture_deadline = 0, last_probe = 0;
std::array<bool, 4> connected{};
std::string message;
std::mutex event_mutex;
struct Event { UINT msg; WPARAM w; LPARAM l; };
std::vector<Event> events;

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (panel_open) {
        if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_KILLFOCUS) {
            std::scoped_lock lock(event_mutex);
            if (events.size() < 512) events.push_back({msg, w, l});
        }
        if ((msg >= WM_KEYFIRST && msg <= WM_KEYLAST) ||
            (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_INPUT) {
            if (msg == WM_INPUT) return DefWindowProcW(hwnd, msg, w, l);
            return 0;
        }
    }
    return CallWindowProcW(previous_proc, hwnd, msg, w, l);
}
void release_rtv() { if (rtv) { rtv->Release(); rtv = nullptr; } }
bool make_rtv(IDXGISwapChain* swap) {
    ID3D11Texture2D* back{};
    if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    auto hr = device->CreateRenderTargetView(back, nullptr, &rtv);
    back->Release();
    return SUCCEEDED(hr);
}
void cleanup() {
    overlay_ready = false;
    panel_open = false;
    if (window && previous_proc && reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC)) == window_proc)
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous_proc));
    if (imgui) {
        ImGui::SetCurrentContext(imgui);
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext(imgui);
        imgui = nullptr;
    }
    release_rtv();
    if (context) { context->Release(); context = nullptr; }
    if (device) { device->Release(); device = nullptr; }
    active_swap = nullptr;
    window = nullptr;
    previous_proc = nullptr;
}
bool initialize(IDXGISwapChain* swap) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap->GetDesc(&desc)) || !desc.OutputWindow) return false;
    wchar_t cls[64]{}; GetClassNameW(desc.OutputWindow, cls, 64);
    if (wcscmp(cls, L"UnityWndClass") && wcscmp(cls, L"OriPadHarness")) return false;
    if (FAILED(swap->GetDevice(IID_PPV_ARGS(&device)))) return false;
    device->GetImmediateContext(&context);
    if (!make_rtv(swap)) { cleanup(); return false; }
    imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    wchar_t windows[MAX_PATH]{}; GetWindowsDirectoryW(windows, MAX_PATH);
    const auto font = std::filesystem::path(windows) / "Fonts" / "msyh.ttc";
    if (std::filesystem::exists(font)) io.Fonts->AddFontFromFileTTF(font.string().c_str(), 19.f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    else io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 10; style.FrameRounding = 5; style.FramePadding = {10, 6};
    style.Colors[ImGuiCol_WindowBg] = {0.035f, 0.065f, 0.10f, 0.98f};
    style.Colors[ImGuiCol_Button] = {0.09f, 0.24f, 0.32f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = {0.10f, 0.40f, 0.51f, 1};
    if (!ImGui_ImplDX11_Init(device, context)) { cleanup(); return false; }
    window = desc.OutputWindow;
    previous_proc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC));
    SetLastError(0);
    const auto replaced = SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(window_proc));
    if (!replaced) { log("Window subclass failed"); cleanup(); return false; }
    active_swap = swap;
    overlay_ready = true;
    log("Overlay attached to game D3D11 swap chain");
    return true;
}
void consume_events(ImGuiIO& io) {
    std::vector<Event> batch;
    { std::scoped_lock lock(event_mutex); batch.swap(events); }
    RECT rect{}; GetClientRect(window, &rect);
    POINT mouse{}; GetCursorPos(&mouse); ScreenToClient(window, &mouse);
    const float sx = rect.right ? io.DisplaySize.x / rect.right : 1.f;
    const float sy = rect.bottom ? io.DisplaySize.y / rect.bottom : 1.f;
    io.AddMousePosEvent(mouse.x * sx, mouse.y * sy);
    for (auto& e : batch) switch (e.msg) {
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: io.AddMouseButtonEvent(0, true); break;
        case WM_LBUTTONUP: io.AddMouseButtonEvent(0, false); break;
        case WM_RBUTTONDOWN: io.AddMouseButtonEvent(1, true); break;
        case WM_RBUTTONUP: io.AddMouseButtonEvent(1, false); break;
        case WM_MOUSEWHEEL: io.AddMouseWheelEvent(0, static_cast<short>(HIWORD(e.w)) / 120.f); break;
        case WM_KILLFOCUS: io.AddMouseButtonEvent(0, false); io.AddMouseButtonEvent(1, false); break;
        default: break;
    }
}
void draw_panel() {
    auto cfg = config();
    bool changed = false;
    const auto now = GetTickCount64();
    if (now - last_probe > 1000) {
        for (int i = 0; i < 4; ++i) { XINPUT_STATE s{}; connected[i] = read_raw(i, s); }
        last_probe = now;
    }
    XINPUT_STATE raw{};
    bool has_pad = connected[selected_pad] && read_raw(selected_pad, raw);
    if (capture >= 0) {
        if (now > capture_deadline) { capture = -1; message = "等待超时，绑定未更改。"; }
        else if (has_pad) {
            int pressed = -1;
            for (int i = 0; i < control_count; ++i) if (magnitude(raw.Gamepad, i, cfg.threshold)) { pressed = i; break; }
            if (pressed < 0) capture_armed = true;
            else if (capture_armed) { assign(cfg, capture, pressed, swap_conflicts); changed = true; capture = -1; message = "绑定已生效；点击保存以便下次启动继续使用。"; }
        }
    }
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x * .5f, display.y * .5f}, ImGuiCond_FirstUseEver, {.5f, .5f});
    ImGui::SetNextWindowSize({std::min(920.f, display.x - 24), std::min(820.f, display.y - 24)}, ImGuiCond_FirstUseEver);
    bool visible = true;
    if (ImGui::Begin("Ori2 手柄改键  |  OriPadMod", &visible)) {
        ImGui::TextWrapped("先暂停游戏，再按 F8 打开面板。点击绑定后按下手柄按键。F8 / Esc 关闭。\n面板打开期间屏蔽手柄游戏输入；世界不会自动暂停。");
        changed |= ImGui::Checkbox("启用自定义映射", &cfg.enabled);
        ImGui::SameLine(); ImGui::Checkbox("自动交换冲突绑定", &swap_conflicts);
        if (ImGui::BeginCombo("捕获手柄", ("XInput " + std::to_string(selected_pad + 1)).c_str())) {
            for (int i = 0; i < 4; ++i) if (ImGui::Selectable(("XInput " + std::to_string(i + 1) + (connected[i] ? "（已连接）" : "（未连接）")).c_str(), selected_pad == i)) { selected_pad = i; capture = -1; }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("同一套映射应用于所有 XInput 手柄；PS 手柄需先经 Steam Input / DS4Windows 转为 XInput。");
        ImGui::TextWrapped("按左侧原版功能找到要改的操作，悬停可查看完整用途。X/Y/B 是可装备能力槽。菜单也会同步改键，游戏原提示不变；模拟摇杆方向保持原样。");
        if (ImGui::BeginTable("bindings", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, {0, std::max(140.f, ImGui::GetContentRegionAvail().y - 150)})) {
            ImGui::TableSetupColumn("原版功能", ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableSetupColumn("游戏原按键", ImGuiTableColumnFlags_WidthFixed, 110);
            ImGui::TableSetupColumn("实际按键（点击修改）", ImGuiTableColumnFlags_WidthStretch, 1.6f);
            ImGui::TableSetupColumn("清除", ImGuiTableColumnFlags_WidthFixed, 55);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (int target = 0; target < control_count; ++target) {
                ImGui::PushID(target);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s", control_descriptions[target].summary);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", control_descriptions[target].details);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(names[target]);
                ImGui::TableSetColumnIndex(2);
                const char* text = capture == target ? "松开后按新按键…" : cfg.source[target] < 0 ? "未绑定" : names[cfg.source[target]];
                if (ImGui::Button(text, {-1, 0})) {
                    capture = target; capture_armed = false; capture_deadline = now + 15000;
                    message = "捕获期间可按 Esc 取消。";
                }
                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("×")) { cfg.source[target] = -1; capture = -1; changed = true; }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        changed |= ImGui::SliderInt("扳机触发阈值", &cfg.threshold, 1, 254);
        if (changed) set_config(cfg);
        if (ImGui::Button("保存配置")) message = save_config() ? "配置已保存。" : "保存失败，请检查目录写入权限。";
        ImGui::SameLine();
        if (ImGui::Button("重新读取")) { message = reload_config() ? "已读取保存的配置。" : "配置格式错误，保留当前映射。"; capture = -1; }
        ImGui::SameLine();
        if (ImGui::Button("恢复默认")) { set_config(Config{}); capture = -1; message = "已恢复默认；点击保存可写入文件。"; }
        if (!message.empty()) ImGui::TextWrapped("%s", message.c_str());
        ImGui::TextDisabled("输入调用: %llu    渲染帧: %llu", input_calls.load(), present_calls.load());
    }
    ImGui::End();
    if (!visible) { panel_open = false; capture = -1; }
}
HRESULT STDMETHODCALLTYPE on_present(IDXGISwapChain* swap, UINT interval, UINT flags) {
    if (flags & DXGI_PRESENT_TEST) return original_present(swap, interval, flags);
    {
        std::scoped_lock lock(render_mutex);
        if (active_swap && active_swap != swap) {
            DXGI_SWAP_CHAIN_DESC desc{}; swap->GetDesc(&desc);
            if (desc.OutputWindow == window) cleanup();
        }
        if (!active_swap) initialize(swap);
        if (active_swap == swap && imgui) {
            ++present_calls;
            ImGui::SetCurrentContext(imgui);
            const bool focused = GetForegroundWindow() == window;
            const bool f8 = focused && (GetAsyncKeyState(VK_F8) & 0x8000);
            const bool esc = focused && (GetAsyncKeyState(VK_ESCAPE) & 0x8000);
            if (f8 && !last_f8) { panel_open = !panel_open; capture = -1; log(panel_open ? "Panel opened" : "Panel closed"); }
            if (esc && !last_esc && panel_open) { if (capture >= 0) capture = -1; else panel_open = false; }
            last_f8 = f8; last_esc = esc;
            auto& io = ImGui::GetIO();
            io.MouseDrawCursor = panel_open && focused;
            if (panel_open) {
                if (!rtv) make_rtv(swap);
                if (rtv) {
                    ID3D11Texture2D* back{};
                    if (SUCCEEDED(swap->GetBuffer(0, IID_PPV_ARGS(&back)))) {
                        D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc); back->Release();
                        io.DisplaySize = {static_cast<float>(desc.Width), static_cast<float>(desc.Height)};
                    }
                    static ULONGLONG last_frame{};
                    const auto now = GetTickCount64();
                    io.DeltaTime = last_frame ? std::clamp((now - last_frame) / 1000.f, .001f, .1f) : 1.f / 60;
                    last_frame = now;
                    consume_events(io);
                    ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
                    draw_panel(); ImGui::Render();
                    ID3D11RenderTargetView* saved[8]{}; ID3D11DepthStencilView* depth{};
                    context->OMGetRenderTargets(8, saved, &depth);
                    context->OMSetRenderTargets(1, &rtv, nullptr);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                    context->OMSetRenderTargets(8, saved, depth);
                    for (auto* v : saved) if (v) v->Release();
                    if (depth) depth->Release();
                }
            } else {
                std::scoped_lock elock(event_mutex); events.clear();
            }
        }
    }
    const auto hr = original_present(swap, interval, flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) { std::scoped_lock lock(render_mutex); cleanup(); }
    return hr;
}
HRESULT STDMETHODCALLTYPE on_resize(IDXGISwapChain* swap, UINT count, UINT w, UINT h, DXGI_FORMAT format, UINT flags) {
    std::scoped_lock lock(render_mutex);
    if (swap == active_swap) release_rtv();
    return original_resize(swap, count, w, h, format, flags);
}
}
bool install_overlay() {
    // Discover public DXGI vtable methods using an invisible, independent helper window.
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.hInstance = instance; wc.lpfnWndProc = DefWindowProcW; wc.lpszClassName = L"OriPadBootstrap";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    HWND helper = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 8, 8, nullptr, nullptr, instance, nullptr);
    if (!helper) return false;
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 1; desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow = helper;
    desc.SampleDesc.Count = 1; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain* swap{}; ID3D11Device* probe{}; ID3D11DeviceContext* ctx{};
    auto hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &swap, &probe, nullptr, &ctx);
    bool result = false;
    if (SUCCEEDED(hr)) {
        auto** vtable = *reinterpret_cast<void***>(swap);
        auto p = vtable[8], r = vtable[13];
        if (MH_CreateHook(p, reinterpret_cast<void*>(&on_present), reinterpret_cast<void**>(&original_present)) == MH_OK) {
            if (MH_CreateHook(r, reinterpret_cast<void*>(&on_resize), reinterpret_cast<void**>(&original_resize)) == MH_OK) {
                if (MH_EnableHook(r) == MH_OK && MH_EnableHook(p) == MH_OK) result = true;
                if (!result) { MH_DisableHook(r); MH_RemoveHook(r); }
            }
            if (!result) { MH_DisableHook(p); MH_RemoveHook(p); }
        }
    }
    if (ctx) ctx->Release(); if (probe) probe->Release(); if (swap) swap->Release();
    DestroyWindow(helper); UnregisterClassW(wc.lpszClassName, instance);
    return result;
}
}
