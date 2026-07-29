#pragma once
// =====================================================
// ⚠ 自动生成文件，勿手改！
// 由 tools/reflect_gen.cpp 扫描 Components/*.h 的 @reflect 标记生成
// 加字段：组件头文件里字段行尾加 // @reflect，重新构建即生效
// =====================================================

inline void registerGeneratedReflection(flecs::world &w)
{
    w.component<BoxColliderComponent>()
        .member<glm::vec3>("halfExtents", 1, offsetof(BoxColliderComponent, halfExtents));

    w.component<CameraComponent>()
        .member<glm::vec3>("position", 1, offsetof(CameraComponent, position))
        .member<float>("yaw", 1, offsetof(CameraComponent, yaw))
        .member<float>("pitch", 1, offsetof(CameraComponent, pitch))
        .member<float>("fov", 1, offsetof(CameraComponent, fov))
        .member<float>("nearPlane", 1, offsetof(CameraComponent, nearPlane))
        .member<float>("farPlane", 1, offsetof(CameraComponent, farPlane))
        .member<float>("moveSpeed", 1, offsetof(CameraComponent, moveSpeed))
        .member<float>("mouseSens", 1, offsetof(CameraComponent, mouseSens))
        .member<bool>("isEditor", 1, offsetof(CameraComponent, isEditor))
        .member<bool>("active", 1, offsetof(CameraComponent, active));

    w.component<HealthComponent>()
        .member<float>("hp", 1, offsetof(HealthComponent, hp))
        .member<float>("maxHp", 1, offsetof(HealthComponent, maxHp))
        .member<float>("regen", 1, offsetof(HealthComponent, regen));

    w.component<MaterialComponent>()
        .member<glm::vec3>("ambient", 1, offsetof(MaterialComponent, ambient))
        .member<glm::vec3>("diffuse", 1, offsetof(MaterialComponent, diffuse))
        .member<glm::vec3>("specular", 1, offsetof(MaterialComponent, specular))
        .member<float>("shininess", 1, offsetof(MaterialComponent, shininess));

    w.component<RigidbodyComponent>()
        .member<int32_t>("motionType", 1, offsetof(RigidbodyComponent, motionType))
        .member<float>("mass", 1, offsetof(RigidbodyComponent, mass))
        .member<float>("friction", 1, offsetof(RigidbodyComponent, friction))
        .member<float>("restitution", 1, offsetof(RigidbodyComponent, restitution));

    w.component<RotateBehavior>()
        .member<float>("speedX", 1, offsetof(RotateBehavior, speedX))
        .member<float>("speedY", 1, offsetof(RotateBehavior, speedY))
        .member<float>("speedZ", 1, offsetof(RotateBehavior, speedZ));

    w.component<SpriteComponent>()
        .member<int32_t>("frame", 1, offsetof(SpriteComponent, frame));

    w.component<TransformComponent>()
        .member<glm::vec3>("position", 1, offsetof(TransformComponent, position))
        .member<glm::vec3>("rotation", 1, offsetof(TransformComponent, rotation))
        .member<glm::vec3>("scale", 1, offsetof(TransformComponent, scale))
        .member<bool>("visible", 1, offsetof(TransformComponent, visible));

}
