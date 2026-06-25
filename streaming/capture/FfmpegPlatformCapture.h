#ifndef STREAMING_CAPTURE_FFMPEGPLATFORMCAPTURE_H
#define STREAMING_CAPTURE_FFMPEGPLATFORMCAPTURE_H

#include "FfmpegDeviceInfo.h"

#include <QList>
#include <QString>

extern "C" {
#include <libavcodec/avcodec.h>
}

class FfmpegPlatformCapture {
public:
    static const char *videoInputFormat();
    static const char *audioInputFormat();
    static const char *inputFormatFor(AVMediaType mediaType);

    static QString defaultVideoDevicePath();
    static QString defaultAudioDevicePath();

    static QList<FfmpegDeviceInfo> listVideoDevices();
    static QList<FfmpegDeviceInfo> listAudioDevices();
};

#endif // STREAMING_CAPTURE_FFMPEGPLATFORMCAPTURE_H
