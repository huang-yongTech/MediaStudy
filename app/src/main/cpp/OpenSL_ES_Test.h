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

// Size of the recording buffer queue
#define NB_BUFFERS_IN_QUEUE 1

// Size of each buffer in the queue
#define BUFFER_SIZE_IN_SAMPLES 8192
#define BUFFER_SIZE_IN_BYTES   (2 * BUFFER_SIZE_IN_SAMPLES)

//engine interfaces
static SLObjectItf engineObject = nullptr;
//创建其他opensl-es对象类型的接口对象，该对象紧随SLObjectItf创建之后创建
SLEngineItf engineEngine = nullptr;

/**
 * 混音器
 */
SLObjectItf outputMixObject = nullptr;
//一些环境混响属性的设置
SLEnvironmentalReverbItf outputMixEnvironmentalReverb = nullptr;
SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

/**
 * assets文件播放
 */
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

/**
 * pcm文件播放
 */
SLObjectItf pcmPlayerObject = nullptr;
SLPlayItf pcmPlayerPlay = nullptr;
//SLSeekItf好像不能和缓冲队列一起使用
SLSeekItf pcmPlayerSeek;
SLVolumeItf pcmPlayerVolume = nullptr;
//缓冲区队列
SLAndroidSimpleBufferQueueItf pcmBufferQueue;
//pcm缓冲相关
FILE *pcmFile;
void *pcmBuffer;
uint8_t *out_buffer;

/**
 * 录音
 */
SLObjectItf recorderObject = nullptr;
//音频录制状态接口
SLRecordItf recorderRecord = nullptr;
SLAndroidSimpleBufferQueueItf recorderBufferQueue = nullptr;
//录音缓冲区大小
static size_t AUDIO_BUFFER_SIZE = BUFFER_SIZE_IN_BYTES;

/**
 * 录音音频播放
 */
SLObjectItf audioPlayerObject;
SLPlayItf audioPlayerPlay;
SLAndroidSimpleBufferQueueItf audioBufferQueue;
SLAndroidSimpleBufferQueueState audioBufferQuqueState;
SLVolumeItf audioPlayerVolumn;

// a mutext to guard against re-entrance to record & playback
// as well as make recording and playing back to be mutually exclusive
// this is to avoid crash at situations like:
// recording is in session [not finished]
// user presses record button and another recording coming in
// The action: when recording/playing back is not finished, ignore the new request
/**
 * 线程安全控制，线程互斥同步
 */
//pthread_mutex_t audioEngineLock = PTHREAD_MUTEX_INITIALIZER;


/**
 * 音频录制数据临时缓存对象
 */
static AudioContext *audioContext = nullptr;

void setBufferSizeInSize_(jint bufferSizeInBytes) {
    AUDIO_BUFFER_SIZE = static_cast<size_t>(bufferSizeInBytes);
}

void createEngine_() {
    SLresult result;

    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };
    //创建SLObjectItf
    result = slCreateEngine(&engineObject, 1, EngineOption, 0, nullptr, nullptr);
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
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
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

void shutdown_() {
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

    if (audioPlayerObject != nullptr) {
        (*audioPlayerObject)->Destroy(audioPlayerObject);
        audioPlayerObject = nullptr;
        audioPlayerPlay = nullptr;
        audioBufferQueue = nullptr;
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

    if (audioContext != nullptr) {
        delete audioContext;
        audioContext = nullptr;
    }

    //pcm音频播放缓冲区
    if (pcmFile != nullptr) {
        fclose(pcmFile);
        free(out_buffer);

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
                                  jstring filePath) {
    SLresult result;

    // convert Java string to UTF-8
    const char *utf8 = env->GetStringUTFChars(filePath, nullptr);
    assert(nullptr != utf8);

    // use asset manager to open asset by filename
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    assert(nullptr != mgr);
    AAsset *asset = AAssetManager_open(mgr, utf8, AASSET_MODE_UNKNOWN);

    // release the Java string and UTF-8
    env->ReleaseStringUTFChars(filePath, utf8);

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
    const SLInterfaceID ids[4] = {SL_IID_PLAY, SL_IID_SEEK, SL_IID_MUTESOLO, SL_IID_VOLUME};
    const SLboolean req[4] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &fdPlayerObject, &audioSrc, &audioSnk,
                                                4, ids, req);
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
    if (!feof(pcmFile)) {
        fread(out_buffer, BUFFER_SIZE_IN_BYTES, 1, pcmFile);
        if (out_buffer == nullptr) {
            LOGI("%s", "read end");
        } else {
            LOGI("%s", "reading");
        }
        *pcm = out_buffer;
    }
}

void pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bufferQueueItf, void *context) {
    //assert(NULL == context);
    assert(bufferQueueItf == pcmBufferQueue);
    getPcmData(&pcmBuffer);
    // for streaming playback, replace this test by logic to find and fill the next buffer
    if (nullptr != pcmBuffer) {
        SLresult result;
        // enqueue another buffer
        result = (*bufferQueueItf)->Enqueue(bufferQueueItf, pcmBuffer, BUFFER_SIZE_IN_BYTES);
        // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        // which for this code example would indicate a programming error
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }
}

