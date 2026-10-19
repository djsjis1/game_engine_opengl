// ============================================================
// LuaApiEntity.cpp — 实体类脚本 API
//   当前实体 Transform 读写 / 实体查询与创建销毁 /
//   建造 API（add_mesh 等纯代码造世界）/ 跨实体操作 / 父子层级 /
//   脚本变量（Godot @export 风格）
// 拆分地图见 LuaApiInternal.h；加新实体 API：实现写这里 + 主表加一行。
// ============================================================
#include "LuaApiInternal.h"
#include "Components/TransformComponent.h"
#include "Components/TagComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/MeshComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/RigidbodyComponent.h"
#include "Components/BoxColliderComponent.h"
#include "Components/SphereColliderComponent.h"
#include "Components/CapsuleColliderComponent.h"
#include "Components/CameraComponent.h"
#include "Components/RotateBehavior.h"
#include "Components/LightComponent.h"
#include "Components/ParticleComponent.h" // add_particles / particles_emit
#include "Render/ResourceManager.h"       // add_mesh 资源句柄（纯记账，无 GL 时序要求）
#include "Systems/PhysicsSystem.h"        // set_position 同步物理体
#include "Core/GamePaths.h"               // add_script 相对路径 → 运行时绝对路径
#include <cstring>

/// get_position() → x, y, z
int api_getPosition(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
        return 0; // 无上下文（如异步回调）或无 Transform
    lua_pushnumber(L, c->tf->position.x);
    lua_pushnumber(L, c->tf->position.y);
    lua_pushnumber(L, c->tf->position.z);
    return 3;
}

/// set_position(x, y, z) — 直接改 C++ 端 Transform 内存（零拷贝）
int api_setPosition(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
        return 0;
    c->tf->position.x = (float)luaL_checknumber(L, 1);
    c->tf->position.y = (float)luaL_checknumber(L, 2);
    c->tf->position.z = (float)luaL_checknumber(L, 3);
    // ★ 同步物理体：带 Dynamic 刚体的实体位置"真身"在 Jolt 里，
    //   只改 Transform 下一帧被物理回写拉回原地（读档/重生瞬移失效实bug）
    //   无刚体实体 physicsSetPosition 找不到 body 返回 false，无副作用
    physicsSetPosition(c->id, c->tf->position.x, c->tf->position.y, c->tf->position.z);
    return 0;
}

/// get_rotation() → x, y, z（欧拉角，度）
int api_getRotation(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
        return 0;
    lua_pushnumber(L, c->tf->rotation.x);
    lua_pushnumber(L, c->tf->rotation.y);
    lua_pushnumber(L, c->tf->rotation.z);
    return 3;
}

/// set_rotation(x, y, z)（欧拉角，度）
int api_setRotation(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
        return 0;
    c->tf->rotation.x = (float)luaL_checknumber(L, 1);
    c->tf->rotation.y = (float)luaL_checknumber(L, 2);
    c->tf->rotation.z = (float)luaL_checknumber(L, 3);
    return 0;
}

/// get_scale() → x, y, z
int api_getScale(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
        return 0;
    lua_pushnumber(L, c->tf->scale.x);
    lua_pushnumber(L, c->tf->scale.y);
    lua_pushnumber(L, c->tf->scale.z);
    return 3;
}

/// set_scale(x, y, z)
int api_setScale(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
        return 0;
    c->tf->scale.x = (float)luaL_checknumber(L, 1);
    c->tf->scale.y = (float)luaL_checknumber(L, 2);
    c->tf->scale.z = (float)luaL_checknumber(L, 3);
    return 0;
}

/// set_visible(true/false) — 显示/隐藏当前实体（对标 Godot node.visible）
int api_setVisible(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
        return 0;
    c->tf->visible = lua_toboolean(L, 1) != 0;
    return 0;
}

/// is_visible() → true/false
int api_isVisible(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->tf)
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, c->tf->visible ? 1 : 0);
    return 1;
}

