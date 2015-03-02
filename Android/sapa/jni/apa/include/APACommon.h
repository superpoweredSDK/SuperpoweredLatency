/*
 * Samsung Handset Platform
 * Copyright (c) 2007 Software Center, Samsung Electronics, Inc.
 * All rights reserved.
 * This software is the confidential and proprietary information
 * of Samsung Electronics, Inc. ("Confidential Information").  You
 * shall not disclose such Confidential Information and shall use
 * it only in accordance with the terms of the license agreement
 * you entered into with Samsung Electronics.
 */

/*
 * @author <a href="mailto:haeseok@samsung.com">HaeSeok Oh</a>
 * @author <a href="mailto:lh.bang@samsung.com">LaeHyuk Bang</a>
 * @author <a href="mailto:jeongyeon.kim@samsung.com">JeongYeon Kim</a>
 * @author <a href="mailto:dbs.park@samsung.com">DaeBeom Park</a>
 *
 */

#ifndef ANDROID_APA_COMMON_H
#define ANDROID_APA_COMMON_H

// This file is used in the JAM Framework and the JAM NDK.

#ifdef __cplusplus
extern "C" {
#endif

#define APA_RETURN_SUCCESS  0
#define APA_RETURN_ERROR    1
#define APA_RETURN_SUPPORT    2
#define APA_RETURN_NOT_SUPPORT    3



#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace android {
#endif

typedef enum _transport_type{
	TRANSPORT_OFF = 0,
	TRANSPORT_ON = 1
} TransportType;

#ifdef __cplusplus
};
#endif
#endif
