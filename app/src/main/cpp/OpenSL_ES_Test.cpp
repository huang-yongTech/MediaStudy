//
// Created by huangyong on 2019/8/23.
//
#include <cassert>
#include <malloc.h>
#include "OpenSL_ES_Test.h"

OpenSLTest::OpenSLTest() {
    createEngine();
}

OpenSLTest::~OpenSLTest() {
    releaseResampleBuf();
    destroy();
}

void OpenSLTest::createEngine() {
    SLresult result;

    //创建engine
    result = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //realise engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //创建混音器效果
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //realise混音器
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;


}

void OpenSLTest::createBufferQueueAudioPlayer(jint sampleRate, jint bufSize) {
    SLresult result;

    if (sampleRate >= 0 && bufSize >= 0) {
        bqPlayerSampleRate = sampleRate * 1000;
        /*
         * device native buffer size is another factor to minimize audio latency, not used in this
         * sample: we only play one giant buffer here
         */
        bqPlayerBufSize = bufSize;
    }

    //配置输入源
    SLDataLocator_AndroidSimpleBufferQueue loc_bufQueue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                           2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_8,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
    /**
     * Enable Fast Audio when possible:  once we set the same rate to be the native, fast audio path
     * will be triggered
     */
    if (bqPlayerSampleRate) {
        format_pcm.samplesPerSec = bqPlayerSampleRate;       //sample rate in mili second
    }
    SLDataSource audioSrc = {&loc_bufQueue, &format_pcm};

    //配置输出源
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, nullptr};

    /**
     * 创建AudioPlayer
     * fast audio does not support when SL_IID_EFFECTSEND is required, skip it
     * for fast audio case
     */
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
                                                bqPlayerSampleRate ? 2 : 3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the player
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the play interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the buffer queue interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // register callback on the buffer queue
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the effect send interface
    bqPlayerEffectSend = nullptr;
    if (0 == bqPlayerSampleRate) {
        result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
                                                 &bqPlayerEffectSend);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }

    // get the volume interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // set the player's state to playing
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

//this callback handler is called every time a buffer finishes playing
void OpenSLTest::bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == bqPlayerBufferQueue);
    assert(nullptr == context);
    //for streaming playback, replace this test by logic to find and fill the next buffer
    if (--nextCount > 0 && nullptr != nextBuffer && 0 != nextSize) {
        SLresult result;
        //enqueue another buffer
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);
        //the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        //which for this code example would indicate a programming error
        if (SL_RESULT_SUCCESS != result) {
            pthread_mutex_unlock(&audioEngineLock);
        }
        (void) result;
    } else {
        releaseResampleBuf();
        pthread_mutex_unlock(&audioEngineLock);
    }
}

void OpenSLTest::playClip(jint which, int count) {
    if (pthread_mutex_trylock(&audioEngineLock)) {
        // If we could not acquire audio engine lock, reject this request and client should re-try
        return;
    }

    switch (which) {
        case 0:// CLIP_NONE
            nextBuffer = nullptr;
            nextSize = 0;
            break;
        case 1:// CLIP_HELLO

            break;

        case 2:// CLIP_ANDROID

            break;
        default:

            break;
    }
}

void OpenSLTest::createAssetsAudioPlayer(jobject assetManager, jstring fileName) {

}

void OpenSLTest::playAssets() {

}

void OpenSLTest::playPCM() {

}

void OpenSLTest::createAudioRecorder() {

}

void OpenSLTest::startRecord() {

}

void OpenSLTest::stopRecord() {

}

void OpenSLTest::playRecord() {

}

/*
 * Only support up-sampling
 */
short *OpenSLTest::createResampledBuf(uint32_t idx, uint32_t srcRate, unsigned *size) {
    short *src = nullptr;
    short *workBuf;
    int upSampleRate;
    int32_t srcSampleCount = 0;

    if (0 == bqPlayerSampleRate) {
        return nullptr;
    }
    if (bqPlayerSampleRate % srcRate) {
        /*
         * simple up-sampling, must be divisible
         */
        return nullptr;
    }
    upSampleRate = bqPlayerSampleRate / srcRate;

    switch (idx) {
        case 0:
            return nullptr;
        case 1: // HELLO_CLIP
            srcSampleCount = sizeof(hello) >> 1;
            src = (short *) hello;
            break;
        case 2: // ANDROID_CLIP
            srcSampleCount = sizeof(android) >> 1;
            src = (short *) android;
            break;
        case 3: // SAWTOOTH_CLIP
            srcSampleCount = SAWTOOTH_FRAMES;
            src = sawtoothBuffer;
            break;
        case 4: // captured frames
            srcSampleCount = recorderSize / sizeof(short);
            src = recorderBuffer;
            break;
        default:
            assert(0);
            return nullptr;
    }

    resampleBuf = (short *) malloc((srcSampleCount * upSampleRate) << 1);
    if (resampleBuf == nullptr) {
        return resampleBuf;
    }
    workBuf = resampleBuf;
    for (int sample = 0; sample < srcSampleCount; sample++) {
        for (int dup = 0; dup < upSampleRate; dup++) {
            *workBuf++ = src[sample];
        }
    }

    *size = (srcSampleCount * upSampleRate) << 1;     // sample format is 16 bit
    return resampleBuf;
}

void OpenSLTest::releaseResampleBuf() {
    if (0 == bqPlayerSampleRate) {
        /*
         * we are not using fast path, so we were not creating buffers, nothing to do
         */
        return;
    }

    free(resampleBuf);
    resampleBuf = nullptr;
}

void OpenSLTest::destroy() {
    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (bqPlayerObject != nullptr) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = nullptr;
        bqPlayerPlay = nullptr;
        bqPlayerBufferQueue = nullptr;
        bqPlayerEffectSend = nullptr;
        bqPlayerMuteSolo = nullptr;
        bqPlayerVolume = nullptr;
    }

    // destroy file descriptor audio player object, and invalidate all associated interfaces
    if (fdPlayerObject != nullptr) {
        (*fdPlayerObject)->Destroy(fdPlayerObject);
        fdPlayerObject = nullptr;
        fdPlayerPlay = nullptr;
        fdPlayerSeek = nullptr;
        fdPlayerMuteSolo = nullptr;
        fdPlayerVolume = nullptr;
    }

    // destroy audio recorder object, and invalidate all associated interfaces
    if (recorderObject != nullptr) {
        (*recorderObject)->Destroy(recorderObject);
        recorderObject = nullptr;
        recorderRecord = nullptr;
        recorderBufferQueue = nullptr;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (outputMixObject != nullptr) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = nullptr;
        outputMixEnvironmentalReverb = nullptr;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != nullptr) {
        (*engineObject)->Destroy(engineObject);
        engineObject = nullptr;
        engineEngine = nullptr;
    }

    pthread_mutex_destroy(&audioEngineLock);
}
