// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>
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
#include "core/hle/service/ptm/ptm.h"
#include "core/memory.h"
#include "core/movie.h"
#include "core/settings.h"
#include "vvctre/common.h"
#include "vvctre/plugins.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#define GetProcAddress dlsym
#endif

bool has_suffix(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

PluginManager::PluginManager(void* core) {
    FileUtil::FSTEntry parent;
    FileUtil::ScanDirectoryTree(
#ifdef _WIN32
        FileUtil::GetExeDirectory()
#else
        "."
#endif
            ,
        parent);
    for (const auto& entry : parent.children) {
        if (!entry.isDirectory &&
#ifdef _WIN32
            entry.virtualName != "SDL2.dll" &&
            has_suffix(entry.virtualName, fmt::format("{}.dll", vvctre_version_major))
#else
            has_suffix(entry.virtualName, fmt::format("{}.so", vvctre_version_major))
#endif
        ) {
#ifdef _WIN32
            HMODULE handle = LoadLibraryA(entry.virtualName.c_str());
#else
            void* handle = dlopen(fmt::format("./{}", entry.virtualName).c_str(), RTLD_LAZY);
#endif
            if (handle == NULL) {
                fmt::print("Plugin {} failed to load: {}\n", entry.virtualName,
#ifdef _WIN32
                           GetLastErrorMsg()
#else
                           dlerror()
#endif
                );
            } else {
                PluginImportedFunctions::GetRequiredFunctionCount GetRequiredFunctionCount =
                    (PluginImportedFunctions::GetRequiredFunctionCount)GetProcAddress(
                        handle, "GetRequiredFunctionCount");
                PluginImportedFunctions::GetRequiredFunctionNames GetRequiredFunctionNames =
                    (PluginImportedFunctions::GetRequiredFunctionNames)GetProcAddress(
                        handle, "GetRequiredFunctionNames");
                PluginImportedFunctions::PluginLoaded PluginLoaded =
                    (PluginImportedFunctions::PluginLoaded)GetProcAddress(handle, "PluginLoaded");
                if (GetRequiredFunctionCount == nullptr || GetRequiredFunctionNames == nullptr ||
                    PluginLoaded == nullptr) {
                    fmt::print("Plugin {} failed to load: a required function is nullptr\n",
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

                    int count = GetRequiredFunctionCount();
                    const char** required_function_names = GetRequiredFunctionNames();
                    std::vector<void*> required_functions(count);
                    for (int i = 0; i < count; ++i) {
                        required_functions[i] =
                            function_map[std::string(required_function_names[i])];
                    }
                    PluginLoaded(core, static_cast<void*>(this), required_functions.data());

                    plugins.push_back(std::move(plugin));
                    fmt::print("Plugin {} loaded\n", entry.virtualName);
                }
            }
        }
    }
}

PluginManager::~PluginManager() {
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulatorClosing f =
            (PluginImportedFunctions::EmulatorClosing)GetProcAddress(plugin.handle,
                                                                     "EmulatorClosing");
        if (f != nullptr) {
            f();
        }
#ifdef _WIN32
        FreeLibrary(plugin.handle);
#else
        dlclose(plugin.handle);
#endif
    }
}

void PluginManager::InitialSettingsOpening() {
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::InitialSettingsOpening f =
            (PluginImportedFunctions::InitialSettingsOpening)GetProcAddress(
                plugin.handle, "InitialSettingsOpening");
        if (f != nullptr) {
            f();
        }
    }
}

void PluginManager::InitialSettingsOkPressed() {
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::InitialSettingsOkPressed f =
            (PluginImportedFunctions::InitialSettingsOkPressed)GetProcAddress(
                plugin.handle, "InitialSettingsOkPressed");
        if (f != nullptr) {
            f();
        }
    }
}

void PluginManager::BeforeLoading() {
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::BeforeLoading f =
            (PluginImportedFunctions::BeforeLoading)GetProcAddress(plugin.handle, "BeforeLoading");
        if (f != nullptr) {
            f();
        }
    }
}

void PluginManager::EmulationStarting() {
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulationStarting f =
            (PluginImportedFunctions::EmulationStarting)GetProcAddress(plugin.handle,
                                                                       "EmulationStarting");
        if (f != nullptr) {
            f();
        }
    }
}

void PluginManager::EmulatorClosing() {
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::EmulatorClosing f =
            (PluginImportedFunctions::EmulatorClosing)GetProcAddress(plugin.handle,
                                                                     "EmulatorClosing");
        if (f != nullptr) {
            f();
        }
    }
}

void PluginManager::FatalError() {
    for (const auto& plugin : plugins) {
        PluginImportedFunctions::FatalError f =
            (PluginImportedFunctions::FatalError)GetProcAddress(plugin.handle, "FatalError");
        if (f != nullptr) {
            f();
        }
    }
}

void PluginManager::BeforeDrawingFPS() {
    for (const auto& plugin : plugins) {
        if (plugin.before_drawing_fps != nullptr) {
            plugin.before_drawing_fps();
        }
    }
}

void PluginManager::AddMenus() {
    for (const auto& plugin : plugins) {
        if (plugin.add_menu != nullptr) {
            plugin.add_menu();
        }
    }
}

