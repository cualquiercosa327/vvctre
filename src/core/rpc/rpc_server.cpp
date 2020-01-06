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

    request_handler_thread = std::thread([&] { server->listen("0.0.0.0", RPC_PORT); });
    LOG_INFO(RPC_Server, "RPC server running on port {}", RPC_PORT);
}

RPCServer::~RPCServer() {
    server->stop();
    request_handler_thread.join();
    LOG_INFO(RPC_Server, "RPC server stopped");
}

} // namespace RPC
