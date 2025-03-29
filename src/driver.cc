// Copyright 2022 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "driver.h"
#include "dwl.h"
#include "h264_decoder_delegate.h"

namespace libvavc8000d
{

VSDriver::VSDriver(int drm_fd) : scoped_bo_mapping_factory_(drm_fd) {}

VSDriver::~VSDriver() = default;

VSConfig::IdType VSDriver::CreateConfig(
    VAProfile profile, VAEntrypoint entrypoint, std::vector<VAConfigAttrib> attrib_list)
{
    return config_.CreateObject(profile, entrypoint, std::move(attrib_list));
}

bool VSDriver::ConfigExists(VSConfig::IdType id) { return config_.ObjectExists(id); }

const VSConfig &VSDriver::GetConfig(VSConfig::IdType id) { return config_.GetObject(id); }

void VSDriver::DestroyConfig(VSConfig::IdType id) { config_.DestroyObject(id); }

VSSurface::IdType VSDriver::CreateSurface(unsigned int format, unsigned int width,
    unsigned int height, std::vector<VASurfaceAttrib> attrib_list)
{
    return surface_.CreateObject(
        format, width, height, std::move(attrib_list), scoped_bo_mapping_factory_);
}

bool VSDriver::SurfaceExists(VSSurface::IdType id) { return surface_.ObjectExists(id); }

const VSSurface &VSDriver::GetSurface(VSSurface::IdType id) { return surface_.GetObject(id); }

void VSDriver::DestroySurface(VSSurface::IdType id) { surface_.DestroyObject(id); }

VSContext::IdType VSDriver::CreateContext(VAConfigID config_id, int picture_width,
    int picture_height, int flag, std::vector<VASurfaceID> render_targets)
{
    return context_.CreateObject(
        GetConfig(config_id), picture_width, picture_height, flag, std::move(render_targets));
}

bool VSDriver::ContextExists(VSContext::IdType id) { return context_.ObjectExists(id); }

const VSContext &VSDriver::GetContext(VSContext::IdType id) { return context_.GetObject(id); }

void VSDriver::DestroyContext(VSContext::IdType id) { context_.DestroyObject(id); }

VSBuffer::IdType VSDriver::CreateBuffer(VAContextID context, VABufferType type,
    unsigned int size_per_element, unsigned int num_elements, const void *data)
{
    return buffers_.CreateObject(context, type, size_per_element, num_elements, data);
}

bool VSDriver::BufferExists(VSBuffer::IdType id) { return buffers_.ObjectExists(id); }

const VSBuffer &VSDriver::GetBuffer(VSBuffer::IdType id) { return buffers_.GetObject(id); }

void VSDriver::DestroyBuffer(VSBuffer::IdType id) { buffers_.DestroyObject(id); }

void VSDriver::CreateImage(const VAImageFormat &format, int width, int height, VAImage *va_image)
{
    images_.CreateObject(format, width, height, /*fake_driver=*/*this, va_image);
}

bool VSDriver::ImageExists(VSImage::IdType id) { return images_.ObjectExists(id); }

const VSImage &VSDriver::GetImage(VSImage::IdType id) { return images_.GetObject(id); }

void VSDriver::DestroyImage(VSImage::IdType id) { images_.DestroyObject(id); }

} // namespace libvavc8000d
