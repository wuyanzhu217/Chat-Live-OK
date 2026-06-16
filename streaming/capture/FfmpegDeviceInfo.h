#ifndef STREAMING_CAPTURE_FFMPEGDEVICEINFO_H
#define STREAMING_CAPTURE_FFMPEGDEVICEINFO_H

#include <QString>

struct FfmpegDeviceInfo {
    QString friendlyName;
    QString devicePath;   // dshow: "video=name" or "audio=name"
    bool isVideo = true;
};

#endif // STREAMING_CAPTURE_FFMPEGDEVICEINFO_H
