#include "FfmpegDeviceEnumerator.h"
#include "FfmpegPlatformCapture.h"

void FfmpegDeviceEnumerator::registerAll()
{
    FfmpegPlatformCapture::listVideoDevices();
}

QList<FfmpegDeviceInfo> FfmpegDeviceEnumerator::videoDevices()
{
    return FfmpegPlatformCapture::listVideoDevices();
}

QList<FfmpegDeviceInfo> FfmpegDeviceEnumerator::audioDevices()
{
    return FfmpegPlatformCapture::listAudioDevices();
}
