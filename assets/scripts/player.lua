-- ============================================================
-- player.lua —— 第一人称多场景跳跳（两关 + 传送门 + 跨场景存档）
--
-- 操作：
--   鼠标 转视角（第一人称）    W/A/S/D 相对朝向移动    空格 跳跃
--   走近旋转的传送门 → 切换关卡（金门→第2关，绿门→回第1关）
--   K 存档    L 读档（跨关卡也能精确回去）    Esc 退出运行（松开鼠标）
--
-- 【第一人称要点】
--   鼠标锁中心（set_mouse_mode captured）→ get_mouse_delta 累加到 yaw/pitch
--   相机每帧跟随玩家：set_camera(玩家位置 + 眼高, yaw, pitch)
--   移动方向按 yaw 旋转：W 永远朝"你看的水平方向"走
--   玩家网格隐藏（第一人称在自己身体里，看不到自己）
--
-- 【多场景 / 跨场景读档】见下方各段注释（沿用原多场景逻辑）
-- ============================================================

local MOVE_SPEED = 5
local JUMP_IMPULSE = 6
local MOUSE_SENS = 0.15 -- 鼠标灵敏度
local EYE_HEIGHT = 0.6  -- 相机相对玩家中心的眼睛高度

-- 视角状态（脚本自己维护，每帧喂给相机）
local yaw = -90 -- 初始朝向 -z（面向场景内部）
local pitch = 0

function ready()
    print("当前关卡: " .. get_scene())
    set_mouse_mode("captured") -- 第一人称：锁鼠标到屏幕中心
    set_visible(false)         -- 隐藏玩家自己的网格（在自己身体里看不到自己）

    -- 跨场景读档第二段：读档触发的切场景，恢复存档位置
    if save_get("__restore", false) then
        set_position(save_get("px", 0), save_get("py", 0.55), save_get("pz", 0))
        set_velocity(0, 0, 0)
        save_set("__restore", false)
        print("读档完成，回到存档点")
    end
end

function update(dt)
    -- ① 鼠标转视角：相对位移累加到 yaw/pitch（pitch 夹住防翻转）
    local mdx, mdy = get_mouse_delta()
    yaw = yaw + mdx * MOUSE_SENS
    pitch = pitch - mdy * MOUSE_SENS
    if pitch > 89 then pitch = 89 end
    if pitch < -89 then pitch = -89 end

    -- ② 按朝向算水平前/右向量（与相机 yaw 同一套约定）
    local yr = math.rad(yaw)
    local fx, fz = math.cos(yr), math.sin(yr) -- 水平前向量
    local rx, rz = -fz, fx                    -- 右向量 = cross(前, 上)

    -- ③ WASD 相对朝向移动（改水平速度、保留竖直速度）
    local vx, vy, vz = get_velocity()
    local mx, mz = 0, 0
    if is_key_pressed("W") then
        mx = mx + fx; mz = mz + fz
    end
    if is_key_pressed("S") then
        mx = mx - fx; mz = mz - fz
    end
    if is_key_pressed("D") then
        mx = mx + rx; mz = mz + rz
    end
    if is_key_pressed("A") then
        mx = mx - rx; mz = mz - rz
    end
    local len = math.sqrt(mx * mx + mz * mz)
    if len > 0 then -- 归一化后乘速度（斜向不加速）
        mx = mx / len * MOVE_SPEED
        mz = mz / len * MOVE_SPEED
    end
    set_velocity(mx, vy, mz)

    -- ④ 跳跃：边沿检测 + 落地射线（从自身中心向下，raycast 自动忽略自己）
    if is_key_just_pressed("space") then
        local x, y, z = get_position()
        if raycast(x, y, z, 0, -1, 0, 0.65) then
            apply_impulse(0, JUMP_IMPULSE, 0)
        end
    end

    -- ⑤ 相机跟随（第一人称核心）：相机在玩家眼睛位置，朝向随鼠标
    local x, y, z = get_position()
    set_camera(x, y + EYE_HEIGHT, z, yaw, pitch)

    -- ⑥ 传送门：靠近 Portal（距离 < 1.5）→ 切换关卡
    local portal = get_node("Portal")
    if portal and portal ~= 0 then
        local px, py, pz = get_node_position(portal)
        local dx, dz = x - px, z - pz
        if dx * dx + dz * dz < 2.25 then           -- 1.5^2
            if string.find(get_scene(), "level1") then
                change_scene("scenes/level2.json") -- 金门 → 第 2 关
            else
                change_scene("scenes/level1.json") -- 绿门 → 回第 1 关
            end
            return                                 -- 本帧别再动（场景帧末就换）
        end
    end

    -- ⑦ 存档（K）：记录关卡 + 位置 → 落盘
    if is_key_just_pressed("K") then
        save_set("level", get_scene())
        save_set("px", x)
        save_set("py", y)
        save_set("pz", z)
        if save_write(1) then
            print("已存档: " .. get_scene())
        end
    end

    -- ⑧ 读档（L）：跨场景两段式
    if is_key_just_pressed("L") then
        if save_load(1) then
            local lv = save_get("level", "")
            if lv ~= "" and lv ~= get_scene() then
                save_set("__restore", true)
                change_scene(lv)
            else
                set_position(save_get("px", 0), save_get("py", 0.55), save_get("pz", 0))
                set_velocity(0, 0, 0)
                print("读档完成（本关内）")
            end
        else
            print("没有存档（先按 K 存一个）")
        end
    end

    -- ⑨ 掉出场地 → 回出生点
    if y < -10 then
        set_position(0, 0.55, 0)
        set_velocity(0, 0, 0)
    end
end
