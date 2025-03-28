/*
 * Copyright (C) 2021 Alibaba Group Holding Limited
 * Author: LuChongzhi <chongzhi.lcz@alibaba-inc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CSI_VENC_H__
#define __CSI_VENC_H__

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "csi_common.h"
#include "csi_allocator.h"
#include "csi_vcodec_common.h"
#include "csi_venc_h264.h"
#include "csi_venc_h265.h"
#include "csi_venc_mjpeg.h"
#include "csi_venc_property.h"
#include "csi_frame.h"
#include "csi_frame_ex.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define CSI_VENC_VERSION_MAJOR 1
#define CSI_VENC_VERSION_MINOR 0

typedef void *csi_venc_dev_t;
typedef void *csi_venc_chn_t;
typedef void *csi_venc_event_handle_t;

typedef enum csi_venc_status {
	CSI_VENC_STATUS_OK = 0,
	CSI_VENC_STATUS_MORE_FRAME_BUFFER = 1,
	CSI_VENC_STATUS_MORE_BITSTREAM = 2,
	CSI_VENC_STATUS_PIC_ENCODED = 4,
	CSI_VENC_STATUS_EOS = 6,

	/* error codes < 0 */
	CSI_VENC_ERR_UNKOWN = -1,
	CSI_VENC_ERR_UNSUPPORTED = -2,
	CSI_VENC_ERR_INSUFFICIENT_RESOURCES = -3,
	CSI_VENC_ERR_WRONG_PARAM = -4,
	CSI_VENC_ERR_BAD_STREAM = -5,
	CSI_VENC_ERR_NOT_INITIALIZED = -6,
	CSI_VENC_ERR_MEMORY = -7,

	CSI_VENC_ERR_INVALID_STATE = -8,
	CSI_VENC_HW_BUS_ERROR = -9,
    CSI_VENC_HW_DATA_ERROR = -10,
    CSI_VENC_HW_TIMEOUT = -11,
    CSI_VENC_SYSTEM_ERROR = -12,
    CSI_VENC_HW_RESET = -13,
	/* add more ... */

	/* warnings > 255 */
	CSI_VENC_WRN_DEVICE_BUSY = 256,
	CSI_VENC_WRN_INCOMPATIBLE_PARAM = 257,
	CSI_VENC_WRN_NOT_IMPLEMENTED = 258,
	/* add more ... */
} csi_venc_status_e;

#define CSI_VENC_NAME_MAX_LEN 32
typedef struct csi_venc_info {
	char module_name[CSI_VENC_NAME_MAX_LEN];
	char device_name[CSI_VENC_NAME_MAX_LEN];
	uint64_t capabilities;	 /* bitmask of 1<<(csi_vcodec_id_e) */
} csi_venc_info_s;

#define CSI_VENC_MAX_COUNT 2
typedef struct csi_venc_infos {
	uint32_t count;
	csi_venc_info_s info[CSI_VENC_MAX_COUNT];
} csi_venc_infos_s;

typedef enum csi_venc_event_type {
	CSI_VENC_EVENT_TYPE_ENCODER,
	CSI_VENC_EVENT_TYPE_CHANNEL,
} csi_venc_event_type_e;

/* the attribute of the roi */
typedef struct csi_venc_chn_roi_prop {
	uint32_t	index;	/* Range:[0, 7]; Index of an ROI. The system supports indexes ranging from 0 to 7 */
	bool		update; /* Range:[0, 1]; Whether to update this ROI incluing qp/region changed*/
	bool		enable;	/* Range:[0, 1]; Whether to enable this ROI ,must config enable before stream start,enable status cannot changed once started */
	bool		abs_qp;	/* Range:[0, 1]; QP mode of an ROI.HI_FALSE: relative QP.HI_TURE: absolute QP.*/
	int32_t		qp;		/* Range:[-51, 51]; QP value,only relative mode can QP value less than 0. */
	csi_rect_s	rect;	/* Region of an ROI*/
} csi_venc_chn_roi_prop_s;

typedef struct csi_venc_chn_intra_prop {
	bool		update; /* Range:[0, 1]; Whether to update this ROI */
	bool		enable;	/* Range:[0, 1]; Whether to enable this ROI */
	csi_rect_s	rect;	/* Region of an ROI*/
} csi_venc_chn_intra_prop_s;

