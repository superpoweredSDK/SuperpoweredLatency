#include "latencyMeasurer.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <pthread.h>
#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "superpoweredlatency", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "superpoweredlatency", __VA_ARGS__)

/*
 These functions wrap the measurer, bridge it to Java, and set up audio I/O using OpenSL ES.
 We need two buffer queues, one for input and one for the output.
 Each buffer queue operates with 1 or 2 input buffers (depending on Android version).
*/

static SLObjectItf openSLEngine, outputMix, outputBufferQueue, inputBufferQueue;
static SLAndroidSimpleBufferQueueItf outputBufferQueueInterface, inputBufferQueueInterface;
static pthread_mutex_t mutex;

static short int *inputBuffers[2], *outputBuffers[2];
static int inputBufferWrite, inputBufferRead, inputBuffersAvailable, outputBufferIndex, samplerate, buffersize, numBuffers;

static latencyMeasurer *measurer = NULL;

// Test with Superpowered Media Server
static bool superpoweredIO = false;
static bool superpoweredThreadLaunched = false;
static void *socketThread(void *param) {
    struct sched_param p;
    p.sched_priority = 0;
    int policy = 0;
    pthread_getschedparam(pthread_self(), &policy, &p);
    LOGI("Superpowered client thread policy is %sSCHED_FIFO, priority: %i.", policy == SCHED_FIFO ? "" : "not ", p.sched_priority);

    short int *buf = (short int *)memalign(16, 1040 * 4);

    while (true) {
        int sck = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (sck < 0) {
            LOGE("Superpowered client socket create error. (%i %s)", errno, strerror(errno));
            break;
        } else {
            struct timeval timeout;
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
            setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            const char *name = "SuperpoweredAudio";
            struct sockaddr_un address;
            memset(&address, 0, sizeof(address));
            int namelen = strlen(name);
            address.sun_path[0] = 0;
            memcpy(address.sun_path + 1, name, namelen);
            address.sun_family = AF_LOCAL;
            int alen = namelen + offsetof(struct sockaddr_un, sun_path) + 1;

            if (connect(sck, (struct sockaddr *)&address, alen) < 0) LOGE("Superpowered client socket connect error. (%i %s)", errno, strerror(errno));
            else {
                timeout.tv_sec = 1;
                timeout.tv_usec = 0;
                setsockopt(sck, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
                setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

                int status = -1;
                if (recv(sck, &status, sizeof(status), 0) == sizeof(status)) {
                    if (status != 0) LOGE("Superpowered client socket is busy.");
                    else {
                        superpoweredIO = true;
                        LOGI("Superpowered audio IO!");
                        int *bufint = (int *)buf;
                        while (true) {
                            int bufferSize = recv(sck, buf, 1040 * 4, 0);
                            if (bufferSize > 0) {
                                if (bufint[0] < 1) continue; // Keep alive.
                                // Process audio here.
                                // bufint[0] is numberOfSamples
                                // bufint[1] is samplerate
                                // buf + 8 is the actual audio data (16-bit stereo interleaved)
                                measurer->processInput(buf + 8, bufint[1], bufint[0]);
                                measurer->processOutput(buf + 8);

                                send(sck, buf, bufferSize, 0);
                            } else if (bufferSize < 0) {
                                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) continue; else break;
                            } else break;
                        };
                        superpoweredIO = false;
                    };
                } else LOGE("Superpowered client socket handshake error.");
            };
            close(sck);
            usleep(100000);
        };
    };

    free(buf);
    pthread_detach(pthread_self());
    pthread_exit(NULL);
    return NULL;
}

// Audio input comes here.
static void inputCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext) {
    if (superpoweredIO) {
        (*caller)->Enqueue(caller, inputBuffers[0], buffersize * 4);
        return;
    }

    pthread_mutex_lock(&mutex);
    short int *inputBuffer = inputBuffers[inputBufferWrite];
    if (inputBuffersAvailable == 0) inputBufferRead = inputBufferWrite;
    inputBuffersAvailable++;
    if (inputBufferWrite < numBuffers - 1) inputBufferWrite++; else inputBufferWrite = 0;
    pthread_mutex_unlock(&mutex);

    measurer->processInput(inputBuffer, samplerate, buffersize);
    (*caller)->Enqueue(caller, inputBuffer, buffersize * 4);
}

