#ifndef ENGINE_HOST_H
#define ENGINE_HOST_H

#include "Core/EngineSystems.h"
#include <QElapsedTimer>
#include <QPoint>
#include <QTimer>
#include <functional>
#include <memory>
#include <string>

class Engine;
class RenderWidget;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

// Owns the shared editor/game runtime. Window callbacks contain only their
// different UI and scene-loading policies; the frame order lives here.
class EngineHost
{
public:
    struct AutoplayResult { int started = 0; int invalid = 0; };

    EngineHost(QObject *receiver, ScriptSystem::HotReloadMode hotReloadMode);
    ~EngineHost();

    EngineHost(const EngineHost &) = delete;
    EngineHost &operator=(const EngineHost &) = delete;

    Engine &engine() { return *m_engine; }
    const DefaultSystems &systems() const { return m_systems; }

    void setEnterPlayCallback(std::function<void()> callback) { m_enterPlay = std::move(callback); }
    void setExitPlayCallback(std::function<void()> callback) { m_exitPlay = std::move(callback); }
    void setFrameCallback(std::function<void()> callback) { m_frame = std::move(callback); }
    void setSceneChangeCallback(std::function<void(const std::string &)> callback)
    { m_sceneChange = std::move(callback); }
    void setFpsCallback(std::function<void(float)> callback) { m_fps = std::move(callback); }

    void start();
    void stop();
    AutoplayResult restartAutoplay();

    void routeKey(QKeyEvent *event, bool pressed);
    void routeMouse(QMouseEvent *event);
    void routeWheel(QWheelEvent *event);
    void applyMouseCapture(RenderWidget *viewport, bool allowCapture);

private:
    void tick();

    std::unique_ptr<Engine> m_engine;
    DefaultSystems m_systems;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    QPoint m_lastMousePos;
    bool m_cursorCaptured = false;
    int m_fpsFrames = 0;
    float m_fpsSeconds = 0.0f;
    std::function<void()> m_enterPlay;
    std::function<void()> m_exitPlay;
    std::function<void()> m_frame;
    std::function<void(const std::string &)> m_sceneChange;
    std::function<void(float)> m_fps;
};

#endif
