#pragma once

#include <QObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

#include "QtScrcpyCore.h"

class QVideoFrame;
class QTimer;

struct SwsContext;

class ScrcpyView : public QObject, public qsc::DeviceObserver
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScrcpyView)

    Q_PROPERTY(QVideoSink *videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(QSize frameSize READ frameSize NOTIFY frameSizeChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString serial READ serial WRITE setSerial NOTIFY serialChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString adbPath READ adbPath WRITE setAdbPath NOTIFY adbPathChanged)
    Q_PROPERTY(QString serverPath READ serverPath WRITE setServerPath NOTIFY serverPathChanged)
    Q_PROPERTY(bool rgbaFallback READ rgbaFallback WRITE setRgbaFallback NOTIFY rgbaFallbackChanged)
    Q_PROPERTY(int maxSize READ maxSize WRITE setMaxSize NOTIFY maxSizeChanged)
    Q_PROPERTY(int bitRate READ bitRate WRITE setBitRate NOTIFY bitRateChanged)
    Q_PROPERTY(int maxFps READ maxFps WRITE setMaxFps NOTIFY maxFpsChanged)
    Q_PROPERTY(bool hardwareDecode READ hardwareDecode WRITE setHardwareDecode NOTIFY hardwareDecodeChanged)
    Q_PROPERTY(bool trackpadOptimizations READ trackpadOptimizations WRITE setTrackpadOptimizations NOTIFY trackpadOptimizationsChanged)

public:
    enum PointerType
    {
        PointerPress = 0,
        PointerRelease = 1,
        PointerMove = 2,
        PointerRightPress = 3,
        PointerRightRelease = 4
    };
    Q_ENUM(PointerType)

    enum KeyType
    {
        KeyPress = 0,
        KeyRelease = 1
    };
    Q_ENUM(KeyType)

    explicit ScrcpyView(QObject *parent = nullptr);
    ~ScrcpyView() override;

    QVideoSink *videoSink() const;
    void setVideoSink(QVideoSink *sink);

    QSize frameSize() const;
    bool connected() const;
    QString serial() const;
    void setSerial(const QString &serial);
    QString errorString() const;
    QString adbPath() const;
    void setAdbPath(const QString &path);
    QString serverPath() const;
    void setServerPath(const QString &path);
    bool rgbaFallback() const;
    void setRgbaFallback(bool fallback);

    int maxSize() const;
    void setMaxSize(int size);
    int bitRate() const;
    void setBitRate(int rate);
    int maxFps() const;
    void setMaxFps(int fps);

    bool hardwareDecode() const;
    void setHardwareDecode(bool enabled);

    bool trackpadOptimizations() const;
    void setTrackpadOptimizations(bool enabled);

    Q_INVOKABLE void start(const QString &serial);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pointer(int type, qreal nx, qreal ny, int pointerId = -1);
    Q_INVOKABLE void key(int type, int qtKey, int modifiers, bool autoRepeat = false);
    Q_INVOKABLE void wheel(qreal nx, qreal ny, qreal dx, qreal dy);
    Q_INVOKABLE void scroll(qreal nx, qreal ny, qreal dx, qreal dy);
    Q_INVOKABLE void back();
    Q_INVOKABLE void home();
    Q_INVOKABLE void recents();
    Q_INVOKABLE void installApk(const QString &localPath);
    Q_INVOKABLE void pushFile(const QString &localPath, const QString &deviceDir = QString());
    Q_INVOKABLE QRectF contentRect(qreal itemWidth, qreal itemHeight) const;
    Q_INVOKABLE QString findAdb() const;
    Q_INVOKABLE QStringList adbDevices() const;

signals:
    void videoSinkChanged();
    void frameSizeChanged();
    void connectedChanged();
    void serialChanged();
    void errorStringChanged();
    void adbPathChanged();
    void serverPathChanged();
    void rgbaFallbackChanged();
    void maxSizeChanged();
    void bitRateChanged();
    void maxFpsChanged();
    void hardwareDecodeChanged();
    void trackpadOptimizationsChanged();
    void errorOccurred(const QString &message);
    void fileTransferFinished(bool success, bool isApk);

protected:
    void onFrame(int width, int height, uint8_t *dataY, uint8_t *dataU, uint8_t *dataV,
                 int linesizeY, int linesizeU, int linesizeV) override;
    void onVideoSessionChanged(const QSize &size, bool clientResized) override;
    void updateFPS(quint32 fps) override;

private:
    void handleDeviceConnected(bool success, const QString &serial, const QString &deviceName, const QSize &size);
    void handleDeviceDisconnected(const QString &serial);
    void setConnected(bool connected);
    void setError(const QString &message);
    bool ensureAdb();
    QString resolveServerPath();

    void injectMouse(int type, qreal nx, qreal ny);
    void ensureFlickTimer();
    void flickTick();
    void requestTransfer(const QString &localPath, const QString &deviceDir, bool isApk);

    bool buildYuv420Frame(int width, int height, uint8_t *dataY, uint8_t *dataU, uint8_t *dataV,
                          int linesizeY, int linesizeU, int linesizeV, QVideoFrame &out) const;
    bool buildRgbaFrame(int width, int height, uint8_t *dataY, uint8_t *dataU, uint8_t *dataV,
                        int linesizeY, int linesizeU, int linesizeV, QVideoFrame &out);

    QVideoSink *m_sink = nullptr;
    qsc::IDevice *m_device = nullptr;
    QSize m_frameSize;
    QString m_serial;
    QString m_error;
    QString m_adbPath;
    QString m_serverPath;
    bool m_connected = false;
    bool m_stopping = false;
    bool m_rgbaFallback = false;
    int m_maxSize = 1920;
    int m_bitRate = 20000000;
    int m_maxFps = 60;
    bool m_hardwareDecode = true;
    bool m_trackpadOptimizations = true;

    QMetaObject::Connection m_transferConn;

    QTimer *m_flickTimer = nullptr;
    int m_flickStep = 0;
    qreal m_flickStartX = 0.0;
    qreal m_flickStartY = 0.0;
    qreal m_flickEndX = 0.0;
    qreal m_flickEndY = 0.0;

    mutable SwsContext *m_sws = nullptr;
    int m_swsWidth = 0;
    int m_swsHeight = 0;
};
