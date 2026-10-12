#include "ScrcpyView.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoSink>
#include <QWheelEvent>

#include "adbprocess.h"

#include <algorithm>
#include <cstring>

extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace {

constexpr qreal kFlickStep = 0.07;
constexpr qreal kFlickMaxNotches = 2.0;
constexpr int kFlickTicks = 8;
constexpr int kFlickIntervalMs = 14;

void copyPlane(QVideoFrame &frame, int plane, const uint8_t *src, int srcStride,
               int planeWidth, int planeHeight)
{
    uint8_t *dst = frame.bits(plane);
    const int dstStride = frame.bytesPerLine(plane);
    if (!dst || dstStride <= 0 || !src || srcStride <= 0 || planeWidth <= 0 || planeHeight <= 0) {
        return;
    }
    const int rowBytes = std::min({dstStride, srcStride, planeWidth});
    for (int y = 0; y < planeHeight; ++y) {
        std::memcpy(dst + static_cast<qsizetype>(y) * dstStride,
                    src + static_cast<qsizetype>(y) * srcStride,
                    static_cast<size_t>(rowBytes));
    }
}

}

ScrcpyView::ScrcpyView(QObject *parent)
    : QObject(parent)
{
    qsc::IDeviceManage &manage = qsc::IDeviceManage::getInstance();
    connect(&manage, &qsc::IDeviceManage::deviceConnected,
            this, &ScrcpyView::handleDeviceConnected);
    connect(&manage, &qsc::IDeviceManage::deviceDisconnected,
            this, &ScrcpyView::handleDeviceDisconnected);
}

ScrcpyView::~ScrcpyView()
{
    stop();
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
}

QVideoSink *ScrcpyView::videoSink() const
{
    return m_sink;
}

void ScrcpyView::setVideoSink(QVideoSink *sink)
{
    if (m_sink == sink) {
        return;
    }
    if (m_sink) {
        disconnect(m_sink, nullptr, this, nullptr);
    }
    m_sink = sink;
    if (m_sink) {
        connect(m_sink, &QObject::destroyed, this, [this]() { m_sink = nullptr; });
    }
    emit videoSinkChanged();
}

QSize ScrcpyView::frameSize() const
{
    return m_frameSize;
}

bool ScrcpyView::connected() const
{
    return m_connected;
}

QString ScrcpyView::serial() const
{
    return m_serial;
}

void ScrcpyView::setSerial(const QString &serial)
{
    if (m_serial == serial) {
        return;
    }
    m_serial = serial;
    emit serialChanged();
}

QString ScrcpyView::errorString() const
{
    return m_error;
}

QString ScrcpyView::adbPath() const
{
    return m_adbPath;
}

void ScrcpyView::setAdbPath(const QString &path)
{
    if (m_adbPath == path) {
        return;
    }
    m_adbPath = path;
    emit adbPathChanged();
}

QString ScrcpyView::serverPath() const
{
    return m_serverPath;
}

void ScrcpyView::setServerPath(const QString &path)
{
    if (m_serverPath == path) {
        return;
    }
    m_serverPath = path;
    emit serverPathChanged();
}

bool ScrcpyView::rgbaFallback() const
{
    return m_rgbaFallback;
}

void ScrcpyView::setRgbaFallback(bool fallback)
{
    if (m_rgbaFallback == fallback) {
        return;
    }
    m_rgbaFallback = fallback;
    emit rgbaFallbackChanged();
}

int ScrcpyView::maxSize() const
{
    return m_maxSize;
}

void ScrcpyView::setMaxSize(int size)
{
    size = qBound(0, size, 65535);
    if (m_maxSize == size) {
        return;
    }
    m_maxSize = size;
    emit maxSizeChanged();
}

int ScrcpyView::bitRate() const
{
    return m_bitRate;
}

void ScrcpyView::setBitRate(int rate)
{
    rate = qBound(0, rate, 0x7FFFFFFF);
    if (m_bitRate == rate) {
        return;
    }
    m_bitRate = rate;
    emit bitRateChanged();
}

int ScrcpyView::maxFps() const
{
    return m_maxFps;
}

void ScrcpyView::setMaxFps(int fps)
{
    fps = qMax(0, fps);
    if (m_maxFps == fps) {
        return;
    }
    m_maxFps = fps;
    emit maxFpsChanged();
}

bool ScrcpyView::hardwareDecode() const
{
    return m_hardwareDecode;
}

void ScrcpyView::setHardwareDecode(bool enabled)
{
    if (m_hardwareDecode == enabled) {
        return;
    }
    m_hardwareDecode = enabled;
    emit hardwareDecodeChanged();
}

