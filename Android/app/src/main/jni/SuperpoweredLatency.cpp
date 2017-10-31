#include "latencyMeasurer.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#if HAS_AAUDIO == 1
#include <aaudio/AAudio.h>
#endif

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "superpoweredlatency", __VA_ARGS__)

static int samplerate;
static int buffersize;
static latencyMeasurer *measurer;
static bool hasAAudio = false;
static bool isRunning = false;

constexpr int kChannelCountMono = 1;
constexpr int kChannelCountStereo = 2;
constexpr int kBytesIn16BitSample = 2;


// ---------
// OPENSL ES
// ---------

#define NUMOPENSLESBUFFERS 128
static struct {
    SLObjectItf engine, outputMix, outputBufferQueue, inputBufferQueue;
    SLAndroidSimpleBufferQueueItf outputBufferQueueInterface, inputBufferQueueInterface;
    short int *inputBuffers[NUMOPENSLESBUFFERS], *outputBuffers[NUMOPENSLESBUFFERS];
    int samplerateFromJava, buffersizeFromJava, inputBufferWriteIndex, inputBufferReadIndex, inputBuffersAvailable, outputBufferWriteIndex;
} openSLES;

// Note: input array must be at least 2 * numFrames in size
static void convertBufferToStereo(short int *buffer, const int numFrames){
    for (int i = numFrames - 1; i >=0; --i) {
        buffer[i*2] = buffer[i];
        buffer[(i*2)+1] = buffer[i];
    }
}

// Audio input comes here.
static void openSLESInputCallback(SLAndroidSimpleBufferQueueItf caller, __unused void *pContext) {
    __sync_fetch_and_add(&openSLES.inputBuffersAvailable, 1);
    short int *inputBuffer = openSLES.inputBuffers[openSLES.inputBufferWriteIndex];
    if (openSLES.inputBufferWriteIndex < NUMOPENSLESBUFFERS - 1) openSLES.inputBufferWriteIndex++; else openSLES.inputBufferWriteIndex = 0;
    (*caller)->Enqueue(caller, inputBuffer, (SLuint32)buffersize * kChannelCountMono * kBytesIn16BitSample);
    convertBufferToStereo(inputBuffer, (SLuint32)buffersize);
}

// Audio output must be provided here.
static void openSLESOutputCallback(SLAndroidSimpleBufferQueueItf caller, __unused void *pContext) {
    short int *outputBuffer = openSLES.outputBuffers[openSLES.outputBufferWriteIndex];
    if (openSLES.outputBufferWriteIndex < NUMOPENSLESBUFFERS - 1) openSLES.outputBufferWriteIndex++; else openSLES.outputBufferWriteIndex = 0;

    if (__sync_fetch_and_add(&openSLES.inputBuffersAvailable, 0) > 0) {
        __sync_fetch_and_add(&openSLES.inputBuffersAvailable, -1);
        short int *inputBuffer = openSLES.inputBuffers[openSLES.inputBufferReadIndex];
        if (openSLES.inputBufferReadIndex < NUMOPENSLESBUFFERS - 1) openSLES.inputBufferReadIndex++; else openSLES.inputBufferReadIndex = 0;

        measurer->processInput(inputBuffer, samplerate, buffersize);
        measurer->processOutput(outputBuffer);
        if (measurer->state == -1) memcpy(outputBuffer, inputBuffer, (size_t)buffersize * kChannelCountStereo * kBytesIn16BitSample);
    } else memset(outputBuffer, 0, (size_t)buffersize * kChannelCountStereo * kBytesIn16BitSample);

    (*caller)->Enqueue(caller, outputBuffer, (SLuint32)buffersize * kChannelCountStereo * kBytesIn16BitSample);
}

