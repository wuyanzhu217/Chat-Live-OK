#include "FfmpegFilterGraph.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/error.h>
}

#include <QDebug>

namespace {

QString avErrorString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

QString buildFilterDescription(const QStringList &specs)
{
    if (specs.isEmpty())
        return {};

    QString descr;
    QString prev = QStringLiteral("[in]");
    for (int i = 0; i < specs.size(); ++i) {
        const QString next = (i + 1 < specs.size())
            ? QStringLiteral("[t%1]").arg(i)
            : QStringLiteral("[out]");
        if (i > 0)
            descr += ';';
        descr += QStringLiteral("%1 %2 %3").arg(prev, specs.at(i), next);
        prev = next;
    }
    return descr;
}

} // namespace

FfmpegFilterGraph::FfmpegFilterGraph() = default;

FfmpegFilterGraph::~FfmpegFilterGraph()
{
    destroy();
}

void FfmpegFilterGraph::destroy()
{
    m_bufferSrc = nullptr;
    m_bufferSink = nullptr;
    if (m_graph) {
        avfilter_graph_free(&m_graph);
        m_graph = nullptr;
    }
}

bool FfmpegFilterGraph::build(const EffectContext &ctx, const QStringList &filterSpecs)
{
    destroy();
    if (filterSpecs.isEmpty())
        return true;

    if (!ctx.isValid())
        return false;

    m_graph = avfilter_graph_alloc();
    if (!m_graph)
        return false;

    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1",
             ctx.width,
             ctx.height,
             static_cast<int>(ctx.pixFmt),
             ctx.timeBase.num,
             ctx.timeBase.den);

    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    if (!buffersrc || !buffersink) {
        destroy();
        return false;
    }

    if (avfilter_graph_create_filter(&m_bufferSrc, buffersrc, "in", args, nullptr, m_graph) < 0) {
        qWarning() << "FfmpegFilterGraph: create buffersrc failed";
        destroy();
        return false;
    }

    if (avfilter_graph_create_filter(&m_bufferSink, buffersink, "out", nullptr, nullptr, m_graph) < 0) {
        qWarning() << "FfmpegFilterGraph: create buffersink failed";
        destroy();
        return false;
    }

    const QString filterDescr = buildFilterDescription(filterSpecs);
    qDebug() << "FfmpegFilterGraph:" << filterDescr;

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    if (!outputs || !inputs) {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        destroy();
        return false;
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = m_bufferSrc;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = m_bufferSink;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    const int parseRet = avfilter_graph_parse_ptr(
        m_graph,
        filterDescr.toUtf8().constData(),
        &inputs,
        &outputs,
        nullptr);

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    if (parseRet < 0) {
        qWarning() << "FfmpegFilterGraph: parse failed:" << avErrorString(parseRet);
        destroy();
        return false;
    }

    if (avfilter_graph_config(m_graph, nullptr) < 0) {
        qWarning() << "FfmpegFilterGraph: config failed";
        destroy();
        return false;
    }

    return true;
}

bool FfmpegFilterGraph::process(const AVFrame *in, AVFrame *out)
{
    if (!isBuilt() || !in || !out)
        return false;

    if (av_buffersrc_add_frame_flags(m_bufferSrc, in, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
        return false;

    return av_buffersink_get_frame(m_bufferSink, out) >= 0;
}
