// Copyright 2023 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#    pragma allow_unsafe_buffers
#endif

#include "image.h"

#include "base/ptr_util.h"
#include "buffer.h"
#include "driver.h"

namespace libvavc8000d
{

std::unique_ptr<VSImage> VSImage::Create(IdType id, const VAImageFormat &format, int width,
    int height, VSDriver &fake_driver, VAImage *va_image)
{
    // Chrome should only request NV12 images from the fake driver.
    CHECK_EQ(format.fourcc, static_cast<uint32_t>(VA_FOURCC_NV12));

    // Validate the |format|. Chrome should request VA_LSB_FIRST images only.
    CHECK_EQ(format.byte_order, static_cast<uint32_t>(VA_LSB_FIRST));
    CHECK_EQ(format.bits_per_pixel, 12u);

    std::vector<Plane> planes;
    planes.emplace_back(/*stride=*/static_cast<uint32_t>(width),
        /*offset=*/0);

    // TODO(b/358445928): bring back safe math.
    // UV stride = ceil(width / 2) * 2.
    uint32_t uv_stride = static_cast<uint32_t>(width);
    uv_stride += 1;
    uv_stride /= 2;
    uv_stride *= 2;

    // UV offset = Y plane size = width * height.
    uint32_t uv_offset = static_cast<uint32_t>(width);
    uv_offset *= static_cast<uint32_t>(height);

    planes.emplace_back(/*stride=*/uv_stride,
        /*offset=*/uv_offset);

    // UV plane size = ceil(height / 2) * UV stride.
    uint32_t uv_size = static_cast<uint32_t>(height);
    uv_size += 1;
    uv_size /= 2;
    uv_size *= uv_stride;

    // Total size = UV offset + UV plane size.
    const unsigned int data_size = uv_offset + uv_size;

    memset(va_image, 0, sizeof(VAImage));
    va_image->image_id = id;
    va_image->format = format;

    VSBuffer::IdType buf = fake_driver.CreateBuffer(
        /*context=*/VA_INVALID_ID, VAImageBufferType,
        /*size_per_element=*/1, data_size, /*data=*/nullptr);
    va_image->buf = buf;

    va_image->width = static_cast<uint16_t>(width);
    va_image->height = static_cast<uint16_t>(height);
    va_image->data_size = data_size;
    va_image->num_planes = 2;
    va_image->pitches[0] = static_cast<uint32_t>(width);
    va_image->pitches[1] = uv_stride;
    va_image->offsets[0] = 0;
    va_image->offsets[1] = uv_offset;

    return base::WrapUnique(new VSImage(
        id, format, width, height, std::move(planes), fake_driver.GetBuffer(buf), fake_driver));
}

VSImage::VSImage(VSImage::IdType id, const VAImageFormat &format, int width, int height,
    std::vector<Plane> planes, const VSBuffer &buffer, VSDriver &driver)
    : id_(id)
    , format_(format)
    , width_(width)
    , height_(height)
    , planes_(std::move(planes))
    , buffer_(buffer)
    , driver_(driver)
{}

VSImage::~VSImage() { driver_.DestroyBuffer(buffer_.GetID()); }

VSImage::IdType VSImage::GetID() const { return id_; }

const VAImageFormat &VSImage::GetFormat() const { return format_; }

int VSImage::GetWidth() const { return width_; }

int VSImage::GetHeight() const { return height_; }

const VSBuffer &VSImage::GetBuffer() const { return buffer_; }

uint32_t VSImage::GetPlaneStride(size_t plane) const
{
    CHECK_LT(plane, planes_.size());
    return planes_[plane].stride;
}

uint32_t VSImage::GetPlaneOffset(size_t plane) const
{
    CHECK_LT(plane, planes_.size());
    return planes_[plane].offset;
}

VSImage::Plane::Plane(uint32_t stride, uint32_t offset) : stride(stride), offset(offset) {}

} // namespace libvavc8000d
