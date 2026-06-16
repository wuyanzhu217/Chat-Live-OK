#ifndef STREAMING_CORE_MEDIATYPES_H
#define STREAMING_CORE_MEDIATYPES_H

#include <QString>

enum class CaptureState {
    Idle,
    Opened,
    Running,
    Error
};

enum class MediaKind {
    Video,
    Audio
};

struct CaptureConfig {
    QString devicePath;     // dshow: "video=XXX" / "audio=YYY"
    int width  = 1280;      // video only
    int height = 720;       // video only
    int fps    = 30;        // video only
    int sampleRate = 48000; // audio hint
    int channels   = 1;     // audio hint
};

#endif // STREAMING_CORE_MEDIATYPES_H
