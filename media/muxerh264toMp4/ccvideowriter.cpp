#include "ccvideowriter.h"
#include <qdebug.h>

CCVideoWriter* CCVideoWriter::m_instance=nullptr;
QMutex CCVideoWriter::m_lock;


CCVideoWriter::CCVideoWriter(QObject *parent)
    : QObject{parent}{}

CCVideoWriter* CCVideoWriter::GetInstance()
{
    QMutexLocker locker(&m_lock);
    if(!m_instance){
        m_instance=new CCVideoWriter(nullptr);
    }
    return m_instance;
}


//开始推流（不写头；等 writevideoStream 解析到 SPS+PPS 后由 ensureMuxerReady 再 init）
bool CCVideoWriter::StartWritestream(const QString &VideoPath)
{
    StopWritestream();
    m_VideoPath=VideoPath;
    if(m_VideoPath.isEmpty()){
        return false;
    }
    m_recordStatus=true;
    m_headerWritten=false;
    m_isfirstFrame=false;
    m_mp4box={};
    return true;
}

bool CCVideoWriter::ensureMuxerReady()
{
    if(m_headerWritten){
        return true;
    }
    if(m_mp4box.sps_length<=0 || m_mp4box.pps_length<=0){
        return false;
    }
    return initVideoWriter();
}

void CCVideoWriter::StopWritestream()
{
    if(m_avformatcontext && m_headerWritten && m_avformatcontext->pb){
        av_write_trailer(m_avformatcontext);
    }
    if(m_avformatcontext && m_avformatcontext->pb){
        avio_closep(&m_avformatcontext->pb);
    }
    m_videostream=nullptr;
    m_audiostream=nullptr;
    if(m_avformatcontext){
        avformat_free_context(m_avformatcontext);
        m_avformatcontext=nullptr;
    }
    m_headerWritten=false;
    m_recordStatus=false;
}

void CCVideoWriter::setVideoSize(int width,int height)
{
    videoweight=width;
    videoheight=height;

}   

//初始化
bool CCVideoWriter::initVideoWriter()
{
    if(!m_avformatcontext){
        m_avformatcontext=avformat_alloc_context();
        m_avformatcontext->oformat=av_guess_format("mov",nullptr,nullptr);
    }
    if(!m_videostream){
        m_videostream=initvideostream();
    }
    if(!m_audiostream){
        m_audiostream=initaudiostream();
    }

    bool status=writeheader();

    return status;
}

AVStream *CCVideoWriter::initvideostream()
{
    AVStream* pOutputStream=avformat_new_stream(m_avformatcontext,0);
    if(!pOutputStream){
        return nullptr;
    }
    pOutputStream->id=m_avformatcontext->nb_streams-1;
    pOutputStream->time_base={1,1000};

    AVCodecParameters *codecParameters=pOutputStream->codecpar;
    if((m_mp4box.pps_length>0)&&(m_mp4box.sps_length>0)){
        int spslength=m_mp4box.sps_length;
        int ppslength=m_mp4box.pps_length;

        //AVCC box 头信息+box_length+sps_length+SPS+"0x01"+"0x00"+pps_length+PPS
        //0x01:版本号
        //0x64:AVCProfileIndication
        //0x00:AVCProfileCompatibility
        //0x28:AVCLevelIndication
        //0xFF:AVCCompatibleBRCoding
        //0xE1:AVCLevelExtIndication
        //0x00:reserved
        //填入额外数据
        unsigned char avccHeader[7]={0x01,0x64,0x00,0x28,0xFF,0xE1,0x00};
        int hlen=sizeof(avccHeader);
        unsigned char avccbox[256]={0};
        memcpy(avccbox,avccHeader,hlen);
        avccbox[hlen]=static_cast<unsigned char>(spslength);
        memcpy(avccbox+hlen+1,m_mp4box.spsBuffer,spslength);
        avccbox[spslength+hlen+1]=0x01;
        avccbox[spslength+hlen+2]=0x00;
        avccbox[spslength+hlen+3]=static_cast<unsigned char>(ppslength);
        memcpy(avccbox+spslength+hlen+4,m_mp4box.ppsBuffer,ppslength);

        int avccLen=spslength+ppslength+hlen+4;
        uint8_t *extra=static_cast<uint8_t*>(av_malloc(static_cast<size_t>(avccLen)+AV_INPUT_BUFFER_PADDING_SIZE));
        if(extra){
            memcpy(extra,avccbox,avccLen);
            codecParameters->extradata=extra;
            codecParameters->extradata_size=avccLen;
        }
    }
    
    codecParameters->codec_id=AV_CODEC_ID_H264;
    codecParameters->codec_type=AVMEDIA_TYPE_VIDEO;
    codecParameters->width=videoweight;
    codecParameters->height=videoheight;

    return pOutputStream;
}

