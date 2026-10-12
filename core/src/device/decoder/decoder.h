#ifndef DECODER_H
#define DECODER_H
#include <QObject>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/hwcontext.h"
}

#include <functional>

#include "IDecoder.h"

class VideoBuffer;
struct SwsContext;
class Decoder : public IDecoder
{
    Q_OBJECT
public:
    Decoder(std::function<void(int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int linesizeY, int linesizeU, int linesizeV)> onFrame, QObject *parent = Q_NULLPTR);
    virtual ~Decoder();

    bool open() override;
    void close() override;
    bool push(const AVPacket *packet) override;
    void peekFrame(std::function<void(int width, int height, uint8_t* dataRGB32)> onFrame) override;
    void onVideoSessionChanged(const QSize &size) override;
    void setRenderExpiredFrames(bool enabled) override;

private slots:
    void onNewFrame();

signals:
    void newFrame();

private:
    void pushFrame();
    AVFrame *ensureYuv420pFrame(int width, int height, AVPixelFormat format);
    void freeConversionResources();

private:
    VideoBuffer *m_vb = Q_NULLPTR;
    AVCodecContext *m_codecCtx = Q_NULLPTR;
    AVFrame *m_hwFrame = Q_NULLPTR;
    AVFrame *m_convertFrame = Q_NULLPTR;
    SwsContext *m_convertCtx = Q_NULLPTR;
    int m_convertWidth = 0;
    int m_convertHeight = 0;
    AVPixelFormat m_convertSrcFormat = AV_PIX_FMT_NONE;
    bool m_isCodecCtxOpen = false;
    std::function<void(int, int, uint8_t*, uint8_t*, uint8_t*, int, int, int)> m_onFrame = Q_NULLPTR;
};

#endif // DECODER_H
