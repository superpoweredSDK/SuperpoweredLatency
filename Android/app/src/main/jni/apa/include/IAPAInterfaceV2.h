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

#ifndef ANDROID_APA_INTERFACEV2_H
#define ANDROID_APA_INTERFACEV2_H

#include <stdint.h>
#include <IAPAInterface.h>

namespace android {

/**
 * \brief An interface version 2 for a APA Client
 * 
 * This is the entry interface for processing module.@n
 * All of processing module has an APA Client.@n
 * You should implement this.@n
 * 
 */
class IAPAInterfaceV2 : public IAPAInterface {
	public:
		/**
		 * Constructor
		 */
		IAPAInterfaceV2();
		
		
		/**
		 * Destructor
		 */
		virtual ~IAPAInterfaceV2();
		
		/**
		 * Handle the stream data from the Java side
		 * @param stream a byte array 
		 * @param size stream's array size
		 * @param id the identification number.
		 * @return APA_RETURN_SUCCESS, APA_RETURN_ERROR
		 */
		virtual int sendStream(const void* stream, const long size, const int id) = 0;
		
		/**
		 * Return the Target device API Level.
		 */
		int getTargetAPILevel();
};

};

#endif // ANDROID_APA_INTERFACEV2_H