/// get_node("实体名") → entity_id（TagComponent.name 匹配）
int api_getNode(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    const char *name = luaL_checkstring(L, 1);
    flecs::entity_t foundId = 0;
    c->w->each([&](flecs::entity e, const TagComponent &tag)
               {
        if (strcmp(tag.name, name) == 0)
            foundId = e.raw_id(); });
    if (foundId)
    {
        lua_pushinteger(L, (lua_Integer)foundId);
        return 1;
    }
    return 0; // 没找到 → Lua 端收到 nil
}

/// create_entity("名称") → entity_id（带 Tag + Transform）
int api_createEntity(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    const char *name = luaL_optstring(L, 1, "Entity");
    auto e = c->w->entity();
    TagComponent tc{};
    strncpy(tc.name, name, sizeof(tc.name) - 1);
    e.set<TransformComponent>({}); // 必须有 Transform，否则不可见
    // TagComponent 用 C API 设置（Flecs MSVC 兼容性）
    e.set_ptr(c->w->component<TagComponent>().raw_id(), sizeof(TagComponent), &tc);
    lua_pushinteger(L, (lua_Integer)e.raw_id());
    return 1;
}

// ---------------- Lua 建造 API（纯代码造世界，LÖVE2D/Godot 纯脚本风格） ----------------
// create_entity 只生"光杆"实体（Tag+Transform），下面这组负责"装配"：
// 形状/颜色/物理/行为/相机——bootstrap 脚本在 ready() 里搭出整个游戏

/// 取"活着的"目标实体（建造 API 公共入口；死实体/0 → 返回 null 实体）
static flecs::entity buildTarget(lua_State *L, int argIdx)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return flecs::entity::null();
    flecs::entity_t id = (flecs::entity_t)luaL_checkinteger(L, argIdx);
    if (!id || !c->w->is_alive(id))
        return flecs::entity::null(); // 防御：脚本可能传过期 ID
    return c->w->entity(id);
}

/// add_mesh(id, "cube") — 给实体加可见网格（不加就是不可见的逻辑实体）
int api_addMesh(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    const char *type = luaL_optstring(L, 2, "cube");
    MeshComponent m{};
    // 句柄化：同名网格命中缓存共享 GPU 数据；acquire 纯记账（延迟创建）
    m.meshHandle = ResourceManager::instance().acquireMesh(type);
    strncpy(m.meshType, type, sizeof(m.meshType) - 1);
    e.set<MeshComponent>(m);
    return 0;
}

/// add_material(id, r, g, b [, metallic, roughness]) — PBR 材质
int api_addMaterial(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    MaterialComponent mat{};
    mat.albedo = {(float)luaL_checknumber(L, 2),
                  (float)luaL_checknumber(L, 3),
                  (float)luaL_checknumber(L, 4)};
    mat.metallic = (float)luaL_optnumber(L, 5, 0.0);
    mat.roughness = (float)luaL_optnumber(L, 6, 0.5);
    const auto compId = e.world().component<MaterialComponent>().raw_id();
    if (const auto *old = static_cast<const MaterialComponent *>(
            ecs_get_id(e.world().c_ptr(), e.raw_id(), compId)))
        ResourceManager::instance().releaseTexture(old->texHandle);
    e.set<MaterialComponent>(mat);
    return 0;
}

/// add_texture(id, "textures/wood.png") — 给材质设 albedo 贴图（相对资源根）
/// 无材质组件时自动加一个白色底材质（贴图当纯色系数，原样显示）
int api_addTexture(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    const char *rel = luaL_checkstring(L, 2);
    auto compId = e.world().component<MaterialComponent>().raw_id();
    auto *mat = (MaterialComponent *)ecs_get_mut_id(e.world().c_ptr(), e.raw_id(), compId);
    if (!mat)
    {
        MaterialComponent m{}; // 白底：贴图不被染色，原样显示
        m.albedo = {1.0f, 1.0f, 1.0f};
        e.set<MaterialComponent>(m);
        mat = (MaterialComponent *)ecs_get_mut_id(e.world().c_ptr(), e.raw_id(), compId);
    }
    if (mat)
    {
        strncpy(mat->albedoTex, rel, sizeof(mat->albedoTex) - 1);
        mat->albedoTex[sizeof(mat->albedoTex) - 1] = '\0';
        ResourceManager::instance().releaseTexture(mat->texHandle);
        mat->texHandle = 0; // 归零 → RenderWidget 下帧惰性重新 acquire
    }
    return 0;
}