void PluginManager::AfterSwapWindow() {
    for (const auto& plugin : plugins) {
        if (plugin.after_swap_window != nullptr) {
            plugin.after_swap_window();
        }
    }
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

// Functions plugins can use

// File, Emulation
void vvctre_load_file(void* core, const char* path) {
    static_cast<Core::System*>(core)->SetResetFilePath(std::string(path));
    static_cast<Core::System*>(core)->RequestReset();
}

bool vvctre_install_cia(const char* path) {
    return Service::AM::InstallCIA(std::string(path)) == Service::AM::InstallStatus::Success;
}

bool vvctre_load_amiibo(void* core, const char* path) {
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

void vvctre_remove_amiibo(void* core) {
    std::shared_ptr<Service::NFC::Module::Interface> nfc =
        static_cast<Core::System*>(core)
            ->ServiceManager()
            .GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc != nullptr) {
        nfc->RemoveAmiibo();
    }
}

void vvctre_restart(void* core) {
    static_cast<Core::System*>(core)->Reset();
}

void vvctre_set_paused(void* plugin_manager, bool paused) {
    static_cast<PluginManager*>(plugin_manager)->paused = paused;
}

bool vvctre_get_paused(void* plugin_manager) {
    return static_cast<PluginManager*>(plugin_manager)->paused;
}

// Memory
u8 vvctre_read_u8(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read8(address);
}

void vvctre_write_u8(void* core, VAddr address, u8 value) {
    static_cast<Core::System*>(core)->Memory().Write8(address, value);
}

u16 vvctre_read_u16(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read16(address);
}

void vvctre_write_u16(void* core, VAddr address, u16 value) {
    static_cast<Core::System*>(core)->Memory().Write16(address, value);
}

u32 vvctre_read_u32(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read32(address);
}

void vvctre_write_u32(void* core, VAddr address, u32 value) {
    static_cast<Core::System*>(core)->Memory().Write32(address, value);
}

u64 vvctre_read_u64(void* core, VAddr address) {
    return static_cast<Core::System*>(core)->Memory().Read64(address);
}

void vvctre_write_u64(void* core, VAddr address, u64 value) {
    static_cast<Core::System*>(core)->Memory().Write64(address, value);
}

// Debugging
void vvctre_set_pc(void* core, u32 addr) {
    static_cast<Core::System*>(core)->CPU().SetPC(addr);
}

u32 vvctre_get_pc(void* core) {
    return static_cast<Core::System*>(core)->CPU().GetPC();
}

void vvctre_set_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetReg(index, value);
}

u32 vvctre_get_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetReg(index);
}

void vvctre_set_vfp_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetVFPReg(index, value);
}

u32 vvctre_get_vfp_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetVFPReg(index);
}

void vvctre_set_vfp_system_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetVFPSystemReg(static_cast<VFPSystemRegister>(index),
                                                            value);
}

u32 vvctre_get_vfp_system_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetVFPSystemReg(
        static_cast<VFPSystemRegister>(index));
}

void vvctre_set_cp15_register(void* core, int index, u32 value) {
    static_cast<Core::System*>(core)->CPU().SetCP15Register(static_cast<CP15Register>(index),
                                                            value);
}

u32 vvctre_get_cp15_register(void* core, int index) {
    return static_cast<Core::System*>(core)->CPU().GetCP15Register(
        static_cast<CP15Register>(index));
}

// Cheats
int vvctre_cheat_count(void* core) {
    return static_cast<int>(static_cast<Core::System*>(core)->CheatEngine().GetCheats().size());
}

const char* vvctre_get_cheat(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->ToString().c_str();
}

const char* vvctre_get_cheat_name(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetName().c_str();
}

const char* vvctre_get_cheat_comments(void* core, int index) {
    return static_cast<Core::System*>(core)
        ->CheatEngine()
        .GetCheats()[index]
        ->GetComments()
        .c_str();
}

const char* vvctre_get_cheat_type(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetType().c_str();
}

const char* vvctre_get_cheat_code(void* core, int index) {
    return static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->GetCode().c_str();
}

void vvctre_set_cheat_enabled(void* core, int index, bool enabled) {
    static_cast<Core::System*>(core)->CheatEngine().GetCheats()[index]->SetEnabled(enabled);
}

void vvctre_add_gateway_cheat(void* core, const char* name, const char* code,
                              const char* comments) {
    static_cast<Core::System*>(core)->CheatEngine().AddCheat(std::make_shared<Cheats::GatewayCheat>(
        std::string(name), std::string(code), std::string(comments)));
}

void vvctre_remove_cheat(void* core, int index) {
    static_cast<Core::System*>(core)->CheatEngine().RemoveCheat(index);
}

void vvctre_update_gateway_cheat(void* core, int index, const char* name, const char* code,
                                 const char* comments) {
    static_cast<Core::System*>(core)->CheatEngine().UpdateCheat(
        index, std::make_shared<Cheats::GatewayCheat>(std::string(name), std::string(code),
                                                      std::string(comments)));
}

// Camera
void vvctre_reload_camera_images(void* core) {
    auto cam = Service::CAM::GetModule(*static_cast<Core::System*>(core));
    if (cam != nullptr) {
        cam->ReloadCameraDevices();
    }
}

// GUI
void vvctre_gui_text(const char* text) {
    ImGui::Text("%s", text);
}

bool vvctre_gui_button(const char* text) {
    return ImGui::Button(text);
}

bool vvctre_gui_begin(const char* name) {
    return ImGui::Begin(name);
}

void vvctre_gui_end() {
    ImGui::End();
}

bool vvctre_gui_begin_menu(const char* name) {
    return ImGui::BeginMenu(name);
}

void vvctre_gui_end_menu() {
    ImGui::EndMenu();
}

bool vvctre_gui_menu_item(const char* name) {
    return ImGui::MenuItem(name);
}

// Button devices
void* vvctre_button_device_new(void* plugin_manager, const char* params) {
    PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
    return pm->NewButtonDevice(params);
}

void vvctre_button_device_delete(void* plugin_manager, void* device) {
    PluginManager* pm = static_cast<PluginManager*>(plugin_manager);
    pm->DeleteButtonDevice(device);
}

bool vvctre_button_device_get_state(void* device) {
    return static_cast<Input::ButtonDevice*>(device)->GetStatus();
}

// TAS
void vvctre_movie_prepare_for_playback(const char* path) {
    Core::Movie::GetInstance().PrepareForPlayback(path);
}

void vvctre_movie_prepare_for_recording() {
    Core::Movie::GetInstance().PrepareForRecording();
}

void vvctre_movie_play(const char* path) {
    Core::Movie::GetInstance().StartPlayback(std::string(path));
}

void vvctre_movie_record(const char* path) {
    Core::Movie::GetInstance().StartRecording(std::string(path));
}

bool vvctre_movie_is_playing() {
    return Core::Movie::GetInstance().IsPlayingInput();
}

bool vvctre_movie_is_recording() {
    return Core::Movie::GetInstance().IsRecordingInput();
}

void vvctre_movie_stop() {
    Core::Movie::GetInstance().Shutdown();
}

void vvctre_set_frame_advancing_enabled(void* core, bool enabled) {
    static_cast<Core::System*>(core)->frame_limiter.SetFrameAdvancing(enabled);
}

bool vvctre_get_frame_advancing_enabled(void* core) {
    return static_cast<Core::System*>(core)->frame_limiter.FrameAdvancingEnabled();
}

void vvctre_advance_frame(void* core) {
    static_cast<Core::System*>(core)->frame_limiter.AdvanceFrame();
}

