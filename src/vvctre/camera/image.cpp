// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <LUrlParser.h>
#include <httplib.h>
#include "common/assert.h"
#include "common/file_util.h"
#include "common/stb_image.h"
#include "common/stb_image_resize.h"
#include "vvctre/camera/image.h"
#include "vvctre/camera/util.h"

namespace Camera {

ImageCamera::ImageCamera(const std::string& file) {
    while (image == nullptr) {
        LUrlParser::clParseURL url = LUrlParser::clParseURL::ParseURL(file);

        if (url.IsValid()) {
            int port;
            std::unique_ptr<httplib::Client> client;

            if (url.m_Scheme == "http") {
                if (!url.GetPort(&port)) {
                    port = 80;
                }

                client = std::make_unique<httplib::Client>(url.m_Host.c_str(), port);
            } else {
                if (!url.GetPort(&port)) {
                    port = 443;
                }

                client = std::make_unique<httplib::SSLClient>(url.m_Host, port);
            }

            client->set_follow_location(true);

            std::shared_ptr<httplib::Response> response = client->Get(('/' + url.m_Path).c_str());

            if (response != nullptr) {
                std::vector<unsigned char> buffer(response->body.size());
                std::memcpy(buffer.data(), response->body.data(), response->body.size());
                image = stbi_load_from_memory(buffer.data(), buffer.size(), &file_width,
                                              &file_height, nullptr, 3);
            }
        } else {
            image = stbi_load(file.c_str(), &file_width, &file_height, nullptr, 3);
        }

        if (image == nullptr) {
            LOG_ERROR(Service_CAM, "Failed to load image");
        }
    }
}

ImageCamera::~ImageCamera() {
    free(image);
}

void ImageCamera::SetResolution(const Service::CAM::Resolution& resolution) {
    requested_width = resolution.width;
    requested_height = resolution.height;
}

void ImageCamera::SetFormat(Service::CAM::OutputFormat format) {
    this->format = format;
}

std::vector<u16> ImageCamera::ReceiveFrame() {
    std::vector<unsigned char> resized(requested_width * requested_height * 3, 0);
    ASSERT(stbir_resize_uint8(image, file_width, file_height, 0, resized.data(), requested_width,
                              requested_height, 0, 3) == 1);

    if (format == Service::CAM::OutputFormat::RGB565) {
        std::vector<u16> frame(requested_width * requested_height, 0);
        std::size_t resized_offset = 0;
        std::size_t pixel = 0;

        while (resized_offset < resized.size()) {
            unsigned char r = resized[resized_offset++];
            unsigned char g = resized[resized_offset++];
            unsigned char b = resized[resized_offset++];
            frame[pixel++] = ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
        }

        return frame;
    } else if (format == Service::CAM::OutputFormat::YUV422) {
        return convert_rgb888_to_yuyv(resized, requested_width, requested_height);
    }

    UNIMPLEMENTED();
    return {};
}

void ImageCamera::StartCapture() {}
void ImageCamera::StopCapture() {}

void ImageCamera::SetFlip(Service::CAM::Flip flip) {
    UNIMPLEMENTED();
}

void ImageCamera::SetEffect(Service::CAM::Effect effect) {
    UNIMPLEMENTED();
}

bool ImageCamera::IsPreviewAvailable() {
    return false;
}

std::unique_ptr<CameraInterface> ImageCameraFactory::Create(const std::string& file,
                                                            const Service::CAM::Flip& /*flip*/) {
    return std::make_unique<ImageCamera>(file);
}

} // namespace Camera