// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <fmt/format.h>
#include <imgui.h>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/memory.h"
#include "core/movie.h"
#include "vvctre/common.h"
#include "vvctre/plugins.h"

#ifdef _WIN32
#include <windows.h>
#endif

bool has_suffix(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

PluginManager::PluginManager(void* core) {
#ifdef _WIN32
    FileUtil::FSTEntry parent;
    FileUtil::ScanDirectoryTree(FileUtil::GetExeDirectory(), parent);
    for (const auto& entry : parent.children) {
        if (!entry.isDirectory && entry.virtualName != "SDL2.dll" &&
            has_suffix(entry.virtualName, fmt::format("{}.dll", version_major))) {
            HMODULE handle = LoadLibraryA(entry.virtualName.c_str());
            if (handle == NULL) {
                fmt::print("Plugin {} failed to load: {}\n", entry.virtualName, GetLastErrorMsg());
            } else {
                PluginImportedFunctions::PluginLoaded f =
                    (PluginImportedFunctions::PluginLoaded)GetProcAddress(handle, "PluginLoaded");
                if (f == nullptr) {
                    fmt::print("Plugin {} failed to load: PluginLoaded "
                               "function not found\n",
                               entry.virtualName);
                } else {
                    Plugin plugin;
                    plugin.handle = handle;
                    plugin.before_drawing_fps =
                        (PluginImportedFunctions::BeforeDrawingFPS)GetProcAddress(
                            handle, "BeforeDrawingFPS");
                    plugin.add_menu =
                        (PluginImportedFunctions::AddMenu)GetProcAddress(handle, "AddMenu");
                    plugin.after_swap_window =
                        (PluginImportedFunctions::AfterSwapWindow)GetProcAddress(handle,
                                                                                 "AfterSwapWindow");
                    PluginImportedFunctions::Log log =
                        (PluginImportedFunctions::Log)GetProcAddress(handle, "Log");
                    if (log != nullptr) {
                        Log::AddBackend(std::make_unique<Log::FunctionLogger>(
                            log, fmt::format("Plugin {}", entry.virtualName)));
                    }
                    plugins.push_back(std::move(plugin));

                    f(core, static_cast<void*>(this));

                    fmt::print("Plugin {} loaded\n", entry.virtualName);
                }
            }
        }
    }
#endif
}

PluginManager::~PluginManager() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulatorClosing f =
            (PluginImportedFunctions::EmulatorClosing)GetProcAddress(plugin.handle,
                                                                     "EmulatorClosing");
        if (f != nullptr) {
            f();
        }
        FreeLibrary(plugin.handle);
    }
#endif
}

void PluginManager::InitialSettingsOpening() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::InitialSettingsOpening f =
            (PluginImportedFunctions::InitialSettingsOpening)GetProcAddress(
                plugin.handle, "InitialSettingsOpening");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::InitialSettingsOkPressed() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::InitialSettingsOkPressed f =
            (PluginImportedFunctions::InitialSettingsOkPressed)GetProcAddress(
                plugin.handle, "InitialSettingsOkPressed");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::BeforeLoading() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::BeforeLoading f =
            (PluginImportedFunctions::BeforeLoading)GetProcAddress(plugin.handle, "BeforeLoading");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::EmulationStarting() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulationStarting f =
            (PluginImportedFunctions::EmulationStarting)GetProcAddress(plugin.handle,
                                                                       "EmulationStarting");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::EmulatorClosing() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulatorClosing f =
            (PluginImportedFunctions::EmulatorClosing)GetProcAddress(plugin.handle,
                                                                     "EmulatorClosing");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::FatalError() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::FatalError f =
            (PluginImportedFunctions::FatalError)GetProcAddress(plugin.handle, "FatalError");
        if (f != nullptr) {
            f();
        }
    }
#endif
}

void PluginManager::BeforeDrawingFPS() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        if (plugin.before_drawing_fps != nullptr) {
            plugin.before_drawing_fps();
        }
    }
#endif
}

void PluginManager::AddMenus() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        if (plugin.add_menu != nullptr) {
            plugin.add_menu();
        }
    }
#endif
}

void PluginManager::AfterSwapWindow() {
#ifdef _WIN32
    for (const auto& plugin : plugins) {
        if (plugin.after_swap_window != nullptr) {
            plugin.after_swap_window();
        }
    }
#endif
}

void* PluginManager::NewButtonDevice(const char* params) {
    auto& b = buttons.emplace_back(Input::CreateDevice<Input::ButtonDevice>(std::string(params)));
    return static_cast<void*>(b.get());
}

