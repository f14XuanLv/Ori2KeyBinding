#include "mapping.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

void require(bool ok, const char* description) {
    if (!ok) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
int main() {
    ori::Config cfg;
    // All combinations of digital buttons; axes and low trigger values must stay bit-exact at defaults.
    for (unsigned b = 0; b < 65536; ++b) {
        XINPUT_GAMEPAD raw{static_cast<WORD>(b), static_cast<BYTE>(b % 256), static_cast<BYTE>((b * 7) % 256), -32768, 32767, -1234, 5678};
        auto mapped = ori::map_pad(raw, cfg);
        require(std::memcmp(&raw, &mapped, sizeof(raw)) == 0, "default mapping must be exact identity");
    }
    ori::assign(cfg, 0, 2, true); // X -> A; A -> X.
    XINPUT_GAMEPAD raw{}; raw.wButtons = XINPUT_GAMEPAD_X;
    auto mapped = ori::map_pad(raw, cfg);
    require(mapped.wButtons == XINPUT_GAMEPAD_A, "swap X to A without duplicate X output");
    raw.wButtons = XINPUT_GAMEPAD_A;
    require(ori::map_pad(raw, cfg).wButtons == XINPUT_GAMEPAD_X, "swap retains opposite mapping");
    cfg = {}; ori::assign(cfg, 0, 6, true);
    raw = {}; raw.bLeftTrigger = 30;
    require(ori::map_pad(raw, cfg).wButtons == 0, "threshold boundary does not press A");
    raw.bLeftTrigger = 31;
    mapped = ori::map_pad(raw, cfg);
    require(mapped.wButtons == XINPUT_GAMEPAD_A && mapped.bLeftTrigger == 0, "LT -> A removes old LT output");
    raw = {}; raw.wButtons = XINPUT_GAMEPAD_A;
    mapped = ori::map_pad(raw, cfg);
    require(mapped.wButtons == 0 && mapped.bLeftTrigger == 255, "A -> LT produces full pull");
    cfg = {}; ori::assign(cfg, 6, 7, true);
    raw = {}; raw.bLeftTrigger = 7; raw.bRightTrigger = 113;
    mapped = ori::map_pad(raw, cfg);
    require(mapped.bLeftTrigger == 113 && mapped.bRightTrigger == 7, "trigger swap preserves analog precision");
    cfg.source.fill(-1); raw.wButtons = 0xFFFF; raw.sThumbLX = -20000;
    mapped = ori::map_pad(raw, cfg);
    require(mapped.wButtons == 0x0C00 && mapped.bLeftTrigger == 0 && mapped.bRightTrigger == 0 && mapped.sThumbLX == -20000, "unbind removes buttons while preserving axes");
    cfg.enabled = false;
    mapped = ori::map_pad(raw, cfg);
    require(std::memcmp(&raw, &mapped, sizeof(raw)) == 0, "disabled mapping passes through");
    cfg = {}; ori::assign(cfg, 0, 2, false);
    raw = {}; raw.wButtons = XINPUT_GAMEPAD_X;
    mapped = ori::map_pad(raw, cfg);
    require(mapped.wButtons == (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X), "explicit duplicate binding keeps both outputs");
    std::cout << "PASS: 65,536 identity states, swaps, trigger boundaries, analog range, unbind and bypass\n";
}
