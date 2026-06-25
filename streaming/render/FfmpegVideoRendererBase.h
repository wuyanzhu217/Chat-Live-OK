#ifndef STREAMING_RENDER_FFMPEGVIDEORENDERERBASE_H
#define STREAMING_RENDER_FFMPEGVIDEORENDERERBASE_H

#include "IVideoFrameSink.h"

#include <QImage>
#include <QMutex>
#include <QQuickItem>

#include <atomic>

class FfmpegVideoRendererBase : public QQuickItem, public IVideoFrameSink {
    Q_OBJECT

    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY frameUpdated)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)

public:
    explicit FfmpegVideoRendererBase(QQuickItem *parent = nullptr);

    bool hasFrame() const { return m_hasFrame.load(); }
    QString placeholderText() const { return m_placeholderText; }
    void setPlaceholderText(const QString &text);

    void submitFrame(AvFramePtr frame, qint64 ptsMs) override;

    Q_INVOKABLE void clearFrame();

signals:
    void frameUpdated();
    void placeholderTextChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

    virtual QImage convertToImage(const AVFrame *frame);

private:
    void requestRepaint();

    mutable QMutex m_mutex;
    QImage m_image;
    std::atomic_bool m_dirty{false};
    std::atomic_bool m_hasFrame{false};
    QString m_placeholderText;
};

#endif // STREAMING_RENDER_FFMPEGVIDEORENDERERBASE_H
