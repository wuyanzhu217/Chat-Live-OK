#include "FfmpegPlatformCapture.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

#include <QDir>
#include <QFileInfo>

namespace {

void ensureDevicesRegistered()
{
    static bool registered = false;
    if (!registered) {
        avdevice_register_all();
        registered = true;
    }
}

QList<FfmpegDeviceInfo> listDevicesByInputFormat(const char *formatName, bool video)
{
    ensureDevicesRegistered();

    const AVInputFormat *inputFormat = av_find_input_format(formatName);
    if (!inputFormat)
        return {};

    AVDeviceInfoList *deviceList = nullptr;
    if (avdevice_list_input_sources(inputFormat, "dummy", nullptr, &deviceList) < 0)
        return {};

    QList<FfmpegDeviceInfo> result;

#if defined(Q_OS_WIN)
    const char *prefix = video ? "video=" : "audio=";
#endif

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

#if defined(Q_OS_WIN)
        entry.devicePath = QString::fromUtf8(prefix) + QString::fromUtf8(info->device_name);
#else
        entry.devicePath = QString::fromUtf8(info->device_name);
#endif
        result.append(entry);
    }

    avdevice_free_list_devices(&deviceList);
    return result;
}

#if !defined(Q_OS_WIN)
QList<FfmpegDeviceInfo> listLinuxVideoDevicesFallback()
{
    QList<FfmpegDeviceInfo> result;
    const QStringList nodes = QDir(QStringLiteral("/dev")).entryList(
        QStringList{QStringLiteral("video*")}, QDir::System);

    for (const QString &node : nodes) {
        const QString path = QStringLiteral("/dev/") + node;
        if (!QFileInfo::exists(path))
            continue;

        FfmpegDeviceInfo entry;
        entry.isVideo = true;
        entry.friendlyName = node;
        entry.devicePath = path;
        result.append(entry);
    }
    return result;
}
#endif

} // namespace

const char *FfmpegPlatformCapture::videoInputFormat()
{
#if defined(Q_OS_WIN)
    return "dshow";
#else
    return "v4l2";
#endif
}

const char *FfmpegPlatformCapture::audioInputFormat()
{
#if defined(Q_OS_WIN)
    return "dshow";
#else
    return "pulse";
#endif
}

const char *FfmpegPlatformCapture::inputFormatFor(const AVMediaType mediaType)
{
    return mediaType == AVMEDIA_TYPE_AUDIO ? audioInputFormat() : videoInputFormat();
}

QString FfmpegPlatformCapture::defaultVideoDevicePath()
{
#if defined(Q_OS_WIN)
    const QList<FfmpegDeviceInfo> devices = listVideoDevices();
    if (!devices.isEmpty())
        return devices.first().devicePath;
    return {};
#else
    if (QFileInfo::exists(QStringLiteral("/dev/video0")))
        return QStringLiteral("/dev/video0");

    const QList<FfmpegDeviceInfo> devices = listVideoDevices();
    if (!devices.isEmpty())
        return devices.first().devicePath;
    return {};
#endif
}

QString FfmpegPlatformCapture::defaultAudioDevicePath()
{
#if defined(Q_OS_WIN)
    const QList<FfmpegDeviceInfo> devices = listAudioDevices();
    if (!devices.isEmpty())
        return devices.first().devicePath;
    return {};
#else
    return QStringLiteral("default");
#endif
}

QList<FfmpegDeviceInfo> FfmpegPlatformCapture::listVideoDevices()
{
#if defined(Q_OS_WIN)
    return listDevicesByInputFormat(videoInputFormat(), true);
#else
    QList<FfmpegDeviceInfo> devices = listDevicesByInputFormat(videoInputFormat(), true);
    if (devices.isEmpty())
        devices = listLinuxVideoDevicesFallback();
    return devices;
#endif
}

QList<FfmpegDeviceInfo> FfmpegPlatformCapture::listAudioDevices()
{
    return listDevicesByInputFormat(audioInputFormat(), false);
}
