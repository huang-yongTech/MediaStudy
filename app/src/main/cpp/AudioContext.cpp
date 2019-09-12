//
// Created by huangyong on 2019/9/1.
//
#include "AudioContext.h"

AudioContext::AudioContext()
        : pFile(nullptr), buffer(nullptr), bufferSize(0) {

}

AudioContext::AudioContext(FILE *pFile, uint8_t *buffer, size_t bufferSize)
        : pFile(pFile), buffer(buffer), bufferSize(bufferSize) {
}

AudioContext::~AudioContext() {
    fclose(pFile);
    delete buffer;

    pFile = nullptr;
    buffer = nullptr;
    bufferSize = 0;
}
