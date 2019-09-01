//
// Created by huangyong on 2019/8/23.
//
#ifndef MEDIASTUDY_OPENSL_ES_TEST_C
#define MEDIASTUDY_OPENSL_ES_TEST_C

//本地音频
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <pthread.h>
#include <jni.h>
#include <cassert>
#include <malloc.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "AudioContext.h"

#define RECORDER_FRAMES (16000 * 5)

// pre-recorded sound clips, both are 8 kHz mono 16-bit signed little endian
static const char hello[] =

#include "hello_clip.h";

static const char android[] =

#include "android_clip.h";

//engine interfaces
static SLObjectItf engineObject = nullptr;
//创建其他opensl-es对象类型的接口对象，该对象紧随SLObjectItf创建之后创建
SLEngineItf engineEngine = nullptr;

//混音器
SLObjectItf outputMixObject = nullptr;
//一些环境混响属性的设置
SLEnvironmentalReverbItf outputMixEnvironmentalReverb = nullptr;
SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

//文件接口功能
//其他对象创建需要依赖的基础对象，该对象必须最先创建
SLObjectItf fdPlayerObject = nullptr;
//播放器，播放状态控制对象
SLPlayItf fdPlayerPlay;
//音频回放接口，通过该接口可实现音频循环播放
SLSeekItf fdPlayerSeek;
//音频播放通道选择
SLMuteSoloItf fdPlayerMuteSolo;
//音频声音属性的控制
SLVolumeItf fdPlayerVolume;

//pcm文件播放器
SLObjectItf pcmPlayerObject = nullptr;
SLPlayItf pcmPlayerPlay = nullptr;
SLSeekItf pcmPlayerSeek;
SLVolumeItf pcmPlayerVolume = nullptr;
//缓冲区队列
SLAndroidSimpleBufferQueueItf pcmBufferQueue;
//pcm缓冲相关
FILE *pcmFile;
void *buffer;
uint8_t *out_buffer;
//pcm缓冲器队列接口

//录音接口功能
SLObjectItf recorderObject = nullptr;
SLPlayItf recorderPlay;
//音频录制状态接口
SLRecordItf recorderRecord = nullptr;
SLAndroidSimpleBufferQueueItf recorderBufferQueue = nullptr;

//缓冲队列
SLObjectItf bqPlayerObject = nullptr;
SLPlayItf bqPlayerPlay;
SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
SLEffectSendItf bqPlayerEffectSend;
SLMuteSoloItf bqPlayerMuteSolo;
SLVolumeItf bqPlayerVolume;
SLmilliHertz bqPlayerSampleRate = 0;
jint bqPlayerBufSize = 0;
short *resampleBuf = nullptr;
// a mutext to guard against re-entrance to record & playback
// as well as make recording and playing back to be mutually exclusive
// this is to avoid crash at situations like:
// recording is in session [not finished]
// user presses record button and another recording coming in
// The action: when recording/playing back is not finished, ignore the new request
pthread_mutex_t audioEngineLock = PTHREAD_MUTEX_INITIALIZER;

// pointer and size of the next player buffer to enqueue, and number of remaining buffers
short *nextBuffer;
unsigned nextSize;
//重复次数
int nextCount;

// 5 seconds of recorded audio at 16 kHz mono, 16-bit signed little endian
//录制5秒音频
short recorderBuffer[RECORDER_FRAMES];
unsigned recorderSize = 0;

//释放缓冲池
void releaseResampleBuf_() {
    if (0 == bqPlayerSampleRate) {
        /*
         * we are not using fast path, so we were not creating buffers, nothing to do
         */
        return;
    }

    free(resampleBuf);
    resampleBuf = nullptr;
}

void bqPlayerCallback_(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == bqPlayerBufferQueue);
    assert(nullptr == context);
    //for streaming playback, replace this test by logic to find and fill the next buffer
    if (--nextCount > 0 && nullptr != nextBuffer &&
        0 != nextSize) {
        SLresult result;
        //enqueue another buffer
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue,
                                                 nextBuffer,
                                                 nextSize);
        //the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        //which for this code example would indicate a programming error
        if (SL_RESULT_SUCCESS != result) {
            pthread_mutex_unlock(&audioEngineLock);
        }
        (void) result;
    } else {
        releaseResampleBuf_();
        pthread_mutex_unlock(&audioEngineLock);
    }
}

void createEngine_() {
    SLresult result;

    //创建SLObjectItf
    result = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //realise engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //创建SLEngineItf接口，该接口主要用于创建其它对象
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
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //创建SLEnvironmentalReverbItf接口实现对象
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }
}

