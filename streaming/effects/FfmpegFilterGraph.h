#ifndef STREAMING_EFFECTS_FFMPEGFILTERGRAPH_H
#define STREAMING_EFFECTS_FFMPEGFILTERGRAPH_H

#include "EffectContext.h"

#include <QStringList>

struct AVFrame;
struct AVFilterContext;
struct AVFilterGraph;

class FfmpegFilterGraph {
public:
    FfmpegFilterGraph();
    ~FfmpegFilterGraph();

    FfmpegFilterGraph(const FfmpegFilterGraph &) = delete;
    FfmpegFilterGraph &operator=(const FfmpegFilterGraph &) = delete;

    bool build(const EffectContext &ctx, const QStringList &filterSpecs);
    void destroy();

    bool process(const AVFrame *in, AVFrame *out);
    bool isBuilt() const { return m_bufferSrc != nullptr && m_bufferSink != nullptr; }

private:
    AVFilterGraph *m_graph = nullptr;
    AVFilterContext *m_bufferSrc = nullptr;
    AVFilterContext *m_bufferSink = nullptr;
};

#endif // STREAMING_EFFECTS_FFMPEGFILTERGRAPH_H
