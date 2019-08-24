//
// Created by huangyong on 2019/8/23.
//
#ifndef MEDIASTUDY_OPENSL_ES_TEST_H
#define MEDIASTUDY_OPENSL_ES_TEST_H

//本地音频
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class OpenSLTest {

public:


private:

    //engine interfaces
    SLObjectItf engineObject = nullptr;
    SLEngineItf engineEngine = nullptr;

    //output mix interfaces
    SLObjectItf outputMixObject = nullptr;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = nullptr;

    //recorder interfaces
    SLObjectItf recorderObject = nullptr;
    SLRecordItf recorderRecord = nullptr;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue = nullptr;
};

#endif //MEDIASTUDY_OPENSL_ES_TEST_H
