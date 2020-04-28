// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sstream>
#include <httplib.h>
#include <json.hpp>
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/stb_image_write.h"
#include "common/thread.h"
#include "core/arm/arm_interface.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/nfc/nfc_u.h"
#include "core/memory.h"
#include "core/movie.h"
#include "core/rpc/server.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Common {

void to_json(nlohmann::json& json, const Vec3<float>& v) {
    json = nlohmann::json{
        {"x", v.x},
        {"y", v.y},
        {"z", v.z},
    };
}

void from_json(const nlohmann::json& json, Vec3<float>& v) {
    json.at("x").get_to(v.x);
    json.at("y").get_to(v.y);
    json.at("z").get_to(v.z);
}

} // namespace Common

namespace Layout {

void to_json(nlohmann::json& json, const Layout::FramebufferLayout& layout) {
    json = nlohmann::json{
        {"swap_screens", Settings::values.swap_screen},
        {"is_rotated", layout.is_rotated},
        {"width", layout.width},
        {"height", layout.height},
        {"top_screen",
         {
             {"enabled", layout.top_screen_enabled},
             {"width", layout.top_screen.GetWidth()},
             {"height", layout.top_screen.GetHeight()},
             {"left", layout.top_screen.left},
             {"top", layout.top_screen.top},
             {"right", layout.top_screen.right},
             {"bottom", layout.top_screen.bottom},
         }},
        {"bottom_screen",
         {
             {"enabled", layout.bottom_screen_enabled},
             {"width", layout.bottom_screen.GetWidth()},
             {"height", layout.bottom_screen.GetHeight()},
             {"left", layout.bottom_screen.left},
             {"top", layout.bottom_screen.top},
             {"right", layout.bottom_screen.right},
             {"bottom", layout.bottom_screen.bottom},
         }},
    };
}

void from_json(const nlohmann::json& json, Layout::FramebufferLayout& layout) {
    json.at("is_rotated").get_to(layout.is_rotated);
    json.at("width").get_to(layout.width);
    json.at("height").get_to(layout.height);
    json.at("top_screen").at("enabled").get_to(layout.top_screen_enabled);
    json.at("top_screen").at("left").get_to(layout.top_screen.left);
    json.at("top_screen").at("top").get_to(layout.top_screen.top);
    json.at("top_screen").at("right").get_to(layout.top_screen.right);
    json.at("top_screen").at("bottom").get_to(layout.top_screen.bottom);
    json.at("bottom_screen").at("enabled").get_to(layout.bottom_screen_enabled);
    json.at("bottom_screen").at("left").get_to(layout.bottom_screen.left);
    json.at("bottom_screen").at("top").get_to(layout.bottom_screen.top);
    json.at("bottom_screen").at("right").get_to(layout.bottom_screen.right);
    json.at("bottom_screen").at("bottom").get_to(layout.bottom_screen.bottom);
}

} // namespace Layout

