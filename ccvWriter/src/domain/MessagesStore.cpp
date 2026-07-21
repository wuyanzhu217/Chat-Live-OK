#include "domain/MessagesStore.h"

#include "api/MessagesApi.h"
#include "api/UploadsApi.h"

#include <QUuid>

namespace ccv {

MessagesStore::MessagesStore(MessagesApi* api, UploadsApi* uploads, QObject* parent)
    : QObject(parent)
    , m_api(api)
    , m_uploads(uploads)
{
}

QVector<Message> MessagesStore::messagesFor(const QString& convId) const
{
    return m_byConv.value(convId);
}

void MessagesStore::load(const QString& convId)
{
    m_api->list(
        convId, QString(),
        [this, convId](const QVector<Message>& items, const QString&) {
            m_byConv[convId] = items;
            emit messagesChanged(convId);
        },
        [](const ApiError&) {});
}

void MessagesStore::append(const Message& msg)
{
    if (msg.conversationId.isEmpty()) {
        return;
    }
    auto& list = m_byConv[msg.conversationId];
    for (const auto& m : list) {
        if (m.id == msg.id) {
            return;
        }
    }
    list.push_back(msg);
    emit messagesChanged(msg.conversationId);
}

void MessagesStore::sendText(const QString& convId, const QString& content)
{
    const QString clientMsgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_api->sendText(
        convId, content, clientMsgId,
        [this](const Message& msg) { append(msg); },
        [this](const ApiError& err) { emit sendFailed(err.message()); });
}

void MessagesStore::sendImageFile(const QString& convId, const QString& filePath)
{
    if (!m_uploads) {
        emit sendFailed(QStringLiteral("UploadsApi 未就绪"));
        return;
    }
    m_uploads->uploadImage(
        filePath,
        [this, convId](const UploadImageResult& up) {
            const QString clientMsgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_api->sendImage(
                convId, up.mediaUrl, up.thumbnailUrl, clientMsgId,
                [this](const Message& msg) { append(msg); },
                [this](const ApiError& err) { emit sendFailed(err.message()); });
        },
        [this](const ApiError& err) { emit sendFailed(err.message()); });
}

} // namespace ccv
