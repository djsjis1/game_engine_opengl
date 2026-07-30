-- ============================================================
-- code_world.lua —— 纯代码造世界（LÖVE2D / Godot 纯脚本风格）
--
-- 场景文件里只有一个挂本脚本的空实体，其余一切——
-- 地面、玩家、箱子、装饰、相机——全部在 ready() 里用代码搭出来。
--
-- 【建造流水线】（每个实体都是同一套四步）
--   create_entity → add_mesh（可见）→ add_material（颜色）
--   → add_rigidbody + add_collider（物理，可选）→ add_script（行为，可选）
--
-- 【适合场景】程序化关卡（随机地牢/无尽跑酷）、原型快速验证、
--   或单纯喜欢"一切皆代码"的开发方式
-- ============================================================

-- 造一个"标准方块"的辅助函数（代码复用——这就是代码造世界的优势）
local function makeBox(name, x, y, z, r, g, b, motion)
    local e = create_entity(name)
    set_node_position(e, x, y, z)
    add_mesh(e, "cube")
    add_material(e, r, g, b)
    if motion >= 0 then
        add_rigidbody(e, motion)
        add_collider(e) -- 默认 0.5 半尺寸配 1x1 立方体
    end
    return e
end

function ready()
    print("代码造世界：开始搭建...")

    -- ① 地面（静态物理，缩放成 16x1x16 的大平板）
    local ground = create_entity("Ground")
    set_node_position(ground, 0, -0.5, 0)
    set_node_scale(ground, 16, 1, 16)
    add_mesh(ground, "cube")
    add_material(ground, 0.4, 0.55, 0.4)
    add_rigidbody(ground, 0) -- 0 = 静态
    add_collider(ground)     -- 半尺寸 0.5×scale(16,1,16) = 8,0.5,8 引擎自动乘

    -- ② 玩家（动态物理 + 挂 player.lua 行为——嵌套脚本！）
    local hero = makeBox("Hero", 0, 0.55, 0, 1.0, 0.55, 0.15, 2)
    add_script(hero, "scripts/player.lua")

    -- ③ 一圈彩色箱子（程序化生成——手摆 8 个箱子 vs 一个循环）
    for i = 1, 8 do
        local angle = (i / 8) * 2 * math.pi
        local bx = math.cos(angle) * 5
        local bz = math.sin(angle) * 5
        -- 彩虹色：HSV 想法的简化版（按角度调 RGB）
        makeBox("Ring" .. i, bx, 0.55, bz,
            0.5 + 0.5 * math.cos(angle),
            0.5 + 0.5 * math.sin(angle),
            0.7, 2)
    end

    -- ④ 中心自转金柱（装饰：无物理 + add_rotate）
    local pillar = create_entity("Pillar")
    set_node_position(pillar, 0, 2.5, -6)
    set_node_scale(pillar, 0.6, 3, 0.6)
    add_mesh(pillar, "cube")
    add_material(pillar, 1.0, 0.85, 0.1)
    add_rotate(pillar, 0, 120, 0) -- 每秒 120 度自转

    -- ⑤ 相机（连相机都是代码建的——场景文件里没有任何相机）
    local cam = create_entity("Camera")
    add_camera(cam, 0, 10, 14, -90, -35)

    print("世界搭建完成：1地面 + 1玩家 + 8箱子 + 1金柱 + 1相机")
end

function update(dt)
    -- bootstrap 实体本身没有逐帧逻辑（玩家行为在 player.lua 里）
end
