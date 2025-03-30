// Copyright 2024 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "h264_decoder_delegate.h"

#include "base/byte_conversions.h"
#include "base/logging.h"
#include "basetype.h"
#include "buffer.h"
#include "decapicommon.h"
#include "dectypes.h"
#include "dwl.h"
#include "h264decapi.h"
#include "surface.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>

const int BUFFER_ALIGN_MASK = 0xF;

namespace libvavc8000d
{
namespace
{

    // TODO(b/328430784): Support additional H264 profiles.
    enum H264ProfileIDC {
        kProfileIDCBaseline = 66,
        kProfileIDCConstrainedBaseline = kProfileIDCBaseline,
        kProfileIDCMain = 77,
        kProfileIDCHigh = 100,
    };

    enum H264LevelIDC : uint8_t {
        kLevelIDC1p0 = 10,
        kLevelIDC1B = 9,
        kLevelIDC1p1 = 11,
        kLevelIDC1p2 = 12,
        kLevelIDC1p3 = 13,
        kLevelIDC2p0 = 20,
        kLevelIDC2p1 = 21,
        kLevelIDC2p2 = 22,
        kLevelIDC3p0 = 30,
        kLevelIDC3p1 = 31,
        kLevelIDC3p2 = 32,
        kLevelIDC4p0 = 40,
        kLevelIDC4p1 = 41,
        kLevelIDC4p2 = 42,
        kLevelIDC5p0 = 50,
        kLevelIDC5p1 = 51,
        kLevelIDC5p2 = 52,
        kLevelIDC6p0 = 60,
        kLevelIDC6p1 = 61,
        kLevelIDC6p2 = 62,
    };

    struct H264NALU
    {
        H264NALU() = default;

        enum Type {
            kUnspecified = 0,
            kNonIDRSlice = 1,
            kSliceDataA = 2,
            kSliceDataB = 3,
            kSliceDataC = 4,
            kIDRSlice = 5,
            kSEIMessage = 6,
            kSPS = 7,
            kPPS = 8,
            kAUD = 9,
            kEOSeq = 10,
            kEOStream = 11,
            kFiller = 12,
            kSPSExt = 13,
            kPrefix = 14,
            kSubsetSPS = 15,
            kDPS = 16,
            kReserved17 = 17,
            kReserved18 = 18,
            kCodedSliceAux = 19,
            kCodedSliceExtension = 20,
        };

        // After (without) start code; we don't own the underlying memory
        // and a shallow copy should be made when copying this struct.
        const uint8_t *data = nullptr;
        off_t size = 0; // From after start code to start code of next NALU (or EOS).

        int nal_ref_idc = 0;
        int nal_unit_type = 0;
    };

    struct VAH264SPS
    {
        u8 profile_idc;
        bool constraint_set3_flag;
        u8 level_idc;
        bool vui_parameters_present_flag;
        bool bitstream_restriction_flag;
        u32 num_reorder_frames;
    };

    // H264BitstreamBuilder is mostly a copy&paste from Chromium's
    // H26xAnnexBBitstreamBuilder
    // (//media/filters/h26x_annex_b_bitstream_builder.h). The reason to not just
    // include that file is that the fake libva driver is in the process of being
    // moved out of the Chromium tree and into its own project, and we want to
    // avoid depending on Chromium's utilities.
    class H264BitstreamBuilder
    {
    public:
        explicit H264BitstreamBuilder(bool insert_emulation_prevention_bytes = false)
            : insert_emulation_prevention_bytes_(insert_emulation_prevention_bytes)
        {
            Reset();
        }

        template <typename T> void AppendBits(size_t num_bits, T val)
        {
            AppendU64(num_bits, static_cast<uint64_t>(val));
        }

        void AppendBits(size_t num_bits, bool val)
        {
            CHECK_EQ(num_bits, 1ul);
            AppendBool(val);
        }

