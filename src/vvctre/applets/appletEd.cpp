// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iostream>
#include <sstream>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/service/ptm/ptm.h"
#include "vvctre/applets/appletEd.h"

namespace Frontend {

void SDL2_MiiSelector::Setup(const MiiSelectorConfig& config) {
    MiiSelector::Setup(config);

    const std::string nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory, true);

    std::vector<HLE::Applets::MiiData> miis{
        HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data};

    auto archive_result = extdata_archive_factory.Open(Service::PTM::ptm_shared_extdata_id, 0);
    if (archive_result.Succeeded()) {
        auto archive = std::move(archive_result).Unwrap();

        FileSys::Path file_path = "/CFL_DB.dat";
        FileSys::Mode mode{};
        mode.read_flag.Assign(1);

        auto file_result = archive->OpenFile(file_path, mode);
        if (file_result.Succeeded()) {
            auto file = std::move(file_result).Unwrap();
            u32 saved_miis_offset = 0x8;

            // The Mii Maker has a 100 Mii limit on the 3DS
            for (int i = 0; i < 100; ++i) {
                HLE::Applets::MiiData mii;
                file->Read(saved_miis_offset, sizeof(mii), reinterpret_cast<u8*>(&mii));
                if (mii.mii_id != 0) {
                    miis.push_back(mii);
                }
                saved_miis_offset += sizeof(mii);
            }
        }
    }

    LOG_INFO(Applet, "Miis:");

    for (std::size_t index = 0; index < miis.size(); ++index) {
        LOG_INFO(Applet, "{} {}", index, Common::UTF16BufferToUTF8(miis[index].mii_name));
    }

    std::size_t index = miis.size();
    std::string button, line;

    while (index >= miis.size() || (config.enable_cancel_button && button != MII_BUTTON_CANCEL &&
                                    button != MII_BUTTON_OKAY)) {
        if (config.enable_cancel_button) {
            LOG_INFO(Applet, "{}. enter the number of a Mii, and then {} or {}.",
                     config.title.empty() ? "Mii Selector" : config.title, MII_BUTTON_CANCEL,
                     MII_BUTTON_OKAY);
        } else {
            LOG_INFO(Applet, "{}. enter the number of a Mii.",
                     config.title.empty() ? "Mii Selector" : config.title);
        }

        std::getline(std::cin, line);
        std::istringstream iss(line);

        iss >> index;

        if (config.enable_cancel_button) {
            iss >> button;
        }
    }

    if (button.empty()) {
        Finalize(0, miis[index]);
    } else if (button == MII_BUTTON_CANCEL) {
        Finalize(1, miis[index]);
    } else if (button == MII_BUTTON_OKAY) {
        Finalize(0, miis[index]);
    }
}

} // namespace Frontend
