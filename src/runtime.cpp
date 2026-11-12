#include "runtime.h"
#include <MinHook.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <vector>
#include <cstring>

namespace ori {
std::atomic<bool> panel_open{false}, overlay_ready{false};
std::atomic<unsigned long long> input_calls{0}, present_calls{0};
namespace {
using GetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
std::filesystem::path directory;
std::mutex config_mutex, log_mutex, packet_mutex;
Config current;
std::array<XINPUT_GAMEPAD, 4> previous{};
std::array<DWORD, 4> packets{1, 1, 1, 1};
struct Hook { GetState original{}; void* target{}; };
std::array<Hook, 6> hooks{};
GetState physical{};
thread_local bool inside = false;

template<int Slot> DWORD WINAPI intercepted(DWORD index, XINPUT_STATE* state) {
    auto fn = hooks[Slot].original;
    if (!fn) return ERROR_DEVICE_NOT_CONNECTED;
    if (inside) return fn(index, state);
    inside = true;
    const DWORD result = fn(index, state);
    inside = false;
    ++input_calls;
    if (result != ERROR_SUCCESS || !state || index >= 4) return result;
    auto mapped = panel_open.load() ? XINPUT_GAMEPAD{} : map_pad(state->Gamepad, config());
    {
        std::scoped_lock lock(packet_mutex);
        if (std::memcmp(&previous[index], &mapped, sizeof(mapped)) != 0) {
            previous[index] = mapped;
            ++packets[index];
        }
        state->dwPacketNumber = packets[index];
    }
    state->Gamepad = mapped;
    return result;
}
const std::array<GetState, 6> detours{intercepted<0>, intercepted<1>, intercepted<2>, intercepted<3>, intercepted<4>, intercepted<5>};

bool install_input() {
    int slot = 0;
    for (const auto* dll : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
        wchar_t system[MAX_PATH]{}; GetSystemDirectoryW(system, MAX_PATH);
        const auto file = std::filesystem::path(system) / dll;
        const auto module = LoadLibraryExW(file.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module) continue;
        for (int ordinal : {2, 100}) {
            auto target = reinterpret_cast<void*>(GetProcAddress(module, ordinal == 2 ? "XInputGetState" : MAKEINTRESOURCEA(100)));
            if (!target) continue;
            bool exists = false;
            for (const auto& h : hooks) if (h.target == target) exists = true;
            if (exists || slot >= static_cast<int>(hooks.size())) continue;
            auto& h = hooks[slot];
            if (MH_CreateHook(target, reinterpret_cast<void*>(detours[slot]), reinterpret_cast<void**>(&h.original)) != MH_OK) {
                log("XInput hook creation failed at slot " + std::to_string(slot));
                continue;
            }
            h.target = target;
            if (MH_EnableHook(target) != MH_OK) { MH_RemoveHook(target); h = {}; continue; }
            if (!physical && ordinal == 2) physical = h.original;
            log("XInput hook enabled: slot=" + std::to_string(slot) + " ordinal=" + std::to_string(ordinal));
            ++slot;
        }
    }
    return physical != nullptr;
}
DWORD WINAPI worker(void* arg) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(static_cast<HMODULE>(arg), path, MAX_PATH);
    directory = std::filesystem::path(path).parent_path() / L"OriPadMod";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    log("OriPadMod 0.1.2 initializing (XInput + D3D11)");
    HMODULE pinned{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&worker), &pinned);
    reload_config();
    if (MH_Initialize() != MH_OK) { log("MinHook initialization failed; Mod inactive"); return 0; }
    if (!install_input()) { log("No XInput hooks available; Mod inactive"); return 0; }
    log(install_overlay() ? "D3D11 hooks installed; press F8 in game" : "D3D11 overlay unavailable; config-file mapping remains active");
    return 0;
}
}
void log(const std::string& text) {
    std::scoped_lock lock(log_mutex);
    std::ofstream stream(directory / "OriPadMod.log", std::ios::app);
    SYSTEMTIME now{}; GetLocalTime(&now);
    stream << now.wHour << ':' << now.wMinute << ':' << now.wSecond << " " << text << '\n';
}
std::filesystem::path data_dir() { return directory; }
Config config() { std::scoped_lock lock(config_mutex); return current; }
void set_config(const Config& cfg) { std::scoped_lock lock(config_mutex); current = cfg; }
bool save_config() {
    const auto cfg = config();
    const auto file = directory / "bindings.ini", temp = directory / "bindings.ini.tmp";
    std::ofstream out(temp, std::ios::trunc);
    out << "# OriPadMod: original game button = physical controller button\n";
    out << "Enabled=" << (cfg.enabled ? "True" : "False") << "\nTriggerThreshold=" << cfg.threshold << '\n';
    for (int i = 0; i < control_count; ++i) out << names[i] << '=' << (cfg.source[i] < 0 ? "None" : names[cfg.source[i]]) << '\n';
    out.close();
    if (!out) return false;
    return MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}
bool reload_config() {
    const auto file = directory / "bindings.ini";
    if (!std::filesystem::exists(file)) return save_config();
    std::ifstream in(file);
    if (!in) return false;
    Config candidate;
    std::array<bool, control_count> seen{};
    std::string line;
    const auto trim = [](std::string s) {
        auto first = s.find_first_not_of(" \t\r\n"), last = s.find_last_not_of(" \t\r\n");
        return first == std::string::npos ? std::string{} : s.substr(first, last - first + 1);
    };
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) { log("Config rejected: missing '='"); return false; }
        auto key = trim(line.substr(0, pos)), value = trim(line.substr(pos + 1));
        if (key == "Enabled") {
            if (value != "True" && value != "False") return false;
            candidate.enabled = value == "True";
        } else if (key == "TriggerThreshold") {
            try { size_t used{}; candidate.threshold = std::stoi(value, &used); if (used != value.size()) return false; }
            catch (...) { return false; }
            if (candidate.threshold < 1 || candidate.threshold > 254) return false;
        } else {
            int target = named(key), source = named(value);
            if (target < 0 || source < -1 || seen[target]) { log("Config rejected: invalid or duplicate control"); return false; }
            seen[target] = true;
            candidate.source[target] = source;
        }
    }
    if (in.bad()) return false;
    set_config(candidate);
    log("Config loaded");
    return true;
}
bool read_raw(int index, XINPUT_STATE& state) {
    if (!physical || index < 0 || index >= 4) return false;
    const bool was_inside = inside;
    inside = true;
    const auto result = physical(static_cast<DWORD>(index), &state);
    inside = was_inside;
    return result == ERROR_SUCCESS;
}
void start(HMODULE self) {
    const HANDLE thread = CreateThread(nullptr, 0, worker, self, 0, nullptr);
    if (thread) CloseHandle(thread);
}
}