//初始化音频流
AVStream *CCVideoWriter::initaudiostream()
{
    AVStream *pOutputStream = avformat_new_stream(m_avformatcontext, nullptr);
    if (!pOutputStream) {
        return nullptr;
    }

    pOutputStream->id = m_avformatcontext->nb_streams - 1;   //设置流ID
    pOutputStream->time_base = {1, 1000};                   //设置时间基
    pOutputStream->discard = AVDISCARD_NONE;                //设置丢弃策略

    AVCodecParameters *codecpar = pOutputStream->codecpar;
    codecpar->codec_type = AVMEDIA_TYPE_AUDIO;          //设置编码类型
    codecpar->codec_id = AV_CODEC_ID_PCM_S16LE;         //设置编码ID
    codecpar->format = AV_SAMPLE_FMT_S16;                //设置采样格式
    codecpar->sample_rate = 8000;                         //设置采样率  PCm16le 8000Hz 16bit
    av_channel_layout_from_mask(&codecpar->ch_layout, AV_CH_LAYOUT_MONO);    //设置通道布局
    codecpar->bit_rate = 8000 * 16;                         //设置比特率
    codecpar->block_align = 2;                              //设置块对齐
    codecpar->bits_per_coded_sample = 16;                   //设置每个编码样本的比特数

    return pOutputStream;
}

//写入头信息
bool CCVideoWriter::writeheader()
{
    if(m_headerWritten){
        return true;
    }
    resetStatus();

    const QByteArray path=m_VideoPath.toUtf8();
    if(avio_open(&m_avformatcontext->pb,path.constData(),AVIO_FLAG_WRITE)!=0){
        m_recordStatus=false;
        qDebug()<<"avio_open Fail";
        return false;
    }
    if(avformat_write_header(m_avformatcontext,nullptr)!=0){
        m_recordStatus=false;
        avio_closep(&m_avformatcontext->pb);
        qDebug()<<"avio_write_header Fail";
        return false;
    }
    m_headerWritten=true;
    return true;
}

//重置信息
void CCVideoWriter::resetStatus()
{
    m_fileSize=0;
    std::memset(&m_audioInfo,0,sizeof(m_audioInfo));
    std::memset(&m_videoInfo,0,sizeof(m_videoInfo));
}

bool CCVideoWriter::writeVideoFrame(H264FrameData *framedata, int type)
{
    if(!m_recordStatus || !framedata){
        return false;
    }
    if(!m_headerWritten || !m_videostream || !m_avformatcontext || !m_avformatcontext->pb){
        return false;
    }

    m_startStatus=true;

    const AVRational time_base{1,1000};

    if(m_videoInfo.m_lastTimeStamp==0){
        m_videoInfo.m_lastTimeStamp=framedata->m_TimeStamp;
    }
    //初始化AVPacket
    AVPacket packet;
    av_init_packet(&packet);

    //时间间隔和包索引赋值
    packet.duration=framedata->m_TimeStamp-m_videoInfo.m_lastTimeStamp;
    packet.stream_index=m_videostream->index;

    if(packet.duration<=0) packet.duration=1;

    m_videoInfo.m_totalTime+=packet.duration;
    m_videoInfo.m_lastTimeStamp=framedata->m_TimeStamp;

    //获取当前帧的pts
    int curpts=av_rescale_q(m_videoInfo.m_totalTime,time_base,m_videostream->time_base);
    if(m_videoInfo.m_currPts>=curpts && curpts!=0){
        return true;
    }
    m_videoInfo.m_currPts=curpts;
    m_videoInfo.m_curTime=((double)curpts*m_videostream->time_base.num)/m_videostream->time_base.den;

    packet.pts=curpts;
    packet.dts=curpts;
    packet.size=static_cast<int>(framedata->m_Size);
    packet.data=framedata->m_FrameBuffer;
    packet.flags|=(type>0)?AV_PKT_FLAG_KEY:0;

    int ret=av_interleaved_write_frame(m_avformatcontext,&packet);
    if(ret<0){
        qDebug()<<"av_write_frame Faile,codeid="<<framedata->m_CodeId;
        av_packet_unref(&packet);
        return false;
    }

    m_fileSize+=packet.size;
    av_packet_unref(&packet);
    return true;
}