typedef enum csi_venc_ext_property_id {
	CSI_VENC_EXT_PROPERTY_ROI,
	CSI_VENC_EXT_PROPERTY_INTRA,
} csi_venc_ext_property_id_e;

#define MAX_ROI_AREA_CNT 8

typedef struct csi_venc_chn_ext_property {
	csi_venc_ext_property_id_e prop_id;
	union {
		csi_venc_chn_roi_prop_s roi_prop[MAX_ROI_AREA_CNT];	//user can change one roi or multi roi once
		csi_venc_chn_intra_prop_s intra_prop;
	};
} csi_venc_chn_ext_property_s;

typedef enum csi_venc_event_id {
	CSI_VENC_EVENT_ID_ERROR	= 1 << 0,
} csi_venc_event_id_e;

typedef enum csi_venc_chn_event_id {
	CSI_VENC_CHANNEL_EVENT_ID_ERROR		= 1 << 0,
	CSI_VENC_CHANNEL_EVENT_ID_FRAME_READY	= 1 << 1,
} csi_venc_chn_event_id_e;

typedef struct csi_venc_event_subscription {
	csi_venc_event_type_e type;
	unsigned int id;	/* bitmasks */
} csi_venc_event_subscription_s;

typedef struct csi_venc_event {
	csi_venc_event_type_e	type;
	unsigned int		id;
	struct timespec		timestamp;
	union {
		char bin[128];
	};
} csi_venc_event_s;

typedef union csi_venc_data_type {
	csi_venc_h264_nalu_e h264_type;
	csi_venc_h265_nalu_e h265_type;
	csi_venc_jpeg_pack_e jpeg_type;
} csi_venc_data_type_u;

typedef struct csi_stream {
	size_t		size;
	uint32_t	width;
	uint32_t	height;
	void	*usr_addr;		// stores in usr contigous memory
	union {
		int		buf_fd;			// stores in dma_buf memory
		int		phy_addr;		// stores in phy contigous memory
	};
	uint64_t	pts;
	bool		stream_end;
	csi_venc_data_type_u	data_type;
	uint32_t	data_num;
} csi_stream_s;


typedef enum csi_venc_prop_type {
	CSI_VENC_FRAME_PROP_NONE = 0,
	CSI_VENC_FRAME_PROP_FORCE_IDR,	// Instantaneous Encoding Refresh
	CSI_VENC_FRAME_PROP_FORCE_SKIP,
} csi_venc_prop_type_e;

typedef struct csi_venc_prop {
	csi_venc_prop_type_e type;
	union {
		bool force_idr;		// CSI_VENC_FRAME_PROP_FORCE_IDR
		bool force_skip;	// CSI_VENC_FRAME_PROP_FORCE_SKIP
	};
} csi_venc_frame_prop_s;

/* the status of the venc chnl*/
typedef struct csi_venc_chn_status {
	uint32_t left_pics;		/* R; left picture number */
	uint32_t left_stream_bytes;	/* R; left stream bytes*/
	uint32_t left_stream_frames;	/* R; left stream frames*/
	uint32_t cur_packs;		/* R; pack number of current frame*/
	uint32_t left_recv_pics;	/* R; Number of frames to be received. This member is valid after HI_MPI_VENC_StartRecvPicEx is called.*/
	uint32_t left_enc_pics;		/* R; Number of frames to be encoded. This member is valid after HI_MPI_VENC_StartRecvPicEx is called.*/
	bool     jpeg_snap_end;		/* R; the end of Snap.*/
} csi_venc_chn_status_s;

typedef enum csi_venc_input_mode {
	CSI_VENC_INPUT_MODE_STREAM,
	CSI_VENC_INPUT_MODE_FRAME,
} csi_venc_input_mode_e;

typedef enum csi_venc_pp_rotate {
	CSI_VENC_PP_ROTATE_0,
	CSI_VENC_PP_ROTATE_90,
	CSI_VENC_PP_ROTATE_180,
	CSI_VENC_PP_ROTATE_270
} csi_venc_pp_rotate_t;

typedef struct csi_venc_pp_config {
	csi_venc_pp_rotate_t rotate;
	bool h_flip;
	bool v_flip;
	csi_rect_s crop;	/* width or height to be zero means no crop */
} csi_venc_pp_config_s;