/// add_rigidbody(id, motionType) — 物理刚体（0=静态地面 1=运动学 2=动态）
int api_addRigidbody(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    RigidbodyComponent rb{};
    rb.motionType = (int)luaL_optinteger(L, 2, 2);
    e.set<RigidbodyComponent>(rb);
    return 0;
}

/// add_collider(id [, hx, hy, hz]) — 盒碰撞体半尺寸（默认 0.5 配 1x1 立方体）
int api_addCollider(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    BoxColliderComponent box{};
    box.halfExtents = {(float)luaL_optnumber(L, 2, 0.5),
                       (float)luaL_optnumber(L, 3, 0.5),
                       (float)luaL_optnumber(L, 4, 0.5)};
    e.remove<SphereColliderComponent>();
    e.remove<CapsuleColliderComponent>();
    e.set<BoxColliderComponent>(box);
    return 0;
}

/// add_sphere_collider(id [, radius]) — 球形碰撞体
int api_addSphereCollider(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    SphereColliderComponent sphere{};
    sphere.radius = (float)luaL_optnumber(L, 2, 0.5);
    e.remove<BoxColliderComponent>();
    e.remove<CapsuleColliderComponent>();
    e.set<SphereColliderComponent>(sphere);
    return 0;
}

/// add_capsule_collider(id [, radius, height]) — 胶囊碰撞体，总高包含两端半球
int api_addCapsuleCollider(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    CapsuleColliderComponent capsule{};
    capsule.radius = (float)luaL_optnumber(L, 2, 0.5);
    capsule.height = (float)luaL_optnumber(L, 3, 2.0);
    e.remove<BoxColliderComponent>();
    e.remove<SphereColliderComponent>();
    e.set<CapsuleColliderComponent>(capsule);
    return 0;
}

/// add_script(id, "scripts/xxx.lua") — 挂行为脚本（相对资源根路径）
/// ★ 运行时拼绝对路径（开发=项目目录，导出=exe/assets/）→ 导出天然有效
int api_addScript(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    const char *rel = luaL_checkstring(L, 2);
    ScriptComponent sc{};
    // 绝对路径直接用，相对路径拼 GamePaths::root()（兼容两种写法）
    QString path = QString::fromUtf8(rel);
    QString abs = (path.size() >= 2 && path[1] == ':') || path.startsWith('/')
                      ? path
                      : GamePaths::root() + path;
    strncpy(sc.filePath, abs.toUtf8().constData(), sizeof(sc.filePath) - 1);
    e.set<ScriptComponent>(sc); // ScriptSystem 下帧自动初始化并跑 ready()
    return 0;
}

/// add_camera(id, px, py, pz [, yaw=-90, pitch=-30]) — 游戏相机（active）
int api_addCamera(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    CameraComponent cam{};
    cam.position = {(float)luaL_checknumber(L, 2),
                    (float)luaL_checknumber(L, 3),
                    (float)luaL_checknumber(L, 4)};
    cam.yaw = (float)luaL_optnumber(L, 5, -90);
    cam.pitch = (float)luaL_optnumber(L, 6, -30);
    cam.isEditor = false; // 游戏相机：不吃编辑器 WASD
    cam.active = true;
    e.set<CameraComponent>(cam);
    return 0;
}

/// add_rotate(id, sx, sy, sz) — 自转装饰（度/秒，传送门那种效果）
int api_addRotate(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    RotateBehavior rot{};
    rot.speedX = (float)luaL_optnumber(L, 2, 0);
    rot.speedY = (float)luaL_optnumber(L, 3, 0);
    rot.speedZ = (float)luaL_optnumber(L, 4, 0);
    e.set<RotateBehavior>(rot);
    return 0;
}

