#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QSize>
#include <functional>
#include <memory>

namespace ccv {

/**
 * Encode RGB/ARGB QImage frames to H.264 Annex-B access units (Baseline, no B-frames).
 * Uses FFmpeg libavcodec + libx264 for browser-friendly WebRTC output.
 */
class H264Encoder {
public:
    H264Encoder();
    ~H264Encoder();

    /**
     * Open encoder. Width/height should be even.
     * @return false on failure
     */
    bool open(int width, int height, int fps = 30, int bitrateKbps = 1000);

    void close();
    bool isOpen() const { return m_open; }
    QSize size() const { return m_size; }

    /**
     * Encode one frame. Returns empty if not ready / failed.
     * Periodically forces IDR (every gop).
     */
    QByteArray encode(const QImage& frame, bool forceKeyframe = false);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_open = false;
    QSize m_size;
    int m_fps = 30;
};

} // namespace ccv
