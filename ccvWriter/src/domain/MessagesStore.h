#pragma once

#include "domain/Types.h"

#include <QHash>
#include <QObject>
#include <QVector>

namespace ccv {

class MessagesApi;
class UploadsApi;

class MessagesStore : public QObject {
    Q_OBJECT
public:
    MessagesStore(MessagesApi* api, UploadsApi* uploads, QObject* parent = nullptr);

    QVector<Message> messagesFor(const QString& convId) const;
    void load(const QString& convId);
    void append(const Message& msg);
    void sendText(const QString& convId, const QString& content);
    void sendImageFile(const QString& convId, const QString& filePath);

signals:
    void messagesChanged(const QString& convId);
    void sendFailed(const QString& message);

private:
    MessagesApi* m_api = nullptr;
    UploadsApi* m_uploads = nullptr;
    QHash<QString, QVector<Message>> m_byConv;
};

} // namespace ccv
