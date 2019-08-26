//
// Created by huangyong on 2019/8/23.
//
#ifndef MEDIASTUDY_OPENSL_ES_TEST_H
#define MEDIASTUDY_OPENSL_ES_TEST_H

//本地音频
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <pthread.h>
#include <jni.h>

#define SAWTOOTH_FRAMES 8000

#define RECORDER_FRAMES (16000 * 5)

class OpenSLTest {

public:

    OpenSLTest();

    ~OpenSLTest();

    void createEngine();

    void createBufferQueueAudioPlayer(jint sampleRate, jint bufSize);

    void playClip(jint which, int count);

    void createAssetsAudioPlayer(jobject assetManager, jstring fileName);

    void playAssets();

    void playPCM();

    void createAudioRecorder();

    void startRecord();

    void stopRecord();

    void playRecord();

private:

    // pre-recorded sound clips, both are 8 kHz mono 16-bit signed little endian
    const char hello[] =

#include "hello_clip.h";
    const char android[] =

#include "android_clip.h";

    //engine interfaces
    SLObjectItf engineObject = nullptr;
    SLEngineItf engineEngine = nullptr;

    //output mix interfaces
    SLObjectItf outputMixObject = nullptr;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = nullptr;

    // file descriptor player interfaces
    SLObjectItf fdPlayerObject = nullptr;
    SLPlayItf fdPlayerPlay;
    SLSeekItf fdPlayerSeek;
    SLMuteSoloItf fdPlayerMuteSolo;
    SLVolumeItf fdPlayerVolume;

    //recorder interfaces
    SLObjectItf recorderObject = nullptr;
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
    int nextCount;

    // 5 seconds of recorded audio at 16 kHz mono, 16-bit signed little endian
    short recorderBuffer[RECORDER_FRAMES];
    unsigned recorderSize = 0;

    // synthesized sawtooth clip
    short sawtoothBuffer[SAWTOOTH_FRAMES];

    void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

    //创建采样缓冲池
    short *createResampledBuf(uint32_t idx, uint32_t srcRate, unsigned *size);

    //释放缓冲池
    void releaseResampleBuf(void);

    void destroy();
};

#endif //MEDIASTUDY_OPENSL_ES_TEST_H