//创建采样缓冲池
short *createResampledBuf_(uint32_t idx, uint32_t srcRate, unsigned *size) {
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

void destroy_() {
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

    if (pcmPlayerObject != nullptr) {
        (*pcmPlayerObject)->Destroy(pcmPlayerObject);
        pcmPlayerObject = nullptr;
        pcmPlayerPlay = nullptr;
        pcmPlayerSeek = nullptr;
        pcmPlayerVolume = nullptr;
        pcmBufferQueue = nullptr;
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

void createBufferQueueAudioPlayer_(jint sampleRate, jint bufSize) {
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
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                   1,
                                   SL_SAMPLINGRATE_8,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER,
                                   SL_BYTEORDER_LITTLEENDIAN};
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
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback_,
                                                      nullptr);
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

/**
 * 事先录制的音频播放
 * @param which
 * @param count
 */
void playClip_(jint which, int count) {
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
            nextBuffer = createResampledBuf_(1, SL_SAMPLINGRATE_8, &nextSize);
            if (nextBuffer != nullptr) {
                nextBuffer = (short *) hello;
                nextSize = sizeof(hello);
            }
            break;
        case 2:// CLIP_ANDROID
            nextBuffer = createResampledBuf_(2, SL_SAMPLINGRATE_8, &nextSize);
            if (nextBuffer != nullptr) {
                nextBuffer = (short *) android;
                nextSize = sizeof(android);
            }
            break;
        default:
            nextBuffer = nullptr;
            nextSize = 0;
            break;
    }

    nextCount = count;
    if (nextSize > 0) {
        // here we only enqueue one buffer because it is a long clip,
        // but for streaming playback we would typically enqueue at least 2 buffers to start
        SLresult result;
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);
        if (SL_RESULT_SUCCESS != result) {
            pthread_mutex_unlock(&audioEngineLock);
        }
    } else {
        pthread_mutex_unlock(&audioEngineLock);
    }
}

/**
 * assests音频播放
 * @param env
 * @param instance
 * @param assetManager
 * @param fileName
 * @return
 */
jboolean createAssetsAudioPlayer_(JNIEnv *env, jobject instance,
                                  jobject assetManager,
                                  jstring fileName) {
    SLresult result;

    // convert Java string to UTF-8
    const char *utf8 = env->GetStringUTFChars(fileName, nullptr);
    assert(nullptr != utf8);

    // use asset manager to open asset by filename
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    assert(nullptr != mgr);
    AAsset *asset = AAssetManager_open(mgr, utf8, AASSET_MODE_UNKNOWN);

    // release the Java string and UTF-8
    env->ReleaseStringUTFChars(fileName, utf8);

    // the asset might not be found
    if (nullptr == asset) {
        return JNI_FALSE;
    }

    // open asset as file descriptor
    off_t start;
    off_t length;
    int fd = AAsset_openFileDescriptor(asset, &start, &length);
    assert(0 <= fd);
    AAsset_close(asset);

    //输入源
    SLDataLocator_AndroidFD loc_fd = {SL_DATALOCATOR_ANDROIDFD, fd, start, length};
    SLDataFormat_MIME format_mime = {SL_DATAFORMAT_MIME, nullptr, SL_CONTAINERTYPE_UNSPECIFIED};
    SLDataSource audioSrc = {&loc_fd, &format_mime};

    //输出源
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, nullptr};

    //创建player
    const SLInterfaceID ids[3] = {SL_IID_SEEK, SL_IID_MUTESOLO, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &fdPlayerObject, &audioSrc, &audioSnk,
                                                3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the player
    result = (*fdPlayerObject)->Realize(fdPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the play interface
    result = (*fdPlayerObject)->GetInterface(fdPlayerObject, SL_IID_PLAY, &fdPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the seek interface
    result = (*fdPlayerObject)->GetInterface(fdPlayerObject, SL_IID_SEEK, &fdPlayerSeek);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the mute/solo interface
    result = (*fdPlayerObject)->GetInterface(fdPlayerObject, SL_IID_MUTESOLO, &fdPlayerMuteSolo);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the volume interface
    result = (*fdPlayerObject)->GetInterface(fdPlayerObject, SL_IID_VOLUME, &fdPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // enable whole file looping
    result = (*fdPlayerSeek)->SetLoop(fdPlayerSeek, SL_BOOLEAN_TRUE, 0, SL_TIME_UNKNOWN);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    return JNI_TRUE;
}

void playAssets_(jboolean isPlaying) {
    SLresult result;

    // make sure the asset audio player was created
    if (nullptr != fdPlayerPlay) {
        // set the player's state
        result = (*fdPlayerPlay)->SetPlayState(fdPlayerPlay,
                                               isPlaying ? SL_PLAYSTATE_PLAYING
                                                         : SL_PLAYSTATE_PAUSED);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }
}

void getPcmData(void **pcm) {
    while (!feof(pcmFile)) {
        fread(out_buffer, 44100 * 2 * 2, 1, pcmFile);
        if (out_buffer == nullptr) {
            LOGI("%s", "read end");
            break;
        } else {
            LOGI("%s", "reading");
        }
        *pcm = out_buffer;
        break;
    }
}

void pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bq, void *context) {
    //assert(NULL == context);
    assert(bq == pcmBufferQueue);
    getPcmData(&buffer);
    // for streaming playback, replace this test by logic to find and fill the next buffer
    if (nullptr != buffer) {
        SLresult result;
        // enqueue another buffer
        result = (*pcmBufferQueue)->Enqueue(pcmBufferQueue, buffer, 44100 * 2 * 2);
        // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        // which for this code example would indicate a programming error
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }
}

/**
 * pcm文件音频播放
 * @param env
 * @param instance
 * @param fileName
 * @return
 */
jboolean createPcmAudioPlayer_(JNIEnv *env, jobject instance, jstring fileName) {
    const char *pcmPath = env->GetStringUTFChars(fileName, nullptr);
    pcmFile = fopen(pcmPath, "re");
    if (pcmFile == nullptr) {
        LOGE("%s", "fopen file error");
        return JNI_FALSE;
    }

    out_buffer = (uint8_t *) malloc(44100 * 2 * 2);
    env->ReleaseStringUTFChars(fileName, pcmPath);

    SLresult result;

    //输入源
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                            2};
    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,//播放pcm格式的数据
            2,//2个声道（立体声）
            SL_SAMPLINGRATE_44_1,//44100hz的频率
            SL_PCMSAMPLEFORMAT_FIXED_16,//位数 16位
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致就行
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,//立体声（前左前右）
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };
    SLDataSource audioSrc = {&android_queue, &format_pcm};

    //输出源
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, nullptr};

    // create audio player
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &pcmPlayerObject, &audioSrc,
                                                &audioSnk,
                                                3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the player
    result = (*pcmPlayerObject)->Realize(pcmPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the play interface
    result = (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_PLAY, &pcmPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //注册回调缓冲区 获取缓冲队列接口
    result = (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_BUFFERQUEUE,
                                              &pcmBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*pcmBufferQueue)->RegisterCallback(pcmBufferQueue, pcmBufferCallBack, nullptr);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the volume interface
    result = (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_VOLUME, &pcmPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    pcmBufferCallBack(pcmBufferQueue, nullptr);

    return JNI_TRUE;
}

void playPCM_(jboolean isPlaying) {
    SLresult result;

    // make sure the asset audio player was created
    if (nullptr != pcmPlayerPlay) {
        // set the player's state
        result = (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay,
                                                isPlaying ? SL_PLAYSTATE_PLAYING
                                                          : SL_PLAYSTATE_PAUSED);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }
}

static AudioContext *audioContext;

/**
 * this callback handler is called every time a buffer finishes recording
 * 录制音频回调函数
 */
void audioRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == recorderBufferQueue);

    audioContext = (AudioContext *) context;
    assert(nullptr != audioContext);
    // for streaming recording, here we would call Enqueue to give recorder the next buffer to fill
    // but instead, this is a one-time buffer so we stop recording
    SLresult result;
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
    if (SL_RESULT_SUCCESS == result) {
        recorderSize = RECORDER_FRAMES * sizeof(short);
    }
}