typedef struct csi_venc_chn_cfg {
	csi_venc_attr_s			attr;
	csi_venc_gop_property_s	gop;
	csi_venc_rc_property_s	rc;
	csi_venc_pp_config_s prep_cfg;
	csi_venc_chn_roi_prop_s roi_prop[MAX_ROI_AREA_CNT]; /*For user set enable,must config enable before stream start*/
} csi_venc_chn_cfg_s;

int csi_venc_get_version(csi_api_version_u *version);
int csi_venc_query_list(csi_venc_infos_s *infos);

int csi_venc_open(csi_venc_dev_t *enc, const char *device_name);
int csi_venc_close(csi_venc_dev_t enc);

int csi_venc_get_io_pattern(csi_venc_dev_t enc, int *pattern);
int csi_venc_get_frame_config(csi_venc_dev_t enc, csi_img_format_t *img_fmt, csi_frame_config_s *frm_cfg);
int csi_venc_set_frame_config(csi_venc_dev_t enc, csi_img_format_t *img_fmt, csi_frame_config_s *frm_cfg);



int csi_venc_create_channel(csi_venc_chn_t *chn, csi_venc_dev_t enc, csi_venc_chn_cfg_s *cfg);
int csi_venc_destory_channel(csi_venc_chn_t chn);

//int csi_venc_set_memory_allocator(csi_venc_chn_t chn, csi_allocator_s *allocator);

int csi_venc_set_ext_property(csi_venc_chn_t chn, csi_venc_chn_ext_property_s *prop);
int csi_venc_get_ext_property(csi_venc_chn_t chn, csi_venc_chn_ext_property_s *prop);

int csi_venc_start(csi_venc_chn_t chn);
int csi_venc_stop(csi_venc_chn_t chn);
int csi_venc_reset(csi_venc_chn_t chn);

int csi_venc_send_frame(csi_venc_chn_t chn, csi_frame_ex_s *frame, int timeout);

/**
 * one source frame, multi-output stream with diffent config(crop parameter, etc..).
 * cunnent only support JPEG encode.
 */
int csi_venc_send_frame_batch(csi_venc_chn_t chn, csi_frame_ex_s *frame, csi_venc_pp_config_s *cfg, csi_stream_s *output_stream, int count, int timeout);
int csi_venc_send_frame_ex(csi_venc_chn_t chn, csi_frame_ex_s *frame, int timeout,
			   csi_venc_frame_prop_s *prop, int prop_count);


int csi_venc_enqueue_frame(csi_venc_chn_t chn, csi_frame_ex_s *frame);
int csi_venc_enqueue_frame_ex(csi_venc_chn_t chn, csi_frame_ex_s *frame,
			      csi_venc_frame_prop_s *prop, int prop_count);
int csi_venc_dequeue_frame(csi_venc_chn_t chn, csi_frame_ex_s **frame, int timeout);


int csi_venc_get_stream(csi_venc_chn_t chn, csi_stream_s *stream, int timeout);// Release by stream.release()

int csi_venc_query_status(csi_venc_chn_t chn, csi_venc_chn_status_s *status);

int csi_venc_create_event_handle(csi_venc_event_handle_t *chn, csi_venc_dev_t event_handle);
int csi_venc_destory_event(csi_venc_event_handle_t event_handle);

int csi_venc_subscribe_event(csi_venc_event_handle_t event_handle,
			     csi_venc_event_subscription_s *subscribe);
int csi_venc_unsubscribe_event(csi_venc_event_handle_t event_handle,
			       csi_venc_event_subscription_s *subscribe);
int csi_venc_get_event(csi_venc_event_handle_t event_handle,
		       csi_venc_event_s *event, int timeout);

int csi_venc_set_pp_config(csi_venc_chn_t chn, csi_venc_pp_config_s *cfg);
int csi_venc_get_chn_config(csi_venc_chn_t chn, csi_venc_chn_cfg_s *cfg);
int csi_venc_set_chn_config(csi_venc_chn_t chn, csi_venc_chn_cfg_s *cfg);

#ifdef  __cplusplus
}
#endif

#endif /* __CSI_ENC_H__ */
