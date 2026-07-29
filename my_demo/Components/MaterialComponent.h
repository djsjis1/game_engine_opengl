#ifndef MATERIALCOMPONENT_H
#define MATERIALCOMPONENT_H

#include <glm/glm.hpp>
#include "Components/Reflect.h"
struct MaterialComponent
{
    REFLECT glm::vec3 ambient{0.2f, 0.2f, 0.2f};
    REFLECT glm::vec3 diffuse{0.8f, 0.8f, 0.8f};
    REFLECT glm::vec3 specular{1.0f, 1.0f, 1.0f};
    REFLECT float shininess = 32.0f;
};

#endif
