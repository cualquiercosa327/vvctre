// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <thread>

namespace Core {
class System;
} // namespace Core

namespace httplib {
class Server;
} // namespace httplib

namespace RPC {

class Server {
public:
    Server(Core::System& system, const int port);
    ~Server();

private:
    std::unique_ptr<httplib::Server> server;
    std::thread request_handler_thread;
};

} // namespace RPC