/// add_light(id [, r, g, b, intensity, type]) — 光源（对标 Godot Light3D）
/// type: 0=方向光(默认) 1=点光源 2=聚光灯；点光/聚光位置取实体 Transform
int api_addLight(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    LightComponent lc{};
    float r = (float)luaL_optnumber(L, 2, 1.0);
    float g = (float)luaL_optnumber(L, 3, 1.0);
    float b = (float)luaL_optnumber(L, 4, 1.0);
    lc.diffuse = {r, g, b};
    lc.specular = {r, g, b};
    lc.ambient = {r * 0.2f, g * 0.2f, b * 0.2f}; // 环境光取漫反射 20%
    lc.intensity = (float)luaL_optnumber(L, 5, 1.0);
    lc.type = (int)luaL_optinteger(L, 6, 0); // 0 方向 / 1 点光 / 2 聚光
    e.set<LightComponent>(lc);
    return 0;
}

/// add_particles(id [, preset]) — 粒子发射器（对标 Godot CPUParticles3D）
/// preset: "fire"(默认 火焰) / "smoke"(烟) / "sparkle"(拾取星光)
/// 细调参数在编辑器 Inspector（rate/speed/颜色渐变/大小/重力全可调）
int api_addParticles(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    const char *preset = luaL_optstring(L, 2, "fire");
    ParticleComponent pc{}; // 默认参数即火焰
    if (strcmp(preset, "smoke") == 0)
    {
        pc.colorStart = {0.5f, 0.5f, 0.5f};
        pc.colorEnd = {0.15f, 0.15f, 0.15f};
        pc.gravity = {0, 0.8f, 0}; // 烟上飘
        pc.speed = 1.0f;
        pc.spread = 15.0f;
        pc.sizeStart = 0.2f;
        pc.sizeEnd = 0.6f; // 烟越飘越散
        pc.lifetime = 2.5f;
        pc.rate = 10.0f;
        pc.intensity = 1.0f;
        pc.additive = 0; // 烟用普通 alpha 混合
    }
    else if (strcmp(preset, "sparkle") == 0)
    {
        pc.colorStart = {0.4f, 0.9f, 1.0f};
        pc.colorEnd = {1.0f, 1.0f, 1.0f};
        pc.gravity = {0, -0.5f, 0};
        pc.speed = 1.5f;
        pc.spread = 80.0f; // 全方向飞散
        pc.sizeStart = 0.06f;
        pc.sizeEnd = 0.0f;
        pc.lifetime = 0.8f;
        pc.rate = 40.0f;
        pc.intensity = 2.0f; // HDR 发光
    }
    e.set<ParticleComponent>(pc);
    return 0;
}

/// particles_emit(id, true/false) — 开/关发射（已有粒子自然飘完，Godot 同行为）
int api_particlesEmit(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    bool on = lua_toboolean(L, 2) != 0;
    auto *pc = static_cast<ParticleComponent *>(ecs_get_mut_id(
        e.world().c_ptr(), e.raw_id(),
        e.world().component<ParticleComponent>().raw_id()));
    if (pc)
        pc->emitting = on;
    return 0;
}

/// set_node_scale(id, x, y, z) — 缩放任意实体（配合 add_collider 记得同步）
int api_setNodeScale(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (!e)
        return 0;
    auto *tf = static_cast<TransformComponent *>(ecs_get_mut_id(
        e.world().c_ptr(), e.raw_id(),
        e.world().component<TransformComponent>().raw_id()));
    if (!tf)
        return 0;
    tf->scale = {(float)luaL_checknumber(L, 2),
                 (float)luaL_checknumber(L, 3),
                 (float)luaL_checknumber(L, 4)};
    return 0;
}

/// destroy_entity(id) — 删除实体（物理体/脚本环境/Mesh 引用由 OnRemove 自动清）
int api_destroyEntity(lua_State *L)
{
    auto e = buildTarget(L, 1);
    if (e)
        e.destruct();
    return 0;
}

// ---------------- 跨实体操作 ----------------

