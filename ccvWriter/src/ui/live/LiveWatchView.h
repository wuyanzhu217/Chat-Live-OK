#pragma once

#include <QWidget>
#include <memory>

class QLabel;
class QLineEdit;
class QListWidget;
class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
class QProcess;

namespace ccv {

class LiveStore;

class LiveWatchView : public QWidget {
    Q_OBJECT
public:
    explicit LiveWatchView(LiveStore* store, QWidget* parent = nullptr);
    ~LiveWatchView() override;

    void openRoom(const QString& roomId);

signals:
    void backToSquare();

private:
    void startPlayback(const QString& hlsUrl);
    void stopPlayback();
    void openExternalPlayer(const QString& hlsUrl);
    void rebuildDanmaku();

    LiveStore* m_store = nullptr;
    QString m_roomId;
    QLabel* m_title = nullptr;
    QLabel* m_meta = nullptr;
    QLabel* m_hint = nullptr;
    QVideoWidget* m_video = nullptr;
    QListWidget* m_danmakuList = nullptr;
    QLineEdit* m_danmakuInput = nullptr;
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio = nullptr;
    QProcess* m_ffplay = nullptr;
};

} // namespace ccv
