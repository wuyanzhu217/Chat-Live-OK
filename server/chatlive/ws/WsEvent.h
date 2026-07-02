#pragma once

#include <string>

namespace chatlive::ws {

enum class EventType {
    Ping,
    Pong,
    PresenceSync,
    MessageNew,
    TypingStart,
    TypingStop,
    CallIncoming,
    CallHangup,
    WebRtcOffer,
    WebRtcAnswer,
    WebRtcCandidate,
    Error,
    Unknown
};

EventType stringToEventType(const std::string& event);
std::string eventTypeToString(EventType type);

} // namespace chatlive::ws