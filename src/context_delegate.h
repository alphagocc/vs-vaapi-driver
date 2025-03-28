// Copyright 2023 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTEXT_DELEGATE_H_
#define CONTEXT_DELEGATE_H_

#include <vector>

namespace libvavc8000d
{

class VSBuffer;
class VSSurface;

// A ContextDelegate implements the details of a specific task (e.g., software
// decoding). A FakeContext can delegate work to a ContextDelegate through a
// task-agnostic API.
//
// Users of a ContextDelegate instance must not assume any of its methods are
// thread-safe.
class ContextDelegate
{
public:
    ContextDelegate() = default;
    ContextDelegate(const ContextDelegate &) = delete;
    ContextDelegate &operator=(const ContextDelegate &) = delete;
    virtual ~ContextDelegate() = default;

    // Sets the |surface| to use as the source or destination of the work
    // performed by the ContextDelegate:
    //
    // - For a decoder ContextDelegate, |surface| is the destination of the
    //   decoded data before applying effects (for example, for AV1 with
    //   film-grain synthesis, |surface| is the decoded data prior to applying
    //   film-grain).
    //
    // - For an encoder ContextDelegate, |surface| is the source data.
    //
    // - For a video-processing ContextDelegate, |surface| is the destination.
    //
    // This is the first method that should be called. It may be called more than
    // once as long as there's no work enqueued, i.e., if EnqueueWork() has been
    // called, Run() must be called prior to calling SetRenderTarget() again.
    // The |surface| must remain alive for as long as it's set as the render
    // target (i.e., until either SetRenderTarget() is called with a different
    // surface or the ContextDelegate is destroyed).
    virtual void SetRenderTarget(const VSSurface &surface) = 0;

    // Enqueues work to be performed using the |surface| passed to
    // SetRenderTarget() as the source or destination (depending on the type of
    // work) and |buffers| as parameters. For example, for decoding, the
    // |surface| passed to SetRenderTarget() generally corresponds to the
    // destination for the decoded data while |buffers| contains
    // (among other things) the entropy-coded data.
    // SetRenderTarget() must be called before this at least once.
    // It's invalid to call EnqueueWork() if there's currently enqueued work.
    // The buffers must remain alive for as long as the work remains enqueued
    // (i.e., until either Run() returns or the ContextDelegate is destroyed).
    virtual void EnqueueWork(const std::vector<const VSBuffer *> &buffers) = 0;

    // Executes the enqueued work. EnqueueWork() must be called before this. After
    // Run() returns, the caller may assume that the ContextDelegate does not have
    // any more work enqueued. Thus, if the caller wants to call Run() again, it
    // must enqueue more work using EnqueueWork().
    virtual void Run() = 0;
};

} // namespace libvavc8000d

#endif // CONTEXT_DELEGATE_H_