// Remote control
void vvctre_set_custom_pad_state(void* core, u32 state) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(Service::HID::PadState{state});
}

void vvctre_use_real_pad_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(std::nullopt);
}

void vvctre_set_custom_circle_pad_state(void* core, float x, float y) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomCirclePadState(std::make_tuple(x, y));
}

void vvctre_use_real_circle_pad_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomCirclePadState(std::nullopt);
}

void vvctre_set_custom_touch_state(void* core, float x, float y, bool pressed) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomTouchState(std::make_tuple(x, y, pressed));
}

void vvctre_use_real_touch_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomTouchState(std::nullopt);
}

void vvctre_set_custom_motion_state(void* core, float accelerometer[3], float gyroscope[3]) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomMotionState(
        std::make_tuple(Common::Vec3f(accelerometer[0], accelerometer[1], accelerometer[2]),
                        Common::Vec3f(gyroscope[0], gyroscope[1], gyroscope[2])));
}

void vvctre_use_real_motion_state(void* core) {
    std::shared_ptr<Service::HID::Module> hid =
        Service::HID::GetModule(*static_cast<Core::System*>(core));

    hid->SetCustomPadState(std::nullopt);
}

// Settings
void vvctre_settings_apply() {
    Settings::Apply();
}

void vvctre_settings_log() {
    Settings::LogSettings();
}

// Start Settings
void vvctre_settings_set_file_path(const char* value) {
    Settings::values.file_path = std::string(value);
}

const char* vvctre_settings_get_file_path() {
    return Settings::values.file_path.c_str();
}

void vvctre_settings_set_play_movie(const char* value) {
    Settings::values.play_movie = std::string(value);
}

const char* vvctre_settings_get_play_movie() {
    return Settings::values.play_movie.c_str();
}

void vvctre_settings_set_record_movie(const char* value) {
    Settings::values.record_movie = std::string(value);
}

const char* vvctre_settings_get_record_movie() {
    return Settings::values.record_movie.c_str();
}

void vvctre_settings_set_region_value(int value) {
    Settings::values.region_value = value;
}

int vvctre_settings_get_region_value() {
    return Settings::values.region_value;
}

void vvctre_settings_set_log_filter(const char* value) {
    Settings::values.log_filter = std::string(value);
}

const char* vvctre_settings_get_log_filter() {
    return Settings::values.log_filter.c_str();
}

void vvctre_settings_set_multiplayer_url(const char* value) {
    Settings::values.multiplayer_url = std::string(value);
}

const char* vvctre_settings_get_multiplayer_url() {
    return Settings::values.multiplayer_url.c_str();
}

void vvctre_settings_set_initial_clock(int value) {
    Settings::values.initial_clock = static_cast<Settings::InitialClock>(value);
}

int vvctre_settings_get_initial_clock() {
    return static_cast<int>(Settings::values.initial_clock);
}

void vvctre_settings_set_unix_timestamp(u64 value) {
    Settings::values.unix_timestamp = value;
}

u64 vvctre_settings_get_unix_timestamp() {
    return Settings::values.unix_timestamp;
}

void vvctre_settings_set_use_virtual_sd(bool value) {
    Settings::values.use_virtual_sd = value;
}

bool vvctre_settings_get_use_virtual_sd() {
    return Settings::values.use_virtual_sd;
}

void vvctre_settings_set_start_in_fullscreen_mode(bool value) {
    Settings::values.start_in_fullscreen_mode = value;
}

bool vvctre_settings_get_start_in_fullscreen_mode() {
    return Settings::values.start_in_fullscreen_mode;
}

void vvctre_settings_set_record_frame_times(bool value) {
    Settings::values.record_frame_times = value;
}

bool vvctre_settings_get_record_frame_times() {
    return Settings::values.record_frame_times;
}

void vvctre_settings_enable_gdbstub(u16 port) {
    Settings::values.use_gdbstub = true;
    Settings::values.gdbstub_port = port;
}

void vvctre_settings_disable_gdbstub() {
    Settings::values.use_gdbstub = false;
}

bool vvctre_settings_is_gdb_stub_enabled() {
    return Settings::values.use_gdbstub;
}

u16 vvctre_settings_get_gdb_stub_port() {
    return Settings::values.use_gdbstub;
}

// General Settings
void vvctre_settings_set_use_cpu_jit(bool value) {
    Settings::values.use_cpu_jit = value;
}

bool vvctre_settings_get_use_cpu_jit() {
    return Settings::values.use_cpu_jit;
}

void vvctre_settings_set_limit_speed(bool value) {
    Settings::values.limit_speed = value;
}

bool vvctre_settings_get_limit_speed() {
    return Settings::values.limit_speed;
}

void vvctre_settings_set_speed_limit(u16 value) {
    Settings::values.speed_limit = value;
}

u16 vvctre_settings_get_speed_limit() {
    return Settings::values.speed_limit;
}

void vvctre_settings_set_use_custom_cpu_ticks(bool value) {
    Settings::values.use_custom_cpu_ticks = value;
}

bool vvctre_settings_get_use_custom_cpu_ticks() {
    return Settings::values.use_custom_cpu_ticks;
}

void vvctre_settings_set_custom_cpu_ticks(u64 value) {
    Settings::values.custom_cpu_ticks = value;
}

u64 vvctre_settings_get_custom_cpu_ticks() {
    return Settings::values.custom_cpu_ticks;
}

void vvctre_settings_set_cpu_clock_percentage(u32 value) {
    Settings::values.cpu_clock_percentage = value;
}

u32 vvctre_settings_get_cpu_clock_percentage() {
    return Settings::values.cpu_clock_percentage;
}

// Audio Settings
void vvctre_settings_set_enable_dsp_lle(bool value) {
    Settings::values.enable_dsp_lle = value;
}

bool vvctre_settings_get_enable_dsp_lle() {
    return Settings::values.enable_dsp_lle;
}

void vvctre_settings_set_enable_dsp_lle_multithread(bool value) {
    Settings::values.enable_dsp_lle_multithread = value;
}

bool vvctre_settings_get_enable_dsp_lle_multithread() {
    return Settings::values.enable_dsp_lle_multithread;
}

void vvctre_settings_set_audio_volume(float value) {
    Settings::values.audio_volume = value;
}

float vvctre_settings_get_audio_volume() {
    return Settings::values.audio_volume;
}

void vvctre_settings_set_audio_sink_id(const char* value) {
    Settings::values.audio_sink_id = std::string(value);
}

