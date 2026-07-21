#pragma once

#include "domain/Types.h"

#include <QObject>
#include <QVector>

namespace ccv {

class ConversationsApi;

class ConversationsStore : public QObject {
    Q_OBJECT
public:
    explicit ConversationsStore(ConversationsApi* api, QObject* parent = nullptr);

    const QVector<Conversation>& conversations() const { return m_list; }

    void refresh();
    void updateLastMessageHint(const QString& convId, const Message& msg);

signals:
    void conversationsChanged();

private:
    ConversationsApi* m_api = nullptr;
    QVector<Conversation> m_list;
};

} // namespace ccv
