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

#ifndef ANDROID_APA_INTERFACE_H
#define ANDROID_APA_INTERFACE_H

#include <stdint.h>
#include <IAPAResponseInterface.h>

namespace android {

class IJackClientInterface;

/**
 * \brief An interface for a APA Client
 * 
 * This is the entry interface for processing module.@n
 * All of processing module has an APA Client.@n
 * You should implement this.@n
 * 
 */
class IAPAInterface {
	public:
		/**
		 * Constructor
		 */
		IAPAInterface(){mResponse = NULL;};

		/**
		 * Destructor
		 */
		virtual ~IAPAInterface(){};
		
		/**
		 * When user call SapaService.register(), @n
		 * It is called right after the library loaded@n
		 * @image html IAPAInterface_register.jpg "IJackClientInterface init sequence diagram"
		 * Initialize your APA Client.
		 * @return if success, use APA_RETURN_SUCCESS
		 */
		virtual int init() = 0; // called automatically

		/** 
		 * Handle a command from UI@n
		 * You can define your own command between UI and APA client.@n
		 * When user call SapaProcessor.sendCommand(), It is called.@n
		 * @image html IAPAInterface_sendCommand.jpg "IJackClientInterface sendCommand sequence diagram"
		 * @param command : command string 
		 * @return if success, use APA_RETURN_SUCCESS
		 */
		virtual int sendCommand(const char* command) = 0;

		/** 
		 * Return the jack client interface that you implemented.@n
		 * If you don't use the jack client, return null.
		 * @return jack interface
		 */
		virtual IJackClientInterface* getJackClientInterface() = 0;

		/**
		 * Set the response interface in the system automatically. @n
		 * So, Do NOT use.@n
		 * If user call this, The message or data can NOT be received in the UI.
		 * @param itf IAPAResponseInterface object
		 */
		void setResponseInterface(IAPAResponseInterface* itf){ mResponse = itf; }; // called automatically

		/**
		 * A message will be sent to UI@n
		 * If you want to send a message to UI, Call this.@n
		 * @image html IAPAInterface_response.jpg "IJackClientInterface response sequence diagram"
		 * @param message an sending text message
		 */
		inline void response(const char* message){if(mResponse != NULL) mResponse->responseToService(message);};

		/**
		 * An data will be sent to UI@n
		 * If you want to send a data to UI, Call this @n
		 * @image html IAPAInterface_response.jpg "IJackClientInterface response sequence diagram"
		 * @param ext1 an extra parameter, You can define
		 * @param len the written data size
		 * @param data the written data pointer
		 */
		inline void response(const int32_t ext1, const size_t len, void const* data){if(mResponse != NULL) mResponse->responseToService(ext1,len,data);};

		/**
		 * When user call SapaProcessor.queryData(), This will be called.@n
		 * A data in processing module can be delivered to UI.@n
		 * @image html IAPAInterface_request.jpg "IJackClientInterface request sequence diagram"
		 * 
		 * @param what the parameter 'query' in queryData() will be sent as a parameter 'what'. [in]
		 * @param ext1 the parameter 'extrra' in queryData() will be sent as a parameter 'ext1'. [in]
		 * @param capacity the maximum size of the data pointer[in]
		 * @param len the written data size [in/out]
		 * @param data the writeen data pointer [in/out]
		 * @return if success, use APA_RETURN_SUCCESS
		 * 
		 */
		virtual int request(const char* what, const long ext1, const long capacity, size_t &len, void* const data) = 0;

		/**
		 * A message will be sent to UI@n
		 * If you want to send a message to UI, Call this.@n
		 * @image html IAPAInterface_response.jpg "IJackClientInterface response sequence diagram"
		 * @param message an sending text message
		 * @param id an id
		 */
		inline void response(const char* message, const int id){if(mResponse != NULL) mResponse->responseToService(id, message);};

		
	private:
		IAPAResponseInterface* mResponse; // Don't need to release
};

#define APA_NDK_API_LEVEL (1)
#define APA_NDK_VERSION_NUMBER (1103)
#define DECLARE_APA_INTERFACE(NAME)							\
	extern "C" NAME* apa_create();							\
	extern "C" void apa_destroy(NAME* p);					\
	extern "C" int apa_get_api_level();						\
	extern "C" int apa_get_ndk_version();					\

#define IMPLEMENT_APA_INTERFACE(NAME)						\
	NAME* apa_create(){										\
		return new NAME();									\
	}														\
	void apa_destroy(NAME* p){								\
		delete p;											\
	}														\
	int apa_get_api_level(){								\
		return APA_NDK_API_LEVEL;							\
	}                                                       \
	extern "C" int apa_get_ndk_version(){					\
		return APA_NDK_VERSION_NUMBER;						\
	}
	

};

#endif // ANDROID_APA_INTERFACE_H
