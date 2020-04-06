// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include <SDL.h>
#include "common/common_types.h"
#include "core/hle/service/cam/cam.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"

namespace Settings {

enum class InitClock {
    SystemTime = 0,
    FixedTime = 1,
};

enum class LayoutOption {
    Default,
    SingleScreen,
    LargeScreen,
    SideScreen,
    MediumScreen,
};

enum class MicInputType {
    None,
    Real,
    Static,
};

enum class StereoRenderOption {
    Off,
    SideBySide,
    Anaglyph,
    Interlaced,
};

namespace NativeButton {
enum Values {
    A,
    B,
    X,
    Y,
    Up,
    Down,
    Left,
    Right,
    L,
    R,
    Start,
    Select,
    Debug,
    Gpio14,

    ZL,
    ZR,

    Home,

    NumButtons,
};

constexpr int BUTTON_HID_BEGIN = A;
constexpr int BUTTON_IR_BEGIN = ZL;
constexpr int BUTTON_NS_BEGIN = Home;

constexpr int BUTTON_HID_END = BUTTON_IR_BEGIN;
constexpr int BUTTON_IR_END = BUTTON_NS_BEGIN;
constexpr int BUTTON_NS_END = NumButtons;

constexpr int NUM_BUTTONS_HID = BUTTON_HID_END - BUTTON_HID_BEGIN;
constexpr int NUM_BUTTONS_IR = BUTTON_IR_END - BUTTON_IR_BEGIN;
constexpr int NUM_BUTTONS_NS = BUTTON_NS_END - BUTTON_NS_BEGIN;

static const std::array<const char*, NumButtons> mapping = {{
    "button_a",
    "button_b",
    "button_x",
    "button_y",
    "button_up",
    "button_down",
    "button_left",
    "button_right",
    "button_l",
    "button_r",
    "button_start",
    "button_select",
    "button_debug",
    "button_gpio14",
    "button_zl",
    "button_zr",
    "button_home",
}};
} // namespace NativeButton

namespace NativeAnalog {

enum Values {
    CirclePad,
    CirclePadPro,

    NumAnalogs,
};

static const std::array<const char*, NumAnalogs> mapping = {{
    "circle_pad",
    "circle_pad_pro",
}};

} // namespace NativeAnalog

// A special value for Values::region_value indicating that vvctre will automatically select a
// region value to fit the region lockout info of the game
static constexpr int REGION_VALUE_AUTO_SELECT = -1;

struct Values {
    // Controls
    std::array<std::string, NativeButton::NumButtons> buttons = {
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_A), // A, JSON buttons[0]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_S), // B, JSON buttons[1]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_Z), // X, JSON buttons[2]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_X), // Y, JSON buttons[3]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_T), // Up, JSON buttons[4]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_G), // Down, JSON buttons[5]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_F), // Left, JSON buttons[6]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_H), // Right, JSON buttons[7]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_Q), // L, JSON buttons[8]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_W), // R, JSON buttons[9]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_M), // Start, JSON buttons[10]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_N), // Select, JSON buttons[11]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_0), // Debug, JSON buttons[12]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_P), // GPIO14, JSON buttons[13]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_1), // ZL, JSON buttons[14]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_2), // ZR, JSON buttons[15]
        InputCommon::GenerateKeyboardParam(SDL_SCANCODE_B), // Home, JSON buttons[16]
    };

