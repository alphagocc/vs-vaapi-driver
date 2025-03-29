// Copyright 2022 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FAKE_DRIVER_H_
#define FAKE_DRIVER_H_

#include <cstdint>
#include <memory>
#include <va/va.h>

#include "buffer.h"
#include "config.h"
#include "context.h"
#include "h264_decoder_delegate.h"
#include "image.h"
#include "object_tracker.h"
#include "scoped_bo_mapping_factory.h"
#include "surface.h"

namespace libvavc8000d
{
// VSDriver is used to keep track of all the state that exists between a call
// to vaInitialize() and a call to vaTerminate(). All public methods are
// thread-safe.
class VSDriver
{
public:
    // VSDriver doesn't dup() or close() |drm_fd|, i.e., it's expected that the
    // driver's user maintains the FD valid at least until after vaTerminate()
    // returns.
    explicit VSDriver(int drm_fd);
    VSDriver(const VSDriver &) = delete;
    VSDriver &operator=(const VSDriver &) = delete;
    ~VSDriver();

    VSConfig::IdType CreateConfig(
        VAProfile profile, VAEntrypoint entrypoint, std::vector<VAConfigAttrib> attrib_list);
    bool ConfigExists(VSConfig::IdType id);
    const VSConfig &GetConfig(VSConfig::IdType id);
    void DestroyConfig(VSConfig::IdType id);

    VSSurface::IdType CreateSurface(unsigned int format, unsigned int width, unsigned int height,
        std::vector<VASurfaceAttrib> attrib_list);
    bool SurfaceExists(VSSurface::IdType id);
    const VSSurface &GetSurface(VSSurface::IdType id);
    void DestroySurface(VSSurface::IdType id);

    VSContext::IdType CreateContext(VAConfigID config_id, int picture_width, int picture_height,
        int flag, std::vector<VASurfaceID> render_targets);
    bool ContextExists(VSContext::IdType id);
    const VSContext &GetContext(VSContext::IdType id);
    void DestroyContext(VSContext::IdType id);

    VSBuffer::IdType CreateBuffer(VAContextID context, VABufferType type,
        unsigned int size_per_element, unsigned int num_elements, const void *data);
    bool BufferExists(VSBuffer::IdType id);
    const VSBuffer &GetBuffer(VSBuffer::IdType id);
    void DestroyBuffer(VSBuffer::IdType id);

    void CreateImage(const VAImageFormat &format, int width, int height, VAImage *va_image);
    bool ImageExists(VSImage::IdType id);
    const VSImage &GetImage(VSImage::IdType id);
    void DestroyImage(VSImage::IdType id);

private:
    // |scoped_bo_mapping_factory_| is used by VSSurface to map BOs. It needs
    // to be declared before |surface_| since we pass a reference to
    // |scoped_bo_mapping_factory_| when creating a VSSurface. Therefore,
    // |scoped_bo_mapping_factory_| should outlive all VSSurface instances.
    ScopedBOMappingFactory scoped_bo_mapping_factory_;
    ObjectTracker<VSConfig> config_;
    ObjectTracker<VSSurface> surface_;
    ObjectTracker<VSContext> context_;
    ObjectTracker<VSBuffer> buffers_;

    // The VSImage instances in |images_| reference VSBuffer instances in
    // |buffers_|, so the latter should outlive the former.
    ObjectTracker<VSImage> images_;
};

} // namespace libvavc8000d

#endif // FAKE_DRIVER_H_