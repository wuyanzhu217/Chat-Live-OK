#include "FfmpegDeviceEnumerator.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

namespace {

bool s_devicesRegistered = false;

void ensureRegistered()
{
    if (!s_devicesRegistered) {
        avdevice_register_all();
        s_devicesRegistered = true;
    }
}

QList<FfmpegDeviceInfo> listByMediaType(bool video)
{
    ensureRegistered();

    const AVInputFormat *inputFormat = av_find_input_format("dshow");
    if (!inputFormat)
        return {};

    AVDeviceInfoList *deviceList = nullptr;
    if (avdevice_list_input_sources(inputFormat, "dummy", nullptr, &deviceList) < 0)
        return {};

    QList<FfmpegDeviceInfo> result;
    const char *prefix = video ? "video=" : "audio=";

    for (int i = 0; i < deviceList->nb_devices; ++i) {
        const AVDeviceInfo *info = deviceList->devices[i];
        if (!info || !info->device_name)
            continue;

        const bool isVideoDevice = info->media_types
            && info->nb_media_types > 0
            && info->media_types[0] == AVMEDIA_TYPE_VIDEO;

        if (video != isVideoDevice)
            continue;

        FfmpegDeviceInfo entry;
        entry.isVideo = video;
        entry.friendlyName = QString::fromUtf8(
            info->device_description ? info->device_description : info->device_name);
        entry.devicePath = QString::fromUtf8(prefix) + QString::fromUtf8(info->device_name);
        result.append(entry);
    }

    avdevice_free_list_devices(&deviceList);
    return result;
}

} // namespace

void FfmpegDeviceEnumerator::registerAll()
{
    ensureRegistered();
}

QList<FfmpegDeviceInfo> FfmpegDeviceEnumerator::videoDevices()
{
    return listByMediaType(true);
}

QList<FfmpegDeviceInfo> FfmpegDeviceEnumerator::audioDevices()
{
    return listByMediaType(false);
}