bool CCVideoWriter::writeAudioFrame(AudioFrameData *framedata, int type)
{
    Q_UNUSED(type);
    if(!m_recordStatus || !m_headerWritten || !m_audiostream || !framedata){
        return false;
    }
    if(!m_avformatcontext || !m_avformatcontext->pb){
        return false;
    }
    if(framedata->m_Size<=0 || framedata->m_Size>AV_AUDIO_MAX_BUFFERSIZE){
        return false;
    }

    m_startStatus=true;

    //与 initaudiostream() 一致：PCM S16LE / 8000Hz / mono
    constexpr int kSampleRate = 8000;
    constexpr int kBytesPerFrame = 2;

    AVRational time_base{1, 1000};

    if(m_audioInfo.m_lastTimeStamp==0){
        m_audioInfo.m_lastTimeStamp=framedata->m_TimeStamp;
    }

    AVPacket packet;
    av_init_packet(&packet);

    int duration_ms = static_cast<int>((framedata->m_Size * 1000LL) / (kSampleRate * kBytesPerFrame));
    if(framedata->m_TimeStamp > m_audioInfo.m_lastTimeStamp){
        duration_ms = static_cast<int>(framedata->m_TimeStamp - m_audioInfo.m_lastTimeStamp);
    }
    if(duration_ms<=0){
        duration_ms=1;
    }

    packet.stream_index=m_audiostream->index;
    packet.duration=duration_ms;

    m_audioInfo.m_totalTime+=packet.duration;
    m_audioInfo.m_lastTimeStamp=framedata->m_TimeStamp;

    int curpts=av_rescale_q(m_audioInfo.m_totalTime,time_base,m_audiostream->time_base);
    if(m_audioInfo.m_currPts>=curpts && curpts!=0){
        return true;
    }
    m_audioInfo.m_currPts=curpts;

    m_audioInfo.m_curTime=((double)curpts*m_audiostream->time_base.num)/m_audiostream->time_base.den;

    packet.pts=curpts;
    packet.dts=curpts;
    packet.size=static_cast<int>(framedata->m_Size);
    packet.data=framedata->m_FrameBuffer;

    int ret=av_interleaved_write_frame(m_avformatcontext,&packet);
    if(ret<0){
        qDebug()<<"av_write_frame audio Fail,codeid="<<framedata->m_CodeId;
        av_packet_unref(&packet);
        return false;
    }

    m_fileSize+=packet.size;
    av_packet_unref(&packet);
    return true;
}

//读一个NALU
bool CCVideoWriter::readOneNaluFrombuffer(unsigned char *buffer,int length,unsigned int aOffset,Nalu *nalu)
{
    if(!buffer || length<=0 || aOffset<0||aOffset>=length){
        return false;
    }

    int i=aOffset;
    while(i<length){
        if(buffer[i++]==0x00 && buffer[i++]==0x00)
        {
            unsigned char c=buffer[i++];
            if((c==0x01)||((c==0x00)&&(buffer[i++]==0x01))){
                int pos=i;
                int num=4;
                while (pos<length) {
                    if(buffer[pos++]==0x00&&buffer[pos++]==0x00){
                        c=buffer[pos++];
                        if(c==0x01){
                            num=3;
                            break;
                        }
                        else if((c==0x00)&&(buffer[pos++]==0x01)){
                            num=4;
                            break;
                        }
                    }
                }
                if(pos==length){
                    nalu->size=pos-i;
                }
                else{
                    nalu->size=pos-i;
                }
                nalu->type = static_cast<NaluType>(buffer[i] & 0x1F);
                nalu->data=&buffer[i];
                m_currPos=pos-num;      //重置起始偏移值
                return true;
            }
        }
    }
    return false;
}