        // Append a one-bit bool/flag value |val| to the stream.
        void AppendBool(bool val)
        {
            if (bits_left_in_reg_ == 0u) { FlushReg(); }

            reg_ <<= 1;
            reg_ |= (static_cast<uint64_t>(val) & 1u);
            --bits_left_in_reg_;
        }

        // Append a signed value in |val| in Exp-Golomb code.
        void AppendSE(int val)
        {
            if (val > 0) {
                AppendUE(val * 2 - 1);
            } else {
                AppendUE(-val * 2);
            }
        }

        // Append an unsigned value in |val| in Exp-Golomb code.
        void AppendUE(unsigned int val)
        {
            size_t num_zeros = 0u;
            unsigned int v = val + 1u;

            while (v > 1) {
                v >>= 1;
                ++num_zeros;
            }

            AppendBits(num_zeros, 0);
            AppendBits(num_zeros + 1, val + 1u);
        }

        void BeginNALU(H264NALU::Type nalu_type, int nal_ref_idc)
        {
            CHECK(!in_nalu_);
            CHECK_EQ(bits_left_in_reg_, kRegBitSize);

            CHECK_LE(nalu_type, H264NALU::kEOStream);
            CHECK_GE(nal_ref_idc, 0);
            CHECK_LE(nal_ref_idc, 3);

            AppendBits(32, 0x00000001);
            Flush();
            in_nalu_ = true;
            AppendBits(1, 0); // forbidden_zero_bit.
            AppendBits(2, nal_ref_idc);
            CHECK_NE(nalu_type, 0);
            AppendBits(5, nalu_type);
        }

        void FinishNALU()
        {
            // RBSP stop one bit.
            AppendBits(1, 1);

            // Byte-alignment zero bits.
            AppendBits(bits_left_in_reg_ % 8, 0);

            Flush();
            in_nalu_ = false;
        }

        void Flush()
        {
            if (bits_left_in_reg_ != kRegBitSize) { FlushReg(); }
        }

        size_t BytesInBuffer() const
        {
            CHECK_EQ(bits_left_in_reg_, kRegBitSize);
            return pos_;
        }

        const uint8_t *data() const
        {
            CHECK(!data_.empty());
            CHECK_EQ(bits_left_in_reg_, kRegBitSize);

            return data_.data();
        }

    private:
        typedef uint64_t RegType;
        enum {
            // Sizes of reg_.
            kRegByteSize = sizeof(RegType),
            kRegBitSize = kRegByteSize * 8,
            // Amount of bytes to grow the buffer by when we run out of
            // previously-allocated memory for it.
            kGrowBytes = 4096,
        };

        void Grow()
        {
            static_assert(
                kGrowBytes >= kRegByteSize, "kGrowBytes must be larger than kRegByteSize");
            data_.resize(data_.size() + kGrowBytes);
        }

        void Reset()
        {
            data_ = std::vector<uint8_t>(kGrowBytes, 0);
            pos_ = 0;
            bits_in_buffer_ = 0;
            reg_ = 0;
            bits_left_in_reg_ = kRegBitSize;
            in_nalu_ = false;
        }

        void AppendU64(size_t num_bits, uint64_t val)
        {
            CHECK_LE(num_bits, kRegBitSize);
            while (num_bits > 0u) {
                if (bits_left_in_reg_ == 0u) { FlushReg(); }

                uint64_t bits_to_write
                    = num_bits > bits_left_in_reg_ ? bits_left_in_reg_ : num_bits;
                uint64_t val_to_write = (val >> (num_bits - bits_to_write));
                if (bits_to_write < 64u) {
                    val_to_write &= ((1ull << bits_to_write) - 1);
                    reg_ <<= bits_to_write;
                    reg_ |= val_to_write;
                } else {
                    reg_ = val_to_write;
                }
                num_bits -= bits_to_write;
                bits_left_in_reg_ -= bits_to_write;
            }
        }