bool ScrcpyView::trackpadOptimizations() const
{
    return m_trackpadOptimizations;
}

void ScrcpyView::setTrackpadOptimizations(bool enabled)
{
    if (m_trackpadOptimizations == enabled) {
        return;
    }
    m_trackpadOptimizations = enabled;
    emit trackpadOptimizationsChanged();
}

void ScrcpyView::start(const QString &serial)
{
    if (m_device) {
        stop();
    }
    setError(QString());

    if (!ensureAdb()) {
        return;
    }

    const QString target = serial.trimmed().isEmpty() ? m_serial : serial.trimmed();
    if (target.isEmpty()) {
        setError(QStringLiteral("no device serial provided"));
        return;
    }
    if (target != m_serial) {
        m_serial = target;
        emit serialChanged();
    }

    const QString server = resolveServerPath();
    if (server.isEmpty() || !QFileInfo::exists(server)) {
        setError(QStringLiteral("scrcpy-server not found (set serverPath): %1").arg(server));
        return;
    }

    qsc::DeviceParams params;
    params.serial = target;
    params.scid = QRandomGenerator::global()->bounded(1, 10000) & 0x7FFFFFFF;
    params.serverLocalPath = server;
    params.serverRemotePath = QStringLiteral("/data/local/tmp/scrcpy-server.jar");
    params.serverVersion = QStringLiteral("4.1");
    params.localPort = 27183;
    params.maxSize = static_cast<quint16>(m_maxSize);
    params.bitRate = static_cast<quint32>(m_bitRate);
    params.maxFps = static_cast<quint32>(m_maxFps);
    params.renderExpiredFrames = false;
    params.useReverse = true;
    params.display = true;
    params.stayAwake = true;
    params.logLevel = QStringLiteral("info");

    qputenv("NOON_SCRCPY_SOFTWARE_DECODE", m_hardwareDecode ? QByteArrayLiteral("0") : QByteArrayLiteral("1"));

    qsc::IDeviceManage &manage = qsc::IDeviceManage::getInstance();
    if (!manage.connectDevice(params)) {
        setError(QStringLiteral("could not create a scrcpy session for %1").arg(target));
        return;
    }

    QPointer<qsc::IDevice> device = manage.getDevice(target);
    if (device.isNull()) {
        setError(QStringLiteral("no scrcpy device interface for %1").arg(target));
        return;
    }

    m_device = device.data();
    m_device->registerDeviceObserver(this);
    m_transferConn = connect(m_device, &qsc::IDevice::fileHandlerResult,
                             this, &ScrcpyView::fileTransferFinished);
}

void ScrcpyView::stop()
{
    if (m_flickTimer) {
        m_flickTimer->stop();
    }

    m_stopping = true;
    if (m_device) {
        m_device->deRegisterDeviceObserver(this);
        m_device->disconnectDevice();
        m_device = nullptr;
    }
    if (m_transferConn) {
        disconnect(m_transferConn);
        m_transferConn = QMetaObject::Connection();
    }
    m_stopping = false;

    setConnected(false);
    if (m_sink) {
        m_sink->setVideoFrame(QVideoFrame());
    }
}

void ScrcpyView::pointer(int type, qreal nx, qreal ny, int pointerId)
{
    Q_UNUSED(pointerId)

    if (type == PointerRightPress) {
        if (m_device) {
            m_device->postGoBack();
        }
        return;
    }
    if (type == PointerRightRelease) {
        return;
    }

    injectMouse(type, nx, ny);
}

void ScrcpyView::injectMouse(int type, qreal nx, qreal ny)
{
    if (!m_device) {
        return;
    }

    const QSize frame = m_frameSize;
    if (!frame.isValid() || frame.isEmpty()) {
        return;
    }

    QEvent::Type eventType = QEvent::None;
    Qt::MouseButton button = Qt::LeftButton;
    Qt::MouseButtons buttons = Qt::NoButton;
    switch (type) {
    case PointerPress:
        eventType = QEvent::MouseButtonPress;
        buttons = Qt::LeftButton;
        break;
    case PointerRelease:
        eventType = QEvent::MouseButtonRelease;
        break;
    case PointerMove:
        eventType = QEvent::MouseMove;
        buttons = Qt::LeftButton;
        break;
    default:
        return;
    }

    const QPointF pos(nx * frame.width(), ny * frame.height());
    QMouseEvent event(eventType, pos, pos, button, buttons, Qt::NoModifier);
    m_device->mouseEvent(&event, frame, frame);
}

