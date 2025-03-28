// Copyright 2024 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef H264_DECODER_DELEGATE_H_
#define H264_DECODER_DELEGATE_H_

#include <va/va.h>

#include "base/lru_cache.h"
#include "context_delegate.h"
#include <hal/csi_vdec.h>

namespace libvavc8000d
{

// Class used for H264 software decoding.
class H264DecoderDelegate : public ContextDelegate
{
public:
    explicit H264DecoderDelegate(
        int picture_width_hint, int picture_height_hint, VAProfile profile);
    H264DecoderDelegate(const H264DecoderDelegate &) = delete;
    H264DecoderDelegate &operator=(const H264DecoderDelegate &) = delete;
    ~H264DecoderDelegate() override;

    // ContextDelegate implementation.
    void SetRenderTarget(const VSSurface &surface) override;
    void EnqueueWork(const std::vector<const VSBuffer *> &buffers) override;
    void Run() override;

private:
    void OnFrameReady(unsigned char *pData[3], SBufferInfo *pDstInfo);

    csi_vdec_dev_t csi_decoder_;
    const VAProfile profile_;

    std::vector<const VSBuffer *> slice_data_buffers_;
    std::vector<const VSBuffer *> slice_param_buffers_;

    const VSSurface *render_target_{ nullptr };
    const VSBuffer *pic_param_buffer_{ nullptr };
    const VSBuffer *matrix_buffer_{ nullptr };

    uint32_t current_ts_ = 0;
    base::LRUCache<uint32_t, const VSSurface *> ts_to_render_target_;
};

} // namespace libvavc8000d

#endif // H264_DECODER_DELEGATE_H_