void pcmPlayEndCallback(SLPlayItf playItf, void *context, SLuint32 event) {
    SLresult result;

    if (event & SL_PLAYEVENT_HEADATEND) {
        result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        rewind(pcmFile);
    }
}

/**
 * pcm文件音频播放
 * @param env
 * @param instance
 * @param fileName
 * @return
 */
jboolean createPcmAudioPlayer_(JNIEnv *env, jobject instance, jstring filePath) {
    const char *pcmPath = env->GetStringUTFChars(filePath, nullptr);
    pcmFile = fopen(pcmPath, "re");
    if (pcmFile == nullptr) {
        LOGE("open file %s error", pcmPath);
        return JNI_FALSE;
    }
    env->ReleaseStringUTFChars(filePath, pcmPath);

    //分配读取文件缓冲区
    out_buffer = (uint8_t *) malloc(BUFFER_SIZE_IN_BYTES);

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
    const SLInterfaceID ids[3] = {SL_IID_PLAY, SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
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

    //获取播放器接口
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

    //获取音量接口
    result = (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_VOLUME, &pcmPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //注册播放结束回调
    result = (*pcmPlayerPlay)->RegisterCallback(pcmPlayerPlay, pcmPlayEndCallback, nullptr);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //设置播放结束回调
    result = (*pcmPlayerPlay)->SetCallbackEventsMask(pcmPlayerPlay, SL_PLAYEVENT_HEADATEND);
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

/**
 * Callback for recording buffer queue events
 */
void recCallback(SLRecordItf caller, void *context, SLuint32 event) {
    if (SL_RECORDEVENT_HEADATNEWPOS & event) {
        SLmillisecond pMsec = 0;
        (*caller)->GetPosition(caller, &pMsec);
        LOGI("SL_RECORDEVENT_HEADATNEWPOS current position=%ums\n", pMsec);
    }

    if (SL_RECORDEVENT_HEADATMARKER & event) {
        SLmillisecond pMsec = 0;
        (*caller)->GetPosition(caller, &pMsec);
        LOGI("SL_RECORDEVENT_HEADATMARKER current position=%ums\n", pMsec);
    }
}

/**
 * this callback handler is called every time a buffer finishes recording
 * 录制音频回调函数
 */
void audioRecorderCallback(SLAndroidSimpleBufferQueueItf bufferQueueItf, void *context) {
    assert(bufferQueueItf == recorderBufferQueue);

    audioContext = (AudioContext *) context;
    assert(nullptr != audioContext);

    // for streaming recording, here we would call Enqueue to give recorder the next buffer to fill
    // but instead, this is a one-time buffer so we stop recording
    if (nullptr != audioContext->buffer) {
        fwrite(audioContext->buffer, audioContext->bufferSize, 1, audioContext->pFile);

        SLresult result;
        //音频录制状态
        SLuint32 state;

        //获取录制状态
        result = (*recorderRecord)->GetRecordState(recorderRecord, &state);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        if (state == SL_RECORDSTATE_RECORDING) {
            result = (*bufferQueueItf)->Enqueue(bufferQueueItf, audioContext->buffer,
                                                audioContext->bufferSize);
            assert(SL_RESULT_SUCCESS == result);
            (void) result;
        }
    }
}

/**
 * 录制音频播放回调函数
 * @param bq
 * @param context
 */
void audioPlayerCallback(SLAndroidSimpleBufferQueueItf bufferQueueItf, void *context) {
    assert(bufferQueueItf == audioBufferQueue);

    audioContext = (AudioContext *) context;
    assert(nullptr != audioContext);

    //开始读取文件
    if (!feof(audioContext->pFile)) {
        fread(audioContext->buffer, audioContext->bufferSize, 1,
              audioContext->pFile);
        if (audioContext->buffer == nullptr) {
            LOGI("%s", "read end");
        } else {
            LOGI("%s", "reading");
        }
    }

    if (audioContext->buffer != nullptr) {
        SLresult result = (*bufferQueueItf)->Enqueue(bufferQueueItf, audioContext->buffer,
                                                     audioContext->bufferSize);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }
}

/**
 * 录音功能
 */
jboolean createAudioRecorder_() {
    SLresult result;

    //输入源
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,
                                      SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT,
                                      nullptr};
    SLDataSource audioSrc = {&loc_dev, nullptr};

    //输出源
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                   2,
                                   SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    //创建audio recorder，需要RECORD_AUDIO权限
    const SLInterfaceID id[2] = {SL_IID_RECORD, SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc,
                                                  &audioSnk,
                                                  2, id, req);
    if (SL_RESULT_SUCCESS != result) {
        return JNI_FALSE;
    }
    (void) result;

    // realize the audio recorder
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return JNI_FALSE;
    }
    (void) result;

    //获取缓冲队列接口对象
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                             &recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //获取音频录制接口对象
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    /* Set up the recorder callback to get events during the recording */
    result = (*recorderRecord)->SetMarkerPosition(recorderRecord, 2000);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recorderRecord)->SetPositionUpdatePeriod(recorderRecord, 500);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recorderRecord)->SetCallbackEventsMask(recorderRecord, SL_RECORDEVENT_HEADATMARKER |
                                                                      SL_RECORDEVENT_HEADATNEWPOS);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*recorderRecord)->RegisterCallback(recorderRecord, recCallback, nullptr);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    return JNI_TRUE;
}