void ScrcpyView::key(int type, int qtKey, int modifiers, bool autoRepeat)
{
    if (!m_device) {
        return;
    }

    const QSize frame = (m_frameSize.isValid() && !m_frameSize.isEmpty())
                            ? m_frameSize
                            : QSize(1, 1);
    const QEvent::Type eventType = (type == KeyRelease) ? QEvent::KeyRelease : QEvent::KeyPress;
    QKeyEvent event(eventType, qtKey, static_cast<Qt::KeyboardModifiers>(modifiers),
                    QString(), autoRepeat);
    m_device->keyEvent(&event, frame, frame);
}

void ScrcpyView::wheel(qreal nx, qreal ny, qreal dx, qreal dy)
{
    if (!m_device) {
        return;
    }

    const QSize frame = m_frameSize;
    if (!frame.isValid() || frame.isEmpty()) {
        return;
    }

    const QPoint angle(qRound(dx), qRound(dy));
    if (angle.isNull()) {
        return;
    }

    const QPointF pos(nx * frame.width(), ny * frame.height());
    QWheelEvent event(pos, pos, QPoint(0, 0), angle, Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    m_device->wheelEvent(&event, frame, frame);
}

void ScrcpyView::scroll(qreal nx, qreal ny, qreal dx, qreal dy)
{
    if (!m_trackpadOptimizations) {
        wheel(nx, ny, dx * 120.0, dy * 120.0);
        return;
    }

    if (!m_device || m_frameSize.isEmpty()) {
        return;
    }

    const qreal ox = qBound(-kFlickMaxNotches, dx, kFlickMaxNotches) * kFlickStep;
    const qreal oy = qBound(-kFlickMaxNotches, dy, kFlickMaxNotches) * kFlickStep;
    if (qFuzzyIsNull(ox) && qFuzzyIsNull(oy)) {
        return;
    }

    if (m_flickTimer && m_flickTimer->isActive()) {
        m_flickEndX = qBound(0.02, m_flickEndX - ox, 0.98);
        m_flickEndY = qBound(0.02, m_flickEndY + oy, 0.98);
        return;
    }

    const qreal startX = qBound(0.02, nx, 0.98);
    const qreal startY = qBound(0.02, ny, 0.98);
    m_flickStartX = startX;
    m_flickStartY = startY;
    m_flickEndX = qBound(0.02, startX - ox, 0.98);
    m_flickEndY = qBound(0.02, startY + oy, 0.98);
    m_flickStep = 0;

    injectMouse(PointerPress, startX, startY);
    ensureFlickTimer();
    m_flickTimer->start();
}

void ScrcpyView::ensureFlickTimer()
{
    if (m_flickTimer) {
        return;
    }
    m_flickTimer = new QTimer(this);
    m_flickTimer->setInterval(kFlickIntervalMs);
    connect(m_flickTimer, &QTimer::timeout, this, &ScrcpyView::flickTick);
}

void ScrcpyView::flickTick()
{
    const int totalTicks = kFlickTicks;
    ++m_flickStep;
    const qreal t = qMin<qreal>(1.0, qreal(m_flickStep) / qreal(totalTicks));
    const qreal x = m_flickStartX + (m_flickEndX - m_flickStartX) * t;
    const qreal y = m_flickStartY + (m_flickEndY - m_flickStartY) * t;

    injectMouse(PointerMove, x, y);

    if (t >= 1.0) {
        injectMouse(PointerRelease, x, y);
        m_flickTimer->stop();
    }
}

void ScrcpyView::back()
{
    if (m_device) {
        m_device->postGoBack();
    }
}

void ScrcpyView::home()
{
    if (m_device) {
        m_device->postGoHome();
    }
}

void ScrcpyView::recents()
{
    if (m_device) {
        m_device->postAppSwitch();
    }
}

void ScrcpyView::installApk(const QString &localPath)
{
    requestTransfer(localPath, QString(), true);
}

void ScrcpyView::pushFile(const QString &localPath, const QString &deviceDir)
{
    requestTransfer(localPath, deviceDir, false);
}

void ScrcpyView::requestTransfer(const QString &localPath, const QString &deviceDir, bool isApk)
{
    if (!m_device) {
        setError(QStringLiteral("not connected; cannot transfer files"));
        return;
    }

    QString path = localPath;
    const QUrl url(localPath);
    if (url.isLocalFile()) {
        path = url.toLocalFile();
    } else {
        path = QUrl::fromPercentEncoding(localPath.toUtf8());
    }
    path = path.trimmed();
    if (path.isEmpty()) {
        setError(QStringLiteral("empty file path"));
        return;
    }

    if (isApk) {
        m_device->installApkRequest(path);
        return;
    }

    QString dir = deviceDir.trimmed();
    if (dir.isEmpty()) {
        dir = QStringLiteral("/sdcard/");
    } else if (!dir.endsWith(QLatin1Char('/'))) {
        dir += QLatin1Char('/');
    }
    m_device->pushFileRequest(path, dir);
}

QRectF ScrcpyView::contentRect(qreal itemWidth, qreal itemHeight) const
{
    if (itemWidth <= 0.0 || itemHeight <= 0.0) {
        return QRectF();
    }
    if (!m_frameSize.isValid() || m_frameSize.isEmpty()) {
        return QRectF(0.0, 0.0, itemWidth, itemHeight);
    }

    const qreal frameAspect = qreal(m_frameSize.width()) / qreal(m_frameSize.height());
    const qreal itemAspect = itemWidth / itemHeight;

    qreal width = 0.0;
    qreal height = 0.0;
    if (itemAspect > frameAspect) {
        height = itemHeight;
        width = height * frameAspect;
    } else {
        width = itemWidth;
        height = width / frameAspect;
    }

    return QRectF((itemWidth - width) / 2.0, (itemHeight - height) / 2.0, width, height);
}

QString ScrcpyView::findAdb() const
{
    const QString configured = m_adbPath.trimmed();
    if (configured.isEmpty()) {
        return QStandardPaths::findExecutable(QStringLiteral("adb"));
    }

    const QFileInfo info(configured);
    if (info.isFile() && info.isExecutable()) {
        return info.absoluteFilePath();
    }

    return QStandardPaths::findExecutable(configured);
}

QStringList ScrcpyView::adbDevices() const
{
    QStringList result;

    const QString adb = findAdb();
    if (adb.isEmpty()) {
        return result;
    }

    QProcess process;
    process.start(adb, QStringList { QStringLiteral("devices") });
    if (!process.waitForStarted(2000)) {
        return result;
    }
    if (!process.waitForFinished(4000)) {
        process.kill();
        return result;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QRegularExpression separator(QStringLiteral("\\s+"));

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("List of devices"))) {
            continue;
        }

        const QStringList parts = trimmed.split(separator, Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        const QString serial = parts.first();
        const QString state = (parts.size() > 1) ? parts.at(1) : QString();
        if (state == QStringLiteral("device")) {
            result << serial;
        }
    }

    return result;
}

