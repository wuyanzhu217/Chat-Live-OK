#include "EffectController.h"

EffectController::EffectController(QObject *parent)
    : QObject(parent)
{
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(200);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &EffectController::onRebuildTimer);
}

void EffectController::bind(EffectChain *chain,
                            const std::shared_ptr<EqEffect> &eq,
                            const std::shared_ptr<BlurEffect> &blur,
                            const std::shared_ptr<GrayscaleEffect> &grayscale,
                            const std::shared_ptr<FlipEffect> &flip,
                            const std::shared_ptr<SharpenEffect> &sharpen)
{
    m_chain = chain;
    m_eq = eq;
    m_blur = blur;
    m_grayscale = grayscale;
    m_flip = flip;
    m_sharpen = sharpen;

    emit brightnessChanged();
    emit contrastChanged();
    emit saturationChanged();
    emit blurChanged();
    emit enabledChanged();
    emit grayscaleChanged();
    emit flipHChanged();
    emit flipVChanged();
    emit sharpenChanged();
}

qreal EffectController::brightness() const
{
    return m_eq ? m_eq->brightness() : 0.0;
}

qreal EffectController::contrast() const
{
    return m_eq ? m_eq->contrast() : 1.0;
}

qreal EffectController::saturation() const
{
    return m_eq ? m_eq->saturation() : 1.0;
}

qreal EffectController::blur() const
{
    return m_blur ? m_blur->sigma() : 0.0;
}

bool EffectController::grayscale() const
{
    return m_grayscale && m_grayscale->enabled();
}

bool EffectController::flipH() const
{
    return m_flip && m_flip->horizontal();
}

bool EffectController::flipV() const
{
    return m_flip && m_flip->vertical();
}

qreal EffectController::sharpen() const
{
    return m_sharpen ? m_sharpen->amount() : 0.0;
}

void EffectController::setBrightness(qreal value)
{
    if (!m_eq)
        return;
    m_eq->setBrightness(value);
    emit brightnessChanged();
    scheduleRebuild();
}

void EffectController::setContrast(qreal value)
{
    if (!m_eq)
        return;
    m_eq->setContrast(value);
    emit contrastChanged();
    scheduleRebuild();
}

void EffectController::setSaturation(qreal value)
{
    if (!m_eq)
        return;
    m_eq->setSaturation(value);
    emit saturationChanged();
    scheduleRebuild();
}

void EffectController::setBlur(qreal value)
{
    if (!m_blur)
        return;
    m_blur->setSigma(value);
    emit blurChanged();
    scheduleRebuild();
}

void EffectController::setEnabled(bool on)
{
    if (m_enabled == on)
        return;

    m_enabled = on;
    if (m_eq)
        m_eq->setEnabled(on);
    if (m_blur)
        m_blur->setEnabled(on);

    scheduleRebuild();
    emit enabledChanged();
}

void EffectController::setGrayscale(bool on)
{
    if (!m_grayscale)
        return;
    m_grayscale->setEnabled(on);
    emit grayscaleChanged();
    scheduleRebuild();
}

void EffectController::setFlipH(bool on)
{
    if (!m_flip)
        return;
    m_flip->setHorizontal(on);
    m_flip->setEnabled(m_flip->horizontal() || m_flip->vertical());
    emit flipHChanged();
    scheduleRebuild();
}

void EffectController::setFlipV(bool on)
{
    if (!m_flip)
        return;
    m_flip->setVertical(on);
    m_flip->setEnabled(m_flip->horizontal() || m_flip->vertical());
    emit flipVChanged();
    scheduleRebuild();
}

void EffectController::setSharpen(qreal value)
{
    if (!m_sharpen)
        return;
    m_sharpen->setAmount(value);
    m_sharpen->setEnabled(value > 0.001);
    emit sharpenChanged();
    scheduleRebuild();
}

void EffectController::scheduleRebuild()
{
    m_rebuildTimer.start();
}

void EffectController::onRebuildTimer()
{
    if (m_chain)
        m_chain->rebuildIfDirty();
}
