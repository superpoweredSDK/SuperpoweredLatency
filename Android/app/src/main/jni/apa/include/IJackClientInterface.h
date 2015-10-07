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

#ifndef ANDROID_JACK_CLIENT_INTERFACE_H
#define ANDROID_JACK_CLIENT_INTERFACE_H

#include <APACommon.h>
namespace android {

/**
 * \brief An interface for a JACK client
 * 
 * It is used for making Jack client on the APA client.
 */
class IJackClientInterface {
	public:

		/**
		 * Constructor
		 */
		IJackClientInterface(){};

		/**
		 * Destructor
		 */
		virtual ~IJackClientInterface(){};

		/**
		 * After loading your Wave module, It will be called when user call SapaService.register() after calling IAPAInterface.init() @n
		 * Initialize your client. @n
		 * by default, argc == 1, argv[0] == the alias of package name( the last name seperated by dot ) @n
		 * If you send extra parameters by newClient(), It will be add next. @n
		 * @image html IAPAInterface_register.jpg "IJackClientInterface setUp sequence diagram"
		 * @param argc argument count
		 * @param argv arguments sent from the SapaProcessor constructor parameters, or plugin mainfest file
		 * @return if success, use APA_RETURN_SUCCESS
		 */
		virtual int setUp(int argc, char *argv[]) = 0;

		/**
		 * Exit point @n
		 * It will be called when user call SapaService.unregister() or the process suddenly died.@n
		 * @image html IJackClientInterface_tearDown.jpg "IJackClientInterface tearDown sequence diagram"
		 * Release resources.@n
		 * @return if success, use APA_RETURN_SUCCESS
		 */
		virtual int tearDown() = 0;

		/**
		 * Activate jack client@n
		 * It will be called when user call SapaProcessor.activate().@n
		 * @image html IJackClientInterface_activate.jpg "IJackClientInterface  activate sequence diagram"
		 * @return if success, use APA_RETURN_SUCCESS
		 */
		virtual int activate() = 0;

		/**
		 * Deactivate jack client@n
		 * It will be called when user call SapaProcessor.deactivate().@n
		 * @image html IJackClientInterface_deactivate.jpg "IJackClientInterface deactivate sequence diagram"
		 * @return if success, use APA_RETURN_SUCCESS
		 */
		virtual int deactivate() = 0;

		/**
		 * Enable or disable transport feature of jack.@n
		 * It will be called when user call SapaProcessor.setTransportEnabled(). @n
		 * @image html IJackClientInterface_transport.jpg "IJackClientInterface transport sequence diagram"
		 * @param type TRANSPORT_OFF, TRANSPORT_ON
		 * @return if success, use APA_RETURN_SUCCESS
		 */
		virtual int transport(TransportType type) = 0;

		/**
		 * Receive a MIDI string. @n
		 * It will be called when user call SapaProcessor.sendCommand() with parameter starting with "midi:".@n
		 * For example, if sendCommand("midi:aabbcc"), the "aabbcc" will be sent as parameter midi. @n
		 * @image html IJackClientInterface_sendMidi.jpg "IJackClientInterface sendMidi sequence diagram"
		 * 
		 * @param midi the midi string
		 * @return if success, use APA_RETURN_SUCCESS
		 */
		virtual int sendMidi(char* midi) = 0;
};

};

#endif // ANDROID_JACK_CLIENT_INTERFACE_H

