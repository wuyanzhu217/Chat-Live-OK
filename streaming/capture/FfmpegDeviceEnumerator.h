#ifndef STREAMING_CAPTURE_FFMPEGDEVICEENUMERATOR_H
#define STREAMING_CAPTURE_FFMPEGDEVICEENUMERATOR_H

#include "FfmpegDeviceInfo.h"

#include <QList>

class FfmpegDeviceEnumerator
{
public:
    static void registerAll();
    static QList<FfmpegDeviceInfo> videoDevices();
    static QList<FfmpegDeviceInfo> audioDevices();
};

#endif // STREAMING_CAPTURE_FFMPEGDEVICEENUMERATOR_H
