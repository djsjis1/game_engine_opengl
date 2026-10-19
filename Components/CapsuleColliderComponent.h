#ifndef CAPSULECOLLIDERCOMPONENT_H
#define CAPSULECOLLIDERCOMPONENT_H

#include "Components/Reflect.h"

// Height includes both hemispherical ends, matching common editor terminology.
// Physics backends derive the cylindrical half-height from height and radius.
struct CapsuleColliderComponent
{
    REFLECT float radius = 0.5f;
    REFLECT float height = 2.0f;
    REFLECT bool isTrigger = false;
};

#endif