/// get_node_position(entity_id) → x, y, z（读任意实体位置）
int api_getNodePosition(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1);
    // 用 C API 查询（Flecs C++ e.get<T>() 在 MSVC 有 bug）
    auto *tf = (TransformComponent *)ecs_get_id(c->w->c_ptr(), eid,
                                                c->w->component<TransformComponent>().raw_id());
    if (!tf)
    {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushnil(L);
        return 3;
    }
    lua_pushnumber(L, tf->position.x);
    lua_pushnumber(L, tf->position.y);
    lua_pushnumber(L, tf->position.z);
    return 3;
}

/// get_node_rotation(entity_id) → rx, ry, rz（欧拉角，度）
int api_getNodeRotation(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1);
    auto *tf = (TransformComponent *)ecs_get_id(c->w->c_ptr(), eid,
                                                c->w->component<TransformComponent>().raw_id());
    if (!tf)
    {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushnil(L);
        return 3;
    }
    lua_pushnumber(L, tf->rotation.x);
    lua_pushnumber(L, tf->rotation.y);
    lua_pushnumber(L, tf->rotation.z);
    return 3;
}

/// set_node_position(entity_id, x, y, z)（跨实体操控）
int api_setNodePosition(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    auto eid = (flecs::entity_t)luaL_checkinteger(L, 1);
    auto *tf = (TransformComponent *)ecs_get_mut_id(c->w->c_ptr(), eid,
                                                    c->w->component<TransformComponent>().raw_id());
    if (!tf)
        return 0;
    tf->position.x = (float)luaL_checknumber(L, 2);
    tf->position.y = (float)luaL_checknumber(L, 3);
    tf->position.z = (float)luaL_checknumber(L, 4);
    // 同步物理体（同 set_position：防止被物理回写覆盖）
    physicsSetPosition(eid, tf->position.x, tf->position.y, tf->position.z);
    return 0;
}

// ---------------- 父子层级（Flecs 原生 ChildOf 关系） ----------------

/// set_parent(child_id, parent_id) — 设置父实体（parent_id=0 解除）
int api_setParent(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
        return 0;
    auto childId = (flecs::entity_t)luaL_checkinteger(L, 1);
    auto parentId = (flecs::entity_t)luaL_optinteger(L, 2, 0);

    flecs::entity child = c->w->entity(childId);
    if (!child.is_alive())
        return 0;

    if (parentId == 0)
        child.remove(flecs::ChildOf, flecs::Wildcard); // 解除父子
    else
        child.child_of(c->w->entity(parentId));
    return 0;
}

/// get_parent(entity_id) → parent_id 或 nil
int api_getParent(lua_State *L)
{
    auto *c = getCtx(L);
    if (!c || !c->w)
    {
        lua_pushnil(L);
        return 1;
    }
    auto id = (flecs::entity_t)luaL_checkinteger(L, 1);
    flecs::entity e = c->w->entity(id);
    if (!e.is_alive())
    {
        lua_pushnil(L);
        return 1;
    }
    flecs::entity p = e.parent();
    if (p)
        lua_pushinteger(L, (lua_Integer)p.raw_id());
    else
        lua_pushnil(L);
    return 1;
}

// ---------------- 脚本变量（Godot @export 风格，编辑器可见） ----------------
// 变量存 ScriptComponent.properties，编辑器选中组件时显示

static ScriptComponent *getScriptComp(LuaCtx *c)
{
    if (!c || !c->w)
        return nullptr; // 无上下文（如异步回调）
    auto compId = c->w->component<ScriptComponent>().raw_id();
    return (ScriptComponent *)ecs_get_mut_id(c->w->c_ptr(), c->id, compId);
}

/// export("name", default) — 定义变量，编辑器可见
int api_export(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    auto *sc = getScriptComp(c);
    if (sc && sc->properties.find(name) == sc->properties.end())
        sc->properties[name] = val;
    return 0;
}

/// get_prop("name") → value
int api_getProp(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    auto *sc = getScriptComp(c);
    if (sc)
    {
        auto it = sc->properties.find(name);
        if (it != sc->properties.end())
        {
            lua_pushnumber(L, it->second);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

/// set_prop("name", value)
int api_setProp(lua_State *L)
{
    auto *c = getCtx(L);
    const char *name = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    auto *sc = getScriptComp(c);
    if (sc)
        sc->properties[name] = val;
    return 0;
}
