#ifndef SAPACLIENT
#define SAPACLIENT

#include <jack/jack.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#include "IJackClientInterface.h"
#include "APACommon.h"

namespace android {

// Samsung Professional Audio SDK is based on the JACK audio connectivity library (a media server).
// This is our JACK client.
    class sapaClient: public IJackClientInterface {
    public:
        static float volume;

        sapaClient();
        virtual ~sapaClient();

        int setUp(int argc, char *argv[]);
        int tearDown();
        int activate();
        int deactivate();
        int transport(TransportType type);
        int sendMidi(char *midi);

        static void toggleMeasurer();
        static void togglePassThrough();
        static int getState();
        static int getLatencyMs();
        static int getSamplerate();
        static int getBuffersize();

    private:
        jack_port_t *leftOutput, *rightOutput, *leftInput, *rightInput;
        jack_client_t *jackClient;
        short int *stereo;

        static int process(jack_nframes_t frames, void *arg);
    };

};

#endif
