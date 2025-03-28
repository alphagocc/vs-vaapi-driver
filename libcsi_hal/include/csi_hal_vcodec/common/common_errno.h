/*
 * Copyright 2018 C-SKY Microsystems Co., Ltd.
 * Copyright (C) 2021 Alibaba Group Holding Limited
 * Author: LuChongzhi <chongzhi.lcz@alibaba-inc.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef __COMMON_ERRNO_H__
#define __COMMON_ERRNO_H__
#include <errno.h>

typedef enum {
	FAILURE = -1,
	SUCCESS = 0,

	ERRNO_INVALID_PARAMETER,
	ERRNO_INVALID_STATE,

	ERRNO_INVALID_CLIENT_PACKAGE,
} common_errno_e;

#endif /* __COMMON_ERRNO_H__ */

