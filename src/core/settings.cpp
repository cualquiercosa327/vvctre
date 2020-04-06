// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "audio_core/dsp_interface.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/mic_u.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Settings {

Values values = {};

void Apply() {
    GDBStub::SetServerPort(values.gdbstub_port);
    GDBStub::ToggleServer(values.use_gdbstub);
    InputCommon::ReloadInputDevices();

    VideoCore::g_hw_renderer_enabled = values.use_hw_renderer;
    VideoCore::g_shader_jit_enabled = values.use_shader_jit;
    VideoCore::g_hw_shader_enabled = values.use_hw_shader;
    VideoCore::g_hw_shader_accurate_mul = values.shaders_accurate_mul;

    if (VideoCore::g_renderer) {
        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
    }

    VideoCore::g_renderer_bg_color_update_requested = true;
    VideoCore::g_renderer_sampler_update_requested = true;
    VideoCore::g_renderer_shader_update_requested = true;
    VideoCore::g_texture_filter_update_requested = true;

    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        system.CoreTiming().UpdateClockSpeed(Settings::values.cpu_clock_percentage);

        AudioCore::DspInterface& dsp = system.DSP();
        dsp.SetSink(values.sink_id, values.audio_device_id);

        auto hid = Service::HID::GetModule(system);
        if (hid) {
            hid->ReloadInputDevices();
        }

        auto sm = system.ServiceManager();
        auto ir_user = sm.GetService<Service::IR::IR_USER>("ir:USER");
        if (ir_user) {
            ir_user->ReloadInputDevices();
        }

        auto cam = Service::CAM::GetModule(system);
        if (cam) {
            cam->ReloadCameraDevices();
        }

        Service::MIC::ReloadMic(system);
    }
}

template <typename T>
void LogSetting(const std::string& name, const T& value) {
    LOG_INFO(Config, "{}: {}", name, value);
}

void LogSettings() {
    LOG_INFO(Config, "Configuration:");
    LogSetting("use_cpu_jit", values.use_cpu_jit);
    LogSetting("multiplayer_url", values.multiplayer_url);
    LogSetting("use_virtual_sd", values.use_virtual_sd);
    LogSetting("region_value", values.region_value);
    LogSetting("init_clock", static_cast<int>(values.init_clock));
    LogSetting("init_time", values.init_time);
    LogSetting("use_hw_renderer", values.use_hw_renderer);
    LogSetting("use_hw_shader", values.use_hw_shader);
    LogSetting("shaders_accurate_mul", values.shaders_accurate_mul);
    LogSetting("use_shader_jit", values.use_shader_jit);
    LogSetting("resolution_factor", values.resolution_factor);
    LogSetting("use_frame_limit", values.use_frame_limit);
    LogSetting("frame_limit", values.frame_limit);
    LogSetting("texture_filter_name", values.texture_filter_name);
    LogSetting("bg_red", values.bg_red);
    LogSetting("bg_green", values.bg_green);
    LogSetting("bg_blue", values.bg_blue);
    LogSetting("render_3d", static_cast<int>(values.render_3d));
    LogSetting("factor_3d", values.factor_3d.load());
    LogSetting("filter_mode", values.filter_mode);
    LogSetting("pp_shader_name", values.pp_shader_name);
    LogSetting("enable_vsync", values.enable_vsync);
    LogSetting("sharper_distant_objects", values.sharper_distant_objects);
    LogSetting("layout_option", static_cast<int>(values.layout_option));
    LogSetting("swap_screen", values.swap_screen);
    LogSetting("upright_screen", values.upright_screen);
    LogSetting("custom_layout", values.custom_layout);
    LogSetting("custom_top_left", values.custom_top_left);
    LogSetting("custom_top_top", values.custom_top_top);
    LogSetting("custom_top_right", values.custom_top_right);
    LogSetting("custom_top_bottom", values.custom_top_bottom);
    LogSetting("custom_bottom_left", values.custom_bottom_left);
    LogSetting("custom_bottom_top", values.custom_bottom_top);
    LogSetting("custom_bottom_right", values.custom_bottom_right);
    LogSetting("custom_bottom_bottom", values.custom_bottom_bottom);
    LogSetting("dump_textures", values.dump_textures);
    LogSetting("custom_textures", values.custom_textures);
    LogSetting("preload_textures", values.preload_textures);
    LogSetting("enable_dsp_lle", values.enable_dsp_lle);
    LogSetting("enable_dsp_lle_multithread", values.enable_dsp_lle_multithread);
    LogSetting("sink_id", values.sink_id);
    LogSetting("audio_device_id", values.audio_device_id);
    LogSetting("volume", values.volume);
    LogSetting("mic_input_type", static_cast<int>(values.mic_input_type));
    LogSetting("mic_input_device", values.mic_input_device);
    LogSetting("audio_speed", values.audio_speed);
    for (std::size_t i = 0; i < static_cast<std::size_t>(Service::CAM::NumCameras); i++) {
        LogSetting(fmt::format("camera_name[{}]", i), values.camera_name[i]);
        LogSetting(fmt::format("camera_config[{}]", i), values.camera_config[i]);
        LogSetting(fmt::format("camera_flip[{}]", i), values.camera_flip[i]);
    }
    LogSetting("log_filter", values.log_filter);
    LogSetting("record_frame_times", values.record_frame_times);
    LogSetting("use_gdbstub", values.use_gdbstub);
    LogSetting("gdbstub_port", values.gdbstub_port);
    for (const auto& module : values.lle_modules) {
        LogSetting(fmt::format("lle_modules[\"{}\"]", module.first), module.second);
    }
}

} // namespace Settings
