#pragma once
#include <Windows.h>
#include <Xinput.h>
#include <array>
#include <algorithm>
#include <string_view>

namespace ori {
inline constexpr int control_count = 16;
inline constexpr std::array<const char*, control_count> names{
    "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "L3", "R3", "Back", "Start",
    "DPadUp", "DPadDown", "DPadLeft", "DPadRight"};
inline constexpr std::array<WORD, control_count> bits{
    0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0, 0, 0x0040, 0x0080, 0x0020, 0x0010,
    0x0001, 0x0002, 0x0004, 0x0008};
struct Config {
    bool enabled = true;
    int threshold = 30;
    std::array<int, control_count> source{}; // Original game button -> physical control; -1 = unbound.
    Config() { for (int i = 0; i < control_count; ++i) source[i] = i; }
};
inline int named(std::string_view name) {
    if (name == "None") return -1;
    for (int i = 0; i < control_count; ++i) if (name == names[i]) return i;
    return -2;
}
inline int magnitude(const XINPUT_GAMEPAD& p, int control, int threshold) {
    if (control < 0 || control >= control_count) return 0;
    if (control == 6) return p.bLeftTrigger > threshold ? p.bLeftTrigger : 0;
    if (control == 7) return p.bRightTrigger > threshold ? p.bRightTrigger : 0;
    return (p.wButtons & bits[control]) ? 255 : 0;
}
inline XINPUT_GAMEPAD map_pad(const XINPUT_GAMEPAD& raw, const Config& cfg) {
    if (!cfg.enabled) return raw;
    XINPUT_GAMEPAD out = raw;
    out.wButtons = raw.wButtons & 0x0C00; // Preserve reserved bits; remap all documented buttons.
    out.bLeftTrigger = out.bRightTrigger = 0;
    for (int target = 0; target < control_count; ++target) {
        const int source = cfg.source[target];
        if (target == 6 || target == 7) {
            // Identity trigger mappings preserve the entire analog range, including below threshold.
            int value = source == 6 ? raw.bLeftTrigger : source == 7 ? raw.bRightTrigger : magnitude(raw, source, cfg.threshold);
            if (target == 6) out.bLeftTrigger = static_cast<BYTE>(value);
            else out.bRightTrigger = static_cast<BYTE>(value);
        } else if (magnitude(raw, source, cfg.threshold)) out.wButtons |= bits[target];
    }
    return out;
}
inline void assign(Config& cfg, int target, int source, bool swap) {
    if (target < 0 || target >= control_count || source < -1 || source >= control_count) return;
    const int old = cfg.source[target];
    if (swap && source >= 0) for (int i = 0; i < control_count; ++i)
        if (i != target && cfg.source[i] == source) cfg.source[i] = old;
    cfg.source[target] = source;
}
}