static void startOpenSLES() {
    hasAAudio = false;
    samplerate = openSLES.samplerateFromJava;
    buffersize = openSLES.buffersizeFromJava;

    openSLES.inputBufferWriteIndex = 1; // Start from 1, because we enqueue one buffer when we start the input buffer queue.
    openSLES.inputBufferReadIndex = 0;
    openSLES.inputBuffersAvailable = 0;
    openSLES.outputBufferWriteIndex = 1; // Start from 1, because we enqueue one buffer when we start the output buffer queue.

    // Allocating audio buffers for input and output.
    for (int n = 0; n < NUMOPENSLESBUFFERS; n++) {
        openSLES.inputBuffers[n] = (short int *)malloc(((size_t)buffersize + 16) * kChannelCountStereo * kBytesIn16BitSample);
        openSLES.outputBuffers[n] = (short int *)malloc(((size_t)buffersize + 16) * kChannelCountStereo * kBytesIn16BitSample);
        memset(openSLES.inputBuffers[n], 0, (size_t)buffersize * kChannelCountStereo * kBytesIn16BitSample);
        memset(openSLES.outputBuffers[n], 0, (size_t)buffersize * kChannelCountStereo * kBytesIn16BitSample);
    };

    const SLboolean requireds[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE };

    // Create the OpenSL ES engine.
    slCreateEngine(&openSLES.engine, 0, NULL, 0, NULL, NULL);
    (*openSLES.engine)->Realize(openSLES.engine, SL_BOOLEAN_FALSE);
    SLEngineItf openSLEngineInterface = NULL;
    (*openSLES.engine)->GetInterface(openSLES.engine, SL_IID_ENGINE, &openSLEngineInterface);
    // Create the output mix.
    (*openSLEngineInterface)->CreateOutputMix(openSLEngineInterface, &openSLES.outputMix, 0, NULL, NULL);
    (*openSLES.outputMix)->Realize(openSLES.outputMix, SL_BOOLEAN_FALSE);
    SLDataLocator_OutputMix outputMixLocator = { SL_DATALOCATOR_OUTPUTMIX, openSLES.outputMix };

    // Create the output buffer queue.
    SLDataLocator_AndroidSimpleBufferQueue outputLocator = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1 };
    SLDataFormat_PCM outputFormat = { SL_DATAFORMAT_PCM, kChannelCountStereo, (SLuint32)samplerate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN };
    SLDataSource outputSource = { &outputLocator, &outputFormat };
    const SLInterfaceID outputInterfaces[1] = { SL_IID_BUFFERQUEUE };
    SLDataSink outputSink = { &outputMixLocator, NULL };
    (*openSLEngineInterface)->CreateAudioPlayer(openSLEngineInterface, &openSLES.outputBufferQueue, &outputSource, &outputSink, 1, outputInterfaces, requireds);
    (*openSLES.outputBufferQueue)->Realize(openSLES.outputBufferQueue, SL_BOOLEAN_FALSE);

    // Create the input buffer queue.
    SLDataLocator_IODevice deviceInputLocator = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };
    SLDataSource inputSource = { &deviceInputLocator, NULL };
    SLDataLocator_AndroidSimpleBufferQueue inputLocator = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1 };
    SLDataFormat_PCM inputFormat = { SL_DATAFORMAT_PCM, kChannelCountMono, (SLuint32)samplerate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN };
    SLDataSink inputSink = { &inputLocator, &inputFormat };
    const SLInterfaceID inputInterfaces[2] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
    (*openSLEngineInterface)->CreateAudioRecorder(openSLEngineInterface, &openSLES.inputBufferQueue, &inputSource, &inputSink, 2, inputInterfaces, requireds);

    // Configure the voice recognition preset which has no signal processing for lower latency.
    SLAndroidConfigurationItf inputConfiguration;
    if ((*openSLES.inputBufferQueue)->GetInterface(openSLES.inputBufferQueue, SL_IID_ANDROIDCONFIGURATION, &inputConfiguration) == SL_RESULT_SUCCESS) {
        SLuint32 presetValue = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
        (*inputConfiguration)->SetConfiguration(inputConfiguration, SL_ANDROID_KEY_RECORDING_PRESET, &presetValue, sizeof(SLuint32));
    };
    (*openSLES.inputBufferQueue)->Realize(openSLES.inputBufferQueue, SL_BOOLEAN_FALSE);

    // Initialize and start the output buffer queue.
    (*openSLES.outputBufferQueue)->GetInterface(openSLES.outputBufferQueue, SL_IID_BUFFERQUEUE, &openSLES.outputBufferQueueInterface);
    (*openSLES.outputBufferQueueInterface)->RegisterCallback(openSLES.outputBufferQueueInterface, openSLESOutputCallback, NULL);
    (*openSLES.outputBufferQueueInterface)->Enqueue(openSLES.outputBufferQueueInterface, openSLES.outputBuffers[0], (SLuint32)buffersize * 4);
    SLPlayItf outputPlayInterface;
    (*openSLES.outputBufferQueue)->GetInterface(openSLES.outputBufferQueue, SL_IID_PLAY, &outputPlayInterface);
    (*outputPlayInterface)->SetPlayState(outputPlayInterface, SL_PLAYSTATE_PLAYING);

    // Initialize and start the input buffer queue.
    (*openSLES.inputBufferQueue)->GetInterface(openSLES.inputBufferQueue, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &openSLES.inputBufferQueueInterface);
    (*openSLES.inputBufferQueueInterface)->RegisterCallback(openSLES.inputBufferQueueInterface, openSLESInputCallback, NULL);
    SLRecordItf recordInterface;
    (*openSLES.inputBufferQueue)->GetInterface(openSLES.inputBufferQueue, SL_IID_RECORD, &recordInterface);
    (*openSLES.inputBufferQueueInterface)->Enqueue(openSLES.inputBufferQueueInterface, openSLES.inputBuffers[0], (SLuint32)buffersize * 4);
    (*recordInterface)->SetRecordState(recordInterface, SL_RECORDSTATE_RECORDING);
}

