// Copyright 2022 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FAKE_CONTEXT_H_
#define FAKE_CONTEXT_H_

#include <va/va.h>

#include <memory>
#include <vector>

namespace libvavc8000d
{

class ContextDelegate;
class VSSurface;
class VSBuffer;
class VSConfig;

// Class used for tracking a VAContext and all information relevant to it.
// All objects of this class are immutable, but three of the methods must be
// synchronized externally: BeginPicture(), RenderPicture(), and EndPicture().
// The other methods are thread-safe and may be called concurrently with any of
// those three methods.
class VSContext
{
public:
    using IdType = VAContextID;

    // Note: |config| must outlive the VSContext.
    VSContext(IdType id, const VSConfig &config, int picture_width, int picture_height, int flag,
        std::vector<VASurfaceID> render_targets);
    VSContext(const VSContext &) = delete;
    VSContext &operator=(const VSContext &) = delete;
    ~VSContext();

    IdType GetID() const;
    const VSConfig &GetConfig() const;
    int GetPictureWidth() const;
    int GetPictureHeight() const;
    int GetFlag() const;
    const std::vector<VASurfaceID> &GetRenderTargets() const;

    void BeginPicture(const VSSurface &surface) const;
    void RenderPicture(const std::vector<const VSBuffer *> &buffers) const;
    void EndPicture() const;

private:
    const IdType id_;
    const VSConfig &config_;

    const int picture_width_;
    const int picture_height_;
    const int flag_;
    const std::vector<VASurfaceID> render_targets_;
    const std::unique_ptr<ContextDelegate> delegate_;
};

} // namespace libvavc8000d

#endif // FAKE_CONTEXT_H_
