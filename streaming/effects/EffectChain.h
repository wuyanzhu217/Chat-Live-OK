#ifndef STREAMING_EFFECTS_EFFECTCHAIN_H
#define STREAMING_EFFECTS_EFFECTCHAIN_H

#include "EffectContext.h"
#include "FfmpegFilterGraph.h"
#include "IVideoEffect.h"
#include "streaming/core/FramePool.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

struct AVFrame;

class EffectChain : public QObject {
    Q_OBJECT

public:
    explicit EffectChain(QObject *parent = nullptr);

    void addEffect(const std::shared_ptr<IVideoEffect> &fx);
    void removeEffect(const QString &id);
    std::shared_ptr<IVideoEffect> effect(const QString &id) const;

    bool configure(const EffectContext &ctx);
    bool process(const AVFrame *in, AVFrame *out);
    bool isEmpty() const;
    bool rebuildIfDirty();

public slots:
    void setEffectEnabled(const QString &id, bool on);

signals:
    void effectsChanged();

private:
    bool collectFilterSpecs(QStringList *specs) const;
    bool runFilterGraph(const AVFrame *in, AVFrame *out);

    EffectContext m_ctx;
    FfmpegFilterGraph m_graph;
    FramePool m_pool;
    QVector<std::shared_ptr<IVideoEffect>> m_effects;
};

#endif // STREAMING_EFFECTS_EFFECTCHAIN_H