static void stopOpenSLES() {
    SLRecordItf recordInterface;
    (*openSLES.inputBufferQueue)->GetInterface(openSLES.inputBufferQueue, SL_IID_RECORD, &recordInterface);
    (*recordInterface)->SetRecordState(recordInterface, SL_RECORDSTATE_STOPPED);

    SLPlayItf outputPlayInterface;
    (*openSLES.outputBufferQueue)->GetInterface(openSLES.outputBufferQueue, SL_IID_PLAY, &outputPlayInterface);
    (*outputPlayInterface)->SetPlayState(outputPlayInterface, SL_PLAYSTATE_STOPPED);

    usleep(200000);

    (*openSLES.outputBufferQueue)->Destroy(openSLES.outputBufferQueue);
    (*openSLES.inputBufferQueue)->Destroy(openSLES.inputBufferQueue);
    (*openSLES.outputMix)->Destroy(openSLES.outputMix);
    (*openSLES.engine)->Destroy(openSLES.engine);

    for (int n = 0; n < NUMOPENSLESBUFFERS; n++) {
        free(openSLES.inputBuffers[n]);
        free(openSLES.outputBuffers[n]);
    }
}

#if HAS_AAUDIO == 1
// ------
// AAudio
// ------

static struct {
    AAudioStream *outputStream;
    AAudioStream *inputStream;
    bool firstAAudioInput, restarting;
} aaudio;