/**
 * 开始录音
 */
void startRecord_(JNIEnv *env, jobject instance, jstring filePath) {
    SLresult result;

    //根据传入的参数构造文件路径
    const char *audioPath = env->GetStringUTFChars(filePath, nullptr);
    FILE *pFile = fopen(audioPath, "we+");
    if (nullptr == pFile) {
        LOGI("%s, Open File Error", audioPath);
        return;
    }
    env->ReleaseStringUTFChars(filePath, audioPath);

    uint8_t *buffer = new uint8_t[AUDIO_BUFFER_SIZE];
    audioContext = new AudioContext(pFile, buffer, AUDIO_BUFFER_SIZE);
    //注册录制音频回调函数
    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, audioRecorderCallback,
                                                      audioContext);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we would enqueue at least 2 empty buffers to start things off)
    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, audioContext->buffer,
                                             audioContext->bufferSize);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //开始录音
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

/**
 * 停止录制
 */
void stopRecord_() {
    SLresult result;

    if (nullptr != recorderRecord) {
        result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        result = (*recorderBufferQueue)->Clear(recorderBufferQueue);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        if (audioContext != nullptr) {
            delete audioContext;
            audioContext = nullptr;
        }
    }
}

/**
 * 创建录音音频播放器
 * @return
 */
jboolean createAudioPlayer_() {
    SLresult result;

    //输入源
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                            2};
    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM, //播放pcm格式的数据
            2, //1个声道（非立体声）
            SL_SAMPLINGRATE_44_1,//44100hz的频率
            SL_PCMSAMPLEFORMAT_FIXED_16,//位数 16位
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致就行
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, //非立体声（前左前右）
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };

    SLDataSource audioSrc = {&android_queue, &format_pcm};

    //输出源
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, nullptr};

    //创建audio player
    const SLInterfaceID ids[3] = {SL_IID_PLAY, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &audioPlayerObject, &audioSrc,
                                                &audioSnk,
                                                3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*audioPlayerObject)->Realize(audioPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*audioPlayerObject)->GetInterface(audioPlayerObject, SL_IID_PLAY, &audioPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*audioPlayerObject)->GetInterface(audioPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                &audioBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*audioPlayerObject)->GetInterface(audioPlayerObject, SL_IID_VOLUME,
                                                &audioPlayerVolumn);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    return JNI_TRUE;
}

/**
 * 播放录制的音频
 * @param isPlaying
 */
void playRecord_(JNIEnv *env, jobject instance, jstring filePath, jboolean isPlaying) {
    const char *audioPath = env->GetStringUTFChars(filePath, nullptr);
    FILE *pFile = fopen(audioPath, "re");
    if (pFile == nullptr) {
        LOGE("open file %s error", audioPath);
        return;
    }
    env->ReleaseStringUTFChars(filePath, audioPath);

    uint8_t *buffer = new uint8_t[AUDIO_BUFFER_SIZE];
    audioContext = new AudioContext(pFile, buffer, AUDIO_BUFFER_SIZE);

    SLresult result;

    result = (*audioBufferQueue)->GetState(audioBufferQueue, &audioBufferQuqueState);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    if (audioBufferQuqueState.count == 0) {
        result = (*audioBufferQueue)->RegisterCallback(audioBufferQueue, audioPlayerCallback,
                                                       audioContext);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
        audioPlayerCallback(audioBufferQueue, audioContext);
    }

    result = (*audioPlayerPlay)->SetPlayState(audioPlayerPlay, isPlaying ? SL_PLAYSTATE_PLAYING
                                                                         : SL_PLAYSTATE_STOPPED);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

#endif //MEDIASTUDY_OPENSL_ES_TEST_C
