#include "CallSessionController.h"

#include "streaming/capture/FfmpegDeviceEnumerator.h"
#include "streaming/capture/FfmpegPlatformCapture.h"
#include "streaming/effects/EffectChain.h"

CallSessionController::CallSessionController(QObject *parent)
    : QObject(parent)
{
    setupEffectChain();

    connect(&m_publishPipeline, &PublishPipeline::previewStarted,
            this, &CallSessionController::previewStarted);
    connect(&m_publishPipeline, &PublishPipeline::previewStopped,
            this, &CallSessionController::previewStopped);
    connect(&m_publishPipeline, &PublishPipeline::errorOccurred,
            this, &CallSessionController::errorOccurred);

    m_playPipeline.start();
}

void CallSessionController::setupEffectChain()
{
    m_effectChain = new EffectChain(this);
    m_eqEffect = std::make_shared<EqEffect>();
    m_sharpenEffect = std::make_shared<SharpenEffect>();
    m_blurEffect = std::make_shared<BlurEffect>();
    m_grayscaleEffect = std::make_shared<GrayscaleEffect>();
    m_flipEffect = std::make_shared<FlipEffect>();

    m_effectChain->addEffect(m_eqEffect);
    m_effectChain->addEffect(m_sharpenEffect);
    m_effectChain->addEffect(m_blurEffect);
    m_effectChain->addEffect(m_grayscaleEffect);
    m_effectChain->addEffect(m_flipEffect);

    m_effects.bind(
        m_effectChain,
        m_eqEffect,
        m_blurEffect,
        m_grayscaleEffect,
        m_flipEffect,
        m_sharpenEffect);

    m_publishPipeline.setEffectChain(m_effectChain);
}

void CallSessionController::bindRenderers(LocalVideoRenderer *local, RemoteVideoRenderer *remote)
{
    m_localRenderer = local;
    m_remoteRenderer = remote;
    m_publishPipeline.setLocalRenderer(local);
    m_playPipeline.setRemoteRenderer(remote);
}

bool CallSessionController::startPreview()
{
    QString devicePath;
    const QStringList devices = listVideoDevices();
    if (!devices.isEmpty()) {
        devicePath = devices.first();
    } else {
        devicePath = FfmpegPlatformCapture::defaultVideoDevicePath();
    }

    if (devicePath.isEmpty()) {
        emit errorOccurred(QStringLiteral("No video capture device found"));
        return false;
    }

    return m_publishPipeline.startPreview(devicePath);
}

void CallSessionController::stopPreview()
{
    m_publishPipeline.stopPreview();
}

QStringList CallSessionController::listVideoDevices()
{
    FfmpegDeviceEnumerator::registerAll();

    QStringList paths;
    for (const FfmpegDeviceInfo &device : FfmpegDeviceEnumerator::videoDevices())
        paths.append(device.devicePath);
    return paths;
}
