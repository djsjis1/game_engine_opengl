#include <gtest/gtest.h>
#include "Core/EngineHost.h"
#include "Core/Engine.h"
#include "Components/TransformComponent.h"
#include "Components/ScriptComponent.h"
#include <QObject>
#include <cstring>

TEST(EngineHostTest, PlayCallbacksRunOnTransitionButNotDuringDestruction)
{
    QObject receiver;
    int enters = 0;
    int exits = 0;
    {
        EngineHost host(&receiver, ScriptSystem::HotReloadMode::Disabled);
        host.setEnterPlayCallback([&] { ++enters; });
        host.setExitPlayCallback([&] { ++exits; });
        host.engine().setPlay();
        EXPECT_EQ(host.engine().state(), EngineState::Play);
        EXPECT_EQ(enters, 1);
        host.engine().setEdit();
        EXPECT_EQ(host.engine().state(), EngineState::Edit);
        EXPECT_EQ(exits, 1);
        host.engine().setPlay();
        EXPECT_EQ(enters, 2);
        auto scripted = host.engine().world().entity();
        scripted.set<TransformComponent>({});
        ScriptComponent script{};
        std::strncpy(script.source, "function update(dt) end", sizeof(script.source) - 1);
        scripted.set<ScriptComponent>(script);
        host.engine().runFrame(1.0f / 60.0f);
    }
    // HFSM2 exits the active state in Engine's destructor. The host detaches
    // the window callbacks first, so this must stay at the explicit exit count.
    EXPECT_EQ(exits, 1);
}
