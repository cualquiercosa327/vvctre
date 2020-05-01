// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <exception>
#include <optional>
#include <sstream>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_ncch.h"
#include "core/hle/service/fs/archive.h"
#include "core/hw/aes/arithmetic128.h"
#include "core/hw/aes/key.h"
#include "core/hw/rsa/rsa.h"

namespace HW::AES {

namespace {

// The generator constant was calculated using the 0x39 KeyX and KeyY retrieved from a 3DS and the
// normal key dumped from a Wii U solving the equation:
// NormalKey = (((KeyX ROL 2) XOR KeyY) + constant) ROL 87
// On a real 3DS the generation for the normal key is hardware based, and thus the constant can't
// get dumped. Generated normal keys are also not accesible on a 3DS. The used formula for
// calculating the constant is a software implementation of what the hardware generator does.
constexpr AESKey generator_constant = {{0x1F, 0xF9, 0xE9, 0xAA, 0xC5, 0xFE, 0x04, 0x08, 0x02, 0x45,
                                        0x91, 0xDC, 0x5D, 0x52, 0x76, 0x8A}};

struct KeyDesc {
    char key_type;
    std::size_t slot_id;
    // This key is identical to the key with the same key_type and slot_id -1
    bool same_as_before;
};

AESKey HexToKey(const std::string& hex) {
    if (hex.size() < 32) {
        throw std::invalid_argument("hex string is too short");
    }

    AESKey key;
    for (std::size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<u8>(std::stoi(hex.substr(i * 2, 2), 0, 16));
    }

    return key;
}

struct KeySlot {
    std::optional<AESKey> x;
    std::optional<AESKey> y;
    std::optional<AESKey> normal;

    void SetKeyX(std::optional<AESKey> key) {
        x = key;
        GenerateNormalKey();
    }

    void SetKeyY(std::optional<AESKey> key) {
        y = key;
        GenerateNormalKey();
    }

    void SetNormalKey(std::optional<AESKey> key) {
        normal = key;
    }

    void GenerateNormalKey() {
        if (x && y) {
            normal = Lrot128(Add128(Xor128(Lrot128(*x, 2), *y), generator_constant), 87);
        } else {
            normal = {};
        }
    }

