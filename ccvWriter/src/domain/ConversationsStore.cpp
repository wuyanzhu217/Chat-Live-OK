#include "domain/ConversationsStore.h"

#include "api/ConversationsApi.h"

namespace ccv {

ConversationsStore::ConversationsStore(ConversationsApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
{
}

void ConversationsStore::refresh()
{
    m_api->list(
        [this](const QVector<Conversation>& items) {
            m_list = items;
            emit conversationsChanged();
        },
        [](const ApiError&) {});
}

void ConversationsStore::updateLastMessageHint(const QString& convId, const Message& msg)
{
    for (int i = 0; i < m_list.size(); ++i) {
        if (m_list[i].id == convId) {
            m_list[i].updatedAt = msg.createdAt;
            // Move to front (most recent first)
            const Conversation c = m_list.takeAt(i);
            m_list.prepend(c);
            emit conversationsChanged();
            return;
        }
    }
}

} // namespace ccv
