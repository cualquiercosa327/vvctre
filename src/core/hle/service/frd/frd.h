// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::FRD {

struct FriendKey {
    u32 friend_id;
    u32 unknown;
    u64 friend_code;
};

struct MyPresence {
    u8 unknown[0x12C];
};

struct Profile {
    u8 region;
    u8 country;
    u8 area;
    u8 language;
    u32 unknown;
};

class Module final {
public:
    Module();
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> frd, const char* name, u32 max_session);
        ~Interface();

    protected:
        void GetMyPresence(Kernel::HLERequestContext& ctx);
        void GetFriendKeyList(Kernel::HLERequestContext& ctx);
        void GetFriendProfile(Kernel::HLERequestContext& ctx);
        void GetFriendAttributeFlags(Kernel::HLERequestContext& ctx);
        void GetMyFriendKey(Kernel::HLERequestContext& ctx);
        void GetMyScreenName(Kernel::HLERequestContext& ctx);
        void UnscrambleLocalFriendCode(Kernel::HLERequestContext& ctx);
        void SetClientSdkVersion(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> frd;
    };

private:
    FriendKey my_friend_key = {0, 0, 0ull};
    MyPresence my_presence = {};
};

void InstallInterfaces(Core::System& system);

} // namespace Service::FRD