    std::array<std::string, NativeAnalog::NumAnalogs> analogs = {
        // Up, Down, Left, Right, Modifier Key, Modifier
        // Circle Pad, JSON analogs[0]
        InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                                                 SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                                 SDL_SCANCODE_D, 0.5f),
        // Circle Pad Pro, JSON analogs[1]
        InputCommon::GenerateAnalogParamFromKeys(SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J,
                                                 SDL_SCANCODE_L, SDL_SCANCODE_D, 0.5f),
    };

    /// JSON motion_device
    std::string motion_device =
        "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0";

    /// JSON touch_device
    std::string touch_device = "engine:emu_window";

    // JSON udp_input_address
    std::string udp_input_address = InputCommon::CemuhookUDP::DEFAULT_ADDR;

    /// JSON udp_input_port
    u16 udp_input_port = InputCommon::CemuhookUDP::DEFAULT_PORT;

    /// JSON udp_pad_index
    u8 udp_pad_index = 0;

    // Core
    bool use_cpu_jit = true;
    std::string multiplayer_url = "ws://vvctre-multiplayer.glitch.me";
    bool use_custom_cpu_ticks = false;
    u64 custom_cpu_ticks = 77;
    u32 cpu_clock_percentage = 100;

    // Data Storage
    bool use_virtual_sd = true;

    // System

    /**
     * Valid values:
     *  -1: Auto-select
     *   0: Japan
     *   1: USA
     *   2: Europe
     *   3: Australia
     *   4: China
     *   5: Korea
     *   6: Taiwan
     */
    int region_value = REGION_VALUE_AUTO_SELECT;

    /**
     * Valid values:
     * - InitClock::SystemTime
     * - InitClock::FixedTime
     */
    InitClock init_clock = InitClock::SystemTime;

    u64 init_time = 0; // Unix timestamp

    // Renderer
    bool use_hw_renderer = true;
    bool use_hw_shader = true;
    bool shaders_accurate_mul = false;
    bool use_shader_jit = true;
    u16 resolution_factor = 1;
    bool use_frame_limit = true;
    u16 frame_limit = 100;
    std::string texture_filter_name = "none";
    float bg_red = 0.0f;
    float bg_green = 0.0f;
    float bg_blue = 0.0f;

    /**
     * Valid values:
     *  - StereoRenderOption::Off
     *  - StereoRenderOption::SideBySide
     *  - StereoRenderOption::Anaglyph
     *  - StereoRenderOption::Interlaced
     */
    StereoRenderOption render_3d = StereoRenderOption::Off;

    std::atomic<u8> factor_3d{0};
    bool filter_mode = true;
    std::string pp_shader_name = "none (builtin)";
    bool enable_vsync = false;
    bool sharper_distant_objects = false;

    // Layout

    /**
     * Valid values:
     *  - LayoutOption::Default
     *  - LayoutOption::SingleScreen
     *  - LayoutOption::LargeScreen
     *  - LayoutOption::SideScreen
     *  - LayoutOption::MediumScreen
     */
    LayoutOption layout_option = LayoutOption::Default;

    bool swap_screen = false;
    bool upright_screen = false;
    bool custom_layout = false;
    u16 custom_top_left = 0;
    u16 custom_top_top = 0;
    u16 custom_top_right = 400;
    u16 custom_top_bottom = 240;
    u16 custom_bottom_left = 40;
    u16 custom_bottom_top = 240;
    u16 custom_bottom_right = 360;
    u16 custom_bottom_bottom = 480;

    // Utility
    bool dump_textures = false;
    bool custom_textures = false;
    bool preload_textures = false;

    // Audio
    bool enable_dsp_lle = false;
    bool enable_dsp_lle_multithread = false;
    std::string sink_id = "auto";
    std::string audio_device_id = "auto";
    float volume = 1.0f;

    /**
     * Valid values:
     *  - MicInputType::None
     *  - MicInputType::Real
     *  - MicInputType::Static
     */
    MicInputType mic_input_type = MicInputType::None;

    std::string mic_input_device;
    float audio_speed = 1.0f;

    // Camera
    std::array<std::string, Service::CAM::NumCameras> camera_name{
        "blank",
        "blank",
        "blank",
    };
    std::array<std::string, Service::CAM::NumCameras> camera_config;
    std::array<int, Service::CAM::NumCameras> camera_flip;

    // Miscellaneous
    std::string log_filter = "*:Info";

    // Debugging
    bool record_frame_times = false;
    bool use_gdbstub = false;
    u16 gdbstub_port = 24689;
    std::unordered_map<std::string, bool> lle_modules = {
        {"FS", false},
        {"PM", false},
        {"LDR", false},
        {"PXI", false},
        {"ERR", false},
        {"AC", false},
        {"ACT", false},
        {"AM", false},
        {"BOSS", false},
        {"CAM", false},
        {"CECD", false},
        {"CFG", false},
        {"DLP", false},
        {"DSP", false},
        {"FRD", false},
        {"GSP", false},
        {"HID", false},
        {"IR", false},
        {"MIC", false},
        {"MVD", false},
        {"NDM", false},
        {"NEWS", false},
        {"NFC", false},
        {"NIM", false},
        {"NS", false},
        {"NWM", false},
        {"PTM", false},
        {"QTM", false},
        {"CSND", false},
        {"HTTP", false},
        {"SOC", false},
        {"SSL", false},
        {"PS", false},
        {"MCU", false},
        // No HLE implementation
        {"CDC", false},
        {"GPIO", false},
        {"I2C", false},
        {"MP", false},
        {"PDN", false},
        {"SPI", false},
    };

    // Used by the frontend
    std::string file_path;
    bool start_in_fullscreen_mode = false;
    int rpc_server_port = 47889;
    std::string play_movie;
    std::string record_movie;
} extern values;

void Apply();
void LogSettings();

} // namespace Settings
