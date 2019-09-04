//
// Created by huangyong on 2019/9/1.
//

#ifndef MEDIASTUDY_AUDIOCONTEXT_H
#define MEDIASTUDY_AUDIOCONTEXT_H

#include <stdio.h>
#include <SLES/OpenSLES.h>

/**
 * 音频缓存相关配置
 */
class AudioContext {

public:
    //文件指针，用于写入录制数据到文件或者从文件读取录制数据
    FILE *pFile;
    //音频临时缓存数据
    uint8_t *buffer;
    //缓存数据大小
    size_t bufferSize;

    AudioContext();

    AudioContext(FILE *pFile, uint8_t *buffer, size_t bufferSize);

    ~AudioContext();
};

#endif //MEDIASTUDY_AUDIOCONTEXT_H
