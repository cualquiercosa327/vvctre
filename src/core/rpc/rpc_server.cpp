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
        res.set_content(nlohmann::json{
            {"vvctre", version::vvctre.to_string()},
            {"movie", version::movie},
            {"shader_cache", version::shader_cache},
        }
                            .dump());
    });

    request_handler_thread = std::thread([&] { server->listen("0.0.0.0", RPC_PORT); });
    LOG_INFO(RPC_Server, "RPC server running on port {}", RPC_PORT);
}

RPCServer::~RPCServer() {
    server->stop();
    LOG_INFO(RPC_Server, "RPC server stopped");
}

// void RPCServer::HandleReadMemory(Packet& packet, u32 address, u32 data_size) {
//     if (data_size > MAX_READ_SIZE) {
//         return;
//     }

//     // Note: Memory read occurs asynchronously from the state of the emulator
//     Core::System::GetInstance().Memory().ReadBlock(
//         *Core::System::GetInstance().Kernel().GetCurrentProcess(), address,
//         packet.GetPacketData().data(), data_size);
//     packet.SetPacketDataSize(data_size);
//     packet.SendReply();
// }

// void RPCServer::HandleWriteMemory(Packet& packet, u32 address, const u8* data, u32 data_size) {
//     // Only allow writing to certain memory regions
//     if ((address >= Memory::PROCESS_IMAGE_VADDR && address <= Memory::PROCESS_IMAGE_VADDR_END) ||
//         (address >= Memory::HEAP_VADDR && address <= Memory::HEAP_VADDR_END) ||
//         (address >= Memory::N3DS_EXTRA_RAM_VADDR && address <= Memory::N3DS_EXTRA_RAM_VADDR_END))
//         {
//         // Note: Memory write occurs asynchronously from the state of the emulator
//         Core::System::GetInstance().Memory().WriteBlock(
//             *Core::System::GetInstance().Kernel().GetCurrentProcess(), address, data, data_size);
//         // If the memory happens to be executable code, make sure the changes become visible
//         Core::CPU().InvalidateCacheRange(address, data_size);
//     }
//     packet.SetPacketDataSize(0);
//     packet.SendReply();
// }

} // namespace RPC
