#include "sapaClient.h"
#include "latencyMeasurer.h"
#include <android/log.h>

namespace android {

    static latencyMeasurer *measurer = NULL;

    // This is the main audio processing callback.
    int sapaClient::process(jack_nframes_t frames, void *arg) {
        sapaClient *self = (sapaClient *)arg;

        float *left = (float *)jack_port_get_buffer(self->leftInput, frames);
        float *right = (float *)jack_port_get_buffer(self->rightInput, frames);

        short int *intBuffer = self->stereo;
        for (int n = 0; n < frames; n++) {
            *intBuffer++ = (short int)(*left++ * 32767.0f);
            *intBuffer++ = (short int)(*right++ * 32767.0f);
        };

        measurer->processInput(self->stereo, jack_get_sample_rate(self->jackClient), frames);
        measurer->processOutput(self->stereo);

        intBuffer = self->stereo;
        left = (float *)jack_port_get_buffer(self->leftOutput, frames);
        right = (float *)jack_port_get_buffer(self->rightOutput, frames);
        static const float int_to_float = 1.0f / 32767.0f;
        for (int n = 0; n < frames; n++) {
            *left++ = float(*intBuffer++) * int_to_float;
            *right++ = float(*intBuffer++) * int_to_float;
        };

        return 0;
    }

    sapaClient::sapaClient() {
        leftInput = rightInput = leftOutput = rightOutput = NULL;
        jackClient = NULL;
        stereo = (short int *)malloc(4096 * 4); // an interleaved buffer
        measurer = new latencyMeasurer();
    }

    sapaClient::~sapaClient() {
        delete measurer;
        free(stereo);
    }

    // SAPA sets up audio here. We register two ports for the left and right channels.
    int sapaClient::setUp(int argc, char *argv[]) {
        jackClient = jack_client_open(argv[0], JackNullOption, NULL, NULL);
        if (jackClient == NULL) return APA_RETURN_ERROR;

        jack_set_process_callback(jackClient, process, this);

        leftInput = jack_port_register(jackClient, "inL", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        if (leftInput == NULL) return APA_RETURN_ERROR;
        rightInput = jack_port_register(jackClient, "inR", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        if (rightInput == NULL) return APA_RETURN_ERROR;

        leftOutput = jack_port_register(jackClient, "outL", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (leftOutput == NULL) return APA_RETURN_ERROR;
        rightOutput = jack_port_register(jackClient, "outR", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (rightOutput == NULL) return APA_RETURN_ERROR;

        return APA_RETURN_SUCCESS;
    }

    int sapaClient::tearDown() {
        jack_client_close(jackClient);
        return APA_RETURN_SUCCESS;
    }

    // Everything was set up, let's start audio.
    int sapaClient::activate() {
        jack_activate(jackClient);

        const char **systemPorts = jack_get_ports(jackClient, NULL, NULL, JackPortIsPhysical | JackPortIsOutput);
        if (systemPorts == NULL) return APA_RETURN_ERROR;
        jack_connect(jackClient, systemPorts[0], jack_port_name(leftInput));
        jack_connect(jackClient, systemPorts[1], jack_port_name(rightInput));
        free(systemPorts);

        systemPorts = jack_get_ports(jackClient, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
        if (systemPorts == NULL) return APA_RETURN_ERROR;
        jack_connect(jackClient, jack_port_name(leftOutput), systemPorts[0]);
        jack_connect(jackClient, jack_port_name(rightOutput), systemPorts[1]);
        free(systemPorts);

        return APA_RETURN_SUCCESS;
    }
    
    void sapaClient::toggleMeasurer() {
        if (measurer) measurer->toggle();
    }
    
    void sapaClient::togglePassThrough() {
        if (measurer) measurer->togglePassThrough();
    }

    int sapaClient::getState() {
        return measurer ? measurer->state : 0;
    }

    int sapaClient::getLatencyMs() {
        return measurer ? measurer->latencyMs : 0;
    }

    int sapaClient::getSamplerate() {
        return measurer ? measurer->samplerate : 0;
    }

    int sapaClient::getBuffersize() {
        return measurer ? measurer->buffersize : 0;
    }
    
    int sapaClient::deactivate() {
        jack_deactivate(jackClient);
        return APA_RETURN_SUCCESS;
    }
    
    int sapaClient::transport(TransportType type) {
        return APA_RETURN_SUCCESS;
    }
    
    int sapaClient::sendMidi(char* midi) {
        return APA_RETURN_SUCCESS;
    }

};
