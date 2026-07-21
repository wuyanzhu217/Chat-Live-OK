#pragma once

#include <QException>
#include <QString>

namespace ccv {

/**
 * Unified API error — mirrors web ApiError (code + message + httpStatus).
 * chatlive responses: { "code": N, "message": "...", "data": ... }
 */
class ApiError {
public:
    ApiError() = default;
    ApiError(int code, QString message, int httpStatus = 0)
        : m_code(code)
        , m_message(std::move(message))
        , m_httpStatus(httpStatus)
    {
    }

    int code() const { return m_code; }
    QString message() const { return m_message; }
    int httpStatus() const { return m_httpStatus; }

    bool isBusy() const { return m_code == 4002; }

private:
    int m_code = 0;
    QString m_message;
    int m_httpStatus = 0;
};

} // namespace ccv
