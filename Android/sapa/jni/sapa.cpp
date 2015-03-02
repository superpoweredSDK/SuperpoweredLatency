#include <string.h>
#include <stdio.h>
#include "sapa.h"

namespace android {

IMPLEMENT_APA_INTERFACE(APAWave)

APAWave::APAWave() {}

APAWave::~APAWave() {}

int APAWave::init() {
	return APA_RETURN_SUCCESS;
}

int APAWave::sendCommand(const char *command) {
    if (command) {
        if (command[0] == 't') sapaClient::toggleMeasurer();
        else if (command[0] == 'p') sapaClient::togglePassThrough();
    };
	return APA_RETURN_SUCCESS;
}

// Samsung Professional Audio SDK is based on the JACK audio connectivity library (a media server).
// We return with our JACK client's class here.
IJackClientInterface* APAWave::getJackClientInterface() {
    return &client;
}

// The queryData method in Java arrives here.
int APAWave::request(const char *what, const long ext1, const long capacity, size_t &len, void *data) {
    int value = 0;
    switch (what[0]) {
        case 's': value = sapaClient::getState(); break;
        case 'l': value = sapaClient::getLatencyMs(); break;
        case 'r': value = sapaClient::getSamplerate(); break;
        case 'b': value = sapaClient::getBuffersize(); break;
    };
    
    len = sizeof(value);
    memcpy(data, &value, len);
	return APA_RETURN_SUCCESS;
}

};