aaudio_data_callback_result_t aaudioProcessingCallback(__unused AAudioStream *stream, __unused void *userData, void *audioData, int32_t numFrames) {
    // Drain excess samples in input the first time.
    if (aaudio.firstAAudioInput) {
        aaudio.firstAAudioInput = false;
        aaudio_result_t drainedFrames = 0;
        do {
            drainedFrames = AAudioStream_read(aaudio.inputStream, audioData, numFrames, 0);
        } while (drainedFrames > 0);
    }

    if (numFrames > buffersize) buffersize = numFrames;

    aaudio_result_t frameCount = AAudioStream_read(aaudio.inputStream, audioData, numFrames, 0); // Read input.

    if (frameCount > 0) { // Has input.
        short int *audio = (short int *)audioData;
        measurer->processInput(audio, samplerate, frameCount);
        measurer->processOutput(audio);

        // If the input frames are less than the output frames. Should not happen.
        if (frameCount < numFrames) memset(audio + frameCount * 2, 0, size_t(numFrames - frameCount) * 4);
    } else memset(audioData, 0, (size_t)numFrames * 4); // No input, return with silence.

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void *restartAAudioThread(__unused void *param);

static void AAudioErrorCallback(AAudioStream *stream, __unused void *userData, aaudio_result_t error) {
    LOGI("%s aaudio stream error: %s", (AAudioStream_getDirection(stream) == AAUDIO_DIRECTION_OUTPUT) ? "output" : "input", AAudio_convertResultToText(error));

    if (AAudioStream_getState(stream) == AAUDIO_STREAM_STATE_DISCONNECTED) { // If the audio routing has been changed, restart audio I/O.
        if (aaudio.restarting) LOGI("Restarting already.");
        else {
            aaudio.restarting = true;
            pthread_t thread;
            pthread_create(&thread, NULL, restartAAudioThread, NULL);
        }
    }
}

static bool startAAudio() {
    hasAAudio = false;
    aaudio.firstAAudioInput = true;
    aaudio.inputStream = NULL;
    aaudio.outputStream = NULL;

    // Setup output.
    AAudioStreamBuilder *outputStreamBuilder;
    if (AAudio_createStreamBuilder(&outputStreamBuilder) != AAUDIO_OK) return false;

    AAudioStreamBuilder_setDirection(outputStreamBuilder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(outputStreamBuilder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(outputStreamBuilder, 2);
    AAudioStreamBuilder_setSharingMode(outputStreamBuilder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setPerformanceMode(outputStreamBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setErrorCallback(outputStreamBuilder, AAudioErrorCallback, NULL);
    AAudioStreamBuilder_setDataCallback(outputStreamBuilder, aaudioProcessingCallback, NULL);

    if ((AAudioStreamBuilder_openStream(outputStreamBuilder, &aaudio.outputStream) == AAUDIO_OK) && (aaudio.outputStream != NULL)) {
        samplerate = AAudioStream_getSampleRate(aaudio.outputStream);
        AAudioStream_setBufferSizeInFrames(aaudio.outputStream, AAudioStream_getFramesPerBurst(aaudio.outputStream));
        buffersize = AAudioStream_getBufferSizeInFrames(aaudio.outputStream);
    } else {
        AAudioStreamBuilder_delete(outputStreamBuilder);
        return false;
    }

    // Setup input.
    AAudioStreamBuilder *inputStreamBuilder;
    if (AAudio_createStreamBuilder(&inputStreamBuilder) != AAUDIO_OK) {
        AAudioStreamBuilder_delete(outputStreamBuilder);
        AAudioStream_close(aaudio.outputStream);
        return false;
    }

    AAudioStreamBuilder_setDirection(inputStreamBuilder, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setFormat(inputStreamBuilder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(inputStreamBuilder, 2);
    AAudioStreamBuilder_setSharingMode(inputStreamBuilder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setPerformanceMode(inputStreamBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setErrorCallback(inputStreamBuilder, AAudioErrorCallback, NULL);
    AAudioStreamBuilder_setSampleRate(inputStreamBuilder, samplerate);

    if (!((AAudioStreamBuilder_openStream(inputStreamBuilder, &aaudio.inputStream) == AAUDIO_OK) && (aaudio.inputStream != NULL))) {
        AAudioStreamBuilder_delete(inputStreamBuilder);
        AAudioStreamBuilder_delete(outputStreamBuilder);
        AAudioStream_close(aaudio.outputStream);
        return false;
    }

    AAudioStreamBuilder_delete(inputStreamBuilder);
    AAudioStreamBuilder_delete(outputStreamBuilder);

    // Start audio I/O.
    bool success = (AAudioStream_requestStart(aaudio.outputStream) == AAUDIO_OK) && (AAudioStream_requestStart(aaudio.inputStream) == AAUDIO_OK);
    if (!success) {
        AAudioStream_close(aaudio.outputStream);
        AAudioStream_close(aaudio.inputStream);
    } else hasAAudio = true;

    aaudio.restarting = false;
    return success;
}

static void stopAAudio() {
    AAudioStream_requestStop(aaudio.outputStream);
    AAudioStream_requestStop(aaudio.inputStream);
    AAudioStream_close(aaudio.outputStream);
    AAudioStream_close(aaudio.inputStream);
}

// This is started by the error callback.
static void *restartAAudioThread(__unused void *param) {
    LOGI("Restarting AAudio.");
    stopAAudio();
    usleep(200000);
    startAAudio();
    pthread_detach(pthread_self());
    pthread_exit(NULL);
    return NULL;
}

#endif

// Ugly Java-native bridges - JNI, that is.
extern "C" {
    JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_SuperpoweredLatency(JNIEnv *javaEnvironment, jobject self, jlong samplerateFromJava, jlong buffersizeFromJava);
    JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_toggleMeasurer(JNIEnv *javaEnvironment, jobject self, jboolean useAAudioIfAvailable);
    JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_togglePassThrough(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getState(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getLatencyMs(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getSamplerate(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getBuffersize(JNIEnv *javaEnvironment, jobject self);
    JNIEXPORT jboolean Java_com_superpowered_superpoweredlatency_MainActivity_getAAudio(JNIEnv *javaEnvironment, jobject self);
}

JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_toggleMeasurer(__unused JNIEnv *javaEnvironment, __unused jobject self, jboolean useAAudioIfAvailable) {
    measurer->toggle();
    isRunning = !isRunning;
#if HAS_AAUDIO == 1
    if (isRunning) {
        if (useAAudioIfAvailable) {
            if (!startAAudio()) startOpenSLES(); // Use OpenSL ES if AAudio is not available.
        } else startOpenSLES();
        LOGI(hasAAudio ? "Using AAudio." : "Using OpenSL ES.");
    } else {
        if (hasAAudio) stopAAudio(); else stopOpenSLES();
    }
#else
    if (isRunning) startOpenSLES(); else stopOpenSLES();
#endif
}
JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_togglePassThrough(__unused JNIEnv *javaEnvironment, __unused jobject self) { measurer->togglePassThrough(); }
JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getState(__unused JNIEnv *javaEnvironment, __unused jobject self) { return measurer->state; }
JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getLatencyMs(__unused JNIEnv *javaEnvironment, __unused jobject self) { return measurer->latencyMs; }
JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getSamplerate(__unused JNIEnv *javaEnvironment, __unused jobject self) { return measurer->samplerate; }
JNIEXPORT jint Java_com_superpowered_superpoweredlatency_MainActivity_getBuffersize(__unused JNIEnv *javaEnvironment, __unused jobject self) { return measurer->buffersize; }
JNIEXPORT jboolean Java_com_superpowered_superpoweredlatency_MainActivity_getAAudio(__unused JNIEnv *javaEnvironment, __unused jobject self) { return (jboolean)hasAAudio; }

// Set up audio and measurer.
JNIEXPORT void Java_com_superpowered_superpoweredlatency_MainActivity_SuperpoweredLatency(__unused JNIEnv *javaEnvironment, __unused jobject self, jlong _samplerateFromJava, jlong _buffersizeFromJava) {
    openSLES.samplerateFromJava = (int)_samplerateFromJava;
    openSLES.buffersizeFromJava = (int)_buffersizeFromJava;
    measurer = new latencyMeasurer();
}
