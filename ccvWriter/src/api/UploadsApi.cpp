#include "api/UploadsApi.h"

#include "api/UsersApi.h"
#include "util/MediaUrl.h"

#include <QJsonObject>

namespace ccv {

UploadsApi::UploadsApi(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

void UploadsApi::uploadImage(const QString& filePath,
                             std::function<void(const UploadImageResult&)> onOk,
                             ApiClient::ErrorCb onErr)
{
    m_client->uploadForm(
        QStringLiteral("/uploads/images"), filePath,
        [onOk](const QJsonValue& data) {
            const QJsonObject o = data.toObject();
            UploadImageResult r;
            r.mediaUrl = normalizeMediaUrl(o.value(QStringLiteral("media_url")).toString());
            r.thumbnailUrl = normalizeMediaUrl(o.value(QStringLiteral("thumbnail_url")).toString());
            onOk(r);
        },
        std::move(onErr));
}

void UploadsApi::uploadAvatar(const QString& filePath,
                              std::function<void(const UploadAvatarResult&)> onOk,
                              ApiClient::ErrorCb onErr)
{
    m_client->uploadForm(
        QStringLiteral("/users/me/avatar"), filePath,
        [onOk](const QJsonValue& data) {
            const QJsonObject o = data.toObject();
            UploadAvatarResult r;
            r.avatarUrl = normalizeMediaUrl(o.value(QStringLiteral("avatar_url")).toString());
            r.user = UsersApi::parseUser(o.value(QStringLiteral("user")).toObject());
            if (r.user.avatarUrl.isEmpty()) {
                r.user.avatarUrl = r.avatarUrl;
            }
            onOk(r);
        },
        std::move(onErr));
}

} // namespace ccv
