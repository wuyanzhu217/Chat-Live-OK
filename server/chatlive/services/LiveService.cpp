#include "LiveService.h"
#include "UserService.h"
#include "../ws/WsHub.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <deque>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace chatlive {
namespace {

std::string g_publicBase = "https://127.0.0.1:8888";
std::string g_rtmpHost = "127.0.0.1:1935";
constexpr int kPushTokenTtlSec = 7200;
constexpr size_t kMaxDanmaku = 100;

struct PushTokenRecord {
    std::string roomId;
    std::string streamKey;
    std::string anchorId;
    std::chrono::steady_clock::time_point expiresAt;
};

struct RoomRuntime {
    std::unordered_set<std::string> viewers;
    std::deque<Json::Value> danmaku;
};

std::mutex g_mu;
std::unordered_map<std::string, PushTokenRecord> g_tokens;
std::unordered_map<std::string, std::string> g_streamToken;
std::unordered_map<std::string, RoomRuntime> g_rooms;
std::unordered_map<std::string, std::unordered_set<std::string>> g_userRooms;

std::string randomHex(size_t bytes)
{
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 255);
    std::ostringstream oss;
    for (size_t i = 0; i < bytes; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << dist(rng);
    }
    return oss.str();
}

std::string makeStreamKey() { return "sk_" + randomHex(8); }
std::string makePushToken() { return "pt_" + randomHex(16); }

std::string isoNowPlus(int sec)
{
    const auto t = std::chrono::system_clock::now() + std::chrono::seconds(sec);
    const std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string extractToken(const std::string& param)
{
    size_t pos = param.find("token=");
    if (pos == std::string::npos) {
        return {};
    }
    pos += 6;
    const size_t end = param.find('&', pos);
    return end == std::string::npos ? param.substr(pos) : param.substr(pos, end - pos);
}

void purgeExpiredTokensLocked()
{
    const auto now = std::chrono::steady_clock::now();
    for (auto it = g_tokens.begin(); it != g_tokens.end();) {
        if (it->second.expiresAt <= now) {
            g_streamToken.erase(it->second.streamKey);
            it = g_tokens.erase(it);
        } else {
            ++it;
        }
    }
}

int viewerCountLocked(const std::string& roomId)
{
    auto it = g_rooms.find(roomId);
    return it == g_rooms.end() ? 0 : static_cast<int>(it->second.viewers.size());
}

Json::Value pageOf(const Json::Value& items)
{
    Json::Value data;
    data["items"] = items;
    data["next_cursor"] = Json::Value::null;
    data["has_more"] = false;
    return data;
}

Json::Value buildPlayUrls(const std::string& streamKey)
{
    // Path-only URLs stay same-origin for both :8888 and :8443 gateways.
    Json::Value play;
    play["hls"] = "/live/" + streamKey + ".m3u8";
    play["flv"] = Json::Value::null;
    play["whep"] = "/rtc/v1/whep/?app=live&stream=" + streamKey;
    play["ll_hls"] = Json::Value::null;
    return play;
}

Json::Value buildPushUrls(const std::string& streamKey, const std::string& token)
{
    Json::Value push;
    push["whip"] = "/rtc/v1/whip/?app=live&stream=" + streamKey + "&token=" + token;
    push["rtmp"] = "rtmp://" + g_rtmpHost + "/live/" + streamKey + "?token=" + token;
    return push;
}

void broadcastViewerCount(const std::string& roomId)
{
    int count = 0;
    std::vector<std::string> users;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        count = viewerCountLocked(roomId);
        auto it = g_rooms.find(roomId);
        if (it != g_rooms.end()) {
            users.assign(it->second.viewers.begin(), it->second.viewers.end());
        }
    }
    Json::Value data;
    data["room_id"] = roomId;
    data["count"] = count;
    WsHub::instance().pushToUsers(users, "live.viewer_count", data);
}

void addViewerLocked(const std::string& roomId, const std::string& userId)
{
    g_rooms[roomId].viewers.insert(userId);
    g_userRooms[userId].insert(roomId);
}

