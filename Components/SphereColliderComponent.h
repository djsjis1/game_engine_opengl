#ifndef SPHERECOLLIDERCOMPONENT_H
#define SPHERECOLLIDERCOMPONENT_H

#include "Components/Reflect.h"

// Sphere collider radius is measured in local space and scaled by the largest
// world scale axis so a non-uniformly scaled sphere remains conservative.
struct SphereColliderComponent
{
    REFLECT float radius = 0.5f;
    REFLECT bool isTrigger = false;
};

#endif