//写入视频流
bool CCVideoWriter::writevideoStream(unsigned char *pBuffer, int length)
{
    if(!m_recordStatus || !pBuffer||length<=0){
        return false;
    }

    Nalu naluUnit;
    memset(&naluUnit,0,sizeof(naluUnit));

    m_currPos=0;

    while(readOneNaluFrombuffer(pBuffer,length,m_currPos,&naluUnit)){
        if(naluUnit.type==NaluType::NaluType_IDR){
            qDebug()<<" IDR Frame ";
            if(!ensureMuxerReady()){
                continue;
            }
            if(!m_isfirstFrame){
                m_isfirstFrame=true;
            }
            int datalen=naluUnit.size+4;
            unsigned char* pData=(unsigned char*)malloc(datalen);

            //将nalu的大小size字段写成大端序
            // pData[0]=naluUnit.size>>24;
            // pData[1]=naluUnit.size>>16;
            // pData[2]=naluUnit.size>>8;
            // pData[3]=naluUnit.size & 0xFF;
            pData[0]=(naluUnit.size & 0xFF000000)>>24;
            pData[1]=(naluUnit.size & 0x00FF0000)>>16;
            pData[2]=(naluUnit.size & 0x0000FF00)>>8;
            pData[3]=naluUnit.size & 0xFF;
            memcpy(pData+4,naluUnit.data,naluUnit.size);

            //封装到H264FrameData结构体
            H264FrameData* h264framedata=(H264FrameData*)malloc(sizeof(H264FrameData));
            h264framedata->m_CodeId=AV_CODEC_ID_H264;
            h264framedata->m_Size=datalen;
            h264framedata->m_TimeStamp = m_videoInfo.m_totalTime + (1000 / videoFrameRate);
            memcpy(h264framedata->m_FrameBuffer,pData,datalen);

            //写I帧到ffmpeg
            writeVideoFrame(h264framedata,1);

            //释放堆资源
            free(pData);
            free(h264framedata);
        }
        else if(naluUnit.type==NaluType::NaluType_SPS){
            qDebug()<<" SPS Frame ";
            memcpy(m_mp4box.spsBuffer,naluUnit.data,naluUnit.size);
            m_mp4box.sps_length=naluUnit.size;
            ensureMuxerReady();
        }
        else if(naluUnit.type==NaluType::NaluType_PPS){
            qDebug()<<" PPS Frame ";
            memcpy(m_mp4box.ppsBuffer,naluUnit.data,naluUnit.size);
            m_mp4box.pps_length=naluUnit.size;
            ensureMuxerReady();
        }
        // else if(naluUnit.type==NaluType::NaluType_P){
        //     qDebug()<<" P Frame ";
        //     if(!m_isfirstFrame){
        //         continue;
        //     }
        // }
        // else if(naluUnit.type==NaluType::NaluType_B){
        //     qDebug()<<" B Frame ";
        //     if(!m_isfirstFrame){
        //         continue;
        //     }
        // }
        else{
            if(!m_isfirstFrame){
                continue;
            }
            if(!ensureMuxerReady()){
                continue;
            }
            int datalen=naluUnit.size+4;
            unsigned char* pData=(unsigned char*)malloc(datalen);

            //将nalu的大小size字段写成大端序
            pData[0]=naluUnit.size>>24;
            pData[1]=naluUnit.size>>16;
            pData[2]=naluUnit.size>>8;
            pData[3]=naluUnit.size & 0xFF;
            memcpy(pData+4,naluUnit.data,naluUnit.size);

            //封装到H264FrameData结构体
            H264FrameData* h264framedata=(H264FrameData*)malloc(sizeof(H264FrameData));
            h264framedata->m_CodeId=AV_CODEC_ID_H264;
            h264framedata->m_Size=datalen;
            h264framedata->m_TimeStamp = m_videoInfo.m_totalTime + (1000 / videoFrameRate);
            memcpy(h264framedata->m_FrameBuffer,pData,datalen);

            //写非关键帧到ffmpeg
            writeVideoFrame(h264framedata,0);

            //释放堆资源
            free(pData);
            free(h264framedata);
        }
    }
    return true;
}

//写入音频流（PCM S16LE 原始数据，按 AV_AUDIO_MAX_BUFFERSIZE 分帧）
bool CCVideoWriter::writeaudioStream(unsigned char *pBuffer, int length)
{
    if(!m_recordStatus || !pBuffer||length<=0){
        return false;
    }

    const int maxChunk=AV_AUDIO_MAX_BUFFERSIZE;

    int offset=0;
    while(offset<length){
        int chunkSize=length-offset;
        if(chunkSize>maxChunk){
            chunkSize=maxChunk;
        }

        AudioFrameData framedata{};
        framedata.m_CodeId=AV_CODEC_ID_PCM_S16LE;
        framedata.m_Size=chunkSize;
        framedata.m_TimeStamp=m_audioInfo.m_totalTime;
        memcpy(framedata.m_FrameBuffer,pBuffer+offset,chunkSize);

        if(!writeAudioFrame(&framedata,0)){
            return false;
        }
        offset+=chunkSize;
    }
    return true;
}

CCVideoWriter::~CCVideoWriter()
{
    StopWritestream();
}