// Audio output must be provided here.
static void outputCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext) {
    if (superpoweredIO) {
        (*caller)->Enqueue(caller, outputBuffers[0], buffersize * 4);
        return;
    }
    if (!superpoweredThreadLaunched) {
        superpoweredThreadLaunched = true;
        pthread_t thread;
        pthread_create(&thread, NULL, socketThread, NULL);
    };

	short int *outputBuffer = outputBuffers[outputBufferIndex];
    pthread_mutex_lock(&mutex);

    if (inputBuffersAvailable < 1) {
    	pthread_mutex_unlock(&mutex);
  		memset(outputBuffer, 0, buffersize * 4);
   	} else {
    	short int *inputBuffer = inputBuffers[inputBufferRead];
    	if (inputBufferRead < numBuffers - 1) inputBufferRead++; else inputBufferRead = 0;
    	inputBuffersAvailable--;
    	pthread_mutex_unlock(&mutex);

        measurer->processOutput(outputBuffer);
        if (measurer->state == -1) memcpy(outputBuffer, inputBuffer, buffersize * 4);
   	};

   	if (outputBufferIndex < numBuffers - 1) outputBufferIndex++; else outputBufferIndex = 0;
    (*caller)->Enqueue(caller, outputBuffer, buffersize * 4);
}

