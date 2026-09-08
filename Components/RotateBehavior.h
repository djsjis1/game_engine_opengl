#ifndef ROTATEBEHAVIOR_H
#define ROTATEBEHAVIOR_H

#include "Components/Reflect.h"

struct RotateBehavior
{
    REFLECT float speedX = 0.0f;
    REFLECT float speedY = 0.0f;
    REFLECT float speedZ = 0.0f;
};

#endif