        void FlushReg()
        {
            // Flush all bytes that have at least one bit cached, but not more
            // (on Flush(), reg_ may not be full).
            size_t bits_in_reg = kRegBitSize - bits_left_in_reg_;
            if (bits_in_reg == 0u) { return; }

            size_t bytes_in_reg = base::AlignUp(bits_in_reg, size_t{ 8 }) / 8u;
            reg_ <<= (kRegBitSize - bits_in_reg);

            // Convert to MSB and append as such to the stream.
            std::array<uint8_t, 8> reg_be = base::U64ToBigEndian(reg_);

            if (insert_emulation_prevention_bytes_ && in_nalu_) {
                // The EPB only works on complete bytes being flushed.
                CHECK_EQ(bits_in_reg % 8u, 0u);
                // Insert emulation prevention bytes (spec 7.3.1).
                constexpr uint8_t kEmulationByte = 0x03u;

                for (size_t i = 0; i < bytes_in_reg; ++i) {
                    // This will possibly check the NALU header byte. However the
                    // CHECK_NE(nalu_type, 0) makes sure that it is not 0.
                    if (pos_ >= 2u && data_[pos_ - 2u] == 0 && data_[pos_ - 1u] == 0u
                        && reg_be[i] <= kEmulationByte) {
                        if (pos_ + 1u > data_.size()) { Grow(); }
                        data_[pos_++] = kEmulationByte;
                        bits_in_buffer_ += 8u;
                    }
                    if (pos_ + 1u > data_.size()) { Grow(); }
                    data_[pos_++] = reg_be[i];
                    bits_in_buffer_ += 8u;
                }
            } else {
                // Make sure we have enough space.
                if (pos_ + bytes_in_reg > data_.size()) { Grow(); }

                std::copy(reg_be.cbegin(), reg_be.cbegin() + bytes_in_reg, data_.begin() + pos_);

                bits_in_buffer_ = pos_ * 8u + bits_in_reg;
                pos_ += bytes_in_reg;
            }

            reg_ = 0u;
            bits_left_in_reg_ = kRegBitSize;
        }

        // Whether to insert emulation prevention bytes in RBSP.
        bool insert_emulation_prevention_bytes_;

        // Whether BeginNALU() has been called but not FinishNALU().
        bool in_nalu_;

        // Unused bits left in reg_.
        size_t bits_left_in_reg_;

        // Cache for appended bits. Bits are flushed to data_ with kRegByteSize
        // granularity, i.e. when reg_ becomes full, or when an explicit FlushReg()
        // is called.
        RegType reg_;

        // Current byte offset in data_ (points to the start of unwritten bits).
        size_t pos_;
        // Current last bit in data_ (points to the start of unwritten bit).
        size_t bits_in_buffer_;

        // Buffer for stream data. Only the bytes before `pos_` can be assumed to have
        // been initialized.
        std::vector<uint8_t> data_;
    };

