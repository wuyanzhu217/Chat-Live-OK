#ifndef CCVIDEOWRITER_H
#define CCVIDEOWRITER_H
/*
 *单例模式，使用懒汉模式，利用互斥锁保证线程安全
*/

#include <QObject>
#include<QMutex>

extern "C"{
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libavutil/avutil.h>
#include<libavdevice/avdevice.h>
}

#define AV_AUDIO_MAX_BUFFERSIZE  1280
#define AV_VIDEO_MAX_BUFFERSIZE  64*1024


//mp4容器
typedef struct Mp4avcc_Box{
    int sps_length;
    int pps_length;
    unsigned char spsBuffer[128];
    unsigned char ppsBuffer[64];
}mp4Box;

//H264帧数据
typedef struct{
    int         m_CodeId;
    uint64_t    m_TimeStamp;
    uint64_t    m_Size;
    unsigned char m_FrameBuffer[AV_VIDEO_MAX_BUFFERSIZE];
}H264FrameData;

//音频帧数据
typedef struct{
    int         m_CodeId;
    uint64_t    m_TimeStamp;
    uint64_t    m_Size;
    unsigned char m_FrameBuffer[AV_AUDIO_MAX_BUFFERSIZE];
}AudioFrameData;

//NALU类型
enum NaluType{
    NaluType_SCLICE=1,
    NaluType_P=2,
    NaluType_B=3,
    NaluTypeINT_IDR=4,
    NaluType_IDR=5,
    NaluType_SEI=6,
    NaluType_SPS=7,
    NaluType_PPS=8,
};


//H.264 NALU结构体
typedef struct Nalu_t{
    unsigned char *data;
    int size;
    NaluType type;
}Nalu;


//录像信息
typedef struct RecordInfo{
    int     m_currPts;
    double  m_curTime;
    uint    m_totalTime;
    uint    m_lastTimeStamp;
}RecordInfo;


//单例模式，使用懒汉模式，利用互斥锁保证线程安全
//单独使用，使用必须先获取到锁并保持一段使用权
class CCVideoWriter : public QObject
{
    Q_OBJECT
public:
    static CCVideoWriter* GetInstance();

    //使用顺序：StartWritestream->setVideoSize->writevideoStream->writeaudioStream->StopWritestream
    bool StartWritestream(const QString& VideoPath);
    void StopWritestream();
    
    void setVideoSize(int width,int height);
    bool writevideoStream(unsigned char*pBuffer,int length);
    bool writeaudioStream(unsigned char*pBuffer,int length);

private:
    bool ensureMuxerReady();  //确保muxer准备好
    bool initVideoWriter();
    AVStream* initvideostream();
    AVStream* initaudiostream();
    bool writeheader();
    void resetStatus();
    //写视频帧
    bool writeVideoFrame(H264FrameData* framedata,int type);
    //写音频帧
    bool writeAudioFrame(AudioFrameData* framedata, int type);

private:
    explicit CCVideoWriter(QObject *parent = nullptr);
    ~CCVideoWriter();

private:
    int videoweight;
    int videoheight;
    int videoFrameRate;

    QString m_VideoPath;
    AVStream *m_videostream=nullptr;
    AVStream *m_audiostream=nullptr;
    AVFormatContext *m_avformatcontext=nullptr;

    //从H.264读取一个NALU算法
    bool readOneNaluFrombuffer(unsigned char *buffer,int length,unsigned int aOffset,Nalu *nalu);

private:
    static CCVideoWriter *m_instance;
    static QMutex m_lock;

    RecordInfo m_audioInfo;
    RecordInfo m_videoInfo;

    mp4Box  m_mp4box;
    int     m_fileSize=0;
    int     m_currPos=0;
    bool    m_recordStatus=false;
    bool    m_startStatus=false;
    bool    m_isfirstFrame=false;
    bool    m_headerWritten=false;//头信息是否已经写入
};

#endif // CCVIDEOWRITER_H
