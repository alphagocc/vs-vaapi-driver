// Copyright 2023 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "no_op_context_delegate.h"

namespace libvavc8000d
{

void NoOpContextDelegate::SetRenderTarget(const VSSurface &surface) {}

void NoOpContextDelegate::EnqueueWork(const std::vector<const VSBuffer *> &buffers) {}

void NoOpContextDelegate::Run() {}

} // namespace libvavc8000d