    void BuildPackedH264SPS(const VAPictureParameterBufferH264 *pic_param_buffer,
        std::vector<const VSBuffer *> slice_param_buffers, const VAProfile profile,
        H264BitstreamBuilder &bitstream_builder)
    {

        const VASliceParameterBufferH264 *sliceParam
            = reinterpret_cast<VASliceParameterBufferH264 *>(slice_param_buffers[0]->GetData());

        // const VAH264SPS *sps = reinterpret_cast<const VAH264SPS *>(&(sliceParam->RefPicList0));

        // Build NAL header following spec section 7.3.1.
        bitstream_builder.BeginNALU(H264NALU::kSPS, 3);
        int profile_idc = 0;
        // Build SPS following spec section 7.3.2.1.
        switch (profile) {
        case VAProfileH264Baseline:
        case VAProfileH264ConstrainedBaseline:
            profile_idc = kProfileIDCBaseline;
            bitstream_builder.AppendBits(8,
                kProfileIDCBaseline); // profile_idc u(8).
            bitstream_builder.AppendBool(0); // Constraint Set0 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set1 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set2 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set3 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set4 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set5 Flag u(1).
            bitstream_builder.AppendBits(2, 0); // Reserved zero 2bits u(2).
            bitstream_builder.AppendBits(8, kLevelIDC5p1); // level_idc u(8).
            break;
        case VAProfileH264Main:
            profile_idc = kProfileIDCMain;
            bitstream_builder.AppendBits(8, kProfileIDCMain);
            bitstream_builder.AppendBool(0); // Constraint Set0 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set1 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set2 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set3 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set4 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set5 Flag u(1).
            bitstream_builder.AppendBits(2, 0); // Reserved zero 2bits u(2).
            bitstream_builder.AppendBits(8, kLevelIDC5p1); // level_idc u(8).
            break;
        case VAProfileH264High:
            profile_idc = kProfileIDCHigh;
            bitstream_builder.AppendBits(8, kProfileIDCHigh);
            bitstream_builder.AppendBool(0); // Constraint Set0 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set1 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set2 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set3 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set4 Flag u(1).
            bitstream_builder.AppendBool(0); // Constraint Set5 Flag u(1).
            bitstream_builder.AppendBits(2, 0); // Reserved zero 2bits u(2).
            bitstream_builder.AppendBits(8, kLevelIDC5p1); // level_idc u(8).
            break;
        // TODO(b/328430784): Support additional H264 profiles.
        default: CHECK(false); break;
        }

        // TODO(b/328430784): find a way to get the seq_parameter_set_id.
        bitstream_builder.AppendUE(0); // seq_parameter_set_id ue(v).

        if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244
            || profile_idc == 44 || profile_idc == 83 || profile_idc == 86 || profile_idc == 118
            || profile_idc == 128 || profile_idc == 138 || profile_idc == 139 || profile_idc == 134
            || profile_idc == 135) {
            bitstream_builder.AppendUE(
                pic_param_buffer->seq_fields.bits.chroma_format_idc); // chroma_format_idc ue(v).
            if (pic_param_buffer->seq_fields.bits.chroma_format_idc == 3) {
                bitstream_builder.AppendBool(0); // separate_colour_plane_flag u(1).
            }
            bitstream_builder.AppendUE(
                pic_param_buffer->bit_depth_chroma_minus8); // bit_depth_luma_minus8 ue(v).
            bitstream_builder.AppendUE(
                pic_param_buffer->bit_depth_luma_minus8); // bit_depth_chroma_minus8 ue(v).
            bitstream_builder.AppendBool(0); // qpprime_y_zero_transform_bypass_flag u(1).
            bitstream_builder.AppendBool(0); // seq_scaling_matrix_present_flag u(1).
            if (0) {
                for (int i = 0;
                     i < ((pic_param_buffer->seq_fields.bits.chroma_format_idc == 3) ? 12 : 8);
                     i++) {
                    bitstream_builder.AppendBool(0); // seq_scaling_list_present_flag u(1).
                    if (0) {
                        // TODO: Implement scaling list.
                        // H264 7.3.2.1.1.1 Scaling list syntax
                    }
                }
            }
        }

        bitstream_builder.AppendUE(
            pic_param_buffer->seq_fields.bits
                .log2_max_frame_num_minus4); // log2_max_frame_num_minus4 ue(v).
        bitstream_builder.AppendUE(
            pic_param_buffer->seq_fields.bits.pic_order_cnt_type); // pic_order_cnt_type ue(v).

        if (pic_param_buffer->seq_fields.bits.pic_order_cnt_type == 0) {
            // log2_max_pic_order_cnt_lsb_minus4 ue(v).
            bitstream_builder.AppendUE(
                pic_param_buffer->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4);
        } else if (pic_param_buffer->seq_fields.bits.pic_order_cnt_type == 1) {
            // Ignoring the content of this branch as we don't produce
            // pic_order_cnt_type == 1.
            CHECK(false);
        }

