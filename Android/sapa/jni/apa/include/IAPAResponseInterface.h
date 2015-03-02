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

#ifndef ANDROID_APARESPONSEINTERFACE_H
#define ANDROID_APARESPONSEINTERFACE_H

#include <stdint.h>

/**
 * \brief An interface for sending message to UI
 * 
 * It is used for sending message to UI(Android java components)@n
 * You don't need to use this interface. @n
 * All of function on it are wrapping in the IAPAInterface.@n
 * Please use IAPAInterface.@n
 * When you want to send message, Use the IAPAInterface.response@n
 */
class IAPAResponseInterface{
	public:
		/** 
		 * Constructor
		 */
		IAPAResponseInterface(){};
		
		/*
		 * Destructor
		 */
		virtual ~IAPAResponseInterface(){};

		/**
		 * Send a message to UI
		 * @param message a text message
		 */
		virtual void responseToService(const char* message) = 0;

		/**
		 * Send a data to UI
		 * @param ext1 an extra parameter
		 * @param len the written data size
		 * @param data the sending data pointer
		 */
		virtual void responseToService(const int32_t ext1, const size_t len, void const* data) = 0;

		/**
		 * Send a message to UI
		 * @param id an id
		 * @param message a text message
		 */
		virtual void responseToService(const int id, const char* message) = 0;



};

#endif // #ifndef ANDROID_APARESPONSEINTERFACE_H


