// Copyright 2023 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FAKE_IMAGE_H_
#define FAKE_IMAGE_H_

#include <va/va.h>

#include <memory>
#include <vector>

namespace libvavc8000d
{

class VSBuffer;
class VSDriver;

// Class used for tracking a VAImage and all information relevant to it.
//
// The metadata (ID, format, dimensions, number of planes, and plane
// stride/offset) of a VSImage is immutable. The accessors for such metadata
// are thread-safe. The contents of the backing VSBuffer object are mutable,
// but the reference to that VSBuffer is immutable, i.e., the backing buffer
// is always the same, but the contents may change. Thus, while the accessor for
// the VSBuffer is thread-safe, writes and reads to this buffer must be
// synchronized externally.
//
// Note: Currently the VSImage only supports the NV12 image format.
class VSImage
{
public:
    using IdType = VAImageID;

    // Creates a VSImage using the specified metadata (|id|, |format|, |width|,
    // and |height|). The |fake_driver| is used to create a backing VSBuffer and
    // manage its lifetime. Thus, the |fake_driver| must outlive the created
    // `VSImage`. Upon return, *|va_image| is filled with all the fields needed
    // by the libva client to use the image.
    static std::unique_ptr<VSImage> Create(IdType id, const VAImageFormat &format, int width,
        int height, VSDriver &fake_driver, VAImage *va_image);

    VSImage(const VSImage &) = delete;
    VSImage &operator=(const VSImage &) = delete;
    ~VSImage();

    IdType GetID() const;
    const VAImageFormat &GetFormat() const;
    int GetWidth() const;
    int GetHeight() const;
    const VSBuffer &GetBuffer() const;
    uint32_t GetPlaneStride(size_t plane) const;
    uint32_t GetPlaneOffset(size_t plane) const;

private:
    struct Plane
    {
        Plane(uint32_t stride, uint32_t offset);

        const uint32_t stride;
        const uint32_t offset;
    };

    VSImage(IdType id, const VAImageFormat &format, int width, int height,
        std::vector<Plane> planes, const VSBuffer &buffer, VSDriver &driver);

    const IdType id_;
    const VAImageFormat format_;
    const int width_;
    const int height_;
    const std::vector<Plane> planes_;

    const VSBuffer &buffer_;

    VSDriver &driver_;
};

} // namespace libvavc8000d

#endif // FAKE_IMAGE_H_
