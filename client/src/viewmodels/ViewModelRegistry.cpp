#include "ViewModelRegistry.h"

// ViewModels
#include "LoginViewModel.h"
#include "SettingsViewModel.h"
#include "NetworkViewModel.h"
#include "VideoRecordViewModel.h"
#include "ReverseRecordViewModel.h"
#include "VideoPlaybackViewModel.h"
#include "PlaybackDetectViewModel.h"
#include "ScreenshotViewModel.h"
#include "ScreenshotListViewModel.h"
#include "BirdRecordViewModel.h"
#include "SystemViewModel.h"
// Services
#include "VideoRecorderService.h"
#include "ReverseRecordService.h"
#include "VideoPlaybackService.h"
#include "PlaybackDetectService.h"
#include "VideoFrameProvider.h"

ViewModelRegistry::ViewModelRegistry(QQmlApplicationEngine &engine, QGuiApplication &app)
    : m_engine(engine)
    , m_app(app)
{
}

ViewModelRegistry::~ViewModelRegistry() = default;

void ViewModelRegistry::registerAll()
{
    // 创建 VideoFrameProvider 并注册为 QML image provider（供视频帧高效显示，替代 base64 方案）
    m_frameProvider = new VideoFrameProvider();
    m_engine.addImageProvider("videoframe", m_frameProvider);

    // ===== ViewModels =====
    // 单例
    m_engine.rootContext()->setContextProperty("settingsViewModel",
                                                SettingsViewModel::instance());
    registerViewModel<LoginViewModel>("loginViewModel");
    registerViewModel<NetworkViewModel>("networkViewModel");
    registerViewModel<VideoRecordViewModel>("videoRecordViewModel");
    registerViewModel<ScreenshotViewModel>("screenshotViewModel");
    registerViewModel<PlaybackDetectViewModel>("playbackDetectViewModel");
    registerViewModel<ReverseRecordViewModel>("reverseRecordViewModel");
    registerViewModel<ScreenshotListViewModel>("screenshotListViewModel");
    registerViewModelWithProvider<VideoPlaybackViewModel>("videoPlaybackViewModel");
    registerViewModelWithProvider<BirdRecordViewModel>("birdRecordViewModel");
    // 系统集成（启动导航/音乐、WiFi 连接）
    registerViewModel<SystemViewModel>("systemViewModel");

    // ===== Services =====
    registerViewModelWithProvider<VideoRecorderService>("videoRecorderService");
    registerViewModel<PlaybackDetectService>("playbackDetectService");
    registerViewModelWithProvider<ReverseRecordService>("reverseRecordService");
    registerViewModelWithProvider<VideoPlaybackService>("videoPlaybackService");
}
