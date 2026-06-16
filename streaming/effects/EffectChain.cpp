#include "EffectChain.h"

extern "C" {
#include <libavutil/frame.h>
}

bool EffectChain::isEmpty() const
{
    for (const auto &fx : m_effects) {
        if (!fx->enabled())
            continue;
        if (fx->type() == EffectType::FilterGraph && !fx->filterSpec().isEmpty())
            return false;
    }
    return true;
}

EffectChain::EffectChain(QObject *parent)
    : QObject(parent)
{
}

void EffectChain::addEffect(const std::shared_ptr<IVideoEffect> &fx)
{
    if (!fx)
        return;

    removeEffect(fx->id());
    m_effects.append(fx);
    emit effectsChanged();
}

void EffectChain::removeEffect(const QString &id)
{
    for (int i = m_effects.size() - 1; i >= 0; --i) {
        if (m_effects.at(i)->id() == id) {
            m_effects.removeAt(i);
            emit effectsChanged();
        }
    }
}

std::shared_ptr<IVideoEffect> EffectChain::effect(const QString &id) const
{
    for (const auto &fx : m_effects) {
        if (fx->id() == id)
            return fx;
    }
    return {};
}

bool EffectChain::collectFilterSpecs(QStringList *specs) const
{
    if (!specs)
        return false;

    specs->clear();
    for (const auto &fx : m_effects) {
        if (!fx->enabled() || fx->type() != EffectType::FilterGraph)
            continue;

        const QString spec = fx->filterSpec();
        if (spec.isEmpty())
            continue;

        const QStringList parts = spec.split(',', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty())
                specs->append(trimmed);
        }
    }
    return true;
}

bool EffectChain::runFilterGraph(const AVFrame *in, AVFrame *out)
{
    return m_graph.process(in, out);
}

bool EffectChain::rebuildIfDirty()
{
    bool need = !m_graph.isBuilt();
    for (const auto &fx : m_effects) {
        if (fx->isDirty()) {
            need = true;
            break;
        }
    }

    if (!need)
        return true;

    QStringList specs;
    collectFilterSpecs(&specs);

    if (specs.isEmpty()) {
        m_graph.destroy();
        for (auto &fx : m_effects)
            fx->clearDirty();
        return true;
    }

    const bool ok = m_graph.build(m_ctx, specs);
    for (auto &fx : m_effects)
        fx->clearDirty();
    return ok;
}

bool EffectChain::configure(const EffectContext &ctx)
{
    m_ctx = ctx;
    m_pool.reset();
    m_graph.destroy();
    return rebuildIfDirty();
}

bool EffectChain::process(const AVFrame *in, AVFrame *out)
{
    if (!in || !out)
        return false;

    if (isEmpty())
        return av_frame_ref(out, in) >= 0;

    rebuildIfDirty();

    if (!m_graph.isBuilt())
        return av_frame_ref(out, in) >= 0;

    AVFrame *temp = m_pool.acquire(in->width, in->height, AV_PIX_FMT_YUV420P);
    if (!temp)
        return av_frame_ref(out, in) >= 0;

    if (!runFilterGraph(in, temp)) {
        m_pool.release(temp);
        return av_frame_ref(out, in) >= 0;
    }

    av_frame_copy_props(out, temp);
    av_frame_copy(out, temp);
    m_pool.release(temp);
    return true;
}

void EffectChain::setEffectEnabled(const QString &id, bool on)
{
    const auto fx = effect(id);
    if (!fx)
        return;

    fx->setEnabled(on);
    rebuildIfDirty();
}
