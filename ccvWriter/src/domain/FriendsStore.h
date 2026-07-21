#pragma once

#include "domain/Types.h"

#include <QObject>
#include <QVector>

namespace ccv {

class FriendsApi;

class FriendsStore : public QObject {
    Q_OBJECT
public:
    explicit FriendsStore(FriendsApi* api, QObject* parent = nullptr);

    const QVector<FriendItem>& friends() const { return m_friends; }

    void refresh();
    void setPresence(const QString& userId, Presence p);

signals:
    void friendsChanged();

private:
    FriendsApi* m_api = nullptr;
    QVector<FriendItem> m_friends;
};

} // namespace ccv