/**
 * 录制音频播放回调函数
 * @param bq
 * @param context
 */
void audioPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    audioContext = (AudioContext *) context;
    assert(nullptr != audioContext);

    //开始读取文件
    if (feof(audioContext->pFile)) {
        fread(audioContext->buffer, audioContext->bufferSize, 1, audioContext->pFile);
        (*recorderBufferQueue)->Enqueue(recorderBufferQueue, audioContext->buffer,
                                        audioContext->bufferSize);
    } else {
        delete audioContext;
    }
}

/**
 * 录音功能
 */
jboolean createAudioRecorder_() {
    SLresult result;

    //输入源
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
    SLDataSource audioSrc = {&loc_dev, nullptr};

    //输出源
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                   1,
                                   SL_SAMPLINGRATE_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER,
                                   SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc,
                                                  &audioSnk, 1, id, req);
    if (SL_RESULT_SUCCESS != result) {
        return JNI_FALSE;
    }

    // realize the audio recorder
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return JNI_FALSE;
    }

    //获取录制音频播放接口对象
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_PLAY, &recorderPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the record interface
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the buffer queue interface
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                             &recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // register callback on the buffer queue
    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, audioRecorderCallback,
                                                      nullptr);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    return JNI_TRUE;
}

/**
 * 开始录音（录音的音频如何保存？）
 */
void startRecord_() {
    SLresult result;

    if (pthread_mutex_trylock(&audioEngineLock)) {
        return;
    }

    //如果已经开始录制了，则停止录制并清空缓冲队列
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recorderBufferQueue)->Clear(recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // the buffer is not valid for playback yet
    recorderSize = 0;

    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we would enqueue at least 2 empty buffers to start things off)
    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recorderBuffer,
                                             RECORDER_FRAMES * sizeof(short));
    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // start recording
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

/**
 * 停止录制
 */
void stopRecord_() {
    if (nullptr != recorderRecord) {
        SLresult result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);

        if (audioContext != nullptr) {
            delete audioContext;
        }
    }
}

/**
 * 播放录制的音频
 * @param isPlaying
 */
void playRecord_(jboolean isPlaying) {
    SLresult result;

    if (nullptr != recorderPlay) {
        result = (*recorderPlay)->SetPlayState(recorderPlay, isPlaying ? SL_PLAYSTATE_PLAYING
                                                                       : SL_PLAYSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }
}

#endif //MEDIASTUDY_OPENSL_ES_TEST_C
