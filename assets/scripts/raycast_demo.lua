-- ============================================================
-- raycast_demo.lua —— 物理射线演示：射线悬停（自动弹跳）
--
-- 原理：每帧从自己下方向地面打射线（走 Jolt 物理世界）
--   命中距离很近 → 说明快落地了 → 向上施加冲量弹起
--   → 方块永远弹跳，落不了地 —— raycast 在工作的直观证明
--
-- 【用法】
--   ① 建地面：新实体 → RigidbodyComponent(motionType=0 静态)
--      + BoxColliderComponent → Transform 缩放 (10, 0.2, 10) 摆在 y=-2
--   ② 建方块：RigidbodyComponent(动态) + BoxColliderComponent + 本脚本
--      摆在地面上方 y=3
--   ③ ▶ 运行 → 方块下落 → 接近地面被射线检测到 → 自动弹起循环
-- ============================================================

function ready()
    print("射线悬停演示启动")
end

function update(dt)
    local x, y, z = get_position()

    -- ⚠ 经典坑：射线从自己中心发出会先打中自己的碰撞体！
    -- 解法：起点移到自身碰撞体外（半高 0.5 + 余量）再发射
    local hit = raycast(x, y - 0.6, z, 0, -1, 0, 50)

    if hit and hit.dist < 0.6 then
        -- 快碰到下方物体：向上弹（冲量大小可调）
        apply_impulse(0, 8, 0)
        emit("hover_bounce", hit.dist) -- 顺便发信号（可被其他实体监听）
    end
end

-- ============================================================
-- raycast_demo.lua —— 物理射线演示：射线悬停（自动弹跳）
--
-- 原理：每帧从自己位置向正下方打射线（走 Jolt 物理世界）
--   命中距离 < 1.2 → 说明快落地了 → 向上施加冲量弹起
--   → 方块永远弹跳，落不了地 —— raycast 在工作的直观证明
--
-- 【用法】
--   ① 建一个地面：新实体 → Rigidbody(motionType=0 静态) + BoxCollider
--      Transform 缩放 (10, 0.2, 10) 摆在 y=-2
--   ② 建一个方块：Rigidbody(动态) + BoxCollider + 本脚本
--      摆在地面上方 y=3
--   ③ ▶ 运行 → 方块下落 → 快落地时被射线检测到 → 自动弹起循环
-- ============================================================

function ready()
    print("射线悬停演示启动")
end

function update(dt)
    local x, y, z = get_position()

    -- 从自己位置向正下方打射线（最远 50 米）
    local hit = raycast(x, y, z, 0, -1, 0, 50)

    if hit and hit.dist < 1.2 then
        -- 快碰到下方物体了：向上弹（冲量大小可调）
        apply_impulse(0, 8, 0)
        emit("hover_bounce", hit.dist) -- 顺便发信号（可被其他实体监听）
    end
end
