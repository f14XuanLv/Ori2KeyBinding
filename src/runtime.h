#pragma once
#include "mapping.h"
#include <atomic>
#include <filesystem>
#include <string>

namespace ori {
extern std::atomic<bool> panel_open;
extern std::atomic<bool> overlay_ready;
extern std::atomic<unsigned long long> input_calls;
extern std::atomic<unsigned long long> present_calls;
void log(const std::string& text);
Config config();
void set_config(const Config& cfg);
bool save_config();
bool reload_config();
std::filesystem::path data_dir();
bool read_raw(int index, XINPUT_STATE& state);
void start(HMODULE self);
bool install_overlay();
}
