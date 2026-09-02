#ifndef VIEWMODELREGISTRY_H
#define VIEWMODELREGISTRY_H

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

class VideoFrameProvider;

// ============================================================================
// ViewModelRegistry - ViewModel/Service 装配中心
//
// 将原先散落在 main.cpp 中的 ViewModel/Service 构造与 QML 上下文注册收敛到此，
// main.cpp 只负责应用生命周期（创建 app/engine、加载 QML、事件循环）。
//
// 新增 ViewModel/Service：
//   - parent-only 构造            → registerViewModel<T>("qmlName")
//   - 需要 VideoFrameProvider     → registerViewModelWithProvider<T>("qmlName")
//   - 单例                        → 直接 setContextProperty(name, T::instance())
//
// 生命周期：所有 new 出的对象 parent 到 QGuiApplication，随 app 退出自动销毁；
//           VideoFrameProvider 的所有权经 addImageProvider 转移给 QML 引擎。
// ============================================================================
class ViewModelRegistry
{
public:
    ViewModelRegistry(QQmlApplicationEngine &engine, QGuiApplication &app);
    ~ViewModelRegistry();

    /// 创建 VideoFrameProvider + 注册全部 ViewModel/Service 到 QML 上下文
    void registerAll();

private:
    // 通用：构造 T(parent=&m_app) 并注册为 qmlName
    template<typename T>
    void registerViewModel(const char *qmlName)
    {
        m_engine.rootContext()->setContextProperty(qmlName, new T(&m_app));
    }

    // 需要 frameProvider 的：构造 T(provider, parent=&m_app)
    template<typename T>
    void registerViewModelWithProvider(const char *qmlName)
    {
        m_engine.rootContext()->setContextProperty(qmlName, new T(m_frameProvider, &m_app));
    }

    QQmlApplicationEngine &m_engine;
    QGuiApplication &m_app;
    VideoFrameProvider *m_frameProvider = nullptr;  // 所有权归 QML 引擎（addImageProvider）
};

#endif // VIEWMODELREGISTRY_H
