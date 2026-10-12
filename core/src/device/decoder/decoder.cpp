#include <QDebug>

#include "compat.h"
#include "decoder.h"
#include "videobuffer.h"

extern "C"
{
#include "libswscale/swscale.h"
}

Decoder::Decoder(std::function<void(int, int, uint8_t*, uint8_t*, uint8_t*, int, int, int)> onFrame, QObject *parent)
    : IDecoder(parent)
    , m_vb(new VideoBuffer())
    , m_onFrame(onFrame)
{
    m_vb->init();
    connect(this, &Decoder::newFrame, this, &Decoder::onNewFrame, Qt::QueuedConnection);
    connect(m_vb, &VideoBuffer::updateFPS, this, &Decoder::updateFPS);
}

Decoder::~Decoder() {
    freeConversionResources();
    m_vb->deInit();
    delete m_vb;
}

bool Decoder::open()
{
    // codec
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        qCritical("H.264 decoder not found");
        return false;
    }

    // codec context
    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        qCritical("Could not allocate decoder context");
        return false;
    }
    if (!qEnvironmentVariableIsSet("NOON_SCRCPY_SOFTWARE_DECODE")) {
        AVBufferRef *hwDevice = Q_NULLPTR;
        if (av_hwdevice_ctx_create(&hwDevice, AV_HWDEVICE_TYPE_VAAPI, Q_NULLPTR, Q_NULLPTR, 0) == 0 && hwDevice) {
            m_codecCtx->hw_device_ctx = av_buffer_ref(hwDevice);
            av_buffer_unref(&hwDevice);
            qInfo("Decoder: VA-API hardware decode enabled");
        } else {
            qWarning("Decoder: VA-API unavailable, falling back to software decode");
        }
    }

    if (avcodec_open2(m_codecCtx, codec, NULL) < 0) {
        if (m_codecCtx->hw_device_ctx) {
            qWarning("Decoder: VA-API init failed, retrying software decode");
            avcodec_free_context(&m_codecCtx);
            m_codecCtx = avcodec_alloc_context3(codec);
        }
        if (!m_codecCtx || avcodec_open2(m_codecCtx, codec, NULL) < 0) {
            if (m_codecCtx) {
                avcodec_free_context(&m_codecCtx);
                m_codecCtx = Q_NULLPTR;
            }
            qCritical("Could not open H.264 codec");
            return false;
        }
        qWarning("Decoder: VA-API could not engage, running software decode");
    }
    m_isCodecCtxOpen = true;
    return true;
}

void Decoder::close()
{
    if (m_vb) {
        m_vb->interrupt();
    }

    freeConversionResources();

    if (!m_codecCtx) {
        return;
    }
    m_isCodecCtxOpen = false;
    avcodec_free_context(&m_codecCtx);
}

void Decoder::onVideoSessionChanged(const QSize &size)
{
    Q_UNUSED(size);
    if (m_codecCtx) {
        avcodec_flush_buffers(m_codecCtx);
    }
}

void Decoder::setRenderExpiredFrames(bool enabled)
{
    if (m_vb) {
        m_vb->setRenderExpiredFrames(enabled);
    }
}

bool Decoder::push(const AVPacket *packet)
{
    if (!m_codecCtx || !m_vb) {
        return false;
    }
    AVFrame *decodingFrame = m_vb->decodingFrame();
#ifdef QTSCRCPY_LAVF_HAS_NEW_ENCODING_DECODING_API
    int ret = -1;
    if ((ret = avcodec_send_packet(m_codecCtx, packet)) < 0) {
        char errorbuf[255] = { 0 };
        av_strerror(ret, errorbuf, 254);
        qCritical("Could not send video packet: %s", errorbuf);
        return false;
    }
    if (decodingFrame) {
        ret = avcodec_receive_frame(m_codecCtx, decodingFrame);
    }
    if (!ret) {
        // a frame was received
        pushFrame();

        //emit getOneFrame(yuvDecoderFrame->data[0], yuvDecoderFrame->data[1], yuvDecoderFrame->data[2],
        //        yuvDecoderFrame->linesize[0], yuvDecoderFrame->linesize[1], yuvDecoderFrame->linesize[2]);

        /*
        // m_conver转换yuv为rgb是使用cpu转的，占用cpu太高，改用opengl渲染yuv
        // QImage的copy也非常占用内存，此方案不考虑
        if (!m_conver.isInit()) {
            qDebug() << "decoder frame format" << decodingFrame->format;
            m_conver.setSrcFrameInfo(codecCtx->width, codecCtx->height, AV_PIX_FMT_YUV420P);
            m_conver.setDstFrameInfo(codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB32);
            m_conver.init();
        }
        if (!outBuffer) {
            outBuffer=new quint8[avpicture_get_size(AV_PIX_FMT_RGB32, codecCtx->width, codecCtx->height)];
            avpicture_fill((AVPicture *)rgbDecoderFrame, outBuffer, AV_PIX_FMT_RGB32, codecCtx->width, codecCtx->height);
        }
        m_conver.convert(decodingFrame, rgbDecoderFrame);
        //QImage tmpImg((uchar *)outBuffer, codecCtx->width, codecCtx->height, QImage::Format_RGB32);
        //QImage image = tmpImg.copy();
        //emit getOneImage(image);
        */
    } else if (ret != AVERROR(EAGAIN)) {
        qCritical("Could not receive video frame: %d", ret);
        return false;
    }
#else
    int gotPicture = 0;
    int len = -1;
    if (decodingFrame) {
        len = avcodec_decode_video2(m_codecCtx, decodingFrame, &gotPicture, packet);
    }
    if (len < 0) {
        qCritical("Could not decode video packet: %d", len);
        return false;
    }
    if (gotPicture) {
        pushFrame();
    }
#endif
    return true;
}

