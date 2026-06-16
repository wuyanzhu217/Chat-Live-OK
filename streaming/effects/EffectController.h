#ifndef STREAMING_EFFECTS_EFFECTCONTROLLER_H
#define STREAMING_EFFECTS_EFFECTCONTROLLER_H

#include "EffectChain.h"
#include "plugins/BlurEffect.h"
#include "plugins/EqEffect.h"
#include "plugins/FlipEffect.h"
#include "plugins/GrayscaleEffect.h"
#include "plugins/SharpenEffect.h"

#include <QObject>
#include <QTimer>

#include <memory>

class EffectController : public QObject {
    Q_OBJECT

    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal contrast READ contrast WRITE setContrast NOTIFY contrastChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    Q_PROPERTY(qreal blur READ blur WRITE setBlur NOTIFY blurChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool grayscale READ grayscale WRITE setGrayscale NOTIFY grayscaleChanged)
    Q_PROPERTY(bool flipH READ flipH WRITE setFlipH NOTIFY flipHChanged)
    Q_PROPERTY(bool flipV READ flipV WRITE setFlipV NOTIFY flipVChanged)
    Q_PROPERTY(qreal sharpen READ sharpen WRITE setSharpen NOTIFY sharpenChanged)

public:
    explicit EffectController(QObject *parent = nullptr);

    void bind(EffectChain *chain,
              const std::shared_ptr<EqEffect> &eq,
              const std::shared_ptr<BlurEffect> &blur,
              const std::shared_ptr<GrayscaleEffect> &grayscale = {},
              const std::shared_ptr<FlipEffect> &flip = {},
              const std::shared_ptr<SharpenEffect> &sharpen = {});

    qreal brightness() const;
    qreal contrast() const;
    qreal saturation() const;
    qreal blur() const;
    bool enabled() const { return m_enabled; }
    bool grayscale() const;
    bool flipH() const;
    bool flipV() const;
    qreal sharpen() const;

public slots:
    void setBrightness(qreal value);
    void setContrast(qreal value);
    void setSaturation(qreal value);
    void setBlur(qreal value);
    void setEnabled(bool on);
    void setGrayscale(bool on);
    void setFlipH(bool on);
    void setFlipV(bool on);
    void setSharpen(qreal value);

signals:
    void brightnessChanged();
    void contrastChanged();
    void saturationChanged();
    void blurChanged();
    void enabledChanged();
    void grayscaleChanged();
    void flipHChanged();
    void flipVChanged();
    void sharpenChanged();

private slots:
    void onRebuildTimer();

private:
    void scheduleRebuild();

    EffectChain *m_chain = nullptr;
    std::shared_ptr<EqEffect> m_eq;
    std::shared_ptr<BlurEffect> m_blur;
    std::shared_ptr<GrayscaleEffect> m_grayscale;
    std::shared_ptr<FlipEffect> m_flip;
    std::shared_ptr<SharpenEffect> m_sharpen;
    QTimer m_rebuildTimer;
    bool m_enabled = true;
};

#endif // STREAMING_EFFECTS_EFFECTCONTROLLER_H
