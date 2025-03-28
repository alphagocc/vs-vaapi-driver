// Copyright 2022 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifdef UNSAFE_BUFFERS_BUILD
// We need to conform to the GBM API, which unfortunately involves a lot of
// unsafe buffer access to maintain C99 compatibility.
#    pragma allow_unsafe_buffers
#endif

#include "surface.h"

#include <libdrm/drm_fourcc.h>
#include <va/va_drmcommon.h>

#include <unordered_set>

#include "base/logging.h"
#include "base/ptr_util.h"

#define GBM_FORMAT_P010 __gbm_fourcc_code('P', '0', '1', '0')

namespace libvavc8000d
{

VSSurface::VSSurface(VSSurface::IdType id, unsigned int format, uint32_t va_fourcc,
    unsigned int width, unsigned int height, std::vector<VASurfaceAttrib> attrib_list,
    ScopedBOMapping mapped_bo)
    : id_(id)
    , format_(format)
    , va_fourcc_(va_fourcc)
    , width_(width)
    , height_(height)
    , attrib_list_(std::move(attrib_list))
    , mapped_bo_(std::move(mapped_bo))
{}

VSSurface::~VSSurface() = default;

std::unique_ptr<VSSurface> VSSurface::Create(IdType id, unsigned int format, unsigned int width,
    unsigned int height, std::vector<VASurfaceAttrib> attrib_list,
    ScopedBOMappingFactory &scoped_bo_mapping_factory)
{
    // There are no specified attributes to this surface
    if (attrib_list.empty()) {
        return base::WrapUnique(
            new VSSurface(id, format, /*va_fourcc=*/0u, width, height, std::move(attrib_list),
                /*mapped_bo=*/{}));
    }

    // Verify attributes and extract surface descriptor.
    std::unordered_set<VASurfaceAttribType> attribs;
    VADRMPRIMESurfaceDescriptor *surf_desc = nullptr;
    for (auto attrib : attrib_list) {
        // Some libva clients are quirky about their surface attributes, so
        // simply ignore unexpected attribute types.
        CHECK(attribs.find(attrib.type) == attribs.end());
        attribs.insert(attrib.type);

        if (attrib.type == VASurfaceAttribMemoryType) {
            CHECK_EQ(attrib.value.type, VAGenericValueTypeInteger);
            CHECK_EQ(attrib.value.value.i, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2);
        } else if (attrib.type == VASurfaceAttribExternalBufferDescriptor) {
            CHECK_EQ(attrib.value.type, VAGenericValueTypePointer);
            surf_desc = static_cast<VADRMPRIMESurfaceDescriptor *>(attrib.value.value.p);
        }
    }
    CHECK(surf_desc);
    CHECK(attribs.find(VASurfaceAttribMemoryType) != attribs.end());

    struct gbm_import_fd_modifier_data fd_data{};

    CHECK_GT(width, 0u);
    CHECK_GT(height, 0u);
    CHECK_EQ(static_cast<unsigned int>(surf_desc->width), width);
    CHECK_EQ(static_cast<unsigned int>(surf_desc->height), height);

    fd_data.width = surf_desc->width;
    fd_data.height = surf_desc->height;
    CHECK(surf_desc->fourcc == VA_FOURCC_NV12 || surf_desc->fourcc == VA_FOURCC_P010);
    const unsigned int expected_rt_format
        = surf_desc->fourcc == VA_FOURCC_NV12 ? VA_RT_FORMAT_YUV420 : VA_RT_FORMAT_YUV420_10;

    const uint32_t expected_drm_format
        = surf_desc->fourcc == VA_FOURCC_NV12 ? DRM_FORMAT_NV12 : DRM_FORMAT_P010;
    const uint32_t expected_num_planes = 2u;
    fd_data.format = surf_desc->fourcc == VA_FOURCC_NV12 ? GBM_FORMAT_NV12 : GBM_FORMAT_P010;

    CHECK_EQ(format, expected_rt_format);
    CHECK_GT(surf_desc->num_objects, 0u);
    CHECK_LE(surf_desc->num_objects, static_cast<uint32_t>(std::size(surf_desc->objects)));
    fd_data.num_fds = surf_desc->num_objects;

    fd_data.modifier = surf_desc->objects[0].drm_format_modifier;
    for (uint32_t i = 1u; i < surf_desc->num_objects; i++) {
        CHECK_EQ(surf_desc->objects[i].drm_format_modifier, fd_data.modifier);
    }

    // In general, the planes may be distributed across multiple layers, but let's
    // only handle the situation in which all the planes are in one layer.
    CHECK_EQ(surf_desc->num_layers, 1u);
    CHECK_EQ(surf_desc->layers[0].drm_format, expected_drm_format);
    CHECK_EQ(surf_desc->layers[0].num_planes, expected_num_planes);

    for (uint32_t plane = 0u; plane < expected_num_planes; ++plane) {
        CHECK_LT(surf_desc->layers[0].object_index[plane], surf_desc->num_objects);
        fd_data.fds[plane] = surf_desc->objects[surf_desc->layers[0].object_index[plane]].fd;
        fd_data.strides[plane] = static_cast<int>(surf_desc->layers[0].pitch[plane]);
        fd_data.offsets[plane] = static_cast<int>(surf_desc->layers[0].offset[plane]);
    }

    ScopedBOMapping mapped_bo = scoped_bo_mapping_factory.Create(fd_data);
    CHECK(!!mapped_bo);
    return base::WrapUnique(new VSSurface(id, format, surf_desc->fourcc, width, height,
        std::move(attrib_list), std::move(mapped_bo)));
}

VSSurface::IdType VSSurface::GetID() const { return id_; }

unsigned int VSSurface::GetFormat() const { return format_; }

uint32_t VSSurface::GetVAFourCC() const { return va_fourcc_; }

unsigned int VSSurface::GetWidth() const { return width_; }

unsigned int VSSurface::GetHeight() const { return height_; }

const std::vector<VASurfaceAttrib> &VSSurface::GetSurfaceAttribs() const { return attrib_list_; }

const ScopedBOMapping &VSSurface::GetMappedBO() const { return mapped_bo_; }

} // namespace libvavc8000d