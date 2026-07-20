#pragma once

#include <string>

namespace chatlive::ws {

enum class EventType {
    Ping,           // 心跳检测
    Pong,           // 心跳检测响应
    PresenceSync, // 同步在线用户
    MessageNew, // 新消息
    MessageRead, // 已读
    TypingStart, // 开始输入
    TypingStop, // 停止输入
    CallIncoming, // 来电
    CallHangup, // 挂断通话
    WebRtcOffer, // 发起WebRTC通话
    WebRtcAnswer, // 接听WebRTC通话
    WebRtcCandidate, // WebRTC候选者
    LiveJoin,
    LiveDanmaku,
    Error, // 错误
    Unknown // 未知事件
};

EventType stringToEventType(const std::string& event);
std::string eventTypeToString(EventType type);

} // namespace chatlive::ws