const char* vvctre_settings_get_audio_sink_id() {
    return Settings::values.audio_sink_id.c_str();
}

void vvctre_settings_set_audio_device_id(const char* value) {
    Settings::values.audio_device_id = std::string(value);
}

const char* vvctre_settings_get_audio_device_id() {
    return Settings::values.audio_device_id.c_str();
}

void vvctre_settings_set_microphone_input_type(int value) {
    Settings::values.microphone_input_type = static_cast<Settings::MicrophoneInputType>(value);
}

int vvctre_settings_get_microphone_input_type() {
    return static_cast<int>(Settings::values.microphone_input_type);
}

void vvctre_settings_set_microphone_device(const char* value) {
    Settings::values.microphone_device = std::string(value);
}

const char* vvctre_settings_get_microphone_device() {
    return Settings::values.microphone_device.c_str();
}

// Camera Settings
void vvctre_settings_set_camera_engine(int index, const char* value) {
    Settings::values.camera_engine[index] = std::string(value);
}

const char* vvctre_settings_get_camera_engine(int index) {
    return Settings::values.camera_engine[index].c_str();
}

void vvctre_settings_set_camera_parameter(int index, const char* value) {
    Settings::values.camera_parameter[index] = std::string(value);
}

const char* vvctre_settings_get_camera_parameter(int index) {
    return Settings::values.camera_parameter[index].c_str();
}

void vvctre_settings_set_camera_flip(int index, int value) {
    Settings::values.camera_flip[index] = static_cast<Service::CAM::Flip>(value);
}

int vvctre_settings_get_camera_flip(int index) {
    return static_cast<int>(Settings::values.camera_flip[index]);
}

// Graphics Settings
void vvctre_settings_set_use_hardware_renderer(bool value) {
    Settings::values.use_hardware_renderer = value;
}

bool vvctre_settings_get_use_hardware_renderer() {
    return Settings::values.use_hardware_renderer;
}

void vvctre_settings_set_use_hardware_shader(bool value) {
    Settings::values.use_hardware_shader = value;
}

bool vvctre_settings_get_use_hardware_shader() {
    return Settings::values.use_hardware_shader;
}

void vvctre_settings_set_hardware_shader_accurate_multiplication(bool value) {
    Settings::values.hardware_shader_accurate_multiplication = value;
}

bool vvctre_settings_get_hardware_shader_accurate_multiplication() {
    return Settings::values.hardware_shader_accurate_multiplication;
}

void vvctre_settings_set_use_shader_jit(bool value) {
    Settings::values.use_shader_jit = value;
}

bool vvctre_settings_get_use_shader_jit() {
    return Settings::values.use_shader_jit;
}

void vvctre_settings_set_enable_vsync(bool value) {
    Settings::values.enable_vsync = value;
}

bool vvctre_settings_get_enable_vsync() {
    return Settings::values.enable_vsync;
}

void vvctre_settings_set_dump_textures(bool value) {
    Settings::values.dump_textures = value;
}

bool vvctre_settings_get_dump_textures() {
    return Settings::values.dump_textures;
}

void vvctre_settings_set_custom_textures(bool value) {
    Settings::values.custom_textures = value;
}

bool vvctre_settings_get_custom_textures() {
    return Settings::values.custom_textures;
}

void vvctre_settings_set_preload_textures(bool value) {
    Settings::values.preload_textures = value;
}

bool vvctre_settings_get_preload_textures() {
    return Settings::values.preload_textures;
}

void vvctre_settings_set_enable_linear_filtering(bool value) {
    Settings::values.enable_linear_filtering = value;
}

bool vvctre_settings_get_enable_linear_filtering() {
    return Settings::values.enable_linear_filtering;
}

void vvctre_settings_set_sharper_distant_objects(bool value) {
    Settings::values.sharper_distant_objects = value;
}

bool vvctre_settings_get_sharper_distant_objects() {
    return Settings::values.sharper_distant_objects;
}

void vvctre_settings_set_resolution(u16 value) {
    Settings::values.resolution = value;
}

u16 vvctre_settings_get_resolution() {
    return Settings::values.resolution;
}

void vvctre_settings_set_background_color_red(float value) {
    Settings::values.background_color_red = value;
}

float vvctre_settings_get_background_color_red() {
    return Settings::values.background_color_red;
}

void vvctre_settings_set_background_color_green(float value) {
    Settings::values.background_color_green = value;
}

float vvctre_settings_get_background_color_green() {
    return Settings::values.background_color_green;
}

void vvctre_settings_set_background_color_blue(float value) {
    Settings::values.background_color_blue = value;
}

float vvctre_settings_get_background_color_blue() {
    return Settings::values.background_color_blue;
}

void vvctre_settings_set_post_processing_shader(const char* value) {
    Settings::values.post_processing_shader = std::string(value);
}

const char* vvctre_settings_get_post_processing_shader() {
    return Settings::values.post_processing_shader.c_str();
}

void vvctre_settings_set_texture_filter(const char* value) {
    Settings::values.texture_filter = std::string(value);
}

const char* vvctre_settings_get_texture_filter() {
    return Settings::values.texture_filter.c_str();
}

void vvctre_settings_set_render_3d(int value) {
    Settings::values.render_3d = static_cast<Settings::StereoRenderOption>(value);
}

int vvctre_settings_get_render_3d() {
    return static_cast<int>(Settings::values.render_3d);
}

void vvctre_settings_set_factor_3d(u8 value) {
    Settings::values.factor_3d = value;
}

u8 vvctre_settings_get_factor_3d() {
    return Settings::values.factor_3d.load();
}

// Controls Settings
void vvctre_settings_set_button(int index, const char* params) {
    Settings::values.buttons[index] = std::string(params);
}

const char* vvctre_settings_get_button(int index) {
    return Settings::values.buttons[index].c_str();
}

void vvctre_settings_set_analog(int index, const char* params) {
    Settings::values.analogs[index] = std::string(params);
}

const char* vvctre_settings_get_analog(int index) {
    return Settings::values.analogs[index].c_str();
}

void vvctre_settings_set_motion_device(const char* params) {
    Settings::values.motion_device = std::string(params);
}

const char* vvctre_settings_get_motion_device() {
    return Settings::values.motion_device.c_str();
}

void vvctre_settings_set_touch_device(const char* params) {
    Settings::values.touch_device = std::string(params);
}

