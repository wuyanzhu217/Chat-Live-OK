#ifndef STREAMING_CALL_CALLSESSIONCONTROLLER_H
#define STREAMING_CALL_CALLSESSIONCONTROLLER_H

#include "streaming/effects/EffectChain.h"
#include "streaming/effects/EffectController.h"
#include "streaming/effects/plugins/BlurEffect.h"
#include "streaming/effects/plugins/EqEffect.h"
#include "streaming/effects/plugins/FlipEffect.h"
#include "streaming/effects/plugins/GrayscaleEffect.h"
#include "streaming/effects/plugins/SharpenEffect.h"
#include "streaming/pipeline/PlayPipeline.h"
#include "streaming/pipeline/PublishPipeline.h"
#include "streaming/render/LocalVideoRenderer.h"
#include "streaming/render/RemoteVideoRenderer.h"

#include <QObject>

#include <memory>

class CallSessionController : public QObject {
    Q_OBJECT

    Q_PROPERTY(EffectController *effects READ effects CONSTANT)

public:
    explicit CallSessionController(QObject *parent = nullptr);

    EffectController *effects() { return &m_effects; }

    Q_INVOKABLE void bindRenderers(LocalVideoRenderer *local, RemoteVideoRenderer *remote);
    Q_INVOKABLE bool startPreview();
    Q_INVOKABLE void stopPreview();
    Q_INVOKABLE QStringList listVideoDevices();

signals:
    void previewStarted();
    void previewStopped();
    void errorOccurred(const QString &message);

private:
    void setupEffectChain();

    PublishPipeline m_publishPipeline;
    PlayPipeline m_playPipeline;
    EffectChain *m_effectChain = nullptr;
    std::shared_ptr<EqEffect> m_eqEffect;
    std::shared_ptr<BlurEffect> m_blurEffect;
    std::shared_ptr<GrayscaleEffect> m_grayscaleEffect;
    std::shared_ptr<FlipEffect> m_flipEffect;
    std::shared_ptr<SharpenEffect> m_sharpenEffect;
    EffectController m_effects;
    LocalVideoRenderer *m_localRenderer = nullptr;
    RemoteVideoRenderer *m_remoteRenderer = nullptr;
};

#endif // STREAMING_CALL_CALLSESSIONCONTROLLER_H
