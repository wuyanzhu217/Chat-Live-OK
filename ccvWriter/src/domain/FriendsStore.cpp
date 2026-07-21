#include "domain/FriendsStore.h"

#include "api/FriendsApi.h"

namespace ccv {

FriendsStore::FriendsStore(FriendsApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
{
}

void FriendsStore::refresh()
{
    m_api->list(
        [this](const QVector<FriendItem>& items) {
            m_friends = items;
            emit friendsChanged();
        },
        [](const ApiError&) {
            // Keep previous list on transient errors.
        });
}

void FriendsStore::setPresence(const QString& userId, Presence p)
{
    for (auto& f : m_friends) {
        if (f.userId == userId) {
            f.presence = p;
            emit friendsChanged();
            return;
        }
    }
}

} // namespace ccv