    void Clear() {
        x.reset();
        y.reset();
        normal.reset();
    }
};

std::array<KeySlot, KeySlotID::MaxKeySlotID> key_slots;
std::array<std::optional<AESKey>, 6> common_key_y_slots;

enum class FirmwareType : u32 {
    ARM9 = 0,  // uses NDMA
    ARM11 = 1, // uses XDMA
};

struct FirmwareSectionHeader {
    u32_le offset;
    u32_le phys_address;
    u32_le size;
    enum_le<FirmwareType> firmware_type;
    std::array<u8, 0x20> hash; // SHA-256 hash
};

struct FIRM_Header {
    u32_le magic;         // FIRM
    u32_le boot_priority; // Usually 0
    u32_le arm11_entrypoint;
    u32_le arm9_entrypoint;
    INSERT_PADDING_BYTES(0x30);                           // Reserved
    std::array<FirmwareSectionHeader, 4> section_headers; // 1st ARM11?, 3rd ARM9
    std::array<u8, 0x100> signature; // RSA-2048 signature of the FIRM header's hash
};

struct ARM9_Header {
    AESKey enc_key_x;
    AESKey key_y;
    AESKey CTR;
    std::array<u8, 8> size;  // in ASCII
    INSERT_PADDING_BYTES(8); // Unknown
    std::array<u8, 16> control_block;
    std::array<u8, 16> hardware_debug_info;
    std::array<u8, 16> enc_key_x_slot_16;
};

std::string KeyToString(AESKey& key) {
    std::string s;
    for (auto pos : key) {
        s += fmt::format("{:02X}", pos);
    }
    return s;
}

void LoadBootromKeys() {
    constexpr std::array<KeyDesc, 80> keys = {
        {{'X', 0x2C, false}, {'X', 0x2D, true},  {'X', 0x2E, true},  {'X', 0x2F, true},
         {'X', 0x30, false}, {'X', 0x31, true},  {'X', 0x32, true},  {'X', 0x33, true},
         {'X', 0x34, false}, {'X', 0x35, true},  {'X', 0x36, true},  {'X', 0x37, true},
         {'X', 0x38, false}, {'X', 0x39, true},  {'X', 0x3A, true},  {'X', 0x3B, true},
         {'X', 0x3C, false}, {'X', 0x3D, false}, {'X', 0x3E, false}, {'X', 0x3F, false},
         {'Y', 0x4, false},  {'Y', 0x5, false},  {'Y', 0x6, false},  {'Y', 0x7, false},
         {'Y', 0x8, false},  {'Y', 0x9, false},  {'Y', 0xA, false},  {'Y', 0xB, false},
         {'N', 0xC, false},  {'N', 0xD, true},   {'N', 0xE, true},   {'N', 0xF, true},
         {'N', 0x10, false}, {'N', 0x11, true},  {'N', 0x12, true},  {'N', 0x13, true},
         {'N', 0x14, false}, {'N', 0x15, false}, {'N', 0x16, false}, {'N', 0x17, false},
         {'N', 0x18, false}, {'N', 0x19, true},  {'N', 0x1A, true},  {'N', 0x1B, true},
         {'N', 0x1C, false}, {'N', 0x1D, true},  {'N', 0x1E, true},  {'N', 0x1F, true},
         {'N', 0x20, false}, {'N', 0x21, true},  {'N', 0x22, true},  {'N', 0x23, true},
         {'N', 0x24, false}, {'N', 0x25, true},  {'N', 0x26, true},  {'N', 0x27, true},
         {'N', 0x28, true},  {'N', 0x29, false}, {'N', 0x2A, false}, {'N', 0x2B, false},
         {'N', 0x2C, false}, {'N', 0x2D, true},  {'N', 0x2E, true},  {'N', 0x2F, true},
         {'N', 0x30, false}, {'N', 0x31, true},  {'N', 0x32, true},  {'N', 0x33, true},
         {'N', 0x34, false}, {'N', 0x35, true},  {'N', 0x36, true},  {'N', 0x37, true},
         {'N', 0x38, false}, {'N', 0x39, true},  {'N', 0x3A, true},  {'N', 0x3B, true},
         {'N', 0x3C, true},  {'N', 0x3D, false}, {'N', 0x3E, false}, {'N', 0x3F, false}}};

    // Bootrom sets all these keys when executed, but later some of the normal keys get overwritten
    // by other applications e.g. process9. These normal keys thus aren't used by any application
    // and have no value for emulation

    const std::string filepath =
        FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + "boot9.bin";
    auto file = FileUtil::IOFile(filepath, "rb");
    if (!file) {
        return;
    }

    const std::size_t length = file.GetSize();
    if (length != 65536) {
        LOG_ERROR(HW_AES, "Bootrom9 size is wrong: {}", length);
        return;
    }

    constexpr std::size_t KEY_SECTION_START = 55760;
    file.Seek(KEY_SECTION_START, SEEK_SET); // Jump to the key section

    AESKey new_key;
    for (const auto& key : keys) {
        if (!key.same_as_before) {
            file.ReadArray(new_key.data(), new_key.size());
            if (!file) {
                LOG_ERROR(HW_AES, "Reading from Bootrom9 failed");
                return;
            }
        }

        LOG_DEBUG(HW_AES, "Loaded Slot{:#02x} Key{}: {}", key.slot_id, key.key_type,
                  KeyToString(new_key));

        switch (key.key_type) {
        case 'X':
            key_slots.at(key.slot_id).SetKeyX(new_key);
            break;
        case 'Y':
            key_slots.at(key.slot_id).SetKeyY(new_key);
            break;
        case 'N':
            key_slots.at(key.slot_id).SetNormalKey(new_key);
            break;
        default:
            LOG_ERROR(HW_AES, "Invalid key type {}", key.key_type);
            break;
        }
    }
}

void LoadNativeFirmKeys() {
    constexpr u64 native_firm_id = 0x00040138'00000002;
    FileSys::NCCHArchive archive(native_firm_id, Service::FS::MediaType::NAND);
    std::array<char, 8> exefs_filepath = {'.', 'f', 'i', 'r', 'm', 0, 0, 0};
    FileSys::Path file_path = FileSys::MakeNCCHFilePath(
        FileSys::NCCHFileOpenType::NCCHData, 0, FileSys::NCCHFilePathType::ExeFS, exefs_filepath);
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto file_result = archive.OpenFile(file_path, open_mode);
    if (file_result.Failed())
        return;

    auto firm = std::move(file_result).Unwrap();
    const std::size_t size = firm->GetSize();
    if (size != 966656) {
        LOG_ERROR(HW_AES, "native firm has wrong size {}", size);
        return;
    }

    const auto rsa = RSA::GetSlot(0);
    if (!rsa) {
        LOG_ERROR(HW_AES, "RSA slot is missing");
        return;
    }

    std::vector<u8> firm_buffer(size);
    firm->Read(0, firm_buffer.size(), firm_buffer.data());
    firm->Close();

    constexpr std::size_t SLOT_0x25_KEY_X_SECRET_OFFSET = 933480;
    constexpr std::size_t SLOT_0x25_KEY_X_SECRET_SIZE = 64;
    std::vector<u8> secret_data(SLOT_0x25_KEY_X_SECRET_SIZE);
    std::memcpy(secret_data.data(), firm_buffer.data() + SLOT_0x25_KEY_X_SECRET_OFFSET,
                secret_data.size());

    auto asn1 = RSA::CreateASN1Message(secret_data);
    auto result = rsa.GetSignature(asn1);
    if (result.size() < 0x100) {
        std::vector<u8> temp(0x100);
        std::copy(result.begin(), result.end(), temp.end() - result.size());
        result = temp;
    } else if (result.size() > 0x100) {
        result.resize(0x100);
    }

    CryptoPP::SHA256 sha;
    std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash_result;
    sha.CalculateDigest(hash_result.data(), result.data(), result.size());
    AESKey key;
    std::memcpy(key.data(), hash_result.data(), sizeof(key));
    key_slots.at(0x2F).SetKeyY(key);
    std::memcpy(key.data(), hash_result.data() + sizeof(key), sizeof(key));
    key_slots.at(0x25).SetKeyX(key);
}

void LoadSafeModeNativeFirmKeys() {
    // Use the safe mode native firm instead of the normal mode since there are only 2 version of it
    // and thus we can use fixed offsets

    constexpr u64 safe_mode_native_firm_id = 0x00040138'00000003;

    FileSys::NCCHArchive archive(safe_mode_native_firm_id, Service::FS::MediaType::NAND);
    std::array<char, 8> exefs_filepath = {'.', 'f', 'i', 'r', 'm', 0, 0, 0};
    FileSys::Path file_path = FileSys::MakeNCCHFilePath(
        FileSys::NCCHFileOpenType::NCCHData, 0, FileSys::NCCHFilePathType::ExeFS, exefs_filepath);
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto file_result = archive.OpenFile(file_path, open_mode);
    if (file_result.Failed())
        return;

    auto firm = std::move(file_result).Unwrap();
    const std::size_t size = firm->GetSize();
    if (size != 843776) {
        LOG_ERROR(HW_AES, "safe mode native firm has wrong size {}", size);
        return;
    }
    std::vector<u8> firm_buffer(size);
    firm->Read(0, firm_buffer.size(), firm_buffer.data());
    firm->Close();

    AESKey key;
    constexpr std::size_t SLOT_0x31_KEY_Y_OFFSET = 817672;
    std::memcpy(key.data(), firm_buffer.data() + SLOT_0x31_KEY_Y_OFFSET, sizeof(key));
    key_slots.at(0x31).SetKeyY(key);
    LOG_DEBUG(HW_AES, "Loaded Slot0x31 KeyY: {}", KeyToString(key));

    auto LoadCommonKey = [&firm_buffer](std::size_t key_slot) -> AESKey {
        constexpr std::size_t START_OFFSET = 836533;
        constexpr std::size_t OFFSET = 0x14; // 0x10 bytes for key + 4 bytes between keys
        AESKey key;
        std::memcpy(key.data(), firm_buffer.data() + START_OFFSET + OFFSET * key_slot, sizeof(key));
        return key;
    };

    for (std::size_t key_slot{0}; key_slot < 6; ++key_slot) {
        AESKey key = LoadCommonKey(key_slot);
        common_key_y_slots[key_slot] = key;
        LOG_DEBUG(HW_AES, "Loaded common key{}: {}", key_slot, KeyToString(key));
    }
}

void LoadPresetKeys() {
    const std::string filepath =
        FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + "aes_keys.txt";
    FileUtil::CreateFullPath(filepath); // Create path if not already created
    std::ifstream file;
    OpenFStream(file, filepath, std::ios_base::in);
    if (!file) {
        return;
    }

    while (!file.eof()) {
        std::string line;
        std::getline(file, line);
        std::vector<std::string> parts;
        Common::SplitString(line, '=', parts);
        if (parts.size() != 2) {
            LOG_ERROR(HW_AES, "Failed to parse {}", line);
            continue;
        }

        const std::string& name = parts[0];
        AESKey key;
        try {
            key = HexToKey(parts[1]);
        } catch (const std::logic_error& e) {
            LOG_ERROR(HW_AES, "Invalid key {}: {}", parts[1], e.what());
            continue;
        }

        std::size_t common_key_index;
        if (std::sscanf(name.c_str(), "common%zd", &common_key_index) == 1) {
            if (common_key_index >= common_key_y_slots.size()) {
                LOG_ERROR(HW_AES, "Invalid common key index {}", common_key_index);
            } else {
                common_key_y_slots[common_key_index] = key;
            }
            continue;
        }

        std::size_t slot_id;
        char key_type;
        if (std::sscanf(name.c_str(), "slot0x%zXKey%c", &slot_id, &key_type) != 2) {
            LOG_ERROR(HW_AES, "Invalid key name {}", name);
            continue;
        }

        if (slot_id >= MaxKeySlotID) {
            LOG_ERROR(HW_AES, "Out of range slot ID {:#X}", slot_id);
            continue;
        }

        switch (key_type) {
        case 'X':
            key_slots.at(slot_id).SetKeyX(key);
            break;
        case 'Y':
            key_slots.at(slot_id).SetKeyY(key);
            break;
        case 'N':
            key_slots.at(slot_id).SetNormalKey(key);
            break;
        default:
            LOG_ERROR(HW_AES, "Invalid key type {}", key_type);
            break;
        }
    }
}

} // namespace

void InitKeys() {
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    HW::RSA::InitSlots();
    LoadBootromKeys();
    LoadNativeFirmKeys();
    LoadSafeModeNativeFirmKeys();
    LoadPresetKeys();
}

void SetKeyX(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetKeyX(key);
}

void SetKeyY(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetKeyY(key);
}

void SetNormalKey(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetNormalKey(key);
}

bool IsNormalKeyAvailable(std::size_t slot_id) {
    return key_slots.at(slot_id).normal.has_value();
}

AESKey GetNormalKey(std::size_t slot_id) {
    return key_slots.at(slot_id).normal.value_or(AESKey{});
}

void SelectCommonKeyIndex(u8 index) {
    key_slots[KeySlotID::TicketCommonKey].SetKeyY(common_key_y_slots.at(index));
}

} // namespace HW::AES