void ScrcpyView::onFrame(int width, int height, uint8_t *dataY, uint8_t *dataU, uint8_t *dataV,
                         int linesizeY, int linesizeU, int linesizeV)
{
    if (!m_sink || width <= 0 || height <= 0 || !dataY || !dataU || !dataV) {
        return;
    }

    const QSize size(width, height);
    if (size != m_frameSize) {
        QMetaObject::invokeMethod(this, [this, size]() {
            if (m_frameSize != size) {
                m_frameSize = size;
                emit frameSizeChanged();
            }
        }, Qt::QueuedConnection);
    }

    QVideoFrame frame;
    if (!m_rgbaFallback
        && buildYuv420Frame(width, height, dataY, dataU, dataV,
                            linesizeY, linesizeU, linesizeV, frame)
        && frame.isValid()) {
        m_sink->setVideoFrame(frame);
        return;
    }

    if (buildRgbaFrame(width, height, dataY, dataU, dataV,
                       linesizeY, linesizeU, linesizeV, frame)
        && frame.isValid()) {
        m_sink->setVideoFrame(frame);
    }
}

void ScrcpyView::onVideoSessionChanged(const QSize &size, bool clientResized)
{
    Q_UNUSED(clientResized)

    if (size.isEmpty()) {
        return;
    }

    QMetaObject::invokeMethod(this, [this, size]() {
        if (m_frameSize != size) {
            m_frameSize = size;
            emit frameSizeChanged();
        }
    }, Qt::QueuedConnection);
}

void ScrcpyView::updateFPS(quint32 fps)
{
    Q_UNUSED(fps)
}

void ScrcpyView::handleDeviceConnected(bool success, const QString &serial,
                                       const QString &deviceName, const QSize &size)
{
    Q_UNUSED(deviceName)
    Q_UNUSED(size)

    if (serial != m_serial) {
        return;
    }

    if (success) {
        setError(QString());
        setConnected(true);
        return;
    }

    if (m_device) {
        m_device->deRegisterDeviceObserver(this);
        m_device = nullptr;
    }
    if (m_transferConn) {
        disconnect(m_transferConn);
        m_transferConn = QMetaObject::Connection();
    }
    setConnected(false);
    setError(QStringLiteral("failed to start scrcpy on %1: adb missing or unauthorized, "
                            "device offline, or scrcpy-server version mismatch").arg(serial));
}