void removeViewerLocked(const std::string& roomId, const std::string& userId)
{
    auto rit = g_rooms.find(roomId);
    if (rit != g_rooms.end()) {
        rit->second.viewers.erase(userId);
    }
    auto uit = g_userRooms.find(userId);
    if (uit != g_userRooms.end()) {
        uit->second.erase(roomId);
        if (uit->second.empty()) {
            g_userRooms.erase(uit);
        }
    }
}

void revokeTokensForRoomLocked(const std::string& roomId, const std::string& streamKey)
{
    auto st = g_streamToken.find(streamKey);
    if (st != g_streamToken.end()) {
        g_tokens.erase(st->second);
        g_streamToken.erase(st);
    }
    for (auto it = g_tokens.begin(); it != g_tokens.end();) {
        if (it->second.roomId == roomId) {
            g_streamToken.erase(it->second.streamKey);
            it = g_tokens.erase(it);
        } else {
            ++it;
        }
    }
}

const char* kRoomSelect =
    "SELECT r.id, r.anchor_id, r.title, r.cover_url, r.category, r.status::text AS status, "
    "       r.stream_key, r.started_at, r.ended_at, r.created_at, "
    "       u.nickname, u.avatar_url "
    "FROM live_rooms r JOIN users u ON u.id = r.anchor_id ";

} // namespace

void LiveService::loadFromEnv()
{
    if (const char* v = std::getenv("LIVE_PUBLIC_BASE")) {
        g_publicBase = v;
        while (!g_publicBase.empty() && g_publicBase.back() == '/') {
            g_publicBase.pop_back();
        }
    }
    if (const char* v = std::getenv("LIVE_RTMP_HOST")) {
        g_rtmpHost = v;
    }
    LOG_INFO << "[Live] public_base=" << g_publicBase << " rtmp_host=" << g_rtmpHost;
}

Json::Value LiveService::roomRowToJson(const drogon::orm::Row& row, int viewerCount)
{
    Json::Value room;
    room["id"] = row["id"].as<std::string>();
    room["anchor_id"] = row["anchor_id"].as<std::string>();
    room["title"] = row["title"].as<std::string>();
    room["cover_url"] = row["cover_url"].isNull() ? Json::Value::null : row["cover_url"].as<std::string>();
    room["category"] = row["category"].isNull() ? Json::Value::null : row["category"].as<std::string>();
    room["status"] = row["status"].as<std::string>();
    room["stream_key"] = row["stream_key"].as<std::string>();
    room["viewer_count"] = viewerCount;
    room["started_at"] = row["started_at"].isNull() ? Json::Value::null : row["started_at"].as<std::string>();
    room["ended_at"] = row["ended_at"].isNull() ? Json::Value::null : row["ended_at"].as<std::string>();
    room["created_at"] = row["created_at"].as<std::string>();

    if (!row["nickname"].isNull()) {
        Json::Value anchor;
        anchor["nickname"] = row["nickname"].as<std::string>();
        anchor["avatar_url"] =
            row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
        room["anchor"] = anchor;
    }
    return room;
}

void LiveService::listRooms(const drogon::orm::DbClientPtr& db,
                            const std::string& status,
                            const std::string& category,
                            int limit,
                            LiveJsonCallback onSuccess,
                            LiveErrorCallback onError)
{
    const int lim = std::max(1, std::min(limit, 50));
    // Inline LIMIT — Drogon/libpq binary bind of C++ int as $N often fails with
    // "insufficient data left in message" when mixed with string params.
    const std::string limitSql = " LIMIT " + std::to_string(lim);
    auto finish = [onSuccess](const drogon::orm::Result& r) {
        Json::Value items(Json::arrayValue);
        {
            std::lock_guard<std::mutex> lock(g_mu);
            for (const auto& row : r) {
                items.append(roomRowToJson(row, viewerCountLocked(row["id"].as<std::string>())));
            }
        }
        onSuccess(pageOf(items));
    };
    auto fail = [onError](const drogon::orm::DrogonDbException& e) {
        onError(std::string("DB error: ") + e.base().what());
    };

    if (status != "all" && !status.empty() && !category.empty()) {
        db->execSqlAsync(std::string(kRoomSelect) +
                             "WHERE r.status = $1::live_room_status AND r.category = $2 "
                             "ORDER BY r.started_at DESC NULLS LAST, r.created_at DESC" +
                             limitSql,
                         finish, fail, status, category);
        return;
    }
    if (status != "all" && !status.empty()) {
        db->execSqlAsync(std::string(kRoomSelect) +
                             "WHERE r.status = $1::live_room_status "
                             "ORDER BY r.started_at DESC NULLS LAST, r.created_at DESC" +
                             limitSql,
                         finish, fail, status);
        return;
    }
    if (!category.empty()) {
        db->execSqlAsync(std::string(kRoomSelect) +
                             "WHERE r.category = $1 "
                             "ORDER BY r.started_at DESC NULLS LAST, r.created_at DESC" +
                             limitSql,
                         finish, fail, category);
        return;
    }
    db->execSqlAsync(std::string(kRoomSelect) +
                         "ORDER BY r.started_at DESC NULLS LAST, r.created_at DESC" + limitSql,
                     finish, fail);
}