const char* vvctre_settings_get_touch_device() {
    return Settings::values.touch_device.c_str();
}

void vvctre_settings_set_cemuhookudp_address(const char* value) {
    Settings::values.cemuhookudp_address = std::string(value);
}

const char* vvctre_settings_get_cemuhookudp_address() {
    return Settings::values.cemuhookudp_address.c_str();
}

void vvctre_settings_set_cemuhookudp_port(u16 value) {
    Settings::values.cemuhookudp_port = value;
}

u16 vvctre_settings_get_cemuhookudp_port() {
    return Settings::values.cemuhookudp_port;
}

void vvctre_settings_set_cemuhookudp_pad_index(u8 value) {
    Settings::values.cemuhookudp_pad_index = value;
}

u8 vvctre_settings_get_cemuhookudp_pad_index() {
    return Settings::values.cemuhookudp_pad_index;
}

// Layout Settings
void vvctre_settings_set_layout(int value) {
    Settings::values.layout = static_cast<Settings::Layout>(value);
}

int vvctre_settings_get_layout() {
    return static_cast<int>(Settings::values.layout);
}

void vvctre_settings_set_swap_screens(bool value) {
    Settings::values.swap_screens = value;
}

bool vvctre_settings_get_swap_screens() {
    return Settings::values.swap_screens;
}

void vvctre_settings_set_upright_screens(bool value) {
    Settings::values.upright_screens = value;
}

bool vvctre_settings_get_upright_screens() {
    return Settings::values.upright_screens;
}

void vvctre_settings_set_use_custom_layout(bool value) {
    Settings::values.use_custom_layout = value;
}

bool vvctre_settings_get_use_custom_layout() {
    return Settings::values.use_custom_layout;
}

void vvctre_settings_set_custom_layout_top_left(u16 value) {
    Settings::values.custom_layout_top_left = value;
}

u16 vvctre_settings_get_custom_layout_top_left() {
    return Settings::values.custom_layout_top_left;
}

void vvctre_settings_set_custom_layout_top_top(u16 value) {
    Settings::values.custom_layout_top_top = value;
}

u16 vvctre_settings_get_custom_layout_top_top() {
    return Settings::values.custom_layout_top_top;
}

void vvctre_settings_set_custom_layout_top_right(u16 value) {
    Settings::values.custom_layout_top_right = value;
}

u16 vvctre_settings_get_custom_layout_top_right() {
    return Settings::values.custom_layout_top_right;
}

void vvctre_settings_set_custom_layout_top_bottom(u16 value) {
    Settings::values.custom_layout_top_bottom = value;
}

u16 vvctre_settings_get_custom_layout_top_bottom() {
    return Settings::values.custom_layout_top_bottom;
}

void vvctre_settings_set_custom_layout_bottom_left(u16 value) {
    Settings::values.custom_layout_bottom_left = value;
}

u16 vvctre_settings_get_custom_layout_bottom_left() {
    return Settings::values.custom_layout_bottom_left;
}

void vvctre_settings_set_custom_layout_bottom_top(u16 value) {
    Settings::values.custom_layout_bottom_top = value;
}

u16 vvctre_settings_get_custom_layout_bottom_top() {
    return Settings::values.custom_layout_bottom_top;
}

void vvctre_settings_set_custom_layout_bottom_right(u16 value) {
    Settings::values.custom_layout_bottom_right = value;
}

u16 vvctre_settings_get_custom_layout_bottom_right() {
    return Settings::values.custom_layout_bottom_right;
}

void vvctre_settings_set_custom_layout_bottom_bottom(u16 value) {
    Settings::values.custom_layout_bottom_bottom = value;
}

u16 vvctre_settings_get_custom_layout_bottom_bottom() {
    return Settings::values.custom_layout_bottom_bottom;
}

// LLE Modules Settings
void vvctre_settings_set_use_lle_module(const char* name, bool value) {
    Settings::values.lle_modules[std::string(name)] = value;
}

bool vvctre_settings_get_use_lle_module(const char* name) {
    return Settings::values.lle_modules[std::string(name)];
}

// Other
const char* vvctre_get_version() {
    return fmt::format("{}.{}.{}", vvctre_version_major, vvctre_version_minor, vvctre_version_patch)
        .c_str();
}

bool vvctre_emulation_running(void* core) {
    return static_cast<Core::System*>(core)->IsPoweredOn();
}

void vvctre_set_play_coins(u16 value) {
    Service::PTM::Module::SetPlayCoins(value);
}

u16 vvctre_get_play_coins() {
    return Service::PTM::Module::GetPlayCoins();
}