// Ugly Java-native bridges - JNI, that is.
extern "C" {
    JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_SuperpoweredLatency(JNIEnv *javaEnvironment, jobject self, jlong _samplerate, jlong _buffersize);
    JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_toggleMeasurer(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_togglePassThrough(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getState(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getLatencyMs(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getSamplerate(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getBuffersize(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jboolean Java_com_superpowered_superpoweredlatency_MainActivity_getSuperpowered(JNIEnv *javaEnvironment, jobject self);
}

JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_toggleMeasurer(JNIEnv *javaEnvironment, jobject self) { measurer->toggle(); }
JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_togglePassThrough(JNIEnv *javaEnvironment, jobject self) { measurer->togglePassThrough(); }
JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getState(JNIEnv *javaEnvironment, jobject self) { return measurer->state; }
JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getLatencyMs(JNIEnv *javaEnvironment, jobject self) { return measurer->latencyMs; }
JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getSamplerate(JNIEnv *javaEnvironment, jobject self) { return measurer->samplerate; }
JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getBuffersize(JNIEnv *javaEnvironment, jobject self) { return measurer->buffersize; }
JNIEXPORT jboolean Java_com_superpowered_superpoweredlatency_MainActivity_getSuperpowered(JNIEnv *javaEnvironment, jobject self) { return superpoweredIO; }

// Set up audio and measurer.
JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_SuperpoweredLatency(JNIEnv *javaEnvironment, jobject self, jlong _samplerate, jlong _buffersize) {
    static const SLboolean requireds[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE };

    measurer = new latencyMeasurer();
    pthread_mutex_init(&mutex, NULL);
    samplerate = _samplerate;
    // If buffersize is negative, then we have Android 4.4 or higher.
    if (_buffersize < 0) {
        buffersize = -_buffersize;
        numBuffers = 1;
    } else {
        buffersize = _buffersize;
        numBuffers = 2;
    };

    // Allocating audio buffers for input and output.
    for (int n = 0; n < numBuffers; n++) {
        inputBuffers[n] = (short int *)malloc((buffersize + 16) * 4);
        outputBuffers[n] = (short int *)malloc((buffersize + 16) * 4);
        memset(inputBuffers[n], 0, buffersize * 4);
        memset(outputBuffers[n], 0, buffersize * 4);
    };

    // Create the OpenSL ES engine.
	slCreateEngine(&openSLEngine, 0, NULL, 0, NULL, NULL);
	(*openSLEngine)->Realize(openSLEngine, SL_BOOLEAN_FALSE);
	SLEngineItf openSLEngineInterface = NULL;
	(*openSLEngine)->GetInterface(openSLEngine, SL_IID_ENGINE, &openSLEngineInterface);
	// Create the output mix.
	(*openSLEngineInterface)->CreateOutputMix(openSLEngineInterface, &outputMix, 0, NULL, NULL);
	(*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE);
	SLDataLocator_OutputMix outputMixLocator = { SL_DATALOCATOR_OUTPUTMIX, outputMix };

	// Create the input buffer queue.
	SLDataLocator_IODevice deviceInputLocator = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };
	SLDataSource inputSource = { &deviceInputLocator, NULL };
	SLDataLocator_AndroidSimpleBufferQueue inputLocator = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1 };
	SLDataFormat_PCM inputFormat = { SL_DATAFORMAT_PCM, 2, (SLuint32)samplerate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN };
	SLDataSink inputSink = { &inputLocator, &inputFormat };
	const SLInterfaceID inputInterfaces[2] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
	(*openSLEngineInterface)->CreateAudioRecorder(openSLEngineInterface, &inputBufferQueue, &inputSource, &inputSink, 2, inputInterfaces, requireds);

    // Configure the voice recognition preset which has no signal processing for lower latency.
    SLAndroidConfigurationItf inputConfiguration;
    if ((*inputBufferQueue)->GetInterface(inputBufferQueue, SL_IID_ANDROIDCONFIGURATION, &inputConfiguration) == SL_RESULT_SUCCESS) {
        SLuint32 presetValue = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
        (*inputConfiguration)->SetConfiguration(inputConfiguration, SL_ANDROID_KEY_RECORDING_PRESET, &presetValue, sizeof(SLuint32));
    };
    (*inputBufferQueue)->Realize(inputBufferQueue, SL_BOOLEAN_FALSE);

	// Create the output buffer queue.
	SLDataLocator_AndroidSimpleBufferQueue outputLocator = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1 };
	SLDataFormat_PCM outputFormat = { SL_DATAFORMAT_PCM, 2, (SLuint32)samplerate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN };
	SLDataSource outputSource = { &outputLocator, &outputFormat };
    const SLInterfaceID outputInterfaces[1] = { SL_IID_BUFFERQUEUE };
    SLDataSink outputSink = { &outputMixLocator, NULL };
    (*openSLEngineInterface)->CreateAudioPlayer(openSLEngineInterface, &outputBufferQueue, &outputSource, &outputSink, 1, outputInterfaces, requireds);
    (*outputBufferQueue)->Realize(outputBufferQueue, SL_BOOLEAN_FALSE);

    // Initialize and start the input buffer queue.
    (*inputBufferQueue)->GetInterface(inputBufferQueue, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &inputBufferQueueInterface);
    (*inputBufferQueueInterface)->RegisterCallback(inputBufferQueueInterface, inputCallback, NULL);
    SLRecordItf recordInterface;
    (*inputBufferQueue)->GetInterface(inputBufferQueue, SL_IID_RECORD, &recordInterface);
    (*inputBufferQueueInterface)->Enqueue(inputBufferQueueInterface, inputBuffers[0], buffersize * 4);
    (*recordInterface)->SetRecordState(recordInterface, SL_RECORDSTATE_RECORDING);

    // Initialize and start the output buffer queue.
    (*outputBufferQueue)->GetInterface(outputBufferQueue, SL_IID_BUFFERQUEUE, &outputBufferQueueInterface);
    (*outputBufferQueueInterface)->RegisterCallback(outputBufferQueueInterface, outputCallback, NULL);
    (*outputBufferQueueInterface)->Enqueue(outputBufferQueueInterface, outputBuffers[0], buffersize * 4);
    SLPlayItf outputPlayInterface;
    (*outputBufferQueue)->GetInterface(outputBufferQueue, SL_IID_PLAY, &outputPlayInterface);
    (*outputPlayInterface)->SetPlayState(outputPlayInterface, SL_PLAYSTATE_PLAYING);
}