        bitstream_builder.AppendUE(pic_param_buffer->num_ref_frames); // num_ref_frames ue(v).
        bitstream_builder.AppendBool(
            // gaps_in_frame_num_value_allowed_flag u(1).
            pic_param_buffer->seq_fields.bits.gaps_in_frame_num_value_allowed_flag);
        bitstream_builder.AppendUE(
            pic_param_buffer->picture_width_in_mbs_minus1); // pic_width_in_mbs_minus1 ue(v).
        bitstream_builder.AppendUE(
            pic_param_buffer->picture_height_in_mbs_minus1); // pic_height_in_map_units_minus1
                                                             // ue(v).
        bitstream_builder.AppendBool(
            pic_param_buffer->seq_fields.bits.frame_mbs_only_flag); // frame_mbs_only_flag u(1).
        if (!pic_param_buffer->seq_fields.bits.frame_mbs_only_flag) {
            bitstream_builder.AppendBool(
                pic_param_buffer->seq_fields.bits
                    .mb_adaptive_frame_field_flag); // mb_adaptive_frame_field_flag
                                                    // u(1).
        }

        bitstream_builder.AppendBool(
            pic_param_buffer->seq_fields.bits
                .direct_8x8_inference_flag); // direct_8x8_inference_flag u(1).

        // TODO(b/328430784): find a way to get these values.
        bitstream_builder.AppendBool(0); // frame_cropping_flag u(1).
        bitstream_builder.AppendBool(1); // vui_parameters_present_flag u(1).
        if (1) {
            // Annex E.1: VUI parameters syntax
            bitstream_builder.AppendBool(1); // aspect_ratio_info_present_flag u(1).
            if (1) {
                bitstream_builder.AppendBits(8, 1); // aspect_ratio_idc u(8).
                if (1 == 255) {
                    bitstream_builder.AppendBits(16, 0); // sar_width u(16).
                    bitstream_builder.AppendBits(16, 0); // sar_height u(16).
                }
            }
            bitstream_builder.AppendBool(0); // overscan_info_present_flag u(1).
            bitstream_builder.AppendBool(0); // video_signal_type_present_flag u(1).
            bitstream_builder.AppendBool(0); // chroma_loc_info_present_flag u(1).
            bitstream_builder.AppendBool(0); // timing_info_present_flag u(1).
            bitstream_builder.AppendBool(0); // nal_hrd_parameters_present_flag u(1).
            bitstream_builder.AppendBool(0); // vcl_hrd_parameters_present_flag u(1).
            if (bool(0) || bool(0)) {
                bitstream_builder.AppendBool(0); // low_delay_hrd_flag u(1).
            }
            bitstream_builder.AppendBool(0); // pic_struct_present_flag u(1).
            bitstream_builder.AppendBool(0); // bitstream_restriction_flag u(1).
            if (0) {
                bitstream_builder.AppendBool(0); // motion_vectors_over_pic_boundaries_flag u(1).
                bitstream_builder.AppendUE(0); // max_bytes_per_pic_denom ue(v).
                bitstream_builder.AppendUE(0); // max_bits_per_mb_denom ue(v).
                bitstream_builder.AppendUE(0); // log2_max_mv_length_horizontal ue(v).
                bitstream_builder.AppendUE(0); // log2_max_mv_length_vertical ue(v).
                bitstream_builder.AppendUE(0); // num_reorder_frames ue(v).
                bitstream_builder.AppendUE(0); // max_dec_frame_buffering ue(v).
            }
        }

        bitstream_builder.FinishNALU();
    }