void LiveService::createRoom(const drogon::orm::DbClientPtr& db,
                             const std::string& anchorId,
                             const std::string& title,
                             const std::string& coverUrl,
                             const std::string& category,
                             LiveJsonCallback onSuccess,
                             LiveErrorCallback onError)
{
    if (title.empty() || title.size() > 128) {
        onError("Invalid title");
        return;
    }

    // One active (non-ended) room per anchor — reuse instead of hard-failing
    db->execSqlAsync(
        "SELECT id FROM live_rooms WHERE anchor_id = $1 AND status <> 'ended' "
        "ORDER BY created_at DESC LIMIT 1",
        [db, anchorId, title, coverUrl, category, onSuccess, onError](const drogon::orm::Result& existing) {
            if (existing.size() > 0) {
                const std::string roomId = existing[0]["id"].as<std::string>();
                // Refresh title if provided
                db->execSqlAsync(
                    "UPDATE live_rooms SET title = $1 WHERE id = $2 AND status <> 'ended'",
                    [db, roomId, onSuccess, onError](const drogon::orm::Result&) {
                        getRoom(db, roomId, onSuccess, onError);
                    },
                    [db, roomId, onSuccess, onError](const drogon::orm::DrogonDbException&) {
                        getRoom(db, roomId, onSuccess, onError);
                    },
                    title, roomId);
                return;
            }
            const std::string streamKey = makeStreamKey();
            db->execSqlAsync(
                "INSERT INTO live_rooms (anchor_id, title, cover_url, category, stream_key, chat_channel_id) "
                "VALUES ($1, $2, NULLIF($3, ''), NULLIF($4, ''), $5, gen_random_uuid()) "
                "RETURNING id",
                [db, onSuccess, onError](const drogon::orm::Result& r) {
                    if (r.size() == 0) {
                        onError("Failed to create room");
                        return;
                    }
                    const std::string roomId = r[0]["id"].as<std::string>();
                    getRoom(db, roomId, onSuccess, onError);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                anchorId, title, coverUrl, category, streamKey);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        anchorId);
}

void LiveService::getRoom(const drogon::orm::DbClientPtr& db,
                          const std::string& roomId,
                          LiveJsonCallback onSuccess,
                          LiveErrorCallback onError)
{
    // Accept room UUID or stream_key (sk_...)
    const bool byStreamKey = roomId.rfind("sk_", 0) == 0;
    const std::string sql = std::string(kRoomSelect) +
                            (byStreamKey ? "WHERE r.stream_key = $1" : "WHERE r.id = $1");

    db->execSqlAsync(
        sql,
        [onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Live room not found");
                return;
            }
            const std::string id = r[0]["id"].as<std::string>();
            int viewers = 0;
            {
                std::lock_guard<std::mutex> lock(g_mu);
                viewers = viewerCountLocked(id);
            }
            onSuccess(roomRowToJson(r[0], viewers));
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        roomId);
}

void LiveService::updateRoom(const drogon::orm::DbClientPtr& db,
                             const std::string& roomId,
                             const std::string& userId,
                             const std::string& title,
                             bool hasTitle,
                             const std::string& coverUrl,
                             bool hasCover,
                             LiveJsonCallback onSuccess,
                             LiveErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT anchor_id, status::text AS status FROM live_rooms WHERE id = $1",
        [db, roomId, userId, title, hasTitle, coverUrl, hasCover, onSuccess, onError](
            const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Live room not found");
                return;
            }
            if (r[0]["anchor_id"].as<std::string>() != userId) {
                onError("Not the room anchor");
                return;
            }
            const std::string st = r[0]["status"].as<std::string>();
            if (st == "ended") {
                onError("Invalid room status");
                return;
            }

            if (hasTitle && hasCover) {
                db->execSqlAsync(
                    "UPDATE live_rooms SET title = $1, cover_url = NULLIF($2, '') WHERE id = $3",
                    [db, roomId, onSuccess, onError](const drogon::orm::Result&) {
                        getRoom(db, roomId, onSuccess, onError);
                    },
                    [onError](const drogon::orm::DrogonDbException& e) {
                        onError(std::string("DB error: ") + e.base().what());
                    },
                    title, coverUrl, roomId);
            } else if (hasTitle) {
                db->execSqlAsync(
                    "UPDATE live_rooms SET title = $1 WHERE id = $2",
                    [db, roomId, onSuccess, onError](const drogon::orm::Result&) {
                        getRoom(db, roomId, onSuccess, onError);
                    },
                    [onError](const drogon::orm::DrogonDbException& e) {
                        onError(std::string("DB error: ") + e.base().what());
                    },
                    title, roomId);
            } else if (hasCover) {
                db->execSqlAsync(
                    "UPDATE live_rooms SET cover_url = NULLIF($1, '') WHERE id = $2",
                    [db, roomId, onSuccess, onError](const drogon::orm::Result&) {
                        getRoom(db, roomId, onSuccess, onError);
                    },
                    [onError](const drogon::orm::DrogonDbException& e) {
                        onError(std::string("DB error: ") + e.base().what());
                    },
                    coverUrl, roomId);
            } else {
                getRoom(db, roomId, onSuccess, onError);
            }
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        roomId);
}

void LiveService::startRoom(const drogon::orm::DbClientPtr& db,
                            const std::string& roomId,
                            const std::string& userId,
                            LiveJsonCallback onSuccess,
                            LiveErrorCallback onError)
{
    db->execSqlAsync(
        std::string(kRoomSelect) + "WHERE r.id = $1",
        [db, roomId, userId, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Live room not found");
                return;
            }
            if (r[0]["anchor_id"].as<std::string>() != userId) {
                onError("Not the room anchor");
                return;
            }
            const std::string st = r[0]["status"].as<std::string>();
            if (st == "ended") {
                onError("Invalid room status");
                return;
            }

            const std::string streamKey = r[0]["stream_key"].as<std::string>();
            const std::string token = makePushToken();

            auto finish = [roomId, streamKey, token, onSuccess](const drogon::orm::Row& row) {
                int viewers = 0;
                std::vector<std::string> viewersList;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    purgeExpiredTokensLocked();
                    revokeTokensForRoomLocked(roomId, streamKey);
                    PushTokenRecord rec;
                    rec.roomId = roomId;
                    rec.streamKey = streamKey;
                    rec.anchorId = row["anchor_id"].as<std::string>();
                    rec.expiresAt = std::chrono::steady_clock::now() +
                                    std::chrono::seconds(kPushTokenTtlSec);
                    g_tokens[token] = rec;
                    g_streamToken[streamKey] = token;
                    viewers = viewerCountLocked(roomId);
                    auto it = g_rooms.find(roomId);
                    if (it != g_rooms.end()) {
                        viewersList.assign(it->second.viewers.begin(), it->second.viewers.end());
                    }
                }

                Json::Value room = roomRowToJson(row, viewers);
                room["status"] = "live";

                Json::Value data;
                data["room"] = room;
                data["push_urls"] = buildPushUrls(streamKey, token);
                data["play_urls"] = buildPlayUrls(streamKey);
                Json::Value dist;
                dist["edge"] = "default";
                dist["regions"] = Json::Value(Json::arrayValue);
                dist["regions"].append("cn-default");
                data["distribution"] = dist;
                data["chat_channel_id"] = roomId;
                data["push_token_expires_at"] = isoNowPlus(kPushTokenTtlSec);

                Json::Value started;
                started["room"] = room;
                WsHub::instance().pushToUsers(viewersList, "live.started", started);

                onSuccess(data);
            };

            if (st == "live") {
                // Re-issue token for already-live room
                finish(r[0]);
                return;
            }

            db->execSqlAsync(
                "UPDATE live_rooms SET status = 'live', started_at = NOW(), ended_at = NULL "
                "WHERE id = $1",
                [db, roomId, finish, onError](const drogon::orm::Result&) {
                    db->execSqlAsync(
                        std::string(kRoomSelect) + "WHERE r.id = $1",
                        [finish, onError](const drogon::orm::Result& r2) {
                            if (r2.size() == 0) {
                                onError("Live room not found");
                                return;
                            }
                            finish(r2[0]);
                        },
                        [onError](const drogon::orm::DrogonDbException& e) {
                            onError(std::string("DB error: ") + e.base().what());
                        },
                        roomId);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                roomId);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        roomId);
}

void LiveService::stopRoom(const drogon::orm::DbClientPtr& db,
                           const std::string& roomId,
                           const std::string& userId,
                           LiveJsonCallback onSuccess,
                           LiveErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT anchor_id, stream_key, status::text AS status FROM live_rooms WHERE id = $1",
        [db, roomId, userId, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Live room not found");
                return;
            }
            if (r[0]["anchor_id"].as<std::string>() != userId) {
                onError("Not the room anchor");
                return;
            }
            const std::string streamKey = r[0]["stream_key"].as<std::string>();

            db->execSqlAsync(
                "UPDATE live_rooms SET status = 'ended', ended_at = NOW() WHERE id = $1",
                [db, roomId, streamKey, onSuccess, onError](const drogon::orm::Result&) {
                    std::vector<std::string> users;
                    {
                        std::lock_guard<std::mutex> lock(g_mu);
                        revokeTokensForRoomLocked(roomId, streamKey);
                        auto it = g_rooms.find(roomId);
                        if (it != g_rooms.end()) {
                            users.assign(it->second.viewers.begin(), it->second.viewers.end());
                            g_rooms.erase(it);
                        }
                    }
                    Json::Value ended;
                    ended["room_id"] = roomId;
                    WsHub::instance().pushToUsers(users, "live.ended", ended);

                    getRoom(db, roomId, onSuccess, onError);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                roomId);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        roomId);
}

void LiveService::joinRoom(const drogon::orm::DbClientPtr& db,
                           const std::string& roomId,
                           const std::string& userId,
                           LiveJsonCallback onSuccess,
                           LiveErrorCallback onError)
{
    const bool byStreamKey = roomId.rfind("sk_", 0) == 0;
    const std::string sql = std::string(kRoomSelect) +
                            (byStreamKey ? "WHERE r.stream_key = $1" : "WHERE r.id = $1");

    db->execSqlAsync(
        sql,
        [userId, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Live room not found");
                return;
            }
            const std::string st = r[0]["status"].as<std::string>();
            if (st != "live") {
                onError("Invalid room status");
                return;
            }
            const std::string id = r[0]["id"].as<std::string>();
            const std::string streamKey = r[0]["stream_key"].as<std::string>();

            Json::Value recent(Json::arrayValue);
            int viewers = 0;
            {
                std::lock_guard<std::mutex> lock(g_mu);
                addViewerLocked(id, userId);
                viewers = viewerCountLocked(id);
                auto it = g_rooms.find(id);
                if (it != g_rooms.end()) {
                    for (const auto& d : it->second.danmaku) {
                        recent.append(d);
                    }
                }
            }
            broadcastViewerCount(id);

            Json::Value data;
            data["room"] = roomRowToJson(r[0], viewers);
            data["play_urls"] = buildPlayUrls(streamKey);
            data["chat_channel_id"] = id;
            data["recent_danmaku"] = recent;
            onSuccess(data);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        roomId);
}

void LiveService::listDanmaku(const std::string& roomId,
                              int limit,
                              LiveJsonCallback onSuccess,
                              LiveErrorCallback onError)
{
    (void)onError;
    const int lim = std::max(1, std::min(limit, 100));
    Json::Value items(Json::arrayValue);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto it = g_rooms.find(roomId);
        if (it != g_rooms.end()) {
            const auto& dq = it->second.danmaku;
            const size_t start = dq.size() > static_cast<size_t>(lim) ? dq.size() - lim : 0;
            for (size_t i = start; i < dq.size(); ++i) {
                items.append(dq[i]);
            }
        }
    }
    onSuccess(pageOf(items));
}

