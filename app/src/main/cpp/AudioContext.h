//
// Created by huangyong on 2019/9/1.
//

#ifndef MEDIASTUDY_AUDIOCONTEXT_H
#define MEDIASTUDY_AUDIOCONTEXT_H


#include <stdio.h>

class AudioContext {

public:

    FILE *pFile;

    uint8_t *buffer;

    size_t bufferSize;

    AudioContext(FILE *pFile, uint8_t *buffer, size_t bufferSize);

    ~AudioContext();
};

#endif //MEDIASTUDY_AUDIOCONTEXT_H
