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

#ifndef ANDROID_JACK_CLIENT_INTERFACEV2_H
#define ANDROID_JACK_CLIENT_INTERFACEV2_H

#include <IJackClientInterface.h>
namespace android {

/**
 * \brief An interface version 2 for a JACK client
 * 
 * It is used for making Jack client on the APA client.
 */
class IJackClientInterfaceV2 : public IJackClientInterface {
	public:

		/**
		 * Constructor
		 */
		IJackClientInterfaceV2();

		/**
		 * Destructor
		 */
		virtual ~IJackClientInterfaceV2();

		/**
		 * Get the Jack Client Name @n
		 * @param name the SapaProcessor's full name. It is the unique name of all client.
		 * @return the Jack Client Name which is matched with the name parameter.
		 */
		virtual char* getJackClientName(const char* name) = 0;

		/**
		 * Enable/Disable the ByPass of jack client@n
		 * It will be called when user call SapaProcessor.setByPassEnabled().@n
		 * @param enable 
		 * @return void
		 */
		virtual void setByPassEnabled(bool enable) = 0;
};

};

#endif // ANDROID_JACK_CLIENT_INTERFACE_H


