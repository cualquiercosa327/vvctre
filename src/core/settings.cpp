// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "audio_core/dsp_interface.h"
#include "common/param_package.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/mic_u.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Settings {

Values values = {};

void Apply() {
    GDBStub::SetServerPort(values.gdbstub_port);
    GDBStub::ToggleServer(values.use_gdbstub);
    InputCommon::ReloadInputDevices();

    VideoCore::g_hardware_renderer_enabled = values.use_hardware_renderer;
    VideoCore::g_shader_jit_enabled = values.use_shader_jit;
    VideoCore::g_hardware_shader_enabled = values.use_hardware_shader;
    VideoCore::g_hardware_shader_accurate_multiplication =
        values.hardware_shader_accurate_multiplication;

    if (VideoCore::g_renderer) {
        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
    }

    VideoCore::g_renderer_background_color_update_requested = true;
    VideoCore::g_renderer_sampler_update_requested = true;
    VideoCore::g_renderer_shader_update_requested = true;
    VideoCore::g_texture_filter_update_requested = true;

    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        AudioCore::DspInterface& dsp = system.DSP();
        dsp.SetSink(values.audio_sink_id, values.audio_device_id);

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

void LogSettings() {
    {
        LOG_INFO(Settings, "Start:");
        LOG_INFO(Settings, "\tFile: {}", values.file_path);
        LOG_INFO(Settings, "\tPlay Movie: {}", values.play_movie);
        LOG_INFO(Settings, "\tRecord Movie: {}", values.record_movie);
        LOG_INFO(Settings, "\tRegion: {}", [] {
            switch (Settings::values.region_value) {
            case -1:
                return "Auto-select";
            case 0:
                return "Japan";
            case 1:
                return "USA";
            case 2:
                return "Europe";
            case 3:
                return "Australia";
            case 4:
                return "China";
            case 5:
                return "Korea";
            case 6:
                return "Taiwan";
            }

            return "Invalid";
        }());
        LOG_INFO(Settings, "\tLog Filter: {}", values.log_filter);
        LOG_INFO(Settings, "\tMultiplayer Server URL: {}", values.multiplayer_url);
        if (values.use_virtual_sd) {
            LOG_INFO(Settings, "\t[x] Use Virtual SD Card");
        } else {
            LOG_INFO(Settings, "\t[ ] Use Virtual SD Card");
        }
        if (values.start_in_fullscreen_mode) {
            LOG_INFO(Settings, "\t[x] Start in Fullscreen Mode");
        } else {
            LOG_INFO(Settings, "\t[ ] Start in Fullscreen Mode");
        }
        if (values.record_frame_times) {
            LOG_INFO(Settings, "\t[x] Record Frame Times");
        } else {
            LOG_INFO(Settings, "\t[ ] Record Frame Times");
        }
        if (values.use_gdbstub) {
            LOG_INFO(Settings, "\t[x] Enable GDB Stub\tPort: {}", values.gdbstub_port);
        } else {
            LOG_INFO(Settings, "\t[ ] Enable GDB Stub");
        }
    }
    {
        LOG_INFO(Settings, "General:");
        if (values.use_cpu_jit) {
            LOG_INFO(Settings, "\t[x] Use CPU JIT");
        } else {
            LOG_INFO(Settings, "\t[ ] Use CPU JIT");
        }
        if (values.limit_speed) {
            LOG_INFO(Settings, "\t[x] Limit Speed To {}", values.speed_limit);
        } else {
            LOG_INFO(Settings, "\t[ ] Limit Speed");
        }
        if (values.use_custom_cpu_ticks) {
            LOG_INFO(Settings, "\t[x] Custom CPU Ticks: {}", values.custom_cpu_ticks);
        } else {
            LOG_INFO(Settings, "\t[ ] Custom CPU Ticks");
        }
        LOG_INFO(Settings, "\tCPU Clock Percentage: {}%", values.cpu_clock_percentage);
    }
    {
        LOG_INFO(Settings, "Camera:");
        LOG_INFO(
            Settings, "\tInner Camera Engine: {}",
            values.camera_engine[static_cast<std::size_t>(Service::CAM::CameraIndex::InnerCamera)]);
        LOG_INFO(Settings, "\tInner Camera Configuration: {}",
                 values.camera_parameter[static_cast<std::size_t>(
                     Service::CAM::CameraIndex::InnerCamera)]);
        LOG_INFO(Settings, "\tOuter Left Camera Engine: {}",
                 values.camera_engine[static_cast<std::size_t>(
                     Service::CAM::CameraIndex::OuterLeftCamera)]);
        LOG_INFO(Settings, "\tOuter Left Camera Configuration: {}",
                 values.camera_parameter[static_cast<std::size_t>(
                     Service::CAM::CameraIndex::OuterLeftCamera)]);
        LOG_INFO(Settings, "\tOuter Right Camera Engine: {}",
                 values.camera_engine[static_cast<std::size_t>(
                     Service::CAM::CameraIndex::OuterRightCamera)]);
        LOG_INFO(Settings, "\tOuter Right Camera Configuration: {}",
                 values.camera_parameter[static_cast<std::size_t>(
                     Service::CAM::CameraIndex::OuterRightCamera)]);
    }
    {
        std::shared_ptr<Service::CFG::Module> cfg = nullptr;
        if (Core::System::GetInstance().IsPoweredOn()) {
            cfg = Service::CFG::GetModule(Core::System::GetInstance());
        }
        if (cfg == nullptr) {
            cfg = std::make_shared<Service::CFG::Module>();
        }
        LOG_INFO(Settings, "System:");
        LOG_INFO(Settings, "\tUsername: {}", Common::UTF16ToUTF8(cfg->GetUsername()));
        auto [month, day] = cfg->GetBirthday();
        LOG_INFO(Settings, "\tBirthday: {:02}/{:02}", day, month);
        LOG_INFO(Settings, "\tLanguage: {}", [&] {
            switch (cfg->GetSystemLanguage()) {
            case Service::CFG::SystemLanguage::LANGUAGE_JP:
                return "Japanese";
            case Service::CFG::SystemLanguage::LANGUAGE_EN:
                return "English";
            case Service::CFG::SystemLanguage::LANGUAGE_FR:
                return "French";
            case Service::CFG::SystemLanguage::LANGUAGE_DE:
                return "German";
            case Service::CFG::SystemLanguage::LANGUAGE_IT:
                return "Italian";
            case Service::CFG::SystemLanguage::LANGUAGE_ES:
                return "Spanish";
            case Service::CFG::SystemLanguage::LANGUAGE_ZH:
                return "Simplified Chinese";
            case Service::CFG::SystemLanguage::LANGUAGE_KO:
                return "Korean";
            case Service::CFG::SystemLanguage::LANGUAGE_NL:
                return "Dutch";
            case Service::CFG::SystemLanguage::LANGUAGE_PT:
                return "Portugese";
            case Service::CFG::SystemLanguage::LANGUAGE_RU:
                return "Russian";
            case Service::CFG::SystemLanguage::LANGUAGE_TW:
                return "Traditional Chinese";
            default:
                break;
            }

            return "Invalid language";
        }());
        LOG_INFO(Settings, "\tSound output mode: {}", [&] {
            switch (cfg->GetSoundOutputMode()) {
            case Service::CFG::SoundOutputMode::SOUND_MONO:
                return "Mono";
            case Service::CFG::SoundOutputMode::SOUND_STEREO:
                return "Stereo";
            case Service::CFG::SoundOutputMode::SOUND_SURROUND:
                return "Surround";
            default:
                break;
            }

            return "Invalid";
        }());
        LOG_INFO(Settings, "\tCountry: {}", [&] {
            switch (cfg->GetCountryCode()) {
            case 1:
                return "Japan";
            case 8:
                return "Anguilla";
            case 9:
                return "Antigua and Barbuda";
            case 10:
                return "Argentina";
            case 11:
                return "Aruba";
            case 12:
                return "Bahamas";
            case 13:
                return "Barbados";
            case 14:
                return "Belize";
            case 15:
                return "Bolivia";
            case 16:
                return "Brazil";
            case 17:
                return "British Virgin Islands";
            case 18:
                return "Canada";
            case 19:
                return "Cayman Islands";
            case 20:
                return "Chile";
            case 21:
                return "Colombia";
            case 22:
                return "Costa Rica";
            case 23:
                return "Dominica";
            case 24:
                return "Dominican Republic";
            case 25:
                return "Ecuador";
            case 26:
                return "El Salvador";
            case 27:
                return "French Guiana";
            case 28:
                return "Grenada";
            case 29:
                return "Guadeloupe";
            case 30:
                return "Guatemala";
            case 31:
                return "Guyana";
            case 32:
                return "Haiti";
            case 33:
                return "Honduras";
            case 34:
                return "Jamaica";
            case 35:
                return "Martinique";
            case 36:
                return "Mexico";
            case 37:
                return "Montserrat";
            case 38:
                return "Netherlands Antilles";
            case 39:
                return "Nicaragua";
            case 40:
                return "Panama";
            case 41:
                return "Paraguay";
            case 42:
                return "Peru";
            case 43:
                return "Saint Kitts and Nevis";
            case 44:
                return "Saint Lucia";
            case 45:
                return "Saint Vincent and the Grenadines";
            case 46:
                return "Suriname";
            case 47:
                return "Trinidad and Tobago";
            case 48:
                return "Turks and Caicos Islands";
            case 49:
                return "United States";
            case 50:
                return "Uruguay";
            case 51:
                return "US Virgin Islands";
            case 52:
                return "Venezuela";
            case 64:
                return "Albania";
            case 65:
                return "Australia";
            case 66:
                return "Austria";
            case 67:
                return "Belgium";
            case 68:
                return "Bosnia and Herzegovina";
            case 69:
                return "Botswana";
            case 70:
                return "Bulgaria";
            case 71:
                return "Croatia";
            case 72:
                return "Cyprus";
            case 73:
                return "Czech Republic";
            case 74:
                return "Denmark";
            case 75:
                return "Estonia";
            case 76:
                return "Finland";
            case 77:
                return "France";
            case 78:
                return "Germany";
            case 79:
                return "Greece";
            case 80:
                return "Hungary";
            case 81:
                return "Iceland";
            case 82:
                return "Ireland";
            case 83:
                return "Italy";
            case 84:
                return "Latvia";
            case 85:
                return "Lesotho";
            case 86:
                return "Liechtenstein";
            case 87:
                return "Lithuania";
            case 88:
                return "Luxembourg";
            case 89:
                return "Macedonia";
            case 90:
                return "Malta";
            case 91:
                return "Montenegro";
            case 92:
                return "Mozambique";
            case 93:
                return "Namibia";
            case 94:
                return "Netherlands";
            case 95:
                return "New Zealand";
            case 96:
                return "Norway";
            case 97:
                return "Poland";
            case 98:
                return "Portugal";
            case 99:
                return "Romania";
            case 100:
                return "Russia";
            case 101:
                return "Serbia";
            case 102:
                return "Slovakia";
            case 103:
                return "Slovenia";
            case 104:
                return "South Africa";
            case 105:
                return "Spain";
            case 106:
                return "Swaziland";
            case 107:
                return "Sweden";
            case 108:
                return "Switzerland";
            case 109:
                return "Turkey";
            case 110:
                return "United Kingdom";
            case 111:
                return "Zambia";
            case 112:
                return "Zimbabwe";
            case 113:
                return "Azerbaijan";
            case 114:
                return "Mauritania";
            case 115:
                return "Mali";
            case 116:
                return "Niger";
            case 117:
                return "Chad";
            case 118:
                return "Sudan";
            case 119:
                return "Eritrea";
            case 120:
                return "Djibouti";
            case 121:
                return "Somalia";
            case 122:
                return "Andorra";
            case 123:
                return "Gibraltar";
            case 124:
                return "Guernsey";
            case 125:
                return "Isle of Man";
            case 126:
                return "Jersey";
            case 127:
                return "Monaco";
            case 128:
                return "Taiwan";
            case 136:
                return "South Korea";
            case 144:
                return "Hong Kong";
            case 145:
                return "Macau";
            case 152:
                return "Indonesia";
            case 153:
                return "Singapore";
            case 154:
                return "Thailand";
            case 155:
                return "Philippines";
            case 156:
                return "Malaysia";
            case 160:
                return "China";
            case 168:
                return "United Arab Emirates";
            case 169:
                return "India";
            case 170:
                return "Egypt";
            case 171:
                return "Oman";
            case 172:
                return "Qatar";
            case 173:
                return "Kuwait";
            case 174:
                return "Saudi Arabia";
            case 175:
                return "Syria";
            case 176:
                return "Bahrain";
            case 177:
                return "Jordan";
            case 184:
                return "San Marino";
            case 185:
                return "Vatican City";
            case 186:
                return "Bermuda";
            default:
                break;
            }

            return "Invalid";
        }());
        LOG_INFO(Settings, "\tPlay Coins: {}", Service::PTM::Module::GetPlayCoins());
    }
    {
        LOG_INFO(Settings, "Graphics:");
        if (values.use_hardware_renderer) {
            LOG_INFO(Settings, "\t[x] Use Hardware Renderer");
        } else {
            LOG_INFO(Settings, "\t[ ] Use Hardware Renderer");
        }
        if (values.use_hardware_shader) {
            LOG_INFO(Settings, "\t\t[x] Use Hardware Shader");
            if (values.hardware_shader_accurate_multiplication) {
                LOG_INFO(Settings, "\t\t\t[x] Accurate Multiplication");
            } else {
                LOG_INFO(Settings, "\t\t\t[ ] Accurate Multiplication");
            }
        } else {
            LOG_INFO(Settings, "\t\t[ ] Use Hardware Shader");
        }
        if (values.use_shader_jit) {
            LOG_INFO(Settings, "\t[x] Use Shader JIT");
        } else {
            LOG_INFO(Settings, "\t[ ] Use Shader JIT");
        }
        if (values.enable_vsync) {
            LOG_INFO(Settings, "\t[x] Enable VSync");
        } else {
            LOG_INFO(Settings, "\t[ ] Enable VSync");
        }
        if (values.dump_textures) {
            LOG_INFO(Settings, "\t[x] Dump Textures");
        } else {
            LOG_INFO(Settings, "\t[ ] Dump Textures");
        }
        if (values.custom_textures) {
            LOG_INFO(Settings, "\t[x] Use Custom Textures");
        } else {
            LOG_INFO(Settings, "\t[ ] Use Custom Textures");
        }
        if (values.preload_textures) {
            LOG_INFO(Settings, "\t[x] Preload Custom Textures");
        } else {
            LOG_INFO(Settings, "\t[ ] Preload Custom Textures");
        }
        if (values.enable_linear_filtering) {
            LOG_INFO(Settings, "\t[x] Enable Linear Filtering");
        } else {
            LOG_INFO(Settings, "\t[ ] Enable Linear Filtering");
        }
        if (values.sharper_distant_objects) {
            LOG_INFO(Settings, "\t[x] Sharper Distant Objects");
        } else {
            LOG_INFO(Settings, "\t[ ] Sharper Distant Objects");
        }
        LOG_INFO(Settings, "\tResolution: {}x", values.resolution);
        LOG_INFO(Settings, "\tBackground Color: #{:02x}{:02x}{:02x}",
                 static_cast<int>(values.background_color_red * 255),
                 static_cast<int>(values.background_color_green * 255),
                 static_cast<int>(values.background_color_blue * 255));
        LOG_INFO(Settings, "\tPost Processing Shader: {}", values.post_processing_shader);
        LOG_INFO(Settings, "\tTexture Filter: {}", values.texture_filter);
        LOG_INFO(
            Settings, "\t3D: {} {}%",
            [] {
                switch (values.render_3d) {
                case Settings::StereoRenderOption::Off:
                    return "Off";
                case Settings::StereoRenderOption::SideBySide:
                    return "Side by Side";
                case Settings::StereoRenderOption::Anaglyph:
                    return "Anaglyph";
                case Settings::StereoRenderOption::Interlaced:
                    return "Interlaced";
                default:
                    break;
                }

                return "Invalid value";
            }(),
            values.factor_3d.load());
    }
    {
        LOG_INFO(Settings, "Controls:");
        {
            LOG_INFO(Settings, "\tButtons:");
            LOG_INFO(Settings, "\t\tA: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::A]));
            LOG_INFO(Settings, "\t\tB: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::B]));
            LOG_INFO(Settings, "\t\tX: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::X]));
            LOG_INFO(Settings, "\t\tY: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Y]));
            LOG_INFO(Settings, "\t\tL: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::L]));
            LOG_INFO(Settings, "\t\tR: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::R]));
            LOG_INFO(Settings, "\t\tZL: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::ZL]));
            LOG_INFO(Settings, "\t\tZR: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::ZR]));
            LOG_INFO(Settings, "\t\tStart: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Start]));
            LOG_INFO(Settings, "\t\tSelect: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Select]));
            LOG_INFO(Settings, "\t\tDebug: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Debug]));
            LOG_INFO(Settings, "\t\tGPIO14: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Gpio14]));
            LOG_INFO(Settings, "\t\tHome: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Home]));
        }
        {
            LOG_INFO(Settings, "\tCircle Pad:");
            LOG_INFO(Settings, "\t\tUp: {}",
                     InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePad], "up"));
            LOG_INFO(Settings, "\t\tDown: {}",
                     InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePad], "down"));
            LOG_INFO(Settings, "\t\tLeft: {}",
                     InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePad], "left"));
            LOG_INFO(Settings, "\t\tRight: {}",
                     InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePad], "right"));

            {
                const Common::ParamPackage params(values.analogs[NativeAnalog::CirclePad]);
                LOG_INFO(
                    Settings, "\t\tModifier: {} ({})",
                    InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePad], "modifier"),
                    params.Get("modifier_scale", 0.5f));
            }
        }
        {
            LOG_INFO(Settings, "\tCircle Pad Pro:");
            LOG_INFO(Settings, "\t\tUp: {}",
                     InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePadPro], "up"));
            LOG_INFO(Settings, "\t\tDown: {}",
                     InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePadPro], "down"));
            LOG_INFO(Settings, "\t\tLeft: {}",
                     InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePadPro], "left"));
            LOG_INFO(
                Settings, "\t\tRight: {}",
                InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePadPro], "right"));

            {
                const Common::ParamPackage params(values.analogs[NativeAnalog::CirclePadPro]);
                if (params.Get("engine", "") == "analog_from_button") {
                    LOG_INFO(Settings, "\t\tModifier: {} ({})",
                             InputCommon::AnalogToText(values.analogs[NativeAnalog::CirclePadPro],
                                                       "modifier"),
                             params.Get("modifier_scale", 0.5f));
                }
            }
        }
        {
            LOG_INFO(Settings, "\tD-Pad:");
            LOG_INFO(Settings, "\t\tUp: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Up]));
            LOG_INFO(Settings, "\t\tDown: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Down]));
            LOG_INFO(Settings, "\t\tLeft: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Left]));
            LOG_INFO(Settings, "\t\tRight: {}",
                     InputCommon::ButtonToText(values.buttons[NativeButton::Right]));
        }
        {
            bool use_cemuhookudp = false;

            const Common::ParamPackage motion_device(values.motion_device);
            if (motion_device.Get("engine", "") == "motion_emu") {
                const int update_period = motion_device.Get("update_period", 100);
                const float sensitivity = motion_device.Get("sensitivity", 0.01f);
                const float clamp = motion_device.Get("tilt_clamp", 90.0f);

                LOG_INFO(Settings,
                         "\tMotion: Right Click\tUpdate Period: {} Sensivity: {} Clamp: {}",
                         update_period, sensitivity, clamp);
            } else if (motion_device.Get("engine", "") == "cemuhookudp") {
                LOG_INFO(Settings, "\tMotion: CemuhookUDP");
                use_cemuhookudp = true;
            }

            const Common::ParamPackage touch_device(values.touch_device);
            if (touch_device.Get("engine", "") == "emu_window") {
                LOG_INFO(Settings, "\tTouch: Mouse");
            } else if (touch_device.Get("engine", "") == "cemuhookudp") {
                const int min_x = touch_device.Get("min_x", 100);
                const int min_y = touch_device.Get("min_y", 50);
                const int max_x = touch_device.Get("max_x", 1800);
                const int max_y = touch_device.Get("max_y", 850);

                LOG_INFO(Settings, "\tTouch: CemuhookUDP\tMin X: {} Min Y: {} Max X: {} Max Y: {}",
                         min_x, min_y, max_x, max_y);
                use_cemuhookudp = true;
            }

            if (use_cemuhookudp) {
                LOG_INFO(Settings, "\tCemuhookUDP: {}:{} Pad {}", values.cemuhookudp_address,
                         values.cemuhookudp_port, values.cemuhookudp_pad_index);
            }
        }
    }
    {
        LOG_INFO(Settings, "Layout:");
        if (!values.use_custom_layout) {
            LOG_INFO(Settings, "\tLayout: {}", [] {
                switch (values.layout) {
                case Settings::Layout::Default:
                    return "Default";
                case Settings::Layout::SingleScreen:
                    return "Single Screen";
                case Settings::Layout::LargeScreen:
                    return "Large Screen";
                case Settings::Layout::SideScreen:
                    return "Side by Side";
                case Settings::Layout::MediumScreen:
                    return "Medium Screen";
                default:
                    break;
                }

                return "Invalid";
            }());
        }
        if (values.use_custom_layout) {
            LOG_INFO(Settings, "\t[x] Use Custom Layout");
        } else {
            LOG_INFO(Settings, "\t[ ] Use Custom Layout");
        }
        if (values.swap_screens) {
            LOG_INFO(Settings, "\t[x] Swap Screens");
        } else {
            LOG_INFO(Settings, "\t[ ] Swap Screens");
        }
        if (values.upright_screens) {
            LOG_INFO(Settings, "\t[x] Upright Orientation");
        } else {
            LOG_INFO(Settings, "\t[ ] Upright Orientation");
        }
        if (values.use_custom_layout) {
            LOG_INFO(Settings, "\tTop:");
            LOG_INFO(Settings, "\t\tTop Left: {}", values.custom_layout_top_left);
            LOG_INFO(Settings, "\t\tTop Top: {}", values.custom_layout_top_top);
            LOG_INFO(Settings, "\t\tTop Right: {}", values.custom_layout_top_right);
            LOG_INFO(Settings, "\t\tTop Bottom: {}", values.custom_layout_top_bottom);
            LOG_INFO(Settings, "\tBottom:");
            LOG_INFO(Settings, "\t\tBottom Left: {}", values.custom_layout_bottom_left);
            LOG_INFO(Settings, "\t\tBottom Top: {}", values.custom_layout_bottom_top);
            LOG_INFO(Settings, "\t\tBottom Right: {}", values.custom_layout_bottom_right);
            LOG_INFO(Settings, "\t\tBottom Bottom: {}", values.custom_layout_bottom_bottom);
        }
    }
    {
        LOG_INFO(Settings, "LLE Modules:");
        for (const auto& module : values.lle_modules) {
            if (module.second) {
                LOG_INFO(Settings, "\t[x] {}", module.first);
            } else {
                LOG_INFO(Settings, "\t[ ] {}", module.first);
            }
        }
    }
}

} // namespace Settings
