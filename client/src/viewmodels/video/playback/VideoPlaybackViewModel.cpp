#include "VideoPlaybackViewModel.h"
#include "services/video/playback/VideoPlaybackService.h"
#include "services/video/common/VideoFrameProvider.h"
#include "services/network/FdbusClientService.h"
#include "services/auth/SessionContext.h"
#include "services/config/AppConfig.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

VideoPlaybackViewModel::VideoPlaybackViewModel(VideoFrameProvider *provider, QObject *parent)
    : QObject(parent)
    , m_service(new VideoPlaybackService(provider, this))
{
    // 连接Service信号
    connect(m_service, &VideoPlaybackService::newFrameReady,
            this, &VideoPlaybackViewModel::onFrameReady);
    connect(m_service, &VideoPlaybackService::playbackFinished,
            this, &VideoPlaybackViewModel::onPlaybackFinished);
    connect(m_service, &VideoPlaybackService::screenshotSaved,
            this, &VideoPlaybackViewModel::onScreenshotSaved);

    // 代理Service属性信号
    connect(m_service, &VideoPlaybackService::playingChanged, this, &VideoPlaybackViewModel::playingChanged);
    connect(m_service, &VideoPlaybackService::frameChanged, this, &VideoPlaybackViewModel::frameChanged);
    connect(m_service, &VideoPlaybackService::videoInfoChanged, this, &VideoPlaybackViewModel::videoInfoChanged);
    connect(m_service, &VideoPlaybackService::vehicleDetectEnabledChanged, this, &VideoPlaybackViewModel::vehicleDetectEnabledChanged);
    connect(m_service, &VideoPlaybackService::trafficLightDetectEnabledChanged, this, &VideoPlaybackViewModel::trafficLightDetectEnabledChanged);
    connect(m_service, &VideoPlaybackService::frameCounterChanged, this, &VideoPlaybackViewModel::frameCounterChanged);

    // frameChanged 同时触发 positionChanged（用于播放时间显示）
    connect(m_service, &VideoPlaybackService::frameChanged, this, [this]() {
        emit positionChanged();
    });

    // 转发毫秒级位置信号
    connect(m_service, &VideoPlaybackService::positionMsChanged, this, &VideoPlaybackViewModel::positionMsChanged);

    // 连接FDBus信号
    connect(&FdbusClientService::instance(), &FdbusClientService::requestSuccess,
            this, &VideoPlaybackViewModel::onFdbusReply);
    connect(&FdbusClientService::instance(), &FdbusClientService::requestFailed,
            this, &VideoPlaybackViewModel::onFdbusError);
}

VideoPlaybackViewModel::~VideoPlaybackViewModel()
{
}

bool VideoPlaybackViewModel::isPlaying() const { return m_service->isPlaying(); }
int VideoPlaybackViewModel::currentFrame() const { return m_service->currentFrame(); }
int VideoPlaybackViewModel::totalFrames() const { return m_service->totalFrames(); }
double VideoPlaybackViewModel::fps() const { return m_service->fps(); }
bool VideoPlaybackViewModel::videoLoaded() const { return m_service->videoLoaded(); }
bool VideoPlaybackViewModel::vehicleDetectEnabled() const { return m_service->vehicleDetectEnabled(); }
bool VideoPlaybackViewModel::trafficLightDetectEnabled() const { return m_service->trafficLightDetectEnabled(); }
QString VideoPlaybackViewModel::screenshotMessage() const { return m_screenshotMessage; }
int VideoPlaybackViewModel::frameCounter() const { return m_service->frameCounter(); }

int VideoPlaybackViewModel::currentPositionSec() const
{
    // 优先使用毫秒级位置（更可靠），回退到帧数计算
    if (m_service->currentPositionMs() > 0) {
        return m_service->currentPositionMs() / 1000;
    }
    return m_service->fps() > 0 ? static_cast<int>(m_service->currentFrame() / m_service->fps()) : 0;
}

int VideoPlaybackViewModel::totalDurationSec() const
{
    // 优先使用毫秒级时长（更可靠），回退到帧数计算
    if (m_service->totalDurationMs() > 0) {
        return (m_service->totalDurationMs() + 500) / 1000;  // 四舍五入
    }
    return m_service->fps() > 0 ? static_cast<int>(m_service->totalFrames() / m_service->fps()) : 0;
}

int VideoPlaybackViewModel::currentPositionMs() const
{
    return m_service->currentPositionMs();
}

int VideoPlaybackViewModel::totalDurationMs() const
{
    return m_service->totalDurationMs();
}

double VideoPlaybackViewModel::playbackSpeed() const
{
    return m_service->property("playbackSpeed").toDouble();
}

void VideoPlaybackViewModel::setPlaybackSpeed(double speed)
{
    m_service->setPlaybackSpeed(speed);
    emit speedChanged();
}

