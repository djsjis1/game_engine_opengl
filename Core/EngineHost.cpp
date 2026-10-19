#include "Core/EngineHost.h"
#include "Core/Engine.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include "Core/SceneRouter.h"
#include "Core/SignalQueue.h"
#include "Render/AnimationLibrary.h"
#include "Render/RenderWidget.h"
#include "Components/AnimationComponent.h"
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

EngineHost::EngineHost(QObject *receiver, ScriptSystem::HotReloadMode hotReloadMode)
    : m_engine(std::make_unique<Engine>())
{
    m_systems = registerDefaultSystems(*m_engine, hotReloadMode);
    auto &context = m_engine->context();
    context.onEnterEdit = [this] { m_engine->onEnterEdit(); };
    context.onExitEdit = [this] { m_engine->onExitEdit(); };
    context.onEnterPlay = [this]
    {
        m_engine->onEnterPlay();
        clearDeferredSignals();
        if (m_enterPlay) m_enterPlay();
    };
    context.onExitPlay = [this]
    {
        m_engine->onExitPlay();
        clearDeferredSignals();
        if (m_exitPlay) m_exitPlay();
    };
    QObject::connect(&m_timer, &QTimer::timeout, receiver, [this] { tick(); });
}

EngineHost::~EngineHost()
{
    stop();
    // HFSM2 exits its active state during destruction. Window callbacks may
    // already reference destroyed widgets, so detach before destroying Engine.
    m_engine->context() = EngineContext{};
}

void EngineHost::start()
{
    m_elapsed.start();
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.start(16);
}

void EngineHost::stop()
{
    m_timer.stop();
}

void EngineHost::tick()
{
    const float rawDt = m_elapsed.restart() / 1000.0f;
    auto &time = Time::instance();
    time.step(rawDt);
    m_engine->runFrame(time.delta);

    if (m_fps)
    {
        ++m_fpsFrames;
        m_fpsSeconds += rawDt;
        if (m_fpsSeconds >= 0.5f)
        {
            m_fps(m_fpsFrames / m_fpsSeconds);
            m_fpsFrames = 0;
            m_fpsSeconds = 0.0f;
        }
    }

    if (m_frame) m_frame();
    Input::instance().endFrame();

    auto &router = SceneRouter::instance();
    if (m_engine->state() == EngineState::Play)
    {
        if (router.hasPending())
        {
            std::string scene = router.takePending();
            if (m_sceneChange) m_sceneChange(scene);
        }
    }
    else
    {
        router.takePending();
    }
}

EngineHost::AutoplayResult EngineHost::restartAutoplay()
{
    AutoplayResult result;
    m_engine->world().each([&](flecs::entity, AnimationComponent &animation)
    {
        if (!animation.autoplay || !animation.animFile[0] || !animation.current[0]) return;
        AnimLib *library = AnimationLibraryManager::instance().getOrCreate(animation.animFile);
        if (!library || !library->clips.count(animation.current))
        {
            ++result.invalid;
            return;
        }
        animation.playing = true;
        animation.time = 0.0f;
        ++result.started;
    });
    return result;
}

void EngineHost::routeKey(QKeyEvent *event, bool pressed)
{
    Input::instance().onKey(event->key(), pressed);
}

void EngineHost::routeMouse(QMouseEvent *event)
{
    auto &input = Input::instance();
    auto buttonIndex = [](Qt::MouseButton button)
    {
        if (button == Qt::LeftButton) return 0;
        if (button == Qt::MiddleButton) return 1;
        if (button == Qt::RightButton) return 2;
        return -1;
    };
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)
    {
        const int index = buttonIndex(event->button());
        if (index >= 0) input.onMouseButton(index, event->type() == QEvent::MouseButtonPress);
    }
    else if (event->type() == QEvent::MouseMove)
    {
        input.onMousePosition(static_cast<float>(event->x()), static_cast<float>(event->y()));
        if (!m_lastMousePos.isNull())
        {
            const QPoint delta = event->globalPos() - m_lastMousePos;
            input.onMouseMove(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        }
        m_lastMousePos = event->globalPos();
    }
}

void EngineHost::routeWheel(QWheelEvent *event)
{
    Input::instance().onWheel(static_cast<float>(event->angleDelta().y()) / 120.0f);
}

void EngineHost::applyMouseCapture(RenderWidget *viewport, bool allowCapture)
{
    const bool capture = allowCapture && Input::instance().mouseMode() == Input::kMouseCaptured;
    if (capture != m_cursorCaptured)
    {
        m_cursorCaptured = capture;
        viewport->setCursor(capture ? Qt::BlankCursor : Qt::ArrowCursor);
    }
    if (!capture) return;

    const QPoint center = viewport->mapToGlobal(QPoint(viewport->width() / 2, viewport->height() / 2));
    if (QCursor::pos() != center)
    {
        // setPos may synchronously emit MouseMove on Windows. Update first.
        m_lastMousePos = center;
        QCursor::setPos(center);
    }
    if (!viewport->hasFocus()) viewport->setFocus();
}
