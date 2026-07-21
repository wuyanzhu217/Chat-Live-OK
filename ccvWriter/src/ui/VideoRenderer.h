#pragma once

#include <QImage>
#include <QWidget>

namespace ccv {

/**
 * Simple video surface: paints the latest QImage scaled to fit.
 */
class VideoRenderer : public QWidget {
    Q_OBJECT
public:
    explicit VideoRenderer(QWidget* parent = nullptr);

public slots:
    void setFrame(const QImage& frame);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_frame;
};

} // namespace ccv
