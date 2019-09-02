//
// Created by huangyong on 2019/9/2.
//

#ifndef MEDIASTUDY_TIMEHELPER_H
#define MEDIASTUDY_TIMEHELPER_H

#include <string>
#include <ctime>

static std::string genFileName() {
    std::string fileName;

    time_t now = time(0);
    tm *local_tm = localtime(&now);

    if (local_tm != nullptr) {
        fileName.append(std::string("" + 1900 + local_tm->tm_year))
                .append(std::string("" + local_tm->tm_mon))
                .append(std::string("" + local_tm->tm_mday))
                .append(std::string("_"))
                .append(std::string("" + local_tm->tm_hour))
                .append(std::string("" + local_tm->tm_min))
                .append(std::string("" + local_tm->tm_sec))
                .append(std::string(".pcm"));
    }

    return fileName;
}

#endif //MEDIASTUDY_TIMEHELPER_H
