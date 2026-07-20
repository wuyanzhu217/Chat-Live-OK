#include "WsEvent.h"

namespace chatlive::ws {

EventType stringToEventType(const std::string& event) {
    if (event == "ping") return EventType::Ping;
    if (event == "pong") return EventType::Pong;
    if (event == "presence.sync") return EventType::PresenceSync;
    if (event == "message.new") return EventType::MessageNew;
    if (event == "message.read") return EventType::MessageRead;
    if (event == "typing.start") return EventType::TypingStart;
    if (event == "typing.stop") return EventType::TypingStop;
    if (event == "call.incoming") return EventType::CallIncoming;
    if (event == "call.hangup") return EventType::CallHangup;
    if (event == "webrtc.offer") return EventType::WebRtcOffer;
    if (event == "webrtc.answer") return EventType::WebRtcAnswer;
    if (event == "webrtc.candidate") return EventType::WebRtcCandidate;
    if (event == "live.join") return EventType::LiveJoin;
    if (event == "live.danmaku") return EventType::LiveDanmaku;
    if (event == "error") return EventType::Error;
    return EventType::Unknown;
}

std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::Ping: return "ping";
        case EventType::Pong: return "pong";
        case EventType::PresenceSync: return "presence.sync";
        case EventType::MessageNew: return "message.new";
        case EventType::MessageRead: return "message.read";
        case EventType::TypingStart: return "typing.start";
        case EventType::TypingStop: return "typing.stop";
        case EventType::CallIncoming: return "call.incoming";
        case EventType::CallHangup: return "call.hangup";
        case EventType::WebRtcOffer: return "webrtc.offer";
        case EventType::WebRtcAnswer: return "webrtc.answer";
        case EventType::WebRtcCandidate: return "webrtc.candidate";
        case EventType::LiveJoin: return "live.join";
        case EventType::LiveDanmaku: return "live.danmaku";
        case EventType::Error: return "error";
        default: return "unknown";
    }
}

} // namespace chatlive::ws