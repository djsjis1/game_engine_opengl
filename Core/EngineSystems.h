#ifndef ENGINE_SYSTEMS_H
#define ENGINE_SYSTEMS_H

#include "Systems/ScriptSystem.h"

class Engine;
class PhysicsSystem;
class AudioSystem;

// Windows only retain these handles for editor-specific Play/Stop cleanup.
struct DefaultSystems
{
    PhysicsSystem *physics = nullptr;
    ScriptSystem *script = nullptr;
    AudioSystem *audio = nullptr;
};

DefaultSystems registerDefaultSystems(Engine &engine,
                                      ScriptSystem::HotReloadMode hotReloadMode);

#endif
