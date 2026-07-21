#pragma once

#include <QString>

namespace ccv {

/** Rewrite MinIO :9000 / path-only URLs to gateway /media (same idea as web mediaUrl.ts). */
QString normalizeMediaUrl(const QString& url);

} // namespace ccv
