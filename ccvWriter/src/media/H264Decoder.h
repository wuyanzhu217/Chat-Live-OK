#pragma once

#include <QByteArray>
#include <QImage>
#include <memory>

namespace ccv {

/**
 * Decode H.264 Annex-B access units to QImage (RGB32).
 */
class H264Decoder {
public:
    H264Decoder();
    ~H264Decoder();

    bool open();
    void close();
    bool isOpen() const { return m_open; }

    /** Decode one access unit; returns null image if no picture yet. */
    QImage decode(const QByteArray& annexB);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_open = false;
};

} // namespace ccv
