#include "ReverseRecordViewModel.h"
#include "services/video/reverse/ReverseRecordService.h"
#include <QDebug>

ReverseRecordViewModel::ReverseRecordViewModel(QObject *parent)
    : QObject(parent)
    , m_service(new ReverseRecordService(nullptr, this))
{
    // 连接Service信号到ViewModel信号（属性代理）
    connect(m_service, &ReverseRecordService::autoRecordingChanged,
            this, &ReverseRecordViewModel::autoRecordingChanged);
    connect(m_service, &ReverseRecordService::autoRecordEnabledChanged,
            this, &ReverseRecordViewModel::autoRecordEnabledChanged);
    connect(m_service, &ReverseRecordService::autoRecordIntervalChanged,
            this, &ReverseRecordViewModel::autoRecordIntervalChanged);
    connect(m_service, &ReverseRecordService::targetFramesChanged,
            this, &ReverseRecordViewModel::targetFramesChanged);
    connect(m_service, &ReverseRecordService::currentFrameCountChanged,
            this, &ReverseRecordViewModel::currentFrameCountChanged);
    connect(m_service, &ReverseRecordService::runningChanged,
            this, &ReverseRecordViewModel::runningChanged);
    connect(m_service, &ReverseRecordService::videoDirChanged,
            this, &ReverseRecordViewModel::videoDirChanged);
    connect(m_service, &ReverseRecordService::steeringAngleChanged,
            this, &ReverseRecordViewModel::steeringAngleChanged);
    connect(m_service, &ReverseRecordService::useStaticImageChanged,
            this, &ReverseRecordViewModel::useStaticImageChanged);
    connect(m_service, &ReverseRecordService::staticImageChanged,
            this, &ReverseRecordViewModel::staticImageChanged);
    connect(m_service, &ReverseRecordService::recordingSaved,
            this, &ReverseRecordViewModel::recordingSaved);
}

ReverseRecordViewModel::~ReverseRecordViewModel()
{
}

bool ReverseRecordViewModel::isAutoRecording() const { return m_service->isAutoRecording(); }
bool ReverseRecordViewModel::isAutoRecordEnabled() const { return m_service->isAutoRecordEnabled(); }
int ReverseRecordViewModel::autoRecordIntervalSec() const { return m_service->autoRecordIntervalSec(); }
int ReverseRecordViewModel::targetFrames() const { return m_service->targetFrames(); }
int ReverseRecordViewModel::currentFrameCount() const { return m_service->currentFrameCount(); }
bool ReverseRecordViewModel::isRunning() const { return m_service->isRunning(); }
QString ReverseRecordViewModel::videoDir() const { return m_service->videoDir(); }
qreal ReverseRecordViewModel::steeringAngle() const { return m_service->steeringAngle(); }
bool ReverseRecordViewModel::useStaticImage() const { return m_service->useStaticImage(); }
QString ReverseRecordViewModel::staticImagePath() const { return m_service->staticImagePath(); }

void ReverseRecordViewModel::setAutoRecordEnabled(bool enabled) { m_service->setAutoRecordEnabled(enabled); }
void ReverseRecordViewModel::setAutoRecordIntervalSec(int sec) { m_service->setAutoRecordIntervalSec(sec); }
void ReverseRecordViewModel::setTargetFrames(int frames) { m_service->setTargetFrames(frames); }
void ReverseRecordViewModel::setVideoDir(const QString &dir) { m_service->setVideoDir(dir); }

bool ReverseRecordViewModel::start() { return m_service->start(); }
void ReverseRecordViewModel::stop() { m_service->stop(); }
bool ReverseRecordViewModel::openCamera(int camId)
{
    return m_service->openCamera(camId);
}
void ReverseRecordViewModel::closeCamera() { m_service->closeCamera(); }
void ReverseRecordViewModel::steerLeft() { m_service->steerLeft(); }
void ReverseRecordViewModel::steerRight() { m_service->steerRight(); }
void ReverseRecordViewModel::steerCenter() { m_service->steerCenter(); }
bool ReverseRecordViewModel::loadStaticImage(const QString &path) { return m_service->loadStaticImage(path); }
