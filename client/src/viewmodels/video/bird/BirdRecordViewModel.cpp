#include "BirdRecordViewModel.h"
#include "services/video/bird/BirdRecordService.h"

BirdRecordViewModel::BirdRecordViewModel(VideoFrameProvider *provider, QObject *parent)
    : QObject(parent)
    , m_service(new BirdRecordService(provider, this))
{
    connect(m_service, &BirdRecordService::runningChanged,
            this, &BirdRecordViewModel::runningChanged);
    connect(m_service, &BirdRecordService::imagesLoadedChanged,
            this, &BirdRecordViewModel::imagesLoadedChanged);
    connect(m_service, &BirdRecordService::frameCounterChanged,
            this, &BirdRecordViewModel::frameCounterChanged);
    connect(m_service, &BirdRecordService::recordingStateChanged,
            this, &BirdRecordViewModel::recordingStateChanged);
    connect(m_service, &BirdRecordService::recordFileNameChanged,
            this, &BirdRecordViewModel::recordFileNameChanged);
    connect(m_service, &BirdRecordService::recordDurationChanged,
            this, &BirdRecordViewModel::recordDurationChanged);
    connect(m_service, &BirdRecordService::writtenFramesChanged,
            this, &BirdRecordViewModel::writtenFramesChanged);
    connect(m_service, &BirdRecordService::videoDirChanged,
            this, &BirdRecordViewModel::videoDirChanged);
    connect(m_service, &BirdRecordService::buildFailed,
            this, &BirdRecordViewModel::buildFailed);
}

BirdRecordViewModel::~BirdRecordViewModel()
{
}

bool BirdRecordViewModel::isRunning() const { return m_service->isRunning(); }
bool BirdRecordViewModel::imagesLoaded() const { return m_service->imagesLoaded(); }
int BirdRecordViewModel::frameCounter() const { return m_service->frameCounter(); }
bool BirdRecordViewModel::isRecording() const { return m_service->isRecording(); }
QString BirdRecordViewModel::recordFileName() const { return m_service->recordFileName(); }
int BirdRecordViewModel::recordDuration() const { return m_service->recordDuration(); }
int BirdRecordViewModel::writtenFrames() const { return m_service->writtenFrames(); }
QString BirdRecordViewModel::videoDir() const { return m_service->videoDir(); }

bool BirdRecordViewModel::start() { return m_service->start(); }
void BirdRecordViewModel::stop() { m_service->stop(); }
