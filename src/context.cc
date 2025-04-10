// Copyright 2022 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "context.h"

#include "base/logging.h"
#include "config.h"
#include "h264_decoder_delegate.h"
#include "no_op_context_delegate.h"
#include <fstream>
#include <va/va.h>

namespace
{

std::unique_ptr<libvavc8000d::ContextDelegate> CreateDelegate(
    const libvavc8000d::VSConfig &config, int picture_width, int picture_height)
{
    const char *use_no_op_context_delegate_env_var = getenv("USE_NO_OP_CONTEXT_DELEGATE");
    if (use_no_op_context_delegate_env_var
        && strcmp(use_no_op_context_delegate_env_var, "1") == 0) {
        return std::make_unique<libvavc8000d::NoOpContextDelegate>();
    }

    if (config.GetEntrypoint() != VAEntrypointVLD) { return nullptr; }

    switch (config.GetProfile()) {
    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
    case VAProfileH264High:
        return std::make_unique<libvavc8000d::H264DecoderDelegate>(
            picture_width, picture_height, config.GetProfile());
    default: break;
    }

    return nullptr;
}

} // namespace

namespace libvavc8000d
{

VSContext::VSContext(VSContext::IdType id, const VSConfig &config, int picture_width,
    int picture_height, int flag, std::vector<VASurfaceID> render_targets)
    : id_(id)
    , config_(config)
    , picture_width_(picture_width)
    , picture_height_(picture_height)
    , flag_(flag)
    , render_targets_(std::move(render_targets))
    , delegate_(CreateDelegate(config_, picture_width_, picture_height_))
{}
VSContext::~VSContext() = default;

VSContext::IdType VSContext::GetID() const { return id_; }

const VSConfig &VSContext::GetConfig() const { return config_; }

int VSContext::GetPictureWidth() const { return picture_width_; }

int VSContext::GetPictureHeight() const { return picture_height_; }

int VSContext::GetFlag() const { return flag_; }

const std::vector<VASurfaceID> &VSContext::GetRenderTargets() const { return render_targets_; }

void VSContext::BeginPicture(const VSSurface &surface) const
{
    CHECK(delegate_);
    delegate_->SetRenderTarget(surface);
}

void VSContext::RenderPicture(const std::vector<const VSBuffer *> &buffers) const
{
    CHECK(delegate_);
    delegate_->EnqueueWork(buffers);
}

void VSContext::EndPicture() const
{
    CHECK(delegate_);
    delegate_->Run();
}

} // namespace libvavc8000d
