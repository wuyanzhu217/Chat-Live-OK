#include "MinioService.h"

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>

#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <vector>

namespace chatlive {

std::string MinioService::endpoint_ = "http://minio:9000";
std::string MinioService::publicBase_ = "http://localhost:9000";
std::string MinioService::accessKey_ = "chatlive";
std::string MinioService::secretKey_ = "chatlive_dev_minio";
std::string MinioService::bucket_ = "chatlive-media";

namespace {

std::string sha256Hex(const std::string& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char c : hash) {
        oss << std::setw(2) << static_cast<int>(c);
    }
    return oss.str();
}

std::vector<unsigned char> hmacSha256Raw(const std::string& key, const std::string& data)
{
    unsigned int len = EVP_MAX_MD_SIZE;
    std::vector<unsigned char> out(len);
    HMAC(EVP_sha256(),
         key.data(),
         static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()),
         data.size(),
         out.data(),
         &len);
    out.resize(len);
    return out;
}

std::vector<unsigned char> hmacSha256Raw(const std::vector<unsigned char>& key, const std::string& data)
{
    unsigned int len = EVP_MAX_MD_SIZE;
    std::vector<unsigned char> out(len);
    HMAC(EVP_sha256(),
         key.data(),
         static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()),
         data.size(),
         out.data(),
         &len);
    out.resize(len);
    return out;
}

std::string hmacSha256Hex(const std::vector<unsigned char>& key, const std::string& data)
{
    const auto raw = hmacSha256Raw(key, data);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char c : raw) {
        oss << std::setw(2) << static_cast<int>(c);
    }
    return oss.str();
}

std::string utcAmzTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%dT%H%M%SZ");
    return oss.str();
}

std::string parseHost(const std::string& endpoint)
{
    std::string host = endpoint;
    if (host.rfind("http://", 0) == 0) {
        host = host.substr(7);
    } else if (host.rfind("https://", 0) == 0) {
        host = host.substr(8);
    }
    while (!host.empty() && host.back() == '/') {
        host.pop_back();
    }
    return host;
}

size_t curlWrite(char* ptr, size_t size, size_t nmemb, void*)
{
    return size * nmemb;
}

std::string extFromContentType(const std::string& contentType)
{
    if (contentType.find("png") != std::string::npos) {
        return ".png";
    }
    if (contentType.find("gif") != std::string::npos) {
        return ".gif";
    }
    if (contentType.find("webp") != std::string::npos) {
        return ".webp";
    }
    return ".jpg";
}

} // namespace

void MinioService::loadFromEnv()
{
    if (const char* v = std::getenv("MINIO_ENDPOINT"); v && *v) {
        endpoint_ = v;
    }
    if (const char* v = std::getenv("MINIO_PUBLIC_URL"); v && *v) {
        publicBase_ = v;
    }
    if (const char* v = std::getenv("MINIO_ROOT_USER"); v && *v) {
        accessKey_ = v;
    }
    if (const char* v = std::getenv("MINIO_ROOT_PASSWORD"); v && *v) {
        secretKey_ = v;
    }
    if (const char* v = std::getenv("MINIO_BUCKET"); v && *v) {
        bucket_ = v;
    }
}

size_t MinioService::maxImageBytes()
{
    return 5 * 1024 * 1024;
}

bool MinioService::putObject(const std::string& objectKey,
                             const std::string& contentType,
                             const std::string& body,
                             std::string& outUrl,
                             std::string& err)
{
    const std::string host = parseHost(endpoint_);
    const std::string amzDate = utcAmzTimestamp();
    const std::string dateStamp = amzDate.substr(0, 8);
    const std::string region = "us-east-1";
    const std::string service = "s3";
    const std::string payloadHash = sha256Hex(body);

    const std::string canonicalUri = "/" + bucket_ + "/" + objectKey;
    const std::string canonicalHeaders =
        "content-type:" + contentType + "\n"
        "host:" + host + "\n"
        "x-amz-content-sha256:" + payloadHash + "\n"
        "x-amz-date:" + amzDate + "\n";
    const std::string signedHeaders = "content-type;host;x-amz-content-sha256;x-amz-date";

    const std::string canonicalRequest =
        "PUT\n" + canonicalUri + "\n\n" + canonicalHeaders + "\n" + signedHeaders + "\n" + payloadHash;

    const std::string credentialScope = dateStamp + "/" + region + "/" + service + "/aws4_request";
    const std::string stringToSign =
        "AWS4-HMAC-SHA256\n" + amzDate + "\n" + credentialScope + "\n" + sha256Hex(canonicalRequest);

    auto kDate = hmacSha256Raw("AWS4" + secretKey_, dateStamp);
    auto kRegion = hmacSha256Raw(kDate, region);
    auto kService = hmacSha256Raw(kRegion, service);
    auto kSigning = hmacSha256Raw(kService, "aws4_request");
    const std::string signature = hmacSha256Hex(kSigning, stringToSign);

    const std::string authorization =
        "AWS4-HMAC-SHA256 Credential=" + accessKey_ + "/" + credentialScope + ", "
        "SignedHeaders=" + signedHeaders + ", Signature=" + signature;

    const std::string url = endpoint_;
    const std::string fullUrl =
        (url.back() == '/' ? url.substr(0, url.size() - 1) : url) + canonicalUri;

    CURL* curl = curl_easy_init();
    if (!curl) {
        err = "curl init failed";
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("content-type: " + contentType).c_str());
    headers = curl_slist_append(headers, ("host: " + host).c_str());
    headers = curl_slist_append(headers, ("x-amz-content-sha256: " + payloadHash).c_str());
    headers = curl_slist_append(headers, ("x-amz-date: " + amzDate).c_str());
    headers = curl_slist_append(headers, ("Authorization: " + authorization).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);

    const CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        err = std::string("curl error: ") + curl_easy_strerror(res);
        return false;
    }
    if (httpCode < 200 || httpCode >= 300) {
        err = "MinIO upload failed HTTP " + std::to_string(httpCode);
        return false;
    }

    std::string pub = publicBase_;
    if (!pub.empty() && pub.back() == '/') {
        pub.pop_back();
    }
    outUrl = pub + canonicalUri;
    return true;
}

void MinioService::uploadImage(const std::string& userId,
                               const std::string& fileName,
                               const std::string& contentType,
                               const std::string& body,
                               MinioOkCallback onSuccess,
                               MinioErrorCallback onError)
{
    if (body.size() > maxImageBytes()) {
        onError("Image too large");
        return;
    }

    if (contentType.find("image/") != 0) {
        onError("Only image uploads are supported");
        return;
    }

    std::string ext = extFromContentType(contentType);
    if (!fileName.empty() && fileName.find('.') != std::string::npos) {
        const auto dot = fileName.rfind('.');
        ext = fileName.substr(dot);
    }

    const std::string objectKey = "images/" + userId + "/" + drogon::utils::getUuid() + ext;
    std::string url;
    std::string err;
    if (!putObject(objectKey, contentType, body, url, err)) {
        LOG_ERROR << "[MinIO] upload failed: " << err;
        onError(err);
        return;
    }

    onSuccess(url, url);
}

} // namespace chatlive
