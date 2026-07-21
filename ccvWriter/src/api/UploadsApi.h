#pragma once

#include "api/ApiClient.h"
#include "domain/Types.h"

#include <QObject>
#include <functional>

namespace ccv {

struct UploadImageResult {
    QString mediaUrl;
    QString thumbnailUrl;
};

struct UploadAvatarResult {
    QString avatarUrl;
    User user;
};

class UploadsApi : public QObject {
    Q_OBJECT
public:
    explicit UploadsApi(ApiClient* client, QObject* parent = nullptr);

    void uploadImage(const QString& filePath,
                     std::function<void(const UploadImageResult&)> onOk,
                     ApiClient::ErrorCb onErr);

    void uploadAvatar(const QString& filePath,
                      std::function<void(const UploadAvatarResult&)> onOk,
                      ApiClient::ErrorCb onErr);

private:
    ApiClient* m_client = nullptr;
};

} // namespace ccv