void LiveService::onPublish(const drogon::orm::DbClientPtr& db,
                            const std::string& stream,
                            const std::string& param,
                            LiveBoolCallback onDone)
{
    const std::string token = extractToken(param);
    if (stream.empty() || token.empty()) {
        onDone(false);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_mu);
        purgeExpiredTokensLocked();
        auto it = g_tokens.find(token);
        if (it == g_tokens.end() || it->second.streamKey != stream ||
            it->second.expiresAt <= std::chrono::steady_clock::now()) {
            onDone(false);
            return;
        }
    }

    db->execSqlAsync(
        "SELECT id, status::text AS status FROM live_rooms WHERE stream_key = $1",
        [onDone, stream](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onDone(false);
                return;
            }
            const std::string st = r[0]["status"].as<std::string>();
            onDone(st == "live" || st == "idle");
            LOG_INFO << "[Live] on_publish stream=" << stream << " allow=" << (st == "live" || st == "idle");
        },
        [onDone](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[Live] on_publish db error: " << e.base().what();
            onDone(false);
        },
        stream);
}

void LiveService::onUnpublish(const drogon::orm::DbClientPtr& db,
                              const std::string& stream,
                              LiveJsonCallback onDone)
{
    (void)db;
    LOG_INFO << "[Live] on_unpublish stream=" << stream;
    Json::Value data;
    data["stream"] = stream;
    data["ok"] = true;
    onDone(data);
}

