#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <json/json.h>
#include <string>

namespace chatlive {

using CallJsonCallback = std::function<void(const Json::Value& call)>;
using RtcConfigCallback = std::function<void(const Json::Value& config)>;
using HangupResultCallback = std::function<void(const Json::Value& result)>;
using VoidCallback = std::function<void()>;
using ErrorCallback = std::function<void(const std::string& message)>;

class CallService {
public:
    static void pushCallState(const std::string& callId,
                              const std::string& status,
                              const std::vector<std::string>& userIds,
                              const std::string& reason = "");

    static void pushCallIncoming(const Json::Value& call, const std::string& calleeId);

    static void isUserBusy(const drogon::orm::DbClientPtr& db,
                           const std::string& userId,
                           std::function<void(bool busy)> onResult,
                           ErrorCallback onError);

    static void areFriends(const drogon::orm::DbClientPtr& db,
                           const std::string& userId,
                           const std::string& peerId,
                           std::function<void(bool)> onResult,
                           ErrorCallback onError);

    static void loadCall(const drogon::orm::DbClientPtr& db,
                         const std::string& callId,
                         CallJsonCallback onSuccess,
                         ErrorCallback onError);

    static void initiate(const drogon::orm::DbClientPtr& db,
                         const std::string& callerId,
                         const std::string& calleeId,
                         const std::string& type,
                         const std::string& conversationId,
                         CallJsonCallback onSuccess,
                         ErrorCallback onError);

    static void accept(const drogon::orm::DbClientPtr& db,
                       const std::string& callId,
                       const std::string& userId,
                       CallJsonCallback onSuccess,
                       ErrorCallback onError);

    static void reject(const drogon::orm::DbClientPtr& db,
                       const std::string& callId,
                       const std::string& userId,
                       CallJsonCallback onSuccess,
                       ErrorCallback onError);

    static void cancel(const drogon::orm::DbClientPtr& db,
                       const std::string& callId,
                       const std::string& userId,
                       CallJsonCallback onSuccess,
                       ErrorCallback onError);

    static void hangup(const drogon::orm::DbClientPtr& db,
                       const std::string& callId,
                       const std::string& userId,
                       HangupResultCallback onSuccess,
                       ErrorCallback onError);

    static void getRtcConfig(const drogon::orm::DbClientPtr& db,
                             const std::string& callId,
                             const std::string& userId,
                             RtcConfigCallback onSuccess,
                             ErrorCallback onError);

    static void validateWebRtcParticipant(const drogon::orm::DbClientPtr& db,
                                          const std::string& callId,
                                          const std::string& fromUserId,
                                          const std::string& toUserId,
                                          VoidCallback onOk,
                                          ErrorCallback onError);

    static void expireRingingCalls(const drogon::orm::DbClientPtr& db);

private:
    static void loadParticipants(const drogon::orm::DbClientPtr& db,
                                   const std::string& callId,
                                   const drogon::orm::Row& callRow,
                                   CallJsonCallback onSuccess,
                                   ErrorCallback onError);

    static void ensureConversation(const drogon::orm::DbClientPtr& db,
                                   const std::string& callerId,
                                   const std::string& calleeId,
                                   const std::string& conversationId,
                                   std::function<void(const std::string& convId)> onReady,
                                   ErrorCallback onError);

    static void createCall(const drogon::orm::DbClientPtr& db,
                           const std::string& callerId,
                           const std::string& calleeId,
                           const std::string& type,
                           const std::string& convId,
                           CallJsonCallback onSuccess,
                           ErrorCallback onError);

    static void transitionStatus(const drogon::orm::DbClientPtr& db,
                                 const std::string& callId,
                                 const std::string& userId,
                                 const std::string& expectedRole,
                                 const std::string& fromStatus,
                                 const std::string& toStatus,
                                 bool setStarted,
                                 bool setEnded,
                                 CallJsonCallback onSuccess,
                                 ErrorCallback onError);

    static void insertCallRecordMessage(const drogon::orm::DbClientPtr& db,
                                        const std::string& convId,
                                        const std::string& senderId,
                                        const std::string& type,
                                        int durationSec,
                                        std::function<void(const Json::Value& msg)> onSuccess,
                                        ErrorCallback onError);
};

} // namespace chatlive
