#pragma once

#include <functional>
#include <string>

namespace chatlive {

using MinioOkCallback = std::function<void(const std::string& mediaUrl,
                                           const std::string& thumbnailUrl)>;
using MinioErrorCallback = std::function<void(const std::string& message)>;

class MinioService {
public:
    static void loadFromEnv();

    static void uploadImage(const std::string& userId,
                            const std::string& fileName,
                            const std::string& contentType,
                            const std::string& body,
                            MinioOkCallback onSuccess,
                            MinioErrorCallback onError);

    static size_t maxImageBytes();

private:
    static bool putObject(const std::string& objectKey,
                          const std::string& contentType,
                          const std::string& body,
                          std::string& outUrl,
                          std::string& err);

    static std::string endpoint_;
    static std::string publicBase_;
    static std::string accessKey_;
    static std::string secretKey_;
    static std::string bucket_;
};

} // namespace chatlive