    void BuildPackedH264PPS(const VAPictureParameterBufferH264 *pic_param_buffer,
        std::vector<const VSBuffer *> slice_param_buffers, const VAProfile profile,
        H264BitstreamBuilder &bitstream_builder)
    {
        // Build NAL header following spec section 7.3.1.
        bitstream_builder.BeginNALU(H264NALU::kPPS, 3);

        // Build PPS following spec section 7.3.2.2.

        // TODO(b/328430784): find a way to get these values.
        bitstream_builder.AppendUE(0); // pic_parameter_set_id ue(v).
        bitstream_builder.AppendUE(0); // seq_parameter_set_id ue(v).

        bitstream_builder.AppendBool(
            pic_param_buffer->pic_fields.bits
                .entropy_coding_mode_flag); // entropy_coding_mode_flag u(1).
        bitstream_builder.AppendBool(pic_param_buffer->pic_fields.bits
                                         .pic_order_present_flag); // pic_order_present_flag u(1).

        // TODO(b/328430784): find a way to get this value.
        bitstream_builder.AppendUE(0); // num_slice_groups_minus1 ue(v).

        CHECK(!slice_param_buffers.empty());
        const VASliceParameterBufferH264 *sliceParam
            = reinterpret_cast<VASliceParameterBufferH264 *>(slice_param_buffers[0]->GetData());

        // TODO(b/328430784): we don't have access to the
        // num_ref_idx_l0_default_active_minus1 and
        // num_ref_idx_l1_default_active_minus1 syntax elements here. Instead, we use
        // the num_ref_idx_l0_active_minus1 and num_ref_idx_l1_active_minus1 from the
        // first slice. This may be good enough for now but will probably not work in
        // general. Figure out what to do.
        bitstream_builder.AppendUE(4); // num_ref_idx_l0_default_active_minus1 ue(v).
        bitstream_builder.AppendUE(0); // num_ref_idx_l1_default_active_minus1 ue(v).

        bitstream_builder.AppendBool(
            pic_param_buffer->pic_fields.bits.weighted_pred_flag); // weighted_pred_flag u(1).
        bitstream_builder.AppendBits(
            2, pic_param_buffer->pic_fields.bits.weighted_bipred_idc); // weighted_bipred_idc u(2).
        bitstream_builder.AppendSE(
            pic_param_buffer->pic_init_qp_minus26); // pic_init_qp_minus26 se(v).
        bitstream_builder.AppendSE(
            pic_param_buffer->pic_init_qs_minus26); // pic_init_qs_minus26 se(v).
        bitstream_builder.AppendSE(
            pic_param_buffer->chroma_qp_index_offset); // chroma_qp_index_offset se(v).

        // deblocking_filter_control_present_flag u(1).
        bitstream_builder.AppendBool(
            pic_param_buffer->pic_fields.bits.deblocking_filter_control_present_flag);
        bitstream_builder.AppendBool(
            pic_param_buffer->pic_fields.bits
                .constrained_intra_pred_flag); // constrained_intra_pred_flag u(1).
        bitstream_builder.AppendBool(
            pic_param_buffer->pic_fields.bits
                .redundant_pic_cnt_present_flag); // redundant_pic_cnt_present_flag
                                                  // u(1).

        bitstream_builder.FinishNALU();
    }

} // namespace

// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;

DWLInstance::DWLInstance(u32 client_type)
{
    DWLInitParam param;
    param.client_type = client_type;
    instance = nullptr;
    instance = DWLInit(&param);
}

DWLInstance::~DWLInstance() { DWLRelease(instance); }

H264DecoderDelegate::H264DecoderDelegate(
    int picture_width_hint, int picture_height_hint, VAProfile profile)
    : profile_(profile), ts_to_render_target_(kTimestampCacheSize)
{
    dwl_instance_ = std::make_unique<DWLInstance>(DWL_CLIENT_TYPE_H264_DEC);
    H264DecConfig dec_config;
    memset(&dec_config, 0, sizeof(dec_config));
    dec_config.dpb_flags = DEC_REF_FRM_RASTER_SCAN;
    dec_config.decoder_mode = DEC_NORMAL;
    dec_config.error_handling = DEC_EC_FAST_FREEZE;
    dec_config.no_output_reordering = 1;
    dec_config.use_display_smoothing = 0;
    dec_config.use_video_compressor = 0;
    dec_config.use_adaptive_buffers = 1;
    dec_config.guard_size = 0;
    auto ret = H264DecInit(
        const_cast<const void **>(&hw_decoder_), dwl_instance_->instance, &dec_config);
    std::cerr << "HW Decoder Initialized. Return code: " << ret << std::endl;
}

H264DecoderDelegate::~H264DecoderDelegate() { H264DecRelease(hw_decoder_); }