void ScrcpyView::handleDeviceDisconnected(const QString &serial)
{
    if (serial != m_serial) {
        return;
    }

    if (m_device) {
        m_device->deRegisterDeviceObserver(this);
        m_device = nullptr;
    }
    if (m_transferConn) {
        disconnect(m_transferConn);
        m_transferConn = QMetaObject::Connection();
    }
    setConnected(false);
    if (!m_stopping) {
        setError(QStringLiteral("device %1 disconnected").arg(serial));
    }
}

void ScrcpyView::setConnected(bool connected)
{
    if (m_connected == connected) {
        return;
    }
    m_connected = connected;
    emit connectedChanged();
}

void ScrcpyView::setError(const QString &message)
{
    if (m_error == message) {
        return;
    }
    m_error = message;
    emit errorStringChanged();
    if (!message.isEmpty()) {
        emit errorOccurred(message);
    }
}

bool ScrcpyView::ensureAdb()
{
    const QString path = findAdb();
    if (path.isEmpty()) {
        setError(QStringLiteral("adb not found; install android-tools or set adbPath"));
        return false;
    }
    qsc::AdbProcess::setAdbPath(path);
    return true;
}

QString ScrcpyView::resolveServerPath()
{
    const QString configured = m_serverPath.trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }

    const QString beside = QCoreApplication::applicationDirPath() + QStringLiteral("/scrcpy-server");
    if (QFileInfo::exists(beside)) {
        return beside;
    }

#ifdef SCRCPY_SERVER_DEFAULT_PATH
    const QString compiled = QStringLiteral(SCRCPY_SERVER_DEFAULT_PATH);
    if (QFileInfo::exists(compiled)) {
        return compiled;
    }
#endif

    return beside;
}

bool ScrcpyView::buildYuv420Frame(int width, int height, uint8_t *dataY, uint8_t *dataU,
                                  uint8_t *dataV, int linesizeY, int linesizeU, int linesizeV,
                                  QVideoFrame &out) const
{
    QVideoFrameFormat format(QSize(width, height), QVideoFrameFormat::Format_YUV420P);
    if (!format.isValid()) {
        return false;
    }

    QVideoFrame frame(format);
    if (!frame.isValid()) {
        return false;
    }
    if (!frame.map(QVideoFrame::WriteOnly)) {
        return false;
    }

    const int chromaWidth = (width + 1) / 2;
    const int chromaHeight = (height + 1) / 2;
    copyPlane(frame, 0, dataY, linesizeY, width, height);
    copyPlane(frame, 1, dataU, linesizeU, chromaWidth, chromaHeight);
    copyPlane(frame, 2, dataV, linesizeV, chromaWidth, chromaHeight);
    frame.unmap();

    out = frame;
    return true;
}

bool ScrcpyView::buildRgbaFrame(int width, int height, uint8_t *dataY, uint8_t *dataU,
                                uint8_t *dataV, int linesizeY, int linesizeU, int linesizeV,
                                QVideoFrame &out)
{
    if (m_sws && (m_swsWidth != width || m_swsHeight != height)) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (!m_sws) {
        m_sws = sws_getContext(width, height, AV_PIX_FMT_YUV420P,
                               width, height, AV_PIX_FMT_BGRA,
                               SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        m_swsWidth = width;
        m_swsHeight = height;
        if (!m_sws) {
            return false;
        }
    }

    QVideoFrameFormat format(QSize(width, height), QVideoFrameFormat::Format_BGRA8888);
    if (!format.isValid()) {
        return false;
    }

    QVideoFrame frame(format);
    if (!frame.isValid()) {
        return false;
    }
    if (!frame.map(QVideoFrame::WriteOnly)) {
        return false;
    }

    const uint8_t *sourceData[4] = { dataY, dataU, dataV, nullptr };
    int sourceStride[4] = { linesizeY, linesizeU, linesizeV, 0 };
    uint8_t *targetData[4] = { frame.bits(0), nullptr, nullptr, nullptr };
    int targetStride[4] = { frame.bytesPerLine(0), 0, 0, 0 };
    sws_scale(m_sws, sourceData, sourceStride, 0, height, targetData, targetStride);
    frame.unmap();

    out = frame;
    return true;
}
