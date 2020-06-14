
// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include "common/file_util.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "vvctre/common.h"

const u8 vvctre_version_major = 33;
const u8 vvctre_version_minor = 3;
const u8 vvctre_version_patch = 2;

std::vector<std::tuple<std::string, std::string>> GetInstalledList() {
    std::vector<std::tuple<std::string, std::string>> all;
    std::vector<std::tuple<std::string, std::string>> bootable;
    std::vector<std::tuple<std::string, std::string>> updates;
    std::vector<std::tuple<std::string, std::string>> dlc;
    std::vector<std::tuple<std::string, std::string>> system;
    std::vector<std::tuple<std::string, std::string>> download_play;

    const auto AddTitlesForMediaType = [&](Service::FS::MediaType media_type) {
        FileUtil::FSTEntry entries;
        FileUtil::ScanDirectoryTree(Service::AM::GetMediaTitlePath(media_type), entries, 1);
        for (const FileUtil::FSTEntry& tid_high : entries.children) {
            for (const FileUtil::FSTEntry& tid_low : tid_high.children) {
                std::string tid_string = tid_high.virtualName + tid_low.virtualName;

                if (tid_string.length() == Service::AM::TITLE_ID_VALID_LENGTH) {
                    const u64 tid = std::stoull(tid_string, nullptr, 16);

                    const std::string path = Service::AM::GetTitleContentPath(media_type, tid);
                    std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(path);
                    if (loader != nullptr) {
                        std::string title;
                        loader->ReadTitle(title);

                        switch (std::stoull(tid_high.virtualName, nullptr, 16)) {
                        case 0x00040000: {
                            bootable.push_back(std::make_tuple(
                                path, fmt::format("[Bootable] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        case 0x0004000e: {
                            updates.push_back(std::make_tuple(
                                path, fmt::format("[Update] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        case 0x0004008c: {
                            dlc.push_back(std::make_tuple(
                                path, fmt::format("[DLC] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        case 0x00040001: {
                            download_play.push_back(std::make_tuple(
                                path, fmt::format("[Download Play] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        default: {
                            system.push_back(std::make_tuple(
                                path, fmt::format("[System] {} ({:016X})",
                                                  title.empty() ? "Unknown" : title, tid)));
                            break;
                        }
                        }
                    }
                }
            }
        }
    };

    AddTitlesForMediaType(Service::FS::MediaType::NAND);
    AddTitlesForMediaType(Service::FS::MediaType::SDMC);

    all.insert(all.end(), bootable.begin(), bootable.end());
    all.insert(all.end(), updates.begin(), updates.end());
    all.insert(all.end(), dlc.begin(), dlc.end());
    all.insert(all.end(), system.begin(), system.end());
    all.insert(all.end(), download_play.begin(), download_play.end());

    return all;
}
