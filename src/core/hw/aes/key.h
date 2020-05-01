// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include "common/common_types.h"

namespace HW::AES {

enum KeySlotID : std::size_t {
    // Used to decrypt the SSL client cert/private-key stored in ClCertA.
    SSLKey = 0x0D,

    // AES keyslots used to decrypt NCCH
    NCCHSecure1 = 0x2C,
    NCCHSecure2 = 0x25,
    NCCHSecure3 = 0x18,
    NCCHSecure4 = 0x1B,

    // AES Keyslot used to generate the UDS data frame CCMP key.
    UDSDataKey = 0x2D,

    // AES Keyslot used to encrypt the BOSS container data.
    BOSSDataKey = 0x38,

    // AES Keyslot used to calculate DLP data frame checksum.
    DLPDataKey = 0x39,

    // AES Keyslot used to generate the StreetPass CCMP key.
    CECDDataKey = 0x2E,

    // AES Keyslot used by the friends module.
    FRDKey = 0x36,

    // AES Keyslot used by the NFC module.
    NFCKey = 0x39,

    // AES keyslot used for APT:Wrap/Unwrap functions
    APTWrap = 0x31,

    // AES keyslot used for decrypting ticket title key
    TicketCommonKey = 0x3D,

    MaxKeySlotID = 0x40,
};

constexpr std::size_t AES_BLOCK_SIZE = 16;

using AESKey = std::array<u8, AES_BLOCK_SIZE>;

void InitKeys();

void SetGeneratorConstant(const AESKey& key);
void SetKeyX(std::size_t slot_id, const AESKey& key);
void SetKeyY(std::size_t slot_id, const AESKey& key);
void SetNormalKey(std::size_t slot_id, const AESKey& key);

bool IsNormalKeyAvailable(std::size_t slot_id);
AESKey GetNormalKey(std::size_t slot_id);

void SelectCommonKeyIndex(u8 index);

} // namespace HW::AES