std::unordered_map<std::string, void*> PluginManager::function_map = {
    {"vvctre_load_file", (void*)&vvctre_load_file},
    {"vvctre_install_cia", (void*)&vvctre_install_cia},
    {"vvctre_load_amiibo", (void*)&vvctre_load_amiibo},
    {"vvctre_remove_amiibo", (void*)&vvctre_remove_amiibo},
    {"vvctre_restart", (void*)&vvctre_restart},
    {"vvctre_set_paused", (void*)&vvctre_set_paused},
    {"vvctre_get_paused", (void*)&vvctre_get_paused},
    {"vvctre_read_u8", (void*)&vvctre_read_u8},
    {"vvctre_write_u8", (void*)&vvctre_write_u8},
    {"vvctre_read_u16", (void*)&vvctre_read_u16},
    {"vvctre_write_u16", (void*)&vvctre_write_u16},
    {"vvctre_read_u32", (void*)&vvctre_read_u32},
    {"vvctre_write_u32", (void*)&vvctre_write_u32},
    {"vvctre_read_u64", (void*)&vvctre_read_u64},
    {"vvctre_write_u64", (void*)&vvctre_write_u64},
    {"vvctre_set_pc", (void*)&vvctre_set_pc},
    {"vvctre_get_pc", (void*)&vvctre_get_pc},
    {"vvctre_set_register", (void*)&vvctre_set_register},
    {"vvctre_get_register", (void*)&vvctre_get_register},
    {"vvctre_set_vfp_register", (void*)&vvctre_set_vfp_register},
    {"vvctre_get_vfp_register", (void*)&vvctre_get_vfp_register},
    {"vvctre_set_vfp_system_register", (void*)&vvctre_set_vfp_system_register},
    {"vvctre_get_vfp_system_register", (void*)&vvctre_get_vfp_system_register},
    {"vvctre_set_cp15_register", (void*)&vvctre_set_cp15_register},
    {"vvctre_get_cp15_register", (void*)&vvctre_get_cp15_register},
    {"vvctre_cheat_count", (void*)&vvctre_cheat_count},
    {"vvctre_get_cheat", (void*)&vvctre_get_cheat},
    {"vvctre_get_cheat_name", (void*)&vvctre_get_cheat_name},
    {"vvctre_get_cheat_comments", (void*)&vvctre_get_cheat_comments},
    {"vvctre_get_cheat_type", (void*)&vvctre_get_cheat_type},
    {"vvctre_get_cheat_code", (void*)&vvctre_get_cheat_code},
    {"vvctre_set_cheat_enabled", (void*)&vvctre_set_cheat_enabled},
    {"vvctre_add_gateway_cheat", (void*)&vvctre_add_gateway_cheat},
    {"vvctre_remove_cheat", (void*)&vvctre_remove_cheat},
    {"vvctre_update_gateway_cheat", (void*)&vvctre_update_gateway_cheat},
    {"vvctre_reload_camera_images", (void*)&vvctre_reload_camera_images},
    {"vvctre_gui_text", (void*)&vvctre_gui_text},
    {"vvctre_gui_button", (void*)&vvctre_gui_button},
    {"vvctre_gui_begin", (void*)&vvctre_gui_begin},
    {"vvctre_gui_end", (void*)&vvctre_gui_end},
    {"vvctre_gui_begin_menu", (void*)&vvctre_gui_begin_menu},
    {"vvctre_gui_end_menu", (void*)&vvctre_gui_end_menu},
    {"vvctre_gui_menu_item", (void*)&vvctre_gui_menu_item},
    {"vvctre_button_device_new", (void*)&vvctre_button_device_new},
    {"vvctre_button_device_delete", (void*)&vvctre_button_device_delete},
    {"vvctre_button_device_get_state", (void*)&vvctre_button_device_get_state},
    {"vvctre_movie_prepare_for_playback", (void*)&vvctre_movie_prepare_for_playback},
    {"vvctre_movie_prepare_for_recording", (void*)&vvctre_movie_prepare_for_recording},
    {"vvctre_movie_play", (void*)&vvctre_movie_play},
    {"vvctre_movie_record", (void*)&vvctre_movie_record},
    {"vvctre_movie_is_playing", (void*)&vvctre_movie_is_playing},
    {"vvctre_movie_is_recording", (void*)&vvctre_movie_is_recording},
    {"vvctre_movie_stop", (void*)&vvctre_movie_stop},
    {"vvctre_set_frame_advancing_enabled", (void*)&vvctre_set_frame_advancing_enabled},
    {"vvctre_get_frame_advancing_enabled", (void*)&vvctre_get_frame_advancing_enabled},
    {"vvctre_advance_frame", (void*)&vvctre_advance_frame},
    {"vvctre_set_custom_pad_state", (void*)&vvctre_set_custom_pad_state},
    {"vvctre_use_real_pad_state", (void*)&vvctre_use_real_pad_state},
    {"vvctre_set_custom_circle_pad_state", (void*)&vvctre_set_custom_circle_pad_state},
    {"vvctre_use_real_circle_pad_state", (void*)&vvctre_use_real_circle_pad_state},
    {"vvctre_set_custom_touch_state", (void*)&vvctre_set_custom_touch_state},
    {"vvctre_use_real_touch_state", (void*)&vvctre_use_real_touch_state},
    {"vvctre_set_custom_motion_state", (void*)&vvctre_set_custom_motion_state},
    {"vvctre_use_real_motion_state", (void*)&vvctre_use_real_motion_state},
    {"vvctre_settings_apply", (void*)&vvctre_settings_apply},
    {"vvctre_settings_log", (void*)&vvctre_settings_log},
    {"vvctre_settings_set_file_path", (void*)&vvctre_settings_set_file_path},
    {"vvctre_settings_get_file_path", (void*)&vvctre_settings_get_file_path},
    {"vvctre_settings_set_play_movie", (void*)&vvctre_settings_set_play_movie},
    {"vvctre_settings_get_play_movie", (void*)&vvctre_settings_get_play_movie},
    {"vvctre_settings_set_record_movie", (void*)&vvctre_settings_set_record_movie},
    {"vvctre_settings_get_record_movie", (void*)&vvctre_settings_get_record_movie},
    {"vvctre_settings_set_region_value", (void*)&vvctre_settings_set_region_value},
    {"vvctre_settings_get_region_value", (void*)&vvctre_settings_get_region_value},
    {"vvctre_settings_set_log_filter", (void*)&vvctre_settings_set_log_filter},
    {"vvctre_settings_get_log_filter", (void*)&vvctre_settings_get_log_filter},
    {"vvctre_settings_set_multiplayer_url", (void*)&vvctre_settings_set_multiplayer_url},
    {"vvctre_settings_get_multiplayer_url", (void*)&vvctre_settings_get_multiplayer_url},
    {"vvctre_settings_set_initial_clock", (void*)&vvctre_settings_set_initial_clock},
    {"vvctre_settings_get_initial_clock", (void*)&vvctre_settings_get_initial_clock},
    {"vvctre_settings_set_unix_timestamp", (void*)&vvctre_settings_set_unix_timestamp},
    {"vvctre_settings_get_unix_timestamp", (void*)&vvctre_settings_get_unix_timestamp},
    {"vvctre_settings_set_use_virtual_sd", (void*)&vvctre_settings_set_use_virtual_sd},
    {"vvctre_settings_get_use_virtual_sd", (void*)&vvctre_settings_get_use_virtual_sd},
    {"vvctre_settings_set_start_in_fullscreen_mode",
     (void*)&vvctre_settings_set_start_in_fullscreen_mode},
    {"vvctre_settings_get_start_in_fullscreen_mode",
     (void*)&vvctre_settings_get_start_in_fullscreen_mode},
    {"vvctre_settings_set_record_frame_times", (void*)&vvctre_settings_set_record_frame_times},
    {"vvctre_settings_get_record_frame_times", (void*)&vvctre_settings_get_record_frame_times},
    {"vvctre_settings_enable_gdbstub", (void*)&vvctre_settings_enable_gdbstub},
    {"vvctre_settings_disable_gdbstub", (void*)&vvctre_settings_disable_gdbstub},
    {"vvctre_settings_is_gdb_stub_enabled", (void*)&vvctre_settings_is_gdb_stub_enabled},
    {"vvctre_settings_get_gdb_stub_port", (void*)&vvctre_settings_get_gdb_stub_port},
    {"vvctre_settings_set_use_cpu_jit", (void*)&vvctre_settings_set_use_cpu_jit},
    {"vvctre_settings_get_use_cpu_jit", (void*)&vvctre_settings_get_use_cpu_jit},
    {"vvctre_settings_set_limit_speed", (void*)&vvctre_settings_set_limit_speed},
    {"vvctre_settings_get_limit_speed", (void*)&vvctre_settings_get_limit_speed},
    {"vvctre_settings_set_speed_limit", (void*)&vvctre_settings_set_speed_limit},
    {"vvctre_settings_get_speed_limit", (void*)&vvctre_settings_get_speed_limit},
    {"vvctre_settings_set_use_custom_cpu_ticks", (void*)&vvctre_settings_set_use_custom_cpu_ticks},
    {"vvctre_settings_get_use_custom_cpu_ticks", (void*)&vvctre_settings_get_use_custom_cpu_ticks},
    {"vvctre_settings_set_custom_cpu_ticks", (void*)&vvctre_settings_set_custom_cpu_ticks},
    {"vvctre_settings_get_custom_cpu_ticks", (void*)&vvctre_settings_get_custom_cpu_ticks},
    {"vvctre_settings_set_cpu_clock_percentage", (void*)&vvctre_settings_set_cpu_clock_percentage},
    {"vvctre_settings_get_cpu_clock_percentage", (void*)&vvctre_settings_get_cpu_clock_percentage},
    {"vvctre_settings_set_enable_dsp_lle", (void*)&vvctre_settings_set_enable_dsp_lle},
    {"vvctre_settings_get_enable_dsp_lle", (void*)&vvctre_settings_get_enable_dsp_lle},
    {"vvctre_settings_set_enable_dsp_lle_multithread",
     (void*)&vvctre_settings_set_enable_dsp_lle_multithread},
    {"vvctre_settings_get_enable_dsp_lle_multithread",
     (void*)&vvctre_settings_get_enable_dsp_lle_multithread},
    {"vvctre_settings_set_audio_volume", (void*)&vvctre_settings_set_audio_volume},
    {"vvctre_settings_get_audio_volume", (void*)&vvctre_settings_get_audio_volume},
    {"vvctre_settings_set_audio_sink_id", (void*)&vvctre_settings_set_audio_sink_id},
    {"vvctre_settings_get_audio_sink_id", (void*)&vvctre_settings_get_audio_sink_id},
    {"vvctre_settings_set_audio_device_id", (void*)&vvctre_settings_set_audio_device_id},
    {"vvctre_settings_get_audio_device_id", (void*)&vvctre_settings_get_audio_device_id},
    {"vvctre_settings_set_microphone_input_type",
     (void*)&vvctre_settings_set_microphone_input_type},
    {"vvctre_settings_get_microphone_input_type",
     (void*)&vvctre_settings_get_microphone_input_type},
    {"vvctre_settings_set_microphone_device", (void*)&vvctre_settings_set_microphone_device},
    {"vvctre_settings_get_microphone_device", (void*)&vvctre_settings_get_microphone_device},
    {"vvctre_settings_set_camera_engine", (void*)&vvctre_settings_set_camera_engine},
    {"vvctre_settings_get_camera_engine", (void*)&vvctre_settings_get_camera_engine},
    {"vvctre_settings_set_camera_parameter", (void*)&vvctre_settings_set_camera_parameter},
    {"vvctre_settings_get_camera_parameter", (void*)&vvctre_settings_get_camera_parameter},
    {"vvctre_settings_set_camera_flip", (void*)&vvctre_settings_set_camera_flip},
    {"vvctre_settings_get_camera_flip", (void*)&vvctre_settings_get_camera_flip},
    {"vvctre_settings_set_use_hardware_renderer",
     (void*)&vvctre_settings_set_use_hardware_renderer},
    {"vvctre_settings_get_use_hardware_renderer",
     (void*)&vvctre_settings_get_use_hardware_renderer},
    {"vvctre_settings_set_use_hardware_shader", (void*)&vvctre_settings_set_use_hardware_shader},
    {"vvctre_settings_get_use_hardware_shader", (void*)&vvctre_settings_get_use_hardware_shader},
    {"vvctre_settings_set_hardware_shader_accurate_multiplication",
     (void*)&vvctre_settings_set_hardware_shader_accurate_multiplication},
    {"vvctre_settings_get_hardware_shader_accurate_multiplication",
     (void*)&vvctre_settings_get_hardware_shader_accurate_multiplication},
    {"vvctre_settings_set_use_shader_jit", (void*)&vvctre_settings_set_use_shader_jit},
    {"vvctre_settings_get_use_shader_jit", (void*)&vvctre_settings_get_use_shader_jit},
    {"vvctre_settings_set_enable_vsync", (void*)&vvctre_settings_set_enable_vsync},
    {"vvctre_settings_get_enable_vsync", (void*)&vvctre_settings_get_enable_vsync},
    {"vvctre_settings_set_dump_textures", (void*)&vvctre_settings_set_dump_textures},
    {"vvctre_settings_get_dump_textures", (void*)&vvctre_settings_get_dump_textures},
    {"vvctre_settings_set_custom_textures", (void*)&vvctre_settings_set_custom_textures},
    {"vvctre_settings_get_custom_textures", (void*)&vvctre_settings_get_custom_textures},
    {"vvctre_settings_set_preload_textures", (void*)&vvctre_settings_set_preload_textures},
    {"vvctre_settings_get_preload_textures", (void*)&vvctre_settings_get_preload_textures},
    {"vvctre_settings_set_enable_linear_filtering",
     (void*)&vvctre_settings_set_enable_linear_filtering},
    {"vvctre_settings_get_enable_linear_filtering",
     (void*)&vvctre_settings_get_enable_linear_filtering},
    {"vvctre_settings_set_sharper_distant_objects",
     (void*)&vvctre_settings_set_sharper_distant_objects},
    {"vvctre_settings_get_sharper_distant_objects",
     (void*)&vvctre_settings_get_sharper_distant_objects},
    {"vvctre_settings_set_resolution", (void*)&vvctre_settings_set_resolution},
    {"vvctre_settings_get_resolution", (void*)&vvctre_settings_get_resolution},
    {"vvctre_settings_set_background_color_red", (void*)&vvctre_settings_set_background_color_red},
    {"vvctre_settings_get_background_color_red", (void*)&vvctre_settings_get_background_color_red},
    {"vvctre_settings_set_background_color_green",
     (void*)&vvctre_settings_set_background_color_green},
    {"vvctre_settings_get_background_color_green",
     (void*)&vvctre_settings_get_background_color_green},
    {"vvctre_settings_set_background_color_blue",
     (void*)&vvctre_settings_set_background_color_blue},
    {"vvctre_settings_get_background_color_blue",
     (void*)&vvctre_settings_get_background_color_blue},
    {"vvctre_settings_set_post_processing_shader",
     (void*)&vvctre_settings_set_post_processing_shader},
    {"vvctre_settings_get_post_processing_shader",
     (void*)&vvctre_settings_get_post_processing_shader},
    {"vvctre_settings_set_texture_filter", (void*)&vvctre_settings_set_texture_filter},
    {"vvctre_settings_get_texture_filter", (void*)&vvctre_settings_get_texture_filter},
    {"vvctre_settings_set_render_3d", (void*)&vvctre_settings_set_render_3d},
    {"vvctre_settings_get_render_3d", (void*)&vvctre_settings_get_render_3d},
    {"vvctre_settings_set_factor_3d", (void*)&vvctre_settings_set_factor_3d},
    {"vvctre_settings_get_factor_3d", (void*)&vvctre_settings_get_factor_3d},
    {"vvctre_settings_set_button", (void*)&vvctre_settings_set_button},
    {"vvctre_settings_get_button", (void*)&vvctre_settings_get_button},
    {"vvctre_settings_set_analog", (void*)&vvctre_settings_set_analog},
    {"vvctre_settings_get_analog", (void*)&vvctre_settings_get_analog},
    {"vvctre_settings_set_motion_device", (void*)&vvctre_settings_set_motion_device},
    {"vvctre_settings_get_motion_device", (void*)&vvctre_settings_get_motion_device},
    {"vvctre_settings_set_touch_device", (void*)&vvctre_settings_set_touch_device},
    {"vvctre_settings_get_touch_device", (void*)&vvctre_settings_get_touch_device},
    {"vvctre_settings_set_cemuhookudp_address", (void*)&vvctre_settings_set_cemuhookudp_address},
    {"vvctre_settings_get_cemuhookudp_address", (void*)&vvctre_settings_get_cemuhookudp_address},
    {"vvctre_settings_set_cemuhookudp_port", (void*)&vvctre_settings_set_cemuhookudp_port},
    {"vvctre_settings_get_cemuhookudp_port", (void*)&vvctre_settings_get_cemuhookudp_port},
    {"vvctre_settings_set_cemuhookudp_pad_index",
     (void*)&vvctre_settings_set_cemuhookudp_pad_index},
    {"vvctre_settings_get_cemuhookudp_pad_index",
     (void*)&vvctre_settings_get_cemuhookudp_pad_index},
    {"vvctre_settings_set_layout", (void*)&vvctre_settings_set_layout},
    {"vvctre_settings_get_layout", (void*)&vvctre_settings_get_layout},
    {"vvctre_settings_set_swap_screens", (void*)&vvctre_settings_set_swap_screens},
    {"vvctre_settings_get_swap_screens", (void*)&vvctre_settings_get_swap_screens},
    {"vvctre_settings_set_upright_screens", (void*)&vvctre_settings_set_upright_screens},
    {"vvctre_settings_get_upright_screens", (void*)&vvctre_settings_get_upright_screens},
    {"vvctre_settings_set_use_custom_layout", (void*)&vvctre_settings_set_use_custom_layout},
    {"vvctre_settings_get_use_custom_layout", (void*)&vvctre_settings_get_use_custom_layout},
    {"vvctre_settings_set_custom_layout_top_left",
     (void*)&vvctre_settings_set_custom_layout_top_left},
    {"vvctre_settings_get_custom_layout_top_left",
     (void*)&vvctre_settings_get_custom_layout_top_left},
    {"vvctre_settings_set_custom_layout_top_top",
     (void*)&vvctre_settings_set_custom_layout_top_top},
    {"vvctre_settings_get_custom_layout_top_top",
     (void*)&vvctre_settings_get_custom_layout_top_top},
    {"vvctre_settings_set_custom_layout_top_right",
     (void*)&vvctre_settings_set_custom_layout_top_right},
    {"vvctre_settings_get_custom_layout_top_right",
     (void*)&vvctre_settings_get_custom_layout_top_right},
    {"vvctre_settings_set_custom_layout_top_bottom",
     (void*)&vvctre_settings_set_custom_layout_top_bottom},
    {"vvctre_settings_get_custom_layout_top_bottom",
     (void*)&vvctre_settings_get_custom_layout_top_bottom},
    {"vvctre_settings_set_custom_layout_bottom_left",
     (void*)&vvctre_settings_set_custom_layout_bottom_left},
    {"vvctre_settings_get_custom_layout_bottom_left",
     (void*)&vvctre_settings_get_custom_layout_bottom_left},
    {"vvctre_settings_set_custom_layout_bottom_top",
     (void*)&vvctre_settings_set_custom_layout_bottom_top},
    {"vvctre_settings_get_custom_layout_bottom_top",
     (void*)&vvctre_settings_get_custom_layout_bottom_top},
    {"vvctre_settings_set_custom_layout_bottom_right",
     (void*)&vvctre_settings_set_custom_layout_bottom_right},
    {"vvctre_settings_get_custom_layout_bottom_right",
     (void*)&vvctre_settings_get_custom_layout_bottom_right},
    {"vvctre_settings_set_custom_layout_bottom_bottom",
     (void*)&vvctre_settings_set_custom_layout_bottom_bottom},
    {"vvctre_settings_get_custom_layout_bottom_bottom",
     (void*)&vvctre_settings_get_custom_layout_bottom_bottom},
    {"vvctre_settings_set_use_lle_module", (void*)&vvctre_settings_set_use_lle_module},
    {"vvctre_settings_get_use_lle_module", (void*)&vvctre_settings_get_use_lle_module},
    {"vvctre_get_version", (void*)&vvctre_get_version},
    {"vvctre_emulation_running", (void*)&vvctre_emulation_running},
    {"vvctre_set_play_coins", (void*)&vvctre_set_play_coins},
    {"vvctre_get_play_coins", (void*)&vvctre_get_play_coins},
};