void LiveService::wsJoin(const std::string& roomId, const std::string& userId)
{
    {
        std::lock_guard<std::mutex> lock(g_mu);
        addViewerLocked(roomId, userId);
    }
    broadcastViewerCount(roomId);
}

void LiveService::wsDanmaku(const drogon::orm::DbClientPtr& db,
                            const std::string& roomId,
                            const std::string& userId,
                            const std::string& content,
                            LiveJsonCallback onSuccess,
                            LiveErrorCallback onError)
{
    std::string trimmed = content;
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\n')) {
        trimmed.pop_back();
    }
    if (trimmed.empty() || trimmed.size() > 200) {
        onError("Invalid danmaku");
        return;
    }

    db->execSqlAsync(
        "SELECT status::text AS status FROM live_rooms WHERE id = $1",
        [db, roomId, userId, trimmed, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Live room not found");
                return;
            }
            if (r[0]["status"].as<std::string>() != "live") {
                onError("Invalid room status");
                return;
            }

            UserService::getUserProfile(
                db, userId,
                [roomId, userId, trimmed, onSuccess](const Json::Value& profile) {
                    Json::Value item;
                    item["user_id"] = userId;
                    item["nickname"] = profile.get("nickname", "观众").asString();
                    item["content"] = trimmed;
                    item["created_at"] = isoNowPlus(0);
                    item["room_id"] = roomId;

                    std::vector<std::string> users;
                    {
                        std::lock_guard<std::mutex> lock(g_mu);
                        addViewerLocked(roomId, userId);
                        auto& rt = g_rooms[roomId];
                        rt.danmaku.push_back(item);
                        while (rt.danmaku.size() > kMaxDanmaku) {
                            rt.danmaku.pop_front();
                        }
                        users.assign(rt.viewers.begin(), rt.viewers.end());
                    }
                    WsHub::instance().pushToUsers(users, "live.danmaku", item);
                    onSuccess(item);
                },
                onError);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        roomId);
}

void LiveService::onUserDisconnect(const std::string& userId)
{
    std::vector<std::string> rooms;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto it = g_userRooms.find(userId);
        if (it == g_userRooms.end()) {
            return;
        }
        rooms.assign(it->second.begin(), it->second.end());
        for (const auto& roomId : rooms) {
            removeViewerLocked(roomId, userId);
        }
    }
    for (const auto& roomId : rooms) {
        broadcastViewerCount(roomId);
    }
}

} // namespace chatlive