void PluginManager::DeleteButtonDevice(void* device) {
    auto itr = std::find_if(std::begin(buttons), std::end(buttons),
                            [device](auto& b) { return b.get() == device; });
    if (itr != buttons.end()) {
        buttons.erase(itr);
    }
}

// Exports

#ifdef _WIN32
#define VVCTRE_PLUGIN_FUNCTION extern "C" __declspec(dllexport)
#else
#define VVCTRE_PLUGIN_FUNCTION
#endif

#include "vvctre/plugin_functions.h"

// File, Emulation
VVCTRE_PLUGIN_FUNCTION void vvctre_load_file(void* core, const char* path) {
    static_cast<Core::System*>(core)->SetResetFilePath(std::string(path));
    static_cast<Core::System*>(core)->RequestReset();
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_install_cia(const char* path) {
    return Service::AM::InstallCIA(std::string(path)) == Service::AM::InstallStatus::Success;
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_load_amiibo(void* core, const char* path) {
    FileUtil::IOFile file(std::string(path), "rb");
    Service::NFC::AmiiboData data;

    if (file.ReadArray(&data, 1) == 1) {
        std::shared_ptr<Service::NFC::Module::Interface> nfc =
            static_cast<Core::System*>(core)
                ->ServiceManager()
                .GetService<Service::NFC::Module::Interface>("nfc:u");
        if (nfc != nullptr) {
            nfc->LoadAmiibo(data);
            return true;
        }
    }

    return false;
}

VVCTRE_PLUGIN_FUNCTION void vvctre_remove_amiibo(void* core) {
    std::shared_ptr<Service::NFC::Module::Interface> nfc =
        static_cast<Core::System*>(core)
            ->ServiceManager()
            .GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc != nullptr) {
        nfc->RemoveAmiibo();
    }
}

VVCTRE_PLUGIN_FUNCTION void vvctre_restart(void* core) {
    static_cast<Core::System*>(core)->Reset();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_paused(void* plugin_manager, bool paused) {
    static_cast<PluginManager*>(plugin_manager)->paused = paused;
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_get_paused(void* plugin_manager) {
    return static_cast<PluginManager*>(plugin_manager)->paused;
}

// Memory
VVCTRE_PLUGIN_FUNCTION u8 vvctre_read_u8(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read8(address);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_write_u8(void* core, VAddr address, u8 value) {
    static_cast<Core::System*>(core)->Memory().Write8(address, value);
}

VVCTRE_PLUGIN_FUNCTION u16 vvctre_read_u16(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read16(address);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_write_u16(void* core, VAddr address, u16 value) {
    static_cast<Core::System*>(core)->Memory().Write16(address, value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_read_u32(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read32(address);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_write_u32(void* core, VAddr address, u32 value) {
    static_cast<Core::System*>(core)->Memory().Write32(address, value);
}

VVCTRE_PLUGIN_FUNCTION u64 vvctre_read_u64(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read64(address);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_write_u64(void* core, VAddr address, u64 value) {
    static_cast<Core::System*>(core)->Memory().Write64(address, value);
}

// Debugging
VVCTRE_PLUGIN_FUNCTION void vvctre_set_pc(void* core, u32 addr) {
    static_cast<Core::System*>(core)->CPU().SetPC(addr);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_pc(void* core) {
    return static_cast<Core::System*>(core)->CPU().GetPC();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetReg(index, value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetReg(index);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_vfp_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetVFPReg(index, value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_vfp_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetVFPReg(index);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_vfp_system_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetVFPSystemReg(static_cast<VFPSystemRegister>(index),
                                                            value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_vfp_system_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetVFPSystemReg(
        static_cast<VFPSystemRegister>(index));
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_cp15_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetCP15Register(static_cast<CP15Register>(index),
                                                            value);
}

VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_cp15_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetCP15Register(
        static_cast<CP15Register>(index));
}

// Cheats
VVCTRE_PLUGIN_FUNCTION int vvctre_cheat_count(void* core) {
    return static_cast<int>(static_cast<Core::System*>(core)->CheatEngine().GetCheats().size());
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->ToString().c_str();
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_name(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetName().c_str();
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_comments(void* core, int index) {
    return static_cast<Core::System*>(core)
        ->CheatEngine()
        .GetCheats()[index]
        ->GetComments()
        .c_str();
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_type(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetType().c_str();
}

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_code(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetCode().c_str();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_cheat_enabled(void* core, int index, bool enabled) {
    static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->SetEnabled(enabled);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_add_gateway_cheat(void* core, const char* name, const char* code,
                                                     const char* comments) {
    static_cast<Core::System*>(core)->CheatEngine().AddCheat(std::make_shared<Cheats::GatewayCheat>(
        std::string(name), std::string(code), std::string(comments)));
}

VVCTRE_PLUGIN_FUNCTION void vvctre_remove_cheat(void* core, int index) {
    static_cast<Core::System*>(core)->CheatEngine().RemoveCheat(index);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_update_gateway_cheat(void* core, int index, const char* name,
                                                        const char* code, const char* comments) {
    static_cast<Core::System*>(core)->CheatEngine().UpdateCheat(
        index, std::make_shared<Cheats::GatewayCheat>(std::string(name), std::string(code),
                                                      std::string(comments)));
}

// Camera
VVCTRE_PLUGIN_FUNCTION void vvctre_reload_camera_images(void* core) {
    auto cam = Service::CAM::GetModule(*static_cast<Core::System*>(core));
    if (cam != nullptr) {
        cam->ReloadCameraDevices();
    }
}

// GUI
VVCTRE_PLUGIN_FUNCTION void vvctre_gui_text(const char* text) {
    ImGui::Text(text);
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_button(const char* text) {
    return ImGui::Button(text);
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_begin(const char* name) {
    return ImGui::Begin(name);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_gui_end() {
    ImGui::End();
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_begin_menu(const char* name) {
    return ImGui::BeginMenu(name);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_gui_end_menu() {
    ImGui::EndMenu();
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_menu_item(const char* name) {
    return ImGui::MenuItem(name);
}

// Button devices
VVCTRE_PLUGIN_FUNCTION void* vvctre_button_device_new(void* plugin_manager, const char* params) {
    PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
    return pm->NewButtonDevice(params);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_button_device_delete(void* plugin_manager, void* device) {
    PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
    pm->DeleteButtonDevice(device);
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_button_device_get_state(void* device) {
    return static_cast<Input::ButtonDevice*>(device)->GetStatus();
}

// TAS
VVCTRE_PLUGIN_FUNCTION void vvctre_movie_prepare_for_playback(const char* path) {
    Core::Movie::GetInstance().PrepareForPlayback(path);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_movie_prepare_for_recording() {
    Core::Movie::GetInstance().PrepareForRecording();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_movie_play(const char* path) {
    Core::Movie::GetInstance().StartPlayback(std::string(path));
}

VVCTRE_PLUGIN_FUNCTION void vvctre_movie_record(const char* path) {
    Core::Movie::GetInstance().StartRecording(std::string(path));
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_movie_is_playing() {
    return Core::Movie::GetInstance().IsPlayingInput();
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_movie_is_recording() {
    return Core::Movie::GetInstance().IsRecordingInput();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_movie_stop() {
    Core::Movie::GetInstance().Shutdown();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_frame_advancing_enabled(void* core, bool enabled) {
    static_cast<Core::System*>(core)->frame_limiter.SetFrameAdvancing(enabled);
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_get_frame_advancing_enabled(void* core) {
    return static_cast<Core::System*>(core)->frame_limiter.FrameAdvancingEnabled();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_advance_frame(void* core) {
    static_cast<Core::System*>(core)->frame_limiter.AdvanceFrame();
}

// Remote control
VVCTRE_PLUGIN_FUNCTION void vvctre_set_custom_pad_state(void* core, u32 state) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(Service::HID::PadState{state});
}

VVCTRE_PLUGIN_FUNCTION void vvctre_use_real_pad_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(std::nullopt);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_custom_circle_pad_state(void* core, float x, float y) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomCirclePadState(std::make_tuple(x, y));
}

VVCTRE_PLUGIN_FUNCTION void vvctre_use_real_circle_pad_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomCirclePadState(std::nullopt);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_custom_touch_state(void* core, float x, float y,
                                                          bool pressed) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomTouchState(std::make_tuple(x, y, pressed));
}

VVCTRE_PLUGIN_FUNCTION void vvctre_use_real_touch_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomTouchState(std::nullopt);
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_custom_motion_state(void* core, float accelerometer[3],
                                                           float gyroscope[3]) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomMotionState(
        std::make_tuple(Common::Vec3f(accelerometer[0], accelerometer[1], accelerometer[2]),
                        Common::Vec3f(gyroscope[0], gyroscope[1], gyroscope[2])));
}

VVCTRE_PLUGIN_FUNCTION void vvctre_use_real_motion_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(std::nullopt);
}

// Start Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_file_path(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_file_path();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_play_movie(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_play_movie();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_record_movie(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_record_movie();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_region_value(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_region_value();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_log_filter(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_log_filter();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_multiplayer_url(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_multiplayer_url();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_use_system_time();
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_use_unix_timestamp(u64 timestamp);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_clock();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_virtual_sd(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_virtual_sd();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_start_in_fullscreen_mode(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_start_in_fullscreen_mode();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_record_frame_times(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_record_frame_times();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_enable_gdbstub(u16 port);
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_disable_gdbstub();

VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_is_gdb_stub_enabled();
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_gdb_stub_port();

// General Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_cpu_jit(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_cpu_jit();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_speed_limit(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_speed_limit();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_speed_limit(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_speed_limit();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_custom_cpu_ticks(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_custom_cpu_ticks();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_cpu_ticks(u64 value);
VVCTRE_PLUGIN_FUNCTION u64 vvctre_settings_get_custom_cpu_ticks();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_cpu_clock_percentage(u32 value);
VVCTRE_PLUGIN_FUNCTION u32 vvctre_settings_get_cpu_clock_percentage();

// Audio Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_enable_dsp_lle(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_enable_dsp_lle();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_enable_dsp_lle_multithread(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_enable_dsp_lle_multithread();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_audio_volume(float value);
VVCTRE_PLUGIN_FUNCTION float vvctre_settings_get_audio_volume();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_audio_sink_id(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_audio_sink_id();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_audio_device_id(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_audio_device_id();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_microphone_input_type(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_microphone_input_type();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_microphone_input_device(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_microphone_input_device();

// Camera Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_camera_engine(int index, const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_camera_engine(int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_camera_parameter(int index, const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_camera_parameter(int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_camera_flip(int index, int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_camera_flip(int index);

// Graphics Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_hardware_renderer(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_hardware_renderer();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_hardware_shader(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_hardware_shader();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_hardware_shader_accurate_multiplication(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_hardware_shader_accurate_multiplication();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_shader_jit(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_shader_jit();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_enable_vsync(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_enable_vsync();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_dump_textures(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_dump_textures();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_textures(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_custom_textures();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_preload_textures(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_preload_textures();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_enable_linear_filtering(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_enable_linear_filtering();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_sharper_distant_objects(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_sharper_distant_objects();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_resolution(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_resolution();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_background_color_red(float value);
VVCTRE_PLUGIN_FUNCTION float vvctre_settings_get_background_color_red();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_background_color_green(float value);
VVCTRE_PLUGIN_FUNCTION float vvctre_settings_get_background_color_green();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_background_color_blue(float value);
VVCTRE_PLUGIN_FUNCTION float vvctre_settings_get_background_color_blue();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_post_processing_shader(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_post_processing_shader();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_texture_filter(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_texture_filter();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_render_3d(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_render_3d();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_factor_3d(u8 value);
VVCTRE_PLUGIN_FUNCTION u8 vvctre_settings_get_factor_3d();

// Controls Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_button(int index, const char* params);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_button(int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_analog(int index, const char* params);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_analog(int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_motion_device(const char* params);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_motion_device();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_touch_device(const char* params);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_touch_device();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_cemuhookudp_address(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_cemuhookudp_address();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_cemuhookudp_port(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_cemuhookudp_port();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_cemuhookudp_pad_index(u8 value);
VVCTRE_PLUGIN_FUNCTION u8 vvctre_settings_get_cemuhookudp_pad_index();

// Layout Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_layout(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_layout();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_swap_screens(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_swap_screens();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_upright_screens(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_upright_screens();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_custom_layout(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_custom_layout();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_top_left(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_top_left();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_top_top(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_top_top();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_top_right(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_top_right();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_top_bottom(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_top_bottom();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_bottom_left(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_bottom_left();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_bottom_top(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_bottom_top();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_bottom_right(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_bottom_right();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_bottom_bottom(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_bottom_bottom();

// LLE Modules Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_lle_module(const char* name, bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_lle_module(const char* name);

// Other
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_version() {
    return fmt::format("{}.{}.{}", vvctre_version_major, vvctre_version_minor, vvctre_version_patch)
        .c_str();
}

VVCTRE_PLUGIN_FUNCTION bool vvctre_emulation_running(void* core) {
    return static_cast<Core::System*>(core)->IsPoweredOn();
}

VVCTRE_PLUGIN_FUNCTION void vvctre_set_play_coins(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_get_play_coins();
