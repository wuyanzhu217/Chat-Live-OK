#include "util/MediaUrl.h"

#include "app/AppConfig.h"

#include <QUrl>

namespace ccv {

QString normalizeMediaUrl(const QString& url)
{
    if (url.isEmpty()) {
        return {};
    }

    if (url.startsWith(QLatin1Char('/'))) {
        return AppConfig::instance().absoluteMediaUrl(url);
    }

    QUrl parsed(url);
    if (parsed.isValid() && parsed.port() == 9000 &&
        parsed.path().startsWith(QLatin1String("/chatlive-media/"))) {
        const QString origin = AppConfig::instance().gatewayOrigin();
        if (!origin.isEmpty()) {
            QString out = origin + QStringLiteral("/media") + parsed.path();
            if (parsed.hasQuery()) {
                out += QLatin1Char('?') + parsed.query(QUrl::FullyEncoded);
            }
            return out;
        }
    }

    return url;
}

} // namespace ccv
