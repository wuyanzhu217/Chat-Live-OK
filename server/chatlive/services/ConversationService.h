#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <string>
#include <vector>

namespace chatlive {

using MemberIdsCallback = std::function<void(const std::vector<std::string>& memberIds)>;
using StringCallback = std::function<void(const std::string& value)>;
using VoidCallback = std::function<void()>;
using ErrorCallback = std::function<void(const std::string& message)>;

class ConversationService {
public:
    static void isMember(const drogon::orm::DbClientPtr& db,
                         const std::string& convId,
                         const std::string& userId,
                         VoidCallback onMember,
                         ErrorCallback onDenied);

    static void getMemberIds(const drogon::orm::DbClientPtr& db,
                             const std::string& convId,
                             MemberIdsCallback onSuccess,
                             ErrorCallback onError);

    static void getOtherMemberIds(const drogon::orm::DbClientPtr& db,
                                  const std::string& convId,
                                  const std::string& userId,
                                  MemberIdsCallback onSuccess,
                                  ErrorCallback onError);

    static void markReadAndPush(const drogon::orm::DbClientPtr& db,
                                const std::string& convId,
                                const std::string& userId,
                                const std::string& lastReadMsgId,
                                VoidCallback onSuccess,
                                ErrorCallback onError);

    static void findDirectConversation(const drogon::orm::DbClientPtr& db,
                                       const std::string& userId,
                                       const std::string& peerUserId,
                                       StringCallback onFound,
                                       VoidCallback onNotFound,
                                       ErrorCallback onError);
};

} // namespace chatlive