namespace RPC {

Server::Server(Core::System& system, const int port, const std::string& vvctre_version) {
    server = std::make_unique<httplib::Server>();

    server->Get("/version", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"vvctre", vvctre_version},
                {"movie", Core::Movie::Version},
            }
                .dump(),
            "application/json");
    });

    server->Post("/memory/read", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const VAddr address = json["address"].get<VAddr>();
            const std::size_t size = json["size"].get<std::size_t>();

            std::vector<u8> data(size);

            // Note: Memory read occurs asynchronously from the state of the emulator
            system.Memory().ReadBlock(*system.Kernel().GetCurrentProcess(), address, &data[0],
                                      size);

            res.set_content(nlohmann::json(data).dump(), "application/json");
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/memory/write", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const VAddr address = json["address"].get<VAddr>();
            const std::vector<u8> data = json["data"].get<std::vector<u8>>();

            // Note: Memory write occurs asynchronously from the state of the emulator
            system.Memory().WriteBlock(*system.Kernel().GetCurrentProcess(), address, &data[0],
                                       data.size());

            system.CPU().InvalidateCacheRange(address, data.size());

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/padstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto hid = Service::HID::GetModule(system);
        const Service::HID::PadState state = hid->GetPadState();

        res.set_content(
            nlohmann::json{
                {"hex", state.hex},
                {"a", static_cast<bool>(state.a)},
                {"b", static_cast<bool>(state.b)},
                {"select", static_cast<bool>(state.select)},
                {"start", static_cast<bool>(state.start)},
                {"right", static_cast<bool>(state.right)},
                {"left", static_cast<bool>(state.left)},
                {"up", static_cast<bool>(state.up)},
                {"down", static_cast<bool>(state.down)},
                {"r", static_cast<bool>(state.r)},
                {"l", static_cast<bool>(state.l)},
                {"x", static_cast<bool>(state.x)},
                {"y", static_cast<bool>(state.y)},
                {"debug", static_cast<bool>(state.debug)},
                {"gpio14", static_cast<bool>(state.gpio14)},
                {"circle_right", static_cast<bool>(state.circle_right)},
                {"circle_left", static_cast<bool>(state.circle_left)},
                {"circle_up", static_cast<bool>(state.circle_up)},
                {"circle_down", static_cast<bool>(state.circle_down)},
            }
                .dump(),
            "application/json");
    });

    server->Post("/padstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            auto hid = Service::HID::GetModule(system);
            const nlohmann::json json = nlohmann::json::parse(req.body);

            if (json.contains("hex")) {
                Service::HID::PadState state;
                state.hex = json["hex"].get<u32>();
                hid->SetCustomPadState(state);

                res.set_content(
                    nlohmann::json{
                        {"a", static_cast<bool>(state.a)},
                        {"b", static_cast<bool>(state.b)},
                        {"select", static_cast<bool>(state.select)},
                        {"start", static_cast<bool>(state.start)},
                        {"right", static_cast<bool>(state.right)},
                        {"left", static_cast<bool>(state.left)},
                        {"up", static_cast<bool>(state.up)},
                        {"down", static_cast<bool>(state.down)},
                        {"r", static_cast<bool>(state.r)},
                        {"l", static_cast<bool>(state.l)},
                        {"x", static_cast<bool>(state.x)},
                        {"y", static_cast<bool>(state.y)},
                        {"debug", static_cast<bool>(state.debug)},
                        {"gpio14", static_cast<bool>(state.gpio14)},
                        {"circle_right", static_cast<bool>(state.circle_right)},
                        {"circle_left", static_cast<bool>(state.circle_left)},
                        {"circle_up", static_cast<bool>(state.circle_up)},
                        {"circle_down", static_cast<bool>(state.circle_down)},
                    }
                        .dump(),
                    "application/json");
            } else {
                hid->SetCustomPadState(std::nullopt);
                res.status = 204;
            }
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/circlepadstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto hid = Service::HID::GetModule(system);
        const auto [x, y] = hid->GetCirclePadState();

        res.set_content(
            nlohmann::json{
                {"x", x},
                {"y", y},
            }
                .dump(),
            "application/json");
    });

    server->Post("/circlepadstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            auto hid = Service::HID::GetModule(system);
            const nlohmann::json json = nlohmann::json::parse(req.body);

            if (json.contains("x") && json.contains("y")) {
                hid->SetCustomCirclePadState(
                    std::make_tuple(json["x"].get<float>(), json["y"].get<float>()));
            } else {
                hid->SetCustomCirclePadState(std::nullopt);
            }

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/touchstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto hid = Service::HID::GetModule(system);
        const auto [x, y, pressed] = hid->GetTouchState();

        res.set_content(
            nlohmann::json{
                {"x", x},
                {"y", y},
                {"pressed", pressed},
            }
                .dump(),
            "application/json");
    });

    server->Post("/touchstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            auto hid = Service::HID::GetModule(system);
            const nlohmann::json json = nlohmann::json::parse(req.body);

            if (json.contains("x") && json.contains("y") && json.contains("pressed")) {
                hid->SetCustomTouchState(std::make_tuple(
                    json["x"].get<float>(), json["y"].get<float>(), json["pressed"].get<bool>()));
            } else {
                hid->SetCustomTouchState(std::nullopt);
            }

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/motionstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto hid = Service::HID::GetModule(system);
        const auto [accel, gyro] = hid->GetMotionState();

        res.set_content(
            nlohmann::json{
                {"accel", accel},
                {"gyro", gyro},
            }
                .dump(),
            "application/json");
    });

    server->Post("/motionstate", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            auto hid = Service::HID::GetModule(system);
            const nlohmann::json json = nlohmann::json::parse(req.body);

            if (json.contains("accel") && json.contains("gyro")) {
                hid->SetCustomMotionState(std::make_tuple(json["accel"].get<Common::Vec3<float>>(),
                                                          json["gyro"].get<Common::Vec3<float>>()));
            } else {
                hid->SetCustomMotionState(std::nullopt);
            }

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/screenshot", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        if (VideoCore::g_renderer == nullptr) {
            res.status = 503;
            res.set_content("booting", "text/plain");
            return;
        }

        const Layout::FramebufferLayout& layout =
            VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout();

        Common::Event done;
        std::vector<u8> data(layout.width * layout.height * 4);
        if (VideoCore::RequestScreenshot(
                data.data(), [&] { done.Set(); }, layout)) {
            res.status = 503;
            res.set_content("another screenshot is pending", "text/plain");
            return;
        }
        done.Wait();

        const auto rotate = [](const std::vector<u8>& input,
                               const Layout::FramebufferLayout& layout) {
            std::vector<u8> output(input.size());

            for (std::size_t i = 0; i < layout.height; i++) {
                for (std::size_t j = 0; j < layout.width; j++) {
                    for (std::size_t k = 0; k < 4; k++) {
                        output[i * (layout.width * 4) + j * 4 + k] =
                            input[(layout.height - i - 1) * (layout.width * 4) + j * 4 + k];
                    }
                }
            }

            return output;
        };

        const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                             const Layout::FramebufferLayout& layout) {
            int offset = 0;
            std::vector<u8> output(input.size());

            for (u32 y = 0; y < layout.height; ++y) {
                for (u32 x = 0; x < layout.width; ++x) {
                    output[offset] = input[offset + 2];
                    output[offset + 1] = input[offset + 1];
                    output[offset + 2] = input[offset];
                    output[offset + 3] = input[offset + 3];

                    offset += 4;
                }
            }

            return output;
        };

        data = convert_bgra_to_rgba(rotate(data, layout), layout);

        std::vector<u8> out;
        stbi_write_func* f = [](void* context, void* data, int size) {
            std::vector<u8>* out = static_cast<std::vector<u8>*>(context);
            out->resize(size);
            std::memcpy(out->data(), data, size);
        };
        if (stbi_write_png_to_func(f, &out, layout.width, layout.height, 4, data.data(),
                                   layout.width * 4) == 0) {
            res.set_content("failed to encode", "text/plain");
        } else {
            res.set_content(reinterpret_cast<const char*>(out.data()), out.size(), "image/png");
        }
    });

    server->Post("/customscreenshot", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        if (VideoCore::g_renderer == nullptr) {
            res.status = 503;
            res.set_content("booting", "text/plain");
            return;
        }

        try {
            const Layout::FramebufferLayout layout = nlohmann::json::parse(req.body);

            Common::Event done;
            std::vector<u8> data(layout.width * layout.height * 4);
            if (VideoCore::RequestScreenshot(
                    data.data(), [&] { done.Set(); }, layout)) {
                res.status = 503;
                res.set_content("another screenshot is pending", "text/plain");
                return;
            }
            done.Wait();

            const auto rotate = [](const std::vector<u8>& input,
                                   const Layout::FramebufferLayout& layout) {
                std::vector<u8> output(input.size());

                for (std::size_t i = 0; i < layout.height; i++) {
                    for (std::size_t j = 0; j < layout.width; j++) {
                        for (std::size_t k = 0; k < 4; k++) {
                            output[i * (layout.width * 4) + j * 4 + k] =
                                input[(layout.height - i - 1) * (layout.width * 4) + j * 4 + k];
                        }
                    }
                }

                return output;
            };

            const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                                 const Layout::FramebufferLayout& layout) {
                int offset = 0;
                std::vector<u8> output(input.size());

                for (u32 y = 0; y < layout.height; ++y) {
                    for (u32 x = 0; x < layout.width; ++x) {
                        output[offset] = input[offset + 2];
                        output[offset + 1] = input[offset + 1];
                        output[offset + 2] = input[offset];
                        output[offset + 3] = input[offset + 3];

                        offset += 4;
                    }
                }

                return output;
            };

            data = convert_bgra_to_rgba(rotate(data, layout), layout);

            std::vector<u8> out;
            stbi_write_func* f = [](void* context, void* data, int size) {
                std::vector<u8>* out = static_cast<std::vector<u8>*>(context);
                out->resize(size);
                std::memcpy(out->data(), data, size);
            };
            if (stbi_write_png_to_func(f, &out, layout.width, layout.height, 4, data.data(),
                                       layout.width * 4) == 0) {
                res.set_content("failed to encode", "text/plain");
            } else {
                res.set_content(reinterpret_cast<const char*>(out.data()), out.size(), "image/png");
            }
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/layout", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        res.set_content(
            nlohmann::json(VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout()).dump(),
            "application/json");
    });

    server->Post("/layout/custom", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.custom_layout = true;
            Settings::values.custom_top_left = json["top_screen"]["left"].get<u16>();
            Settings::values.custom_top_top = json["top_screen"]["top"].get<u16>();
            Settings::values.custom_top_right = json["top_screen"]["right"].get<u16>();
            Settings::values.custom_top_bottom = json["top_screen"]["bottom"].get<u16>();
            Settings::values.custom_bottom_left = json["bottom_screen"]["left"].get<u16>();
            Settings::values.custom_bottom_top = json["bottom_screen"]["top"].get<u16>();
            Settings::values.custom_bottom_right = json["bottom_screen"]["right"].get<u16>();
            Settings::values.custom_bottom_bottom = json["bottom_screen"]["bottom"].get<u16>();
            Settings::Apply();
            Settings::LogSettings();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/layout/default", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::Default;
        Settings::Apply();
        Settings::LogSettings();

        res.status = 204;
    });

    server->Get("/layout/singlescreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::SingleScreen;
        Settings::Apply();
        Settings::LogSettings();

        res.status = 204;
    });

    server->Get("/layout/largescreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::LargeScreen;
        Settings::Apply();
        Settings::LogSettings();

        res.status = 204;
    });

    server->Get("/layout/sidebyside", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::SideScreen;
        Settings::Apply();
        Settings::LogSettings();

        res.status = 204;
    });

    server->Get("/layout/mediumscreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::MediumScreen;
        Settings::Apply();
        Settings::LogSettings();

        res.status = 204;
    });

    server->Post("/layout/swapscreens", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.swap_screen = json["enabled"].get<bool>();
            Settings::Apply();
            Settings::LogSettings();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/layout/upright", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.upright_screen = json["upright"].get<bool>();
            Settings::Apply();
            Settings::LogSettings();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/backgroundcolor", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"red", Settings::values.bg_red},
                {"green", Settings::values.bg_green},
                {"blue", Settings::values.bg_blue},
            }
                .dump(),
            "application/json");
    });

    server->Post("/backgroundcolor", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.bg_red = json["red"].get<float>();
            Settings::values.bg_green = json["green"].get<float>();
            Settings::values.bg_blue = json["blue"].get<float>();
            Settings::Apply();
            Settings::LogSettings();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/speedlimit", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_frame_limit},
                {"percentage", Settings::values.frame_limit},
            }
                .dump(),
            "application/json");
    });

    server->Post("/speedlimit", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_frame_limit = json["enabled"].get<bool>();
            Settings::values.frame_limit = json["percentage"].get<u16>();
            Settings::Apply();
            Settings::LogSettings();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/amiibo", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        std::shared_ptr<Service::NFC::Module::Interface> nfc =
            system.ServiceManager().GetService<Service::NFC::Module::Interface>("nfc:u");
        if (nfc == nullptr) {
            res.status = 500;
            res.set_content("nfc:u is null", "text/plain");
        } else {
            if (req.body.empty()) {
                nfc->RemoveAmiibo();
                res.status = 204;
            } else if (req.body.size() == sizeof(Service::NFC::AmiiboData)) {
                Service::NFC::AmiiboData data;
                std::memcpy(&data, &req.body[0], sizeof(data));
                nfc->LoadAmiibo(data);
                res.status = 204;
            } else {
                res.status = 400;
                res.set_content("invalid body size. the current amiibo is removed if the body is "
                                "empty, or a amiibo is loaded if the body size is 540.",
                                "text/plain");
            }
        }
    });

    server->Get("/3d", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"mode", static_cast<int>(Settings::values.render_3d)},
                {"intensity", Settings::values.factor_3d.load()},
            }
                .dump(),
            "application/json");
    });

    server->Post("/3d", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.render_3d =
                static_cast<Settings::StereoRenderOption>(json["mode"].get<int>());
            Settings::values.factor_3d = json["intensity"].get<u8>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/microphone", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"type", static_cast<int>(Settings::values.mic_input_type)},
                {"device", Settings::values.mic_input_device},
            }
                .dump(),
            "application/json");
    });

    server->Post("/microphone", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.mic_input_type =
                static_cast<Settings::MicInputType>(json["type"].get<int>());
            Settings::values.mic_input_device = json["device"].get<std::string>();
            Settings::Apply();
            Settings::LogSettings();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/resolution", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"resolution", Settings::values.resolution_factor},
            }
                .dump(),
            "application/json");
    });

    server->Post("/resolution", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.resolution_factor = json["resolution"].get<u16>();
            Settings::LogSettings();

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/frameadvancing", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", system.frame_limiter.FrameAdvancingEnabled()},
            }
                .dump(),
            "application/json");
    });

    server->Post("/frameadvancing", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            system.frame_limiter.SetFrameAdvancing(json["enabled"].get<bool>());

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/frameadvancing/advance",
                [&](const httplib::Request& req, httplib::Response& res) {
                    system.frame_limiter.AdvanceFrame();
                    res.status = 204;
                });

    server->Get("/controls", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"buttons", Settings::values.buttons},
                {"analogs", Settings::values.analogs},
                {"motion_device", Settings::values.motion_device},
                {"touch_device", Settings::values.touch_device},
                {"udp_input_address", Settings::values.udp_input_address},
                {"udp_input_port", Settings::values.udp_input_port},
                {"udp_pad_index", Settings::values.udp_pad_index},
            }
                .dump(),
            "application/json");
    });

    server->Post("/controls", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.buttons =
                json["buttons"].get<std::array<std::string, Settings::NativeButton::NumButtons>>();
            Settings::values.analogs =
                json["analogs"].get<std::array<std::string, Settings::NativeAnalog::NumAnalogs>>();
            Settings::values.motion_device = json["motion_device"].get<std::string>();
            Settings::values.touch_device = json["touch_device"].get<std::string>();
            Settings::values.udp_input_address = json["udp_input_address"].get<std::string>();
            Settings::values.udp_input_port = json["udp_input_port"].get<u16>();
            Settings::values.udp_pad_index = json["udp_pad_index"].get<u8>();
            Settings::Apply();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/multiplayerurl", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.multiplayer_url},
            }
                .dump(),
            "application/json");
    });

    server->Post("/multiplayerurl", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.multiplayer_url = json["value"].get<std::string>();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usehardwarerenderer", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_hw_renderer},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usehardwarerenderer", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_hw_renderer = json["enabled"].get<bool>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usehardwareshader", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_hw_shader},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usehardwareshader", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_hw_shader = json["enabled"].get<bool>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/shaderaccuratemultiplication",
                [&](const httplib::Request& req, httplib::Response& res) {
                    res.set_content(
                        nlohmann::json{
                            {"enabled", Settings::values.shaders_accurate_mul},
                        }
                            .dump(),
                        "application/json");
                });

    server->Post("/shaderaccuratemultiplication",
                 [&](const httplib::Request& req, httplib::Response& res) {
                     try {
                         const nlohmann::json json = nlohmann::json::parse(req.body);
                         Settings::values.shaders_accurate_mul = json["enabled"].get<bool>();
                         Settings::Apply();
                         Settings::LogSettings();
                         res.status = 204;
                     } catch (nlohmann::json::exception& exception) {
                         res.status = 500;
                         res.set_content(exception.what(), "text/plain");
                     }
                 });

    server->Get("/useshaderjit", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_shader_jit},
            }
                .dump(),
            "application/json");
    });

    server->Post("/useshaderjit", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_shader_jit = json["enabled"].get<bool>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/filtermode", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"mode", Settings::values.filter_mode ? "linear" : "nearest"},
            }
                .dump(),
            "application/json");
    });

    server->Get("/filtermode/nearest", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.filter_mode = false;
        Settings::Apply();
        Settings::LogSettings();
        res.status = 204;
    });

    server->Get("/filtermode/linear", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.filter_mode = true;
        Settings::Apply();
        Settings::LogSettings();
        res.status = 204;
    });

    server->Get("/postprocessingshader", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"name", Settings::values.pp_shader_name},
            }
                .dump(),
            "application/json");
    });

    server->Post("/postprocessingshader", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.pp_shader_name = json["name"].get<std::string>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/dumptextures", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.dump_textures},
            }
                .dump(),
            "application/json");
    });

    server->Post("/dumptextures", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.dump_textures = json["enabled"].get<bool>();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/customtextures", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.custom_textures},
            }
                .dump(),
            "application/json");
    });

    server->Post("/customtextures", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.custom_textures = json["enabled"].get<bool>();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/preloadcustomtextures", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.preload_textures},
            }
                .dump(),
            "application/json");
    });

    server->Post("/preloadcustomtextures",
                 [&](const httplib::Request& req, httplib::Response& res) {
                     try {
                         const nlohmann::json json = nlohmann::json::parse(req.body);
                         Settings::values.preload_textures = json["enabled"].get<bool>();
                         Settings::LogSettings();
                         res.status = 202;
                     } catch (nlohmann::json::exception& exception) {
                         res.status = 500;
                         res.set_content(exception.what(), "text/plain");
                     }
                 });

    server->Get("/usecpujit", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_cpu_jit},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usecpujit", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_cpu_jit = json["enabled"].get<bool>();
            Settings::LogSettings();
            if (system.IsPoweredOn()) {
                system.RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/dspemulation", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"emulation", Settings::values.enable_dsp_lle ? "lle" : "hle"},
                {"multithreaded",
                 Settings::values.enable_dsp_lle && Settings::values.enable_dsp_lle_multithread},
            }
                .dump(),
            "application/json");
    });

    server->Post("/dspemulation", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.enable_dsp_lle = json["emulation"].get<std::string>() == "lle";
            if (Settings::values.enable_dsp_lle) {
                Settings::values.enable_dsp_lle_multithread = json["multithreaded"].get<bool>();
            }
            Settings::LogSettings();
            if (system.IsPoweredOn()) {
                system.RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/audioengine", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"name", Settings::values.sink_id},
            }
                .dump(),
            "application/json");
    });

    server->Post("/audioengine", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.sink_id = json["name"].get<std::string>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/audiodevice", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.audio_device_id},
            }
                .dump(),
            "application/json");
    });

    server->Post("/audiodevice", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.audio_device_id = json["value"].get<bool>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/audiovolume", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.volume},
            }
                .dump(),
            "application/json");
    });

    server->Post("/audiovolume", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.volume = json["value"].get<float>();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usevirtualsdcard", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_virtual_sd},
            }
                .dump(),
            "application/json");
    });

    server->Post("/usevirtualsdcard", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_virtual_sd = json["enabled"].get<bool>();
            Settings::LogSettings();
            if (system.IsPoweredOn()) {
                system.RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/region", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.region_value},
            }
                .dump(),
            "application/json");
    });

    server->Post("/region", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.region_value = json["value"].get<int>();
            Settings::LogSettings();
            if (system.IsPoweredOn()) {
                system.RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/startclock", [&](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json json;
        switch (Settings::values.init_clock) {
        case Settings::InitClock::SystemTime: {
            json["clock"] = "system";
            break;
        }
        case Settings::InitClock::FixedTime: {
            json["clock"] = "fixed";
            json["unix_timestamp"] = Settings::values.init_time;
            break;
        }
        }
        res.set_content(json.dump(), "application/json");
    });

    server->Post("/startclock", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.init_clock = json["clock"].get<std::string>() == "system"
                                              ? Settings::InitClock::SystemTime
                                              : Settings::InitClock::FixedTime;
            if (Settings::values.init_clock == Settings::InitClock::FixedTime) {
                Settings::values.init_time = json["unix_timestamp"].get<u64>();
            }
            Settings::LogSettings();
            if (system.IsPoweredOn()) {
                system.RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/usevsync", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.enable_vsync},
            }
                .dump(),
            "application/json");
    });

    server->Get("/logfilter", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"value", Settings::values.log_filter},
            }
                .dump(),
            "application/json");
    });

    server->Post("/logfilter", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.log_filter = json["value"].get<std::string>();
            Log::Filter log_filter(Log::Level::Debug);
            log_filter.ParseFilterString(Settings::values.log_filter);
            Log::SetGlobalFilter(log_filter);
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/recordframetimes", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.record_frame_times},
            }
                .dump(),
            "application/json");
    });

    server->Post("/recordframetimes", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.record_frame_times = json["enabled"].get<bool>();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/cameras", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"name", Settings::values.camera_name},
                {"config", Settings::values.camera_config},
                {"flip", Settings::values.camera_flip},
            }
                .dump(),
            "application/json");
    });

    server->Post("/cameras", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.camera_name =
                json["name"].get<std::array<std::string, Service::CAM::NumCameras>>();
            Settings::values.camera_config =
                json["config"].get<std::array<std::string, Service::CAM::NumCameras>>();
            Settings::values.camera_flip =
                json["flip"].get<std::array<int, Service::CAM::NumCameras>>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/gdbstub", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_gdbstub},
                {"port", Settings::values.gdbstub_port},
            }
                .dump(),
            "application/json");
    });

    server->Post("/gdbstub", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_gdbstub = json["enabled"].get<bool>();
            Settings::values.gdbstub_port = json["port"].get<u16>();
            Settings::Apply();
            Settings::LogSettings();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/llemodules", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(nlohmann::json(Settings::values.lle_modules).dump(), "application/json");
    });

    server->Post("/llemodules", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.lle_modules = json.get<std::unordered_map<std::string, bool>>();
            Settings::LogSettings();
            if (system.IsPoweredOn()) {
                system.RequestReset();
            }
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/movie", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"playing", Core::Movie::GetInstance().IsPlayingInput()},
                {"recording", Core::Movie::GetInstance().IsRecordingInput()},
            }
                .dump(),
            "application/json");
    });

    server->Get("/movie/stop", [&](const httplib::Request& req, httplib::Response& res) {
        Core::Movie::GetInstance().Shutdown();
        res.status = 204;
    });

    server->Post("/movie/play", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string file = json["file"].get<std::string>();
            Core::Movie::GetInstance().StartPlayback(file, [] {});
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/movie/record", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string file = json["file"].get<std::string>();
            Core::Movie::GetInstance().StartRecording(file);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/boot", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string file = json["file"].get<std::string>();
            system.SetResetFilePath(file);
            system.RequestReset();
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/installciafile", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string file = json["file"].get<std::string>();
            const auto status = Service::AM::InstallCIA(file);
            if (status == Service::AM::InstallStatus::Success) {
                res.status = 204;
            } else {
                res.status = 500;
                res.set_content(std::to_string(static_cast<int>(status)), "text/plain");
            }
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/cheats", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        nlohmann::json json = nlohmann::json::array();
        const auto cheats = system.CheatEngine().GetCheats();

        for (std::size_t i = 0; i < cheats.size(); ++i) {
            const auto& cheat = cheats[i];

            json.push_back(nlohmann::json{
                {"name", cheat->GetName()},
                {"type", cheat->GetType()},
                {"code", cheat->GetCode()},
                {"comments", cheat->GetComments()},
                {"enabled", cheat->IsEnabled()},
                {"index", i},
            });
        }

        res.set_content(json.dump(), "application/json");
    });

    server->Get("/reloadcheats", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        system.CheatEngine().LoadCheatFile();
        res.status = 204;
    });

    server->Get("/savecheats", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        system.CheatEngine().SaveCheatFile();
        res.status = 204;
    });

    server->Post("/addcheat", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const std::string name = json["name"].get<std::string>();
            const std::string type = json["type"].get<std::string>();
            const std::string code = json["code"].get<std::string>();
            const std::string comments = json["comments"].get<std::string>();
            const bool enabled = json["enabled"].get<bool>();
            if (type == "Gateway") {
                auto cheat = std::make_shared<Cheats::GatewayCheat>(name, code, comments);
                cheat->SetEnabled(enabled);
                system.CheatEngine().AddCheat(cheat);
                res.status = 204;
            } else {
                res.status = 400;
                res.set_content("invalid type", "text/plain");
            }
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/removecheat", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const int index = json["index"].get<int>();
            const auto cheats = system.CheatEngine().GetCheats();
            if (index < 0 || index >= static_cast<int>(cheats.size())) {
                res.status = 400;
                res.set_content("invalid index", "text/plain");
                return;
            }
            system.CheatEngine().RemoveCheat(index);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/updatecheat", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const int index = json["index"].get<int>();
            const auto cheats = system.CheatEngine().GetCheats();
            if (index < 0 || index >= static_cast<int>(cheats.size())) {
                res.status = 400;
                res.set_content("invalid index", "text/plain");
                return;
            }
            const std::string name = json["name"].get<std::string>();
            const std::string type = json["type"].get<std::string>();
            const std::string code = json["code"].get<std::string>();
            const std::string comments = json["comments"].get<std::string>();
            const bool enabled = json["enabled"].get<bool>();
            if (type == "Gateway") {
                auto cheat = std::make_shared<Cheats::GatewayCheat>(name, code, comments);
                cheat->SetEnabled(enabled);
                system.CheatEngine().UpdateCheat(index, cheat);
                res.status = 204;
            } else {
                res.status = 400;
                res.set_content("invalid type", "text/plain");
            }
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/pause", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        paused = true;
        res.status = 204;
    });

    server->Get("/continue", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        paused = false;
        res.status = 204;
    });

    server->Get("/registers/0-15", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        res.set_content(
            nlohmann::json(std::vector<u32>{system.CPU().GetReg(0), system.CPU().GetReg(1),
                                            system.CPU().GetReg(2), system.CPU().GetReg(3),
                                            system.CPU().GetReg(4), system.CPU().GetReg(5),
                                            system.CPU().GetReg(6), system.CPU().GetReg(7),
                                            system.CPU().GetReg(8), system.CPU().GetReg(9),
                                            system.CPU().GetReg(10), system.CPU().GetReg(11),
                                            system.CPU().GetReg(12), system.CPU().GetReg(13),
                                            system.CPU().GetReg(14), system.CPU().GetReg(15)})
                .dump(),
            "application/json");
    });

    server->Post("/registers/0-15", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const int index = json["index"].get<int>();
            const u32 value = json["value"].get<u32>();
            system.CPU().SetReg(index, value);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/registers/cpsr", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        res.set_content(std::to_string(system.CPU().GetCPSR()), "text/plain");
    });

    server->Post("/registers/cpsr", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const u32 value = json["value"].get<u32>();
            system.CPU().SetCPSR(value);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/registers/vfp", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        res.set_content(
            nlohmann::json(std::vector<u32>{system.CPU().GetVFPReg(0),  system.CPU().GetVFPReg(1),
                                            system.CPU().GetVFPReg(2),  system.CPU().GetVFPReg(3),
                                            system.CPU().GetVFPReg(4),  system.CPU().GetVFPReg(5),
                                            system.CPU().GetVFPReg(8),  system.CPU().GetVFPReg(9),
                                            system.CPU().GetVFPReg(10), system.CPU().GetVFPReg(11),
                                            system.CPU().GetVFPReg(12), system.CPU().GetVFPReg(13),
                                            system.CPU().GetVFPReg(14), system.CPU().GetVFPReg(15),
                                            system.CPU().GetVFPReg(16), system.CPU().GetVFPReg(17),
                                            system.CPU().GetVFPReg(18), system.CPU().GetVFPReg(19),
                                            system.CPU().GetVFPReg(20), system.CPU().GetVFPReg(21),
                                            system.CPU().GetVFPReg(22), system.CPU().GetVFPReg(23),
                                            system.CPU().GetVFPReg(24), system.CPU().GetVFPReg(25),
                                            system.CPU().GetVFPReg(26), system.CPU().GetVFPReg(27),
                                            system.CPU().GetVFPReg(28), system.CPU().GetVFPReg(29),
                                            system.CPU().GetVFPReg(30), system.CPU().GetVFPReg(31)})
                .dump(),
            "application/json");
    });

    server->Post("/registers/vfp", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const int index = json["index"].get<int>();
            const u32 value = json["value"].get<u32>();
            system.CPU().SetVFPReg(index, value);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/registers/vfpsystem", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        res.set_content(nlohmann::json(std::vector<u32>{system.CPU().GetVFPSystemReg(VFP_FPSID),
                                                        system.CPU().GetVFPSystemReg(VFP_FPSCR),
                                                        system.CPU().GetVFPSystemReg(VFP_FPEXC),
                                                        system.CPU().GetVFPSystemReg(VFP_FPINST),
                                                        system.CPU().GetVFPSystemReg(VFP_FPINST2),
                                                        system.CPU().GetVFPSystemReg(VFP_MVFR0),
                                                        system.CPU().GetVFPSystemReg(VFP_MVFR1)})
                            .dump(),
                        "application/json");
    });

    server->Post("/registers/vfpsystem", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const int index = json["index"].get<int>();
            const u32 value = json["value"].get<u32>();
            system.CPU().SetVFPSystemReg(static_cast<VFPSystemRegister>(index), value);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/registers/cp15", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        res.set_content(
            nlohmann::json(
                std::vector<u32>{
                    system.CPU().GetCP15Register(CP15_MAIN_ID),
                    system.CPU().GetCP15Register(CP15_CACHE_TYPE),
                    system.CPU().GetCP15Register(CP15_TCM_STATUS),
                    system.CPU().GetCP15Register(CP15_TLB_TYPE),
                    system.CPU().GetCP15Register(CP15_CPU_ID),
                    system.CPU().GetCP15Register(CP15_PROCESSOR_FEATURE_0),
                    system.CPU().GetCP15Register(CP15_PROCESSOR_FEATURE_1),
                    system.CPU().GetCP15Register(CP15_DEBUG_FEATURE_0),
                    system.CPU().GetCP15Register(CP15_AUXILIARY_FEATURE_0),
                    system.CPU().GetCP15Register(CP15_MEMORY_MODEL_FEATURE_0),
                    system.CPU().GetCP15Register(CP15_MEMORY_MODEL_FEATURE_1),
                    system.CPU().GetCP15Register(CP15_MEMORY_MODEL_FEATURE_2),
                    system.CPU().GetCP15Register(CP15_MEMORY_MODEL_FEATURE_3),
                    system.CPU().GetCP15Register(CP15_ISA_FEATURE_0),
                    system.CPU().GetCP15Register(CP15_ISA_FEATURE_1),
                    system.CPU().GetCP15Register(CP15_ISA_FEATURE_2),
                    system.CPU().GetCP15Register(CP15_ISA_FEATURE_3),
                    system.CPU().GetCP15Register(CP15_ISA_FEATURE_4),
                    system.CPU().GetCP15Register(CP15_CONTROL),
                    system.CPU().GetCP15Register(CP15_AUXILIARY_CONTROL),
                    system.CPU().GetCP15Register(CP15_COPROCESSOR_ACCESS_CONTROL),
                    system.CPU().GetCP15Register(CP15_TRANSLATION_BASE_TABLE_0),
                    system.CPU().GetCP15Register(CP15_TRANSLATION_BASE_TABLE_1),
                    system.CPU().GetCP15Register(CP15_TRANSLATION_BASE_CONTROL),
                    system.CPU().GetCP15Register(CP15_DOMAIN_ACCESS_CONTROL),
                    system.CPU().GetCP15Register(CP15_RESERVED),
                    system.CPU().GetCP15Register(CP15_FAULT_STATUS),
                    system.CPU().GetCP15Register(CP15_INSTR_FAULT_STATUS),
                    system.CPU().GetCP15Register(CP15_INST_FSR),
                    system.CPU().GetCP15Register(CP15_FAULT_ADDRESS),
                    system.CPU().GetCP15Register(CP15_WFAR),
                    system.CPU().GetCP15Register(CP15_IFAR),
                    system.CPU().GetCP15Register(CP15_WAIT_FOR_INTERRUPT),
                    system.CPU().GetCP15Register(CP15_PHYS_ADDRESS),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_INSTR_CACHE),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_INSTR_CACHE_USING_MVA),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_INSTR_CACHE_USING_INDEX),
                    system.CPU().GetCP15Register(CP15_FLUSH_PREFETCH_BUFFER),
                    system.CPU().GetCP15Register(CP15_FLUSH_BRANCH_TARGET_CACHE),
                    system.CPU().GetCP15Register(CP15_FLUSH_BRANCH_TARGET_CACHE_ENTRY),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_DATA_CACHE),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_DATA_CACHE_LINE_USING_MVA),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_DATA_CACHE_LINE_USING_INDEX),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_DATA_AND_INSTR_CACHE),
                    system.CPU().GetCP15Register(CP15_CLEAN_DATA_CACHE),
                    system.CPU().GetCP15Register(CP15_CLEAN_DATA_CACHE_LINE_USING_MVA),
                    system.CPU().GetCP15Register(CP15_CLEAN_DATA_CACHE_LINE_USING_INDEX),
                    system.CPU().GetCP15Register(CP15_DATA_SYNC_BARRIER),
                    system.CPU().GetCP15Register(CP15_DATA_MEMORY_BARRIER),
                    system.CPU().GetCP15Register(CP15_CLEAN_AND_INVALIDATE_DATA_CACHE),
                    system.CPU().GetCP15Register(
                        CP15_CLEAN_AND_INVALIDATE_DATA_CACHE_LINE_USING_MVA),
                    system.CPU().GetCP15Register(
                        CP15_CLEAN_AND_INVALIDATE_DATA_CACHE_LINE_USING_INDEX),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_ITLB),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_ITLB_SINGLE_ENTRY),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_ITLB_ENTRY_ON_ASID_MATCH),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_ITLB_ENTRY_ON_MVA),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_DTLB),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_DTLB_SINGLE_ENTRY),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_DTLB_ENTRY_ON_ASID_MATCH),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_DTLB_ENTRY_ON_MVA),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_UTLB),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_UTLB_SINGLE_ENTRY),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_UTLB_ENTRY_ON_ASID_MATCH),
                    system.CPU().GetCP15Register(CP15_INVALIDATE_UTLB_ENTRY_ON_MVA),
                    system.CPU().GetCP15Register(CP15_DATA_CACHE_LOCKDOWN),
                    system.CPU().GetCP15Register(CP15_TLB_LOCKDOWN),
                    system.CPU().GetCP15Register(CP15_PRIMARY_REGION_REMAP),
                    system.CPU().GetCP15Register(CP15_NORMAL_REGION_REMAP),
                    system.CPU().GetCP15Register(CP15_PID),
                    system.CPU().GetCP15Register(CP15_CONTEXT_ID),
                    system.CPU().GetCP15Register(CP15_THREAD_UPRW),
                    system.CPU().GetCP15Register(CP15_THREAD_URO),
                    system.CPU().GetCP15Register(CP15_THREAD_PRW),
                    system.CPU().GetCP15Register(CP15_PERFORMANCE_MONITOR_CONTROL),
                    system.CPU().GetCP15Register(CP15_CYCLE_COUNTER),
                    system.CPU().GetCP15Register(CP15_COUNT_0),
                    system.CPU().GetCP15Register(CP15_COUNT_1),
                    system.CPU().GetCP15Register(CP15_READ_MAIN_TLB_LOCKDOWN_ENTRY),
                    system.CPU().GetCP15Register(CP15_WRITE_MAIN_TLB_LOCKDOWN_ENTRY),
                    system.CPU().GetCP15Register(CP15_MAIN_TLB_LOCKDOWN_VIRT_ADDRESS),
                    system.CPU().GetCP15Register(CP15_MAIN_TLB_LOCKDOWN_PHYS_ADDRESS),
                    system.CPU().GetCP15Register(CP15_MAIN_TLB_LOCKDOWN_ATTRIBUTE),
                    system.CPU().GetCP15Register(CP15_TLB_DEBUG_CONTROL),
                    system.CPU().GetCP15Register(CP15_TLB_FAULT_ADDR),
                    system.CPU().GetCP15Register(CP15_TLB_FAULT_STATUS)})
                .dump(),
            "application/json");
    });

    server->Post("/registers/cp15", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const int index = json["index"].get<int>();
            const u32 value = json["value"].get<u32>();
            system.CPU().SetCP15Register(static_cast<CP15Register>(index), value);
            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/restart", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        system.RequestReset();
        res.status = 204;
    });

    server->Get("/reloadcameras", [&](const httplib::Request& req, httplib::Response& res) {
        if (!system.IsPoweredOn()) {
            res.status = 503;
            res.set_content("emulation not running", "text/plain");
            return;
        }

        auto cam = Service::CAM::GetModule(system);
        if (cam != nullptr) {
            cam->ReloadCameraDevices();
        }

        res.status = 204;
    });

    server->Get("/texturefilter", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(Settings::values.texture_filter_name, "text/plain");
    });

    server->Post("/texturefilter", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.texture_filter_name = req.body;
        Settings::Apply();
        Settings::LogSettings();
        res.status = 204;
    });

    server->Get("/usecustomcputicks", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(Settings::values.use_custom_cpu_ticks ? "true" : "false", "text/plain");
    });

    server->Post("/usecustomcputicks", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.use_custom_cpu_ticks = req.body == "true";
        Settings::LogSettings();
        res.status = 204;
    });

    server->Get("/customcputicks", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(std::to_string(Settings::values.custom_cpu_ticks), "text/plain");
    });

    server->Post("/customcputicks", [&](const httplib::Request& req, httplib::Response& res) {
        std::istringstream iss(req.body);
        iss >> Settings::values.custom_cpu_ticks;
        Settings::LogSettings();
        res.status = 204;
    });

    server->Get("/cpuclockpercentage", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(std::to_string(Settings::values.cpu_clock_percentage), "text/plain");
    });

    server->Post("/cpuclockpercentage", [&](const httplib::Request& req, httplib::Response& res) {
        std::istringstream iss(req.body);
        iss >> Settings::values.cpu_clock_percentage;
        Settings::LogSettings();
        res.status = 204;
    });

    request_handler_thread = std::thread([this, port] { server->listen("0.0.0.0", port); });
    LOG_INFO(RPC_Server, "RPC server running on port {}", port);
}

Server::~Server() {
    server->stop();
    request_handler_thread.join();
    LOG_INFO(RPC_Server, "RPC server stopped");
}

} // namespace RPC
