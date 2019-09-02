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
#include "TimeHelper.h"

#define SAMPLE_RATE 44100
#define PERIOD_TIME 20  // 20ms
#define RECORDER_FRAMES (16000 * 5)
#define FRAME_SIZE SAMPLE_RATE * PERIOD_TIME / 1000
#define CHANNELS 2
#define BUFFER_SIZE (FRAME_SIZE * CHANNELS)

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
SLSeekItf pcmPlayerSeek;
SLVolumeItf pcmPlayerVolume = nullptr;
//缓冲区队列
SLAndroidSimpleBufferQueueItf pcmBufferQueue;
//pcm缓冲相关
FILE *pcmFile;
void *buffer;
uint8_t *out_buffer;
//pcm缓冲器队列接口

/**
 * 录音
 */
SLObjectItf recorderObject = nullptr;
SLPlayItf recorderPlay;
//音频录制状态接口
SLRecordItf recorderRecord = nullptr;
SLAndroidSimpleBufferQueueItf recorderBufferQueue = nullptr;

/**
 * 录音音频播放
 */
SLObjectItf audioPlayerObject;
SLPlayItf audioPlayerPlay;
SLAndroidSimpleBufferQueueItf audioBufferQueue;

// a mutext to guard against re-entrance to record & playback
// as well as make recording and playing back to be mutually exclusive
// this is to avoid crash at situations like:
// recording is in session [not finished]
// user presses record button and another recording coming in
// The action: when recording/playing back is not finished, ignore the new request
/**
 * 线程安全控制
 */
pthread_mutex_t audioEngineLock = PTHREAD_MUTEX_INITIALIZER;


/**
 * 音频录制数据临时缓存对象
 */
static AudioContext *audioContext;

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

void destroy_() {
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

    if (audioPlayerObject != nullptr) {
        (*audioPlayerObject)->Destroy(audioPlayerObject);
        audioPlayerObject = nullptr;
        audioPlayerPlay = nullptr;
        audioBufferQueue = nullptr;
    }

    if (audioContext != nullptr) {
        delete audioContext;
    }

    pthread_mutex_destroy(&audioEngineLock);
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
jboolean createPcmAudioPlayer_(JNIEnv *env, jobject instance, jstring filePath) {
    const char *pcmPath = env->GetStringUTFChars(filePath, nullptr);
    pcmFile = fopen(pcmPath, "re");
    if (pcmFile == nullptr) {
        LOGE("open file %s error", pcmPath);
        return JNI_FALSE;
    }

    out_buffer = (uint8_t *) malloc(44100 * 2 * 2);
    env->ReleaseStringUTFChars(filePath, pcmPath);

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
    //谷歌的官方例子是只录制一个指定时间（大概5秒左右）的音频，这里修改了下，录制时间由我们自己手动控制
    if (nullptr != audioContext->buffer) {
        fwrite(audioContext->buffer, audioContext->bufferSize, 1, audioContext->pFile);

        SLresult result;
        //音频录制状态
        SLuint32 state;

        result = (*recorderRecord)->GetRecordState(recorderRecord, &state);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        if (state == SL_RECORDSTATE_RECORDING) {
            result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, audioContext->buffer,
                                                     audioContext->bufferSize);
            assert(SL_RESULT_SUCCESS == result);
            (void) result;
        }
    }

    pthread_mutex_unlock(&audioEngineLock);
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

    pthread_mutex_unlock(&audioEngineLock);
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
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                     1};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                   2,
                                   SL_SAMPLINGRATE_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[2] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                 SL_IID_RECORD};
    const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioRecorder(engineEngine,
                                                  &recorderObject,
                                                  &audioSrc,
                                                  &audioSnk,
                                                  2, id, req);
    if (SL_RESULT_SUCCESS != result) {
        return JNI_FALSE;
    }

    // realize the audio recorder
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return JNI_FALSE;
    }

    //获取音频录制接口对象
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //获取缓冲队列接口对象
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                             &recorderBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    return JNI_TRUE;
}

/**
 * 创建录音音频播放器
 * @return
 */
jboolean createAudioPlayer_(JNIEnv *env, jobject instance, jstring filePath) {
    const char *pcmPath = env->GetStringUTFChars(filePath, nullptr);
    FILE *pFile = fopen(pcmPath, "re");
    if (pFile == nullptr) {
        LOGE("open file %s error", pcmPath);
        return JNI_FALSE;
    }
    env->ReleaseStringUTFChars(filePath, pcmPath);

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
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_PLAY};
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

    uint8_t *buffer = new uint8_t[BUFFER_SIZE];
    audioContext = new AudioContext(pFile, buffer, BUFFER_SIZE);
    //注册播放音频回调函数
    result = (*audioBufferQueue)->RegisterCallback(audioBufferQueue, audioPlayerCallback,
                                                   audioContext);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    return JNI_TRUE;
}

/**
 * 开始录音
 */
void startRecord_(JNIEnv *env, jobject instance, jstring filePath) {
    SLresult result;

    if (pthread_mutex_trylock(&audioEngineLock)) {
        return;
    }

    //根据传入的参数构造文件路径
    const char *pcmPath = env->GetStringUTFChars(filePath, nullptr);
    FILE *pFile = fopen(pcmPath, "w+");
    if (nullptr == pFile) {
        LOGI("%s, Open File Error", pcmPath);
        return;
    }
    env->ReleaseStringUTFChars(filePath, pcmPath);

    uint8_t *buffer = new uint8_t[BUFFER_SIZE];
    audioContext = new AudioContext(pFile, buffer, BUFFER_SIZE);

    //注册录制音频回调函数
    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, audioRecorderCallback,
                                                      audioContext);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    audioRecorderCallback(recorderBufferQueue, audioContext);

    // enqueue an empty buffer to be filled by the recorder
    // (for streaming recording, we would enqueue at least 2 empty buffers to start things off)
    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, audioContext->buffer,
                                             audioContext->bufferSize);
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

    if (nullptr != audioPlayerPlay) {
        result = (*audioPlayerPlay)->SetPlayState(audioPlayerPlay, isPlaying ? SL_PLAYSTATE_PLAYING
                                                                             : SL_PLAYSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }
}

#endif //MEDIASTUDY_OPENSL_ES_TEST_C
