// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <httplib.h>
#include <json.hpp>
#include <lodepng.h>
#include "common/logging/log.h"
#include "common/thread.h"
#include "common/version.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/nfc/nfc_u.h"
#include "core/memory.h"
#include "core/rpc/rpc_server.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace RPC {

constexpr int RPC_PORT = 47889;

RPCServer::RPCServer() {
    server = std::make_unique<httplib::Server>();

    server->Get("/version", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"vvctre", version::vvctre.to_string()},
                {"movie", version::movie},
                {"shader_cache", version::shader_cache},
            }
                .dump(),
            "application/json");
    });

    server->Post("/memory/read", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const VAddr address = json["address"].get<VAddr>();
            const std::size_t size = json["size"].get<std::size_t>();

            std::vector<u8> data(size);

            // Note: Memory read occurs asynchronously from the state of the emulator
            Core::System::GetInstance().Memory().ReadBlock(
                *Core::System::GetInstance().Kernel().GetCurrentProcess(), address, &data[0], size);

            res.set_content(nlohmann::json(data).dump(), "application/json");
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/memory/write", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            const VAddr address = json["address"].get<VAddr>();
            const std::vector<u8> data = json["data"].get<std::vector<u8>>();

            // Note: Memory write occurs asynchronously from the state of the emulator
            Core::System::GetInstance().Memory().WriteBlock(
                *Core::System::GetInstance().Kernel().GetCurrentProcess(), address, &data[0],
                data.size());

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/padstate", [&](const httplib::Request& req, httplib::Response& res) {
        auto hid = Service::HID::GetModule(Core::System::GetInstance());
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
        try {
            auto hid = Service::HID::GetModule(Core::System::GetInstance());
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
        auto hid = Service::HID::GetModule(Core::System::GetInstance());
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
        try {
            auto hid = Service::HID::GetModule(Core::System::GetInstance());
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
        auto hid = Service::HID::GetModule(Core::System::GetInstance());
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
        try {
            auto hid = Service::HID::GetModule(Core::System::GetInstance());
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

    server->Get("/screenshot", [&](const httplib::Request& req, httplib::Response& res) {
        const Layout::FramebufferLayout& layout =
            VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout();

        Common::Event done;
        std::vector<u8> data(layout.width * layout.height * 4);
        VideoCore::RequestScreenshot(data.data(), [&] { done.Set(); }, layout);
        done.Wait();

        // Rotate the image to put the pixels in correct order
        // (As OpenGL returns pixel data starting from the lowest position)
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

            for (int y = 0; y < layout.height; y++) {
                for (int x = 0; x < layout.width; x++) {
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
        const u32 result = lodepng::encode(out, data, layout.width, layout.height);
        if (result) {
            res.status = 500;
            res.set_content(lodepng_error_text(result), "text/plain");
        } else {
            res.set_content(reinterpret_cast<const char*>(out.data()), out.size(), "image/png");
        }
    });

    server->Get("/layout", [&](const httplib::Request& req, httplib::Response& res) {
        const Layout::FramebufferLayout& layout =
            VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout();
        res.set_content(
            nlohmann::json{
                {"width", layout.width},
                {"height", layout.height},
                {"top_screen",
                 {
                     {"width", layout.top_screen.GetWidth()},
                     {"height", layout.top_screen.GetHeight()},
                     {"left", layout.top_screen.left},
                     {"top", layout.top_screen.top},
                     {"right", layout.top_screen.right},
                     {"bottom", layout.top_screen.bottom},
                 }},
                {"bottom_screen",
                 {
                     {"width", layout.bottom_screen.GetWidth()},
                     {"height", layout.bottom_screen.GetHeight()},
                     {"left", layout.bottom_screen.left},
                     {"top", layout.bottom_screen.top},
                     {"right", layout.bottom_screen.right},
                     {"bottom", layout.bottom_screen.bottom},
                 }},
            }
                .dump(),
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

        res.status = 204;
    });

    server->Get("/layout/singlescreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::SingleScreen;
        Settings::Apply();

        res.status = 204;
    });

    server->Get("/layout/largescreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::LargeScreen;
        Settings::Apply();

        res.status = 204;
    });

    server->Get("/layout/sidebyside", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::SideScreen;
        Settings::Apply();

        res.status = 204;
    });

    server->Get("/layout/mediumscreen", [&](const httplib::Request& req, httplib::Response& res) {
        Settings::values.custom_layout = false;
        Settings::values.layout_option = Settings::LayoutOption::MediumScreen;
        Settings::Apply();

        res.status = 204;
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

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Get("/customticks", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            nlohmann::json{
                {"enabled", Settings::values.use_custom_cpu_ticks},
                {"ticks", Settings::values.custom_cpu_ticks},
            }
                .dump(),
            "application/json");
    });

    server->Post("/customticks", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const nlohmann::json json = nlohmann::json::parse(req.body);
            Settings::values.use_custom_cpu_ticks = json["enabled"].get<bool>();
            Settings::values.custom_cpu_ticks = json["ticks"].get<u64>();
            Settings::Apply();

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

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    server->Post("/amiibo", [&](const httplib::Request& req, httplib::Response& res) {
        std::shared_ptr<Service::NFC::Module::Interface> nfc =
            Core::System::GetInstance()
                .ServiceManager()
                .GetService<Service::NFC::Module::Interface>("nfc:u");
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

            res.status = 204;
        } catch (nlohmann::json::exception& exception) {
            res.status = 500;
            res.set_content(exception.what(), "text/plain");
        }
    });

    request_handler_thread = std::thread([&] { server->listen("0.0.0.0", RPC_PORT); });
    LOG_INFO(RPC_Server, "RPC server running on port {}", RPC_PORT);
}

RPCServer::~RPCServer() {
    server->stop();
    request_handler_thread.join();
    LOG_INFO(RPC_Server, "RPC server stopped");
}

} // namespace RPC