void VideoPlaybackViewModel::setVehicleDetectEnabled(bool enabled)
{
    m_service->setVehicleDetectEnabled(enabled);
}

void VideoPlaybackViewModel::setTrafficLightDetectEnabled(bool enabled)
{
    m_service->setTrafficLightDetectEnabled(enabled);
}

bool VideoPlaybackViewModel::openVideo(const QString &filePath, int resumeSec)
{
    bool ok = m_service->openVideo(filePath);
    if (ok && resumeSec > 0) {
        m_service->seekToSec(resumeSec);
    }
    emit videoInfoChanged();
    return ok;
}

void VideoPlaybackViewModel::closeVideo()
{
    m_service->closeVideo();
}

void VideoPlaybackViewModel::togglePlayPause()
{
    if (m_service->isPlaying()) {
        m_service->pause();
    } else {
        m_service->play();
    }
}

void VideoPlaybackViewModel::seekToSec(double sec)
{
    m_service->seekToSec(sec);
}

void VideoPlaybackViewModel::takeScreenshotAndUpload()
{
    QString videoDir = AppConfig::instance().videoDir();
    QString filePath = m_service->saveScreenshot(videoDir);

    if (filePath.isEmpty()) {
        m_screenshotMessage = "截图失败：保存图片失败";
        emit screenshotMessageChanged();
        emit screenshotUploaded(false, m_screenshotMessage);
        return;
    }

    // 上传截图元数据到服务器
    QFileInfo fi(filePath);
    int sizeKB = static_cast<int>(fi.size() / 1024);

    m_pendingRequestType = 9;  // REQ_UPLOAD_SCREENSHOT
    m_pendingFilePath = filePath;

    m_pendingRequestId = FdbusClientService::instance().sendUploadScreenshotRequest(
        SessionContext::instance().username(),
        fi.fileName(), sizeKB, filePath, "播放截图", ""
    );
    if (m_pendingRequestId == 0) {
        m_screenshotMessage = "截图上传失败：网络未连接";
        emit screenshotMessageChanged();
        emit screenshotUploaded(false, m_screenshotMessage);
        m_pendingFilePath.clear();
        m_pendingRequestType = 0;
        return;
    }

    qDebug() << "播放截图上传:" << fi.fileName() << "大小:" << sizeKB << "KB";
}

void VideoPlaybackViewModel::updatePlayRecord(int videoId, int lastPlaySec)
{
    m_currentVideoId = videoId;
    m_pendingRequestType = 8;  // REQ_UPDATE_PLAY_RECORD
    m_pendingRequestId = FdbusClientService::instance().sendUpdatePlayRecordRequest(
        videoId, SessionContext::instance().username(), lastPlaySec);
    if (m_pendingRequestId == 0) {
        emit playRecordUpdated(false);
        m_pendingRequestType = 0;
    }
}

void VideoPlaybackViewModel::onFrameReady(const QImage &frame)
{
    emit newFrameReady(frame);
    emit positionChanged();
}

void VideoPlaybackViewModel::onPlaybackFinished()
{
    emit playbackFinished();
}

void VideoPlaybackViewModel::onScreenshotSaved(const QString &filePath)
{
    qDebug() << "截图已保存到本地:" << filePath;
}

void VideoPlaybackViewModel::onFdbusReply(qint64 requestId, const QJsonObject &data)
{
    // 仅处理自己发起的请求，避免被其他 ViewModel 的响应串扰
    if (requestId != m_pendingRequestId) return;

    if (m_pendingRequestType == 9) {
        bool success = data.value("success").toBool();
        if (success) {
            m_screenshotMessage = "截图上传成功！";
        } else {
            m_screenshotMessage = "截图上传失败：" + data.value("message").toString();
        }
        emit screenshotMessageChanged();
        emit screenshotUploaded(success, m_screenshotMessage);
        m_pendingRequestType = 0;
        m_pendingRequestId = 0;
        m_pendingFilePath.clear();
    } else if (m_pendingRequestType == 8) {
        bool success = data.value("success").toBool();
        emit playRecordUpdated(success);
        m_pendingRequestType = 0;
        m_pendingRequestId = 0;
    }
}

void VideoPlaybackViewModel::onFdbusError(qint64 requestId, const QString &error)
{
    if (requestId != m_pendingRequestId) return;

    if (m_pendingRequestType == 9) {
        m_screenshotMessage = "截图上传失败：" + error;
        emit screenshotMessageChanged();
        emit screenshotUploaded(false, m_screenshotMessage);
        m_pendingRequestType = 0;
        m_pendingRequestId = 0;
        m_pendingFilePath.clear();
    } else if (m_pendingRequestType == 8) {
        emit playRecordUpdated(false);
        m_pendingRequestType = 0;
        m_pendingRequestId = 0;
    }
}
