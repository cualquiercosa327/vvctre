// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"

namespace Camera {

std::vector<u16> convert_rgb888_to_yuyv(const std::vector<unsigned char>& source, int width,
                                        int height);

} // namespace Camera