void Decoder::peekFrame(std::function<void (int, int, uint8_t *)> onFrame)
{
    if (!m_vb) {
        return;
    }
    m_vb->peekRenderedFrame(onFrame);
}

void Decoder::pushFrame()
{
    if (!m_vb) {
        return;
    }
    bool previousFrameSkipped = true;
    m_vb->offerDecodedFrame(previousFrameSkipped);
    if (previousFrameSkipped) {
        // the previous newFrame will consume this frame
        return;
    }
    emit newFrame();
}

void Decoder::onNewFrame() {
    if (!m_onFrame) {
        return;
    }

    m_vb->lock();
    const AVFrame *frame = m_vb->consumeRenderedFrame();
    const AVFrame *src = frame;
    if (frame->hw_frames_ctx) {
        if (!m_hwFrame) {
            m_hwFrame = av_frame_alloc();
        }
        if (!m_hwFrame) {
            m_vb->unLock();
            return;
        }
        av_frame_unref(m_hwFrame);
        if (av_hwframe_transfer_data(m_hwFrame, frame, 0) < 0) {
            qWarning("Decoder: VA-API frame transfer failed, frame dropped");
            m_vb->unLock();
            return;
        }
        src = m_hwFrame;
    }
    if (src->format != AV_PIX_FMT_YUV420P && src->format != AV_PIX_FMT_YUVJ420P) {
        AVFrame *yuv = ensureYuv420pFrame(src->width, src->height, static_cast<AVPixelFormat>(src->format));
        if (!yuv) {
            m_vb->unLock();
            return;
        }
        sws_scale(m_convertCtx, (const uint8_t * const *)src->data, src->linesize, 0, src->height, yuv->data, yuv->linesize);
        src = yuv;
    }
    m_onFrame(src->width, src->height, src->data[0], src->data[1], src->data[2], src->linesize[0], src->linesize[1], src->linesize[2]);
    m_vb->unLock();
}

AVFrame *Decoder::ensureYuv420pFrame(int width, int height, AVPixelFormat format)
{
    if (m_convertFrame && m_convertWidth == width && m_convertHeight == height && m_convertSrcFormat == format) {
        return m_convertFrame;
    }
    if (m_convertFrame) {
        av_frame_free(&m_convertFrame);
        m_convertFrame = Q_NULLPTR;
    }
    if (m_convertCtx) {
        sws_freeContext(m_convertCtx);
        m_convertCtx = Q_NULLPTR;
    }
    m_convertFrame = av_frame_alloc();
    if (!m_convertFrame) {
        return Q_NULLPTR;
    }
    m_convertFrame->format = AV_PIX_FMT_YUV420P;
    m_convertFrame->width = width;
    m_convertFrame->height = height;
    if (av_frame_get_buffer(m_convertFrame, 32) < 0) {
        av_frame_free(&m_convertFrame);
        m_convertFrame = Q_NULLPTR;
        return Q_NULLPTR;
    }
    m_convertCtx = sws_getContext(width, height, format, width, height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, Q_NULLPTR, Q_NULLPTR, Q_NULLPTR);
    if (!m_convertCtx) {
        av_frame_free(&m_convertFrame);
        m_convertFrame = Q_NULLPTR;
        return Q_NULLPTR;
    }
    m_convertWidth = width;
    m_convertHeight = height;
    m_convertSrcFormat = format;
    return m_convertFrame;
}

void Decoder::freeConversionResources()
{
    if (m_hwFrame) {
        av_frame_free(&m_hwFrame);
        m_hwFrame = Q_NULLPTR;
    }
    if (m_convertFrame) {
        av_frame_free(&m_convertFrame);
        m_convertFrame = Q_NULLPTR;
    }
    if (m_convertCtx) {
        sws_freeContext(m_convertCtx);
        m_convertCtx = Q_NULLPTR;
    }
    m_convertWidth = 0;
    m_convertHeight = 0;
    m_convertSrcFormat = AV_PIX_FMT_NONE;
}
