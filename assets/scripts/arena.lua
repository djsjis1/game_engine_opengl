-- ============================================================
-- arena.lua —— 双人大战竞技场（纯代码搭建 + 裁判逻辑）
-- 修复版：摄像机降低拉近 + 方向光调暖双倍环境光
-- ============================================================

local player = 0
local enemy = 0
local gameOver = false

local function makeWall(name, x, z, sx, sz, r, g, b)
    local w = create_entity(name)
    set_node_position(w, x, 1.0, z)
    set_node_scale(w, sx, 2, sz)
    add_mesh(w, "cube")
    add_material(w, r, g, b, 0.1, 0.8)
    add_rigidbody(w, 0)
    add_collider(w)
    return w
end

local function makeFighter(name, x, z, r, g, b)
    local body = create_entity(name)
    set_node_position(body, x, 1.0, z)
    add_mesh(body, "cube")
    add_material(body, r, g, b, 0.3, 0.4)
    add_rigidbody(body, 2)
    add_collider(body)
    local barBg = create_entity(name .. "_hpbg")
    add_mesh(barBg, "cube")
    add_material(barBg, 0.5, 0.1, 0.1)
    set_parent(barBg, body)
    set_node_position(barBg, 0, 1.1, 0)
    set_node_scale(barBg, 1.1, 0.12, 0.12)
    local bar = create_entity(name .. "_hp")
    add_mesh(bar, "cube")
    add_material(bar, 0.2, 0.95, 0.3, 0.0, 0.6)
    set_parent(bar, body)
    set_node_position(bar, 0, 1.1, 0)
    set_node_scale(bar, 1.0, 0.14, 0.14)
    return body
end

function ready()
    set_fog(0.5, 0.45, 0.55, 0.008)

    -- ① 场地
    local ground = create_entity("Ground")
    set_node_position(ground, 0, -0.5, 0)
    set_node_scale(ground, 20, 1, 20)
    add_mesh(ground, "cube")
    add_material(ground, 0.45, 0.42, 0.4, 0.0, 0.9)
    add_rigidbody(ground, 0)
    add_collider(ground)

    makeWall("WallN", 0, -10, 20, 0.5, 0.35, 0.33, 0.32)
    makeWall("WallS", 0, 10, 20, 0.5, 0.35, 0.33, 0.32)
    makeWall("WallW", -10, 0, 0.5, 20, 0.35, 0.33, 0.32)
    makeWall("WallE", 10, 0, 0.5, 20, 0.35, 0.33, 0.32)
    makeWall("Pillar1", -4, -3, 1.2, 1.2, 0.3, 0.3, 0.38)
    makeWall("Pillar2", 4, -3, 1.2, 1.2, 0.3, 0.3, 0.38)
    makeWall("Pillar3", -4, 3, 1.2, 1.2, 0.3, 0.3, 0.38)
    makeWall("Pillar4", 4, 3, 1.2, 1.2, 0.3, 0.3, 0.38)
    makeWall("MidWall", 0, 0, 5, 0.8, 0.28, 0.26, 0.3)

    -- ② 角色
    player = makeFighter("Player", 0, 6, 1.0, 0.75, 0.2)
    add_script(player, "D:/code/.std/opengl/my_demo/assets/scripts/arena_player.lua")
    enemy = makeFighter("Enemy", 0, -6, 0.85, 0.15, 0.12)
    add_script(enemy, "D:/code/.std/opengl/my_demo/assets/scripts/arena_enemy.lua")

    -- ③ 火盆
    local brazier = create_entity("Brazier")
    set_node_position(brazier, 7, 0.4, -7)
    set_node_scale(brazier, 0.8, 0.8, 0.8)
    add_mesh(brazier, "cube")
    add_material(brazier, 0.2, 0.18, 0.16, 0.0, 0.9)
    local flame = create_entity("BrazierFlame")
    set_node_position(flame, 7, 1.0, -7)
    add_particles(flame, "fire")
    add_light(flame, 1.0, 0.55, 0.25, 1.6, 1)

    -- ④ 主光：暖金调，环境光 0.5（原 0.18 太暗，竞技场看不清）
    local sun = create_entity("Sun")
    set_node_position(sun, 0, 10, 0)
    add_light(sun, 1.0, 0.95, 0.85, 0.6, 0)

    -- ⑤ 相机初始（第一人称：玩家眼睛位置，朝前看）
    local cam = create_entity("ArenaCam")
    add_camera(cam, 0, 1.8, 6, -90, 0)

    -- ⑥ 寻路烘焙
    local obstacles = nav_bake()
    print("竞技场就绪！障碍烘焙: " .. obstacles .. " 个。击败红色恶魔！")

    -- ⑦ 裁判（修复：回调里不依赖 ctx，只调可定向的函数）
    local me = get_node("ArenaMain")
    local function scheduleRestart()
        -- Z 轴微偏 0.001 确保 tween 有"路程"可走——补间为 0→0 时引擎一帧
        -- 判定完成、不发射 doneSignal，导致 change_scene 永远不调用（R1）
        tween(me, "position", 0, 0, 0.001, 3.0, "linear", "restart_timer_done")
    end
    connect("restart_timer_done", function()
        change_scene("D:/code/.std/opengl/my_demo/assets/scenes/arena.json")
    end)
    connect_to(player, "died", function()
        if gameOver then return end
        gameOver = true
        print("========== 你输了... 3 秒后重开 ==========")
        scheduleRestart()
    end)
    connect_to(enemy, "died", function()
        if gameOver then return end
        gameOver = true
        print("========== 胜利！！3 秒后重开 ==========")
        scheduleRestart()
    end)
end

function update(dt)
    -- 摄像机由 arena_player.lua 控制（第一人称），这里不再管
end
