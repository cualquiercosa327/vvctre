// Copyright 2020 vvctre emulator project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <httplib.h>
#include <json.hpp>
#include "common/logging/log.h"
#include "common/version.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"
#include "core/rpc/rpc_server.h"

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

    request_handler_thread = std::thread([&] { server->listen("0.0.0.0", RPC_PORT); });
    LOG_INFO(RPC_Server, "RPC server running on port {}", RPC_PORT);
}

RPCServer::~RPCServer() {
    server->stop();
    request_handler_thread.join();
    LOG_INFO(RPC_Server, "RPC server stopped");
}

} // namespace RPC