void H264DecoderDelegate::SetRenderTarget(const VSSurface &surface)
{
    render_target_ = &surface;
    ts_to_render_target_.Put(current_ts_, &surface);
}

void H264DecoderDelegate::EnqueueWork(const std::vector<const VSBuffer *> &buffers)
{
    CHECK(render_target_);
    CHECK(slice_data_buffers_.empty());
    for (auto buffer : buffers) {
        switch (buffer->GetType()) {
        case VASliceDataBufferType: slice_data_buffers_.push_back(buffer); break;
        case VAPictureParameterBufferType: pic_param_buffer_ = buffer; break;
        case VAIQMatrixBufferType: matrix_buffer_ = buffer; break;
        case VASliceParameterBufferType: slice_param_buffers_.push_back(buffer); break;
        default: break;
        };
    }
}

void H264DecoderDelegate::Run()
{
    H264BitstreamBuilder bitstream_builder;

    CHECK(pic_param_buffer_);
    const VAPictureParameterBufferH264 *pic_param_buffer
        = reinterpret_cast<VAPictureParameterBufferH264 *>(pic_param_buffer_->GetData());

    if (reinterpret_cast<VASliceParameterBufferH264 *>(slice_param_buffers_[0]->GetData())
            ->slice_type
        == 2 /*SPS PPS is only before I frame*/) {
        BuildPackedH264SPS(pic_param_buffer, slice_param_buffers_, profile_, bitstream_builder);
        BuildPackedH264PPS(pic_param_buffer, slice_param_buffers_, profile_, bitstream_builder);
    }

    {
        std::ofstream bitstream_file("bitstream0.h264", std::ios::binary | std::ios::trunc);
        if (bitstream_file.is_open()) {
            bitstream_file.write(reinterpret_cast<const char *>(bitstream_builder.data()),
                bitstream_builder.BytesInBuffer());
            bitstream_file.close();
        } else {
            std::cerr << "Unable to open bitstream file for writing." << std::endl;
        }
    }

    for (const auto &slice_data_buffer : slice_data_buffers_) {
        // Add the H264 start code for each slice.
        bitstream_builder.AppendBits(32, 0x00000001);
        const uint8_t *data = reinterpret_cast<uint8_t *>(slice_data_buffer->GetData());
        for (size_t i = 0; i < slice_data_buffer->GetDataSize(); i++) {
            bitstream_builder.AppendBits<uint8_t>(8, data[i]);
        }
    }

    bitstream_builder.Flush();

    // Dump bitstream to file for debugging.
    {
        std::ofstream bitstream_file("bitstream.h264", std::ios::binary | std::ios::app);
        if (bitstream_file.is_open()) {
            bitstream_file.write(reinterpret_cast<const char *>(bitstream_builder.data()),
                bitstream_builder.BytesInBuffer());
            bitstream_file.close();
        } else {
            std::cerr << "Unable to open bitstream file for writing." << std::endl;
        }
    }

    // Invoke HW Decoder
    H264DecInput input;
    input.skip_non_reference = 0;

    DWLLinearMem stream_mem;
    memset(&stream_mem, 0, sizeof(stream_mem));
    stream_mem.mem_type = DWL_MEM_TYPE_SLICE;
    DWLMallocLinear(dwl_instance_->instance, bitstream_builder.BytesInBuffer() * 2, &stream_mem);
    input.stream = reinterpret_cast<uint8_t *>(stream_mem.virtual_address);
    input.stream_bus_address = stream_mem.bus_address;
    input.data_len = bitstream_builder.BytesInBuffer();
    memcpy(stream_mem.virtual_address, bitstream_builder.data(), bitstream_builder.BytesInBuffer());

    input.buffer
        = reinterpret_cast<u8 *>(reinterpret_cast<addr_t>(input.stream) & ~BUFFER_ALIGN_MASK);
    input.buffer_bus_address = input.stream_bus_address & ~BUFFER_ALIGN_MASK;
    input.buff_len = input.data_len + (input.stream_bus_address & BUFFER_ALIGN_MASK);
    input.pic_id = current_ts_++;

    input.p_user_data = stream_mem.virtual_address;

    H264DecOutput output;
    memset(&output, 0, sizeof(output));
    std::cerr << "HW Decoder Started" << std::endl;
    bool ok = 0, fail = 0;
    do {
        auto ret = H264DecDecode(hw_decoder_, &input, &output);
        std::cerr << "HW Decoder Return: " << ret << std::endl;
        switch (ret) {
        case DEC_STREAM_NOT_SUPPORTED: {
            fail = true;
            break;
        }
        case DEC_HDRS_RDY: break;
        case DEC_PENDING_FLUSH:
        case DEC_PIC_DECODED: {
            H264DecPicture picture;
            for (;;) {
                auto ret = H264DecNextPicture(hw_decoder_, &picture, 0);
                std::cerr << "HW Decoder Next Picture Return: " << ret << std::endl;
                if (ret == DEC_PIC_RDY || ret == DEC_FLUSHED) {
                    OnFrameReady(picture);
                    H264DecPictureConsumed(hw_decoder_, &picture);
                } else
                    break;
            }
            break;
        }
        case DEC_STRM_PROCESSED: {
            // All data has been processed, we can stop the loop.
            ok = true;
            break;
        }
        case DEC_OK:
            /* nothing to do, just call again */
            break;
        case DEC_WAITING_FOR_BUFFER: {
            DWLLinearMem mem;
            H264DecBufferInfo buffer_info;
            H264DecGetBufferInfo(hw_decoder_, &buffer_info);
            std::cerr << "HW Decoder Buffer Info:\n"
                      << "\t Buf to free:" << std::hex << buffer_info.buf_to_free.virtual_address
                      << "\n"
                      << "\t Next buf size:" << std::dec << buffer_info.next_buf_size
                      << "\n\t Buf num:" << std::dec << buffer_info.buf_num << std::endl;

            for (int i = 0; i < buffer_info.buf_num; i++) {
                mem.mem_type = DWL_MEM_TYPE_DPB;
                DWLMallocLinear(dwl_instance_->instance, buffer_info.next_buf_size, &mem);
                H264DecAddBuffer(hw_decoder_, &mem);
            }
            break;
        }
        default: {
            std::cerr << "HW Decoder Error: " << ret << std::endl;
            fail = true;
            break;
        }
        }
        // update input stream
        input.stream = output.strm_curr_pos;
        input.data_len = output.data_left;
        input.stream_bus_address = output.strm_curr_bus_address;

    } while (!ok && !fail);

    std::cerr << "HW Decoder Stopped" << std::endl;
    if (fail) { H264DecAbort(hw_decoder_); }
    DWLFreeLinear(dwl_instance_->instance, &stream_mem);
    slice_data_buffers_.clear();
    slice_param_buffers_.clear();
}

void H264DecoderDelegate::OnFrameReady(H264DecPicture picture)
{
    const uint32_t ts = picture.pic_id;
    std::cerr << "Picture Id: " << ts << std::endl;
    // auto render_target_it = ts_to_render_target_.Peek(ts);
    // CHECK(render_target_it != ts_to_render_target_.end());
    // const VSSurface *render_target = render_target_it->second;
    // CHECK(render_target);

    // const ScopedBOMapping &bo_mapping = render_target->GetMappedBO();
    // CHECK(bo_mapping.IsValid());
    // const ScopedBOMapping::ScopedAccess mapped_bo = bo_mapping.BeginAccess();
    for (int i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if (picture.pictures[i].pic_width == 0 || picture.pictures[i].pic_height == 0) continue;
        std::cerr << "Picture " << i << ": width=" << picture.pictures[i].pic_width
                  << ", height=" << picture.pictures[i].pic_height << std::endl;
        // TODO: Copy the data from the picture to the render target.
    }
    //;
}

} // namespace libvavc8000d

void H264DecTrace(const char *string) { std::cerr << "[TRACE]" << string << std::endl; }